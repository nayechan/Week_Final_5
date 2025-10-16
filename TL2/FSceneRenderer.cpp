#include "pch.h"
#include "FSceneRenderer.h"

// FSceneRenderer가 사용하는 모든 헤더 포함
#include "World.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Renderer.h"
#include "RHIDevice.h"
#include "PrimitiveComponent.h"
#include "DecalComponent.h"
#include "StaticMeshActor.h"
#include "GridActor.h"
#include "GizmoActor.h"
#include "RenderSettings.h"
#include "Occlusion.h"
#include "Frustum.h"
#include "WorldPartitionManager.h"
#include "BVHierarchy.h"
#include "SelectionManager.h"
#include "StaticMeshComponent.h"
#include "DecalStatManager.h"
#include "BillboardComponent.h"
#include "TextRenderComponent.h"
#include "OBB.h"
#include "BoundingSphere.h"
#include "FireBallComponent.h"
#include "HeightFogComponent.h"
#include "GizmoArrowComponent.h"
#include "GizmoRotateComponent.h"
#include "GizmoScaleComponent.h"

FSceneRenderer::FSceneRenderer(UWorld* InWorld, ACameraActor* InCamera, FViewport* InViewport, URenderer* InOwnerRenderer)
	: World(InWorld)
	, Camera(InCamera)
	, Viewport(InViewport)
	, OwnerRenderer(InOwnerRenderer)
	, RHIDevice(InOwnerRenderer->GetRHIDevice()) // OwnerRenderer를 통해 RHI를 가져옴
{
	//OcclusionCPU = std::make_unique<FOcclusionCullingManagerCPU>();

	// 라인 수집 시작
	OwnerRenderer->BeginLineBatch();
}

FSceneRenderer::~FSceneRenderer()
{
}

//====================================================================================
// 메인 렌더 함수
//====================================================================================
void FSceneRenderer::Render()
{
	if (!IsValid()) return;

	// 1. 뷰(View) 준비: 행렬, 절두체 등 프레임에 필요한 기본 데이터 계산
	PrepareView();
	// 2. 렌더링할 대상 수집 (Cull + Gather)
	GatherVisibleProxies();

	RenderEditorPrimitivesPass();	// 그리드 출력
	RenderDebugPass();	// 빌보드나 선택한 물체의 경계 출력

	if (EffectiveViewMode == EViewModeIndex::VMI_Lit)
	{
		RenderLitPath();
	}
	else if (EffectiveViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderWireframePath();
	}
	else if (EffectiveViewMode == EViewModeIndex::VMI_SceneDepth)
	{
		RenderSceneDepthPath();
	}

	// 3. 공통 오버레이(Overlay) 렌더링
	RenderOverayEditorPrimitivesPass();	// 기즈모 출력

	// FXAA 등 화면에서 최종 이미지 품질을 위해 적용되는 효과를 적용
	ApplyScreenEffectsPass();

	// 최종적으로 Scene에 그려진 텍스쳐를 Back 버퍼에 그힌다
	CompositeToBackBuffer();

	// --- 렌더링 종료 ---
	FinalizeFrame();
}

//====================================================================================
// Render Path 함수 구현
//====================================================================================

void FSceneRenderer::RenderLitPath()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::Scene);

	// Base Pass
	RenderOpaquePass();
	RenderDecalPass();
	RenderFireBallPass();

	// Scene To PostProcessDestination
	Blit(RHI_SRV_Index::Scene, ERTVMode::PostProcessDestination);
	RHIDevice->SwapPostProcessTextures();

	// 후처리 체인 실행
	RenderPostProcessingPasses();
}

