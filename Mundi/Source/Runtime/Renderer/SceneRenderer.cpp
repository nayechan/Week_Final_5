#include "pch.h"
#include "SceneRenderer.h"

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
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
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
#include "Gizmo/GizmoArrowComponent.h"
#include "Gizmo/GizmoRotateComponent.h"
#include "Gizmo/GizmoScaleComponent.h"
#include "DirectionalLightComponent.h"
#include "AmbientLightComponent.h"
#include "PointLightComponent.h"
#include "SpotLightComponent.h"
#include "SwapGuard.h"
#include "MeshBatchElement.h"
#include "SceneView.h"
#include "Shader.h"
#include "ResourceManager.h"
#include "TileLightCuller.h"

FSceneRenderer::FSceneRenderer(UWorld* InWorld, FSceneView* InView, URenderer* InOwnerRenderer)
	: World(InWorld)
	, View(InView) // 전달받은 FSceneView 저장
	, OwnerRenderer(InOwnerRenderer)
	, RHIDevice(InOwnerRenderer->GetRHIDevice())
{
	//OcclusionCPU = std::make_unique<FOcclusionCullingManagerCPU>();

	// 타일 라이트 컬러 초기화
	TileLightCuller = std::make_unique<FTileLightCuller>();
	TileLightCuller->Initialize(RHIDevice, 16);  // 16x16 타일 크기

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

	// ViewMode에 따라 렌더링 경로 결정
	if (View->ViewMode == EViewModeIndex::VMI_Lit ||
		View->ViewMode == EViewModeIndex::VMI_Lit_Gouraud ||
		View->ViewMode == EViewModeIndex::VMI_Lit_Lambert ||
		View->ViewMode == EViewModeIndex::VMI_Lit_Phong)
	{
		//UpdateLightConstant();
		//라이트 구조체 버퍼 업데이트, 바인딩
		GWorld->GetLightManager()->UpdateLightBuffer(RHIDevice);
		PerformTileLightCulling();	// 타일 기반 라이트 컬링 수행
		RenderLitPath();
		RenderTileCullingDebug();	// 타일 컬링 디버그 시각화 draw
	}
	else if (View->ViewMode == EViewModeIndex::VMI_Unlit)
	{
		// Unlit 모드는 조명 없이 렌더링
		RenderLitPath();
	}
	else if (View->ViewMode == EViewModeIndex::VMI_WorldNormal)
	{
		// World Normal 시각화 모드
		RenderLitPath();
	}
	else if (View->ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderWireframePath();
	}
	else if (View->ViewMode == EViewModeIndex::VMI_SceneDepth)
	{
		RenderSceneDepthPath();
	}

	//그리드와 디버그용 Primitive는 Post Processing 적용하지 않음.
	RenderEditorPrimitivesPass();	// 그리드 출력
	RenderDebugPass();	//  선택한 물체의 경계 출력

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
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithId);

	// Base Pass
	RenderOpaquePass();
	RenderDecalPass();
	RenderFireBallPass();

	// 후처리 체인 실행
	RenderPostProcessingPasses();
}

void FSceneRenderer::RenderWireframePath()
{
	RHIDevice->RSSetState(ERasterizerMode::Solid);
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneIdTarget);

	RHIDevice->SetAndUpdateConstantBuffer(FLightBufferType());

	RenderOpaquePass();

	// 상태 변경: Wireframe으로 레스터라이즈 모드 설정하도록 설정
	RHIDevice->RSSetState(ERasterizerMode::Wireframe);

	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTarget);

	RenderOpaquePass();

	// 상태 복구: 원래의 Lit(Solid) 상태로 되돌림 (매우 중요!)
	RHIDevice->RSSetState(ERasterizerMode::Solid);
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
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithId);

	// ✅ 디버그: SceneRTV 전환 후 viewport 확인
	D3D11_VIEWPORT vpAfter;
	RHIDevice->GetDeviceContext()->RSGetViewports(&numVP, &vpAfter);
	UE_LOG("[RenderSceneDepthPath] AFTER OMSetRenderTargets(Scene): Viewport(%.1f x %.1f) at (%.1f, %.1f)",
		vpAfter.Width, vpAfter.Height, vpAfter.TopLeftX, vpAfter.TopLeftY);

	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetCurrentTargetRTV(), ClearColor);
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

	// 4. SceneDepth Post 프로세싱 처리
	RenderSceneDepthPostProcess();

}

//====================================================================================
// Private 헬퍼 함수 구현
//====================================================================================

bool FSceneRenderer::IsValid() const
{
	return World && View && OwnerRenderer && RHIDevice;
}

