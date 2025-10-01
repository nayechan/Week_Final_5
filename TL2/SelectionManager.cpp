#include "pch.h"
#include "SelectionManager.h"
#include "Actor.h"

void USelectionManager::SelectActor(AActor* Actor)
{
    if (!Actor) return;
    
    // 이미 선택되어 있는지 확인
    if (IsActorSelected(Actor)) return;
    
    // 단일 선택 모드 (기존 선택 해제)
    ClearSelection();
    
    // 새 액터 선택
    SelectedActors.Add(Actor);
}

void USelectionManager::DeselectActor(AActor* Actor)
{
    if (!Actor) return;
    
    auto it = std::find(SelectedActors.begin(), SelectedActors.end(), Actor);
    if (it != SelectedActors.end())
    {
        SelectedActors.erase(it);
    }
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
