#include "pch.h"
#include "CameraControlWidget.h"
#include "UIManager.h"
#include "ImGui/imgui.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "Vector.h"
#include "World.h"
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
#include <algorithm>
#include "EditorEngine.h"

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

// Camera Mode
static const char* CameraMode[] = {
	"Perspective",
	"Orthographic"
};

IMPLEMENT_CLASS(UCameraControlWidget)

UCameraControlWidget::UCameraControlWidget()
	: UWidget("Camera Control Widget")
	, UIManager(&UUIManager::GetInstance())
{
}

UCameraControlWidget::~UCameraControlWidget() = default;

void UCameraControlWidget::Initialize()
{
	// UIManager 참조 확보
	UIManager = &UUIManager::GetInstance();

	// UIManager에 자신 등록(씬 로드시 UI 재동기화 호출 받을 수 있게)
	if (UIManager)
	{
		UIManager->RegisterCameraControlWidget(this);
	}
}

void UCameraControlWidget::Update()
{
	// GizmoActor 참조 업데이트
	extern UEditorEngine GEngine;
	if (AGizmoActor* Gizmo = GEngine.GetDefaultWorld()->GetGizmoActor())
	{
		GizmoActor = Gizmo;
	}
	// 월드 정보 업데이트 (옵션)
	if (UIManager && UIManager->GetWorld())
	{
		UWorld* World = UIManager->GetWorld();
		WorldActorCount = static_cast<uint32>(World->GetActors().size());
	}
}

ACameraActor* UCameraControlWidget::GetCurrentCamera() const
{
	GEngine.GetDefaultWorld()->GetCameraActor();
	if (!GEngine.GetDefaultWorld())
		return nullptr;
		
	return GEngine.GetDefaultWorld()->GetCameraActor();
}

void UCameraControlWidget::RenderWidget()
{
	ACameraActor* Camera = GetCurrentCamera();
	
	if (!Camera)
	{
		ImGui::TextUnformatted("Camera not available.");
		ImGui::Separator();
		ImGui::TextUnformatted("No camera is currently set in UIManager.");
		return;
	}

	// 최초 1회 동기화
	if (!bSyncedOnce)
	{
		SyncFromCamera();
		bSyncedOnce = true;
	}

	ImGui::TextUnformatted("Camera Transform");
	ImGui::Spacing();

	// 카메라 이동속도 표시 및 조절 (World와 동기화)
	if (GEngine.GetDefaultWorld() && GEngine.GetDefaultWorld()->GetCameraActor())
	{
		// World에서 현재 카메라 이동 속도 가져오기
		float WorldMoveSpeed = GEngine.GetDefaultWorld()->GetCameraActor()->GetCameraSpeed();
		
		ImGui::Text("Move Speed: %.1f", WorldMoveSpeed);
		if (ImGui::SliderFloat("##MoveSpeed", &WorldMoveSpeed, 1.0f, 20.0f, "%.1f"))
		{
			// World에 카메라 이동속도 설정
			GEngine.GetDefaultWorld()->GetCameraActor()->SetCameraSpeed(WorldMoveSpeed);
			// 위젯의 로컬 값도 업데이트
			CameraMoveSpeed = WorldMoveSpeed;
		}
	}
	else
	{
		// World가 없는 경우 로컬 값만 표시
		ImGui::Text("Move Speed: %.1f (World not available)", CameraMoveSpeed);
		ImGui::SliderFloat("##MoveSpeed", &CameraMoveSpeed, 1.0f, 20.0f, "%.1f");
	}
	ImGui::Spacing();

	// 카메라 모드 선택
	if (ImGui::Combo("Mode", &CameraModeIndex, CameraMode, IM_ARRAYSIZE(CameraMode)))
	{
		PushToCamera();
	}

	// 카메라 위치 제어
	FVector Location = Camera->GetActorLocation();
	if (ImGui::DragFloat3("Camera Location", &Location.X, 0.05f))
	{
		Camera->SetActorLocation(Location);
	}

	// 카메라 회전 제어 (Euler angles)
	FVector Rotation = Camera->GetActorRotation().ToEulerZYXDeg();
	bool RotationChanged = false;
	RotationChanged |= ImGui::DragFloat3("Camera Rotation", &Rotation.X, 0.1f);

	// Pitch 제한 (-89 ~ 89도)
	Rotation.X = FMath::Clamp(Rotation.X, -89.0f, 89.0f);

	if (RotationChanged)
	{
		FQuat NewRotation = FQuat::MakeFromEulerZYX(Rotation);
		Camera->SetActorRotation(NewRotation);
	}

	ImGui::TextUnformatted("Camera Optics");
	ImGui::Spacing();

	// FOV 조절
	bool bChanged = false;
	bChanged |= ImGui::SliderFloat("FOV", &UiFovY, 1.0f, 170.0f, "%.1f");

	// Near/Far 조절
	bChanged |= ImGui::DragFloat("Z Near", &UiNearZ, 0.01f, 0.0001f, 1e6f, "%.4f");
	bChanged |= ImGui::DragFloat("Z Far", &UiFarZ, 0.1f, 0.001f, 1e7f, "%.3f");

	if (bChanged)
	{
		PushToCamera();
	}

	// 리셋 버튼
	if (ImGui::Button("Reset Optics"))
	{
		UiFovY = 80.0f;
		UiNearZ = 0.1f;
		UiFarZ = 1000.0f;
		PushToCamera();
	}

	ImGui::Spacing();
	ImGui::Separator();

	// 월드 정보 표시
	ImGui::Text("World Information");
	ImGui::Text("Actor Count: %u", WorldActorCount);
	ImGui::Separator();

	AGridActor* gridActor = UIManager->GetWorld()->GetGridActor();
	if (gridActor)
	{
		float currentLineSize = gridActor->GetLineSize();
		if (ImGui::DragFloat("Grid Spacing", &currentLineSize, 0.1f, 0.1f, 1000.0f))
		{
			gridActor->SetLineSize(currentLineSize);
			EditorINI["GridSpacing"] = std::to_string(currentLineSize);
		}
	}
	else
	{
		ImGui::Text("GridActor not found in the world.");
	}

	ImGui::Text("Transform Editor");


	// 기즈모 스페이스 모드 선택
	if (GizmoActor)
	{
		const char* spaceItems[] = { "World", "Local" };
		int currentSpaceIndex = static_cast<int>(CurrentGizmoSpace);

		if (ImGui::Combo("Gizmo Space", &currentSpaceIndex, spaceItems, IM_ARRAYSIZE(spaceItems)))
		{
			CurrentGizmoSpace = static_cast<EGizmoSpace>(currentSpaceIndex);
		}
		ImGui::Separator();
	}

}

