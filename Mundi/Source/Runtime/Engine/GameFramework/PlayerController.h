// ────────────────────────────────────────────────────────────────────────────
// PlayerController.h
// 플레이어 입력을 처리하는 Controller 클래스
// ────────────────────────────────────────────────────────────────────────────
#pragma once

#include "Controller.h"
#include "APlayerController.generated.h"

// 전방 선언
class UInputComponent;
class APlayerCameraManager;
class UCameraComponent;

// EInputMode는 InputManager.h에 정의됨
#include "InputManager.h"

/**
 * APlayerController
 *
 * 플레이어 입력을 처리하는 Controller입니다.
 * UInputManager와 통합되어 키보드/마우스 입력을 Pawn에 전달합니다.
 *
 * 주요 기능:
 * - InputManager와 통합
 * - InputComponent로 키 바인딩 관리
 * - 마우스 커서 제어
 * - 입력 우선순위 관리
 */
UCLASS(DisplayName="PlayerController", Description="플레이어 입력을 처리하는 Controller입니다.")
class APlayerController : public AController
{
public:
	GENERATED_REFLECTION_BODY()

	APlayerController();
	virtual ~APlayerController() override;

	// ────────────────────────────────────────────────
	// Pawn 빙의 관련 (오버라이드)
	// ────────────────────────────────────────────────

	void Possess(APawn* InPawn) override;
	void UnPossess() override;

protected:
	void OnPossess(APawn* InPawn) override;
	void OnUnPossess() override;

public:
	// ────────────────────────────────────────────────
	// 입력 처리
	// ────────────────────────────────────────────────

	/**
	 * 매 프레임 호출되어 입력을 처리합니다.
	 * Pawn의 InputComponent에 입력 이벤트를 전달합니다.
	 */
	void ProcessPlayerInput();

	/**
	 * 마우스 입력을 처리하여 Controller 회전에 반영합니다.
	 */
	void ProcessMouseInput();

	/**
	 * 입력이 활성화되어 있는지 확인합니다.
	 */
	bool IsInputEnabled() const { return bInputEnabled; }

	/**
	 * 입력을 활성화/비활성화합니다.
	 *
	 * @param bEnabled - true면 입력 활성화, false면 비활성화
	 */
	void SetInputEnabled(bool bEnabled) { bInputEnabled = bEnabled; }

	// ────────────────────────────────────────────────
	// 마우스 커서 제어
	// ────────────────────────────────────────────────

	/**
	 * 마우스 커서를 표시합니다.
	 */
	void ShowMouseCursor();

	/**
	 * 마우스 커서를 숨깁니다.
	 */
	void HideMouseCursor();

	/**
	 * 마우스 커서가 표시되는지 확인합니다.
	 */
	bool IsMouseCursorVisible() const { return bShowMouseCursor; }

	/**
	 * 마우스 커서를 잠급니다 (FPS 게임용).
	 * 잠기면 커서가 중앙에 고정되고 Delta만 사용됩니다.
	 */
	void LockMouseCursor();

	/**
	 * 마우스 커서 잠금을 해제합니다.
	 */
	void UnlockMouseCursor();

	/**
	 * 마우스 커서가 잠겨있는지 확인합니다.
	 */
	bool IsMouseCursorLocked() const { return bIsMouseLocked; }

	// ────────────────────────────────────────────────
	// 마우스 감도 설정
	// ────────────────────────────────────────────────

	/**
	 * 마우스 감도를 설정합니다.
	 *
	 * @param Sensitivity - 마우스 감도 (기본값 1.0)
	 */
	void SetMouseSensitivity(float Sensitivity) { MouseSensitivity = Sensitivity; }

	/**
	 * 마우스 감도를 반환합니다.
	 */
	float GetMouseSensitivity() const { return MouseSensitivity; }

	/**
	 * 카메라 회전에 마우스 버튼이 필요한지 설정합니다.
	 * false로 설정하면 마우스 버튼 없이도 카메라가 회전합니다.
	 *
	 * @param bRequired - true면 마우스 버튼 필요, false면 항상 회전
	 */
	void SetRequireMouseButtonForRotation(bool bRequired) { bRequireMouseButtonForRotation = bRequired; }

	/**
	 * 카메라 회전에 마우스 버튼이 필요한지 반환합니다.
	 */
	bool IsMouseButtonRequiredForRotation() const { return bRequireMouseButtonForRotation; }

	// ────────────────────────────────────────────────
	// Input Mode
	// ────────────────────────────────────────────────

	/**
	 * 입력 모드를 설정합니다.
	 * - GameOnly: 커서 숨김, 마우스 잠금, 게임 입력만
	 * - UIOnly: 커서 표시, 마우스 잠금 해제, UI 입력만
	 * - GameAndUI: 커서 표시, 마우스 잠금 해제, 둘 다
	 */
	void SetInputMode(EInputMode NewMode);

	/**
	 * 현재 입력 모드를 반환합니다.
	 */
	EInputMode GetInputMode() const { return CurrentInputMode; }

	/**
	 * 게임 입력을 받을 수 있는 모드인지 확인합니다.
	 */
	bool CanReceiveGameInput() const { return CurrentInputMode == EInputMode::GameOnly || CurrentInputMode == EInputMode::GameAndUI; }

	/**
	 * UI 입력을 받을 수 있는 모드인지 확인합니다.
	 */
	bool CanReceiveUIInput() const { return CurrentInputMode == EInputMode::UIOnly || CurrentInputMode == EInputMode::GameAndUI; }

	// ────────────────────────────────────────────────
	// 카메라 관리
	// ────────────────────────────────────────────────

	/**
	 * PlayerCameraManager를 반환합니다.
	 */
	APlayerCameraManager* GetPlayerCameraManager() const { return PlayerCameraManager; }

	/**
	 * 렌더링용 카메라 컴포넌트를 반환합니다.
	 * PIE 모드에서 PlayerCameraManager의 카메라를 사용합니다.
	 */
	UCameraComponent* GetCameraComponentForRendering() const;

protected:
	// ────────────────────────────────────────────────
	// 생명주기
	// ────────────────────────────────────────────────

	void BeginPlay() override;
	void Tick(float DeltaSeconds) override;

	// ────────────────────────────────────────────────
	// 복제
	// ────────────────────────────────────────────────

	void DuplicateSubObjects() override;
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────

	/** InputManager 참조 */
	UInputManager* InputManager;

	/** 입력 활성화 여부 */
	bool bInputEnabled;

	/** 마우스 커서 표시 여부 */
	bool bShowMouseCursor;

	/** 마우스 커서 잠금 여부 */
	bool bIsMouseLocked;

	/** 마우스 감도 */
	float MouseSensitivity;

	/** 카메라 회전에 마우스 버튼이 필요한지 여부 (false면 항상 회전) */
	bool bRequireMouseButtonForRotation;

	/** 현재 입력 모드 */
	EInputMode CurrentInputMode;

	/** 카메라 매니저 (PIE 모드에서만 사용) */
	APlayerCameraManager* PlayerCameraManager;
};
