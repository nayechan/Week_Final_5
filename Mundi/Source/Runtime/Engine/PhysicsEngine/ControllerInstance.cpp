#include "pch.h"
#include "ControllerInstance.h"

#include <cmath>

#include "PhysScene.h"
#include "CapsuleComponent.h"
#include "CharacterMovementComponent.h"
#include "CCTQueryFilterCallback.h"
#include "CCTHitReport.h"
#include "CCTBehaviorCallback.h"

// PI 상수
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ==================================================================================
// FCCTSettings
// ==================================================================================

void FCCTSettings::SetDefaults()
{
    Radius = 0.5f;
    Height = 1.0f;
    StepOffset = 0.45f;
    SlopeLimit = 0.707f;  // cos(45도)
    ContactOffset = 0.1f;
    // PhysX는 Y-up 좌표계이므로 UpDirection은 (0, 1, 0)
    UpDirection = PxVec3(0, 1, 0);
    NonWalkableMode = PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING;
}

void FCCTSettings::SetFromComponents(UCapsuleComponent* Capsule, UCharacterMovementComponent* Movement)
{
    SetDefaults();

    if (Capsule)
    {
        // 프로젝트가 cm 단위를 직접 사용 (변환 없음)
        float RadiusCm = Capsule->GetCapsuleRadius();
        float HalfHeightCm = Capsule->GetCapsuleHalfHeight();

        Radius = RadiusCm;
        Height = (HalfHeightCm * 2.0f - RadiusCm * 2.0f);  // 원통 부분만
        if (Height < 0.0f) Height = 0.01f;  // 최소값 보장
    }

    if (Movement)
    {
        // StepOffset 설정
        // 1. PhysX CCT 요구사항: stepOffset <= height + 2*radius
        // 2. 현실적인 계단 높이: 캡슐 전체 높이(Height + 2*Radius)의 약 25%
        float TotalCapsuleHeight = Height + 2.0f * Radius;
        float PhysXMaxStep = TotalCapsuleHeight;  // PhysX 제한
        float ReasonableMaxStep = TotalCapsuleHeight * 0.25f;  // 현실적인 계단 높이

        StepOffset = Movement->MaxStepHeight;

        // 현실적인 최대값으로 먼저 제한
        if (StepOffset > ReasonableMaxStep)
        {
            UE_LOG("[PhysX CCT] StepOffset(%.2f)이 권장값(%.2f, 캡슐높이의 25%%)을 초과하여 클램프됨",
                   StepOffset, ReasonableMaxStep);
            StepOffset = ReasonableMaxStep;
        }

        // PhysX 제한도 확인
        if (StepOffset > PhysXMaxStep * 0.9f)
        {
            StepOffset = PhysXMaxStep * 0.9f;
        }

        // 각도를 라디안으로 변환 후 cos 계산
        float AngleRadians = Movement->WalkableFloorAngle * static_cast<float>(M_PI) / 180.0f;
        SlopeLimit = std::cos(AngleRadians);
    }
}

// ==================================================================================
// FControllerInstance
// ==================================================================================

FControllerInstance::FControllerInstance()
    : FPhysicsUserData(EPhysicsUserDataType::ControllerInstance)
    , Controller(nullptr)
    , OwnerComponent(nullptr)
    , MovementComponent(nullptr)
    , PhysScene(nullptr)
    , CollisionChannel(ECollisionChannel::Pawn)
    , CollisionMask(CollisionMasks::DefaultPawn)
    , QueryFilter(nullptr)
    , bLastGrounded(false)
    , bLastTouchingCeiling(false)
    , bLastTouchingSide(false)
{
    CachedFilterData = PxFilterData();
}

FControllerInstance::~FControllerInstance()
{
    TermController();
}

