#pragma once
#include "Object.h"
#include "UEContainer.h"
#include "Vector.h"

// Forward Declarations
class AActor;

/**
 * SelectionManager
 * - 액터 선택 상태를 관리하는 싱글톤 클래스
 * UGameInstanceSubsystem 써야함
 */
class USelectionManager : public UObject
{
public:
    DECLARE_CLASS(USelectionManager, UObject)

    /** === 선택 관리 === */
    void SelectActor(AActor* Actor);
    void DeselectActor(AActor* Actor);
    void ClearSelection();
    
    bool IsActorSelected(AActor* Actor) const;
    
    /** === 선택된 액터 접근 === */
    AActor* GetSelectedActor() const; // 단일 선택용
    const TArray<AActor*>& GetSelectedActors() const { return SelectedActors; }
    
    int32 GetSelectionCount() const { return SelectedActors.Num(); }
    bool HasSelection() const { return SelectedActors.Num() > 0; }
    
    /** === 삭제된 액터 정리 === */
    void CleanupInvalidActors(); // null이나 삭제된 액터 제거

public:
    USelectionManager();
    ~USelectionManager() override;
protected:
    
    // 복사 금지
    USelectionManager(const USelectionManager&) = delete;
    USelectionManager& operator=(const USelectionManager&) = delete;
    
    /** === 선택된 액터들 === */
    TArray<AActor*> SelectedActors;
};
