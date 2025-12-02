#include "pch.h"
#include "DepthOfFieldPass.h"
#include "SceneView.h"
#include "SwapGuard.h"
#include "DepthOfFieldComponent.h"

// @brief - 텍스처를 다른 렌더 타겟으로 복사해주는 내부 헬퍼 함수
static void ExecuteCopyPass(FSceneView* View, D3D11RHI* RHIDevice, ERTVMode Dst, RHI_SRV_Index Src)
{
	ID3D11RenderTargetView* nullRTV = nullptr;
	RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &nullRTV, nullptr);
	ID3D11ShaderResourceView* nullSRV = nullptr;
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &nullSRV);

	FSwapGuard Swap(RHIDevice, 0, 1);
	RHIDevice->OMSetRenderTargets(Dst);
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	UShader* VS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* PS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/Passthrough_PS.hlsl");
	if (!VS || !VS->GetVertexShader() || !PS || !PS->GetPixelShader()) { return; }

	RHIDevice->PrepareShader(VS, PS);

	ID3D11ShaderResourceView* SrcSRV = RHIDevice->GetSRV(Src);
	ID3D11SamplerState* PointSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SrcSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &PointSampler);

	RHIDevice->DrawFullScreenQuad();
	Swap.Commit();
}

void FDepthOfFieldPass::Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
	if (UDepthOfFieldComponent* DofComponent = Cast<UDepthOfFieldComponent>(M.SourceObject); DofComponent == nullptr)
	{
		UE_LOG("DepthOfField: Source object is not a DepthOfFieldComponent!\n");
		return;
	}

	// ============================================================
	// (1) CoC 단계
	// ============================================================

	// [필수] B. CoC 계산
	ExecuteCoCPass(M, View, RHIDevice);

	// [선택] E. Dilation
	// 전경 픽셀을 외곽으로 확대하여 경계 아티팩트 제거
	ExecuteDilationPass(View, RHIDevice);


	// ============================================================
	// (2) Blur 단계
	// ============================================================

	// [선택] D. Near/Far Split
	// Near와 Far 영역을 분리하여 각각 독립적으로 블러 적용 (아티팩트 절단)
	ExecuteNearFarSplitPass(View, RHIDevice);

	// [선택] C. Prefilter - Near/Far 각각에 적용하여 밝은 영역 강조
	ExecutePreFilterPass(View, RHIDevice, RHI_SRV_Index::DofNearMap, ERTVMode::DofBlurTarget);
	ExecuteCopyPass(View, RHIDevice, ERTVMode::DofNearTarget, RHI_SRV_Index::DofBlurMap);

	ExecutePreFilterPass(View, RHIDevice, RHI_SRV_Index::DofFarMap, ERTVMode::DofBlurTarget);
	ExecuteCopyPass(View, RHIDevice, ERTVMode::DofFarTarget, RHI_SRV_Index::DofBlurMap);

	// [필수] F. Hex Blur (3-pass) - Near/Far 분리 방식
	ExecuteHexBlurPass(View, RHIDevice, RHI_SRV_Index::DofNearMap, ERTVMode::DofNearTarget, RHI_SRV_Index::DofNearMap);
	ExecuteHexBlurPass(View, RHIDevice, RHI_SRV_Index::DofFarMap, ERTVMode::DofFarTarget, RHI_SRV_Index::DofFarMap);

	// 3) Near와 Far 블러 결과 병합
	ExecuteMergeNearFarPass(View, RHIDevice);

	// [선택] G. Upscale
	//ExecuteUpscalePass(View, RHIDevice);

	// [선택] T. Temporal Blend
	//ExecuteTemporalBlendPass(View, RHIDevice);


	// ============================================================
	// (3) Composite 단계
	// ============================================================
	// 
	// [필수] H. Composite
	ExecuteCompositePass(M, View, RHIDevice);
}

