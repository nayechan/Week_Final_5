#pragma once
#include "Widget.h"

class UUIManager;
class ACameraActor;

class UCameraControlWidget
	: public UWidget
{
public:
	DECLARE_CLASS(UCameraControlWidget, UWidget)

	void Initialize() override;
	void Update() override;
	void RenderWidget() override;
	void SyncFromCamera();
	void PushToCamera();

	// Special Member Function
	UCameraControlWidget();
	~UCameraControlWidget() override;

private:
	UUIManager* UIManager = nullptr;
	ACameraActor* GetCurrentCamera() const;
	
	// UI 상태 변수들
	float UiFovY = 60.f;  // CameraComponent 기본값과 맞춤
	float UiNearZ = 0.1f;
	float UiFarZ = 1000.f;
	int   CameraModeIndex = 0;
	
	// 카메라 제어 상태
	float CameraMoveSpeed = 5.0f;
	bool bSyncedOnce = false;

	AActor* SelectedActor = nullptr;
	// 기즈모 설정
	EGizmoSpace CurrentGizmoSpace = EGizmoSpace::World;
	AGizmoActor* GizmoActor = nullptr;
	AActor* GetCurrentSelectedActor() const;
	// 월드 정보 (옵션)
	uint32 WorldActorCount = 0;
};
