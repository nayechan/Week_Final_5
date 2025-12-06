#pragma once

#include "ActorComponent.h"
#include "UItemComponent.generated.h"

/**
 * 아이템 타입 열거형
 */
UENUM()
enum class EItemType : uint8
{
    /** 아이템 */
    Item = 0,

    /** 사람 */
    Person = 1,
};

/**
 * 아이템 컴포넌트
 * - 액터에 부착하여 아이템 정보를 저장
 * - Lua에서 접근 가능한 프로퍼티 제공
 * - 하이라이트 상태 관리
 */
UCLASS(DisplayName="아이템 컴포넌트", Description="아이템 정보를 저장하는 컴포넌트입니다")
class UItemComponent : public UActorComponent
{
public:
    GENERATED_REFLECTION_BODY()

    UItemComponent();
    virtual ~UItemComponent() override;

    // ===== 에디터/Lua 노출 프로퍼티 =====

    /** 아이템 이름 */
    UPROPERTY(EditAnywhere, Category="Item")
    FString ItemName;

    /** 아이템 타입 */
    UPROPERTY(EditAnywhere, Category="Item")
    EItemType ItemType = EItemType::Item;

    /** 아이템 수량 */
    UPROPERTY(EditAnywhere, Category="Item")
    int32 Quantity = 1;

    /** 획득 가능 여부 */
    UPROPERTY(EditAnywhere, Category="Item")
    bool bCanPickUp = true;

    /** 하이라이트 상태 (런타임) */
    UPROPERTY(EditAnywhere, Category="Item")
    bool bIsHighlighted = false;

    // ===== Getter/Setter =====

    const FString& GetItemName() const { return ItemName; }
    void SetItemName(const FString& InName) { ItemName = InName; }

    EItemType GetItemType() const { return ItemType; }
    void SetItemType(EItemType InType) { ItemType = InType; }

    int32 GetQuantity() const { return Quantity; }
    void SetQuantity(int32 InQuantity) { Quantity = InQuantity; }

    bool CanPickUp() const { return bCanPickUp; }
    void SetCanPickUp(bool bInCanPickUp) { bCanPickUp = bInCanPickUp; }

    bool IsHighlighted() const { return bIsHighlighted; }
    void SetHighlighted(bool bInHighlighted);
};
