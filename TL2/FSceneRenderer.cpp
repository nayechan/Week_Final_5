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
	// 수집된 라인을 출력하고 소멸됨
	OwnerRenderer->EndLineBatch(FMatrix::Identity(), ViewMatrix, ProjectionMatrix);
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

	EffectiveViewMode = EViewModeIndex::VMI_SceneDepth; // 임시: Lit 모드에서 SceneDepth 경로 테스트

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
	RenderEditorPrimitivesPass();	// 기즈모, 그리드 출력
	RenderDebugPass();	// 빌보드나 선택한 물체의 경계 출력

	// FXAA 등 화면에서 최종 이미지 품질을 위해 적용되는 효과를 적용
	ApplyScreenEffectsPass();

	// 최종적으로 Scene에 그려진 텍스쳐를 Back 버퍼에 그힌다
	//CompositeToBackBuffer();

	// --- 렌더링 종료 ---
	FinalizeFrame();
}

//====================================================================================
// Render Path 함수 구현
//====================================================================================

void FSceneRenderer::RenderLitPath()
{
	// Base Pass
	RenderOpaquePass();
	RenderDecalPass();
	RenderFireBallPass();

	// 후처리 체인 실행
	RenderPostProcessingPasses();
}

void FSceneRenderer::RenderWireframePath()
{
	// 상태 변경: Wireframe으로 레스터라이즈 모드 설정하도록 설정
	RHIDevice->RSSetState(ERasterizerMode::Wireframe);

	RenderOpaquePass();

	// Wireframe은 Post 프로세싱 처리하지 않음

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
	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Wireframe))
	{
		EffectiveViewMode = EViewModeIndex::VMI_Wireframe;
	}

	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	RHIDevice->GetDeviceContext()->ClearRenderTargetView(RHIDevice->GetSceneRTV(), ClearColor);
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
			//else if (UHeightFogComponent* FogComponent = Cast<UHeightFogComponent>(Component); FogComponent && bDrawFog)
			//{
			//	SceneGlobals.Fogs.Add(FogComponent);
			//}
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
	RHIDevice->OMSetRenderTargets(ERTVMode::Scene);

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
	RHIDevice->OMSetRenderTargets(ERTVMode::Scene);

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
	if (Proxies.Decals.empty())
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

	for (UFireBallComponent* FireBall: Proxies.FireBalls)
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
	if (0 < SceneGlobals.Fogs.Num())
	{
		if (SceneGlobals.Fogs[0])
		{
			// SceneGlobals.Fogs[0] 를 사용
		}
	}

	//// 1-1. 적용할 효과 목록을 구성합니다. (설정에 따라 동적으로 생성)
	//std::vector<IPostProcessEffect*> effectChain;
	//if (World->GetRenderSettings().IsFogEnabled())
	//{
	//	effectChain.push_back(&FogEffect);
	//}
	//if (World->GetRenderSettings().IsFXAAEnabled())
	//{
	//	effectChain.push_back(&FXAAEffect);
	//}
	//// ... 다른 효과들도 추가 ...

	//// 1-2. 핑퐁에 사용할 렌더 타겟 2개를 가져옵니다.
	//RenderTarget* PostProcessRT_A = OwnerRenderer->GetPostProcessRT_A();
	//RenderTarget* PostProcessRT_B = OwnerRenderer->GetPostProcessRT_B();

	//// 1-3. Base Pass의 결과물이 첫 번째 소스(Source)가 됩니다.
	////     (Base Pass가 PostProcessRT_A에 그려졌다고 가정)
	//RenderTarget* sourceRT = PostProcessRT_A;
	//RenderTarget* destinationRT = PostProcessRT_B;

	//// 1-4. [엣지 케이스] 적용할 효과가 없다면, 원본 이미지를 화면에 복사하고 종료합니다.
	//if (effectChain.empty())
	//{
	//	RHI->CopyTexture(RHI->GetBackBuffer(), sourceRT->GetTexture());
	//	return;
	//}

	//// 2. 후처리 체인을 순회하며 실행합니다.
	//for (size_t i = 0; i < effectChain.size(); ++i)
	//{
	//	IPostProcessEffect* currentEffect = effectChain[i];

	//	// 2-1. [핵심] 마지막 효과인지 판단하여 최종 목적지를 설정합니다.
	//	if (i == effectChain.size() - 1)
	//	{
	//		// 마지막 효과는 최종 화면(Back Buffer)에 직접 그립니다.
	//		RHI->SetRenderTarget(RHI->GetBackBufferRTV(), nullptr);
	//	}
	//	else
	//	{
	//		// 중간 과정은 임시 버퍼(destinationRT)에 그립니다.
	//		RHI->SetRenderTarget(destinationRT->GetRTV(), nullptr);
	//	}

	//	// 2-2. 이전 단계의 결과물(sourceRT)을 입력 텍스처로 설정합니다.
	//	RHI->SetShaderResource(0, sourceRT->GetSRV());

	//	// 2-3. 현재 효과를 적용합니다. (내부적으로 FullScreenQuad를 그림)
	//	currentEffect->Apply(RHI);

	//	// 2-4. [핵심] 다음 단계를 위해 소스와 목적지의 역할을 맞바꿉니다.
	//	std::swap(sourceRT, destinationRT);
	//}
}

void FSceneRenderer::RenderSceneDepthPostProcess()
{
    // 렌더 타겟 설정 (Depth 없이 BackBuffer에만 그리기)
    RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithoutDepth);

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
    RHIDevice->UpdatePostProcessCB(ZNear, ZFar);

    // Draw
    RHIDevice->GetDeviceContext()->DrawIndexed(IndexCount, 0, 0);

    // Unbind SRV (중요! 다음 프레임에서 Depth를 RTV로 사용할 수 있게)
    ID3D11ShaderResourceView* nullSRV = nullptr;
    RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &nullSRV);
}

void FSceneRenderer::RenderEditorPrimitivesPass()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithDepth);

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
					RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
					Primitive->Render(OwnerRenderer, ViewMatrix, ProjectionMatrix);
				}
			}
		}
	}
}

void FSceneRenderer::RenderDebugPass()
{
	RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithDepth);

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
}

void FSceneRenderer::ApplyScreenEffectsPass()
{
}

void FSceneRenderer::CompositeToBackBuffer()
{
	// 1. Scene RTV와 Depth Buffer Clear
	RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithoutDepth);

	// 텍스쳐 관련 설정
	ID3D11ShaderResourceView* DepthSRV = RHIDevice->GetSRV(RHI_SRV_Index::Scene);
	ID3D11SamplerState* SamplerState = RHIDevice->GetSamplerState(RHI_Sampler_Index::LinearClamp);
	if (!DepthSRV || !SamplerState)
	{
		UE_LOG("PointClamp Sampler is null!\n");
		return;
	}

	// Shader Resource 바인딩 (슬롯 확인!)
	RHIDevice->GetDeviceContext()->PSSetShaderResources(0, 1, &DepthSRV);  // t0
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
