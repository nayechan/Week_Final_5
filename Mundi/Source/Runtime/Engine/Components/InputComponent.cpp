// ────────────────────────────────────────────────────────────────────────────
// InputComponent.cpp
// 입력 바인딩 컴포넌트 구현
// ────────────────────────────────────────────────────────────────────────────
#include "pch.h"
#include "InputComponent.h"
#include "InputManager.h"

UInputComponent::UInputComponent()
{
}

UInputComponent::~UInputComponent()
{
}

// ────────────────────────────────────────────────────────────────────────────
// 액션 바인딩
// ────────────────────────────────────────────────────────────────────────────

void UInputComponent::BindAction(const FString& ActionName, int32 KeyCode,
	std::function<void()> PressedCallback,
	std::function<void()> ReleasedCallback)
{
	FInputActionBinding Binding(ActionName, KeyCode);
	Binding.PressedCallback = PressedCallback;
	Binding.ReleasedCallback = ReleasedCallback;
	ActionBindings.Add(Binding);
}

// ────────────────────────────────────────────────────────────────────────────
// 축 바인딩
// ────────────────────────────────────────────────────────────────────────────

void UInputComponent::BindAxis(const FString& AxisName, int32 KeyCode, float Scale, std::function<void(float)> Callback)
{
	FInputAxisBinding Binding(AxisName, KeyCode, Scale);
	Binding.Callback = Callback;
	AxisBindings.Add(Binding);
}

// ────────────────────────────────────────────────────────────────────────────
// 입력 처리
// ────────────────────────────────────────────────────────────────────────────

void UInputComponent::ProcessInput()
{
	UInputManager& InputManager = UInputManager::GetInstance();

	// 액션 바인딩 처리
	for (const FInputActionBinding& Binding : ActionBindings)
	{
		// 눌림 이벤트
		if (InputManager.IsKeyPressed(Binding.KeyCode) && Binding.PressedCallback)
		{
			Binding.PressedCallback();
		}

		// 떼어짐 이벤트
		if (InputManager.IsKeyReleased(Binding.KeyCode) && Binding.ReleasedCallback)
		{
			Binding.ReleasedCallback();
		}
	}

	// 축 바인딩 처리
	for (const FInputAxisBinding& Binding : AxisBindings)
	{
		if (InputManager.IsKeyDown(Binding.KeyCode) && Binding.Callback)
		{
			Binding.Callback(Binding.Scale);
		}
	}
}

void UInputComponent::ClearBindings()
{
	ActionBindings.clear();
	AxisBindings.clear();
}

// ────────────────────────────────────────────────────────────────────────────
// 복제
// ────────────────────────────────────────────────────────────────────────────

void UInputComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void UInputComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 입력 바인딩은 런타임에 설정되므로 직렬화하지 않음
}