void FSceneRenderer::PrepareView()
{
	OwnerRenderer->SetCurrentViewportSize(View->ViewRect.Width(), View->ViewRect.Height());

	// FSceneRenderer 멤버 변수(View->ViewMatrix, View->ProjectionMatrix)를 채우는 대신
	// FSceneView의 멤버를 직접 사용합니다. (예: RenderOpaquePass에서 View->ViewMatrix 사용)

	// 뷰포트 크기 설정
	D3D11_VIEWPORT Vp = {};
	Vp.TopLeftX = (float)View->ViewRect.MinX;
	Vp.TopLeftY = (float)View->ViewRect.MinY;
	Vp.Width = (float)View->ViewRect.Width();
	Vp.Height = (float)View->ViewRect.Height();
	Vp.MinDepth = 0.0f;
	Vp.MaxDepth = 1.0f;
	RHIDevice->GetDeviceContext()->RSSetViewports(1, &Vp);

	// 뷰포트 상수 버퍼 설정 (View->ViewRect, RHIDevice 크기 정보 사용)
	FViewportConstants ViewConstData;
	// 1. 뷰포트 정보 채우기
	ViewConstData.ViewportRect.X = Vp.TopLeftX;
	ViewConstData.ViewportRect.Y = Vp.TopLeftY;
	ViewConstData.ViewportRect.Z = Vp.Width;
	ViewConstData.ViewportRect.W = Vp.Height;
	// 2. 전체 화면(렌더 타겟) 크기 정보 채우기
	ViewConstData.ScreenSize.X = static_cast<float>(RHIDevice->GetViewportWidth());
	ViewConstData.ScreenSize.Y = static_cast<float>(RHIDevice->GetViewportHeight());
	ViewConstData.ScreenSize.Z = 1.0f / RHIDevice->GetViewportWidth();
	ViewConstData.ScreenSize.W = 1.0f / RHIDevice->GetViewportHeight();
	RHIDevice->SetAndUpdateConstantBuffer((FViewportConstants)ViewConstData);
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
	const bool bDrawLight = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Lighting);
	const bool bUseAntiAliasing = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_FXAA);

	// Helper lambda to collect components from an actor
	auto CollectComponentsFromActor = [&](AActor* Actor, bool bIsEditorActor)
	{
		if (!Actor || Actor->GetActorHiddenInGame())
		{
			return;
		}

		for (USceneComponent* Component : Actor->GetSceneComponents())
		{
			if (!Component || !Component->IsActive())
			{
				continue;
			}

			// Collect Gizmo components (for editor overlay rendering)
			if (UGizmoArrowComponent* GizmoComponent = Cast<UGizmoArrowComponent>(Component))
			{
				Proxies.Gizmos.Add(GizmoComponent);
				continue; // Gizmos are handled separately in overlay pass
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
				else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(PrimitiveComponent); DecalComponent && bDrawDecals)
				{
					Proxies.Decals.Add(DecalComponent);
				}
				else if (UFireBallComponent* FireBallComponent = Cast<UFireBallComponent>(PrimitiveComponent))
				{
					Proxies.FireBalls.Add(FireBallComponent);
				}
			}
			else
			{
				if (UHeightFogComponent* FogComponent = Cast<UHeightFogComponent>(Component); FogComponent && bDrawFog)
				{
					SceneGlobals.Fogs.Add(FogComponent);
				}

				else if (UDirectionalLightComponent* LightComponent = Cast<UDirectionalLightComponent>(Component); LightComponent && bDrawLight)
				{
					SceneGlobals.DirectionalLights.Add(LightComponent);
				}

				else if (UAmbientLightComponent* LightComponent = Cast<UAmbientLightComponent>(Component); LightComponent && bDrawLight)
				{
					SceneGlobals.AmbientLights.Add(LightComponent);
				}

				else if (UPointLightComponent* LightComponent = Cast<UPointLightComponent>(Component); LightComponent && bDrawLight)
				{
					if (USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(LightComponent); SpotLightComponent)
					{
						SceneLocals.SpotLights.Add(SpotLightComponent);
					}
					else
					{
						SceneLocals.PointLights.Add(LightComponent);
					}
				}
			}
		}
	};

	// Collect from Editor Actors (Gizmo, Grid, etc.)
	for (AActor* EditorActor : World->GetEditorActors())
	{
		CollectComponentsFromActor(EditorActor, true);
	}

	// Collect from Level Actors (including their Gizmo components)
	for (AActor* Actor : World->GetActors())
	{
		CollectComponentsFromActor(Actor, false);
	}
}

void FSceneRenderer::UpdateLightConstant()
{
//	FLightBufferType LightBuffer{};
//
//	// AmbientLight 수집 (활성화된 것만)
//	for (UAmbientLightComponent* LightComponent : SceneGlobals.AmbientLights)
//	{
//		if (!LightComponent->IsEnabled())
//			continue;
//
//		LightBuffer.AmbientLight = FAmbientLightInfo(LightComponent->GetLightInfo());
//		break;
//	}
//
//	// DirectionalLight 수집 (활성화된 것만)
//	for (UDirectionalLightComponent* LightComponent : SceneGlobals.DirectionalLights)
//	{
//		if (!LightComponent->IsEnabled())
//			continue;
//
//		LightBuffer.DirectionalLight = FDirectionalLightInfo(LightComponent->GetLightInfo());
//		break;
//	}
//
//	// PointLight 수집 (활성화된 것만)
//	for (UPointLightComponent* LightComponent : SceneLocals.PointLights)
//	{
//		if (!LightComponent->IsEnabled())
//			continue;
//
//		if (LightBuffer.PointLightCount >= NUM_POINT_LIGHT_MAX)
//		{
//			UE_LOG("PointLight의 최대 개수는 %d개 입니다.", NUM_POINT_LIGHT_MAX);
//			break;
//		}
//		LightBuffer.PointLights[LightBuffer.PointLightCount++] = FPointLightInfo(LightComponent->GetLightInfo());
//	}
//
//	// SpotLight 수집 (활성화된 것만)
//	for (USpotLightComponent* LightComponent : SceneLocals.SpotLights)
//	{
//		if (!LightComponent->IsEnabled())
//			continue;
//
//		if (LightBuffer.SpotLightCount >= NUM_SPOT_LIGHT_MAX)
//		{
//			UE_LOG("SpotLight의 최대 개수는 %d개 입니다.", NUM_SPOT_LIGHT_MAX);
//			break;
//		}
//		LightBuffer.SpotLights[LightBuffer.SpotLightCount++] = FSpotLightInfo(LightComponent->GetLightInfo());
//	}

	// CRITICAL FIX: Use SetAndUpdateConstantBuffer instead of UpdateConstantBuffer
	// UpdateConstantBuffer only updates the buffer data but doesn't bind it to slot b8
	// SetAndUpdateConstantBuffer both updates the data AND binds it to VS/PS
	//RHIDevice->SetAndUpdateConstantBuffer(LightBuffer);
}