void FDepthOfFieldPass::ExecuteCoCPass(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
	// CoC 계산: 깊이 버퍼를 읽어서 Circle of Confusion 값을 계산하고 DofCoCTarget에 저장
	UDepthOfFieldComponent* DofComponent = Cast<UDepthOfFieldComponent>(M.SourceObject);
	if (!DofComponent) { return; }

	// 이전 패스의 RTV/SRV 완전히 언바인드
	ID3D11RenderTargetView* nullRTV = nullptr;
	RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &nullRTV, nullptr);

	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, nullSRVs);

	// 1) 스왑 + SRV 언바인드 관리
	FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/2);

	// 2) DofCoC 타깃으로 렌더링
	RHIDevice->OMSetRenderTargets(ERTVMode::DofCoCTarget);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 3) 셰이더 로드
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* CoCPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_CoC_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !CoCPS || !CoCPS->GetPixelShader())
	{
		UE_LOG("DoF CoC 셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(FullScreenTriangleVS, CoCPS);

	// 4) SRV/Sampler (깊이 + 씬 컬러)
	ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* PointClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
	ID3D11SamplerState* LinearClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

	if (!DepthSRV || !SceneSRV || !PointClampSampler || !LinearClampSampler)
	{
		UE_LOG("DoF CoC: Required SRVs or Samplers are null!\n");
		return;
	}

	ID3D11ShaderResourceView* Srvs[2] = { DepthSRV, SceneSRV };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, Srvs);

	ID3D11SamplerState* Smps[2] = { PointClampSampler, LinearClampSampler };
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Smps);

	// 5) 상수 버퍼 업데이트
	ECameraProjectionMode ProjectionMode = View->ProjectionMode;
	RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->NearClip, View->FarClip, ProjectionMode == ECameraProjectionMode::Orthographic));

	// DoF 설정 가져오기
	FDepthOfFieldSettings DofSettings = DofComponent->GetDepthOfFieldSettings();
	FDepthOfFieldBufferType DofBuffer;
	DofBuffer.FocalDistance = DofSettings.FocalDistance;
	DofBuffer.FocalLength = DofSettings.FocalLength;
	DofBuffer.FNumber = DofSettings.Aperture;
	DofBuffer.MaxCoc = DofSettings.MaxCocRadius;
	DofBuffer.NearTransitionRange = DofSettings.NearTransitionRange;
	DofBuffer.FarTransitionRange = DofSettings.FarTransitionRange;
	DofBuffer.NearBlurScale = DofSettings.NearBlurScale;
	DofBuffer.FarBlurScale = DofSettings.FarBlurScale;
	DofBuffer.BlurSampleCount = DofSettings.BlurSampleCount;
	DofBuffer.BokehRotation = DofSettings.BokehRotationRadians;
	DofBuffer.Weight = M.Weight;
	DofBuffer._Pad = 0.0f;

	RHIDevice->SetAndUpdateConstantBuffer(DofBuffer);

	// 6) Draw
	RHIDevice->DrawFullScreenQuad();

	// 7) 확정
	Swap.Commit();
}

void FDepthOfFieldPass::ExecuteDilationPass(FSceneView* View, D3D11RHI* RHIDevice)
{
	// Dilation Pass: CoC 맵을 확장하여 전경 영역 보호
	// Min Filter를 적용하여 작은 CoC(전경)를 주변으로 퍼뜨림

	// 1) CoC 맵을 임시 버퍼(DofBlurTarget)로 복사
	{
		FSwapGuard Swap(RHIDevice, 0, 1);
		RHIDevice->OMSetRenderTargets(ERTVMode::DofBlurTarget);
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
		RHIDevice->OMSetBlendState(false);

		UShader* VS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
		UShader* PS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_Dilation_PS.hlsl");

		if (!VS || !VS->GetVertexShader() || !PS || !PS->GetPixelShader())
		{
			UE_LOG("DoF Dilation 셰이더 없음!\n");
			return;
		}

		RHIDevice->PrepareShader(VS, PS);

		// CoC 맵을 입력으로 사용 (Point Sampler로 정확한 픽셀 값 샘플링)
		ID3D11ShaderResourceView* CoCMapSRV = RHIDevice->GetSRV(RHI_SRV_Index::DofCocMap);
		ID3D11SamplerState* PointClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);

		if (!CoCMapSRV || !PointClampSampler)
		{
			UE_LOG("DoF Dilation: CoC Map SRV or Sampler is null!\n");
			return;
		}

		RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &CoCMapSRV);
		RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &PointClampSampler);

		RHIDevice->DrawFullScreenQuad();
		Swap.Commit();
	}

	// 2) Dilated 결과를 다시 CoC 맵으로 복사 (단순 1:1 복사, Dilation 재적용 X)
	ExecuteCopyPass(View, RHIDevice, ERTVMode::DofCoCTarget, RHI_SRV_Index::DofBlurMap);
}


