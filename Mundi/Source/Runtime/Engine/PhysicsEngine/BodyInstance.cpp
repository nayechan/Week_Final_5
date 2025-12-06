#include "pch.h"
#include "BodyInstance.h"
#include "BodySetup.h"
#include "PhysScene.h"
#include "World.h"
#include "PrimitiveComponent.h"

#ifdef _EDITOR
#include "EditorEngine.h"
extern UEditorEngine GEngine;
#endif

FBodyInstance::FBodyInstance()
    : FPhysicsUserData(EPhysicsUserDataType::BodyInstance)
    , OwnerComponent(nullptr)
    , BodySetup(nullptr)
    , PhysScene(nullptr)
    , RigidActor(nullptr)
    , PhysicalMaterialOverride(nullptr)
    , Scale3D(FVector(1.f, 1.f, 1.f))
    , bSimulatePhysics(false)
    , LinearDamping(0.01f)
    , AngularDamping(0.f)
    , MassInKgOverride(100.0f)
    , bOverrideMass(false)
{
}

FBodyInstance::FBodyInstance(const FBodyInstance& Other)
    : FPhysicsUserData(EPhysicsUserDataType::BodyInstance)
    , OwnerComponent(nullptr)   // 런타임 포인터는 복사하지 않음
    , BodySetup(nullptr)        // InitBody에서 새로 설정됨
    , PhysScene(nullptr)        // InitBody에서 새로 설정됨
    , RigidActor(nullptr)       // InitBody에서 새로 생성됨
    , LifeHandle(nullptr)       // 복사되면 안 됨
    , PhysicalMaterialOverride(Other.PhysicalMaterialOverride)
    , Scale3D(Other.Scale3D)
    , bSimulatePhysics(Other.bSimulatePhysics)
    , LinearDamping(Other.LinearDamping)
    , AngularDamping(Other.AngularDamping)
    , MassInKgOverride(Other.MassInKgOverride)
    , bOverrideMass(Other.bOverrideMass)
{
    // PhysX 리소스는 복사하지 않음 - 새 컴포넌트에서 InitBody를 호출해야 함
}

FBodyInstance& FBodyInstance::operator=(const FBodyInstance& Other)
{
    if (this != &Other)
    {
        // 기존 PhysX 리소스 정리
        TermBody();

        // 값 복사 (런타임 포인터 제외)
        PhysicalMaterialOverride = Other.PhysicalMaterialOverride;
        Scale3D = Other.Scale3D;
        bSimulatePhysics = Other.bSimulatePhysics;
        LinearDamping = Other.LinearDamping;
        AngularDamping = Other.AngularDamping;
        MassInKgOverride = Other.MassInKgOverride;
        bOverrideMass = Other.bOverrideMass;

        // 런타임 포인터는 nullptr로 유지 - InitBody 호출 필요
        LifeHandle = nullptr;
        OwnerComponent = nullptr;
        BodySetup = nullptr;
        PhysScene = nullptr;
        RigidActor = nullptr;
    }
    return *this;
}

FBodyInstance::~FBodyInstance()
{
    // 소멸 시 반드시 TermBody 호출하여 PhysX 리소스 정리
    TermBody();
}