void FSceneRenderer::RenderWireframePath()
{
	// 상태 변경: Wireframe으로 레스터라이즈 모드 설정하도록 설정
	RHIDevice->RSSetState(ERasterizerMode::Wireframe);

	RHIDevice->OMSetRenderTargets(ERTVMode::Scene);

	RenderOpaquePass();

	// 상태 복구: 원래의 Lit(Solid) 상태로 되돌림 (매우 중요!)
	RHIDevice->RSSetState(ERasterizerMode::Solid);

	// Wireframe은 Post 프로세싱 처리하지 않음
	Blit(RHI_SRV_Index::Scene, ERTVMode::PostProcessDestination);
	RHIDevice->SwapPostProcessTextures();
}

void FSceneRenderer::RenderSceneDepthPath()
{
    // ✅ 디버그: SceneRTV 전환 전 viewport 확인
    D3D11_VIEWPORT vpBefore;
    UINT numVP = 1;
    RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpBefore);
    UE_LOG("[RenderSceneDepthPath] BEFORE OMSetRenderTargets(Scene): Viewport(%.1f x %.1f) at (%.1f, %.1f)", 
        vpBefore.Width, vpBefore.Height, vpBefore.TopLeftX, vpBefore.TopLeftY);

    // 1. Scene RTV와 Depth Buffer Clear
    RHIDevice->OMSetRenderTargets(ERTVMode::Scene);
    
    // ✅ 디버그: SceneRTV 전환 후 viewport 확인
    D3D11_VIEWPORT vpAfter;
    RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpAfter);
    UE_LOG("[RenderSceneDepthPath] AFTER OMSetRenderTargets(Scene): Viewport(%.1f x %.1f) at (%.1f, %.1f)", 
        vpAfter.Width, vpAfter.Height, vpAfter.TopLeftX, vpAfter.TopLeftY);
    
    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetSceneRTV(), ClearColor);
    RHIDevice->ClearDepthBuffer(1.0f, 0);

    // 2. Base Pass - Scene에 메시 그리기
    RenderOpaquePass();

    // ✅ 디버그: BackBuffer 전환 전 viewport 확인
    RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpBefore);
    UE_LOG("[RenderSceneDepthPath] BEFORE OMSetRenderTargets(BackBuffer): Viewport(%.1f x %.1f)", 
        vpBefore.Width, vpBefore.Height);

    // 3. BackBuffer Clear
    RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithoutDepth);
    RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetBackBufferRTV(), ClearColor);

    // ✅ 디버그: BackBuffer 전환 후 viewport 확인
    RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpAfter);
    UE_LOG("[RenderSceneDepthPath] AFTER OMSetRenderTargets(BackBuffer): Viewport(%.1f x %.1f)", 
        vpAfter.Width, vpAfter.Height);

	Blit(RHI_SRV_Index::Scene, ERTVMode::PostProcessDestination);
	RHIDevice->SwapPostProcessTextures();

    // 4. SceneDepth Post 프로세싱 처리
    RenderSceneDepthPostProcess();
}

//====================================================================================
// Private 헬퍼 함수 구현
//====================================================================================

bool FSceneRenderer::IsValid() const
{
	return World && Camera && Viewport && OwnerRenderer && RHIDevice;
}

void FSceneRenderer::PrepareView()
{
	RHIDevice->RSSetViewport();

	float ViewportAspectRatio = static_cast<float>(Viewport->GetSizeX()) / static_cast<float>(Viewport->GetSizeY());
	if (Viewport->GetSizeY() == 0) ViewportAspectRatio = 1.0f;

	OwnerRenderer->SetCurrentViewportSize(Viewport->GetSizeX(), Viewport->GetSizeY());

	// FSceneRenderer의 멤버 변수로 ViewMatrix, ProjectionMatrix 등을 저장
	ViewMatrix = Camera->GetViewMatrix();
	ProjectionMatrix = Camera->GetProjectionMatrix(ViewportAspectRatio, Viewport);

	if (UCameraComponent* CamComp = Camera->GetCameraComponent())
	{
		ViewFrustum = CreateFrustumFromCamera(*CamComp, ViewportAspectRatio);
		ZNear = CamComp->GetNearClip();
		ZFar = CamComp->GetFarClip();
	}

	EffectiveViewMode = World->GetRenderSettings().GetViewModeIndex();

	
	//RHIDevice->OnResize(Viewport->GetSizeX(), Viewport->GetSizeY());
	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetSceneRTV(), ClearColor);

	RHIDevice->ClearDepthBuffer(1.0f, 0);                 // 깊이값 초기화
}

