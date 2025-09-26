#include "pch.h"
#include "ShowFlagWidget.h"
#include "../../World.h"
#include "../UIManager.h"

UShowFlagWidget::UShowFlagWidget()
{
}

UShowFlagWidget::~UShowFlagWidget()
{
}

void UShowFlagWidget::Initialize()
{
    // 기본값으로 초기화
    bPrimitives = true;
    bStaticMeshes = true; 
    bWireframe = false;
    bBillboardText = false;
    bBoundingBoxes = false;
    bGrid = true;
    bLighting = true;
}

void UShowFlagWidget::Update()
{
    // 매 프레임마다 업데이트할 내용이 있다면 여기에 추가
}

void UShowFlagWidget::RenderWidget()
{
    UWorld* World = GetWorld();
    if (!World) return;
    
    // World의 현재 Show Flag 상태와 동기화
    SyncWithWorld(World);
    
    // Show Flag 섹션 헤더
    ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
    bool bHeaderOpen = ImGui::CollapsingHeader("Show Flags", bIsExpanded ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleColor();
    
    if (bHeaderOpen)
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, SectionColor);
        
        // 컴팩트 모드 토글
        ImGui::Checkbox("Compact Mode", &bCompactMode);
        ImGui::SameLine();
        ImGui::Checkbox("Show Tooltips", &bShowTooltips);
        
        ImGui::Separator();
        
        if (ImGui::BeginChild("ShowFlagsContent", ImVec2(0, bCompactMode ? 120.0f : 200.0f), true))
        {
            if (bCompactMode)
            {
                // 컴팩트 모드: 중요한 것들만 간단히
                RenderShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives, World);
                ImGui::SameLine();
                RenderShowFlagCheckbox("Grid", EEngineShowFlags::SF_Grid, World);
                
                RenderShowFlagCheckbox("Static Meshes", EEngineShowFlags::SF_StaticMeshes, World);
                ImGui::SameLine();
                RenderShowFlagCheckbox("Text", EEngineShowFlags::SF_BillboardText, World);
                
                RenderShowFlagCheckbox("Bounds", EEngineShowFlags::SF_BoundingBoxes, World);
                ImGui::SameLine();
                RenderShowFlagCheckbox("Wireframe", EEngineShowFlags::SF_Wireframe, World);
            }
            else
            {
                // 전체 제어 버튼들
                RenderControlButtons(World);
                
                ImGui::Separator();
                
                // 카테고리별 섹션들
                RenderPrimitiveSection(World);
                ImGui::Separator();
                
                RenderDebugSection(World);
                ImGui::Separator();
                
                RenderLightingSection(World);
            }
        }
        ImGui::EndChild();
        
        ImGui::PopStyleColor();
    }
}

UWorld* UShowFlagWidget::GetWorld()
{
    // UIManager를 통해 World 참조 가져오기
    return UUIManager::GetInstance().GetWorld();
}

void UShowFlagWidget::SyncWithWorld(UWorld* World)
{
    if (!World) return;
    
    EEngineShowFlags CurrentFlags = World->GetShowFlags();
    
    // 각 플래그 상태를 로컬 변수에 동기화
    bPrimitives = World->IsShowFlagEnabled(EEngineShowFlags::SF_Primitives);
    bStaticMeshes = World->IsShowFlagEnabled(EEngineShowFlags::SF_StaticMeshes);
    bWireframe = World->IsShowFlagEnabled(EEngineShowFlags::SF_Wireframe);
    bBillboardText = World->IsShowFlagEnabled(EEngineShowFlags::SF_BillboardText);
    bBoundingBoxes = World->IsShowFlagEnabled(EEngineShowFlags::SF_BoundingBoxes);
    bGrid = World->IsShowFlagEnabled(EEngineShowFlags::SF_Grid);
    bLighting = World->IsShowFlagEnabled(EEngineShowFlags::SF_Lighting);
    bOctree = World->IsShowFlagEnabled(EEngineShowFlags::SF_OctreeDebug);
}

