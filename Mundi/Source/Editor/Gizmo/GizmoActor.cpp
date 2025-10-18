#include "pch.h"
#include "Grid/GridActor.h"
#include "Gizmo/GizmoActor.h"
#include "Gizmo/GizmoArrowComponent.h"
#include "Gizmo/GizmoScaleComponent.h"
#include "Gizmo/GizmoRotateComponent.h"
#include "RenderSettings.h"
#include "CameraActor.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "UIManager.h"
#include "FViewport.h"
#include "Picking.h"
#include "EditorEngine.h"

IMPLEMENT_CLASS(AGizmoActor)

AGizmoActor::AGizmoActor()
{
	Name = "Gizmo Actor";

	const float GizmoTotalSize = 1.5f;

	//======= Arrow Component 생성 =======
	/*ArrowX = NewObject<UGizmoArrowComponent>();
	ArrowY = NewObject<UGizmoArrowComponent>();
	ArrowZ = NewObject<UGizmoArrowComponent>();*/

	ArrowX = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
	ArrowY = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");
	ArrowZ = CreateDefaultSubobject<UGizmoArrowComponent>("GizmoArrowComponent");

	ArrowX->SetDirection(FVector(1.0f, 0.0f, 0.0f));//빨 
	ArrowY->SetDirection(FVector(0.0f, 1.0f, 0.0f));//초
	ArrowZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));//파

	ArrowX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	ArrowY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	ArrowZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	ArrowX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ArrowY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ArrowZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	ArrowX->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	ArrowY->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	ArrowZ->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });

	AddOwnedComponent(ArrowX);
	AddOwnedComponent(ArrowY);
	AddOwnedComponent(ArrowZ);
	GizmoArrowComponents.Add(ArrowX);
	GizmoArrowComponents.Add(ArrowY);
	GizmoArrowComponents.Add(ArrowZ);

	if (ArrowX) ArrowX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (ArrowY) ArrowY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (ArrowZ) ArrowZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	//======= Rotate Component 생성 =======
	RotateX = NewObject<UGizmoRotateComponent>();
	RotateY = NewObject<UGizmoRotateComponent>();
	RotateZ = NewObject<UGizmoRotateComponent>();

	RotateX->SetDirection(FVector(1.0f, 0.0f, 0.0f));
	RotateY->SetDirection(FVector(0.0f, 1.0f, 0.0f));
	RotateZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));

	RotateX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	RotateY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	RotateZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	RotateX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	RotateY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	RotateZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	RotateX->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	RotateY->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	RotateZ->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });

	AddOwnedComponent(RotateX);
	AddOwnedComponent(RotateY);
	AddOwnedComponent(RotateZ);
	GizmoRotateComponents.Add(RotateX);
	GizmoRotateComponents.Add(RotateY);
	GizmoRotateComponents.Add(RotateZ);

	if (RotateX) RotateX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (RotateY) RotateY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (RotateZ) RotateZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	//======= Scale Component 생성 =======
	ScaleX = NewObject<UGizmoScaleComponent>();
	ScaleY = NewObject<UGizmoScaleComponent>();
	ScaleZ = NewObject<UGizmoScaleComponent>();

	ScaleX->SetDirection(FVector(1.0f, 0.0f, 0.0f));
	ScaleY->SetDirection(FVector(0.0f, 1.0f, 0.0f));
	ScaleZ->SetDirection(FVector(0.0f, 0.0f, 1.0f));

	ScaleX->SetColor(FVector(1.0f, 0.0f, 0.0f));
	ScaleY->SetColor(FVector(0.0f, 1.0f, 0.0f));
	ScaleZ->SetColor(FVector(0.0f, 0.0f, 1.0f));

	ScaleX->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ScaleY->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
	ScaleZ->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);

	ScaleX->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	ScaleY->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });
	ScaleZ->SetDefaultScale({ GizmoTotalSize, GizmoTotalSize, GizmoTotalSize });

	if (ScaleX) ScaleX->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 0)));
	if (ScaleY) ScaleY->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, 0, 90)));
	if (ScaleZ) ScaleZ->SetRelativeRotation(FQuat::MakeFromEulerZYX(FVector(0, -90, 0)));

	AddOwnedComponent(ScaleX);
	AddOwnedComponent(ScaleY);
	AddOwnedComponent(ScaleZ);
	GizmoScaleComponents.Add(ScaleX);
	GizmoScaleComponents.Add(ScaleY);
	GizmoScaleComponents.Add(ScaleZ);

	CurrentMode = EGizmoMode::Translate;

	// 매니저 참조 초기화 (월드 소유)
	SelectionManager = GetWorld() ? GetWorld()->GetSelectionManager() : nullptr;
	InputManager = &UInputManager::GetInstance();
	UIManager = &UUIManager::GetInstance();
}