void FSceneRenderer::GatherVisibleProxies()
{
	// NOTE: 일단 컴포넌트 단위와 데칼 관련 이슈 해결까지 컬링 무시
	//// 절두체 컬링 수행 -> 결과가 멤버 변수 PotentiallyVisibleActors에 저장됨
	//PerformFrustumCulling();

	const bool bDrawPrimitives = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Primitives);
	const bool bDrawStaticMeshes = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
	const bool bDrawDecals = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Decals);
	const bool bDrawFog = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Fog);
	const bool bUseAntiAliasing = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_FXAA);

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || Actor->GetActorHiddenInGame())
		{
			continue;
		}

		for (USceneComponent* Component : Actor->GetSceneComponents())
		{
			if (!Component || !Component->IsActive())
			{
				continue;
			}

			if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component); PrimitiveComponent && bDrawPrimitives)
			{
				if (UMeshComponent* MeshComponent = Cast<UMeshComponent>(PrimitiveComponent))
				{
					bool bShouldAdd = true;

					// 메시 타입이 '스태틱 메시'인 경우에만 ShowFlag를 검사하여 추가 여부를 결정
					if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
					{
						bShouldAdd = bDrawStaticMeshes;
					}
					// else if (USkeletalMeshComponent* SkeletalMeshComponent = ...)
					// {
					//     bShouldAdd = bDrawSkeletalMeshes;
					// }

					if (bShouldAdd)
					{
						Proxies.Meshes.Add(MeshComponent);
					}
				}
				else if (UBillboardComponent* BillboardComponent = Cast<UBillboardComponent>(PrimitiveComponent))
				{
					Proxies.Billboards.Add(BillboardComponent);
				}
				else if (UBillboardComponent* BillboardComponent = Cast<UBillboardComponent>(PrimitiveComponent))
				{
					Proxies.Billboards.Add(BillboardComponent);
				}
				else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(PrimitiveComponent); DecalComponent && bDrawDecals)
				{
					Proxies.Decals.Add(DecalComponent);
				}
				else if (UFireBallComponent* FireBallComponent = Cast<UFireBallComponent>(PrimitiveComponent))
				{
					Proxies.FireBalls.Add(FireBallComponent);
				}
			}
			else if (UHeightFogComponent* FogComponent = Cast<UHeightFogComponent>(Component); FogComponent && bDrawFog)
			{
				SceneGlobals.Fogs.Add(FogComponent);
			}
		}
	}
}

void FSceneRenderer::PerformFrustumCulling()
{
	PotentiallyVisibleComponents.clear();	// 할 필요 없는데 명목적으로 초기화

	// Todo: 프로스텀 컬링 수행, 추후 프로스텀 컬링이 컴포넌트 단위로 변경되면 적용

	//World->GetPartitionManager()->FrustumQuery(ViewFrustum)

	//for (AActor* Actor : World->GetActors())
	//{
	//	if (!Actor || Actor->GetActorHiddenInGame()) continue;

	//	// 절두체 컬링을 통과한 액터만 목록에 추가
	//	if (ViewFrustum.Intersects(Actor->GetBounds()))
	//	{
	//		PotentiallyVisibleActors.Add(Actor);
	//	}
	//}
}