void FSceneRenderer::PerformTileLightCulling()
{
	if (!TileLightCuller)
		return;

	// 뷰포트 크기 가져오기
	UINT ViewportWidth = static_cast<UINT>(View->ViewRect.Width());
	UINT ViewportHeight = static_cast<UINT>(View->ViewRect.Height());

	// PointLight와 SpotLight 정보 수집
	TArray<FPointLightInfo>& PointLights = GWorld->GetLightManager()->GetPointLightInfoList();
	TArray<FSpotLightInfo>& SpotLights = GWorld->GetLightManager()->GetSpotLightInfoList();

	// 타일 컬링 수행
	TileLightCuller->CullLights(
		PointLights,
		SpotLights,
		View->ViewMatrix,
		View->ProjectionMatrix,
		View->ZNear,
		View->ZFar,
		ViewportWidth,
		ViewportHeight
	);

	// 타일 컬링 상수 버퍼 업데이트
	FTileCullingBufferType TileCullingBuffer;
	TileCullingBuffer.TileSize = 16;  // TileLightCuller 초기화 시 사용한 값과 일치
	TileCullingBuffer.TileCountX = (ViewportWidth + 16 - 1) / 16;
	TileCullingBuffer.TileCountY = (ViewportHeight + 16 - 1) / 16;
	TileCullingBuffer.bUseTileCulling = 1;  // 타일 컬링 활성화

	RHIDevice->SetAndUpdateConstantBuffer(TileCullingBuffer);

	// Structured Buffer SRV를 t2 슬롯에 바인딩
	ID3D11ShaderResourceView* TileLightIndexSRV = TileLightCuller->GetLightIndexBufferSRV();
	if (TileLightIndexSRV)
	{
		RHIDevice->GetDeviceContext()->PSSetShaderResources(2, 1, &TileLightIndexSRV);
	}

	// 통계를 전역 매니저에 업데이트
	FTileCullingStatManager::GetInstance().UpdateStats(TileLightCuller->GetStats());
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
	TArray<FShaderMacro> ShaderMacros;
	FString ShaderPath = "Shaders/Materials/UberLit.hlsl";
	bool bNeedsShaderOverride = true; // 뷰 모드가 셰이더를 강제하는지 여부

	switch (View->ViewMode)
	{
	case EViewModeIndex::VMI_Lit_Phong:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });
		break;
	case EViewModeIndex::VMI_Lit_Gouraud:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_GOURAUD", "1" });
		break;
	case EViewModeIndex::VMI_Lit_Lambert:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_LAMBERT", "1" });
		break;
	case EViewModeIndex::VMI_Unlit:
		// 매크로 없음 (Unlit)
		break;
	case EViewModeIndex::VMI_WorldNormal:
		ShaderMacros.push_back(FShaderMacro{ "VIEWMODE_WORLD_NORMAL", "1" });
		break;
	default:
		// 기본 Lit 모드 등, 셰이더를 강제하지 않는 모드는 여기서 처리 가능
		bNeedsShaderOverride = false; // 예시: 기본 Lit 모드는 머티리얼 셰이더 사용
		break;
	}

	// ViewMode에 맞는 셰이더 로드 (셰이더 오버라이드가 필요한 경우에만)
	UShader* ViewModeShader = nullptr;
	if (bNeedsShaderOverride)
	{
		ViewModeShader = UResourceManager::GetInstance().Load<UShader>(ShaderPath, ShaderMacros);
		if (!ViewModeShader)
		{
			// 필요시 기본 셰이더로 대체하거나 렌더링 중단
			UE_LOG("RenderOpaquePass: Failed to load ViewMode shader: %s", ShaderPath.c_str());
			return;
		}
	}

	// --- 1. 수집 (Collect) ---
	MeshBatchElements.Empty();
	for (UMeshComponent* MeshComponent : Proxies.Meshes)
	{
		MeshComponent->CollectMeshBatches(MeshBatchElements, View);
	}

	// --- UMeshComponent 셰이더 오버라이드 ---
	if (bNeedsShaderOverride && ViewModeShader)
	{
		// 수집된 UMeshComponent 배치 요소의 셰이더를 ViewModeShader로 강제 변경
		for (FMeshBatchElement& BatchElement : MeshBatchElements)
		{
			BatchElement.VertexShader = ViewModeShader;
			BatchElement.PixelShader = ViewModeShader;
		}
	}

	// Billboard는 Overlay Pass에서 렌더링됨 (DirectionGizmo 다음, Selection Gizmo 전)
	// for (UBillboardComponent* BillboardComponent : Proxies.Billboards)
	// {
	// 	BillboardComponent->Render(OwnerRenderer, View->ViewMatrix, View->ProjectionMatrix);
	// }

	for (UTextRenderComponent* TextRenderComponent : Proxies.Texts)
	{
		// TODO: UTextRenderComponent도 CollectMeshBatches를 통해 FMeshBatchElement를 생성하도록 구현
		//TextRenderComponent->CollectMeshBatches(MeshBatchElements, View);
	}

	// --- 2. 정렬 (Sort) ---
	MeshBatchElements.Sort([](const FMeshBatchElement& A, const FMeshBatchElement& B)
		{
			if (A.VertexShader != B.VertexShader) return A.VertexShader < B.VertexShader;
			if (A.PixelShader != B.PixelShader) return A.PixelShader < B.PixelShader;
			if (A.Material != B.Material) return A.Material < B.Material;
			if (A.Mesh != B.Mesh) return A.Mesh < B.Mesh;
			return false;
		});

	// --- 3. 그리기 (Draw) ---
	DrawMeshBatches(MeshBatchElements, true);
}

