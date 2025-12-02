// ────────────────────────────────────────────────────────────────────────────
// CharacterMovementComponent.h
// Character의 이동을 처리하는 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "MovementComponent.h"
#include "UCharacterMovementComponent.generated.h"

// 전방 선언
class ACharacter;
class FPhysScene;
struct FHitResult;

/**
 * FFindFloorResult
 *
 * 바닥 탐색 결과를 저장하는 구조체입니다.
 */
struct FFindFloorResult
{
	/** 걸을 수 있는 바닥인지 (경사각 체크) */
	bool bWalkableFloor = false;

	/** 바닥이 감지되었는지 */
	bool bBlockingHit = false;

	/** 바닥까지의 거리 */
	float FloorDist = 0.0f;

	/** 바닥 표면의 Z 위치 */
	float FloorZ = 0.0f;

	/** 바닥 표면의 법선 벡터 */
	FVector FloorNormal = FVector(0.0f, 0.0f, 1.0f);

	/** 충돌 지점 */
	FVector HitLocation = FVector::Zero();

	/** 충돌한 액터 */
	AActor* HitActor = nullptr;

	/** 충돌한 컴포넌트 */
	UPrimitiveComponent* HitComponent = nullptr;

	/** 결과 초기화 */
	void Clear()
	{
		bWalkableFloor = false;
		bBlockingHit = false;
		FloorDist = 0.0f;
		FloorZ = 0.0f;
		FloorNormal = FVector(0.0f, 0.0f, 1.0f);
		HitLocation = FVector::Zero();
		HitActor = nullptr;
		HitComponent = nullptr;
	}

	/** 유효한 바닥인지 */
	bool IsWalkableFloor() const { return bBlockingHit && bWalkableFloor; }
};

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
	static constexpr float DefaultGravity = 9.8f;

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
	// 바닥 감지 설정
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="Floor", Tooltip="걸을 수 있는 최대 경사각 (도)")
	float WalkableFloorAngle;

	UPROPERTY(EditAnywhere, Category="Floor", Tooltip="바닥 스냅 허용 거리 (cm)")
	float FloorSnapDistance;

	UPROPERTY(EditAnywhere, Category="Floor", Tooltip="자동으로 오를 수 있는 최대 계단 높이 (cm)")
	float MaxStepHeight;

	/** 현재 바닥 정보 */
	FFindFloorResult CurrentFloor;

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
	 * 충돌 감지와 슬라이딩을 포함한 안전한 이동
	 *
	 * @param Delta - 이동할 벡터
	 * @param OutHit - 충돌 결과 (출력)
	 * @return 실제로 이동했으면 true
	 */
	bool SafeMoveUpdatedComponent(const FVector& Delta, FHitResult& OutHit);

	/**
	 * 충돌 시 슬라이드 벡터를 계산합니다.
	 *
	 * @param Delta - 원래 이동 벡터
	 * @param Normal - 충돌면 법선
	 * @param Hit - 충돌 정보
	 * @return 슬라이드 벡터
	 */
	FVector ComputeSlideVector(const FVector& Delta, const FVector& Normal, const FHitResult& Hit) const;

	/**
	 * 슬라이딩을 포함한 이동을 수행합니다.
	 *
	 * @param Delta - 이동할 벡터
	 * @param MaxIterations - 최대 슬라이딩 반복 횟수
	 */
	void SlideAlongSurface(const FVector& Delta, int32 MaxIterations = 4);

	/**
	 * 계단/장애물을 올라가는 시도를 합니다.
	 *
	 * @param Delta - 원래 이동 벡터
	 * @param Hit - 충돌 정보
	 * @return 계단 오르기에 성공하면 true
	 */
	bool TryStepUp(const FVector& Delta, const FHitResult& Hit);

	/**
	 * 지면 체크 (간단한 Z축 위치 기반)
	 *
	 * @return 지면에 있으면 true
	 */
	bool CheckGround();

	/**
	 * 캡슐 스윕을 사용하여 바닥을 탐색합니다.
	 *
	 * @param OutFloorResult - 바닥 탐색 결과 (출력)
	 * @param SweepDistance - 스윕 거리 (기본값: FloorSnapDistance)
	 * @return 바닥이 감지되면 true
	 */
	bool FindFloor(FFindFloorResult& OutFloorResult, float SweepDistance = -1.0f);

	/**
	 * 법선 벡터가 걸을 수 있는 표면인지 확인합니다.
	 *
	 * @param Normal - 표면 법선 벡터
	 * @return 걸을 수 있으면 true
	 */
	bool IsWalkable(const FVector& Normal) const;

	/**
	 * 바닥에 스냅합니다 (Walking 상태일 때).
	 */
	void SnapToFloor();

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
