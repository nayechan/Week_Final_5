#include "pch.h"
#include "SelectionManager.h"
#include "Actor.h"
#include "SceneComponent.h"

IMPLEMENT_CLASS(USelectionManager)

void USelectionManager::SelectActor(AActor* Actor)
{
    if (!Actor) return;
    
    // 이미 선택되어 있는지 확인
    if (IsActorSelected(Actor)) return;
    
    // 단일 선택 모드 (기존 선택 해제)
    ClearSelection();
    
    // 새 액터 선택
    SelectedActors.Add(Actor);
    SelectedComponent = Actor->GetRootComponent();
}

void USelectionManager::SelectComponent(USceneComponent* Component)
{

    if (!Component)
    {
        return;
    }
    AActor* SelectedActor = Component->GetOwner();
    //이미 엑터가 피킹되어 있는 상황, 어느 컴포넌트든 선택 가능
    if (IsActorSelected(SelectedActor))
    {
        SelectedComponent = Component;
    }
    //오너 엑터가 선택 안돼있음, 루트 컴포넌트 피킹
    else
    {
        ClearSelection();
        SelectedActors.Add(SelectedActor);
        SelectedComponent = SelectedActor->GetRootComponent();
    }
    
}

void USelectionManager::DeselectActor(AActor* Actor)
{
    if (!Actor) return;
    
    auto it = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (it != SelectedActors.end())
    {
        SelectedActors.erase(it);
    }
    SelectedComponent = nullptr;
}

void USelectionManager::ClearSelection()
{
    for (AActor* Actor : SelectedActors)
    {
        if (Actor) // null 체크 추가
        {
            Actor->SetIsPicked(false);
        }
    }
    SelectedActors.clear();
    SelectedComponent = nullptr;
}

bool USelectionManager::IsActorSelected(AActor* Actor) const
{
    if (!Actor) return false;
    
    return std::find(SelectedActors.begin(), SelectedActors.end(), Actor) != SelectedActors.end();
}

AActor* USelectionManager::GetSelectedActor() const
{
    // 첫 번째 유효한 액터 연기
    for (AActor* Actor : SelectedActors)
    {
        if (Actor) return Actor;
    }
    return nullptr;
}

void USelectionManager::CleanupInvalidActors()
{

    // null이거나 삭제된 액터들을 제거
    auto it = std::remove_if(SelectedActors.begin(), SelectedActors.end(), 
        [](AActor* Actor) { return Actor == nullptr; });
    SelectedActors.erase(it, SelectedActors.end());
}

USelectionManager::USelectionManager()
{
    SelectedActors.Reserve(1);
}

USelectionManager::~USelectionManager()
{
    // No-op: instances are destroyed by ObjectFactory::DeleteAll
}
