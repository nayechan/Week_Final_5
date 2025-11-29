#include "pch.h"
#include "PhysicsConstraintTemplate.h"

// --- 생성자/소멸자 ---

UPhysicsConstraintTemplate::UPhysicsConstraintTemplate()
{
}

UPhysicsConstraintTemplate::~UPhysicsConstraintTemplate()
{
}

// --- 직렬화 ---

void UPhysicsConstraintTemplate::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // TODO: Constraint 데이터 직렬화 구현
}
