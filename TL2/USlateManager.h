#pragma once
#include "Object.h"
#include "SWindow.h" // for FRect and SWindow types used by children
#include "SSplitterV.h"
#include "SSplitterH.h"
#include "SViewportWindow.h"

class SSceneIOWindow; // 새로 추가할 UI
class SDetailsWindow;
class UMenuBarWidget;

// 중앙 레이아웃/입력 라우팅/뷰포트 관리 매니저 (위젯 아님)
class USlateManager : public UObject
{
public:
    DECLARE_CLASS(USlateManager, UObject)

    // Singleton accessor
    static USlateManager& GetInstance();

    // 구성 저장/로드
    void SaveSplitterConfig();
    void LoadSplitterConfig();

    USlateManager();
    virtual ~USlateManager() override;

    USlateManager(const USlateManager&) = delete;
    USlateManager& operator=(const USlateManager&) = delete;

    void Initialize(ID3D11Device* Device, UWorld* World, const FRect& InRect);
    void SwitchLayout(EViewportLayoutMode NewMode);

    void SwitchPanel(SWindow* SwitchPanel);

    // 렌더/업데이트/입력 전달
    void Render();
    void Update(float deltaSecond);
    void ProcessInput();

    void OnMouseMove(FVector2D MousePos);
    void OnMouseDown(FVector2D MousePos, uint32 Button);
    void OnMouseUp(FVector2D MousePos, uint32 Button);

    void OnShutdown();
    // Explicit cleanup to release viewports/contexts before D3D device is destroyed
    void Shutdown();

    // 상태
    static SViewportWindow* ActiveViewport; // 현재 드래그 중인 뷰포트

    // 매니저 자체 위치/크기 (상위 윈도우 크기 기준)
    void SetRect(const FRect& InRect) { Rect = InRect; }
    const FRect& GetRect() const { return Rect; }

    void SetWorld(UWorld* InWorld)
    {
        World = InWorld;
    }

    void SetPIEWorld(UWorld* InWorld);

private:
    FRect Rect; // 이전엔 SWindow로부터 상속받던 영역 정보

    UWorld* World = nullptr;
    ID3D11Device* Device = nullptr;

    SSplitterH* RootSplitter = nullptr;

    // 두 가지 레이아웃을 미리 생성해둠
    SSplitter* FourSplitLayout = nullptr;
    SSplitter* SingleLayout = nullptr;

    // 뷰포트
    SViewportWindow* Viewports[4];
    SViewportWindow* MainViewport;

    SSplitterH* LeftTop;
    SSplitterH* LeftBottom;
    // 오른쪽 고정 UI
    SWindow* SceneIOPanel = nullptr;
    // 아래쪽 UI
    SWindow* ControlPanel = nullptr;
    SWindow* DetailPanel = nullptr;

    SSplitterV* TopPanel = nullptr;
    SSplitterV* LeftPanel = nullptr;

    SSplitterV* BottomPanel;

    // 현재 모드
    EViewportLayoutMode CurrentMode = EViewportLayoutMode::FourSplit;

    // 메뉴바 관련
    //void OnFileMenuAction(const char* action);
    //void OnEditMenuAction(const char* action);
    //void OnWindowMenuAction(const char* action);
    //void OnHelpMenuAction(const char* action);

    UMenuBarWidget* MenuBar;
};
