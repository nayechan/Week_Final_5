#include "pch.h"
#include "FViewportClient.h"
#include "FViewport.h"
#include "CameraComponent.h"
#include "CameraActor.h"
#include "World.h"
#include "Picking.h"
#include "SelectionManager.h"
#include "Gizmo/GizmoActor.h"
#include "RenderManager.h"
#include "RenderSettings.h"
#include "EditorEngine.h"
#include "PrimitiveComponent.h"
#include "Clipboard/ClipboardManager.h"
#include "InputManager.h"
#include "PlayerCameraManager.h"
#include "SceneView.h"
#include "SlateManager.h"
#include "D3D11RHI.h"

// 언리얼 방식: 모든 직교 뷰포트가 하나의 3D 위치를 공유
FVector FViewportClient::CameraAddPosition{};

FViewportClient::FViewportClient()
{
	ViewportType = EViewportType::Perspective;
	// 직교 뷰별 기본 카메라 설정
	Camera = NewObject<ACameraActor>();
	SetupCameraMode();
}

FViewportClient::~FViewportClient()
{
}

void FViewportClient::Tick(float DeltaTime)
{
	if (PerspectiveCameraInput)
	{
		Camera->ProcessEditorCameraInput(DeltaTime);
	}
	MouseWheel(DeltaTime);
	static UClipboardManager* ClipboardManager = NewObject<UClipboardManager>();

	// 키보드 입력 처리 (Ctrl+C/V)
	if (World)
	{
		UInputManager& InputManager = UInputManager::GetInstance();
		USelectionManager* SelectionManager = World->GetSelectionManager();

		// Ctrl 키가 눌려있는지 확인
		bool bIsCtrlDown = InputManager.IsKeyDown(VK_CONTROL);

		// Ctrl + C: 복사
		if (bIsCtrlDown && InputManager.IsKeyPressed('C'))
		{
			if (SelectionManager)
			{
				// 선택된 Actor가 있으면 Actor 복사
				if (SelectionManager->IsActorMode() && SelectionManager->GetSelectedActor())
				{
					ClipboardManager->CopyActor(SelectionManager->GetSelectedActor());
				}
			}
		}
		// Ctrl + V: 붙여넣기
		else if (bIsCtrlDown && InputManager.IsKeyPressed('V'))
		{
			// Actor 붙여넣기
			if (ClipboardManager->HasCopiedActor())
			{
				// 오프셋 적용 (1미터씩 이동)
				FVector Offset(1.0f, 1.0f, 1.0f);
				AActor* NewActor = ClipboardManager->PasteActorToWorld(World, Offset);

				if (NewActor && SelectionManager)
				{
					// 새로 생성된 Actor 선택
					SelectionManager->ClearSelection();
					SelectionManager->SelectActor(NewActor);
				}
			}
		}
	}
}

void FViewportClient::Draw(FViewport* Viewport)
{
	if (!Viewport || !World)
	{
		return;
	}

	URenderer* Renderer = URenderManager::GetInstance().GetRenderer();
	if (!Renderer)
	{
		return;
	}

	// 뷰포트 렌더 타겟 오버라이드 설정 (ImGui::Image 방식, 뷰어용)
	D3D11RHI* RHIDevice = Renderer->GetRHIDevice();
	if (Viewport->UseRenderTarget())
	{
		ID3D11RenderTargetView* ViewportRTV = Viewport->GetRTV();
		ID3D11DepthStencilView* ViewportDSV = Viewport->GetDSV();
		if (ViewportRTV && ViewportDSV)
		{
			RHIDevice->SetViewportRenderTargetOverride(ViewportRTV, ViewportDSV);
		}
	}

	// PIE 중 렌더 호출
	if (World->bPie)
	{
		APlayerCameraManager* PlayerCameraManager = World->GetPlayerCameraManager();
		if (PlayerCameraManager)
		{
			PlayerCameraManager->CacheViewport(Viewport);	// 한프레임 지연 있고, 단일 뷰포트만 지원 (일단 이렇게 처리)

			// PIE 중에도 카메라가 없으면 에디터 카메라로 fallback 처리
			if (PlayerCameraManager->GetViewCamera())
			{
				FMinimalViewInfo* MinimalViewInfo = PlayerCameraManager->GetCurrentViewInfo();
				TArray<FPostProcessModifier> Modifiers = PlayerCameraManager->GetModifiers();

				World->GetRenderSettings().SetViewMode(ViewMode);

				FSceneView CurrentViewInfo(MinimalViewInfo, &World->GetRenderSettings());
				CurrentViewInfo.Modifiers = Modifiers;

				// 배경색 설정 (ViewportClient에서 설정된 색상 사용)
				CurrentViewInfo.BackgroundColor = BackgroundColor;

				// 더 명확한 이름의 함수를 호출
				Renderer->RenderSceneForView(World, &CurrentViewInfo, Viewport);
				// 뷰포트 렌더 타겟 오버라이드 해제 (뷰어용)
				if (Viewport->UseRenderTarget())
				{
					RHIDevice->ClearViewportRenderTargetOverride();
					// RTV를 언바인딩하여 SRV로 사용 가능하게 함 (D3D11 제약)
					RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithoutDepth);
				}
				return;
			}
		}
	}

	// 1. 뷰 타입에 따라 카메라 설정 등 사전 작업을 먼저 수행
	switch (ViewportType)
	{
	case EViewportType::Perspective:
	{
		Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Perspective);
		break;
	}
	default: // 모든 Orthographic 케이스
	{
		Camera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
		SetupCameraMode();
		break;
	}
	}

	// 2. ViewMode를 먼저 설정한 후 FSceneView를 생성해야 올바른 셰이더 매크로가 생성됨
	World->GetRenderSettings().SetViewMode(ViewMode);

	FSceneView RenderView(Camera->GetCameraComponent(), Viewport, &World->GetRenderSettings());

	// 배경색 설정 (ViewportClient에서 설정된 색상 사용)
	RenderView.BackgroundColor = BackgroundColor;

	// 더 명확한 이름의 함수를 호출
	Renderer->RenderSceneForView(World, &RenderView, Viewport);

	// 뷰포트 렌더 타겟 오버라이드 해제 (뷰어용)
	if (Viewport->UseRenderTarget())
	{
		RHIDevice->ClearViewportRenderTargetOverride();
		// RTV를 언바인딩하여 SRV로 사용 가능하게 함 (D3D11 제약)
		RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithoutDepth);
	}
}

