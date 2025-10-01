#include "pch.h"
#include "Actor.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"
#include "ShapeComponent.h"
#include "AABoundingBoxComponent.h"   
#include "MeshComponent.h"
#include "TextRenderComponent.h"
#include "WorldPartitionManager.h"
#include "BillboardComponent.h"

#include "World.h"
AActor::AActor()
{
	Name = "DefaultActor";
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("SceneComponent"));
	CollisionComponent = CreateDefaultSubobject<UAABoundingBoxComponent>(FName("CollisionBox"));
	TextComp = CreateDefaultSubobject<UTextRenderComponent>("TextBox");

	// TODO (동민) - 임시로 루트 컴포넌트에 붙임. 추후 계층 구조 관리 기능 구현 필요
	if (CollisionComponent)
	{
		CollisionComponent->SetupAttachment(RootComponent);
	}
	if (TextComp)
	{
		TextComp->SetupAttachment(RootComponent);
	}
}

AActor::~AActor()
{
	// UE처럼 역순/안전 소멸: 모든 컴포넌트 DestroyComponent
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->DestroyComponent();  // 안에서 Unregister/Detach 처리한다고 가정
	OwnedComponents.clear();
	SceneComponents.Empty();
	RootComponent = nullptr;
}

void AActor::BeginPlay()
{
	// 컴포넌트들 Initialize/BeginPlay 순회
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->InitializeComponent();
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->BeginPlay();
}

void AActor::Tick(float DeltaSeconds)
{
	// 에디터에서 틱 Off면 스킵
	if (!bTickInEditor && /*에디터 중*/ true) return;

	for (UActorComponent* Comp : OwnedComponents)
	{
		if (Comp && Comp->IsComponentTickEnabled())
		{
			Comp->TickComponent(DeltaSeconds /*, … 필요 인자*/);
		}
	}
}
void AActor::EndPlay(EEndPlayReason Reason)
{
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->EndPlay(Reason);
}
void AActor::Destroy()
{
	// 재진입/중복 방지
	if (IsPendingDestroy())
	{
		return;
	}
	MarkPendingDestroy();
	// 월드가 있으면 월드에 위임 (여기서 더 이상 this 만지지 않기)
	if (World) 
	{ 
		World->DestroyActor(this); 
		return; 
	}

	// 월드가 없을 때만 자체 정리
	EndPlay(EEndPlayReason::Destroyed);
	UnregisterAllComponents(true);
	DestroyAllComponents();
	ClearSceneComponentCaches();
	// 최종 delete (ObjectFactory가 소유권 관리 중이면 그 경로 사용)
	ObjectFactory::DeleteObject(this);
}



void AActor::SetRootComponent(USceneComponent* InRoot)
{
	if (RootComponent == InRoot)
	{
		return;
	}

	// 기존 루트가 있으면 Detach 처리 (필요 시)
	RootComponent = InRoot;
	if (RootComponent)
	{
		RootComponent->SetOwner(this);
		// 월드 등록/트리 등록
		RegisterComponentTree(RootComponent);
	}
}

void AActor::AddOwnedComponent(UActorComponent* Component)
{
	
	if (!Component)
	{
		return;
	}
	if (OwnedComponents.count(Component))
	{
		return;
	}

	OwnedComponents.insert(Component);
	Component->SetOwner(this);
	Component->RegisterComponent(); // Register(씬이면 트리 포함)

	if (USceneComponent* SC = Cast<USceneComponent>(Component))
	{
		SceneComponents.AddUnique(SC);
		// 루트가 없으면 자동 루트 지정
		if (!RootComponent)
		{
			SetRootComponent(SC);
		}
	}
}

void AActor::RemoveOwnedComponent(UActorComponent* Component)
{
	if (!Component)
	{
		return;
	}
	if (!OwnedComponents.erase(Component))
	{
		return;
	}

	if (USceneComponent* SC = Cast<USceneComponent>(Component))
	{
		// 루트면 처리 금지/교체 전략
		if (SC == RootComponent)
		{
			// 루트 교체(간단히 null) - 필요시 자식 중 하나로 승급도 가능
			RootComponent = nullptr;
		}
		SceneComponents.Remove(SC);
		UnregisterComponentTree(SC);
		SC->DetachFromParent(true);
	}
	Component->UnregisterComponent();
	Component->DestroyComponent();
}