void FBodyInstance::InitBody(UBodySetup* Setup, const FTransform& Transform, UPrimitiveComponent* Component, FPhysScene* InRBScene, PxAggregate* InAggregate)
{
    if (!Setup || !InRBScene || !GPhysXSDK)
    {
        UE_LOG("[PhysX Error] InitBody에 실패했습니다.");
        return;
    }

    if (IsValidBodyInstance())
    {
        TermBody();
    }

    LifeHandle = std::make_shared<bool>(true);

    BodySetup       = Setup;
    OwnerComponent  = Component;
    PhysScene       = InRBScene;
    Scale3D         = Transform.Scale3D;

    PxTransform PhysicsTransform = U2PTransform(Transform);

    // 컴포넌트의 Mobility에 따라 Actor 타입 결정
    // Static: PxRigidStatic (움직이지 않음, TriangleMesh 허용)
    // Movable: PxRigidDynamic (bSimulatePhysics에 따라 Dynamic/Kinematic 동작)
    EComponentMobility ComponentMobility = Component ? Component->Mobility : EComponentMobility::Movable;
    switch (ComponentMobility)
    {
    case EComponentMobility::Static:
        RigidActor = GPhysXSDK->createRigidStatic(PhysicsTransform);
        break;
    case EComponentMobility::Movable:
        RigidActor = GPhysXSDK->createRigidDynamic(PhysicsTransform);
        break;
    }

    if (!RigidActor)
    {
        UE_LOG("[PhysX Error] RigidActor 생성에 실패했습니다.");
        return;
    }

    RigidActor->userData = this;

    UPhysicalMaterial* PhysicalMaterial = GetSimplePhysicalMaterial();

    {
        SCOPED_SCENE_WRITE_LOCK(PhysScene->GetPxScene());
        Setup->AddShapesToRigidActor_AssumesLocked(this, Scale3D, RigidActor, PhysicalMaterial);
    }

    if (IsDynamic() || IsKinematic())
    {
        PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();

        // Kinematic 모드 설정 (bSimulatePhysics=false이면 Kinematic)
        bool bShouldBeKinematic = !bSimulatePhysics;

        // 에디터 모드에서는 bTickInEditor가 true가 아니면 시뮬레이션 비활성화 (Kinematic으로 설정)
        // PIE 모드이거나 bTickInEditor가 true인 경우에만 실제 시뮬레이션 수행
        // 단, 래그돌 바디는 에디터에서도 시뮬레이션 허용 (Physics Asset Editor 등에서 사용)
#ifdef _EDITOR
        if (!bShouldBeKinematic && Component && !bIsRagdollBody)
        {
            UWorld* World = Component->GetWorld();
            bool bIsPIE = (World && World->bPie) || GEngine.IsPIEActive();
            bool bCanTickInEditor = Component->CanTickInEditor();

            // 에디터 모드(PIE 아님)이고 에디터에서 틱하지 않으면 Kinematic으로 강제 설정
            if (!bIsPIE && !bCanTickInEditor)
            {
                bShouldBeKinematic = true;
            }
        }
#endif

        if (bShouldBeKinematic)
        {
            DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, true);
        }

        if (bOverrideMass)
        {
            // 질량에 맞추어 관성 텐서 계산
            PxRigidBodyExt::setMassAndUpdateInertia(*DynamicActor, MassInKgOverride);
        }
        else
        {
            // 제공된 밀도를 통해 크기에 맞춰서 무게를 계산 (1x1x1 박스 기준 10kg)
            PxRigidBodyExt::updateMassAndInertia(*DynamicActor, 10.0f);
        }

        DynamicActor->setLinearDamping(LinearDamping);
        DynamicActor->setAngularDamping(AngularDamping);
    }

    // Shape들에 충돌 필터 데이터 설정
    {
        SCOPED_SCENE_WRITE_LOCK(PhysScene->GetPxScene());

        PxFilterData FilterData;
        FilterData.word0 = ChannelToBit(ObjectType);
        FilterData.word1 = CollisionMask;
        FilterData.word2 = 0;  // 초기 겹침 무시 마스크 (나중에 설정됨)
        FilterData.word3 = 0;  // 바디 인덱스 (나중에 설정됨)

        PxU32 NumShapes = RigidActor->getNbShapes();
        TArray<PxShape*> Shapes(NumShapes);
        RigidActor->getShapes(Shapes.GetData(), NumShapes);

        for (PxU32 i = 0; i < NumShapes; ++i)
        {
            Shapes[i]->setSimulationFilterData(FilterData);
            Shapes[i]->setQueryFilterData(FilterData);  // CCT Scene Query용
        }
    }

    // Aggregate가 있으면 Aggregate에 추가 (Scene에는 Aggregate가 추가될 때 함께 추가됨)
    // Aggregate가 없으면 Scene에 직접 추가
    if (InAggregate)
    {
        InAggregate->addActor(*RigidActor);
        OwningAggregate = InAggregate;  // Aggregate 참조 저장
    }
    else if (PhysScene->GetPxScene())
    {
        PhysScene->DeferAddActor(RigidActor);
        OwningAggregate = nullptr;
    }
}

