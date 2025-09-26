#pragma once
#include "Widget.h"
#include "../../ImGui/imgui.h"
#include "../../Enums.h"

// Forward declarations
class UWorld;

/**
 * ShowFlagWidget
 * - Show Flag 관리 UI 위젯
 * - SceneManager나 다른 창에서 사용 가능
 * - 각 렌더링 기능을 켜고 끄는 체크박스 제공
 */
class UShowFlagWidget : public UWidget
{
public:
    DECLARE_CLASS(UShowFlagWidget, UWidget)
    
    UShowFlagWidget();
    ~UShowFlagWidget() override;
    
    /** 위젯 초기화 */
    void Initialize() override;
    void Update() override;
    void RenderWidget() override;
    
    /** Show Flag 상태 동기화 */
    void SyncWithWorld(UWorld* World);

private:
    /** 개별 Show Flag 체크박스 렌더링 */
    void RenderShowFlagCheckbox(const char* Label, EEngineShowFlags Flag, UWorld* World);
    
    /** Show Flag 카테고리별 섹션 렌더링 */
    void RenderPrimitiveSection(UWorld* World);
    void RenderDebugSection(UWorld* World);
    void RenderLightingSection(UWorld* World);
    
    /** 전체 제어 버튼들 */
    void RenderControlButtons(UWorld* World);
    
    /** World 참조 가져오기 */
    UWorld* GetWorld();
    
private:
    // UI 상태
    bool bIsExpanded = true;        // 위젯 확장/축소 상태
    bool bShowTooltips = true;      // 툴팁 표시 여부
    bool bCompactMode = false;      // 컴팩트 모드 (작은 인터페이스)
    
    // 각 플래그별 로컬 상태 (ImGui 체크박스용)
    bool bPrimitives = true;
    bool bStaticMeshes = true;
    bool bWireframe = false;
    bool bBillboardText = false;
    bool bBoundingBoxes = false;
    bool bGrid = true;
    bool bLighting = true;
    bool bOctree = false;
    
    // UI 스타일
    ImVec4 HeaderColor = ImVec4(0.4f, 0.6f, 0.9f, 1.0f);
    ImVec4 SectionColor = ImVec4(0.3f, 0.3f, 0.3f, 0.5f);
    ImVec4 ActiveColor = ImVec4(0.2f, 0.8f, 0.2f, 1.0f);
    ImVec4 InactiveColor = ImVec4(0.8f, 0.2f, 0.2f, 1.0f);
};
