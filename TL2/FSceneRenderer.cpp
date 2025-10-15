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
	// Base Pass
	RenderOpaquePass();

	// SceneDepth Post 프로세싱 처리
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
	// 이런식으로 코드 작성?

	//// 1. 최종 목적지를 화면(Back Buffer)으로 설정.
	////    깊이 버퍼는 더 이상 쓰거나 테스트하지 않으므로 nullptr로 설정.
	//RHI->SetRenderTarget(RHI->GetBackBufferRTV(), nullptr);

	//// 2. 뎁스 시각화 전용 셰이더를 파이프라인에 바인딩.
	//RHI->SetVertexShader("FullScreenQuad.vs");
	//RHI->SetPixelShader("SceneDepthVisualizer.ps");

	//// 3. Opaque Pass에서 생성된 깊이 버퍼를 셰이더가 읽을 수 있도록
	////    ShaderResourceView(SRV)로 변환하여 바인딩.
	////    (RHI에 깊이 버퍼의 SRV를 가져오는 함수가 필요합니다.)
	//RHI->SetShaderResource(0, RHI->GetDepthBufferSRV());

	//// 4. 화면 전체를 덮는 사각형을 그림.
	//RHI->DrawFullScreenQuad();
}

void FSceneRenderer::RenderEditorPrimitivesPass()
{
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
