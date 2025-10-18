#include "pch.h"
#include "ActorTerminationWidget.h"
#include "Gizmo/GizmoActor.h"
#include "UIManager.h"
#include "ImGui/imgui.h"
#include "Actor.h"
#include "InputManager.h"
#include "World.h"
#include "SelectionManager.h"
#include <EditorEngine.h>

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

IMPLEMENT_CLASS(UActorTerminationWidget)

UActorTerminationWidget::UActorTerminationWidget()
	: UWidget("Actor Termination Widget")
{
}

UActorTerminationWidget::~UActorTerminationWidget() = default;

void UActorTerminationWidget::Update()
{
	// SelectionManager를 통해 현재 선택된 액터 가져오기
	//AActor* SelectedActor = GWorld->GetSelectionManager()->GetSelectedActor()
	//
	//// Update Current Selected Actor
	//if (SelectedActor != CurrentSelectedActor)
	//{
	//	SelectedActor = CurrentSelectedActor;
	//	// 새로 선택된 액터의 이름을 안전하게 캐시
	//	if (SelectedActor)
	//	{
	//		try 
	//		{
	//			CachedActorName = SelectedActor->GetName().ToString();
	//		}
	//		catch (...)
	//		{
	//			CachedActorName = "[Invalid Actor]";
	//			SelectedActor = nullptr;
	//		}
	//	}
	//	else
	//	{
	//		CachedActorName = "";
	//	}
	//}
	
}

void UActorTerminationWidget::RenderWidget()
{
	AActor* SelectedActor = GWorld->GetSelectionManager()->GetSelectedActor();

	if (SelectedActor)
	{
		// 캐시된 이름 사용하여 안전하게 출력
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "Selected: %s (%p)",
		                   SelectedActor->GetName().ToString().c_str(), SelectedActor);
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
	AActor* SelectedActor = GWorld->GetSelectionManager()->GetSelectedActor();
	if (!SelectedActor)
	{
		UE_LOG("ActorTerminationWidget: No Actor Selected For Deletion");
		return;
	}
	// ??
	// UIManager를 통해 World에 접근

	// 안전한 로깅을 위해 캐시된 이름 사용
	UE_LOG("ActorTerminationWidget: Deleting Selected Actor: %s (%p)",
		SelectedActor->GetName().ToString().c_str(),
	       SelectedActor);

	// 삭제 전에 로컬 변수에 저장
	AActor* ActorToDelete = SelectedActor;
	
	// 즉시 UI 상태 정리
	GWorld->GetSelectionManager()->DeselectActor(SelectedActor);

	// 기즈모가 이 액터를 타겟으로 잡고 있다면 해제
	//if (AGizmoActor* Gizmo = GEngine.GetDefaultWorld()->GetGizmoActor())
	//{
	//	if (Gizmo->GetTargetActor() == ActorToDelete)
	//	{
	//		Gizmo->SetTargetActor(nullptr);
	//	}
	//}

	// World를 통해 액터 삭제
	if (GWorld->DestroyActor(ActorToDelete))
	{
		UE_LOG("ActorTerminationWidget: Actor successfully deleted");
	}
	else
	{
		UE_LOG("ActorTerminationWidget: Failed to delete actor");
	}
}