void AGizmoActor::Tick(float DeltaSeconds)
{
	if (!SelectionManager) SelectionManager = GetWorld() ? GetWorld()->GetSelectionManager() : nullptr;
	if (!InputManager) InputManager = &UInputManager::GetInstance();
	if (!UIManager) UIManager = &UUIManager::GetInstance();

	// 컴포넌트 활성화 상태 업데이트    
	if (SelectionManager->HasSelection() && CameraActor)
	{
		USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();

		// 기즈모 위치를 선택된 액터 위치로 업데이트
		if (SelectedComponent)
		{
			SetSpaceWorldMatrix(CurrentSpace, SelectedComponent);
			SetActorLocation(SelectedComponent->GetWorldLocation());
		}
	}
	UpdateComponentVisibility();
}

AGizmoActor::~AGizmoActor()
{
	// Components are centrally destroyed by AActor's destructor
	ArrowX = ArrowY = ArrowZ = nullptr;
	ScaleX = ScaleY = ScaleZ = nullptr;
	RotateX = RotateY = RotateZ = nullptr;
}

void AGizmoActor::SetMode(EGizmoMode NewMode)
{
	CurrentMode = NewMode;
}
EGizmoMode  AGizmoActor::GetMode()
{
	return CurrentMode;
}

void AGizmoActor::SetSpaceWorldMatrix(EGizmoSpace NewSpace, USceneComponent* SelectedComponent)
{
	SetSpace(NewSpace);

	if (!SelectedComponent)
		return;

	if (NewSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)
	{
		// [수정] 기즈모 액터 자체를 타겟의 회전으로 설정합니다.
		FQuat TargetRot = SelectedComponent->GetWorldRotation();
		SetActorRotation(TargetRot);
	}
	else if (NewSpace == EGizmoSpace::World)
	{
		// [수정] 기즈모 액터를 월드 축에 정렬 (단위 회전으로 설정)
		SetActorRotation(FQuat::Identity());
	}
}

void AGizmoActor::NextMode(EGizmoMode GizmoMode)
{
	CurrentMode = GizmoMode;
}

TArray<USceneComponent*>* AGizmoActor::GetGizmoComponents()
{
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
		return &GizmoArrowComponents;
	case EGizmoMode::Rotate:
		return &GizmoRotateComponents;
	case EGizmoMode::Scale:
		return &GizmoScaleComponents;
	}
	return nullptr;
}

EGizmoMode AGizmoActor::GetGizmoMode() const
{
	return CurrentMode;
}