void FDepthOfFieldPass::ExecutePreFilterPass(FSceneView* View, D3D11RHI* RHIDevice, RHI_SRV_Index InputSRV, ERTVMode OutputRTV)
{
	// PreFilter 패스: 밝은 영역을 추출 및 강조하여 보케 하이라이트 향상
	ID3D11RenderTargetView* nullRTV = nullptr;
	RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &nullRTV, nullptr);

	ID3D11ShaderResourceView* nullSRVs[1] = { nullptr };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, nullSRVs);

	FSwapGuard Swap(RHIDevice, 0, 1);
	RHIDevice->OMSetRenderTargets(OutputRTV);
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	UShader* VS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* PS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_PreFilter_PS.hlsl");
	if (!VS || !VS->GetVertexShader() || !PS || !PS->GetPixelShader())
	{
		UE_LOG("DoF PreFilter 셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(VS, PS);

	ID3D11ShaderResourceView* InputTextureSRV = RHIDevice->GetSRV(InputSRV);
	ID3D11SamplerState* LinearClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

	if (!InputTextureSRV || !LinearClampSampler)
	{
		UE_LOG("DoF PreFilter: Required SRVs or Samplers are null!\n");
		return;
	}

	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &InputTextureSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &LinearClampSampler);

	RHIDevice->DrawFullScreenQuad();
	Swap.Commit();
}

void FDepthOfFieldPass::ExecuteNearFarSplitPass(FSceneView* View, D3D11RHI* RHIDevice)
{
	// Near/Far Split Pass: CoC 맵을 기반으로 Near와 Far 영역을 분리
	// 이를 통해 전경/배경 블러를 독립적으로 처리하여 아티팩트 제거

	// 이전 패스의 RTV/SRV 완전히 언바인드
	ID3D11RenderTargetView* nullRTV = nullptr;
	RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &nullRTV, nullptr);

	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, nullSRVs);

	// Near와 Far 타겟을 Multiple Render Targets(MRT)로 설정
	ID3D11RenderTargetView* RTVs[2] = {
		RHIDevice->GetDofNearRTV(),
		RHIDevice->GetDofFarRTV()
	};
	RHIDevice->OMSetCustomRenderTargets(2, RTVs, nullptr);

	// 렌더 타겟 클리어 (이전 프레임 데이터 제거)
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetDofNearRTV(), clearColor);
	RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetDofFarRTV(), clearColor);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 셰이더 로드
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* NearFarSplitPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_NearFarSplit_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !NearFarSplitPS || !NearFarSplitPS->GetPixelShader())
	{
		UE_LOG("DoF NearFarSplit 셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(FullScreenTriangleVS, NearFarSplitPS);

	// SRV/Sampler (원본 씬 컬러 + CoC 맵)
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11ShaderResourceView* CoCMapSRV = RHIDevice->GetSRV(RHI_SRV_Index::DofCocMap);
	ID3D11SamplerState* LinearClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

	if (!SceneSRV || !CoCMapSRV || !LinearClampSampler)
	{
		UE_LOG("DoF NearFarSplit: Required SRVs or Samplers are null!\n");
		return;
	}

	ID3D11ShaderResourceView* Srvs[2] = { SceneSRV, CoCMapSRV };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, Srvs);

	ID3D11SamplerState* Smps[1] = { LinearClampSampler };
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, Smps);

	// Draw
	RHIDevice->DrawFullScreenQuad();

	// SRV 언바인드 (다음 패스를 위해)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, nullSRVs);

	// RTV 언바인드 (RTV/SRV 충돌 방지 - Near/Far 타겟을 다음 패스에서 SRV로 읽을 것이므로)
	ID3D11RenderTargetView* nullRTVs[2] = { nullptr, nullptr };
	RHIDevice->GetDeviceContext()->OMSetRenderTargets(2, nullRTVs, nullptr);
}

void FDepthOfFieldPass::ExecuteTemporalBlendPass(FSceneView* View, D3D11RHI* RHIDevice)
{
}