void FSceneRenderer::DrawMeshBatches(TArray<FMeshBatchElement>& InMeshBatches, bool bClearListAfterDraw)
{
	if (InMeshBatches.IsEmpty()) return;

	// RHI 상태 초기 설정 (Opaque Pass 기본값)
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual); // 깊이 쓰기 ON
	RHIDevice->OMSetBlendState(false);

	// 현재 GPU 상태 캐싱용 변수
	UShader* CurrentVertexShader = nullptr;
	UShader* CurrentPixelShader = nullptr;
	UMaterial* CurrentMaterial = nullptr;
	UStaticMesh* CurrentMesh = nullptr;

	// 기본 샘플러 미리 가져오기 (루프 내 반복 호출 방지)
	ID3D11SamplerState* DefaultSampler = RHIDevice->GetSamplerState(RHI_Sampler_Index::Default);

	// 공통 상수 버퍼 설정 (View, Projection 등) - 루프 전에 한 번만
	FVector CameraPos = View->ViewLocation;
	RHIDevice->SetAndUpdateConstantBuffer(ViewProjBufferType(View->ViewMatrix, View->ProjectionMatrix));
	RHIDevice->SetAndUpdateConstantBuffer(CameraBufferType(CameraPos, 0.0f));

	// 정렬된 리스트 순회
	for (const FMeshBatchElement& Batch : InMeshBatches)
	{
		// --- 필수 요소 유효성 검사 ---
		if (!Batch.VertexShader || !Batch.PixelShader || !Batch.Mesh)
		{
			// 셰이더나 메시가 없으면 그릴 수 없음
			continue;
		}

		// 1. 셰이더 상태 변경
		if (Batch.VertexShader != CurrentVertexShader || Batch.PixelShader != CurrentPixelShader)
		{
			RHIDevice->PrepareShader(Batch.VertexShader, Batch.PixelShader);
			CurrentVertexShader = Batch.VertexShader;
			CurrentPixelShader = Batch.PixelShader;
		}

		// 2. 머티리얼 상태 변경 (텍스처, 재질 상수 버퍼 바인딩)
		if (Batch.Material != CurrentMaterial)
		{
			ID3D11ShaderResourceView* DiffuseTextureSRV = nullptr;
			ID3D11ShaderResourceView* NormalTextureSRV = nullptr;

			FPixelConstBufferType PixelConst{}; // 기본값으로 초기화

			if (Batch.Material) // 머티리얼 유효성 검사
			{
				// 유효한 머티리얼이 있는 경우
				const FMaterialInfo& MaterialInfo = Batch.Material->GetMaterialInfo();
				PixelConst.Material = MaterialInfo;
				PixelConst.bHasMaterial = true;
				// --- 텍스처 로드 및 SRV 준비 ---
				if (!MaterialInfo.DiffuseTextureFileName.empty())
				{
					if (UTexture* TextureData = Batch.Material->GetTexture(EMaterialTextureSlot::Diffuse))
					{
						if (TextureData->GetShaderResourceView())
						{
							DiffuseTextureSRV = TextureData->GetShaderResourceView();
							PixelConst.bHasDiffuseTexture = true;
						}
					}
				}

				if (!MaterialInfo.NormalTextureFileName.empty())
				{
					if (UTexture* TextureData = Batch.Material->GetTexture(EMaterialTextureSlot::Normal))
					{
						if (TextureData->GetShaderResourceView())
						{
							NormalTextureSRV = TextureData->GetShaderResourceView();
							PixelConst.bHasNormalTexture = true;
						}
					}
				}
			}
			else
			{
				// 머티리얼이 없는 경우 (예: 기본 버텍스 컬러 사용)
				// 기본값 FMaterialParameters 생성 (모든 값이 기본값)
				FMaterialInfo DefaultMaterialInfo;
				PixelConst.Material = DefaultMaterialInfo;
				PixelConst.bHasDiffuseTexture = false;
				PixelConst.bHasNormalTexture = false;
				// srv는 nullptr 유지
			}

			// --- RHI 상태 업데이트 ---
			// 텍스처 바인딩 (srv가 nullptr이어도 안전하게 호출 가능)
			ID3D11ShaderResourceView* Srvs[2] = { DiffuseTextureSRV, NormalTextureSRV };
			RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, Srvs);

			// 샘플러 바인딩 (기본 샘플러 사용)
			ID3D11SamplerState* Samplers[2] = { DefaultSampler, DefaultSampler };
			RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Samplers);

			// 픽셀 상수 버퍼 업데이트
			RHIDevice->SetAndUpdateConstantBuffer(PixelConst);

			CurrentMaterial = Batch.Material; // 현재 머티리얼 상태 캐싱
		}

		// 3. 메시 상태 변경 (Vertex Buffer, Index Buffer 바인딩)
		if (Batch.Mesh != CurrentMesh)
		{
			UINT Stride = 0;
			switch (Batch.Mesh->GetVertexType()) // Batch.Mesh는 위에서 null 체크 완료
			{
			case EVertexLayoutType::PositionColor:               Stride = sizeof(FVertexSimple); break;
			case EVertexLayoutType::PositionColorTexturNormal:   Stride = sizeof(FVertexDynamic); break;
			case EVertexLayoutType::PositionTextBillBoard:       Stride = sizeof(FBillboardVertexInfo_GPU); break;
			case EVertexLayoutType::PositionBillBoard:           Stride = sizeof(FBillboardVertex); break;

			default:
				// 이 경우는 CollectMeshBatches 단계에서 걸러졌어야 함
				UE_LOG("DrawMeshBatches: Unknown vertex type for Mesh!");
				continue; // 이 배치는 건너뜀
			}

			ID3D11Buffer* VertexBuffer = Batch.Mesh->GetVertexBuffer();
			ID3D11Buffer* IndexBuffer = Batch.Mesh->GetIndexBuffer();

			// 버퍼 포인터 유효성 검사 (추가적인 안전장치)
			if (!VertexBuffer || !IndexBuffer)
			{
				UE_LOG("DrawMeshBatches: VertexBuffer or IndexBuffer is null for Mesh!");
				continue; // 이 배치는 건너뜀
			}

			UINT Offset = 0;
			RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
			RHIDevice->GetDeviceContext()->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);

			CurrentMesh = Batch.Mesh; // 현재 메시 상태 캐싱
		}

		// 4. 오브젝트별 상수 버퍼 설정 (매번 변경)
		RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(Batch.WorldMatrix, Batch.WorldMatrix.InverseAffine().Transpose()));
		RHIDevice->SetAndUpdateConstantBuffer(ColorBufferType(FLinearColor(), Batch.ObjectID));

		// 5. 드로우 콜 실행
		RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(Batch.PrimitiveTopology);
		RHIDevice->GetDeviceContext()->DrawIndexed(Batch.IndexCount, Batch.StartIndex, Batch.BaseVertexIndex); // BaseVertexIndex 사용
	}

	// 루프 종료 후 리스트 비우기 (옵션)
	if (bClearListAfterDraw)
	{
		InMeshBatches.Empty();
	}
}