void FBodyInstance::TermBody()
{
    LifeHandle = nullptr;

    if (RigidActor)
    {
        // userData를 먼저 클리어하여 dangling pointer 방지
        RigidActor->userData = nullptr;

        if (PhysScene && PhysScene->GetPxScene())
        {
            // 시뮬레이션 중이면 완료될 때까지 대기
            PhysScene->WaitPhysScene();
        }

        // Aggregate에 속한 Actor는 Aggregate에서 제거
        // Aggregate가 없으면 Scene에서 직접 제거
        if (OwningAggregate)
        {
            OwningAggregate->removeActor(*RigidActor);
            OwningAggregate = nullptr;
        }
        else
        {
            PxScene* ActorScene = RigidActor->getScene();
            if (ActorScene)
            {
                ActorScene->removeActor(*RigidActor);
            }
        }

        if (PhysScene)
        {
            PhysScene->DeferReleaseActor(RigidActor); 
        }
        else
        {
            RigidActor->release();
        }
        RigidActor = nullptr;
    }

    BodySetup = nullptr;
    PhysScene = nullptr;
    OwnerComponent = nullptr;
}

FTransform FBodyInstance::GetUnrealWorldTransform() const
{
    if (IsValidBodyInstance())
    {
        PxTransform PhysicsTransform = RigidActor->getGlobalPose();
        FTransform Result = P2UTransform(PhysicsTransform);
        Result.Scale3D = Scale3D;  // 저장된 스케일 복원
        return Result;
    }
    return FTransform();
}

UPhysicalMaterial* FBodyInstance::GetSimplePhysicalMaterial() const
{
    if (PhysicalMaterialOverride)
    {
        return PhysicalMaterialOverride;
    }

    if (BodySetup && BodySetup->PhysicalMaterial)
    {
        return BodySetup->PhysicalMaterial;
    }

    return nullptr;
}

void FBodyInstance::SetBodyTransform(const FTransform& NewTransform, bool bTeleport)
{
    if (!IsValidBodyInstance() || !PhysScene) { return; }

    PxScene* PScene = PhysScene->GetPxScene();
    if (!PScene) { return; }

    // Actor가 이미 Scene에 있으면 즉시 실행, 없으면 Enqueue
    if (RigidActor->getScene())
    {
        SCOPED_SCENE_WRITE_LOCK(PScene);

        PxTransform PNewTransform = U2PTransform(NewTransform);
        RigidActor->setGlobalPose(PNewTransform);

        if (bTeleport && IsDynamic())
        {
            PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
            if (DynamicActor)
            {
                DynamicActor->setLinearVelocity(PxVec3(0.0f));
                DynamicActor->setAngularVelocity(PxVec3(0.0f));
                DynamicActor->wakeUp();
            }
        }
    }
    else
    {
        std::weak_ptr<bool> WeakHandle = LifeHandle;

        PhysScene->EnqueueCommand([this, NewTransform, bTeleport, WeakHandle]()
        {
            if (WeakHandle.expired()) return;
            if (!IsValidBodyInstance() || !PhysScene) return;

            PxTransform PNewTransform = U2PTransform(NewTransform);
            RigidActor->setGlobalPose(PNewTransform);

            if (bTeleport && IsDynamic())
            {
                PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
                if (DynamicActor)
                {
                    DynamicActor->setLinearVelocity(PxVec3(0.0f));
                    DynamicActor->setAngularVelocity(PxVec3(0.0f));
                    DynamicActor->wakeUp();
                }
            }
        });
    }
}

void FBodyInstance::SetLinearVelocity(const FVector& NewVel, bool bAddToCurrent)
{
    if (!IsValidBodyInstance() || !PhysScene) return;

    auto SetVelocityImpl = [this, NewVel, bAddToCurrent]()
    {
        if (!IsValidBodyInstance()) return;

        if (IsDynamic())
        {
            PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
            if (!DynamicActor) return;

            if (DynamicActor->getRigidBodyFlags() & PxRigidBodyFlag::eKINEMATIC)
            {
                DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, false);
            }

            if (bAddToCurrent)
            {
                PxVec3 CurrentVel = DynamicActor->getLinearVelocity();
                DynamicActor->setLinearVelocity(CurrentVel + U2PVector(NewVel));
            }
            else
            {
                DynamicActor->setLinearVelocity(U2PVector(NewVel));
            }
            DynamicActor->wakeUp();
        }
    };

    // Actor가 이미 Scene에 있으면 즉시 실행, 없으면 Enqueue
    if (RigidActor->getScene())
    {
        SCOPED_SCENE_WRITE_LOCK(PhysScene->GetPxScene());
        SetVelocityImpl();
    }
    else
    {
        std::weak_ptr<bool> WeakHandle = LifeHandle;
        PhysScene->EnqueueCommand([this, SetVelocityImpl, WeakHandle]()
        {
            if (WeakHandle.expired()) return;
            SetVelocityImpl();
        });
    }
}

