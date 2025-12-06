#pragma once
#include "PhysXSupport.h"
#include "ECollisionChannel.h"
#include "PhysicsUserData.h"

// Forward declarations
class UCapsuleComponent;
class UCharacterMovementComponent;
class FPhysScene;
class FCCTQueryFilterCallback;

/**
 * FCCTSettings
 *
 * CCT 생성에 필요한 설정값들을 담는 구조체
 * CharacterMovementComponent와 CapsuleComponent의 설정을 CCT용으로 변환
 */
struct FCCTSettings
{
    /** 캡슐 반경 (미터) */
    float Radius = 0.5f;

    /** 캡슐 높이 - 원통 부분만 (미터) */
    float Height = 1.0f;

    /** 자동 계단 오르기 높이 (미터) */
    float StepOffset = 0.45f;

    /** 걸을 수 있는 경사면 한계 (cos 값, 0~1) */
    float SlopeLimit = 0.707f;  // cos(45도)

    /** 접촉 오프셋/스킨 두께 (미터) */
    float ContactOffset = 0.1f;

    /** 위 방향 벡터 */
    PxVec3 UpDirection = PxVec3(0, 0, 1);

    /** 걸을 수 없는 경사면 처리 모드 */
    PxControllerNonWalkableMode::Enum NonWalkableMode = PxControllerNonWalkableMode::ePREVENT_CLIMBING_AND_FORCE_SLIDING;

    /** 기본값으로 초기화 */
    void SetDefaults();

    /** 컴포넌트로부터 설정값 추출 */
    void SetFromComponents(UCapsuleComponent* Capsule, UCharacterMovementComponent* Movement);
};

/**
 * FControllerInstance
 *
 * PhysX Character Controller(CCT)의 래퍼 클래스
 * CapsuleComponent가 소유하며, CCT의 생성/해제/이동을 담당
 */
struct FControllerInstance : public FPhysicsUserData
{
    /** PhysX Controller 인스턴스 */
    PxController* Controller = nullptr;

    /** 소유 컴포넌트 */
    UCapsuleComponent* OwnerComponent = nullptr;

    /** 연결된 이동 컴포넌트 */
    UCharacterMovementComponent* MovementComponent = nullptr;

    /** 물리 씬 참조 */
    FPhysScene* PhysScene = nullptr;

    /** 충돌 채널 */
    ECollisionChannel CollisionChannel = ECollisionChannel::Pawn;

    /** 충돌 마스크 */
    uint32 CollisionMask = CollisionMasks::DefaultPawn;

    /** Scene Query 필터 콜백 (인스턴스별) */
    FCCTQueryFilterCallback* QueryFilter = nullptr;

    // ==================================================================================
    // 생명주기
    // ==================================================================================

    FControllerInstance();
    ~FControllerInstance();

    /**
     * CCT 초기화
     * @param InCapsule 소유 캡슐 컴포넌트
     * @param InMovement 연결된 이동 컴포넌트 (nullptr 가능)
     * @param InPhysScene 물리 씬
     */
    void InitController(UCapsuleComponent* InCapsule, UCharacterMovementComponent* InMovement, FPhysScene* InPhysScene);

    /** CCT 해제 */
    void TermController();

    /** 유효한 Controller인지 확인 */
    bool IsValid() const { return Controller != nullptr; }

    // ==================================================================================
    // 이동 인터페이스
    // ==================================================================================

    /**
     * CCT 이동
     * @param Displacement 이동 벡터 (월드 좌표, cm)
     * @param DeltaTime 프레임 시간
     * @return 충돌 플래그 (eCOLLISION_DOWN, eCOLLISION_UP, eCOLLISION_SIDES)
     */
    PxControllerCollisionFlags Move(const FVector& Displacement, float DeltaTime);

    /**
     * CCT 위치 직접 설정 (텔레포트)
     * @param NewPosition 새 위치 (월드 좌표, cm)
     */
    void SetPosition(const FVector& NewPosition);

    /**
     * CCT 중심 위치 반환
     * @return 캡슐 중심 위치 (월드 좌표, cm)
     */
    FVector GetPosition() const;

    /**
     * CCT 발 위치 반환 (캡슐 바닥)
     * @return 발 위치 (월드 좌표, cm)
     */
    FVector GetFootPosition() const;

    // ==================================================================================
    // 상태 조회
    // ==================================================================================

    /** 바닥에 닿아있는지 (마지막 move 결과) */
    bool IsGrounded() const { return bLastGrounded; }

    /** 천장에 닿았는지 (마지막 move 결과) */
    bool IsTouchingCeiling() const { return bLastTouchingCeiling; }

    /** 측면에 닿았는지 (마지막 move 결과) */
    bool IsTouchingSide() const { return bLastTouchingSide; }

    // ==================================================================================
    // 필터링
    // ==================================================================================

    /** move()에서 사용할 필터 설정 반환 */
    PxControllerFilters GetFilters();

    /** 충돌 채널/마스크 설정 */
    void SetCollisionChannel(ECollisionChannel Channel, uint32 Mask);

private:
    /** 마지막 move 결과 캐시 */
    bool bLastGrounded = false;
    bool bLastTouchingCeiling = false;
    bool bLastTouchingSide = false;

    /** 필터 데이터 (move 호출 시 사용) */
    PxFilterData CachedFilterData;
};
