#include "pch.h"
#include "ItemComponent.h"

UItemComponent::UItemComponent()
{
    bCanEverTick = false;  // 아이템 컴포넌트는 틱 불필요
}

UItemComponent::~UItemComponent()
{
}

void UItemComponent::SetHighlighted(bool bInHighlighted)
{
    if (bIsHighlighted == bInHighlighted)
    {
        return;
    }

    bIsHighlighted = bInHighlighted;

    // TODO: SceneRenderer에 하이라이트 등록/해제 요청
    // 아웃라인 셰이더 구현 후 연동 예정
}
