#include "pch.h"
#include "FPSWidget.h"
#include "ImGui/imgui.h"
#include "UIManager.h"
#include <algorithm>
#include <string>

using namespace std;
// #include "Manager/Time/Public/TimeManager.h"  // TimeManager를 찾을 수 없으므로 주석처리

//// UE_LOG 대체 매크로
//#define UE_LOG(fmt, ...)

constexpr float REFRESH_INTERVAL = 0.1f;

IMPLEMENT_CLASS(UFPSWidget)

UFPSWidget::UFPSWidget()
	: UWidget("FPS Widget")
{
}

UFPSWidget::~UFPSWidget() = default;

void UFPSWidget::Initialize()
{
	// 히스토리 초기화
	for (int i = 0; i < 60; ++i)
	{
		FrameTimeHistory[i] = 0.0f;
	}

	FrameTimeIndex = 0;
	AverageFrameTime = 0.0f;
	CurrentFPS = 0.0f;
	MinFPS = 999.0f;
	MaxFPS = 0.0f;
	TotalGameTime = 0.0f;

	UE_LOG("FPSWidget: Successfully Initialized");
}

void UFPSWidget::Update()
{
	// UIManager로부터 실제 DeltaTime을 가져옴
	UUIManager& UIMgr = UUIManager::GetInstance();
	CurrentDeltaTime = UIMgr.GetDeltaTime();
	
	// DeltaTime이 0이면 이전 값 유지 (초기화 시에만 발생)
	if (CurrentDeltaTime <= 0.0f)
	{
		CurrentDeltaTime = 0.f;
	}
	
	TotalGameTime += CurrentDeltaTime;

	// 실제 FPS 계산
	CurrentFPS = 1.0f / CurrentDeltaTime;

	// FPS 통계 업데이트
	MaxFPS = max(CurrentFPS, MaxFPS);
	MinFPS = min(CurrentFPS, MinFPS);

	// 프레임 시간 히스토리 업데이트 (ms 단위)
	FrameTimeHistory[FrameTimeIndex] = CurrentDeltaTime * 1000.0f;
	FrameTimeIndex = (FrameTimeIndex + 1) % 60;

	// 평균 프레임 시간 계산
	float Total = 0.0f;
	for (int i = 0; i < 60; ++i)
	{
		Total += FrameTimeHistory[i];
	}
	AverageFrameTime = Total / 60.0f;
}

void UFPSWidget::RenderWidget()
{
	// 러프한 일정 간격으로 FPS 및 Delta Time 정보 출력
	if (TotalGameTime - PreviousTime > REFRESH_INTERVAL)
	{
		PrintFPS = CurrentFPS;
		PrintDeltaTime = CurrentDeltaTime * 1000.0f;
		PreviousTime = TotalGameTime;
	}

	ImGui::Checkbox("Show Details", &bShowGraph);

	// Details
	if (bShowGraph)
	{
		ImGui::Text("Frame Time History:");
		ImGui::PlotLines("##FrameTime", FrameTimeHistory, 60, FrameTimeIndex,
		                 ("Average: " + to_string(AverageFrameTime) + " ms").c_str(),
		                 0.0f, 50.0f, ImVec2(0, 80));

		ImGui::Text("Statistics:");
		ImGui::Text("  Min FPS: %.1f", MinFPS);
		ImGui::Text("  Max FPS: %.1f", MaxFPS);
		ImGui::Text("  Average Frame Time: %.2f ms", AverageFrameTime);

		if (ImGui::Button("Reset Statistics"))
		{
			MinFPS = 999.0f;
			MaxFPS = 0.0f;
			for (int i = 0; i < 60; ++i)
			{
				FrameTimeHistory[i] = 0.0f;
			}
		}
	}

	ImGui::Separator();
}

ImVec4 UFPSWidget::GetFPSColor(float InFPS)
{
	if (InFPS >= 60.0f)
	{
		return {0.0f, 1.0f, 0.0f, 1.0f}; // 녹색 (우수)
	}
	else if (InFPS >= 30.0f)
	{
		return {1.0f, 1.0f, 0.0f, 1.0f}; // 노란색 (보통)
	}
	else
	{
		return {1.0f, 0.0f, 0.0f, 1.0f}; // 빨간색 (주의)
	}
}