// 개선된 축 투영 함수 - 수직 각도에서도 안정적
static FVector2D GetStableAxisDirection(const FVector& WorldAxis, const ACameraActor* Camera)
{
	const FVector CameraRight = Camera->GetRight();
	const FVector CameraUp = Camera->GetUp();
	const FVector CameraForward = Camera->GetForward();

	// 축과 카메라 forward의 각도 확인 (수직도 측정)
	float ForwardDot = FVector::Dot(WorldAxis.GetNormalized(), CameraForward.GetNormalized());
	float PerpendicularThreshold = 0.95f; // cos(약 18도)

	// 거의 수직인 경우 (축이 스크린과 거의 평행)
	if (std::fabs(ForwardDot) > PerpendicularThreshold)
	{
		// 가장 큰 성분을 가진 카메라 축 성분 사용
		float RightComponent = std::fabs(FVector::Dot(WorldAxis, CameraRight));
		float UpComponent = std::fabs(FVector::Dot(WorldAxis, CameraUp));

		if (RightComponent > UpComponent)
		{
			// Right 성분이 더 클 때: X축 우선 (원래 부호 유지)
			float Sign = FVector::Dot(WorldAxis, CameraRight) > 0 ? 1.0f : -1.0f;
			return FVector2D(Sign, 0.0f);
		}
		else
		{
			// Up 성분이 더 클 때: Y축 우선 (DirectX는 Y가 아래쪽이므로 반전)
			float Sign = FVector::Dot(WorldAxis, CameraUp) > 0 ? -1.0f : 1.0f;
			return FVector2D(0.0f, Sign);
		}
	}

	// 일반적인 경우: 스크린 투영 사용
	float RightDot = FVector::Dot(WorldAxis, CameraRight);
	float UpDot = FVector::Dot(WorldAxis, CameraUp);

	// DirectX 스크린 좌표계 고려 (Y축 반전)
	FVector2D ScreenDirection = FVector2D(RightDot, -UpDot);

	// 안전한 정규화 (최소 길이 보장)
	float Length = ScreenDirection.Length();
	if (Length > 0.001f)
	{
		return ScreenDirection * (1.0f / Length);
	}

	// 예외 상황: 기본 X축 방향
	return FVector2D(1.0f, 0.0f);
}