void FViewportClient::SetupCameraMode()
{
	switch (ViewportType)
	{
	case EViewportType::Perspective:

		Camera->SetActorLocation(PerspectiveCameraPosition);
		Camera->SetRotationFromEulerAngles(PerspectiveCameraRotation);
		Camera->GetCameraComponent()->SetFOV(PerspectiveCameraFov);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 2000.0f);
		break;
	case EViewportType::Orthographic_Top:
	{
		FVector newPos = { CameraAddPosition.X, CameraAddPosition.Y, 1000 };
		Camera->SetActorLocation(newPos);
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 90, 0 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 2000.0f);
		break;
	}
	case EViewportType::Orthographic_Bottom:

		Camera->SetActorLocation({ CameraAddPosition.X, CameraAddPosition.Y, -1000 });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, -90, 0 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 2000.0f);
		break;
	case EViewportType::Orthographic_Left:
		Camera->SetActorLocation({ CameraAddPosition.X, 1000 , CameraAddPosition.Z });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, -90 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 2000.0f);
		break;
	case EViewportType::Orthographic_Right:
		Camera->SetActorLocation({ CameraAddPosition.X, -1000, CameraAddPosition.Z });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 90 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 2000.0f);
		break;

	case EViewportType::Orthographic_Front:
		Camera->SetActorLocation({ -1000 , CameraAddPosition.Y, CameraAddPosition.Z });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 0 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 2000.0f);
		break;
	case EViewportType::Orthographic_Back:
		Camera->SetActorLocation({ 1000 , CameraAddPosition.Y, CameraAddPosition.Z });
		Camera->SetActorRotation(FQuat::MakeFromEulerZYX({ 0, 0, 180 }));
		Camera->GetCameraComponent()->SetFOV(100);
		Camera->GetCameraComponent()->SetClipPlanes(0.1f, 2000.0f);
		break;
	}
}

void FViewportClient::MouseMove(FViewport* Viewport, int32 X, int32 Y)
{
	if (World->GetGizmoActor())
		World->GetGizmoActor()->ProcessGizmoInteraction(Camera, Viewport, static_cast<float>(X), static_cast<float>(Y));

	if (!bIsMouseButtonDown &&
		(!World->GetGizmoActor() || !World->GetGizmoActor()->GetbIsHovering()) &&
		bIsMouseRightButtonDown) // 직교투영이고 마우스 버튼이 눌려있을 때
	{
		if (ViewportType != EViewportType::Perspective)
		{
			// 마우스가 숨겨져 있을 때는 InputManager의 MouseDelta를 사용
			UInputManager& InputManager = UInputManager::GetInstance();
			FVector2D MouseDelta = InputManager.GetMouseDelta();
			float deltaX = MouseDelta.X;
			float deltaY = MouseDelta.Y;

			if (Camera && (deltaX != 0.0f || deltaY != 0.0f))
			{
				// 기준 픽셀→월드 스케일
				const float basePixelToWorld = 0.05f;

				// 줌인(값↑)일수록 더 천천히 움직이도록 역수 적용
				float zoom = Camera->GetCameraComponent()->GetZoomFactor();
				zoom = (zoom <= 0.f) ? 1.f : zoom; // 안전장치
				const float pixelToWorld = basePixelToWorld * zoom;

				const FVector right = Camera->GetRight();
				const FVector up = Camera->GetUp();

				CameraAddPosition = CameraAddPosition
					- right * (deltaX * pixelToWorld)
					+ up * (deltaY * pixelToWorld);

				SetupCameraMode();
			}
		}
		else if (ViewportType == EViewportType::Perspective)
		{
			PerspectiveCameraInput = true;
		}
	}
}