void FSceneRenderer::RenderDecalPass()
{
	if (Proxies.Decals.empty())
		return;

	// WorldNormal 모드에서는 Decal 렌더링 스킵
	if (View->ViewMode == EViewModeIndex::VMI_WorldNormal)
		return;

	UWorldPartitionManager* Partition = World->GetPartitionManager();
	if (!Partition)
		return;

	const FBVHierarchy* BVH = Partition->GetBVH();
	if (!BVH)
		return;

	FDecalStatManager::GetInstance().AddTotalDecalCount(Proxies.Decals.Num());	// TODO: 추후 월드 컴포넌트 추가/삭제 이벤트에서 데칼 컴포넌트의 개수만 추적하도록 수정 필요
	FDecalStatManager::GetInstance().AddVisibleDecalCount(Proxies.Decals.Num());	// 그릴 Decal 개수 수집

	// ViewMode에 따라 조명 모델 매크로 설정
	TArray<FShaderMacro> ShaderMacros;
	FString ShaderPath = "Shaders/Effects/Decal.hlsl";

	switch (View->ViewMode)
	{
	case EViewModeIndex::VMI_Lit_Phong:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });
		break;
	case EViewModeIndex::VMI_Lit_Gouraud:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_GOURAUD", "1" });
		break;
	case EViewModeIndex::VMI_Lit_Lambert:
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_LAMBERT", "1" });
		break;
	case EViewModeIndex::VMI_Lit:
		// 기본 Lit 모드는 Phong 사용
		ShaderMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });
		break;
	case EViewModeIndex::VMI_Unlit:
		// 매크로 없음 (Unlit)
		break;
	default:
		// 기타 ViewMode는 매크로 없음
		break;
	}

	// ViewMode에 따른 Decal 셰이더 로드
	UShader* DecalShader = UResourceManager::GetInstance().Load<UShader>(ShaderPath, ShaderMacros);
	if (!DecalShader)
	{
		UE_LOG("RenderDecalPass: Failed to load Decal shader with ViewMode macros!");
		return;
	}

	// 데칼 렌더 설정
	RHIDevice->RSSetState(ERasterizerMode::Decal);
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly); // 깊이 쓰기 OFF
	RHIDevice->OMSetBlendState(true);

	// Decal 셰이더 설정 (모든 Decal에 공통 적용)
	RHIDevice->GetDeviceContext()->VSSetShader(DecalShader->GetVertexShader(), nullptr, 0);
	RHIDevice->GetDeviceContext()->PSSetShader(DecalShader->GetPixelShader(), nullptr, 0);
	RHIDevice->GetDeviceContext()->IASetInputLayout(DecalShader->GetInputLayout());

	// CameraBuffer 설정 (조명 계산용 - 모든 Decal에 공통)
	FMatrix ViewInverse = View->ViewMatrix.InverseAffine();
	FVector CameraPosition(ViewInverse.M[3][0], ViewInverse.M[3][1], ViewInverse.M[3][2]);
	RHIDevice->SetAndUpdateConstantBuffer(CameraBufferType(CameraPosition));

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
			Decal->RenderAffectedPrimitives(OwnerRenderer, Target, View->ViewMatrix, View->ProjectionMatrix);
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
			FireBall->RenderAffectedPrimitives(OwnerRenderer, Target, View->ViewMatrix, View->ProjectionMatrix);
		}
	}

	RHIDevice->RSSetState(ERasterizerMode::Solid);
	RHIDevice->OMSetBlendState(false);
}

