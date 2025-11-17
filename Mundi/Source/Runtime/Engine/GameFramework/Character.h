// ────────────────────────────────────────────────────────────────────────────
// Character.h
// 이동 가능한 Pawn 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Pawn.h"
#include "ACharacter.generated.h"

// 전방 선언
class UCharacterMovementComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;

/**
 * ACharacter
 *
 * 이동, 점프 등의 기능을 가진 Pawn입니다.
 * CharacterMovementComponent를 통해 물리 기반 이동을 처리합니다.
 *
 * 주요 기능:
 * - CharacterMovementComponent 포함
 * - Jump(), Crouch() 등 기본 동작
 * - 이동 입력 처리
 */
UCLASS(DisplayName="캐릭터", Description="이동, 점프 등의 기능을 가진 Character 클래스입니다.")
class ACharacter : public APawn
{
public:
	GENERATED_REFLECTION_BODY()

	ACharacter();

protected:
	~ACharacter() override;

public:
	// ────────────────────────────────────────────────
	// 컴포넌트 접근
	// ────────────────────────────────────────────────

	UFUNCTION(LuaBind, DisplayName="GetCharacterMovement")
	UCharacterMovementComponent* GetCharacterMovement() const { return CharacterMovement; }

	UFUNCTION(LuaBind, DisplayName="GetCapsuleComponent")
	UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComponent; }

	UFUNCTION(LuaBind, DisplayName="GetMesh")
	USkeletalMeshComponent* GetMesh() const { return MeshComponent; }

	// ────────────────────────────────────────────────
	// 이동 입력 처리 (APawn 오버라이드)
	// ────────────────────────────────────────────────

	virtual void AddMovementInput(FVector WorldDirection, float ScaleValue = 1.0f) override;

	// ────────────────────────────────────────────────
	// Character 동작
	// ────────────────────────────────────────────────

	/**
	 * 점프를 시도합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="Jump")
	virtual void Jump();

	/**
	 * 점프를 중단합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="StopJumping")
	virtual void StopJumping();

	/**
	 * 점프 가능 여부를 확인합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="CanJump")
	virtual bool CanJump() const;

	/**
	 * 웅크리기를 시작합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="Crouch")
	virtual void Crouch();

	/**
	 * 웅크리기를 해제합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="UnCrouch")
	virtual void UnCrouch();

	/**
	 * 웅크리고 있는지 확인합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="IsCrouched")
	bool IsCrouched() const { return bIsCrouched; }

	// ────────────────────────────────────────────────
	// 상태 조회
	// ────────────────────────────────────────────────

	/**
	 * 현재 속도를 반환합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="GetVelocity")
	virtual FVector GetVelocity() const;

	/**
	 * 지면에 있는지 확인합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="IsGrounded")
	bool IsGrounded() const;

	/**
	 * 낙하 중인지 확인합니다.
	 */
	UFUNCTION(LuaBind, DisplayName="IsFalling")
	bool IsFalling() const;

	// ────────────────────────────────────────────────
	// 이동 입력 함수 (Lua에서 호출 가능)
	// ────────────────────────────────────────────────

	/** 앞/뒤 이동 */
	UFUNCTION(LuaBind, DisplayName="MoveForward")
	void MoveForward(float Value);

	/** 좌/우 이동 */
	UFUNCTION(LuaBind, DisplayName="MoveRight")
	void MoveRight(float Value);

	/** 좌/우 회전 */
	UFUNCTION(LuaBind, DisplayName="Turn")
	void Turn(float Value);

	/** 위/아래 회전 */
	UFUNCTION(LuaBind, DisplayName="LookUp")
	void LookUp(float Value);

protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// 입력 바인딩 (오버라이드)
	// ────────────────────────────────────────────────

	virtual void SetupPlayerInputComponent(UInputComponent* InInputComponent) override;

	// ────────────────────────────────────────────────
	// 복제
	// ────────────────────────────────────────────────

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** Character 이동 컴포넌트 */
	UCharacterMovementComponent* CharacterMovement;

	/** 캡슐 컴포넌트 (충돌 및 물리) */
	UCapsuleComponent* CapsuleComponent;

	/** 스켈레탈 메시 컴포넌트 (애니메이션) */
	USkeletalMeshComponent* MeshComponent;

	/** 웅크리기 상태 */
	bool bIsCrouched;

	/** 웅크릴 때 높이 비율 */
	float CrouchedHeightRatio;
};