void FBodyInstance::AddForce(const FVector& Force, bool bAccelChange)
{
    if (!IsValidBodyInstance() || !PhysScene) return;

    std::weak_ptr<bool> WeakHandle = LifeHandle;

    PhysScene->EnqueueCommand([this, Force, bAccelChange, WeakHandle]()
    {
        if (WeakHandle.expired())
        {
            return;
        }

        if (!IsValidBodyInstance()) return;

        if (IsDynamic())
        {
            PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
            if (!DynamicActor) return;

            PxForceMode::Enum Mode = bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE;
            DynamicActor->addForce(U2PVector(Force), Mode);
            DynamicActor->wakeUp();
        }
    });
}

void FBodyInstance::AddTorque(const FVector& Torque, bool bAccelChange)
{
    if (!IsValidBodyInstance() || !PhysScene) return;

    std::weak_ptr<bool> WeakHandle = LifeHandle;

    PhysScene->EnqueueCommand([this, Torque, bAccelChange, WeakHandle]()
    {
        if (WeakHandle.expired())
        {
            return;
        }

        if (!IsValidBodyInstance()) return;

        if (IsDynamic())
        {
            PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
            if (!DynamicActor) return;

            PxForceMode::Enum Mode = bAccelChange ? PxForceMode::eACCELERATION : PxForceMode::eFORCE;
            DynamicActor->addTorque(U2PVector(Torque), Mode);
            DynamicActor->wakeUp();
        }
    });
}

void FBodyInstance::AddImpulse(const FVector& Impulse, bool bVelChange)
{
    if (!IsValidBodyInstance() || !PhysScene) return;

    std::weak_ptr<bool> WeakHandle = LifeHandle;

    PhysScene->EnqueueCommand([this, Impulse, bVelChange, WeakHandle]()
    {
        if (WeakHandle.expired())
        {
            return;
        }

        if (!IsValidBodyInstance()) return;

        if (!RigidActor->getScene()) return;

        if (IsDynamic())
        {
            PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
            if (!DynamicActor) return;

            PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;

            DynamicActor->addForce(U2PVector(Impulse), Mode);
            DynamicActor->wakeUp();
        }
    });
}

void FBodyInstance::AddAngularImpulse(const FVector& Impulse, bool bVelChange)
{
    if (!IsValidBodyInstance() || !PhysScene) return;

    std::weak_ptr<bool> WeakHandle = LifeHandle;

    PhysScene->EnqueueCommand([this, Impulse, bVelChange, WeakHandle]()
    {
        if (WeakHandle.expired())
        {
            return;
        }

        if (!IsValidBodyInstance()) return;

        if (IsDynamic())
        {
            PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
            if (!DynamicActor) return;

            PxForceMode::Enum Mode = bVelChange ? PxForceMode::eVELOCITY_CHANGE : PxForceMode::eIMPULSE;

            DynamicActor->addTorque(U2PVector(Impulse), Mode);
            DynamicActor->wakeUp();
        }
    });
}

bool FBodyInstance::IsDynamic() const
{
    // Movable이고 물리 시뮬레이션이 활성화되어 있는 경우
    if (!OwnerComponent) return false;
    return OwnerComponent->Mobility == EComponentMobility::Movable && bSimulatePhysics;
}

bool FBodyInstance::IsStatic() const
{
    if (!OwnerComponent) return false;
    return OwnerComponent->Mobility == EComponentMobility::Static;
}

bool FBodyInstance::IsKinematic() const
{
    // Movable이고 물리 시뮬레이션이 비활성화되어 있는 경우
    if (!OwnerComponent) return false;
    return OwnerComponent->Mobility == EComponentMobility::Movable && !bSimulatePhysics;
}

void FBodyInstance::SetCollisionChannel(ECollisionChannel InChannel, uint32 InMask)
{
    ObjectType = InChannel;
    CollisionMask = InMask;

    // 이미 생성된 바디가 있으면 FilterData 업데이트
    if (IsValidBodyInstance())
    {
        UpdateFilterData();
    }
}