void FViewportClient::MouseButtonDown(FViewport* Viewport, int32 X, int32 Y, int32 Button)
{
	if (!Viewport || !World) // Only handle left mouse button
		return;

	// GetInstance viewport size
	FVector2D ViewportSize(static_cast<float>(Viewport->GetSizeX()), static_cast<float>(Viewport->GetSizeY()));
	FVector2D ViewportOffset(static_cast<float>(Viewport->GetStartX()), static_cast<float>(Viewport->GetStartY()));

	// X, Y are already local coordinates within the viewport, convert to global coordinates for picking
	FVector2D ViewportMousePos(static_cast<float>(X) + ViewportOffset.X, static_cast<float>(Y) + ViewportOffset.Y);
	UPrimitiveComponent* PickedComponent = nullptr;
	TArray<AActor*> AllActors = World->GetActors();
	if (Button == 0)
	{
		if (!World->GetGizmoActor())
			return;

		bIsMouseButtonDown = true;
		// 뷰포트의 실제 aspect ratio 계산
		float PickingAspectRatio = ViewportSize.X / ViewportSize.Y;
		if (ViewportSize.Y == 0) PickingAspectRatio = 1.0f; // 0으로 나누기 방지
		if (World->GetGizmoActor()->GetbIsHovering())
		{
			return;
		}
		Camera->SetWorld(World);
		PickedComponent = URenderManager::GetInstance().GetRenderer()->GetPrimitiveCollided(static_cast<int>(ViewportMousePos.X), static_cast<int>(ViewportMousePos.Y));
		// PickedActor = CPickingSystem::PerformViewportPicking(AllActors, Camera, ViewportMousePos, ViewportSize, ViewportOffset, PickingAspectRatio,  Viewport);


		if (PickedComponent)
		{
			if (World) World->GetSelectionManager()->SelectComponent(PickedComponent);

		}
		else
		{
			// Clear selection if nothing was picked
			if (World) World->GetSelectionManager()->ClearSelection();
		}
	}
	else if (Button == 1)
	{
		//우클릭시
		bIsMouseRightButtonDown = true;
		MouseLastX = X;
		MouseLastY = Y;
	}

}

void FViewportClient::MouseButtonUp(FViewport* Viewport, int32 X, int32 Y, int32 Button)
{
	if (Button == 0) // Left mouse button
	{
		bIsMouseButtonDown = false;

		// 드래그 종료 처리를 위해 한번 더 호출
		if (World->GetGizmoActor())
		{
			World->GetGizmoActor()->ProcessGizmoInteraction(Camera, Viewport, static_cast<float>(X), static_cast<float>(Y));
		}
	}
	else
	{
		bIsMouseRightButtonDown = false;
		PerspectiveCameraInput = false;
	}
}

void FViewportClient::MouseWheel(float DeltaSeconds)
{
	if (!Camera) return;

	UCameraComponent* CameraComponent = Camera->GetCameraComponent();
	if (!CameraComponent) return;

	// OwnerWindow가 설정된 경우(메인 에디터 뷰포트)만 체크
	if (OwnerWindow != nullptr)
	{
		// ImGui가 마우스 입력을 캡처 중이면 뷰포트에서 마우스 휠 처리 안 함
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse)
			return;

		// ActiveViewport가 설정되어 있고, 자신이 아니면 처리 안 함
		// (다른 뷰포트에서 우클릭 드래그 중일 때 줌 인/아웃 방지)
		if (USlateManager::ActiveViewport != nullptr && USlateManager::ActiveViewport != OwnerWindow)
			return;
	}
	// 뷰어 창(OwnerWindow == nullptr)은 WantCaptureMouse 체크 안 함
	// (뷰어는 ImGui 윈도우 내부에 있어서 항상 WantCaptureMouse가 true이므로)

	float WheelDelta = UInputManager::GetInstance().GetMouseWheelDelta();

	// 마우스 휠이 움직이지 않았으면 처리 안 함
	if (WheelDelta == 0.0f)
		return;

	// 우클릭이 눌려있을 때: 카메라 이동 속도 조절
	if (bIsMouseRightButtonDown)
	{
		float currentSpeed = Camera->GetCameraSpeed();
		// 휠 업(양수): 속도 증가, 휠 다운(음수): 속도 감소
		currentSpeed += WheelDelta * 1.0f;
		// 속도 범위 제한 (1.0 ~ 100.0)
		currentSpeed = std::max(1.0f, std::min(100.0f, currentSpeed));
		Camera->SetCameraSpeed(currentSpeed);
	}
	// 우클릭이 안 눌려있을 때: 줌 조절
	else
	{
		float zoomFactor = CameraComponent->GetZoomFactor();
		zoomFactor *= (1.0f - WheelDelta * DeltaSeconds * 100.0f);
		CameraComponent->SetZoomFactor(zoomFactor);
	}
}