void FSceneRenderer::RenderOpaquePass()
{
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual); // 깊이 쓰기 ON
	RHIDevice->OMSetBlendState(false);

	for (UMeshComponent* MeshComponent : Proxies.Meshes)
	{
		MeshComponent->Render(OwnerRenderer, ViewMatrix, ProjectionMatrix);
	}

	for (UBillboardComponent* BillboardComponent : Proxies.Billboards)
	{
		BillboardComponent->Render(OwnerRenderer, ViewMatrix, ProjectionMatrix);
	}

	for (UTextRenderComponent* TextRenderComponent : Proxies.Texts)
	{
		TextRenderComponent->Render(OwnerRenderer, ViewMatrix, ProjectionMatrix);
	}
}

void FSceneRenderer::RenderDecalPass()
{
	if (Proxies.Decals.empty())
		return;

	UWorldPartitionManager* Partition = World->GetPartitionManager();
	if (!Partition)
		return;

	const FBVHierarchy* BVH = Partition->GetBVH();
	if (!BVH)
		return;

	FDecalStatManager::GetInstance().AddTotalDecalCount(Proxies.Decals.Num());	// TODO: 추후 월드 컴포넌트 추가/삭제 이벤트에서 데칼 컴포넌트의 개수만 추적하도록 수정 필요
	FDecalStatManager::GetInstance().AddVisibleDecalCount(Proxies.Decals.Num());	// 그릴 Decal 개수 수집

	// 데칼 렌더 설정
	RHIDevice->RSSetState(ERasterizerMode::Decal);
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly); // 깊이 쓰기 OFF
	RHIDevice->OMSetBlendState(true);

	for (UDecalComponent* Decal : Proxies.Decals)
	{
		// Decal이 그려질 Primitives
		TArray<UPrimitiveComponent*> TargetPrimitives;

		// 1. Decal의 World AABB와 충돌한 모든 StaticMeshComponent 쿼리
		const FOBB DecalOBB = Decal->GetWorldOBB();
		TArray<UStaticMeshComponent*> IntersectedStaticMeshComponents = BVH->QueryIntersectedComponents(DecalOBB);

		// 2. 충돌한 모든 visible Actor의 PrimitiveComponent를 TargetPrimitives에 추가
		// Actor에 기본으로 붙어있는 TextRenderComponent, BoundingBoxComponent는 decal 적용 안되게 하기 위해,
		// 임시로 PrimitiveComponent가 아닌 UStaticMeshComponent를 받도록 함
		for (UStaticMeshComponent* SMC : IntersectedStaticMeshComponents)
		{
			if (!SMC)
				continue;

			AActor* Owner = SMC->GetOwner();
			if (!Owner || !Owner->IsActorVisible())
				continue;

			FDecalStatManager::GetInstance().IncrementAffectedMeshCount();
			TargetPrimitives.push_back(SMC);
		}

		// --- 데칼 렌더 시간 측정 시작 ---
		auto CpuTimeStart = std::chrono::high_resolution_clock::now();

		// 3. TargetPrimitive 순회하며 렌더링
		for (UPrimitiveComponent* Target : TargetPrimitives)
		{
			Decal->RenderAffectedPrimitives(OwnerRenderer, Target, ViewMatrix, ProjectionMatrix);
		}

		// --- 데칼 렌더 시간 측정 종료 및 결과 저장 ---
		auto CpuTimeEnd = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double, std::milli> CpuTimeMs = CpuTimeEnd - CpuTimeStart;
		FDecalStatManager::GetInstance().GetDecalPassTimeSlot() += CpuTimeMs.count(); // CPU 소요 시간 저장
	}

	RHIDevice->RSSetState(ERasterizerMode::Solid);
	RHIDevice->OMSetBlendState(false); // 상태 복구
}

