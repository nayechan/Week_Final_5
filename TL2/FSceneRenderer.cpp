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

FSceneRenderer::FSceneRenderer(UWorld* InWorld, ACameraActor* InCamera, FViewport* InViewport, URenderer* InOwnerRenderer)
	: World(InWorld)
	, Camera(InCamera)
	, Viewport(InViewport)
	, OwnerRenderer(InOwnerRenderer)
	, RHI(InOwnerRenderer->GetRHIDevice()) // OwnerRenderer를 통해 RHI를 가져옴
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

	// NOTE: 일단 컴포넌트 단위와 데칼 관련 이슈 해결까지 컬링 무시
	//// 2-1. 절두체 컬링 수행 -> 결과가 멤버 변수 PotentiallyVisibleActors에 저장됨
	//PerformFrustumCulling();
	//// 2-2. 오클루전 컬링 수행 -> PotentiallyVisibleActors를 입력으로 사용
	//PerformCPUOcclusion();

	// 3. 렌더링할 대상 수집 (Gather)
	GatherVisibleProxies();

	// 4. 각 렌더링 패스(Pass) 실행
	RenderOpaquePass();
	RenderDecalPass();
	RenderEditorPrimitivesPass();
	RenderDebugPass();

	// --- 렌더링 종료 ---
	FinalizeFrame();
}

//====================================================================================
// Private 헬퍼 함수 구현
//====================================================================================

bool FSceneRenderer::IsValid() const
{
	return World && Camera && Viewport && OwnerRenderer && RHI;
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

void FSceneRenderer::PerformCPUOcclusion()
{
	// Todo: 추후 오클루전이 컴포넌트 단위로 변경되면 적용

	//if (!bUseCPUOcclusion) return;

	//UpdateOcclusionGridSizeForViewport(Viewport);

	//TArray<FCandidateDrawable> Occluders, Occludees;
	//BuildCpuOcclusionSets(ViewFrustum, ViewMatrix, ProjectionMatrix, ZNear, ZFar, Occluders, Occludees);

	//OcclusionCPU->BuildOccluderDepth(Occluders, Viewport->GetSizeX(), Viewport->GetSizeY());
	//OcclusionCPU->BuildHZB();

	//uint32_t maxUUID = 0;
	//for (auto& C : Occludees) maxUUID = std::max(maxUUID, C.ActorIndex);
	//if (VisibleFlags.size() <= size_t(maxUUID))
	//	VisibleFlags.assign(size_t(maxUUID + 1), 1);

	//OcclusionCPU->TestOcclusion(Occludees, Viewport->GetSizeX(), Viewport->GetSizeY(), VisibleFlags);
}

void FSceneRenderer::GatherVisibleProxies()
{
	const bool bDrawPrimitives = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Primitives);
	const bool bDrawStaticMeshes = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
	const bool bDrawDecals = World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Decals);

	for (AActor* Actor : World->GetActors())
	{
		if (!Actor || Actor->GetActorHiddenInGame())
		{
			continue;
		}

		// NOTE: 컬링 코드 삭제, 컬링은 컴포넌트 단위로 수정 필요
		//if (!Actor || Actor->GetActorHiddenInGame() || Actor->GetCulled()) continue;
		//if (bUseCPUOcclusion && VisibleFlags.size() > Actor->UUID && VisibleFlags[Actor->UUID] == 0) continue;

		for (USceneComponent* Component : Actor->GetSceneComponents())
		{
			if (!Component || !Component->IsActive())
			{
				continue;
			}

			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				if (bDrawPrimitives)
				{
					if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Primitive))
					{
						if (bDrawStaticMeshes)
						{
							Proxies.Primitives.Add(Primitive);
						}
					}
					else
					{
						// NOTE: 나머지 Primitive 타입 일단 플래그 검사 없이 추가 (추후 수정)
						Proxies.Primitives.Add(Primitive);
					}
				}
			}
			else if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
			{
				FDecalStatManager::GetInstance().IncrementTotalDecalCount();

				if (bDrawDecals)
				{
					Proxies.Decals.Add(Decal);
				}
			}
		}
	}
}