void FBodyInstance::UpdateFilterData()
{
    if (!IsValidBodyInstance() || !PhysScene) return;

    std::weak_ptr<bool> WeakHandle = LifeHandle;

    PhysScene->EnqueueCommand([this, WeakHandle]()
    {
        if (WeakHandle.expired())
        {
            return;
        }

        if (!IsValidBodyInstance() || !PhysScene) return;

        PxScene* PScene = PhysScene->GetPxScene();
        if (!PScene) return;

        // FilterData 구조:
        // word0: 충돌 채널 비트 (이 바디의 타입)
        // word1: 충돌 마스크 (어떤 채널과 충돌할지)
        // word2: 초기 겹침으로 인한 충돌 무시 마스크
        // word3: 바디 인덱스
        PxFilterData FilterData;
        FilterData.word0 = ChannelToBit(ObjectType);
        FilterData.word1 = CollisionMask;
        // word2, word3는 기존 값 유지 (초기 겹침 필터용)

        PxU32 NumShapes = RigidActor->getNbShapes();
        std::vector<PxShape*> Shapes(NumShapes);
        RigidActor->getShapes(Shapes.data(), NumShapes);

        for (PxU32 i = 0; i < NumShapes; ++i)
        {
            PxFilterData ExistingData = Shapes[i]->getSimulationFilterData();
            FilterData.word2 = ExistingData.word2;  // 기존 겹침 무시 마스크 유지
            FilterData.word3 = ExistingData.word3;  // 기존 바디 인덱스 유지
            Shapes[i]->setSimulationFilterData(FilterData);
            Shapes[i]->setQueryFilterData(FilterData);  // CCT Scene Query용
        }

        // 필터 변경 후 Scene에 알림
        PScene->resetFiltering(*RigidActor);
    });
}

void FBodyInstance::SetKinematicTarget(const FTransform& NewTransform)
{
    if (!IsValidBodyInstance() || !IsKinematic() || !PhysScene) return;

    auto SetTargetImpl = [this, NewTransform]()
    {
        if (!IsValidBodyInstance()) return;

        PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
        if (DynamicActor)
        {
            PxTransform PxTarget = U2PTransform(NewTransform);
            DynamicActor->setKinematicTarget(PxTarget);
        }
    };

    // Actor가 이미 Scene에 있으면 즉시 실행, 없으면 Enqueue
    if (RigidActor->getScene())
    {
        SCOPED_SCENE_WRITE_LOCK(PhysScene->GetPxScene());
        SetTargetImpl();
    }
    else
    {
        std::weak_ptr<bool> WeakHandle = LifeHandle;
        PhysScene->EnqueueCommand([this, SetTargetImpl, WeakHandle]()
        {
            if (WeakHandle.expired()) return;
            SetTargetImpl();
        });
    }
}

void FBodyInstance::SetKinematic(bool bEnable)
{
    // Static은 Kinematic으로 전환 불가
    if (!OwnerComponent || OwnerComponent->Mobility == EComponentMobility::Static)
    {
        UE_LOG("[FBodyInstance] Static 바디는 Kinematic으로 전환할 수 없습니다.");
        return;
    }

    // bSimulatePhysics와 bEnable은 반대 관계
    // SetKinematic(true) → bSimulatePhysics = false
    // SetKinematic(false) → bSimulatePhysics = true
    bool bNewSimulatePhysics = !bEnable;

    // 이미 같은 상태면 무시
    if (bSimulatePhysics == bNewSimulatePhysics)
    {
        return;
    }

    bSimulatePhysics = bNewSimulatePhysics;

    if (!IsValidBodyInstance() || !PhysScene)
    {
        return;
    }

    auto SetKinematicImpl = [this, bEnable]()
    {
        if (!IsValidBodyInstance()) return;

        PxRigidDynamic* DynamicActor = RigidActor->is<PxRigidDynamic>();
        if (DynamicActor)
        {
            DynamicActor->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, bEnable);

            if (!bEnable)
            {
                // Kinematic에서 Dynamic으로 전환 시 속도 초기화
                DynamicActor->setLinearVelocity(PxVec3(0.0f));
                DynamicActor->setAngularVelocity(PxVec3(0.0f));
                DynamicActor->wakeUp();
            }
        }
    };

    // Actor가 이미 Scene에 있으면 즉시 실행, 없으면 Enqueue
    if (RigidActor->getScene())
    {
        SCOPED_SCENE_WRITE_LOCK(PhysScene->GetPxScene());
        SetKinematicImpl();
    }
    else
    {
        std::weak_ptr<bool> WeakHandle = LifeHandle;
        PhysScene->EnqueueCommand([this, SetKinematicImpl, WeakHandle]()
        {
            if (WeakHandle.expired()) return;
            SetKinematicImpl();
        });
    }
}