void FSceneRenderer::RenderPostProcessingPasses()
{
	UHeightFogComponent* FogComponent = nullptr;
	if (0 < SceneGlobals.Fogs.Num())
	{
		FogComponent = SceneGlobals.Fogs[0];
	}

	if (!FogComponent)
	{
		return;
	}

	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 0번 슬롯부터 2개의 SRV를 자동 해제하도록 설정
	FSwapGuard SwapGuard(RHIDevice, 0, 2);

	// 렌더 타겟 설정
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 쉐이더 설정
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* HeightFogPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/HeightFog_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !HeightFogPS || !HeightFogPS->GetPixelShader())
	{
		UE_LOG("HeightFog용 셰이더 없음!\n");
		return;
	}

	RHIDevice->PrepareShader(FullScreenTriangleVS, HeightFogPS);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneDepth);
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* PointClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::PointClamp);
	ID3D11SamplerState* LinearClampSamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);

	if (!DepthSRV || !SceneSRV || !PointClampSamplerState || !LinearClampSamplerState)
	{
		UE_LOG("Depth SRV / Scene SRV / PointClamp Sampler / LinearClamp Sampler is null!\n");
		return;
	}

	ID3D11ShaderResourceView* srvs[2] = { DepthSRV, SceneSRV };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 2, srvs);

	ID3D11SamplerState* Samplers[2] = { LinearClampSamplerState, PointClampSamplerState };
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 2, Samplers);

	// 상수 버퍼 업데이트
	ECameraProjectionMode ProjectionMode = View->ProjectionMode;
	//RHIDevice->UpdatePostProcessCB(ZNear, ZFar, ProjectionMode == ECameraProjectionMode::Orthographic);
	RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->ZNear, View->ZFar, ProjectionMode == ECameraProjectionMode::Orthographic));
	FMatrix InvView = View->ViewMatrix.InverseAffine();
	FMatrix InvProjection;
	if (ProjectionMode == ECameraProjectionMode::Perspective)
	{
		InvProjection = View->ProjectionMatrix.InversePerspectiveProjection();
	}
	else
	{
		InvProjection = View->ProjectionMatrix.InverseOrthographicProjection();
	}
	//RHIDevice->UpdateInvViewProjCB(InvView, InvProjection);
	RHIDevice->SetAndUpdateConstantBuffer(InvViewProjBufferType(InvView, InvProjection));
	UHeightFogComponent* F = FogComponent;
	//RHIDevice->UpdateFogCB(F->GetFogDensity(), F->GetFogHeightFalloff(), F->GetStartDistance(), F->GetFogCutoffDistance(), F->GetFogInscatteringColor()->ToFVector4(), F->GetFogMaxOpacity(), F->GetFogHeight());
	RHIDevice->SetAndUpdateConstantBuffer(FogBufferType(F->GetFogDensity(), F->GetFogHeightFalloff(), F->GetStartDistance(), F->GetFogCutoffDistance(), F->GetFogInscatteringColor()->ToFVector4(), F->GetFogMaxOpacity(), F->GetFogHeight()));

	// Draw
	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	// 이제 소멸자는 버퍼 스왑을 되돌리지 않고, SRV 해제 작업만 수행함
	SwapGuard.Commit();
}

void FSceneRenderer::RenderSceneDepthPostProcess()
{
	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 0번 슬롯부터 1개의 SRV를 자동 해제하도록 설정
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정 (Depth 없이 BackBuffer에만 그리기)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 쉐이더 설정
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* SceneDepthPS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/SceneDepth_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !SceneDepthPS || !SceneDepthPS->GetPixelShader())
	{
		UE_LOG("HeightFog용 셰이더 없음!\n");
		return;
	}
	RHIDevice->PrepareShader(FullScreenTriangleVS, SceneDepthPS);

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
	ECameraProjectionMode ProjectionMode = View->ProjectionMode;
	//RHIDevice->UpdatePostProcessCB(ZNear, ZFar, ProjectionMode == ECameraProjectionMode::Orthographic);
	RHIDevice->SetAndUpdateConstantBuffer(PostProcessBufferType(View->ZNear, View->ZFar, ProjectionMode == ECameraProjectionMode::Orthographic));

	// Draw
	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	// 이제 소멸자는 버퍼 스왑을 되돌리지 않고, SRV 해제 작업만 수행함
	SwapGuard.Commit();
}

void FSceneRenderer::RenderTileCullingDebug()
{
	// SF_TileCullingDebug가 비활성화되어 있으면 아무것도 하지 않음
	if (!World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_TileCullingDebug))
	{
		return;
	}

	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 SRV를 자동 해제하도록 설정
	// t0 (SceneColorSource), t2 (TileLightIndices) 사용
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정 (Depth 없이 SceneColor에 블렌딩)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// Depth State: Depth Test/Write 모두 OFF
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);
	RHIDevice->OMSetBlendState(false);

	// 셰이더 설정
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* TileDebugPS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/TileDebugVisualization_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !TileDebugPS || !TileDebugPS->GetPixelShader())
	{
		UE_LOG("TileDebugVisualization 셰이더 없음!\n");
		return;
	}
	RHIDevice->PrepareShader(FullScreenTriangleVS, TileDebugPS);

	// 텍스처 관련 설정
	ID3D11ShaderResourceView* SceneSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SceneSRV || !SamplerState)
	{
		UE_LOG("TileDebugVisualization: Scene SRV or Sampler is null!\n");
		return;
	}

	// t0: 원본 씬 텍스처
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SceneSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	// t2: 타일 라이트 인덱스 버퍼 (이미 PerformTileLightCulling에서 바인딩됨)
	// 별도 바인딩 불필요, 유지됨

	// b11: 타일 컬링 상수 버퍼 (이미 PerformTileLightCulling에서 설정됨)
	// 별도 업데이트 불필요, 유지됨

	// 전체 화면 쿼드 그리기
	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	SwapGuard.Commit();
}