void FDepthOfFieldPass::ExecuteHexBlurPass(FSceneView* View, D3D11RHI* RHIDevice, RHI_SRV_Index InputSRV, ERTVMode FinalRTV, RHI_SRV_Index FinalSRV)
{
	ID3D11DeviceContext* Ctx = RHIDevice->GetDeviceContext();
	ID3D11RenderTargetView* nullRTV = nullptr;
	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };

	ID3D11SamplerState* LinearClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	UShader* VS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* PS1 = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_SeparatedBlur1_PS.hlsl");
	UShader* PS2 = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_SeparatedBlur2_PS.hlsl");
	UShader* PS3 = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_SeparatedBlur3_PS.hlsl");

	// Pass 1: InputSRV -> DofBlurTarget
	Ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
	Ctx->PSSetShaderResources(0, 2, nullSRVs);
	{
		FSwapGuard Swap(RHIDevice, 0, 2);
		RHIDevice->OMSetRenderTargets(ERTVMode::DofBlurTarget);
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
		RHIDevice->OMSetBlendState(false);
		RHIDevice->PrepareShader(VS, PS1);
		ID3D11ShaderResourceView* Srvs[2] = { RHIDevice->GetSRV(InputSRV), RHIDevice->GetSRV(RHI_SRV_Index::DofCocMap) };
		Ctx->PSSetShaderResources(0, 2, Srvs);
		Ctx->PSSetSamplers(0, 1, &LinearClampSampler);
		RHIDevice->DrawFullScreenQuad();
	}

	// Pass 2: DofBlurTarget -> FinalRTV
	Ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
	Ctx->PSSetShaderResources(0, 2, nullSRVs);
	{
		FSwapGuard Swap(RHIDevice, 0, 2);
		RHIDevice->OMSetRenderTargets(FinalRTV);
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
		RHIDevice->OMSetBlendState(false);
		RHIDevice->PrepareShader(VS, PS2);
		ID3D11ShaderResourceView* Srvs[2] = { RHIDevice->GetSRV(RHI_SRV_Index::DofBlurMap), RHIDevice->GetSRV(RHI_SRV_Index::DofCocMap) };
		Ctx->PSSetShaderResources(0, 2, Srvs);
		Ctx->PSSetSamplers(0, 1, &LinearClampSampler);
		RHIDevice->DrawFullScreenQuad();
	}

	// Pass 3: FinalRTV -> DofBlurTarget
	Ctx->OMSetRenderTargets(1, &nullRTV, nullptr);
	Ctx->PSSetShaderResources(0, 2, nullSRVs);
	{
		FSwapGuard Swap(RHIDevice, 0, 2);
		RHIDevice->OMSetRenderTargets(ERTVMode::DofBlurTarget);
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
		RHIDevice->OMSetBlendState(false);
		RHIDevice->PrepareShader(VS, PS3);
		ID3D11ShaderResourceView* Srvs[2] = { RHIDevice->GetSRV(FinalSRV), RHIDevice->GetSRV(RHI_SRV_Index::DofCocMap) };
		Ctx->PSSetShaderResources(0, 2, Srvs);
		Ctx->PSSetSamplers(0, 1, &LinearClampSampler);
		RHIDevice->DrawFullScreenQuad();
	}

	// Final Copy: DofBlurTarget -> FinalRTV
	ExecuteCopyPass(View, RHIDevice, FinalRTV, RHI_SRV_Index::DofBlurMap);
}

void FDepthOfFieldPass::ExecuteMergeNearFarPass(FSceneView* View, D3D11RHI* RHIDevice)
{
	// Near와 Far 블러 결과를 병합하여 최종 블러 이미지 생성
	ID3D11RenderTargetView* nullRTV = nullptr;
	RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &nullRTV, nullptr);

	ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 3, nullSRVs);

	FSwapGuard Swap(RHIDevice, 0, 3);
	RHIDevice->OMSetRenderTargets(ERTVMode::DofBlurTarget);
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	UShader* VS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* PS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_MergeNearFar_PS.hlsl");
	if (!VS || !VS->GetVertexShader() || !PS || !PS->GetPixelShader())
	{
		UE_LOG("DoF MergeNearFar 셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(VS, PS);

	ID3D11ShaderResourceView* Srvs[3] = {
		RHIDevice->GetSRV(RHI_SRV_Index::DofNearMap),  // Near 블러 최종 결과 (DofNearTarget)
		RHIDevice->GetSRV(RHI_SRV_Index::DofFarMap),   // Far 블러 최종 결과 (DofFarTarget)
		RHIDevice->GetSRV(RHI_SRV_Index::DofCocMap)
	};
	ID3D11SamplerState* LinearClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 3, Srvs);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &LinearClampSampler);

	RHIDevice->DrawFullScreenQuad();
	Swap.Commit();
}