void FSceneRenderer::RenderFireBallPass()
{
	if (Proxies.FireBalls.empty())
		return;

	UWorldPartitionManager* Partition = World->GetPartitionManager();
	if (!Partition)
		return;

	const FBVHierarchy* BVH = Partition->GetBVH();
	if (!BVH)
		return;

	// 데칼과 같은 설정 사용
	RHIDevice->RSSetState(ERasterizerMode::Decal); // z-fighting 방지용 DepthBias 포함
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly); // 깊이 쓰기 OFF
	RHIDevice->OMSetBlendState(true); // 블렌딩 ON

	for (UFireBallComponent* FireBall : Proxies.FireBalls)
	{
		// FireBall 렌더링
		TArray<UPrimitiveComponent*> TargetPrimitives;

		FBoundingSphere FireBallSphere = FireBall->GetBoundingSphere();
		TArray<UStaticMeshComponent*> IntersectedStaticMeshComponents = BVH->QueryIntersectedComponents(FireBallSphere);

		for (UStaticMeshComponent* SMC : IntersectedStaticMeshComponents)
		{
			if (!SMC)
				continue;

			AActor* Owner = SMC->GetOwner();
			if (!Owner || !Owner->IsActorVisible())
				continue;

			TargetPrimitives.push_back(SMC);
		}

		for (UPrimitiveComponent* Target : TargetPrimitives)
		{
			FireBall->RenderAffectedPrimitives(OwnerRenderer, Target, ViewMatrix, ProjectionMatrix);
		}
	}

	RHIDevice->RSSetState(ERasterizerMode::Solid);
	RHIDevice->OMSetBlendState(false);
}

void FSceneRenderer::RenderPostProcessingPasses()
{
	// 렌더 타겟 설정
	RHIDevice->OMSetRenderTargets(ERTVMode::PostProcessDestination);

	UHeightFogComponent* FogComponent = nullptr;
	if (0 < SceneGlobals.Fogs.Num())
	{
		if (SceneGlobals.Fogs[0])
		{
			FogComponent = SceneGlobals.Fogs[0];
		}
		else
		{
			return;
		}
	}
	else
	{
		return;
	}

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 쉐이더 설정
	UShader* FogShader = UResourceManager::GetInstance().Load<UShader>("Fog.hlsl");
	if (!FogShader)
	{
		UE_LOG("Fog.hlsl shader not found!\n");
		return;
	}
	RHIDevice->PrepareShader(FogShader);

	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 메시 설정 (FullScreenQuad)
	UStaticMesh* FullScreenQuadMesh = UResourceManager::GetInstance().Load<UStaticMesh>("Data/FullScreenQuad.obj");
	if (!FullScreenQuadMesh)
	{
		UE_LOG("FullScreenQuad mesh not found!\n");
		return;
	}

	ID3D11Buffer* VertexBuffer = FullScreenQuadMesh->GetVertexBuffer();
	ID3D11Buffer* IndexBuffer = FullScreenQuadMesh->GetIndexBuffer();
	uint32 IndexCount = FullScreenQuadMesh->GetIndexCount();

	// Vertex Layout에 맞는 Stride 사용 (중요!)
	UINT Stride = 0;
	switch (FullScreenQuadMesh->GetVertexType())
	{
	case EVertexLayoutType::PositionColorTexturNormal:
		Stride = sizeof(FNormalVertex);
		break;
	case EVertexLayoutType::PositionTextBillBoard:
		Stride = sizeof(FBillboardVertex);
		break;
	default:
		Stride = sizeof(FNormalVertex);
		break;
	}

	UINT Offset = 0;
	RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
	if (!DepthSRV)
	{
		UE_LOG("Depth SRV is null!\n");
		return;
	}

	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::PostProcessSource);
	if (!SceneSRV)
	{
		UE_LOG("Scene SRV is null!\n");
		return;
	}

	ID3D11SamplerState* PointClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
	if (!PointClampSamplerState)
	{
		UE_LOG("PointClamp Sampler is null!\n");
		return;
	}

	ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!LinearClampSamplerState)
	{
		UE_LOG("LinearClamp Sampler is null!\n");
		return;
	}

	ID3D11ShaderResourceView* srvs[2] = { DepthSRV, SceneSRV };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, srvs); 

	ID3D11SamplerState* Samplers[2] = { LinearClampSamplerState, PointClampSamplerState };
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Samplers);

	// 상수 버퍼 업데이트
	ECameraProjectionMode ProjectionMode = Camera->GetCameraComponent()->GetProjectionMode();
	RHIDevice->UpdatePostProcessCB(ZNear, ZFar, ProjectionMode == ECameraProjectionMode::Orthographic);
	FMatrix InvView = ViewMatrix.InverseAffine();
	FMatrix InvProjection;
	if(ProjectionMode == ECameraProjectionMode::Perspective)
	{
		InvProjection = ProjectionMatrix.InversePerspectiveProjection();
	}
	else
	{
		InvProjection = ProjectionMatrix.InverseOrthographicProjection();
	}
	RHIDevice->UpdateInvViewProjCB(InvView, InvProjection);
	UHeightFogComponent* F = FogComponent;
	RHIDevice->UpdateFogCB(F->GetFogDensity(), F->GetFogHeightFalloff(), F->GetStartDistance(), F->GetFogCutoffDistance(), F->GetFogInscatteringColor()->ToFVector4(), F->GetFogMaxOpacity(), F->GetFogHeight());

	// Draw
	RHIDevice->GetDeviceContext()->DrawIndexed(IndexCount, 0, 0);

	// Unbind SRV (중요! 다음 프레임에서 Depth를 RTV로 사용할 수 있게)
	ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, nullSRVs);
	
	// PostProcess 핑퐁 스왑
	RHIDevice->SwapPostProcessTextures();
}