void FSceneRenderer::RenderEditorPrimitivesPass()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTarget);

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
					Primitive->Render(OwnerRenderer, View->ViewMatrix, View->ProjectionMatrix);
				}
			}
		}
	}
}

void FSceneRenderer::RenderDebugPass()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTarget);

	// 선택된 액터의 디버그 볼륨 렌더링 (다형성 활용)
	for (AActor* SelectedActor : World->GetSelectionManager()->GetSelectedActors())
	{
		for (USceneComponent* Component : SelectedActor->GetSceneComponents())
		{
			// 모든 컴포넌트에서 RenderDebugVolume 호출
			// 각 컴포넌트는 필요한 경우 override하여 디버그 시각화 제공
			Component->RenderDebugVolume(OwnerRenderer, View->ViewMatrix, View->ProjectionMatrix);
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
	OwnerRenderer->EndLineBatch(FMatrix::Identity(), View->ViewMatrix, View->ProjectionMatrix);
}

// FSceneRenderer.cpp

void FSceneRenderer::RenderOverayEditorPrimitivesPass()
{
	// 후처리된 최종 이미지 위에 원본 씬의 뎁스 버퍼를 사용하여 3D 오버레이를 렌더링합니다.
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithId);

	// 뎁스 버퍼를 Clear하고 LessEqual로 그리기 때문에 오버레이로 표시되는데
	// 오버레이 끼리는 깊이 테스트가 가능함
	RHIDevice->ClearDepthBuffer(1.0f, 0);

	// Sort Gizmos by render priority (lower priority first, so higher priority renders on top)
	Proxies.Gizmos.Sort([](const UGizmoArrowComponent* A, const UGizmoArrowComponent* B)
	{
		// Null 체크 추가 (안전성)
		if (!A || !B) return false;
		return A->GetRenderPriority() < B->GetRenderPriority();
	});

	// Render all overlays (Gizmos and Billboards) with priority-based ordering
	// Priority 0: DirectionGizmos + Billboards (same depth buffer)
	// Priority 100: Selection Gizmos
	// Clear depth buffer when priority changes to prevent lower priority elements from occluding higher priority ones
	int32 CurrentPriority = INT32_MIN;
	bool bBillboardsRendered = false;

	// Render Priority 0 elements (DirectionGizmos + Billboards)
	for (UGizmoArrowComponent* GizmoComp : Proxies.Gizmos)
	{
		// Check if we've moved past priority 0
		if (GizmoComp->GetRenderPriority() > 0)
		{
			break; // Stop processing priority 0 gizmos
		}

		// Clear depth buffer when switching to a new priority level
		if (GizmoComp->GetRenderPriority() > CurrentPriority)
		{
			RHIDevice->ClearDepthBuffer(1.0f, 0);
			CurrentPriority = GizmoComp->GetRenderPriority();
		}

		// 1. [상태 설정] 이 기즈모의 색상 및 하이라이트 상태 설정
		RHIDevice->SetAndUpdateConstantBuffer(
			GizmoBufferType{ GizmoComp->GetColor(), GizmoComp->IsHighlighted() ? 1u : 0u }
		);
		// [상태 설정] Gizmo는 자체 머티리얼 색상을 쓰므로, 글로벌 컬러 오버라이드 끄기
		RHIDevice->SetAndUpdateConstantBuffer(ColorBufferType(FLinearColor(), 0));

		// 2. [수집] 이 기즈모의 FMeshBatchElement만 수집
		MeshBatchElements.Empty();
		GizmoComp->CollectMeshBatches(MeshBatchElements, View);

		// 3. [그리기] 수집된 배치를 *즉시* 그립니다.
		DrawMeshBatches(MeshBatchElements, true);
	}

	// Render Billboards at Priority 0 (same depth buffer as DirectionGizmos)
	// No depth clear needed - share depth buffer with Priority 0 gizmos
	for (UBillboardComponent* BillboardComponent : Proxies.Billboards)
	{
		BillboardComponent->Render(OwnerRenderer, View->ViewMatrix, View->ProjectionMatrix);
	}
	bBillboardsRendered = true;

	// Render higher priority gizmos (Priority 100: Selection Gizmos)
	for (UGizmoArrowComponent* GizmoComp : Proxies.Gizmos)
	{
		// Skip priority 0 gizmos (already rendered)
		if (GizmoComp->GetRenderPriority() <= 0)
		{
			continue;
		}

		// Clear depth buffer when switching to a new priority level
		if (GizmoComp->GetRenderPriority() > CurrentPriority)
		{
			RHIDevice->ClearDepthBuffer(1.0f, 0);
			CurrentPriority = GizmoComp->GetRenderPriority();
		}

		// 1. [상태 설정] 이 기즈모의 색상 및 하이라이트 상태 설정
		RHIDevice->SetAndUpdateConstantBuffer(
			GizmoBufferType{ GizmoComp->GetColor(), GizmoComp->IsHighlighted() ? 1u : 0u }
		);
		// [상태 설정] Gizmo는 자체 머티리얼 색상을 쓰므로, 글로벌 컬러 오버라이드 끄기
		RHIDevice->SetAndUpdateConstantBuffer(ColorBufferType(FLinearColor(), 0));

		// 2. [수집] 이 기즈모의 FMeshBatchElement만 수집
		MeshBatchElements.Empty();
		GizmoComp->CollectMeshBatches(MeshBatchElements, View);

		// 3. [그리기] 수집된 배치를 *즉시* 그립니다.
		DrawMeshBatches(MeshBatchElements, true);
	}

	// 모든 오버레이 렌더링 후 하이라이트 상태 비활성화
	// No need to reset GizmoBuffer - it's set per-gizmo during rendering
}

