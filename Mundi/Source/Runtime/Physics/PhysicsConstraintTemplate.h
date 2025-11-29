#pragma once
#include "Object.h"
#include "ConstraintInstance.h"
#include "UPhysicsConstraintTemplate.generated.h"

/**
 * UPhysicsConstraintTemplate
 *
 * 두 본 사이의 물리 조인트(관절) 설정 템플릿.
 * PhysicsAsset에서 사용되어 랙돌의 관절 제한을 정의.
 *
 * 예: 팔꿈치는 한 방향으로만 꺾임, 어깨는 넓은 범위로 회전 가능
 */
UCLASS(DisplayName="Physics Constraint Template", Description="물리 조인트 템플릿")
class UPhysicsConstraintTemplate : public UObject
{
    GENERATED_REFLECTION_BODY()

public:
    // 조인트 설정 데이터
    UPROPERTY(EditAnywhere, Category="Constraint")
    FConstraintInstance DefaultInstance;

    // --- 생성자/소멸자 ---
    UPhysicsConstraintTemplate();
    virtual ~UPhysicsConstraintTemplate();

    // --- 유틸리티 ---

    // 조인트 이름 가져오기
    FName GetJointName() const { return DefaultInstance.JointName; }

    // 연결된 본 이름 가져오기
    FName GetBone1Name() const { return DefaultInstance.ConstraintBone1; }
    FName GetBone2Name() const { return DefaultInstance.ConstraintBone2; }

    // --- 직렬화 ---
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