void FControllerInstance::InitController(UCapsuleComponent* InCapsule, UCharacterMovementComponent* InMovement, FPhysScene* InPhysScene)
{
    if (!InPhysScene || !InPhysScene->GetControllerManager() || !InCapsule)
    {
        UE_LOG("[PhysX CCT] InitController 실패: 필수 파라미터가 nullptr");
        return;
    }

    OwnerComponent = InCapsule;
    MovementComponent = InMovement;
    PhysScene = InPhysScene;

    // 설정 추출
    FCCTSettings Settings;
    Settings.SetFromComponents(InCapsule, InMovement);

    // PxCapsuleControllerDesc 구성
    PxCapsuleControllerDesc Desc;
    Desc.radius = Settings.Radius;
    Desc.height = Settings.Height;
    Desc.stepOffset = Settings.StepOffset;
    Desc.slopeLimit = Settings.SlopeLimit;
    Desc.contactOffset = Settings.ContactOffset;
    Desc.upDirection = Settings.UpDirection;
    Desc.nonWalkableMode = Settings.NonWalkableMode;

    // 초기 위치 - 에디터(Z-up) → PhysX(Y-up) 좌표 변환 적용
    FVector WorldPos = InCapsule->GetWorldLocation();
    // U2PVector 변환: (X, Y, Z) → (Y, Z, -X)
    Desc.position = PxExtendedVec3(WorldPos.Y, WorldPos.Z, -WorldPos.X);

    // Material (기본 마찰/반발 계수로 생성)
    PxMaterial* DefaultMaterial = GPhysXSDK->createMaterial(0.5f, 0.5f, 0.1f);
    Desc.material = DefaultMaterial;

    // 콜백 설정
    Desc.reportCallback = InPhysScene->GetCCTHitReport();
    Desc.behaviorCallback = InPhysScene->GetCCTBehaviorCallback();

    // 사용자 데이터
    Desc.userData = this;

    // 유효성 검사
    if (!Desc.isValid())
    {
        UE_LOG("[PhysX CCT] PxCapsuleControllerDesc 유효성 검사 실패");
        UE_LOG("[PhysX CCT] Radius: %.3f, Height: %.3f, StepOffset: %.3f", Desc.radius, Desc.height, Desc.stepOffset);
        return;
    }

    // Controller 생성
    Controller = InPhysScene->GetControllerManager()->createController(Desc);
    if (!Controller)
    {
        UE_LOG("[PhysX CCT] PxController 생성 실패");
        return;
    }

    // CCT 내부 Actor의 userData에 FControllerInstance 설정
    // 이를 통해 스윕/레이캐스트에서 CCT를 Owner로 식별하여 무시할 수 있음
    PxRigidDynamic* CCTActor = Controller->getActor();
    if (CCTActor)
    {
        CCTActor->userData = this;
    }

    // QueryFilter 생성
    QueryFilter = new FCCTQueryFilterCallback();
    QueryFilter->MyChannel = CollisionChannel;
    QueryFilter->MyCollisionMask = CollisionMask;

    // FilterData 캐시 (채널을 비트로 변환)
    CachedFilterData.word0 = ChannelToBit(CollisionChannel);
    CachedFilterData.word1 = CollisionMask;

    // 초기 바닥 상태 확인을 위해 아래 방향으로 작은 이동 수행
    // 이렇게 해야 첫 프레임 점프 입력 시 bLastGrounded가 올바른 값을 가짐
    Move(FVector(0.0f, 0.0f, -0.01f), 0.0f);

    UE_LOG("[PhysX CCT] Controller 초기화 완료 (Radius: %.2f, Height: %.2f, StepOffset: %.2f, Grounded: %s)",
           Settings.Radius, Settings.Height, Settings.StepOffset, bLastGrounded ? "true" : "false");
}

void FControllerInstance::TermController()
{
    if (QueryFilter)
    {
        delete QueryFilter;
        QueryFilter = nullptr;
    }

    if (Controller)
    {
        Controller->release();
        Controller = nullptr;
    }

    OwnerComponent = nullptr;
    MovementComponent = nullptr;
    PhysScene = nullptr;
}

PxControllerCollisionFlags FControllerInstance::Move(const FVector& Displacement, float DeltaTime)
{
    if (!Controller)
    {
        return PxControllerCollisionFlags(0);
    }

    // 프로젝트가 cm 단위 직접 사용 (변환 없음)
    PxVec3 PxDisp = U2PVector(Displacement);

    // 필터 구성
    PxControllerFilters Filters = GetFilters();

    // 이동 수행
    PxControllerCollisionFlags CollisionFlags = Controller->move(PxDisp, 0.001f, DeltaTime, Filters);

    // 충돌 상태 캐시
    bLastGrounded = CollisionFlags.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN);
    bLastTouchingCeiling = CollisionFlags.isSet(PxControllerCollisionFlag::eCOLLISION_UP);
    bLastTouchingSide = CollisionFlags.isSet(PxControllerCollisionFlag::eCOLLISION_SIDES);

    return CollisionFlags;
}

void FControllerInstance::SetPosition(const FVector& NewPosition)
{
    if (!Controller)
    {
        return;
    }

    // 에디터(Z-up) → PhysX(Y-up) 좌표 변환: (X, Y, Z) → (Y, Z, -X)
    PxExtendedVec3 PxPos(NewPosition.Y, NewPosition.Z, -NewPosition.X);
    Controller->setPosition(PxPos);
}

FVector FControllerInstance::GetPosition() const
{
    if (!Controller)
    {
        return FVector::Zero();
    }

    PxExtendedVec3 PxPos = Controller->getPosition();
    // PhysX(Y-up) → 에디터(Z-up) 좌표 변환: (x, y, z) → (-z, x, y)
    return FVector(
        static_cast<float>(-PxPos.z),
        static_cast<float>(PxPos.x),
        static_cast<float>(PxPos.y)
    );
}

FVector FControllerInstance::GetFootPosition() const
{
    if (!Controller)
    {
        return FVector::Zero();
    }

    PxExtendedVec3 PxPos = Controller->getFootPosition();
    // PhysX(Y-up) → 에디터(Z-up) 좌표 변환: (x, y, z) → (-z, x, y)
    return FVector(
        static_cast<float>(-PxPos.z),
        static_cast<float>(PxPos.x),
        static_cast<float>(PxPos.y)
    );
}

PxControllerFilters FControllerInstance::GetFilters()
{
    PxControllerFilters Filters;
    Filters.mFilterData = &CachedFilterData;
    Filters.mFilterCallback = QueryFilter;
    Filters.mFilterFlags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;

    if (PhysScene)
    {
        Filters.mCCTFilterCallback = PhysScene->GetCCTControllerFilter();
    }

    return Filters;
}

void FControllerInstance::SetCollisionChannel(ECollisionChannel Channel, uint32 Mask)
{
    CollisionChannel = Channel;
    CollisionMask = Mask;

    CachedFilterData.word0 = ChannelToBit(Channel);
    CachedFilterData.word1 = Mask;

    if (QueryFilter)
    {
        QueryFilter->MyChannel = Channel;
        QueryFilter->MyCollisionMask = Mask;
    }
}