void AActor::UnregisterAllComponents(bool bCallEndPlayOnBegun)
{
	// 파괴 경로에서 안전하게 등록 해제
	// 복사본으로 순회 (컨테이너 변형 중 안전)
	TArray<UActorComponent*> Temp;
	Temp.reserve(OwnedComponents.size());
	for (UActorComponent* C : OwnedComponents) Temp.push_back(C);

	for (UActorComponent* C : Temp)
	{
		if (!C) continue;

		// BeginPlay가 이미 호출된 컴포넌트라면 EndPlay(RemovedFromWorld) 보장
		if (bCallEndPlayOnBegun && C->HasBegunPlay())
		{
			C->EndPlay(EEndPlayReason::RemovedFromWorld);
		}
		C->UnregisterComponent(); // 내부 OnUnregister/리소스 해제
	}
}

void AActor::DestroyAllComponents()
{
	// Unregister 이후 최종 파괴
	TArray<UActorComponent*> Temp;
	Temp.reserve(OwnedComponents.size());
	for (UActorComponent* C : OwnedComponents) Temp.push_back(C);

	for (UActorComponent* C : Temp)
	{
		if (!C) continue;
		C->DestroyComponent(); // 내부에서 Owner=nullptr 등도 처리
	}
	OwnedComponents.clear();
}

void AActor::ClearSceneComponentCaches()
{
	SceneComponents.Empty();
	RootComponent = nullptr;
}



// ───────────────
// Transform API
// ───────────────
void AActor::SetActorTransform(const FTransform& NewTransform)
{
	if (RootComponent == nullptr)
	{
		return;
	}

	FTransform OldTransform = RootComponent->GetWorldTransform();
	if (!(OldTransform == NewTransform))
	{
		RootComponent->SetWorldTransform(NewTransform);
		MarkPartitionDirty();
	}
}

FTransform AActor::GetActorTransform() const
{
	return RootComponent ? RootComponent->GetWorldTransform() : FTransform();
}

void AActor::SetActorLocation(const FVector& NewLocation)
{
	if (RootComponent == nullptr)
	{
		return;
	}

	FVector OldLocation = RootComponent->GetWorldLocation();
	if (!(OldLocation == NewLocation)) // 위치가 실제로 변경되었을 때만
	{
		RootComponent->SetWorldLocation(NewLocation);
		MarkPartitionDirty();
	}
}

FVector AActor::GetActorLocation() const
{
	return RootComponent ? RootComponent->GetWorldLocation() : FVector();
}

void AActor::MarkPartitionDirty()
{
	if(GetWorld() && GetWorld()->GetPartitionManager())
		GetWorld()->GetPartitionManager()->MarkDirty(this);
}

void AActor::SetActorRotation(const FVector& EulerDegree)
{
	if (RootComponent)
	{
		FQuat NewRotation = FQuat::MakeFromEuler(EulerDegree);
		FQuat OldRotation = RootComponent->GetWorldRotation();
		if (!(OldRotation == NewRotation)) // 회전이 실제로 변경되었을 때만
		{
			RootComponent->SetWorldRotation(NewRotation);
			MarkPartitionDirty();
		}
	}
}

void AActor::SetActorRotation(const FQuat& InQuat)
{
	if (RootComponent == nullptr)
	{
		return;
	}
	FQuat OldRotation = RootComponent->GetWorldRotation();
	if (!(OldRotation == InQuat)) // 회전이 실제로 변경되었을 때만
	{
		RootComponent->SetWorldRotation(InQuat);
		MarkPartitionDirty();
	}
}

FQuat AActor::GetActorRotation() const
{
	return RootComponent ? RootComponent->GetWorldRotation() : FQuat();
}

void AActor::SetActorScale(const FVector& NewScale)
{
	if (RootComponent == nullptr)
	{
		return;
	}

	FVector OldScale = RootComponent->GetWorldScale();
	if (!(OldScale == NewScale)) // 스케일이 실제로 변경되었을 때만
	{
		RootComponent->SetWorldScale(NewScale);
		MarkPartitionDirty();
	}
}

FVector AActor::GetActorScale() const
{
	return RootComponent ? RootComponent->GetWorldScale() : FVector(1, 1, 1);
}