void UShowFlagWidget::RenderShowFlagCheckbox(const char* Label, EEngineShowFlags Flag, UWorld* World)
{
    if (!World) return;
    
    bool bCurrentState = World->IsShowFlagEnabled(Flag);
    
    // 상태에 따라 색상 변경
    ImVec4 checkboxColor = bCurrentState ? ActiveColor : InactiveColor;
    ImGui::PushStyleColor(ImGuiCol_CheckMark, checkboxColor);
    
    if (ImGui::Checkbox(Label, &bCurrentState))
    {
        // 체크박스 상태가 변경되면 World의 Show Flag 업데이트
        if (bCurrentState)
        {
            World->EnableShowFlag(Flag);
        }
        else
        {
            World->DisableShowFlag(Flag);
        }
    }
    
    ImGui::PopStyleColor();
    
    // 툴팁 표시
    if (bShowTooltips && ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        
        // 각 플래그별 설명
        switch (Flag)
        {
        case EEngineShowFlags::SF_Primitives:
            ImGui::Text("모든 프리미티브 지오메트리 표시/숨김");
            ImGui::Text("이 옵션을 끄면 모든 3D 오브젝트가 숨겨집니다.");
            break;
        case EEngineShowFlags::SF_StaticMeshes:
            ImGui::Text("Static Mesh 액터들 표시/숨김");
            ImGui::Text("일반적인 3D 메시 오브젝트들의 가시성을 제어합니다.");
            break;
        case EEngineShowFlags::SF_Wireframe:
            ImGui::Text("와이어프레임 오버레이 표시/숨김");
            ImGui::Text("3D 모델의 와이어프레임을 표시합니다.");
            break;
        case EEngineShowFlags::SF_BillboardText:
            ImGui::Text("오브젝트 위의 UUID 텍스트 표시/숨김");
            ImGui::Text("각 오브젝트 위에 표시되는 식별자 텍스트입니다.");
            break;
        case EEngineShowFlags::SF_BoundingBoxes:
            ImGui::Text("충돌 경계(Bounding Box) 표시/숨김");
            ImGui::Text("오브젝트의 충돌 범위를 시각적으로 표시합니다.");
            break;
        case EEngineShowFlags::SF_Grid:
            ImGui::Text("월드 그리드 표시/숨김");
            ImGui::Text("3D 공간의 참조용 격자를 표시합니다.");
            break;
        case EEngineShowFlags::SF_OctreeDebug:
            ImGui::Text("옥트리 디버그 바운드 표시/숨김");
            ImGui::Text("공간 분할 노드의 AABB를 라인으로 렌더링합니다.");
            break;
        case EEngineShowFlags::SF_Lighting:
            ImGui::Text("조명 효과 활성화/비활성화");
            ImGui::Text("라이팅 계산을 켜거나 끕니다.");
            break;
        default:
            ImGui::Text("Show Flag 설정");
            break;
        }
        
        ImGui::EndTooltip();
    }
}

void UShowFlagWidget::RenderPrimitiveSection(UWorld* World)
{
    ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
    if (ImGui::TreeNode("Primitive Rendering"))
    {
        ImGui::PopStyleColor();
        
        RenderShowFlagCheckbox("Primitives", EEngineShowFlags::SF_Primitives, World);
        
        // Primitives가 활성화되어 있을 때만 하위 옵션들 활성화
        bool bPrimitivesEnabled = World->IsShowFlagEnabled(EEngineShowFlags::SF_Primitives);
        if (!bPrimitivesEnabled)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }
        
        ImGui::Indent(15.0f);
        RenderShowFlagCheckbox("Static Meshes", EEngineShowFlags::SF_StaticMeshes, World);
        RenderShowFlagCheckbox("Wireframe", EEngineShowFlags::SF_Wireframe, World);
        ImGui::Unindent(15.0f);
        
        if (!bPrimitivesEnabled)
        {
            ImGui::PopStyleVar();
        }
        
        ImGui::TreePop();
    }
    else
    {
        ImGui::PopStyleColor();
    }
}

void UShowFlagWidget::RenderDebugSection(UWorld* World)
{
    ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
    if (ImGui::TreeNode("Debug Features"))
    {
        ImGui::PopStyleColor();
        
        RenderShowFlagCheckbox("Billboard Text", EEngineShowFlags::SF_BillboardText, World);
        RenderShowFlagCheckbox("Bounding Boxes", EEngineShowFlags::SF_BoundingBoxes, World);
        RenderShowFlagCheckbox("Grid", EEngineShowFlags::SF_Grid, World);
        RenderShowFlagCheckbox("Octree", EEngineShowFlags::SF_OctreeDebug, World);
        
        ImGui::TreePop();
    }
    else
    {
        ImGui::PopStyleColor();
    }
}

void UShowFlagWidget::RenderLightingSection(UWorld* World)
{
    ImGui::PushStyleColor(ImGuiCol_Text, HeaderColor);
    if (ImGui::TreeNode("Lighting"))
    {
        ImGui::PopStyleColor();
        
        RenderShowFlagCheckbox("Lighting", EEngineShowFlags::SF_Lighting, World);
        
        ImGui::TreePop();
    }
    else
    {
        ImGui::PopStyleColor();
    }
}

void UShowFlagWidget::RenderControlButtons(UWorld* World)
{
    if (!World) return;
    
    ImGui::Text("Quick Controls:");
    
    // 버튼들을 한 줄에 배치
    if (ImGui::Button("Show All"))
    {
        World->SetShowFlags(EEngineShowFlags::SF_All);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Hide All"))
    {
        World->SetShowFlags(EEngineShowFlags::None);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Reset"))
    {
        World->SetShowFlags(EEngineShowFlags::SF_DefaultEnabled);
    }
}
