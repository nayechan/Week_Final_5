#include "pch.h"
#include "Actor.h"
#include "SceneComponent.h"
#include "ObjectFactory.h"
#include "MeshComponent.h"
#include "TextRenderComponent.h"
#include "WorldPartitionManager.h"
#include "BillboardComponent.h"
#include "AABB.h"
#include "JsonSerializer.h"
#include "World.h"
#include "PrimitiveComponent.h"

IMPLEMENT_CLASS(AActor)

AActor::AActor()
{
	Name = "DefaultActor";
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
	LuaGameObject = new FGameObject();
	LuaGameObject->Location = GetActorLocation();
	// 그냥 테스트입니다. 수정해주세요
	LuaGameObject->Velocity = FVector(10, 0, 0);
	LuaGameObject->Scale = GetActorScale();
	
	// 컴포넌트들 Initialize/BeginPlay 순회
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->InitializeComponent();
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->BeginPlay();
}

void AActor::Tick(float DeltaSeconds)
{
	// 에디터에서 틱 Off면 스킵
	if (!bTickInEditor && World->bPie == false) return;
	
	for (UActorComponent* Comp : OwnedComponents)
	{
		if (Comp && Comp->IsComponentTickEnabled())
		{
			Comp->TickComponent(DeltaSeconds /*, … 필요 인자*/);
		}
	}

	if (LuaGameObject)
	{
		SetActorLocation(LuaGameObject->Location);
		SetActorScale(LuaGameObject->Scale);
	}
}
void AActor::EndPlay()
{
	for (UActorComponent* Comp : OwnedComponents)
		if (Comp) Comp->EndPlay();
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
	EndPlay();
	UnregisterAllComponents(true);
	DestroyAllComponents();
	// 최종 delete (ObjectFactory가 소유권 관리 중이면 그 경로 사용)
	ObjectFactory::DeleteObject(this);
}

void AActor::SetRootComponent(USceneComponent* InRoot)
{
	if (RootComponent == InRoot)
	{
		return;
	}

	// 루트 교체
	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = InRoot;
	RemoveOwnedComponent(TempRootComponent);

	if (RootComponent)
	{
		RootComponent->SetOwner(this);
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

	if (!OwnedComponents.count(Component) || RootComponent == Component)
	{
		return;
	}

	if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
	{
		// 자식 컴포넌트들을 먼저 재귀적으로 삭제
		// (자식을 먼저 삭제하면 부모의 AttachChildren이 변경되므로 복사본으로 순회)
		TArray<USceneComponent*> ChildrenCopy = SceneComponent->GetAttachChildren();
		for (USceneComponent* Child : ChildrenCopy)
		{
			RemoveOwnedComponent(Child); // 재귀 호출로 자식들 먼저 삭제
		}

		if (SceneComponent == RootComponent)
		{
			RootComponent = nullptr;
		}

		SceneComponents.Remove(SceneComponent);
		GWorld->GetPartitionManager()->Unregister(SceneComponent);
		SceneComponent->DetachFromParent(true);
	}

	// OwnedComponents에서 제거
	OwnedComponents.erase(Component);

	Component->UnregisterComponent();
	Component->DestroyComponent();
}

void AActor::RegisterAllComponents(UWorld* InWorld)
{
	for (UActorComponent* Component : OwnedComponents)
	{
		Component->RegisterComponent(InWorld);
	}
	if (!RootComponent)
	{
		RootComponent = CreateDefaultSubobject<USceneComponent>("DefaultSceneComponent");
		RootComponent->RegisterComponent(InWorld);
	}
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
			C->EndPlay();
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
		FQuat NewRotation = FQuat::MakeFromEulerZYX(EulerDegree);
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
		MarkPartitionDirty();
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

void AActor::SetActorHiddenInEditor(bool bNewHidden)
{
	bHiddenInEditor = bNewHidden; 
	GWorld->GetLightManager()->SetDirtyFlag();
}

bool AActor::IsActorVisible() const
{
	return GWorld->bPie ? !bHiddenInGame : !bHiddenInEditor;
}

void AActor::PostDuplicate()
{
	Super::PostDuplicate();

	for (UActorComponent* Comp : OwnedComponents)
	{
		if (Comp)
		{
			Comp->SetRegistered(false);
		}
	}
}

bool AActor::IsOverlappingActor(const AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    for (UActorComponent* OwnedComp : OwnedComponents)
    {
        if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(OwnedComp))
        {
            if (PrimComp->GetOverlapInfos().Num() > 0 && PrimComp->IsOverlappingActor(Other))
            {
                return true;
            }
        }
    }
    return false;
}

void AActor::OnBeginOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
	UE_LOG("On Begin Overlap");
}