void FSceneRenderer::RenderOpaquePass()
{
	OwnerRenderer->SetViewModeType(EffectiveViewMode);
	RHI->OMSetDepthStencilState(EComparisonFunc::LessEqual); // 깊이 쓰기 ON
	RHI->OMSetBlendState(false);

	for (UPrimitiveComponent* Primitive : Proxies.Primitives)
	{
		Primitive->Render(OwnerRenderer, ViewMatrix, ProjectionMatrix);
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

	FDecalStatManager::GetInstance().AddVisibleDecalCount(Proxies.Decals.Num());

	// 데칼 렌더 설정
	OwnerRenderer->SetViewModeType(EffectiveViewMode);
	RHI->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly); // 깊이 쓰기 OFF
	RHI->OMSetBlendState(true);

	for (UDecalComponent* Decal : Proxies.Decals)
	{
		// Decal이 그려질 Primitives
		TArray<UPrimitiveComponent*> TargetPrimitives;

		// 1. Decal의 World AABB와 충돌한 모든 StaticMeshComponent 쿼리
		const FAABB DecalAABB = Decal->GetWorldAABB();
		TArray<UStaticMeshComponent*> IntersectedStaticMeshComponents = BVH->QueryIntersectedComponents(DecalAABB);

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

	RHI->OMSetBlendState(false); // 상태 복구
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
					OwnerRenderer->SetViewModeType(EffectiveViewMode);
					RHI->OMSetDepthStencilState(EComparisonFunc::LessEqual);
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
	RHI->UpdateHighLightConstantBuffers(false, FVector(1, 1, 1), 0, 0, 0, 0);

	if (World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Culling))
	{
		int totalActors = static_cast<int>(World->GetActors().size());
		uint64 visiblePrimitives = Proxies.Primitives.size();
		UE_LOG("Total Actors: %d, Visible Primitives: %llu\r\n", totalActors, visiblePrimitives);
	}
}
//
//void FSceneRenderer::UpdateOcclusionGridSizeForViewport(FViewport* Viewport)
//{
//	// 기존 URenderer에 있던 로직과 동일
//	if (!Viewport) return;
//	int vw = (1 > Viewport->GetSizeX()) ? 1 : Viewport->GetSizeX();
//	int vh = (1 > Viewport->GetSizeY()) ? 1 : Viewport->GetSizeY();
//	int gw = std::max(1, vw / std::max(1, OcclGridDiv));
//	int gh = std::max(1, vh / std::max(1, OcclGridDiv));
//	OcclusionCPU->Initialize(gw, gh);
//}
//
//void FSceneRenderer::BuildCpuOcclusionSets(const Frustum& ViewFrustum, const FMatrix& View, const FMatrix& Proj, float ZNear, float ZFar, TArray<FCandidateDrawable>& OutOccluders, TArray<FCandidateDrawable>& OutOccludees)
//{
//	OutOccluders.clear();
//	OutOccludees.clear();
//
//	size_t estimatedCount = 0;
//	for (AActor* Actor : World->GetActors())
//	{
//		if (Actor && !Actor->GetActorHiddenInGame() && !Actor->GetCulled())
//		{
//			if (Actor->IsA<AStaticMeshActor>()) estimatedCount++;
//		}
//	}
//	OutOccluders.reserve(estimatedCount);
//	OutOccludees.reserve(estimatedCount);
//
//	const FMatrix VP = View * Proj; // 행벡터: p_world * View * Proj
//
//	for (AActor* Actor : World->GetActors())
//	{
//		if (!Actor) continue;
//		if (Actor->GetActorHiddenInGame()) continue;
//		if (Actor->GetCulled()) continue;
//
//		AStaticMeshActor* SMA = Cast<AStaticMeshActor>(Actor);
//		if (!SMA) continue;
//
//		UAABoundingBoxComponent* Box = Cast<UAABoundingBoxComponent>(SMA->CollisionComponent);
//		if (!Box) continue;
//
//		OutOccluders.emplace_back();
//		FCandidateDrawable& occluder = OutOccluders.back();
//		occluder.ActorIndex = Actor->UUID;
//		occluder.Bound = Box->GetWorldBound();
//		occluder.WorldViewProj = VP;
//		occluder.WorldView = View;
//		occluder.ZNear = ZNear;
//		occluder.ZFar = ZFar;
//
//		OutOccludees.emplace_back(occluder);
//	}
//}