void UCameraControlWidget::SyncFromCamera()
{
	ACameraActor* Camera = GetCurrentCamera();
	if (!Camera) 
		return;

	// 카메라 컴포넌트에서 실제 설정 값 가져오기
	if (UCameraComponent* CameraComp = Camera->GetCameraComponent())
	{
		// 실제 카메라 컴포넌트에서 값 읽어오기
		UiFovY = CameraComp->GetFOV();
		UiNearZ = CameraComp->GetNearClip();
		UiFarZ = CameraComp->GetFarClip();
		
		// 프로젝션 모드 설정
		CameraModeIndex = (CameraComp->GetProjectionMode() == ECameraProjectionMode::Perspective) ? 0 : 1;
		
		UE_LOG("CameraControl: Synced from camera - FOV=%.1f, Near=%.4f, Far=%.1f, Mode=%d", 
			UiFovY, UiNearZ, UiFarZ, CameraModeIndex);
	}
	
	// World에서 카메라 이동 속도 동기화
	if (GEngine.GetDefaultWorld() && GEngine.GetDefaultWorld()->GetCameraActor())
	{
		CameraMoveSpeed = GEngine.GetDefaultWorld()->GetCameraActor()->GetCameraSpeed();
		UE_LOG("CameraControl: Synced camera move speed from world - Speed=%.1f", CameraMoveSpeed);
	}
}

void UCameraControlWidget::PushToCamera()
{
	ACameraActor* Camera = GetCurrentCamera();
	if (!Camera) 
		return;

	// Near/Far 값 검증
	UiNearZ = FMath::Max(0.0001f, UiNearZ);
	UiFarZ = FMath::Max(UiNearZ + 0.0001f, UiFarZ);
	UiFovY = FMath::Clamp(UiFovY, 1.0f, 170.0f);

	// 카메라 컴포넌트에 실제 설정 적용
	if (UCameraComponent* CameraComp = Camera->GetCameraComponent())
	{
		// 카메라 설정 업데이트
		CameraComp->SetFOV(UiFovY);
		CameraComp->SetNearClipPlane(UiNearZ);
		CameraComp->SetFarClipPlane(UiFarZ);
		
		// 프로젝션 모드 설정
		ECameraProjectionMode NewMode = (CameraModeIndex == 0) ? ECameraProjectionMode::Perspective : ECameraProjectionMode::Orthographic;
		CameraComp->SetProjectionMode(NewMode);
		
		UE_LOG("CameraControl: Applied to camera - FOV=%.1f, Near=%.4f, Far=%.1f, Mode=%s", 
			UiFovY, UiNearZ, UiFarZ, (CameraModeIndex == 0) ? "Perspective" : "Orthographic");
	}
}