void FSceneRenderer::RenderSceneDepthPostProcess()
{
    // 렌더 타겟 설정 (Depth 없이 BackBuffer에만 그리기)
    RHIDevice->OMSetRenderTargets(ERTVMode::PostProcessDestination);

    // Depth State: Depth Test/Write 모두 OFF
    RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
    RHIDevice->OMSetBlendState(false);

    // 쉐이더 설정
    UShader* SceneDepthShader = UResourceManager::GetInstance().Load<UShader>("SceneDepth.hlsl");
    if (!SceneDepthShader)
    {
        UE_LOG("SceneDepth.hlsl shader not found!\n");
        return;
    }
    RHIDevice->PrepareShader(SceneDepthShader);

    RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 메시 설정 (FullScreenQuad)
    UStaticMesh* FullScreenQuadMesh = UResourceManager::GetInstance().Load<UStaticMesh>("Data/FullScreenQuad.obj");
    if (!FullScreenQuadMesh)
    {
        UE_LOG("FullScreenQuad mesh not found!\n");
        return;
    }

    ID3D11Buffer* VertexBuffer = FullScreenQuadMesh->GetVertexBuffer();
    ID3D11Buffer* IndexBuffer = FullScreenQuadMesh->GetIndexBuffer();
    uint32 IndexCount = FullScreenQuadMesh->GetIndexCount();
    
    // Vertex Layout에 맞는 Stride 사용 (중요!)
    UINT Stride = 0;
    switch (FullScreenQuadMesh->GetVertexType())
    {
    case EVertexLayoutType::PositionColorTexturNormal:
        Stride = sizeof(FNormalVertex);
        break;
    case EVertexLayoutType::PositionTextBillBoard:
        Stride = sizeof(FBillboardVertex);
        break;
    default:
        Stride = sizeof(FNormalVertex);
        break;
    }

    UINT Offset = 0;
    RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
    RHIDevice->GetDeviceContext()->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

    // 텍스쳐 관련 설정
    ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
    if (!DepthSRV)
    {
        UE_LOG("Depth SRV is null!\n");
        return;
    }

    ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
    if (!SamplerState)
    {
        UE_LOG("PointClamp Sampler is null!\n");
        return;
    }

    // Shader Resource 바인딩 (슬롯 확인!)
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &DepthSRV);  // t0
    RHIDevice->GetDeviceContext()->PSSetSamplers(1, 1, &SamplerState); 

    // 상수 버퍼 업데이트
	ECameraProjectionMode ProjectionMode = Camera->GetCameraComponent()->GetProjectionMode();
	RHIDevice->UpdatePostProcessCB(ZNear, ZFar, ProjectionMode == ECameraProjectionMode::Orthographic);

    // Draw
    RHIDevice->GetDeviceContext()->DrawIndexed(IndexCount, 0, 0);

    // Unbind SRV (중요! 다음 프레임에서 Depth를 RTV로 사용할 수 있게)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &nullSRV);

	RHIDevice->SwapPostProcessTextures();
}

