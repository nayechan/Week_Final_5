#include "pch.h"
#include "ActorTerminationWidget.h"
#include "GizmoActor.h"
#include "../UIManager.h"
#include "../../ImGui/imgui.h"
#include "../../Actor.h"
#include "../../InputManager.h"
#include "../../World.h"

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

UActorTerminationWidget::UActorTerminationWidget()
	: UWidget("Actor Termination Widget")
	, SelectedActor(nullptr)
	, UIManager(&UUIManager::GetInstance())
{
}

UActorTerminationWidget::~UActorTerminationWidget() = default;

void UActorTerminationWidget::Initialize()
{
	// UIManager 참조 확보
	UIManager = &UUIManager::GetInstance();
}

void UActorTerminationWidget::Update()
{
	// UIManager를 통해 현재 선택된 액터 가져오기
	if (UIManager)
	{
		AActor* CurrentSelectedActor = UIManager->GetSelectedActor();
		
		// Update Current Selected Actor
		if (SelectedActor != CurrentSelectedActor)
		{
			SelectedActor = CurrentSelectedActor;
			// 새로 선택된 액터의 이름을 안전하게 캐시
			if (SelectedActor)
			{
				try 
				{
					CachedActorName = SelectedActor->GetName().ToString();
				}
				catch (...)
				{
					CachedActorName = "[Invalid Actor]";
					SelectedActor = nullptr;
				}
			}
			else
			{
				CachedActorName = "";
			}
		}
	}
}

void UActorTerminationWidget::RenderWidget()
{
	auto& InputManager = UInputManager::GetInstance();

	if (SelectedActor)
	{
		// 캐시된 이름 사용하여 안전하게 출력
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Selected: %s (%p)",
		                   CachedActorName.c_str(), SelectedActor);
		// 키보드 delete 버튼은 해당 윈도우가 포커스된 상태에서만 작동
		if (ImGui::Button("Delete Selected") || (ImGui::IsKeyPressed(ImGuiKey_Delete) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
		{
			DeleteSelectedActor();
		}
	}
	else
	{
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No Actor Selected For Deletion");
	}

	ImGui::Separator();
}

/**
 * @brief Selected Actor 삭제 함수
 */
void UActorTerminationWidget::DeleteSelectedActor()
{
	if (!SelectedActor)
	{
		UE_LOG("ActorTerminationWidget: No Actor Selected For Deletion");
		return;
	}

	if (!UIManager)
	{
		UE_LOG("ActorTerminationWidget: UIManager not available");
		return;
	}

	// UIManager를 통해 World에 접근
	UWorld* World = UIManager->GetWorld();
	if (!World)
	{
		UE_LOG("ActorTerminationWidget: No World available for deletion");
		return;
	}

	// 안전한 로깅을 위해 캐시된 이름 사용
	UE_LOG("ActorTerminationWidget: Deleting Selected Actor: %s (%p)",
	       CachedActorName.empty() ? "UnNamed" : CachedActorName.c_str(),
	       SelectedActor);

	// 삭제 전에 로컬 변수에 저장
	AActor* ActorToDelete = SelectedActor;
	
	// 즉시 UI 상태 정리
	SelectedActor = nullptr;
	CachedActorName = "";
	UIManager->ResetPickedActor();

	// Transform 위젯의 선택도 해제
	UIManager->ClearTransformWidgetSelection();

	// 기즈모가 이 액터를 타겟으로 잡고 있다면 해제
	if (AGizmoActor* Gizmo = UIManager->GetGizmoActor())
	{
		if (Gizmo->GetTargetActor() == ActorToDelete)
		{
			Gizmo->SetTargetActor(nullptr);
		}
	}

	// World를 통해 액터 삭제
	if (World->DestroyActor(ActorToDelete))
	{
		UE_LOG("ActorTerminationWidget: Actor successfully deleted");
	}
	else
	{
		UE_LOG("ActorTerminationWidget: Failed to delete actor");
	}
}
