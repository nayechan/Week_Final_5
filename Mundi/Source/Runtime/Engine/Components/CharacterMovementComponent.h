// ────────────────────────────────────────────────────────────────────────────
// CharacterMovementComponent.h
// Character의 이동을 처리하는 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "MovementComponent.h"
#include "UCharacterMovementComponent.generated.h"

// 전방 선언
class ACharacter;

/**
 * EMovementMode
 *
 * Character의 이동 모드를 나타냅니다.
 */
enum class EMovementMode : uint8
{
	/** 지면에서 걷기 */
	Walking,

	/** 공중에서 낙하 */
	Falling,

	/** 비행 (중력 무시) */
	Flying,

	/** 이동 불가 */
	None
};

/**
 * UCharacterMovementComponent
 *
 * Character의 이동, 중력, 점프 등을 처리하는 컴포넌트입니다.
 * 기본 물리 시뮬레이션을 수행합니다.
 *
 * 주요 기능:
 * - 중력 적용
 * - 속도/가속도 기반 이동
 * - 점프
 * - 이동 모드 관리 (Walking, Falling, Flying)
 */
UCLASS(DisplayName="CharacterMovementComponent", Description="Character의 이동을 처리하는 컴포넌트입니다.")
class UCharacterMovementComponent : public UMovementComponent
{
public:
	GENERATED_REFLECTION_BODY()

	UCharacterMovementComponent();
	virtual ~UCharacterMovementComponent() override;

	// ────────────────────────────────────────────────
	// 이동 설정
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="Movement", Tooltip="최대 걷기 속도 (cm/s)")
	float MaxWalkSpeed;

	UPROPERTY(EditAnywhere, Category="Movement", Tooltip="최대 가속도 (cm/s²)")
	float MaxAcceleration;

	UPROPERTY(EditAnywhere, Category="Movement", Tooltip="마찰력 (감속)")
	float GroundFriction;

	UPROPERTY(EditAnywhere, Category="Movement", Tooltip="공중 제어력 (0.0 ~ 1.0)")
	float AirControl;

	UPROPERTY(EditAnywhere, Category="Movement", Tooltip="제동력 (급정지 시)")
	float BrakingDeceleration;

	// ────────────────────────────────────────────────
	// 중력 설정
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="Gravity", Tooltip="중력 스케일 (1.0 = 기본 중력)")
	float GravityScale;

	/** 기본 중력 (-980 cm/s² ≈ -9.8 m/s²) */
	static constexpr float DefaultGravity = 980.0f;

	/** 중력 방향 벡터 (정규화된 방향, 기본값: 아래) */
	FVector GravityDirection;

	/**
	 * 중력 방향을 설정합니다.
	 * @param NewDirection - 새로운 중력 방향 (자동으로 정규화됨)
	 */
	void SetGravityDirection(const FVector& NewDirection);

	/**
	 * 현재 중력 방향을 반환합니다.
	 */
	FVector GetGravityDirection() const { return GravityDirection; }

	// ────────────────────────────────────────────────
	// 점프 설정
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="Jump", Tooltip="점프 초기 속도 (cm/s)")
	float JumpZVelocity;

	UPROPERTY(EditAnywhere, Category="Jump", Tooltip="공중에 있을 수 있는 최대 시간 (초)")
	float MaxAirTime;

	/** 점프 가능 여부 */
	bool bCanJump;

	// ────────────────────────────────────────────────
	// 이동 함수
	// ────────────────────────────────────────────────

	/**
	 * 입력 벡터를 추가합니다.
	 *
	 * @param WorldDirection - 월드 스페이스 이동 방향
	 * @param ScaleValue - 입력 스케일
	 */
	void AddInputVector(FVector WorldDirection, float ScaleValue = 1.0f);

	/**
	 * 누적된 입력 벡터를 초기화합니다.
	 */
	void ConsumeInputVector() { PendingInputVector = FVector::Zero(); }

	/**
	 * 점프를 시도합니다.
	 *
	 * @return 점프에 성공하면 true
	 */
	bool Jump();

	/**
	 * 점프를 중단합니다 (점프 키를 뗐을 때).
	 */
	void StopJumping();

	// ────────────────────────────────────────────────
	// 이동 모드
	// ────────────────────────────────────────────────

	/**
	 * 현재 이동 모드를 반환합니다.
	 */
	EMovementMode GetMovementMode() const { return MovementMode; }

	/**
	 * 이동 모드를 설정합니다.
	 *
	 * @param NewMode - 새로운 이동 모드
	 */
	void SetMovementMode(EMovementMode NewMode);

	/**
	 * 지면에 있는지 확인합니다.
	 */
	bool IsGrounded() const { return MovementMode == EMovementMode::Walking; }

	/**
	 * 낙하 중인지 확인합니다.
	 */
	bool IsFalling() const { return MovementMode == EMovementMode::Falling; }

protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;

	// ────────────────────────────────────────────────
	// 내부 이동 로직
	// ────────────────────────────────────────────────

	/**
	 * 이동 입력을 처리하고 속도를 계산합니다.
	 *
	 * @param DeltaTime - 프레임 시간
	 */
	void UpdateVelocity(float DeltaTime);

	/**
	 * 중력을 적용합니다.
	 *
	 * @param DeltaTime - 프레임 시간
	 */
	void ApplyGravity(float DeltaTime);

	/**
	 * 계산된 속도로 위치를 업데이트합니다.
	 *
	 * @param DeltaTime - 프레임 시간
	 */
	void MoveUpdatedComponent(float DeltaTime);

	/**
	 * 지면 체크 (간단한 Z축 위치 기반)
	 *
	 * @return 지면에 있으면 true
	 */
	bool CheckGround();

	// ────────────────────────────────────────────────
	// 복제
	// ────────────────────────────────────────────────

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** Owner Character */
	ACharacter* CharacterOwner;

	/** 이번 프레임 입력 */
	FVector PendingInputVector;

	/** 현재 이동 모드 */
	EMovementMode MovementMode;

	/** 공중에 있었던 시간 (점프/낙하) */
	float TimeInAir;

	/** 점프 중인지 여부 */
	bool bIsJumping;
};