void FSceneRenderer::RenderEditorPrimitivesPass()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::Scene);

	for (AActor* EngineActor : World->GetEditorActors())
	{
		if (!EngineActor || EngineActor->GetActorHiddenInGame()) continue;
		if (Cast<AGridActor>(EngineActor) && !World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Grid)) continue;

		for (USceneComponent* Component : EngineActor->GetSceneComponents())
		{
			if (Component && Component->IsActive())
			{
				if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
				{
					// NOTE: 추후 깔끔하게 World Editor Actors 와 World Overlay Actors 랑 비슷한 느낌으로 분리가 필요할듯
					// 기즈모는 오버레이 Primitive라서 나중에 따로 그림
					// UGizmoArrowComponent를 다른 기즈모도 상속하고 있어서 Arrow만 검사해도 충분
					if (Cast<UGizmoArrowComponent>(Primitive))
					{
						continue;
					}

					RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
					Primitive->Render(OwnerRenderer, ViewMatrix, ProjectionMatrix);
				}
			}
		}
	}
}

void FSceneRenderer::RenderDebugPass()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::Scene);

	// 선택된 액터 경계 출력
	for (AActor* SelectedActor : World->GetSelectionManager()->GetSelectedActors())
	{
		for (USceneComponent* Component : SelectedActor->GetSceneComponents())
		{
			// 일단 데칼만 구현됨
			if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
			{
				Decal->RenderDebugVolume(OwnerRenderer, ViewMatrix, ProjectionMatrix);
			}
		}
	}

	// Debug draw (BVH, Octree 등)
	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_BVHDebug) && World->GetPartitionManager())
	{
		if (FBVHierarchy* BVH = World->GetPartitionManager()->GetBVH())
		{
			BVH->DebugDraw(OwnerRenderer); // DebugDraw가 LineBatcher를 직접 받도록 수정 필요
		}
	}

	// 수집된 라인을 출력하고 정리
	OwnerRenderer->EndLineBatch(FMatrix::Identity(), ViewMatrix, ProjectionMatrix);
}

void FSceneRenderer::RenderOverayEditorPrimitivesPass()
{
	// 후처리된 최종 이미지 위에 원본 씬의 뎁스 버퍼를 사용하여 3D 오버레이를 렌더링합니다.
	RHIDevice->OMSetRenderTargets(ERTVMode::PostProcessSourceWithDepth);

	// 뎁스 버퍼를 Clear하고 LessEqual로 그리기 때문에 오버레이로 표시되는데 오버레이 끼리는 깊이 테스트가 가능함
	RHIDevice->ClearDepthBuffer(1.0f, 0);

	for (AActor* EngineActor : World->GetEditorActors())
	{
		if (!EngineActor || EngineActor->GetActorHiddenInGame()) continue;

		for (USceneComponent* Component : EngineActor->GetSceneComponents())
		{
			if (Component && Component->IsActive())
			{
				if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
				{
					// UGizmoArrowComponent를 다른 기즈모도 상속하고 있어서 Arrow만 검사해도 충분
					if (Cast<UGizmoArrowComponent>(Primitive))
					{
						RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
						Primitive->Render(OwnerRenderer, ViewMatrix, ProjectionMatrix);
					}
				}
			}
		}
	}
}