void AGizmoActor::OnDrag(USceneComponent* Target, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera, FViewport* Viewport)
{
	if (!Target || !Camera)
		return;

	FVector2D MouseDelta = FVector2D(MouseDeltaX, MouseDeltaY);

	FVector Axis{};
	FVector GizmoPosition = GetActorLocation();

	// ────────────── World / Local 축 선택 ──────────────
	if (CurrentSpace == EGizmoSpace::Local || CurrentMode == EGizmoMode::Scale)	// Scale 은 항상 로컬 축 기준
	{
		switch (GizmoAxis)
		{
		case 1: Axis = Target->GetWorldRotation().RotateVector(FVector(1, 0, 0));   break; // Local X
		case 2: Axis = Target->GetWorldRotation().RotateVector(FVector(0, 1, 0)); break; // Local Y
		case 3: Axis = Target->GetWorldRotation().RotateVector(FVector(0, 0, 1));      break; // Local Z
		}
	}
	else if (CurrentSpace == EGizmoSpace::World)
	{
		switch (GizmoAxis)
		{
		case 1: Axis = FVector(1, 0, 0); break;
		case 2: Axis = FVector(0, 1, 0); break;
		case 3: Axis = FVector(0, 0, 1); break;
		}
	}


	// ────────────── 모드별 처리 ──────────────
	switch (CurrentMode)
	{
	case EGizmoMode::Translate:
	{
		FVector2D ScreenAxis = GetStableAxisDirection(Axis, Camera);
		float px = (MouseDelta.X * ScreenAxis.X + MouseDelta.Y * ScreenAxis.Y);
		float h = Viewport ? static_cast<float>(Viewport->GetSizeY()) : UInputManager::GetInstance().GetScreenSize().Y;
		if (h <= 0.0f) h = 1.0f;
		float w = Viewport ? static_cast<float>(Viewport->GetSizeX()) : UInputManager::GetInstance().GetScreenSize().X;
		float aspect = w / h;
		FMatrix Proj = Camera->GetProjectionMatrix(aspect, Viewport);
		bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
		float worldPerPixel = 0.0f;
		if (bOrtho)
		{
			float halfH = 1.0f / Proj.M[1][1];
			worldPerPixel = (2.0f * halfH) / h;
		}
		else
		{
			float yScale = Proj.M[1][1];
			FVector camPos = Camera->GetActorLocation();
			FVector gizPos = GetActorLocation();
			FVector camF = Camera->GetForward();
			float z = FVector::Dot(gizPos - camPos, camF);
			if (z < 1.0f) z = 1.0f;
			worldPerPixel = (2.0f * z) / (h * yScale);
		}
		float Movement = px * worldPerPixel;
		FVector CurrentLocation = Target->GetWorldLocation();
		Target->SetWorldLocation(CurrentLocation + Axis * Movement);

		break;
	}
	case EGizmoMode::Scale:
	{
		FVector2D ScreenAxis = GetStableAxisDirection(Axis, Camera);
		float px = (MouseDelta.X * ScreenAxis.X + MouseDelta.Y * ScreenAxis.Y);
		float h = Viewport ? static_cast<float>(Viewport->GetSizeY()) : UInputManager::GetInstance().GetScreenSize().Y;
		if (h <= 0.0f) h = 1.0f;
		float w = Viewport ? static_cast<float>(Viewport->GetSizeX()) : UInputManager::GetInstance().GetScreenSize().X;
		float aspect = w / h;
		FMatrix Proj = Camera->GetProjectionMatrix(aspect, Viewport);
		bool bOrtho = std::fabs(Proj.M[3][3] - 1.0f) < KINDA_SMALL_NUMBER;
		float worldPerPixel = 0.0f;
		if (bOrtho)
		{
			float halfH = 1.0f / Proj.M[1][1];
			worldPerPixel = (2.0f * halfH) / h;
		}
		else
		{
			float yScale = Proj.M[1][1];
			FVector camPos = Camera->GetActorLocation();
			FVector gizPos = GetActorLocation();
			FVector camF = Camera->GetForward();
			float z = FVector::Dot(gizPos - camPos, camF);
			if (z < 1.0f) z = 1.0f;
			worldPerPixel = (2.0f * z) / (h * yScale);
		}
		float Movement = px * worldPerPixel;
		FVector NewScale = Target->GetWorldScale();

		// Apply movement to the correct local axis based on which gizmo was dragged
		switch (GizmoAxis)
		{
		case 1: // X Axis
			NewScale.X += Movement;
			break;
		case 2: // Y Axis
			NewScale.Y += Movement;
			break;
		case 3: // Z Axis
			NewScale.Z += Movement;
			break;
		}

		Target->SetWorldScale(NewScale);

		break;
	}
	case EGizmoMode::Rotate:
	{
		float RotationSpeed = 0.005f;
		float DeltaAngleX = MouseDeltaX * RotationSpeed;
		float DeltaAngleY = MouseDeltaY * RotationSpeed;

		float Angle = DeltaAngleX + DeltaAngleY;

		// 로컬 모드일 경우 축을 Target 로컬 축으로
		FVector RotationAxis = Axis.GetSafeNormal();

		// = FQuat::FromAxisAngle(RotationAxis.X, Angle);
		FQuat DeltaQuat{};
		FQuat CurrentRot = Target->GetWorldRotation();
		if (CurrentSpace == EGizmoSpace::World)
		{

			switch (GizmoAxis)
			{
			case 1: // X축 회전
			{
				// 마우스 X → 카메라 Up 축 기반
				FQuat RotByX = FQuat::FromAxisAngle(FVector(-1, 0, 0), DeltaAngleX);
				// 마우스 Y → 카메라 Right 축 기반
				FQuat RotByY = FQuat::FromAxisAngle(FVector(-1, 0, 0), DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			case 2: // Y축 회전
			{
				FQuat RotByX = FQuat::FromAxisAngle(FVector(0, -1, 0), DeltaAngleX);
				FQuat RotByY = FQuat::FromAxisAngle(FVector(0, -1, 0), DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			case 3: // Z축 회전
			{
				FQuat RotByX = FQuat::FromAxisAngle(FVector(0, 0, -1), DeltaAngleX);
				FQuat RotByY = FQuat::FromAxisAngle(FVector(0, 0, -1), DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			}
			FQuat NewRot = DeltaQuat * CurrentRot; // 월드 기준 회전
			Target->SetWorldRotation(NewRot);
		}
		else
		{
			float RotationSpeed = 0.005f;
			float DeltaAngleX = MouseDeltaX * RotationSpeed;
			float DeltaAngleY = MouseDeltaY * RotationSpeed;

			float Angle = DeltaAngleX + DeltaAngleY;

			// 로컬 모드일 경우 축을 Target 로컬 축으로
			FVector RotationAxis = -Axis.GetSafeNormal();

			//FQuat DeltaQuat = FQuat::FromAxisAngle(RotationAxis, Angle);

			FQuat CurrentRot = Target->GetWorldRotation();

			switch (GizmoAxis)
			{
			case 1: // X축 회전
			{
				// 마우스 X → 카메라 Up 축 기반
				FQuat RotByX = FQuat::FromAxisAngle(RotationAxis, DeltaAngleX);
				// 마우스 Y → 카메라 Right 축 기반
				FQuat RotByY = FQuat::FromAxisAngle(RotationAxis, DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			case 2: // Y축 회전
			{
				FQuat RotByX = FQuat::FromAxisAngle(RotationAxis, DeltaAngleX);
				FQuat RotByY = FQuat::FromAxisAngle(RotationAxis, DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			case 3: // Z축 회전
			{
				FQuat RotByX = FQuat::FromAxisAngle(RotationAxis, DeltaAngleX);
				FQuat RotByY = FQuat::FromAxisAngle(RotationAxis, DeltaAngleY);
				DeltaQuat = RotByX * RotByY;
				break;
			}
			}

			FQuat NewRot = DeltaQuat * CurrentRot; // 월드 기준 회전

			Target->SetWorldRotation(NewRot);


			break;
		}
	}
	}
}



//void AGizmoActor::UpdateGizmoPosition()
//{
//	if (!TargetActor) return;
//
//	SetActorLocation(TargetActor->GetActorLocation());
//}

void AGizmoActor::ProcessGizmoInteraction(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	if (!SelectedComponent || !Camera) return;

	// 기즈모 드래그
	ProcessGizmoDragging(Camera, Viewport, MousePositionX, MousePositionY);

	ProcessGizmoHovering(Camera, Viewport, MousePositionX, MousePositionY);

}

void AGizmoActor::ProcessGizmoHovering(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	if (!Camera) return;

	FVector2D ViewportSize(static_cast<float>(Viewport->GetSizeX()), static_cast<float>(Viewport->GetSizeY()));
	FVector2D ViewportOffset(static_cast<float>(Viewport->GetStartX()), static_cast<float>(Viewport->GetStartY()));
	FVector2D ViewportMousePos(static_cast<float>(MousePositionX) + ViewportOffset.X,
		static_cast<float>(MousePositionY) + ViewportOffset.Y);
	if (!bIsDragging)
		GizmoAxis = CPickingSystem::IsHoveringGizmoForViewport(this, Camera, ViewportMousePos, ViewportSize, ViewportOffset, Viewport);

	if (GizmoAxis > 0)//기즈모 축이 0이상이라면 선택 된것 
	{
		bIsHovering = true;
	}
	else
	{
		bIsHovering = false;
	}

}

void AGizmoActor::ProcessGizmoDragging(ACameraActor* Camera, FViewport* Viewport, float MousePositionX, float MousePositionY)
{
	USceneComponent* SelectedComponent = SelectionManager->GetSelectedComponent();
	if (!SelectedComponent || !Camera) return;

	if (InputManager->IsMouseButtonDown(LeftButton))
	{
		FVector2D MouseDelta = InputManager->GetMouseDelta();
		if ((MouseDelta.X * MouseDelta.X + MouseDelta.Y * MouseDelta.Y) > 0.0f)
		{
			OnDrag(SelectedComponent, GizmoAxis, MouseDelta.X, MouseDelta.Y, Camera, Viewport);
			bIsDragging = true;
		}
	}
	if (InputManager->IsMouseButtonReleased(LeftButton))
	{
		bIsDragging = false;
		GizmoAxis = 0;
		SetSpaceWorldMatrix(CurrentSpace, SelectedComponent);
	}
}

void AGizmoActor::ProcessGizmoModeSwitch()
{
	// 스페이스 키로 기즈모 모드 전환
	if (InputManager->IsKeyPressed(VK_SPACE))
	{
		int GizmoModeIndex = static_cast<int>(GetMode());
		GizmoModeIndex = (GizmoModeIndex + 1) % 3;  // 3 = enum 개수
		EGizmoMode NewGizmoMode = static_cast<EGizmoMode>(GizmoModeIndex);
		NextMode(NewGizmoMode);
	}
	// Tab 키로 월드-로컬 모드 전환
	if (InputManager->IsKeyPressed(VK_TAB))
	{
		if (GetSpace() == EGizmoSpace::World)
		{
			SetSpace(EGizmoSpace::Local);
		}
		else
		{
			SetSpace(EGizmoSpace::World);
		}
	}
}

void AGizmoActor::UpdateComponentVisibility()
{
	// 선택된 액터가 없으면 모든 기즈모 컴포넌트를 비활성화
	bool bHasSelection = SelectionManager && SelectionManager->HasSelection();

	// Arrow Components (Translate 모드)
	bool bShowArrows = bHasSelection && (CurrentMode == EGizmoMode::Translate);
	if (ArrowX) { ArrowX->SetActive(bShowArrows); ArrowX->SetHighlighted(GizmoAxis == 1, 1); }
	if (ArrowY) { ArrowY->SetActive(bShowArrows); ArrowY->SetHighlighted(GizmoAxis == 2, 2); }
	if (ArrowZ) { ArrowZ->SetActive(bShowArrows); ArrowZ->SetHighlighted(GizmoAxis == 3, 3); }

	// Rotate Components (Rotate 모드)
	bool bShowRotates = bHasSelection && (CurrentMode == EGizmoMode::Rotate);
	if (RotateX) { RotateX->SetActive(bShowRotates); RotateX->SetHighlighted(GizmoAxis == 1, 1); }
	if (RotateY) { RotateY->SetActive(bShowRotates); RotateY->SetHighlighted(GizmoAxis == 2, 2); }
	if (RotateZ) { RotateZ->SetActive(bShowRotates); RotateZ->SetHighlighted(GizmoAxis == 3, 3); }

	// Scale Components (Scale 모드)
	bool bShowScales = bHasSelection && (CurrentMode == EGizmoMode::Scale);
	if (ScaleX) { ScaleX->SetActive(bShowScales); ScaleX->SetHighlighted(GizmoAxis == 1, 1); }
	if (ScaleY) { ScaleY->SetActive(bShowScales); ScaleY->SetHighlighted(GizmoAxis == 2, 2); }
	if (ScaleZ) { ScaleZ->SetActive(bShowScales); ScaleZ->SetHighlighted(GizmoAxis == 3, 3); }
}

void AGizmoActor::OnDrag(USceneComponent* SelectedComponent, uint32 GizmoAxis, float MouseDeltaX, float MouseDeltaY, const ACameraActor* Camera)
{
	OnDrag(SelectedComponent, GizmoAxis, MouseDeltaX, MouseDeltaY, Camera, nullptr);
}