void FSceneRenderer::ApplyScreenEffectsPass()
{
	if (!World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_FXAA))
	{
		return;
	}

	// Swap 가드 객체 생성: 스왑을 수행하고, 소멸 시 0번 슬롯부터 1개의 SRV를 자동 해제하도록 설정
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 렌더 타겟 설정
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithoutDepth);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* SourceSRV = RHIDevice->GetSRV(RHI_SRV_Index::SceneColorSource);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SourceSRV || !SamplerState)
	{
		UE_LOG("PointClamp Sampler is null!\n");
		return;
	}

	// Shader Resource 바인딩 (슬롯 확인!)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SourceSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* CopyTexturePS = UResourceManager::GetInstance().Load<UShader>("Shaders/PostProcess/FXAA_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !CopyTexturePS || !CopyTexturePS->GetPixelShader())
	{
		UE_LOG("FXAA 셰이더 없음!\n");
		return;
	}

	//RHIDevice->UpdateFXAACB(
	//FVector2D(static_cast<float>(RHIDevice->GetViewportWidth()), static_cast<float>(RHIDevice->GetViewportHeight())),
	//	FVector2D(1.0f / static_cast<float>(RHIDevice->GetViewportWidth()), 1.0f / static_cast<float>(RHIDevice->GetViewportHeight())),
	//	0.0833f,
	//	0.166f,
	//	1.0f,	// 0.75 가 기본값이지만 효과 강조를 위해 1로 설정
	//	12
		//);
	RHIDevice->SetAndUpdateConstantBuffer(FXAABufferType(
		FVector2D(static_cast<float>(RHIDevice->GetViewportWidth()), static_cast<float>(RHIDevice->GetViewportHeight())),
		FVector2D(1.0f / static_cast<float>(RHIDevice->GetViewportWidth()), 1.0f / static_cast<float>(RHIDevice->GetViewportHeight())),
		0.0833f,
		0.166f,
		1.0f,	// 0.75 가 기본값이지만 효과 강조를 위해 1로 설정
		12));

	RHIDevice->PrepareShader(FullScreenTriangleVS, CopyTexturePS);

	RHIDevice->DrawFullScreenQuad();

	// 모든 작업이 성공적으로 끝났으므로 Commit 호출
	// 이제 소멸자는 버퍼 스왑을 되돌리지 않고, SRV 해제 작업만 수행함
	SwapGuard.Commit();
}

// 최종 결과물의 실제 BackBuffer에 그리는 함수
void FSceneRenderer::CompositeToBackBuffer()
{
	// 1. 최종 결과물을 Source로 만들기 위해 스왑하고, 작업 후 SRV 슬롯 0을 자동 해제하는 가드 생성
	FSwapGuard SwapGuard(RHIDevice, 0, 1);

	// 2. 렌더 타겟을 백버퍼로 설정 (깊이 버퍼 없음)
	RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithoutDepth);

	// 3. 텍스처 및 샘플러 설정
	// 이제 RHI_SRV_Index가 아닌, 현재 상태에 맞는 Source SRV를 직접 가져옴
	ID3D11ShaderResourceView* SourceSRV = RHIDevice->GetCurrentSourceSRV();
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!SourceSRV || !SamplerState)
	{
		UE_LOG("CompositeToBackBuffer에 필요한 리소스 없음!\n");
		return; // 가드가 자동으로 스왑을 되돌리고 SRV를 해제해줌
	}

	// 4. 셰이더 리소스 바인딩
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &SourceSRV);
	RHIDevice->GetDeviceContext()->PSSetSamplers(0, 1, &SamplerState);

	// 5. 셰이더 준비
	UShader* FullScreenTriangleVS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/FullScreenTriangle_VS.hlsl");
	UShader* BlitPS = UResourceManager::GetInstance().Load<UShader>("Shaders/Utility/Blit_PS.hlsl");
	if (!FullScreenTriangleVS || !FullScreenTriangleVS->GetVertexShader() || !BlitPS || !BlitPS->GetPixelShader())
	{
		UE_LOG("Blit용 셰이더 없음!\n");
		return; // 가드가 자동으로 스왑을 되돌리고 SRV를 해제해줌
	}
	RHIDevice->PrepareShader(FullScreenTriangleVS, BlitPS);

	// 6. 그리기
	RHIDevice->DrawFullScreenQuad();

	// 7. 모든 작업이 성공했으므로 Commit
	SwapGuard.Commit();
}

void FSceneRenderer::FinalizeFrame()
{
	//RHIDevice->UpdateHighLightConstantBuffers(false, FVector(1, 1, 1), 0, 0, 0, 0);
	// No need to reset GizmoBuffer - it's set per-gizmo during rendering


	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Culling))
	{
		int totalActors = static_cast<int>(World->GetActors().size());
		uint64 visiblePrimitives = Proxies.Meshes.size();
		UE_LOG("Total Actors: %d, Visible Primitives: %llu\r\n", totalActors, visiblePrimitives);
	}
}