void FSceneRenderer::ApplyScreenEffectsPass()
{
	if (!World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_FXAA))
	{
		return;
	}

	// 렌더 타겟 설정
	RHIDevice->OMSetRenderTargets(ERTVMode::PostProcessDestination);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* SourceSRV = RHIDevice->GetSRV(RHI_SRV_Index::PostProcessSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SourceSRV || !SamplerState)
	{
		UE_LOG("PointClamp Sampler is null!\n");
		return;
	}

	// Shader Resource 바인딩 (슬롯 확인!)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SourceSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("FullScreenTriangle.vs.hlsl");
	UShader* CopyTexturePS = UResourceManager::GetInstance().Load<UShader>("FXAA.ps.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !CopyTexturePS || !CopyTexturePS->GetPixelShader())
	{
		UE_LOG("FXAA 셰이더 없음!\n");
		return;
	}

	RHIDevice->UpdateFXAACB(
		FVector2D(RHIDevice->GetViewportWidth(), RHIDevice->GetViewportHeight()),
		FVector2D(1.0f / RHIDevice->GetViewportWidth(), 1.0f / RHIDevice->GetViewportHeight()),
		0.0833f,
		0.166f,
		1.0f,	// 0.75 가 기본값이지만 효과 강조를 위해 1로 설정
		12
	);

	RHIDevice->PrepareShader(FullScreenTriangleVS, CopyTexturePS);

	RHIDevice->DrawFullScreenQuad();

	// PostProcess 핑퐁 스왑
	RHIDevice->SwapPostProcessTextures();
}

void FSceneRenderer::CompositeToBackBuffer()
{
	// 마지막 BackBuffer 그리는 단계에서만 뷰포트 크기 설정
	D3D11_VIEWPORT vp = {};
	vp.TopLeftX = static_cast<float>(Viewport->GetStartX());
	vp.TopLeftY = static_cast<float>(Viewport->GetStartY());
	vp.Width = static_cast<float>(Viewport->GetSizeX());
	vp.Height = static_cast<float>(Viewport->GetSizeY());
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	RHIDevice->GetDeviceContext()->RSSetViewports(1, &vp);

	Blit(RHI_SRV_Index::PostProcessSource, ERTVMode::BackBufferWithoutDepth);
}

// Source 텍스처를 Destination 텍스처로 렌더링(복사)하는 작업
void FSceneRenderer::Blit(RHI_SRV_Index InSource, ERTVMode InDestination)
{
	// 1. Scene RTV와 Depth Buffer Clear
	RHIDevice->OMSetRenderTargets(InDestination);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* SourceSRV = RHIDevice->GetSRV(InSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SourceSRV || !SamplerState)
	{
		UE_LOG("PointClamp Sampler is null!\n");
		return;
	}

	// Shader Resource 바인딩 (슬롯 확인!)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SourceSRV);  // t0
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("FullScreenTriangle.vs.hlsl");
	UShader* CopyTexturePS = UResourceManager::GetInstance().Load<UShader>("CopyTexture.ps.hlsl");
	if (!FullScreenTriangleVS || !CopyTexturePS)
	{
		UE_LOG("셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(FullScreenTriangleVS, CopyTexturePS);

	RHIDevice->DrawFullScreenQuad();
}

void FSceneRenderer::FinalizeFrame()
{
	RHIDevice->UpdateHighLightConstantBuffers(false, FVector(1, 1, 1), 0, 0, 0, 0);

	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Culling))
	{
		int totalActors = static_cast<int>(World->GetActors().size());
		uint64 visiblePrimitives = Proxies.Meshes.size();
		UE_LOG("Total Actors: %d, Visible Primitives: %llu\r\n", totalActors, visiblePrimitives);
	}
}