FMatrix AActor::GetWorldMatrix() const
{
	if (RootComponent == nullptr)
	{
		UE_LOG("RootComponent is nullptr");
	}
	return RootComponent ? RootComponent->GetWorldMatrix() : FMatrix::Identity();
}

void AActor::AddActorWorldRotation(const FQuat& DeltaRotation)
{
	if (RootComponent && !DeltaRotation.IsIdentity()) // 단위 쿼터니온이 아닐 때만
	{
		RootComponent->AddWorldRotation(DeltaRotation);
		MarkPartitionDirty();
	}
}

void AActor::AddActorWorldRotation(const FVector& DeltaEuler)
{
	/* if (RootComponent)
	 {
		 FQuat DeltaQuat = FQuat::FromEuler(DeltaEuler.X, DeltaEuler.Y, DeltaEuler.Z);
		 RootComponent->AddWorldRotation(DeltaQuat);
	 }*/
}

void AActor::AddActorWorldLocation(const FVector& DeltaLocation)
{
	if (RootComponent && !DeltaLocation.IsZero()) // 영 벡터가 아닐 때만
	{
		RootComponent->AddWorldOffset(DeltaLocation);
		MarkPartitionDirty();
	}
}

void AActor::AddActorLocalRotation(const FVector& DeltaEuler)
{
	/*  if (RootComponent)
	  {
		  FQuat DeltaQuat = FQuat::FromEuler(DeltaEuler.X, DeltaEuler.Y, DeltaEuler.Z);
		  RootComponent->AddLocalRotation(DeltaQuat);
	  }*/
}

void AActor::AddActorLocalRotation(const FQuat& DeltaRotation)
{
	if (RootComponent && !DeltaRotation.IsIdentity()) // 단위 쿼터니온이 아닐 때만
	{
		RootComponent->AddLocalRotation(DeltaRotation);
		if (GetWorld() && GetWorld()->GetPartitionManager())
			GetWorld()->GetPartitionManager()->MarkDirty(this);
	}
}

void AActor::AddActorLocalLocation(const FVector& DeltaLocation)
{
	if (RootComponent && !DeltaLocation.IsZero()) // 영 벡터가 아닐 때만
	{
		RootComponent->AddLocalOffset(DeltaLocation);
		MarkPartitionDirty();
	}
}

void AActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	bIsPicked = false;
	bCanEverTick = true;
	bHiddenInGame = false;
	bIsCulled = false;

	RootComponent = RootComponent->Duplicate();
	CollisionComponent = CollisionComponent->Duplicate();
	TextComp = TextComp->Duplicate();

	RootComponent->SetOwner(this);
	CollisionComponent->SetOwner(this);
	TextComp->SetOwner(this);

	World = nullptr; // TODO: World를 PIE World로 할당해야 함.

	/*for (USceneComponent*& Component : SceneComponents)
	{
		Component = Component->Duplicate();
		Component->SetOwner(this);
		Component->
	}*/

	SceneComponents[0] = RootComponent;
	SceneComponents[1] = CollisionComponent;
	SceneComponents[2] = TextComp;
	for (int i = 3; i < SceneComponents.size(); ++i)
	{
		SceneComponents[i] = SceneComponents[i]->Duplicate();
		SceneComponents[i]->SetOwner(this);
	}
	for (int i = 1; i < SceneComponents.size(); ++i)
	{
		SceneComponents[i]->SetParent(RootComponent);
	}
}

//AActor* AActor::Duplicate()
//{
//	AActor* NewActor = ObjectFactory::DuplicateObject<AActor>(this); // 모든 멤버 얕은 복사
//
//	NewActor->DuplicateSubObjects();
//
//	return nullptr;
//}

void AActor::RegisterComponentTree(USceneComponent* SceneComp)
{
	if (!SceneComp)
	{
		return;
	}
	// 씬 그래프 등록(월드에 등록/렌더 시스템 캐시 등)
	SceneComp->RegisterComponent();
	// 자식들도 재귀적으로
	for (USceneComponent* Child : SceneComp->GetAttachChildren())
	{
		RegisterComponentTree(Child);
	}
}

void AActor::UnregisterComponentTree(USceneComponent* SceneComp)
{
	if (!SceneComp)
	{
		return;
	}
	for (USceneComponent* Child : SceneComp->GetAttachChildren())
	{
		UnregisterComponentTree(Child);
	}
	SceneComp->UnregisterComponent();
}
