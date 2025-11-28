#pragma once
#include "PhysicsSystem.h"

// 전방 선언
class UPrimitiveComponent;
class UPhysicalMaterial;

struct FBodyInstance
{
public:
    // --- 소유자 정보 ---
    UPrimitiveComponent* OwnerComponent = nullptr;
    FName BoneName = "None"; // 랙돌용 뼈 이름
    int32 BoneIndex = -1;

    // --- PhysX 객체 ---
    physx::PxRigidActor* RigidActor = nullptr;

    // --- 설정값 (에디터에서 수정할 변수들) ---
    bool bSimulatePhysics = false; // true=움직임, false=고정
    bool bUseGravity = true;
    bool bOverrideMass = false;
    float MassInKg = 10.0f;
    float LinearDamping = 0.01f;  // 공기 저항
    float AngularDamping = 0.05f; // 회전 저항
    
    // 트리거 설정
    bool bIsTrigger;

    // --- 생성자/소멸자 ---
    explicit FBodyInstance(UPrimitiveComponent* Owner) : OwnerComponent(Owner), bIsTrigger(false) {}
    ~FBodyInstance() { TermBody(); }

    void InitBody(const FTransform& Transform, const physx::PxGeometry& Geometry, UPhysicalMaterial* PhysMat);
    void TermBody();
    void AddForce(const FVector& Force);
    void SetLinearVelocity(const FVector& Velocity);
    FTransform GetWorldTransform() const;
};