void FDepthOfFieldPass::ExecuteCompositePass(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice)
{
	// Composite 패스: 원본 씬과 블러 결과를 CoC 맵을 기준으로 블렌딩하여 최종 결과를 SceneColorTarget에 저장
	UDepthOfFieldComponent* DofComponent = Cast<UDepthOfFieldComponent>(M.SourceObject);
	if (!DofComponent) { return; }

	// 이전 단계의 RTV와 SRV 상태를 완전히 끊어줍니다.
	ID3D11RenderTargetView* nullRTV = nullptr;
	RHIDevice->GetDeviceContext()->OMSetRenderTargets(1, &nullRTV, nullptr);

	ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 3, nullSRVs);

	// 1) 스왑 + SRV 언바인드 관리
	FSwapGuard Swap(RHIDevice, /*FirstSlot*/0, /*NumSlotsToUnbind*/3);

	// 2) SceneColorTarget으로 렌더링 (최종 출력)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 3) 셰이더 로드
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* CompositePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/DoF_Composite_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !CompositePS || !CompositePS->GetPixelShader())
	{
		UE_LOG("DoF Composite 셰이더 없음!\n");
		return;
	}

	// 4) SRV/Sampler (원본 씬 + 블러 결과 + CoC 맵)
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11ShaderResourceView* BlurMapSRV = RHIDevice->GetSRV(RHI_SRV_Index::DofBlurMap);
	ID3D11ShaderResourceView* CoCMapSRV = RHIDevice->GetSRV(RHI_SRV_Index::DofCocMap);
	ID3D11SamplerState* LinearClampSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

	if (!SceneSRV || !BlurMapSRV || !CoCMapSRV || !LinearClampSampler)
	{
		UE_LOG("DoF Composite: Required SRVs or Samplers are null!\n");
		return;
	}

	// PrepareShader 호출 (상수 버퍼 설정 전에 호출)
	RHIDevice->PrepareShader(FullScreenTriangleVS, CompositePS);

	// PrepareShader 이후에 SRV 바인딩 (PrepareShader가 다른 버퍼를 바인딩할 수 있으므로)
	ID3D11ShaderResourceView* Srvs[3] = { SceneSRV, BlurMapSRV, CoCMapSRV };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 3, Srvs);

	ID3D11SamplerState* Smps[1] = { LinearClampSampler };
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, Smps);

	// 5) 상수 버퍼 업데이트
	ECameraProjectionMode ProjectionMode = View->ProjectionMode;
	RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->NearClip, View->FarClip, ProjectionMode == ECameraProjectionMode::Orthographic));

	// DoF 설정 가져오기
	FDepthOfFieldSettings DofSettings = DofComponent->GetDepthOfFieldSettings();
	FDepthOfFieldBufferType DofBuffer;
	DofBuffer.FocalDistance = DofSettings.FocalDistance;
	DofBuffer.FocalLength = DofSettings.FocalLength;
	DofBuffer.FNumber = DofSettings.Aperture;
	DofBuffer.MaxCoc = DofSettings.MaxCocRadius;
	DofBuffer.NearTransitionRange = DofSettings.NearTransitionRange;
	DofBuffer.FarTransitionRange = DofSettings.FarTransitionRange;
	DofBuffer.NearBlurScale = DofSettings.NearBlurScale;
	DofBuffer.FarBlurScale = DofSettings.FarBlurScale;
	DofBuffer.BlurSampleCount = DofSettings.BlurSampleCount;
	DofBuffer.BokehRotation = DofSettings.BokehRotationRadians;
	DofBuffer.Weight = M.Weight;
	DofBuffer._Pad = 0.0f;

	RHIDevice->SetAndUpdateConstantBuffer(DofBuffer);

	// 6) Draw
	RHIDevice->DrawFullScreenQuad();

	// 7) 확정
	Swap.Commit();
}