void AActor::OnEndOverlap(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
	UE_LOG("On End Overlap");
}
void AActor::OnHit(UPrimitiveComponent* MyComp, UPrimitiveComponent* OtherComp)
{
	UE_LOG("On Hit");
} 


void AActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// 기본 프로퍼티 초기화
	bIsPicked = false;
	bCanEverTick = true;
	bHiddenInEditor = false;
	bIsCulled = false;
	World = nullptr; // PIE World는 복제 프로세스의 상위 레벨에서 설정해 주어야 합니다.

	if (OwnedComponents.empty())
	{
		return; // 복제할 컴포넌트가 없으면 종료
	}

	// ========================================================================
	// 1단계: 모든 컴포넌트 복제 및 '원본 -> 사본' 매핑 테이블 생성
	// ========================================================================
	TMap<UActorComponent*, UActorComponent*> OldToNewComponentMap;
	TSet<UActorComponent*> NewOwnedComponents;

	for (UActorComponent* OriginalComp : OwnedComponents)
	{
		if (!OriginalComp) continue;

		// NOTE: 이 코드가 없으면 Direction을 나타내는 GizmoComponent가 PIE World에 복사되는 버그가 발생
		// PIE 모드로 복사할 때, 에디터 전용 컴포넌트(bIsEditable == false)는 복사하지 않음
		// BillboardComponent, DirectionGizmo(GizmoArrowComponent) 등이 CREATE_EDITOR_COMPONENT로 생성되며 bIsEditable = false
		if (!OriginalComp->IsEditable())
		{
			continue; // 에디터 전용 컴포넌트는 PIE World로 복사하지 않음
		}
		// 컴포넌트를 깊은 복사합니다.
		UActorComponent* NewComp = OriginalComp->Duplicate();
		NewComp->SetOwner(this);
		
		// 매핑 테이블에 (원본 포인터, 새 포인터) 쌍을 기록합니다.
		OldToNewComponentMap.insert({ OriginalComp, NewComp });

		// 새로운 소유 컴포넌트 목록에 추가합니다.
		NewOwnedComponents.insert(NewComp);
	}

	// 복제된 컴포넌트 목록으로 교체합니다.
	OwnedComponents = NewOwnedComponents;

	// ========================================================================
	// 2단계: 매핑 테이블을 이용해 씬 계층 구조 재구성
	// ========================================================================

	// 2-1. 새로운 루트 컴포넌트 설정
	UActorComponent** FoundNewRootPtr = OldToNewComponentMap.Find(RootComponent);
	if (FoundNewRootPtr)
	{
		RootComponent = Cast<USceneComponent>(*FoundNewRootPtr);
	}
	else
	{
		// 원본 루트를 찾지 못하는 심각한 오류. 
		// 이 경우엔 첫 번째 씬 컴포넌트를 임시 루트로 삼거나 에러 처리.
		RootComponent = nullptr;
	}

	// 2-2. 모든 씬 컴포넌트의 부모-자식 관계 재연결
	SceneComponents.clear(); // 새 컴포넌트로 목록을 다시 채웁니다.
	if (RootComponent)
	{
		SceneComponents.push_back(RootComponent);
	}

	for (auto const& [OriginalComp, NewComp] : OldToNewComponentMap)
	{
		USceneComponent* OriginalSceneComp = Cast<USceneComponent>(OriginalComp);
		USceneComponent* NewSceneComp = Cast<USceneComponent>(NewComp);

		if (OriginalSceneComp && NewSceneComp)
		{
			// 루트가 아닌 경우에만 부모를 찾아 연결합니다.
			if (OriginalSceneComp != GetRootComponent()) // 여기서 비교는 원본 액터의 루트와 해야 합니다.
			{
				USceneComponent* OriginalParent = OriginalSceneComp->GetAttachParent();
				if (OriginalParent)
				{
					// 매핑 테이블에서 원본 부모에 해당하는 '새로운 부모'를 찾습니다.
					UActorComponent** FoundNewParentPtr = OldToNewComponentMap.Find(OriginalParent);
					if (FoundNewParentPtr)
					{
						if (USceneComponent* ParentSceneComponent = Cast<USceneComponent>(*FoundNewParentPtr))
						{
							NewSceneComp->SetupAttachment(ParentSceneComponent, EAttachmentRule::KeepRelative);
						}
					}
				}
			}

			// 루트가 아닌 씬 컴포넌트들을 캐시 목록에 추가합니다.
			if (NewSceneComp != RootComponent)
			{
				SceneComponents.push_back(NewSceneComp);
			}
		}
	}
}



void AActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// 액터 생성자에서 만들어진 컴포넌트를 무시하고 저장된 컴포넌트만 다시 붙인다
		UnregisterAllComponents();
		DestroyAllComponents();

		uint32 RootUUID;
		FJsonSerializer::ReadUint32(InOutHandle, "RootComponentId", RootUUID);
	
		FString NameStrTemp;
		FJsonSerializer::ReadString(InOutHandle, "Name", NameStrTemp);
		SetName(NameStrTemp);
	
		JSON ComponentsJson;
		if (FJsonSerializer::ReadArray(InOutHandle, "OwnedComponents", ComponentsJson))
		{
			// 1) OwnedComponents와 SceneComponents에 Component들 추가
			for (uint32 i = 0; i < static_cast<uint32>(ComponentsJson.size()); ++i)
			{
				JSON ComponentJson = ComponentsJson.at(i);
				
				FString TypeString;
				FJsonSerializer::ReadString(ComponentJson, "Type", TypeString);
	
				UClass* NewClass = UClass::FindClass(TypeString);
	
				UActorComponent* NewComponent = Cast<UActorComponent>(ObjectFactory::NewObject(NewClass));
	
				NewComponent->Serialize(bInIsLoading, ComponentJson);
	
				// RootComponent 설정
				if (USceneComponent* NewSceneComponent = Cast<USceneComponent>(NewComponent))
				{
					if (RootUUID == NewSceneComponent->GetSceneId())
					{
						assert(NewSceneComponent);
						SetRootComponent(NewSceneComponent);
					}
				}
	
				// OwnedComponents와 SceneComponents에 Component 추가
				AddOwnedComponent(NewComponent);
			}
	
			// 2) 컴포넌트 간 부모 자식 관계 설정
			for (auto& Component : OwnedComponents)
			{
				USceneComponent* SceneComp = Cast<USceneComponent>(Component);
				if (!SceneComp)
				{
					continue;
				}
				uint32 ParentId = SceneComp->GetParentId();
				if (ParentId != 0) // RootComponent가 아니면 부모 설정
				{
					USceneComponent** ParentP = SceneComp->GetSceneIdMap().Find(ParentId);
					USceneComponent* Parent = *ParentP;
	
					SceneComp->SetupAttachment(Parent, EAttachmentRule::KeepRelative);
				}
			}
		}
	}
	else if (RootComponent)
	{
		InOutHandle["RootComponentId"] = RootComponent->UUID;
	
		JSON Components = JSON::Make(JSON::Class::Array);
		for (auto& Component : OwnedComponents)
		{
			// 에디터 전용 컴포넌트는 직렬화하지 않음
			// (CREATE_EDITOR_COMPONENT로 생성된 컴포넌트들은 OnRegister()에서 매번 새로 생성됨)
			if (!Component->IsEditable())
			{
				continue;
			}
	
			JSON ComponentJson;
	
			ComponentJson["Type"] = Component->GetClass()->Name;
	
			Component->Serialize(bInIsLoading, ComponentJson);
			Components.append(ComponentJson);
		}
		InOutHandle["OwnedComponents"] = Components;
		InOutHandle["Name"] = GetName().ToString();
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

void AActor::RegisterComponentTree(USceneComponent* SceneComp, UWorld* InWorld)
{
	if (!SceneComp)
	{
		return;
	}
	// 씬 그래프 등록(월드에 등록/렌더 시스템 캐시 등)
	SceneComp->RegisterComponent(InWorld);
	// 자식들도 재귀적으로
	for (USceneComponent* Child : SceneComp->GetAttachChildren())
	{
		RegisterComponentTree(Child, InWorld);
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
