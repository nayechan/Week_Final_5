#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;
class UParticleEmitter;
class UParticleModule;
class ULineComponent;
struct FDistributionFloat;
struct FDistributionVector;
struct FInterpCurveFloat;
struct FInterpCurvePointFloat;
struct FInterpCurveVector;

// 개별 커브 트랙 (하나의 Distribution 프로퍼티)
struct FCurveTrack
{
	UParticleModule* Module = nullptr;
	FString PropertyName;
	FString DisplayName;
	FDistributionFloat* FloatCurve = nullptr;
	FDistributionVector* VectorCurve = nullptr;

	bool bVisible = true;  // 노란 체크박스 (표시 토글)
	bool bShowX = true;    // R/X 채널 토글
	bool bShowY = true;    // G/Y 채널 토글
	bool bShowZ = true;    // B/Z 채널 토글

	uint32 TrackColor = 0;  // 트랙 고유 색상 (ImU32)

	bool IsFloatCurve() const { return FloatCurve != nullptr; }
	bool IsVectorCurve() const { return VectorCurve != nullptr; }
};

// 커브 에디터 상태
struct FCurveEditorState
{
	// 다중 트랙 관리
	TArray<FCurveTrack> Tracks;
	int32 SelectedTrackIndex = -1;

	// 뷰 설정 (공유)
	float ViewMinTime = 0.0f;
	float ViewMaxTime = 1.0f;
	float ViewMinValue = -1.0f;
	float ViewMaxValue = 1.0f;

	// 선택된 키
	int32 SelectedKeyIndex = -1;
	int32 SelectedAxis = -1;  // Vector 커브용: 0=X, 1=Y, 2=Z, -1=전체

	// 헬퍼 함수
	bool HasModule(UParticleModule* Module) const;
	void AddModuleTracks(UParticleModule* Module);
	void RemoveModuleTracks(UParticleModule* Module);

	// 선택된 트랙 가져오기
	FCurveTrack* GetSelectedTrack()
	{
		if (SelectedTrackIndex >= 0 && SelectedTrackIndex < Tracks.Num())
		{
			return &Tracks[SelectedTrackIndex];
		}
		return nullptr;
	}

	void Reset()
	{
		Tracks.Empty();
		SelectedTrackIndex = -1;
		SelectedKeyIndex = -1;
		SelectedAxis = -1;
		ViewMinTime = 0.0f;
		ViewMaxTime = 1.0f;
		ViewMinValue = -1.0f;
		ViewMaxValue = 1.0f;
	}
};

class SParticleEditorWindow : public SViewerWindow
{
public:
	SParticleEditorWindow();
	virtual ~SParticleEditorWindow();

	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void PreRenderViewportUpdate() override;

	// 파티클 에디터 윈도우가 포커스되어 있는지 (다른 위젯에서 Delete 키 중복 처리 방지용)
	static bool bIsAnyParticleEditorFocused;

	// 파일 경로 기반 탭 검색 오버라이드
	void OpenOrFocusTab(UEditorAssetPreviewContext* Context) override;

protected:
	virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
	virtual void DestroyViewerState(ViewerState*& State) override;
	virtual FString GetWindowTitle() const override { return "Particle Editor"; }
	virtual void RenderTabsAndToolbar(EViewerType CurrentViewerType) override;

	// 패널 렌더링
	virtual void RenderLeftPanel(float PanelWidth) override;   // 뷰포트 + 디테일
	virtual void RenderRightPanel() override;                  // 이미터 패널
	virtual void RenderBottomPanel() override;                 // 커브 에디터

private:
	// 툴바
	void RenderToolbar();

	// 디테일 패널
	void RenderDetailsPanel(float PanelWidth);

	// 이미터 패널 하위 렌더링
	void RenderEmitterColumn(int32 EmitterIndex, UParticleEmitter* Emitter);
	void RenderModuleBlock(int32 EmitterIdx, int32 ModuleIdx, UParticleModule* Module);

	// 뷰포트
	void RenderViewportArea(float width, float height);

	// 4개 영역 렌더링 함수
	void RenderLeftViewportArea(float totalHeight);
	void RenderLeftDetailsArea();
	void RenderRightEmitterArea(float totalHeight);
	void RenderRightCurveArea();

	// 파일 작업
	void SaveParticleSystem();
	void SaveParticleSystemAs();
	void LoadParticleSystem();

	// 레이아웃 비율 및 크기
	float LeftViewportHeight = 350.f;   // 좌측 상단 뷰포트 높이
	float RightEmitterHeight = 400.f;   // 우측 상단 이미터 패널 높이

	// 툴바 아이콘
	UTexture* IconSave = nullptr;
	UTexture* IconLoad = nullptr;
	UTexture* IconRestart = nullptr;
	UTexture* IconBounds = nullptr;
	UTexture* IconOriginAxis = nullptr;
	UTexture* IconBackgroundColor = nullptr;
	UTexture* IconLODFirst = nullptr;
	UTexture* IconLODPrev = nullptr;
	UTexture* IconLODInsertBefore = nullptr;
	UTexture* IconLODInsertAfter = nullptr;
	UTexture* IconLODDelete = nullptr;
	UTexture* IconLODNext = nullptr;
	UTexture* IconLODLast = nullptr;
	UTexture* IconCurve = nullptr;

	// 툴바 상태
	void LoadToolbarIcons();

	// 커브 에디터
	FCurveEditorState CurveEditorState;

	// 커브 에디터 함수
	bool HasCurveProperties(UParticleModule* Module);
	void ToggleCurveTrack(UParticleModule* Module);  // 모듈 토글 (추가/제거)
	void RenderCurveGrid(struct ImDrawList* DrawList, struct ImVec2 CanvasPos, struct ImVec2 CanvasSize);
	void RenderCurveKeys(struct ImDrawList* DrawList, struct ImVec2 CanvasPos, struct ImVec2 CanvasSize);
	void RenderTangentHandles(struct ImDrawList* DrawList, FInterpCurvePointFloat& Point, float KeyX, float KeyY, struct ImVec2 CanvasSize);
	void HandleCurveInteraction(struct ImVec2 CanvasPos, struct ImVec2 CanvasSize);
	void AutoFitCurveView();

	// 캐스케이드 스타일 커브 에디터 함수
	void RenderTrackList();                           // 좌측 트랙 리스트 렌더링
	void RenderChannelButtons(FCurveTrack& Track);    // RGB 채널 버튼 렌더링
	void RenderGraphView();                           // 우측 그래프 뷰 렌더링
	void RenderTrackCurve(struct ImDrawList* DrawList, struct ImVec2 CanvasPos, struct ImVec2 CanvasSize, FCurveTrack& Track);  // 개별 트랙 커브 렌더링
	uint32 GetModuleColorInCurveEditor(UParticleModule* Module);  // 모듈이 커브 에디터에 있으면 해당 색상 반환

	// 타입데이터 중복 팝업 상태
	bool bShowTypeDataExistsPopup = false;

	// 헬퍼 함수
	ParticleEditorState* GetActiveParticleState() const
	{
		return static_cast<ParticleEditorState*>(ActiveState);
	}
};
