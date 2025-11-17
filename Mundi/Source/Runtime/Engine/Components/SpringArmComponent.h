// ────────────────────────────────────────────────────────────────────────────
// SpringArmComponent.h
// 3인칭 카메라용 스프링 암 컴포넌트
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "SceneComponent.h"
#include "USpringArmComponent.generated.h"

/**
 * USpringArmComponent
 *
 * 3인칭 카메라를 위한 스프링 암 컴포넌트입니다.
 *
 * 주요 기능:
 * - 카메라를 캐릭터 뒤에 일정 거리만큼 떨어뜨려 배치
 * - Camera Lag (부드러운 따라가기)
 * - Rotation Lag (부드러운 회전)
 * - 충돌 감지 (장애물 회피)
 */
UCLASS(DisplayName="스프링암 컴포넌트", Description="카메라 추적을 위한 스프링암 컴포넌트입니다. 카메라 Lag, 충돌 감지 등을 지원합니다.")
class USpringArmComponent : public USceneComponent
{
public:
	GENERATED_REFLECTION_BODY()

	USpringArmComponent();

protected:
	~USpringArmComponent() override;

public:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void TickComponent(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// Spring Arm 설정
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="SpringArm", Range="0.0, 10000.0", Tooltip="스프링암의 목표 길이입니다.")
	float TargetArmLength;

	UPROPERTY(EditAnywhere, Category="SpringArm", Tooltip="카메라 소켓의 오프셋입니다.")
	FVector SocketOffset;

	UPROPERTY(EditAnywhere, Category="SpringArm", Tooltip="타겟 위치의 오프셋입니다.")
	FVector TargetOffset;

	void SetTargetArmLength(float NewLength) { TargetArmLength = NewLength; }
	float GetTargetArmLength() const { return TargetArmLength; }

	void SetSocketOffset(const FVector& Offset) { SocketOffset = Offset; }
	FVector GetSocketOffset() const { return SocketOffset; }

	void SetTargetOffset(const FVector& Offset) { TargetOffset = Offset; }
	FVector GetTargetOffset() const { return TargetOffset; }

	// ────────────────────────────────────────────────
	// Camera Lag (부드러운 따라가기)
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="CameraLag", Tooltip="카메라 위치에 부드러운 따라가기(Lag)를 적용할지 여부입니다.")
	bool bEnableCameraLag;

	UPROPERTY(EditAnywhere, Category="CameraLag", Range="0.0, 100.0", Tooltip="카메라 위치 Lag의 보간 속도입니다.")
	float CameraLagSpeed;

	UPROPERTY(EditAnywhere, Category="CameraLag", Range="0.0, 10000.0", Tooltip="카메라 Lag의 최대 거리 제한입니다. 0이면 제한 없음.")
	float CameraLagMaxDistance;

	void SetEnableCameraLag(bool bEnable) { bEnableCameraLag = bEnable; }
	bool GetEnableCameraLag() const { return bEnableCameraLag; }

	void SetCameraLagSpeed(float Speed) { CameraLagSpeed = Speed; }
	float GetCameraLagSpeed() const { return CameraLagSpeed; }

	void SetCameraLagMaxDistance(float MaxDistance) { CameraLagMaxDistance = MaxDistance; }
	float GetCameraLagMaxDistance() const { return CameraLagMaxDistance; }

	// ────────────────────────────────────────────────
	// Camera Rotation Lag
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="CameraLag", Tooltip="카메라 회전에 부드러운 따라가기(Lag)를 적용할지 여부입니다.")
	bool bEnableCameraRotationLag;

	UPROPERTY(EditAnywhere, Category="CameraLag", Range="0.0, 100.0", Tooltip="카메라 회전 Lag의 보간 속도입니다.")
	float CameraRotationLagSpeed;

	void SetEnableCameraRotationLag(bool bEnable) { bEnableCameraRotationLag = bEnable; }
	bool GetEnableCameraRotationLag() const { return bEnableCameraRotationLag; }

	void SetCameraRotationLagSpeed(float Speed) { CameraRotationLagSpeed = Speed; }
	float GetCameraRotationLagSpeed() const { return CameraRotationLagSpeed; }

	// ────────────────────────────────────────────────
	// Collision Test
	// ────────────────────────────────────────────────

	UPROPERTY(EditAnywhere, Category="Collision", Tooltip="스프링암이 장애물과의 충돌을 테스트할지 여부입니다.")
	bool bDoCollisionTest;

	UPROPERTY(EditAnywhere, Category="Collision", Range="0.0, 100.0", Tooltip="충돌 테스트에 사용되는 프로브의 크기입니다.")
	float ProbeSize;

	void SetDoCollisionTest(bool bEnable) { bDoCollisionTest = bEnable; }
	bool GetDoCollisionTest() const { return bDoCollisionTest; }

	void SetProbeSize(float Size) { ProbeSize = Size; }
	float GetProbeSize() const { return ProbeSize; }

	// ────────────────────────────────────────────────
	// Socket Transform 조회
	// ────────────────────────────────────────────────

	FVector GetSocketLocation() const;
	FQuat GetSocketRotation() const;
	FTransform GetSocketTransform() const;

	// ────────────────────────────────────────────────
	// 복제 및 직렬화
	// ────────────────────────────────────────────────

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	// ────────────────────────────────────────────────
	// 내부 상태
	// ────────────────────────────────────────────────

	// 현재 스프링암 길이 (충돌 감지로 줄어들 수 있음)
	float CurrentArmLength;

	// Camera Lag 이전 위치/회전
	FVector PreviousDesiredLocation;
	FVector PreviousActorLocation;
	FQuat PreviousDesiredRotation;

	// Socket Transform (자식 컴포넌트가 배치될 위치)
	FVector SocketLocation;
	FQuat SocketRotation;

	// ────────────────────────────────────────────────
	// 내부 함수
	// ────────────────────────────────────────────────

	void UpdateDesiredArmLocation(float DeltaTime, FVector& OutDesiredLocation, FQuat& OutDesiredRotation);
	bool DoCollisionTest(const FVector& DesiredLocation, FVector& OutLocation);
};
