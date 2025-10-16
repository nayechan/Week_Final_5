#pragma once
#include "Widgets/Widget.h"
class USlateManager;

/**
 * 상단 메인 메뉴바 위젯 (File / Edit / Window / Help)
 * - 오너(USlateManager)를 주입받아 레이아웃 스위칭 등 제어
 * - 필요 시 외부 콜백으로 기본 동작 오버라이드 가능
 */
class UMenuBarWidget : public UWidget
{
public:
    DECLARE_CLASS(UMenuBarWidget, UWidget)

    UMenuBarWidget();
    explicit UMenuBarWidget(USlateManager* InOwner);

    // UWidget 인터페이스
    void Initialize() override;
    void Update() override;
    void RenderWidget() override;

    // 오너 설정
    void SetOwner(USlateManager* InOwner) { Owner = InOwner; }

    // 선택: 액션 콜백 핸들러 (설정하면 내부 기본 동작 대신 콜백 호출)
    void SetFileActionHandler(std::function<void(const char*)> Fn) { FileAction = std::move(Fn); }
    void SetEditActionHandler(std::function<void(const char*)> Fn) { EditAction = std::move(Fn); }
    void SetWindowActionHandler(std::function<void(const char*)> Fn) { WindowAction = std::move(Fn); }
    void SetHelpActionHandler(std::function<void(const char*)> Fn) { HelpAction = std::move(Fn); }

private:
    // 기본 동작(콜백 미설정 시 호출)
    void OnFileMenuAction(const char* action);
    void OnEditMenuAction(const char* action);
    void OnWindowMenuAction(const char* action);
    void OnHelpMenuAction(const char* action);

private:
    USlateManager* Owner = nullptr;

    // 외부 주도 액션을 위한 선택적 콜백
    std::function<void(const char*)> FileAction;
    std::function<void(const char*)> EditAction;
    std::function<void(const char*)> WindowAction;
    std::function<void(const char*)> HelpAction;
};
