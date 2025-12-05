#include "pch.h"
#include <unordered_map>
#include "SlateManager.h"
#include "SPhysicsAssetEditorWindow.h"
#include "Source/Slate/Widgets/PropertyRenderer.h"
#include "Source/Editor/Gizmo/GizmoActor.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/Viewer/PhysicsAssetEditorBootstrap.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/GameFramework/World.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysicsAsset.h"
#include "Source/Runtime/Engine/PhysicsEngine/BodySetup.h"
#include "Source/Runtime/Engine/PhysicsEngine/ConstraintInstance.h"
#include "Source/Runtime/Engine/PhysicsEngine/AggregateGeom.h"
#include "Source/Runtime/Engine/PhysicsEngine/PhysScene.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Components/ShapeAnchorComponent.h"
#include "Source/Runtime/Engine/Components/ConstraintAnchorComponent.h"
#include "Source/Runtime/Engine/Components/BoneAnchorComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Engine/Collision/Picking.h"
#include "Source/Runtime/Engine/GameFramework/CameraActor.h"
#include "Source/Runtime/Renderer/FViewportClient.h"
#include "SelectionManager.h"
#include "PlatformProcess.h"
#include "PathUtils.h"
#include "Source/Runtime/Engine/PhysicsEngine/BoneUtils.h"
#include "Source/Runtime/InputCore/InputManager.h"

// Static 변수 정의
bool SPhysicsAssetEditorWindow::bIsAnyPhysicsAssetEditorFocused = false;

// ========== Shape 와이어프레임 생성 함수들 ==========

static const float PI_CONST = 3.14159265358979323846f;

// ========== 부피 기반 질량 계산 함수들 ==========
// 기본 밀도 (kg/m³) - 인체 평균 밀도 (~1000 kg/m³, 물과 비슷)
static const float DEFAULT_BODY_DENSITY = 1000.0f;

// Sphere 부피: (4/3) * π * r³
static float CalculateSphereVolume(float Radius)
{
    return (4.0f / 3.0f) * PI_CONST * Radius * Radius * Radius;
}

// Box 부피: (2*X) * (2*Y) * (2*Z) = 8 * X * Y * Z (half extent 기준)
static float CalculateBoxVolume(float HalfX, float HalfY, float HalfZ)
{
    return 8.0f * HalfX * HalfY * HalfZ;
}

// Capsule 부피: 실린더 + 양쪽 반구 = π * r² * length + (4/3) * π * r³
static float CalculateCapsuleVolume(float Radius, float Length)
{
    float CylinderVolume = PI_CONST * Radius * Radius * Length;
    float SphereVolume = (4.0f / 3.0f) * PI_CONST * Radius * Radius * Radius;
    return CylinderVolume + SphereVolume;
}

// Body의 총 부피 계산 (모든 Shape 합산)
static float CalculateBodyTotalVolume(UBodySetup* Body)
{
    if (!Body) return 0.0f;

    float TotalVolume = 0.0f;

    // Sphere들
    for (const auto& Sphere : Body->AggGeom.SphereElems)
    {
        TotalVolume += CalculateSphereVolume(Sphere.Radius);
    }

    // Box들
    for (const auto& Box : Body->AggGeom.BoxElems)
    {
        TotalVolume += CalculateBoxVolume(Box.X, Box.Y, Box.Z);
    }

    // Capsule(Sphyl)들
    for (const auto& Capsule : Body->AggGeom.SphylElems)
    {
        TotalVolume += CalculateCapsuleVolume(Capsule.Radius, Capsule.Length);
    }

    return TotalVolume;
}

// 부피와 밀도로 질량 계산 후 Body에 적용
static void UpdateBodyMassFromVolume(UBodySetup* Body, float Density = DEFAULT_BODY_DENSITY)
{
    if (!Body) return;

    float Volume = CalculateBodyTotalVolume(Body);
    // 최소 질량 0.1kg, 최대 질량 10000kg
    Body->MassInKg = FMath::Clamp(Volume * Density, 0.1f, 10000.0f);
}

// 박스 와이어프레임 (12개 라인) - 회전 지원
static void CreateBoxWireframe(ULineComponent* LineComp, const FVector& Center, const FVector& HalfExtent, const FQuat& Rotation, const FVector4& Color)
{
    // 로컬 좌표 꼭짓점
    FVector LocalCorners[8] = {
        FVector(-HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z),
        FVector(+HalfExtent.X, -HalfExtent.Y, -HalfExtent.Z),
        FVector(+HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z),
        FVector(-HalfExtent.X, +HalfExtent.Y, -HalfExtent.Z),
        FVector(-HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z),
        FVector(+HalfExtent.X, -HalfExtent.Y, +HalfExtent.Z),
        FVector(+HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z),
        FVector(-HalfExtent.X, +HalfExtent.Y, +HalfExtent.Z),
    };

    // 월드 좌표로 변환
    FVector Corners[8];
    for (int i = 0; i < 8; ++i)
    {
        Corners[i] = Center + Rotation.RotateVector(LocalCorners[i]);
    }

    // 아래 면 (4개 라인)
    LineComp->AddLine(Corners[0], Corners[1], Color);
    LineComp->AddLine(Corners[1], Corners[2], Color);
    LineComp->AddLine(Corners[2], Corners[3], Color);
    LineComp->AddLine(Corners[3], Corners[0], Color);

    // 위 면 (4개 라인)
    LineComp->AddLine(Corners[4], Corners[5], Color);
    LineComp->AddLine(Corners[5], Corners[6], Color);
    LineComp->AddLine(Corners[6], Corners[7], Color);
    LineComp->AddLine(Corners[7], Corners[4], Color);

    // 수직 연결 (4개 라인)
    LineComp->AddLine(Corners[0], Corners[4], Color);
    LineComp->AddLine(Corners[1], Corners[5], Color);
    LineComp->AddLine(Corners[2], Corners[6], Color);
    LineComp->AddLine(Corners[3], Corners[7], Color);
}

// 구 와이어프레임 (3개 원: XY, XZ, YZ 평면)
static void CreateSphereWireframe(ULineComponent* LineComp, const FVector& Center, float Radius, const FVector4& Color)
{
    const int32 NumSegments = 16;  // 성능을 위해 세그먼트 수 감소

    // XY 평면 원
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
        float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

        FVector P1 = Center + FVector(cos(Angle1) * Radius, sin(Angle1) * Radius, 0.0f);
        FVector P2 = Center + FVector(cos(Angle2) * Radius, sin(Angle2) * Radius, 0.0f);
        LineComp->AddLine(P1, P2, Color);
    }

    // XZ 평면 원
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
        float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

        FVector P1 = Center + FVector(cos(Angle1) * Radius, 0.0f, sin(Angle1) * Radius);
        FVector P2 = Center + FVector(cos(Angle2) * Radius, 0.0f, sin(Angle2) * Radius);
        LineComp->AddLine(P1, P2, Color);
    }

    // YZ 평면 원
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
        float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

        FVector P1 = Center + FVector(0.0f, cos(Angle1) * Radius, sin(Angle1) * Radius);
        FVector P2 = Center + FVector(0.0f, cos(Angle2) * Radius, sin(Angle2) * Radius);
        LineComp->AddLine(P1, P2, Color);
    }
}

// 캡슐 와이어프레임 (2개 반구 + 4개 세로선 + 2개 링)
static void CreateCapsuleWireframe(ULineComponent* LineComp, const FVector& Center, const FQuat& Rotation, float Radius, float HalfHeight, const FVector4& Color)
{
    const int32 NumSegments = 16;  // 성능을 위해 세그먼트 수 감소
    const int32 HemiSegments = 8;

    // 캡슐 방향 (Z축 기준)
    FVector Up = Rotation.RotateVector(FVector(0, 0, 1));
    FVector Right = Rotation.RotateVector(FVector(1, 0, 0));
    FVector Forward = Rotation.RotateVector(FVector(0, 1, 0));

    FVector TopCenter = Center + Up * HalfHeight;
    FVector BottomCenter = Center - Up * HalfHeight;

    // 상단/하단 원형 링
    for (int32 i = 0; i < NumSegments; ++i)
    {
        float Angle1 = (float(i) / NumSegments) * 2.0f * PI_CONST;
        float Angle2 = (float((i + 1) % NumSegments) / NumSegments) * 2.0f * PI_CONST;

        FVector Offset1 = Right * cos(Angle1) * Radius + Forward * sin(Angle1) * Radius;
        FVector Offset2 = Right * cos(Angle2) * Radius + Forward * sin(Angle2) * Radius;

        // 상단 링
        LineComp->AddLine(TopCenter + Offset1, TopCenter + Offset2, Color);
        // 하단 링
        LineComp->AddLine(BottomCenter + Offset1, BottomCenter + Offset2, Color);
    }

    // 세로선 (4개)
    for (int32 i = 0; i < 4; ++i)
    {
        float Angle = (float(i) / 4) * 2.0f * PI_CONST;
        FVector Offset = Right * cos(Angle) * Radius + Forward * sin(Angle) * Radius;
        LineComp->AddLine(TopCenter + Offset, BottomCenter + Offset, Color);
    }

    // 상단 반구 (XZ, YZ 평면 반원)
    for (int32 i = 0; i < HemiSegments; ++i)
    {
        float Angle1 = (float(i) / HemiSegments) * PI_CONST * 0.5f;  // 0 ~ 90도
        float Angle2 = (float(i + 1) / HemiSegments) * PI_CONST * 0.5f;

        // XZ 평면 반원
        FVector P1 = TopCenter + Right * cos(Angle1) * Radius + Up * sin(Angle1) * Radius;
        FVector P2 = TopCenter + Right * cos(Angle2) * Radius + Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        P1 = TopCenter - Right * cos(Angle1) * Radius + Up * sin(Angle1) * Radius;
        P2 = TopCenter - Right * cos(Angle2) * Radius + Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        // YZ 평면 반원
        P1 = TopCenter + Forward * cos(Angle1) * Radius + Up * sin(Angle1) * Radius;
        P2 = TopCenter + Forward * cos(Angle2) * Radius + Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        P1 = TopCenter - Forward * cos(Angle1) * Radius + Up * sin(Angle1) * Radius;
        P2 = TopCenter - Forward * cos(Angle2) * Radius + Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);
    }

    // 하단 반구 (XZ, YZ 평면 반원)
    for (int32 i = 0; i < HemiSegments; ++i)
    {
        float Angle1 = (float(i) / HemiSegments) * PI_CONST * 0.5f;
        float Angle2 = (float(i + 1) / HemiSegments) * PI_CONST * 0.5f;

        // XZ 평면 반원
        FVector P1 = BottomCenter + Right * cos(Angle1) * Radius - Up * sin(Angle1) * Radius;
        FVector P2 = BottomCenter + Right * cos(Angle2) * Radius - Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        P1 = BottomCenter - Right * cos(Angle1) * Radius - Up * sin(Angle1) * Radius;
        P2 = BottomCenter - Right * cos(Angle2) * Radius - Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        // YZ 평면 반원
        P1 = BottomCenter + Forward * cos(Angle1) * Radius - Up * sin(Angle1) * Radius;
        P2 = BottomCenter + Forward * cos(Angle2) * Radius - Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);

        P1 = BottomCenter - Forward * cos(Angle1) * Radius - Up * sin(Angle1) * Radius;
        P2 = BottomCenter - Forward * cos(Angle2) * Radius - Up * sin(Angle2) * Radius;
        LineComp->AddLine(P1, P2, Color);
    }
}

// ========== 클래스 구현 ==========

SPhysicsAssetEditorWindow::SPhysicsAssetEditorWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
    LeftPanelRatio = 0.2f;
    RightPanelRatio = 0.25f;
}

SPhysicsAssetEditorWindow::~SPhysicsAssetEditorWindow()
{
    // 포커스 플래그 리셋 (윈도우 파괴 시)
    bIsAnyPhysicsAssetEditorFocused = false;

    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

void SPhysicsAssetEditorWindow::OnRender()
{
    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
        return;
    }

    // 아이콘 로드 (한 번만)
    if (!IconSave && Device)
    {
        IconSave = NewObject<UTexture>();
        IconSave->Load(GDataDir + "/Icon/Toolbar_Save.png", Device);

        IconSaveAs = NewObject<UTexture>();
        IconSaveAs->Load(GDataDir + "/Icon/Toolbar_SaveAs.png", Device);

        IconLoad = NewObject<UTexture>();
        IconLoad->Load(GDataDir + "/Icon/Toolbar_Load.png", Device);
    }

    // 매 프레임 시작 시 포커스 플래그 리셋 (윈도우가 닫혀도 리셋되도록)
    bIsAnyPhysicsAssetEditorFocused = false;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoSavedSettings;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }

    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
    }

    char UniqueTitle[256];
    FString Title = GetWindowTitle();
    sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

    bool bViewerVisible = false;
    if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
    {
        bViewerVisible = true;
        bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // 다른 위젯에서 체크할 수 있도록 static 변수 업데이트
        bIsAnyPhysicsAssetEditorFocused = bIsWindowFocused;

        RenderTabsAndToolbar(EViewerType::PhysicsAsset);

        if (!bIsOpen)
        {
            USlateManager::GetInstance().RequestCloseDetachedWindow(this);
            ImGui::End();
            return;
        }

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y;
        Rect.UpdateMinMax();

        ImVec2 contentAvail = ImGui::GetContentRegionAvail();
        float totalWidth = contentAvail.x;
        float totalHeight = contentAvail.y;
        float splitterWidth = 4.0f;

        float leftWidth = totalWidth * LeftPanelRatio;
        float rightWidth = totalWidth * RightPanelRatio;
        float centerWidth = totalWidth - leftWidth - rightWidth - (splitterWidth * 2);
        if (centerWidth < 0.0f) centerWidth = 0.0f;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // Left panel
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, totalHeight), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        RenderLeftPanel(leftWidth);
        ImGui::EndChild();

        ImGui::SameLine(0, 0);

        // Left splitter
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##LeftSplitter", ImVec2(splitterWidth, totalHeight));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f)
            {
                float newLeftRatio = LeftPanelRatio + delta / totalWidth;
                float maxLeftRatio = 1.0f - RightPanelRatio - (splitterWidth * 2) / totalWidth;
                LeftPanelRatio = std::max(0.1f, std::min(newLeftRatio, maxLeftRatio));
            }
        }

        ImGui::SameLine(0, 0);

        // Center panel
        if (centerWidth > 0.0f)
        {
            ImGui::BeginChild("CenterPanel", ImVec2(centerWidth, totalHeight), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus);

            RenderViewerToolbar();

            float viewportHeight = ImGui::GetContentRegionAvail().y;
            float viewportWidth = ImGui::GetContentRegionAvail().x;

            ImVec2 Pos = ImGui::GetCursorScreenPos();
            CenterRect.Left = Pos.x;
            CenterRect.Top = Pos.y;
            CenterRect.Right = Pos.x + viewportWidth;
            CenterRect.Bottom = Pos.y + viewportHeight;
            CenterRect.UpdateMinMax();

            OnRenderViewport();

            if (ActiveState && ActiveState->Viewport)
            {
                ID3D11ShaderResourceView* SRV = ActiveState->Viewport->GetSRV();
                if (SRV)
                {
                    ImGui::Image((void*)SRV, ImVec2(viewportWidth, viewportHeight));
                    ActiveState->Viewport->SetViewportHovered(ImGui::IsItemHovered());
                }
                else
                {
                    ImGui::Dummy(ImVec2(viewportWidth, viewportHeight));
                }
            }
            else
            {
                ImGui::Dummy(ImVec2(viewportWidth, viewportHeight));
            }

            ImGui::EndChild();
            ImGui::SameLine(0, 0);
        }
        else
        {
            CenterRect = FRect(0, 0, 0, 0);
            CenterRect.UpdateMinMax();
        }

        // Right splitter
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##RightSplitter", ImVec2(splitterWidth, totalHeight));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f)
            {
                float newRightRatio = RightPanelRatio - delta / totalWidth;
                float maxRightRatio = 1.0f - LeftPanelRatio - (splitterWidth * 2) / totalWidth;
                RightPanelRatio = std::max(0.1f, std::min(newRightRatio, maxRightRatio));
            }
        }

        ImGui::SameLine(0, 0);

        // Right panel
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("RightPanel", ImVec2(rightWidth, totalHeight), true);
        ImGui::PopStyleVar();
        RenderRightPanel();
        ImGui::EndChild();

        ImGui::PopStyleVar();

        // Delete 키로 선택된 Constraint 또는 Shape 삭제 (OnRender에서 bIsWindowFocused 설정 후 처리)
        PhysicsAssetEditorState* PhysState = GetPhysicsState();
        if (bIsWindowFocused && ImGui::IsKeyPressed(ImGuiKey_Delete))
        {
            if (PhysState && PhysState->SelectedConstraintIndex >= 0)
            {
                RemoveConstraint(PhysState->SelectedConstraintIndex);
            }
            else
            {
                DeleteSelectedShape();
            }
        }
    }
    ImGui::End();

    if (!bViewerVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
        bIsWindowHovered = false;
        bIsWindowFocused = false;
        bIsAnyPhysicsAssetEditorFocused = false;
    }

    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
    }

    bRequestFocus = false;
}

void SPhysicsAssetEditorWindow::OnUpdate(float DeltaSeconds)
{
    SViewerWindow::OnUpdate(DeltaSeconds);

    PhysicsAssetEditorState* PhysState = GetPhysicsState();

    // === 에디터 시뮬레이션 처리 ===
    // 참고: 물리 시뮬레이션 자체는 SViewerWindow::OnUpdate() → World::Tick()에서 처리됨
    //       StartFrame() → TickPhysScene() → ... → EndFrame() → WaitPhysScene() + ProcessPhysScene()
    //       여기서는 래그돌 결과만 본에 적용
    if (bSimulateInEditor && PhysState && PhysState->World)
    {
        // 래그돌 결과를 본에 적용 (에디터에서는 TickComponent가 호출되지 않으므로 수동으로)
        if (ActiveState && ActiveState->PreviewActor)
        {
            if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
            {
                MeshComp->SyncBonesFromPhysics();
            }
        }
    }

    //// Delete 키로 선택된 Constraint 또는 Shape 삭제
    //if (ImGui::IsKeyPressed(ImGuiKey_Delete))
    //{
    //    if (PhysState && PhysState->SelectedConstraintIndex >= 0)
    //    {
    //        RemoveConstraint(PhysState->SelectedConstraintIndex);
    //    }
    //    else
    //    {
    //        DeleteSelectedShape();
    //    }
    //}

    if (!PhysState) return;

    // 기즈모 드래그 상태 체크 - 드래그 중일 때만 UpdateAnchorFromShape 방지
    AGizmoActor* Gizmo = GetGizmoActor();
    if (PhysState->ShapeGizmoAnchor)
    {
        PhysState->ShapeGizmoAnchor->bIsBeingManipulated = (Gizmo && Gizmo->GetbIsDragging());
    }
    if (PhysState->ConstraintGizmoAnchor)
    {
        static bool bWasConstraintDragging = false;
        bool bIsDragging = (Gizmo && Gizmo->GetbIsDragging());

        // 키 입력에 따른 조작 모드 설정 (드래그 전에도 감지하여 기즈모 위치 변경)
        UInputManager& Input = UInputManager::GetInstance();
        bool bAlt = Input.IsKeyDown(VK_MENU);
        bool bShift = Input.IsKeyDown(VK_SHIFT);

        EConstraintManipulationMode NewMode;
        if (bShift && bAlt)
        {
            // SHIFT+ALT: Child frame만 (화살표 위치 조정 → 한 방향 굽힘)
            NewMode = EConstraintManipulationMode::ChildOnly;
        }
        else if (bAlt)
        {
            // ALT: Parent frame만 (부채꼴 방향 변경)
            NewMode = EConstraintManipulationMode::ParentOnly;
        }
        else
        {
            // 일반: 축 방향 전체 변경
            NewMode = EConstraintManipulationMode::Both;
        }

        PhysState->ConstraintGizmoAnchor->ManipulationMode = NewMode;

        // 드래그 시작 감지 - BeginManipulation() 호출
        if (bIsDragging && !bWasConstraintDragging)
        {
            PhysState->ConstraintGizmoAnchor->BeginManipulation();
        }
        bWasConstraintDragging = bIsDragging;

        PhysState->ConstraintGizmoAnchor->bIsBeingManipulated = bIsDragging;
    }

    // 기즈모로 Shape가 수정되었는지 체크
    if (PhysState->ShapeGizmoAnchor && PhysState->ShapeGizmoAnchor->bShapeModified)
    {
        PhysState->bShapesDirty = true;
        PhysState->bIsDirty = true;
        PhysState->ShapeGizmoAnchor->bShapeModified = false;
    }

    // 기즈모로 Constraint가 수정되었는지 체크
    if (PhysState->ConstraintGizmoAnchor && PhysState->ConstraintGizmoAnchor->bConstraintModified)
    {
        PhysState->bIsDirty = true;
        PhysState->ConstraintGizmoAnchor->bConstraintModified = false;
    }
}

void SPhysicsAssetEditorWindow::PreRenderViewportUpdate()
{
    if (!ActiveState || !ActiveState->PreviewActor) return;

    PhysicsAssetEditorState* PhysState = GetPhysicsState();

    // 본 포즈 업데이트 (새 메시 로드 후 본 트랜스폼이 올바르게 계산되도록)
    // 시뮬레이션 중에는 호출하지 않음 (물리 시뮬레이션 결과가 덮어써지는 것 방지)
    if (!bSimulateInEditor)
    {
        if (USkeletalMeshComponent* MeshComp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            MeshComp->ResetToRefPose();
        }
    }

    // 본이 선택된 경우: 기즈모 위치 업데이트는 SelectBone에서만 수행
    // (BlendSpaceEditor 패턴: 기즈모를 조작해도 본 위치로 돌아가지 않음,
    //  다른 본 선택 시에만 새 본 위치로 이동)

    // 본 라인 하이라이트 업데이트 (SSkeletalMeshViewerWindow 패턴)
    if (ActiveState->bShowBones)
    {
        ActiveState->bBoneLinesDirty = true;
    }
    if (ActiveState->bShowBones && ActiveState->PreviewActor && ActiveState->CurrentMesh && ActiveState->bBoneLinesDirty)
    {
        if (ULineComponent* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
        {
            LineComp->SetLineVisible(true);
        }
        ActiveState->PreviewActor->RebuildBoneLines(ActiveState->SelectedBoneIndex);
        ActiveState->bBoneLinesDirty = false;
    }

    RenderPhysicsBodies();
    RenderConstraintVisuals();

    // Constraint 기즈모 앵커 위치 업데이트 (드래그 중이 아닐 때만)
    if (PhysState && PhysState->ConstraintGizmoAnchor && !PhysState->ConstraintGizmoAnchor->bIsBeingManipulated)
    {
        PhysState->ConstraintGizmoAnchor->UpdateAnchorFromConstraint();
    }
}

void SPhysicsAssetEditorWindow::OnSave()
{
    SavePhysicsAsset();
}

void SPhysicsAssetEditorWindow::SavePhysicsAsset()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState)
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAsset: 활성 상태가 없습니다");
        return;
    }

    if (!PhysState->EditingAsset)
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAsset: 편집 중인 에셋이 없습니다");
        return;
    }

    // 파일 경로가 없으면 SaveAs 호출
    if (PhysState->CurrentFilePath.empty())
    {
        SavePhysicsAssetAs();
        return;
    }

    // 저장 수행 (FBX 경로도 함께 저장)
    FString SourceFbxPath = ActiveState ? ActiveState->LoadedMeshPath : "";
    if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(PhysState->EditingAsset, PhysState->CurrentFilePath, SourceFbxPath))
    {
        PhysState->bIsDirty = false;
        UE_LOG("[PhysicsAssetEditor] 저장 완료: %s (FBX: %s)", PhysState->CurrentFilePath.c_str(), SourceFbxPath.c_str());

        // 리소스 캐시 갱신 (콤보박스 목록에 새 파일 반영)
        UPropertyRenderer::ClearResourcesCache();

        // === Physics Asset 자동 연결 시스템 ===
        // Week_Final_5에는 SetPhysicsAssetPath가 없음 - 에디터에서만 직접 연결
        // RefreshPhysicsAssetInWorld는 에디터 전용 기능이므로 제거
        UE_LOG("[PhysicsAssetEditor] 저장 완료: %s", PhysState->CurrentFilePath.c_str());
    }
    else
    {
        UE_LOG("[PhysicsAssetEditor] 저장 실패: %s", PhysState->CurrentFilePath.c_str());
    }
}

void SPhysicsAssetEditorWindow::SavePhysicsAssetAs()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState)
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAssetAs: 활성 상태가 없습니다");
        return;
    }

    if (!PhysState->EditingAsset)
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAssetAs: 편집 중인 에셋이 없습니다");
        return;
    }

    // 저장 파일 다이얼로그 열기
    std::filesystem::path SavePath = FPlatformProcess::OpenSaveFileDialog(
        L"Data/Physics",           // 기본 디렉토리
        L"physicsasset",           // 기본 확장자
        L"Physics Asset Files",    // 파일 타입 설명
        L"NewPhysicsAsset"         // 기본 파일명
    );

    // 사용자가 취소한 경우
    if (SavePath.empty())
    {
        UE_LOG("[PhysicsAssetEditor] SavePhysicsAssetAs: 사용자가 취소했습니다");
        return;
    }

    // std::filesystem::path를 FString으로 변환 (상대 경로로 변환)
    FString SavePathStr = ResolveAssetRelativePath(NormalizePath(WideToUTF8(SavePath.wstring())), "");

    // 저장 수행 (FBX 경로도 함께 저장)
    FString SourceFbxPath = ActiveState ? ActiveState->LoadedMeshPath : "";
    if (PhysicsAssetEditorBootstrap::SavePhysicsAsset(PhysState->EditingAsset, SavePathStr, SourceFbxPath))
    {
        // 에디터 상태 업데이트
        PhysState->CurrentFilePath = SavePathStr;
        PhysState->bIsDirty = false;

        // 리소스 캐시 갱신 (콤보박스 목록에 새 파일 반영)
        UPropertyRenderer::ClearResourcesCache();

        UE_LOG("[PhysicsAssetEditor] 저장 완료: %s (FBX: %s)", SavePathStr.c_str(), SourceFbxPath.c_str());
    }
    else
    {
        UE_LOG("[PhysicsAssetEditor] 저장 실패: %s", SavePathStr.c_str());
    }
}

void SPhysicsAssetEditorWindow::LoadPhysicsAsset()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState)
    {
        UE_LOG("[PhysicsAssetEditor] LoadPhysicsAsset: 활성 상태가 없습니다");
        return;
    }

    // 불러오기 파일 다이얼로그 열기
    std::filesystem::path LoadPath = FPlatformProcess::OpenLoadFileDialog(
        L"Data/Physics",           // 기본 디렉토리
        L"physicsasset",           // 확장자 필터
        L"Physics Asset Files"     // 파일 타입 설명
    );

    // 사용자가 취소한 경우
    if (LoadPath.empty())
    {
        UE_LOG("[PhysicsAssetEditor] LoadPhysicsAsset: 사용자가 취소했습니다");
        return;
    }

    // std::filesystem::path를 FString으로 변환
    FString LoadPathStr = WideToUTF8(LoadPath.wstring());

    // 파일에서 Physics Asset 로드 (FBX 경로도 함께 반환)
    FString SourceFbxPath;
    UPhysicsAsset* LoadedAsset = PhysicsAssetEditorBootstrap::LoadPhysicsAsset(LoadPathStr, &SourceFbxPath);
    if (!LoadedAsset)
    {
        UE_LOG("[PhysicsAssetEditor] 로드 실패: %s", LoadPathStr.c_str());
        return;
    }

    // 기존 에셋 교체
    PhysState->EditingAsset = LoadedAsset;

    // SkeletalMeshComponent에 EditingAsset 설정 (디버그 렌더링을 위해)
    if (PhysState->PreviewActor)
    {
        if (USkeletalMeshComponent* SkelComp = PhysState->PreviewActor->GetSkeletalMeshComponent())
        {
            SkelComp->SetPhysicsAsset(LoadedAsset);
        }
    }

    // 파일 경로 설정 및 Dirty 플래그 해제
    PhysState->CurrentFilePath = LoadPathStr;
    PhysState->bIsDirty = false;

    // 선택 상태 초기화
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedConstraintIndex = -1;
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;

    // 소스 FBX 파일이 있으면 자동으로 로드
    if (!SourceFbxPath.empty() && ActiveState)
    {
        USkeletalMesh* Mesh = UResourceManager::GetInstance().Load<USkeletalMesh>(SourceFbxPath);
        if (Mesh && ActiveState->PreviewActor)
        {
            ActiveState->PreviewActor->SetSkeletalMesh(SourceFbxPath);
            ActiveState->CurrentMesh = Mesh;

            // MeshPathBuffer 업데이트 (Asset Browser UI에 표시)
            size_t copyLen = std::min(SourceFbxPath.size(), sizeof(ActiveState->MeshPathBuffer) - 1);
            memcpy(ActiveState->MeshPathBuffer, SourceFbxPath.c_str(), copyLen);
            ActiveState->MeshPathBuffer[copyLen] = '\0';

            // Physics Asset 로드 시에는 OnSkeletalMeshLoaded를 호출하지 않음
            // (이미 로드된 Body/Constraint 데이터를 유지하기 위해)

            ActiveState->LoadedMeshPath = SourceFbxPath;
            if (auto* Skeletal = ActiveState->PreviewActor->GetSkeletalMeshComponent())
            {
                Skeletal->SetVisibility(ActiveState->bShowMesh);
            }
            ActiveState->bBoneLinesDirty = true;
            if (auto* LineComp = ActiveState->PreviewActor->GetBoneLineComponent())
            {
                LineComp->ClearLines();
                LineComp->SetLineVisible(ActiveState->bShowBones);
            }
            // 본 라인 캐시 초기화 (다른 메시의 캐시가 남아있으면 본 모양이 이상해짐)
            // TODO: ResetBoneLinesCache는 Week_Final_5에 없음 - bBoneLinesDirty로 대체

            // 본 선택 상태 초기화 (이전 메시의 선택이 남아있으면 하이라이트가 이상해짐)
            ActiveState->SelectedBoneIndex = -1;

            // Shape 라인 재구성 필요
            PhysState->bShapesDirty = true;

            UE_LOG("[PhysicsAssetEditor] 로드 완료: %s (FBX: %s)", LoadPathStr.c_str(), SourceFbxPath.c_str());
        }
        else
        {
            UE_LOG("[PhysicsAssetEditor] 로드 완료: %s (FBX 로드 실패: %s)", LoadPathStr.c_str(), SourceFbxPath.c_str());
        }
    }
    else
    {
        UE_LOG("[PhysicsAssetEditor] 로드 완료: %s (FBX 경로 없음)", LoadPathStr.c_str());
    }
}

void SPhysicsAssetEditorWindow::OnMouseUp(FVector2D MousePos, uint32 Button)
{
    // 부모 클래스 호출 (기즈모 마우스업 처리)
    SViewerWindow::OnMouseUp(MousePos, Button);

    // 마우스 업 시 기즈모가 드래그 중이 아니면 플래그 리셋
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    AGizmoActor* Gizmo = GetGizmoActor();
    if (PhysState && PhysState->ShapeGizmoAnchor && (!Gizmo || !Gizmo->GetbIsDragging()))
    {
        PhysState->ShapeGizmoAnchor->bIsBeingManipulated = false;
    }
}

void SPhysicsAssetEditorWindow::OnMouseDown(FVector2D MousePos, uint32 Button)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->Viewport) return;
    if (!PhysState->Viewport->IsViewportHovered()) return;

    if (!CenterRect.Contains(MousePos)) return;

    FVector2D LocalPos = MousePos - FVector2D(CenterRect.Left, CenterRect.Top);

    // 기즈모 피킹 먼저 시도
    PhysState->Viewport->ProcessMouseButtonDown((int32)LocalPos.X, (int32)LocalPos.Y, (int32)Button);

    // 드래그 상태 추적 (뷰포트 밖에서도 드래그가 동작하도록)
    if (Button == 0)
    {
        bLeftMousePressed = true;
    }
    if (Button == 1)
    {
        INPUT.SetCursorVisible(false);
        INPUT.LockCursor();
        bRightMousePressed = true;
    }

    // 좌클릭일 때만 Shape/Constraint 피킹 시도
    if (Button != 0) return;

    // 기즈모가 선택되었는지 확인 - 선택됐으면 Shape/Constraint 피킹 스킵
    if (PhysState->World)
    {
        UActorComponent* SelectedComp = PhysState->World->GetSelectionManager()->GetSelectedComponent();
        if (SelectedComp)
        {
            // ShapeAnchorComponent, ConstraintAnchorComponent, 또는 BoneAnchorComponent가 선택됐으면 피킹 스킵
            if (Cast<UShapeAnchorComponent>(SelectedComp) ||
                Cast<UConstraintAnchorComponent>(SelectedComp) ||
                Cast<UBoneAnchorComponent>(SelectedComp))
            {
                return;
            }
        }
    }

    if (!PhysState->EditingAsset) return;
    if (!PhysState->CurrentMesh) return;
    if (!PhysState->Client) return;

    const FSkeleton* Skeleton = PhysState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(PhysState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    ACameraActor* Camera = PhysState->Client->GetCamera();
    if (!Camera) return;

    // 레이 생성
    FVector CameraPos = Camera->GetActorLocation();
    FVector CameraRight = Camera->GetRight();
    FVector CameraUp = Camera->GetUp();
    FVector CameraForward = Camera->GetForward();

    FVector2D ViewportMousePos(MousePos.X - CenterRect.Left, MousePos.Y - CenterRect.Top);
    FVector2D ViewportSize(CenterRect.GetWidth(), CenterRect.GetHeight());

    FRay Ray = MakeRayFromViewport(
        Camera->GetViewMatrix(),
        Camera->GetProjectionMatrix(CenterRect.GetWidth() / CenterRect.GetHeight(), PhysState->Viewport),
        CameraPos,
        CameraRight,
        CameraUp,
        CameraForward,
        ViewportMousePos,
        ViewportSize
    );

    // 모든 Body의 Shape들과 레이캐스트
    int32 ClosestBodyIndex = -1;
    int32 ClosestShapeIndex = -1;
    EShapeType ClosestShapeType = EShapeType::None;
    float ClosestDistance = FLT_MAX;

    for (int32 BodyIndex = 0; BodyIndex < PhysState->EditingAsset->Bodies.Num(); ++BodyIndex)
    {
        UBodySetup* Body = PhysState->EditingAsset->Bodies[BodyIndex];
        if (!Body) continue;

        // 본 인덱스 찾기
        int32 BoneIndex = -1;
        for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Body->BoneName.ToString())
            {
                BoneIndex = i;
                break;
            }
        }
        if (BoneIndex < 0) continue;

        FTransform BoneWorldTransform = MeshComp->GetBoneWorldTransform(BoneIndex);

        // Sphere Shape 피킹
        for (int32 i = 0; i < Body->AggGeom.SphereElems.Num(); ++i)
        {
            const FKSphereElem& Sphere = Body->AggGeom.SphereElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Sphere.Center);

            float HitT;
            if (IntersectRaySphere(Ray, ShapeCenter, Sphere.Radius, HitT))
            {
                if (HitT < ClosestDistance)
                {
                    ClosestDistance = HitT;
                    ClosestBodyIndex = BodyIndex;
                    ClosestShapeIndex = i;
                    ClosestShapeType = EShapeType::Sphere;
                }
            }
        }

        // Box Shape 피킹 (OBB 충돌 검사)
        for (int32 i = 0; i < Body->AggGeom.BoxElems.Num(); ++i)
        {
            const FKBoxElem& Box = Body->AggGeom.BoxElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Box.Center);

            // Box.Rotation은 이미 FQuat
            FQuat FinalRotation = BoneWorldTransform.Rotation * Box.Rotation;

            // Box.X/Y/Z는 half extent
            FVector HalfExtent(Box.X, Box.Y, Box.Z);

            float HitT;
            if (IntersectRayOBB(Ray, ShapeCenter, HalfExtent, FinalRotation, HitT))
            {
                if (HitT < ClosestDistance)
                {
                    ClosestDistance = HitT;
                    ClosestBodyIndex = BodyIndex;
                    ClosestShapeIndex = i;
                    ClosestShapeType = EShapeType::Box;
                }
            }
        }

        // Capsule Shape 피킹 (정확한 캡슐 충돌 검사)
        for (int32 i = 0; i < Body->AggGeom.SphylElems.Num(); ++i)
        {
            const FKSphylElem& Capsule = Body->AggGeom.SphylElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Capsule.Center);

            // 기본 축 회전: 엔진 캡슐(Z축) → PhysX 캡슐(X축) - BodyInstance/RagdollDebugRenderer와 동일
            FQuat BaseRotation = FQuat::MakeFromEulerZYX(FVector(-90.0f, 0.0f, 0.0f));
            // Capsule.Rotation은 이미 FQuat
            FQuat UserRotation = Capsule.Rotation;
            FQuat FinalLocalRotation = UserRotation * BaseRotation;
            FQuat FinalRotation = BoneWorldTransform.Rotation * FinalLocalRotation;

            // Week_Final_5 IntersectRayCapsule: (Ray, CapsuleStart, CapsuleEnd, Radius, OutT)
            // CapsuleStart/End는 반구 중심 (cylinder 양 끝)
            float CylinderHalfLength = Capsule.Length * 0.5f;
            FVector CapsuleAxis = FinalRotation.RotateVector(FVector(0, 1, 0));  // Y축이 캡슐 길이 방향
            FVector CapsuleStart = ShapeCenter - CapsuleAxis * CylinderHalfLength;
            FVector CapsuleEnd = ShapeCenter + CapsuleAxis * CylinderHalfLength;

            float HitT;
            if (IntersectRayCapsule(Ray, CapsuleStart, CapsuleEnd, Capsule.Radius, HitT))
            {
                if (HitT < ClosestDistance)
                {
                    ClosestDistance = HitT;
                    ClosestBodyIndex = BodyIndex;
                    ClosestShapeIndex = i;
                    ClosestShapeType = EShapeType::Capsule;
                }
            }
        }
    }

    // Constraint 피킹 (Shape보다 우선순위 낮음)
    int32 ClosestConstraintIndex = -1;
    float ClosestConstraintDistance = FLT_MAX;
    const float ConstraintPickRadius = 0.15f;  // 피킹 반경 (월드 스페이스)

    for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
    {
        FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];

        // Bone2 (Parent 본) 인덱스 찾기 - Constraint 위치는 Position2 기준 (Parent 본)
        int32 Bone2Index = -1;
        for (int32 j = 0; j < (int32)Skeleton->Bones.size(); ++j)
        {
            if (Skeleton->Bones[j].Name == Constraint.ConstraintBone2.ToString())
            {
                Bone2Index = j;
                break;
            }
        }
        if (Bone2Index < 0) continue;

        // Constraint 월드 위치 계산 (Bone2/Parent 본 기준, Position2 사용)
        // ConstraintAnchorComponent::UpdateAnchorFromConstraint()와 동일한 계산
        FTransform Bone2WorldTransform = MeshComp->GetBoneWorldTransform(Bone2Index);
        FVector ConstraintWorldPos = Bone2WorldTransform.Translation +
            Bone2WorldTransform.Rotation.RotateVector(Constraint.Position2);

        // Ray와 점 사이의 최단 거리 계산
        FVector RayToPoint = ConstraintWorldPos - Ray.Origin;
        float ProjectedLength = FVector::Dot(RayToPoint, Ray.Direction);

        // 카메라 뒤에 있으면 스킵
        if (ProjectedLength < 0) continue;

        FVector ClosestPointOnRay = Ray.Origin + Ray.Direction * ProjectedLength;
        float DistanceToRay = (ConstraintWorldPos - ClosestPointOnRay).Size();

        // 피킹 반경 내에 있고 Shape보다 가까우면 선택
        if (DistanceToRay < ConstraintPickRadius && ProjectedLength < ClosestConstraintDistance)
        {
            // Shape가 더 가까우면 Shape 우선
            if (ClosestBodyIndex >= 0 && ClosestDistance < ProjectedLength)
                continue;

            ClosestConstraintDistance = ProjectedLength;
            ClosestConstraintIndex = i;
        }
    }

    // 피킹 결과 적용 (Shape 우선, 그 다음 Constraint)
    if (ClosestBodyIndex >= 0 && (ClosestConstraintIndex < 0 || ClosestDistance <= ClosestConstraintDistance))
    {
        SelectBody(ClosestBodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->SelectedShapeIndex = ClosestShapeIndex;
        PhysState->SelectedShapeType = ClosestShapeType;
        UpdateShapeGizmo();
    }
    else if (ClosestConstraintIndex >= 0)
    {
        SelectConstraint(ClosestConstraintIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
    }
    else
    {
        // 아무것도 피킹되지 않으면 선택 해제
        ClearSelection();
    }
}

ViewerState* SPhysicsAssetEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    return PhysicsAssetEditorBootstrap::CreateViewerState(Name, World, Device, Context);
}

void SPhysicsAssetEditorWindow::DestroyViewerState(ViewerState*& State)
{
    PhysicsAssetEditorBootstrap::DestroyViewerState(State);
}

void SPhysicsAssetEditorWindow::OnSkeletalMeshLoaded(ViewerState* State, const FString& Path)
{
    if (!State || !State->CurrentMesh) return;

    PhysicsAssetEditorState* PhysState = static_cast<PhysicsAssetEditorState*>(State);

    // 선택 상태 초기화 (Bodies/Constraints가 비워지기 전에 먼저 초기화)
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedConstraintIndex = -1;
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;
    PhysState->ConstraintStartBodyIndex = -1;
    PhysState->GraphFocusBodyIndex = -1;
    State->SelectedBoneIndex = -1;  // 본 선택 초기화
    PhysState->bShapesDirty = true;
    PhysState->bConstraintsDirty = true;
    State->bBoneLinesDirty = true;

    // 기즈모 숨김
    UpdateShapeGizmo();
    UpdateConstraintGizmo();

    if (PhysState->EditingAsset)
    {
        PhysState->EditingAsset->Bodies.Empty();
        PhysState->EditingAsset->Constraints.Empty();
    }

    // SkeletalMeshComponent에 EditingAsset 설정 (디버그 렌더링을 위해)
    if (PhysState->PreviewActor)
    {
        if (USkeletalMeshComponent* SkelComp = PhysState->PreviewActor->GetSkeletalMeshComponent())
        {
            SkelComp->SetPhysicsAsset(PhysState->EditingAsset);
        }
    }

    UE_LOG("SPhysicsAssetEditorWindow: Loaded skeletal mesh from %s", Path.c_str());
}

void SPhysicsAssetEditorWindow::RenderLeftPanel(float PanelWidth)
{
    if (!ActiveState) return;

    // Asset Browser 섹션 (부모 클래스에서 가져옴)
    // RenderAssetBrowser(PanelWidth);  // Week_Final_5에 없음

    // Display Options 섹션
    RenderDisplayOptions(PanelWidth);

    float totalHeight = ImGui::GetContentRegionAvail().y;
    float splitterHeight = 4.0f;
    float treeHeight = (totalHeight - splitterHeight) * LeftPanelSplitRatio;
    float graphHeight = (totalHeight - splitterHeight) * (1.0f - LeftPanelSplitRatio);

    RenderSkeletonTreePanel(PanelWidth, treeHeight);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
    ImGui::Button("##LeftPanelSplitter", ImVec2(PanelWidth - 16, splitterHeight));
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (ImGui::IsItemActive())
    {
        float delta = ImGui::GetIO().MouseDelta.y;
        if (delta != 0.0f)
        {
            float newRatio = LeftPanelSplitRatio + delta / totalHeight;
            LeftPanelSplitRatio = std::clamp(newRatio, 0.2f, 0.8f);
        }
    }

    RenderGraphViewPanel(PanelWidth, graphHeight);
}

void SPhysicsAssetEditorWindow::RenderRightPanel()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;

    ImGui::Text("Details");
    ImGui::Separator();
    ImGui::Spacing();

    if (PhysState->SelectedBodyIndex >= 0 && PhysState->EditingAsset)
    {
        if (PhysState->SelectedBodyIndex < PhysState->EditingAsset->Bodies.Num())
        {
            UBodySetup* Body = PhysState->EditingAsset->Bodies[PhysState->SelectedBodyIndex];
            if (Body) RenderBodyDetails(Body);
        }
    }
    else if (PhysState->SelectedConstraintIndex >= 0 && PhysState->EditingAsset)
    {
        if (PhysState->SelectedConstraintIndex < PhysState->EditingAsset->Constraints.Num())
        {
            FConstraintInstance* Constraint = &PhysState->EditingAsset->Constraints[PhysState->SelectedConstraintIndex];
            RenderConstraintDetails(Constraint);
        }
    }
    else
    {
        ImGui::TextDisabled("Select a body or constraint to edit");
    }

    // === Tools 패널 (하단) ===
    ImGui::Spacing();
    ImGui::Spacing();
    RenderToolsPanel();
}

void SPhysicsAssetEditorWindow::RenderBottomPanel()
{
}

void SPhysicsAssetEditorWindow::RenderTabsAndToolbar(EViewerType CurrentViewerType)
{
    // ============================================
    // 1. 탭바 렌더링 (뷰어 전환 버튼 제외)
    // ============================================
    if (!ImGui::BeginTabBar("PhysicsAssetEditorTabs",
        ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_Reorderable))
        return;

    // 탭 렌더링
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        PhysicsAssetEditorState* PState = static_cast<PhysicsAssetEditorState*>(State);
        bool open = true;

        // 동적 탭 이름 생성
        FString TabDisplayName;
        if (PState->CurrentFilePath.empty())
        {
            TabDisplayName = "Untitled";
        }
        else
        {
            // 파일 경로에서 파일명만 추출 (확장자 제외)
            size_t lastSlash = PState->CurrentFilePath.find_last_of("/\\");
            FString filename = (lastSlash != FString::npos)
                ? PState->CurrentFilePath.substr(lastSlash + 1)
                : PState->CurrentFilePath;

            // 확장자 제거
            size_t dotPos = filename.find_last_of('.');
            if (dotPos != FString::npos)
                filename = filename.substr(0, dotPos);

            TabDisplayName = filename;
        }

        // 수정됨 표시 추가
        if (PState->bIsDirty)
        {
            TabDisplayName += "*";
        }

        // ImGui ID 충돌 방지
        TabDisplayName += "##";
        TabDisplayName += State->Name.ToString();

        if (ImGui::BeginTabItem(TabDisplayName.c_str(), &open))
        {
            ActiveTabIndex = i;
            ActiveState = State;
            ImGui::EndTabItem();
        }
        if (!open)
        {
            CloseTab(i);
            ImGui::EndTabBar();
            return;
        }
    }

    // + 버튼 (새 탭 추가)
    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing))
    {
        int maxViewerNum = 0;
        for (int i = 0; i < Tabs.Num(); ++i)
        {
            const FString& tabName = Tabs[i]->Name.ToString();
            const char* prefix = "PhysicsTab";
            if (strncmp(tabName.c_str(), prefix, strlen(prefix)) == 0)
            {
                const char* numberPart = tabName.c_str() + strlen(prefix);
                int num = atoi(numberPart);
                if (num > maxViewerNum)
                {
                    maxViewerNum = num;
                }
            }
        }

        char label[64];
        sprintf_s(label, "PhysicsTab%d", maxViewerNum + 1);
        OpenNewTab(label);
    }
    ImGui::EndTabBar();

    // ============================================
    // 2. 툴바 렌더링 (Save, SaveAs, Load 버튼)
    // ============================================
    ImGui::BeginChild("PhysicsToolbar", ImVec2(0, 30.0f), false, ImGuiWindowFlags_NoScrollbar);

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImVec2 IconSizeVec(22, 22);
    float BtnHeight = IconSizeVec.y + style.FramePadding.y * 2.0f;
    float availableHeight = ImGui::GetContentRegionAvail().y;
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (availableHeight - BtnHeight) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

    // Save 버튼
    if (IconSave && IconSave->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##SavePhysics", (void*)IconSave->GetShaderResourceView(), IconSizeVec))
        {
            SavePhysicsAsset();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Physics Asset 저장 (Ctrl+S)");
    }
    ImGui::SameLine();

    // SaveAs 버튼
    if (IconSaveAs && IconSaveAs->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##SaveAsPhysics", (void*)IconSaveAs->GetShaderResourceView(), IconSizeVec))
        {
            SavePhysicsAssetAs();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("다른 이름으로 저장");
    }
    ImGui::SameLine();

    // Load 버튼
    if (IconLoad && IconLoad->GetShaderResourceView())
    {
        if (ImGui::ImageButton("##LoadPhysics", (void*)IconLoad->GetShaderResourceView(), IconSizeVec))
        {
            LoadPhysicsAsset();
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Physics Asset 불러오기");
    }

    ImGui::PopStyleColor();
    ImGui::EndChild();
}

PhysicsAssetEditorState* SPhysicsAssetEditorWindow::GetPhysicsState() const
{
    return static_cast<PhysicsAssetEditorState*>(ActiveState);
}

void SPhysicsAssetEditorWindow::RenderSkeletonTreePanel(float Width, float Height)
{
    ImGui::BeginChild("SkeletonTree", ImVec2(Width - 16, Height), false);

    // 헤더: Skeleton 텍스트 + 톱니바퀴 설정 버튼
    float headerWidth = ImGui::GetContentRegionAvail().x;
    ImGui::Text("Skeleton");
    ImGui::SameLine(headerWidth - 20);

    // 톱니바퀴 버튼
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    if (ImGui::Button("##TreeSettings", ImVec2(20, 20)))
    {
        ImGui::OpenPopup("SkeletonTreeSettingsPopup");
    }
    ImGui::PopStyleColor(2);

    // 톱니바퀴 아이콘 그리기 (간단한 텍스트로 대체)
    ImVec2 btnMin = ImGui::GetItemRectMin();
    ImVec2 btnMax = ImGui::GetItemRectMax();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center((btnMin.x + btnMax.x) * 0.5f, (btnMin.y + btnMax.y) * 0.5f);
    drawList->AddText(ImVec2(center.x - 4, center.y - 7), IM_COL32(200, 200, 200, 255), "*");

    // 설정 팝업 메뉴
    RenderSkeletonTreeSettings();

    ImGui::Separator();

    if (!ActiveState || !ActiveState->CurrentMesh)
    {
        ImGui::TextDisabled("Load a skeletal mesh first");
        ImGui::EndChild();
        return;
    }

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.empty())
    {
        ImGui::TextDisabled("No skeleton data");
        ImGui::EndChild();
        return;
    }

    // 본 숨김 모드: Body만 트리 구조로 표시
    if (TreeSettings.BoneDisplayMode == EBoneDisplayMode::HideBones)
    {
        PhysicsAssetEditorState* PhysState = GetPhysicsState();
        if (PhysState && PhysState->EditingAsset)
        {
            // 루트 Body 찾기 (부모 본에 Body가 없는 Body들)
            for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
            {
                UBodySetup* Body = PhysState->EditingAsset->Bodies[i];
                if (!Body) continue;

                // 이 Body의 본 인덱스 찾기
                int32 BoneIndex = -1;
                for (int32 j = 0; j < (int32)Skeleton->Bones.size(); ++j)
                {
                    if (Skeleton->Bones[j].Name == Body->BoneName.ToString())
                    {
                        BoneIndex = j;
                        break;
                    }
                }

                if (BoneIndex < 0) continue;

                // 부모 본 체인에서 Body가 있는지 확인
                bool bHasParentBody = false;
                int32 ParentBoneIndex = Skeleton->Bones[BoneIndex].ParentIndex;
                while (ParentBoneIndex >= 0)
                {
                    const FString& ParentBoneName = Skeleton->Bones[ParentBoneIndex].Name;
                    for (int32 k = 0; k < PhysState->EditingAsset->Bodies.Num(); ++k)
                    {
                        if (PhysState->EditingAsset->Bodies[k] &&
                            PhysState->EditingAsset->Bodies[k]->BoneName.ToString() == ParentBoneName)
                        {
                            bHasParentBody = true;
                            break;
                        }
                    }
                    if (bHasParentBody) break;
                    ParentBoneIndex = Skeleton->Bones[ParentBoneIndex].ParentIndex;
                }

                // 부모 Body가 없으면 루트로서 렌더링
                if (!bHasParentBody)
                {
                    RenderBodyTreeNode(i, Skeleton);
                }
            }
        }
    }
    else
    {
        // 일반 모드: 스켈레톤 트리 표시
        for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].ParentIndex == -1)
            {
                RenderBoneTreeNode(i, 0);
            }
        }
    }

    ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderSkeletonTreeSettings()
{
    if (ImGui::BeginPopup("SkeletonTreeSettingsPopup"))
    {
        // 라디오 버튼 스타일: 하나만 선택 가능
        bool bAllBones = (TreeSettings.BoneDisplayMode == EBoneDisplayMode::AllBones);
        bool bMeshBones = (TreeSettings.BoneDisplayMode == EBoneDisplayMode::MeshBones);
        bool bHideBones = (TreeSettings.BoneDisplayMode == EBoneDisplayMode::HideBones);

        if (ImGui::RadioButton("Show All Bones", bAllBones))
        {
            TreeSettings.BoneDisplayMode = EBoneDisplayMode::AllBones;
        }

        if (ImGui::RadioButton("Show Mesh Bones", bMeshBones))
        {
            TreeSettings.BoneDisplayMode = EBoneDisplayMode::MeshBones;
        }

        if (ImGui::RadioButton("Hide Bones", bHideBones))
        {
            TreeSettings.BoneDisplayMode = EBoneDisplayMode::HideBones;
        }

        ImGui::Separator();

        // 컨스트레인트 표시 체크박스
        ImGui::Checkbox("Show Constraints", &TreeSettings.bShowConstraintsInTree);

        ImGui::EndPopup();
    }
}

void SPhysicsAssetEditorWindow::RenderDisplayOptions(float PanelWidth)
{
    if (!ActiveState) return;

    ImGui::Text(reinterpret_cast<const char*>(u8"디스플레이 옵션"));
    ImGui::Dummy(ImVec2(0, 2));

    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.5f, 1.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.23f, 0.25f, 0.27f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.28f, 0.30f, 0.33f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.22f, 0.25f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.75f, 0.80f, 0.90f, 1.00f));

    // 메쉬 체크박스
    if (ImGui::Checkbox(reinterpret_cast<const char*>(u8"메쉬"), &ActiveState->bShowMesh))
    {
        if (ActiveState->PreviewActor)
        {
            if (auto* comp = ActiveState->PreviewActor->GetSkeletalMeshComponent())
                comp->SetVisibility(ActiveState->bShowMesh);
        }
    }

    ImGui::SameLine(0.0f, 12.0f);

    // 스켈레탈 체크박스
    if (ImGui::Checkbox(reinterpret_cast<const char*>(u8"스켈레탈"), &ActiveState->bShowBones))
    {
        if (ActiveState->PreviewActor)
        {
            if (auto* lineComp = ActiveState->PreviewActor->GetBoneLineComponent())
                lineComp->SetLineVisible(ActiveState->bShowBones);
        }
        if (ActiveState->bShowBones)
            ActiveState->bBoneLinesDirty = true;
    }

    ImGui::SameLine(0.0f, 12.0f);

    // 래그돌 체크박스
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (PhysState && PhysState->World)
    {
        bool bShowRagdoll = PhysState->World->GetRenderSettings().IsShowFlagEnabled(EEngineShowFlags::SF_Ragdoll);
        if (ImGui::Checkbox(reinterpret_cast<const char*>(u8"래그돌"), &bShowRagdoll))
        {
            if (bShowRagdoll)
                PhysState->World->GetRenderSettings().EnableShowFlag(EEngineShowFlags::SF_Ragdoll);
            else
                PhysState->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_Ragdoll);
        }
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    // 두 번째 줄: 바디, 컨스트레인트
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1.5f, 1.5f));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.23f, 0.25f, 0.27f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.28f, 0.30f, 0.33f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.20f, 0.22f, 0.25f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.75f, 0.80f, 0.90f, 1.00f));

    // 바디 체크박스
    if (PhysState)
    {
        if (ImGui::Checkbox(reinterpret_cast<const char*>(u8"바디"), &PhysState->bShowBodies))
        {
            // 체크 상태 변경 시 추가 처리 필요 없음 (RenderPhysicsBodies에서 체크)
        }
    }

    ImGui::SameLine(0.0f, 12.0f);

    // 컨스트레인트 체크박스
    if (PhysState)
    {
        if (ImGui::Checkbox(reinterpret_cast<const char*>(u8"컨스트레인트"), &PhysState->bShowConstraints))
        {
            // 체크 상태 변경 시 추가 처리 필요 없음 (RenderConstraintVisuals에서 체크)
        }
    }

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    ImGui::Dummy(ImVec2(0, 4));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4));
}

void SPhysicsAssetEditorWindow::SelectBone(int32 BoneIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size()) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;

    // 본 인덱스 저장
    ActiveState->SelectedBoneIndex = BoneIndex;

    // Body/Shape/Constraint 선택 해제
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedConstraintIndex = -1;

    // Shape 기즈모 숨기기
    if (PhysState->ShapeGizmoAnchor)
    {
        PhysState->ShapeGizmoAnchor->ClearTarget();
        PhysState->ShapeGizmoAnchor->SetVisibility(false);
        PhysState->ShapeGizmoAnchor->SetEditability(false);
    }

    // Constraint 기즈모 숨기기
    if (PhysState->ConstraintGizmoAnchor)
    {
        PhysState->ConstraintGizmoAnchor->ClearTarget();
        PhysState->ConstraintGizmoAnchor->SetVisibility(false);
        PhysState->ConstraintGizmoAnchor->SetEditability(false);
    }

    // 본 라인 하이라이트 업데이트
    ActiveState->bBoneLinesDirty = true;

    // BlendSpaceEditor 패턴 사용: RepositionAnchorToBone + GetBoneGizmoAnchor
    PreviewActor->RepositionAnchorToBone(BoneIndex);
    if (USceneComponent* Anchor = PreviewActor->GetBoneGizmoAnchor())
    {
        PhysState->World->GetSelectionManager()->SelectActor(PreviewActor);
        PhysState->World->GetSelectionManager()->SelectComponent(Anchor);
    }
}

void SPhysicsAssetEditorWindow::RenderBoneTreeNode(int32 BoneIndex, int32 Depth)
{
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size()) return;

    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    const FBone& Bone = Skeleton->Bones[BoneIndex];

    TArray<int32> ChildIndices;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].ParentIndex == BoneIndex)
            ChildIndices.Add(i);
    }

    bool bHasBody = false;
    int32 BodyIndex = -1;
    UBodySetup* BodySetup = nullptr;
    if (PhysState && PhysState->EditingAsset)
    {
        for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
        {
            if (PhysState->EditingAsset->Bodies[i] &&
                PhysState->EditingAsset->Bodies[i]->BoneName == Bone.Name)
            {
                bHasBody = true;
                BodyIndex = i;
                BodySetup = PhysState->EditingAsset->Bodies[i];
                break;
            }
        }
    }

    // Body가 있거나 자식이 있으면 Leaf가 아님
    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    if (ChildIndices.Num() == 0 && !bHasBody)
        nodeFlags |= ImGuiTreeNodeFlags_Leaf;

    // Bone.Name is already FString
    const FString& BoneName = Bone.Name;

    bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)BoneIndex, nodeFlags, "%s", BoneName.c_str());

    if (ImGui::IsItemClicked())
    {
        // 본 클릭 시 기즈모 표시
        SelectBone(BoneIndex);
    }

    // 우클릭 컨텍스트 메뉴
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ContextMenuBoneIndex = BoneIndex;
        ImGui::OpenPopup("BoneContextMenu");
    }

    // 컨텍스트 메뉴 렌더링 (이 노드에서 열린 경우에만)
    if (ContextMenuBoneIndex == BoneIndex)
    {
        RenderBoneContextMenu(BoneIndex, bHasBody, BodyIndex);
    }

    if (bOpen)
    {
        // Body가 있으면 하위에 BodySetup 노드 표시
        if (bHasBody && BodySetup)
        {
            // 이 Body에 연결된 Constraint 개수 확인
            int32 ConstraintCount = 0;
            if (TreeSettings.bShowConstraintsInTree && PhysState && PhysState->EditingAsset)
            {
                for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
                {
                    if (PhysState->EditingAsset->Constraints[i].ConstraintBone2 == BodySetup->BoneName)
                        ConstraintCount++;
                }
            }

            bool bBodySelected = (PhysState && PhysState->SelectedBodyIndex == BodyIndex);
            ImGuiTreeNodeFlags bodyFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
            // Constraint가 없으면 Leaf 노드
            if (ConstraintCount == 0)
                bodyFlags |= ImGuiTreeNodeFlags_Leaf;
            if (bBodySelected)
                bodyFlags |= ImGuiTreeNodeFlags_Selected;

            // 아이콘 + 본 이름으로 BodySetup 표시
            FString bodyLabel = FString("[Body] ") + BoneName;

            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 1.0f, 1.0f)); // 파란색 텍스트
            bool bBodyOpen = ImGui::TreeNodeEx((void*)(intptr_t)(10000 + BodyIndex), bodyFlags, "%s", bodyLabel.c_str());
            ImGui::PopStyleColor();

            if (ImGui::IsItemClicked())
            {
                SelectBody(BodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
            }

            // Body 노드가 열려있으면 Constraint를 자식으로 표시
            if (bBodyOpen)
            {
                // 이 Body에 연결된 Constraint 표시 (설정에서 활성화된 경우)
                // 언리얼 스타일: Bone2(자식) 아래에 Constraint 표시
                if (TreeSettings.bShowConstraintsInTree && PhysState && PhysState->EditingAsset)
                {
                    for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
                    {
                        FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];
                        // Bone2(자식)가 현재 Body인 Constraint만 표시
                        if (Constraint.ConstraintBone2 == BodySetup->BoneName)
                        {
                            RenderConstraintTreeNode(i, BodySetup->BoneName);
                        }
                    }
                }
                ImGui::TreePop();
            }
        }

        // 자식 본 렌더링
        for (int32 ChildIndex : ChildIndices)
            RenderBoneTreeNode(ChildIndex, Depth + 1);

        ImGui::TreePop();
    }
}

void SPhysicsAssetEditorWindow::RenderBodyTreeNode(int32 BodyIndex, const FSkeleton* Skeleton)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset || !Skeleton) return;
    if (BodyIndex < 0 || BodyIndex >= PhysState->EditingAsset->Bodies.Num()) return;

    UBodySetup* Body = PhysState->EditingAsset->Bodies[BodyIndex];
    if (!Body) return;

    // 이 Body의 본 인덱스 찾기
    int32 BoneIndex = -1;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].Name == Body->BoneName.ToString())
        {
            BoneIndex = i;
            break;
        }
    }

    if (BoneIndex < 0) return;

    // 자식 Body 찾기 (이 본의 자손 본들 중 Body가 있는 것)
    TArray<int32> ChildBodyIndices;
    for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
    {
        if (i == BodyIndex) continue;
        UBodySetup* OtherBody = PhysState->EditingAsset->Bodies[i];
        if (!OtherBody) continue;

        // OtherBody의 본 인덱스 찾기
        int32 OtherBoneIndex = -1;
        for (int32 j = 0; j < (int32)Skeleton->Bones.size(); ++j)
        {
            if (Skeleton->Bones[j].Name == OtherBody->BoneName.ToString())
            {
                OtherBoneIndex = j;
                break;
            }
        }

        if (OtherBoneIndex < 0) continue;

        // OtherBody의 부모 체인에서 이 Body가 바로 위 Body인지 확인
        int32 ParentIdx = Skeleton->Bones[OtherBoneIndex].ParentIndex;
        bool bIsDirectChild = false;
        while (ParentIdx >= 0)
        {
            // 이 부모 본에 Body가 있는지 확인
            for (int32 k = 0; k < PhysState->EditingAsset->Bodies.Num(); ++k)
            {
                if (PhysState->EditingAsset->Bodies[k] &&
                    PhysState->EditingAsset->Bodies[k]->BoneName.ToString() == Skeleton->Bones[ParentIdx].Name)
                {
                    // 찾은 Body가 현재 Body면 직접 자식
                    if (k == BodyIndex)
                    {
                        bIsDirectChild = true;
                    }
                    // 다른 Body면 직접 자식이 아님 (중간에 다른 Body가 있음)
                    goto done_search;
                }
            }
            ParentIdx = Skeleton->Bones[ParentIdx].ParentIndex;
        }
    done_search:
        if (bIsDirectChild)
        {
            ChildBodyIndices.Add(i);
        }
    }

    // 이 Body에 연결된 Constraint 개수 확인
    int32 ConstraintCount = 0;
    if (TreeSettings.bShowConstraintsInTree)
    {
        for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
        {
            if (PhysState->EditingAsset->Constraints[i].ConstraintBone2 == Body->BoneName)
                ConstraintCount++;
        }
    }

    // 트리 노드 렌더링
    bool bSelected = (PhysState->SelectedBodyIndex == BodyIndex);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
    // 자식 Body도 없고 Constraint도 없으면 Leaf
    if (ChildBodyIndices.Num() == 0 && ConstraintCount == 0)
        flags |= ImGuiTreeNodeFlags_Leaf;
    if (bSelected)
        flags |= ImGuiTreeNodeFlags_Selected;

    FString label = Body->BoneName.ToString();

    bool bOpen = ImGui::TreeNodeEx((void*)(intptr_t)(20000 + BodyIndex), flags, "%s", label.c_str());

    if (ImGui::IsItemClicked())
    {
        SelectBody(BodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
    }

    if (bOpen)
    {
        // 이 Body에 연결된 Constraint 표시 (설정에서 활성화된 경우)
        // 언리얼 스타일: Bone2(자식) 아래에 Constraint 표시
        if (TreeSettings.bShowConstraintsInTree)
        {
            for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
            {
                FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];
                // Bone2(자식)가 현재 Body인 Constraint만 표시
                if (Constraint.ConstraintBone2 == Body->BoneName)
                {
                    RenderConstraintTreeNode(i, Body->BoneName);
                }
            }
        }

        // 자식 Body 렌더링
        for (int32 ChildBodyIndex : ChildBodyIndices)
        {
            RenderBodyTreeNode(ChildBodyIndex, Skeleton);
        }
        ImGui::TreePop();
    }
}

void SPhysicsAssetEditorWindow::RenderGraphViewPanel(float Width, float Height)
{
    ImGui::BeginChild("GraphView", ImVec2(Width - 16, Height), false);
    ImGui::Text("Graph");
    ImGui::Separator();

    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset)
    {
        ImGui::TextDisabled("No physics asset");
        ImGui::EndChild();
        return;
    }

    if (!ActiveState || !ActiveState->CurrentMesh)
    {
        ImGui::TextDisabled("No mesh loaded");
        ImGui::EndChild();
        return;
    }

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton)
    {
        ImGui::TextDisabled("No skeleton");
        ImGui::EndChild();
        return;
    }

    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    // 폰트 스케일을 줌에 맞게 조절 (언리얼 스타일: 노드와 텍스트가 함께 스케일링)
    const float fontScale = PhysState->GraphZoom;
    ImGui::SetWindowFontScale(fontScale);

    // 배경
    DrawList->AddRectFilled(canvasPos,
        ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
        IM_COL32(30, 30, 30, 255));

    // 그리드
    float gridSize = 80.0f * PhysState->GraphZoom;
    for (float x = fmodf(PhysState->GraphOffset.X, gridSize); x < canvasSize.x; x += gridSize)
    {
        DrawList->AddLine(ImVec2(canvasPos.x + x, canvasPos.y),
            ImVec2(canvasPos.x + x, canvasPos.y + canvasSize.y), IM_COL32(50, 50, 50, 255));
    }
    for (float y = fmodf(PhysState->GraphOffset.Y, gridSize); y < canvasSize.y; y += gridSize)
    {
        DrawList->AddLine(ImVec2(canvasPos.x, canvasPos.y + y),
            ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + y), IM_COL32(50, 50, 50, 255));
    }

    // 본 깊이 계산 헬퍼 (스켈레톤 계층에서 루트로부터의 거리)
    auto GetBoneDepth = [Skeleton](const FName& BoneName) -> int32
    {
        auto it = Skeleton->BoneNameToIndex.find(BoneName.ToString());
        if (it == Skeleton->BoneNameToIndex.end()) return -1;

        int32 boneIdx = it->second;
        int32 depth = 0;
        while (boneIdx >= 0)
        {
            boneIdx = Skeleton->Bones[boneIdx].ParentIndex;
            depth++;
        }
        return depth;
    };

    // GraphFocusBodyIndex 갱신: 트리/뷰포트에서 선택한 경우에만 갱신
    // 그래프에서 노드 클릭 시에는 갱신하지 않음 (SelectionSource로 구분)
    if (PhysState->SelectionSource == PhysicsAssetEditorState::ESelectionSource::TreeOrViewport)
    {
        if (PhysState->SelectedBodyIndex >= 0 && PhysState->SelectedBodyIndex < PhysState->EditingAsset->Bodies.Num())
        {
            // Body 선택 시: 해당 Body를 그래프 포커스로
            PhysState->GraphFocusBodyIndex = PhysState->SelectedBodyIndex;
        }
        else if (PhysState->SelectedConstraintIndex >= 0 && PhysState->SelectedConstraintIndex < PhysState->EditingAsset->Constraints.Num())
        {
            // Constraint 선택 시: 스켈레톤 계층에서 더 상위(Root에 가까운) Body를 그래프 포커스로
            FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[PhysState->SelectedConstraintIndex];

            // Bone1, Bone2에 해당하는 Body 인덱스 찾기
            int32 Body1Idx = -1, Body2Idx = -1;
            for (int32 j = 0; j < PhysState->EditingAsset->Bodies.Num(); ++j)
            {
                if (PhysState->EditingAsset->Bodies[j]->BoneName == Constraint.ConstraintBone1)
                    Body1Idx = j;
                if (PhysState->EditingAsset->Bodies[j]->BoneName == Constraint.ConstraintBone2)
                    Body2Idx = j;
            }

            // 스켈레톤 계층에서 더 상위(깊이가 작은)인 Body 선택
            if (Body1Idx >= 0 && Body2Idx >= 0)
            {
                int32 depth1 = GetBoneDepth(Constraint.ConstraintBone1);
                int32 depth2 = GetBoneDepth(Constraint.ConstraintBone2);

                // 깊이가 작을수록 Root에 가까움 (상위)
                PhysState->GraphFocusBodyIndex = (depth1 <= depth2) ? Body1Idx : Body2Idx;
            }
            else if (Body1Idx >= 0)
            {
                PhysState->GraphFocusBodyIndex = Body1Idx;
            }
            else if (Body2Idx >= 0)
            {
                PhysState->GraphFocusBodyIndex = Body2Idx;
            }
        }
    }

    // GraphFocusBodyIndex가 유효하지 않으면 메시지 표시
    if (PhysState->GraphFocusBodyIndex < 0 || PhysState->GraphFocusBodyIndex >= PhysState->EditingAsset->Bodies.Num())
    {
        ImGui::Dummy(ImVec2(10, 30));
        ImGui::TextDisabled("Select a body or constraint from tree");
        ImGui::EndChild();
        return;
    }

    // 그래프는 항상 GraphFocusBodyIndex 기준으로 렌더링
    UBodySetup* FocusBody = PhysState->EditingAsset->Bodies[PhysState->GraphFocusBodyIndex];
    FName FocusBoneName = FocusBody->BoneName;

    // 포커스 Body와 연결된 모든 Constraint 및 이웃 Body 찾기
    struct FNeighborInfo
    {
        int32 ConstraintIndex;
        int32 NeighborBodyIndex;  // Constraint 건너편 Body
    };
    TArray<FNeighborInfo> Neighbors;

    for (int32 i = 0; i < PhysState->EditingAsset->Constraints.Num(); ++i)
    {
        FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];

        // 포커스 Body가 이 Constraint의 Bone1 또는 Bone2인지 확인
        bool bIsBone1 = (Constraint.ConstraintBone1 == FocusBoneName);
        bool bIsBone2 = (Constraint.ConstraintBone2 == FocusBoneName);

        if (bIsBone1 || bIsBone2)
        {
            // 건너편 본 이름 찾기
            FName NeighborBoneName = bIsBone1 ? Constraint.ConstraintBone2 : Constraint.ConstraintBone1;

            // 건너편 Body 인덱스 찾기
            int32 NeighborBodyIdx = -1;
            for (int32 j = 0; j < PhysState->EditingAsset->Bodies.Num(); ++j)
            {
                if (PhysState->EditingAsset->Bodies[j]->BoneName == NeighborBoneName)
                {
                    NeighborBodyIdx = j;
                    break;
                }
            }

            if (NeighborBodyIdx >= 0)
            {
                FNeighborInfo Info;
                Info.ConstraintIndex = i;
                Info.NeighborBodyIndex = NeighborBodyIdx;
                Neighbors.Add(Info);
            }
        }
    }

    if (Neighbors.Num() == 0)
    {
        ImGui::Dummy(ImVec2(10, 30));
        ImGui::TextDisabled("No constraints connected");
        ImGui::EndChild();
        return;
    }

    // 본 이름에서 prefix 제거 헬퍼 (예: "mixamorig6:Hips" -> "Hips")
    auto StripBonePrefix = [](const FString& boneName) -> FString
    {
        size_t colonPos = boneName.find(':');
        if (colonPos != std::string::npos && colonPos + 1 < boneName.length())
            return boneName.substr(colonPos + 1);
        return boneName;
    };

    // 노드 크기와 간격 모두 줌에 비례 (언리얼 스타일: 비율 유지)
    const float NodeWidth = 100.0f * PhysState->GraphZoom;
    const float NodeHeight = 65.0f * PhysState->GraphZoom;
    const float ConstraintNodeWidth = 160.0f * PhysState->GraphZoom;
    const float ConstraintNodeHeight = 45.0f * PhysState->GraphZoom;
    const float HorizontalSpacing = 170.0f * PhysState->GraphZoom;
    const float VerticalSpacing = 85.0f * PhysState->GraphZoom;

    // 색상 정의 (언리얼 스타일)
    const ImU32 BodyColor = IM_COL32(160, 200, 130, 255);           // 연두색 (Body)
    const ImU32 BodySelectedColor = IM_COL32(180, 220, 150, 255);   // 밝은 연두색 (선택된 Body)
    const ImU32 ConstraintColor = IM_COL32(220, 180, 100, 255);     // 노란색/주황색 (Constraint)
    const ImU32 ConstraintSelectedColor = IM_COL32(240, 200, 120, 255); // 밝은 노란색 (선택된 Constraint)
    const ImU32 LineColor = IM_COL32(150, 150, 150, 255);           // 연결선
    const ImU32 TextColor = IM_COL32(40, 40, 40, 255);              // 검은색 글씨 (통일)

    // 레이아웃: 왼쪽(선택된 Body 1개) - 가운데(Constraint들) - 오른쪽(이웃 Body들)
    int32 numNeighbors = Neighbors.Num();
    float totalHeight = (numNeighbors - 1) * VerticalSpacing;
    float centerY = canvasPos.y + canvasSize.y * 0.5f + PhysState->GraphOffset.Y;
    float startY = centerY - totalHeight * 0.5f;

    // X 위치
    float leftX = canvasPos.x + canvasSize.x * 0.5f + PhysState->GraphOffset.X - HorizontalSpacing;
    float centerX = canvasPos.x + canvasSize.x * 0.5f + PhysState->GraphOffset.X;
    float rightX = canvasPos.x + canvasSize.x * 0.5f + PhysState->GraphOffset.X + HorizontalSpacing;

    // 선택된 Body Y 위치 (세로 중앙)
    float selectedBodyY = centerY;

    // ========== 연결선 그리기 (노드 뒤에) ==========
    for (int32 i = 0; i < numNeighbors; ++i)
    {
        float rowY = startY + i * VerticalSpacing;

        ImVec2 selectedPos(leftX, selectedBodyY);
        ImVec2 constraintPos(centerX, rowY);
        ImVec2 neighborPos(rightX, rowY);

        // 선택된 Body → Constraint
        DrawList->AddLine(
            ImVec2(selectedPos.x + NodeWidth * 0.5f, selectedPos.y),
            ImVec2(constraintPos.x - ConstraintNodeWidth * 0.5f, constraintPos.y),
            LineColor, 2.0f);

        // Constraint → 이웃 Body
        DrawList->AddLine(
            ImVec2(constraintPos.x + ConstraintNodeWidth * 0.5f, constraintPos.y),
            ImVec2(neighborPos.x - NodeWidth * 0.5f, neighborPos.y),
            LineColor, 2.0f);
    }

    // ========== 노드 그리기 ==========

    // 헬퍼 람다: Body 노드 그리기 (언리얼 스타일 - 폰트도 함께 스케일링)
    auto DrawBodyNode = [&](ImVec2 pos, const FString& boneName, int32 bodyIdx, int32 shapeCount, bool bSelected)
    {
        ImVec2 nodeMin(pos.x - NodeWidth * 0.5f, pos.y - NodeHeight * 0.5f);
        ImVec2 nodeMax(pos.x + NodeWidth * 0.5f, pos.y + NodeHeight * 0.5f);

        // 배경색 (연두색)
        ImU32 bgColor = bSelected ? BodySelectedColor : BodyColor;
        DrawList->AddRectFilled(nodeMin, nodeMax, bgColor, 4.0f * PhysState->GraphZoom);
        if (bSelected)
            DrawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 255, 255, 255), 4.0f * PhysState->GraphZoom, 0, 2.0f);

        // 텍스트 (폰트 스케일이 적용되어 노드와 비례함)
        FString displayName = StripBonePrefix(boneName);

        // "바디" 라벨
        const char* bodyLabel = "바디";
        ImVec2 labelSize = ImGui::CalcTextSize(bodyLabel);
        DrawList->AddText(ImVec2(pos.x - labelSize.x * 0.5f, nodeMin.y + 6 * PhysState->GraphZoom), TextColor, bodyLabel);

        // 본 이름
        ImVec2 nameSize = ImGui::CalcTextSize(displayName.c_str());
        DrawList->AddText(ImVec2(pos.x - nameSize.x * 0.5f, pos.y - 5 * PhysState->GraphZoom), TextColor, displayName.c_str());

        // "셰이프 N개"
        char shapeText[32];
        sprintf_s(shapeText, "셰이프 %d개", shapeCount);
        ImVec2 shapeSize = ImGui::CalcTextSize(shapeText);
        DrawList->AddText(ImVec2(pos.x - shapeSize.x * 0.5f, nodeMax.y - 18 * PhysState->GraphZoom), TextColor, shapeText);

        // 클릭 감지
        if (ImGui::IsMouseHoveringRect(nodeMin, nodeMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            SelectBody(bodyIdx, PhysicsAssetEditorState::ESelectionSource::Graph);
        }
    };

    // 헬퍼 람다: Constraint 노드 그리기 (언리얼 스타일 - 폰트도 함께 스케일링)
    // 표시 형식: 자식본:부모본 (스켈레톤 계층 구조 기준, Bone1/Bone2 순서 무관)
    auto DrawConstraintNode = [&](ImVec2 pos, const FString& bone1, const FString& bone2, int32 constraintIdx, bool bSelected)
    {
        ImVec2 nodeMin(pos.x - ConstraintNodeWidth * 0.5f, pos.y - ConstraintNodeHeight * 0.5f);
        ImVec2 nodeMax(pos.x + ConstraintNodeWidth * 0.5f, pos.y + ConstraintNodeHeight * 0.5f);

        // 배경색 (노란색/주황색)
        ImU32 bgColor = bSelected ? ConstraintSelectedColor : ConstraintColor;
        DrawList->AddRectFilled(nodeMin, nodeMax, bgColor, 4.0f * PhysState->GraphZoom);
        if (bSelected)
            DrawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 255, 255, 255), 4.0f * PhysState->GraphZoom, 0, 2.0f);

        // 텍스트 (폰트 스케일이 적용되어 노드와 비례함)
        //FString shortBone1 = StripBonePrefix(bone1);
        //FString shortBone2 = StripBonePrefix(bone2);
        //FString connectionText = shortBone2 + " : " + shortBone1;

        // "컨스트레인트" 라벨
        const char* constraintLabel = "컨스트레인트";
        ImVec2 labelSize = ImGui::CalcTextSize(constraintLabel);
        DrawList->AddText(ImVec2(pos.x - labelSize.x * 0.5f, nodeMin.y + 6 * PhysState->GraphZoom), TextColor, constraintLabel);

        // Child : Parent 표시 (언리얼 규칙: ConstraintBone1=Child, ConstraintBone2=Parent)
        // prefix 제거 후 표시
        FString shortBone1 = StripBonePrefix(bone1);  // Child (ConstraintBone1)
        FString shortBone2 = StripBonePrefix(bone2);  // Parent (ConstraintBone2)
        FString connectionText = shortBone1 + " : " + shortBone2;  // Child : Parent
        ImVec2 connSize = ImGui::CalcTextSize(connectionText.c_str());
        DrawList->AddText(ImVec2(pos.x - connSize.x * 0.5f, nodeMax.y - 18 * PhysState->GraphZoom), TextColor, connectionText.c_str());

        // 클릭 감지
        if (ImGui::IsMouseHoveringRect(nodeMin, nodeMax) && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            SelectConstraint(constraintIdx, PhysicsAssetEditorState::ESelectionSource::Graph);
        }
    };

    // Shape 개수 계산 헬퍼
    auto GetBodyShapeCount = [](UBodySetup* body) -> int32
    {
        if (!body) return 0;
        return body->AggGeom.SphereElems.Num() + body->AggGeom.BoxElems.Num() + body->AggGeom.SphylElems.Num();
    };

    // ========== 포커스 Body 노드 그리기 (왼쪽, 하나만) ==========
    ImVec2 focusPos(leftX, selectedBodyY);
    bool bFocusBodyHighlight = (PhysState->SelectedBodyIndex == PhysState->GraphFocusBodyIndex);
    DrawBodyNode(focusPos, FocusBody->BoneName.ToString(), PhysState->GraphFocusBodyIndex,
        GetBodyShapeCount(FocusBody), bFocusBodyHighlight);

    // ========== Constraint 및 이웃 Body 노드 그리기 ==========
    for (int32 i = 0; i < numNeighbors; ++i)
    {
        const FNeighborInfo& Info = Neighbors[i];
        float rowY = startY + i * VerticalSpacing;

        // Constraint (가운데)
        FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[Info.ConstraintIndex];
        ImVec2 constraintPos(centerX, rowY);
        bool bConstraintSelected = (PhysState->SelectedConstraintIndex == Info.ConstraintIndex);
        DrawConstraintNode(constraintPos, Constraint.ConstraintBone1.ToString(),
            Constraint.ConstraintBone2.ToString(), Info.ConstraintIndex, bConstraintSelected);

        // 이웃 Body (오른쪽)
        UBodySetup* NeighborBody = PhysState->EditingAsset->Bodies[Info.NeighborBodyIndex];
        ImVec2 neighborPos(rightX, rowY);
        bool bNeighborSelected = (PhysState->SelectedBodyIndex == Info.NeighborBodyIndex);
        DrawBodyNode(neighborPos, NeighborBody->BoneName.ToString(), Info.NeighborBodyIndex,
            GetBodyShapeCount(NeighborBody), bNeighborSelected);
    }

    // Ensure valid size for InvisibleButton (패닝/줌용)
    if (canvasSize.x > 1 && canvasSize.y > 1)
    {
        ImGui::SetCursorScreenPos(canvasPos);
        ImGui::InvisibleButton("GraphCanvas", canvasSize, ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonRight);

        bool bIsHovered = ImGui::IsItemHovered();
        bool bIsActive = ImGui::IsItemActive();

        // 패닝: 마우스 휠 버튼 또는 우클릭 드래그
        if (bIsHovered || bIsActive)
        {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            {
                PhysState->GraphOffset.X += ImGui::GetIO().MouseDelta.x;
                PhysState->GraphOffset.Y += ImGui::GetIO().MouseDelta.y;
            }
        }

        // 줌: 마우스 휠
        if (bIsHovered)
        {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
                PhysState->GraphZoom = std::clamp(PhysState->GraphZoom + wheel * 0.1f, 0.1f, 2.0f);
        }
    }

    // 폰트 스케일 복원
    ImGui::SetWindowFontScale(1.0f);

    ImGui::EndChild();
}

void SPhysicsAssetEditorWindow::RenderGraphNode(const FVector2D& Position, const FString& Label, bool bIsBody, bool bSelected, uint32 Color)
{
    ImDrawList* DrawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();

    float nodeWidth = 120.0f;
    float nodeHeight = 40.0f;

    ImVec2 nodeMin(canvasPos.x + Position.X - nodeWidth * 0.5f, canvasPos.y + Position.Y - nodeHeight * 0.5f);
    ImVec2 nodeMax(nodeMin.x + nodeWidth, nodeMin.y + nodeHeight);

    DrawList->AddRectFilled(nodeMin, nodeMax, Color, 4.0f);
    if (bSelected)
        DrawList->AddRect(nodeMin, nodeMax, IM_COL32(255, 255, 255, 255), 4.0f, 0, 2.0f);

    ImVec2 textSize = ImGui::CalcTextSize(Label.c_str());
    ImVec2 textPos(nodeMin.x + (nodeWidth - textSize.x) * 0.5f, nodeMin.y + (nodeHeight - textSize.y) * 0.5f);
    DrawList->AddText(textPos, IM_COL32(255, 255, 255, 255), Label.c_str());
}

void SPhysicsAssetEditorWindow::RenderBodyDetails(UBodySetup* Body)
{
    if (!Body) return;

    PhysicsAssetEditorState* PhysState = GetPhysicsState();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));

    if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        float mass = Body->MassInKg;
        ImGui::Text("Mass (kg)");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##Mass", &mass, 0.1f, 0.0f, 10000.0f, "%.2f"))
        {
            Body->MassInKg = mass;
            if (PhysState) PhysState->bIsDirty = true;
        }

        float linearDamping = Body->LinearDamping;
        ImGui::Text("Linear Damping");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##LinearDamping", &linearDamping, 0.01f, 0.0f, 100.0f, "%.3f"))
        {
            Body->LinearDamping = linearDamping;
            if (PhysState) PhysState->bIsDirty = true;
        }

        float angularDamping = Body->AngularDamping;
        ImGui::Text("Angular Damping");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##AngularDamping", &angularDamping, 0.01f, 0.0f, 100.0f, "%.3f"))
        {
            Body->AngularDamping = angularDamping;
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Unindent();
    }

    // Physical Material 섹션
    if (ImGui::CollapsingHeader("Physical Material", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        // Physical Material 에셋 드롭다운
        ImGui::Text("Physical Material Asset");

        // 에셋 목록 가져오기
        TArray<FString> PhysMatPaths, PhysMatNames;
        PhysicsAssetEditorBootstrap::GetAllPhysicalMaterialPaths(PhysMatPaths, PhysMatNames);

        // 드롭다운 표시용 문자열 배열
        std::vector<const char*> PhysMatItems;
        for (const FString& Name : PhysMatNames)
        {
            PhysMatItems.push_back(Name.c_str());
        }

        // 현재 Body의 PhysMaterial 경로와 매칭되는 인덱스 찾기
        int CurrentPhysMatIndex = 0;  // 기본값: None
        if (Body->PhysMaterial && !Body->PhysMaterialPath.empty())
        {
            for (int32 i = 0; i < PhysMatPaths.Num(); ++i)
            {
                if (PhysMatPaths[i] == Body->PhysMaterialPath)
                {
                    CurrentPhysMatIndex = i;
                    break;
                }
            }
        }

        // Body별 고유 ID를 위해 PushID 사용
        ImGui::PushID(Body);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##PhysMatAsset", &CurrentPhysMatIndex, PhysMatItems.data(), (int)PhysMatItems.size()))
        {
            if (CurrentPhysMatIndex > 0 && CurrentPhysMatIndex < PhysMatPaths.Num())
            {
                // 에셋 로드하여 적용
                UPhysicalMaterial* LoadedMat = PhysicsAssetEditorBootstrap::LoadPhysicalMaterial(PhysMatPaths[CurrentPhysMatIndex]);
                if (LoadedMat)
                {
                    Body->PhysMaterial = LoadedMat;
                    Body->PhysMaterialPath = PhysMatPaths[CurrentPhysMatIndex];  // 경로 저장
                    if (PhysState) PhysState->bIsDirty = true;
                }
            }
            else
            {
                // None 선택 시 PhysMaterial을 nullptr로 설정
                Body->PhysMaterial = nullptr;
                Body->PhysMaterialPath = "";
                if (PhysState) PhysState->bIsDirty = true;
            }
        }
        ImGui::PopID();

        ImGui::Separator();

        // None 선택 시 Friction/Restitution을 0으로 표시 (읽기 전용)
        // 에셋 선택 시 해당 값 표시 (Edit 모드에 따라 활성/비활성)
        bool bHasPhysMaterial = (Body->PhysMaterial != nullptr);
        float friction = bHasPhysMaterial ? Body->PhysMaterial->Friction : 0.0f;
        float restitution = bHasPhysMaterial ? Body->PhysMaterial->Restitution : 0.0f;

        // Edit 모드 상태 관리 (Body 포인터를 키로 사용)
        static std::unordered_map<void*, bool> EditModeMap;
        bool& bEditMode = EditModeMap[Body];

        // Friction (마찰 계수)
        ImGui::Text("Friction");
        ImGui::SetNextItemWidth(-1);
        if (!bHasPhysMaterial)
        {
            ImGui::BeginDisabled();
            ImGui::DragFloat("##Friction", &friction, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::EndDisabled();
        }
        else
        {
            // Edit 모드가 아니면 비활성화
            if (!bEditMode) ImGui::BeginDisabled();
            if (ImGui::DragFloat("##Friction", &friction, 0.01f, 0.0f, 1.0f, "%.2f"))
            {
                Body->PhysMaterial->Friction = friction;
            }
            if (!bEditMode) ImGui::EndDisabled();
        }

        // Restitution (반발 계수)
        ImGui::Text("Restitution");
        ImGui::SetNextItemWidth(-1);
        if (!bHasPhysMaterial)
        {
            ImGui::BeginDisabled();
            ImGui::DragFloat("##Restitution", &restitution, 0.01f, 0.0f, 1.0f, "%.2f");
            ImGui::EndDisabled();
        }
        else
        {
            // Edit 모드가 아니면 비활성화
            if (!bEditMode) ImGui::BeginDisabled();
            if (ImGui::DragFloat("##Restitution", &restitution, 0.01f, 0.0f, 1.0f, "%.2f"))
            {
                Body->PhysMaterial->Restitution = restitution;
            }
            if (!bEditMode) ImGui::EndDisabled();
        }

        // Edit/Save 토글 버튼 (에셋이 선택된 경우만)
        if (bHasPhysMaterial)
        {
            const char* ButtonLabel = bEditMode ? "Save" : "Edit";
            if (ImGui::Button(ButtonLabel, ImVec2(-1, 0)))
            {
                if (bEditMode)
                {
                    // Save 버튼 클릭: 에셋 파일에 저장
                    if (!Body->PhysMaterialPath.empty())
                    {
                        if (PhysicsAssetEditorBootstrap::SavePhysicalMaterial(Body->PhysMaterial, Body->PhysMaterialPath))
                        {
                            UE_LOG("[PhysicsAssetEditor] Physical Material 저장: %s", Body->PhysMaterialPath.c_str());
                        }
                    }
                    bEditMode = false;
                }
                else
                {
                    // Edit 버튼 클릭: 편집 모드 활성화
                    bEditMode = true;
                }
            }
        }

        if (!bHasPhysMaterial)
        {
            ImGui::TextDisabled("Select a Physical Material asset");

            ImGui::Separator();

            // 새 Physical Material 에셋 생성 UI
            ImGui::Text("Create New Asset");

            static char NewPhysMatNameBuffer[256] = "NewPhysicalMaterial";
            static float NewFriction = 0.5f;
            static float NewRestitution = 0.3f;

            ImGui::Text("Name");
            ImGui::SetNextItemWidth(-1);
            ImGui::InputText("##NewPhysMatName", NewPhysMatNameBuffer, sizeof(NewPhysMatNameBuffer));

            ImGui::Text("Friction");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##NewFriction", &NewFriction, 0.01f, 0.0f, 1.0f, "%.2f");

            ImGui::Text("Restitution");
            ImGui::SetNextItemWidth(-1);
            ImGui::DragFloat("##NewRestitution", &NewRestitution, 0.01f, 0.0f, 1.0f, "%.2f");

            if (ImGui::Button("Create & Apply", ImVec2(-1, 0)))
            {
                // 새 PhysMaterial 생성
                UPhysicalMaterial* NewMat = NewObject<UPhysicalMaterial>();
                if (NewMat)
                {
                    NewMat->Friction = NewFriction;
                    NewMat->Restitution = NewRestitution;

                    // 에셋으로 저장
                    FString MaterialsDir = GDataDir + "/Physics/Materials";
                    // 폴더가 없으면 생성
                    std::filesystem::path DirPath(UTF8ToWide(MaterialsDir));
                    if (!std::filesystem::exists(DirPath))
                    {
                        std::filesystem::create_directories(DirPath);
                    }
                    FString SavePath = NormalizePath(MaterialsDir + "/" + NewPhysMatNameBuffer + ".physmat");
                    if (PhysicsAssetEditorBootstrap::SavePhysicalMaterial(NewMat, SavePath))
                    {
                        // Body에 적용
                        Body->PhysMaterial = NewMat;
                        Body->PhysMaterialPath = SavePath;
                        if (PhysState) PhysState->bIsDirty = true;
                        UE_LOG("[PhysicsAssetEditor] Physical Material 생성 및 적용: %s", SavePath.c_str());
                    }
                }
            }
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Body Setup", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        // Bone Name
        ImGui::Text("Bone Name");
        ImGui::TextDisabled("%s", Body->BoneName.ToString().c_str());

        ImGui::Separator();

        // ===== Primitives (트리 구조) =====
        // CollisionEnabled 옵션 배열
        const char* collisionModes[] = { "No Collision", "Query Only", "Physics Only", "Collision Enabled" };

        // ===== Sphere 섹션 =====
        int32 sphereCount = Body->AggGeom.SphereElems.Num();
        ImGui::PushID("Spheres");
        if (ImGui::TreeNode("Sphere"))
        {
            ImGui::SameLine(200);
            ImGui::TextDisabled("Elements: %d", sphereCount);

            for (int32 i = 0; i < sphereCount; ++i)
            {
                FKSphereElem& Sphere = Body->AggGeom.SphereElems[i];
                ImGui::PushID(i);
                if (ImGui::TreeNode("##SphereElem", "Index [%d]", i))
                {
                    ImGui::SameLine(200);
                    ImGui::TextDisabled("%s", Sphere.Name.ToString().c_str());

                    // Center
                    float center[3] = { Sphere.Center.X, Sphere.Center.Y, Sphere.Center.Z };
                    ImGui::Text("Center");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##Center", center, 0.1f))
                    {
                        Sphere.Center = FVector(center[0], center[1], center[2]);
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                        }
                    }

                    // Radius
                    ImGui::Text("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##Radius", &Sphere.Radius, 0.1f, 0.1f, 1000.0f, "%.2f"))
                    {
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                            // Shape 크기 변경 시 부피 기반 질량 재계산
                            UpdateBodyMassFromVolume(Body);
                        }
                    }

                    // Name
                    char nameBuffer[256];
                    strcpy_s(nameBuffer, Sphere.Name.ToString().c_str());
                    ImGui::Text("Name");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
                    {
                        Sphere.Name = FName(nameBuffer);
                        if (PhysState) PhysState->bIsDirty = true;
                    }

                    // CollisionEnabled
                    int collisionMode = static_cast<int>(Sphere.CollisionEnabled);
                    ImGui::Text("Collision Enabled");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##Collision", &collisionMode, collisionModes, 4))
                    {
                        Sphere.CollisionEnabled = static_cast<ECollisionEnabled>(collisionMode);
                        if (PhysState) PhysState->bIsDirty = true;
                    }

                    ImGui::TreePop();
                }
                else
                {
                    ImGui::SameLine(200);
                    ImGui::TextDisabled("%s", Sphere.Name.ToString().c_str());
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        else
        {
            ImGui::SameLine(200);
            ImGui::TextDisabled("Elements: %d", sphereCount);
        }
        ImGui::PopID();

        // ===== Box 섹션 =====
        int32 boxCount = Body->AggGeom.BoxElems.Num();
        ImGui::PushID("Boxes");
        if (ImGui::TreeNode("Box"))
        {
            ImGui::SameLine(200);
            ImGui::TextDisabled("Elements: %d", boxCount);

            for (int32 i = 0; i < boxCount; ++i)
            {
                FKBoxElem& Box = Body->AggGeom.BoxElems[i];
                ImGui::PushID(i);
                if (ImGui::TreeNode("##BoxElem", "Index [%d]", i))
                {
                    ImGui::SameLine(200);
                    ImGui::TextDisabled("%s", Box.Name.ToString().c_str());

                    // Center
                    float center[3] = { Box.Center.X, Box.Center.Y, Box.Center.Z };
                    ImGui::Text("Center");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##Center", center, 0.1f))
                    {
                        Box.Center = FVector(center[0], center[1], center[2]);
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                        }
                    }

                    // Rotation (FQuat -> Euler for UI display)
                    FVector EulerRot = Box.Rotation.ToEulerZYXDeg();
                    float rotation[3] = { EulerRot.X, EulerRot.Y, EulerRot.Z };
                    ImGui::Text("Rotation");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##Rotation", rotation, 0.5f, -180.0f, 180.0f, "%.1f"))
                    {
                        Box.Rotation = FQuat::MakeFromEulerZYX(FVector(rotation[0], rotation[1], rotation[2]));
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                        }
                    }

                    // X Size
                    ImGui::Text("X");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##X", &Box.X, 0.1f, 0.1f, 1000.0f, "%.2f"))
                    {
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                            UpdateBodyMassFromVolume(Body);
                        }
                    }

                    // Y Size
                    ImGui::Text("Y");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##Y", &Box.Y, 0.1f, 0.1f, 1000.0f, "%.2f"))
                    {
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                            UpdateBodyMassFromVolume(Body);
                        }
                    }

                    // Z Size
                    ImGui::Text("Z");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##Z", &Box.Z, 0.1f, 0.1f, 1000.0f, "%.2f"))
                    {
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                            UpdateBodyMassFromVolume(Body);
                        }
                    }

                    // Name
                    char nameBuffer[256];
                    strcpy_s(nameBuffer, Box.Name.ToString().c_str());
                    ImGui::Text("Name");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
                    {
                        Box.Name = FName(nameBuffer);
                        if (PhysState) PhysState->bIsDirty = true;
                    }

                    // CollisionEnabled
                    int collisionMode = static_cast<int>(Box.CollisionEnabled);
                    ImGui::Text("Collision Enabled");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##Collision", &collisionMode, collisionModes, 4))
                    {
                        Box.CollisionEnabled = static_cast<ECollisionEnabled>(collisionMode);
                        if (PhysState) PhysState->bIsDirty = true;
                    }

                    ImGui::TreePop();
                }
                else
                {
                    ImGui::SameLine(200);
                    ImGui::TextDisabled("%s", Box.Name.ToString().c_str());
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        else
        {
            ImGui::SameLine(200);
            ImGui::TextDisabled("Elements: %d", boxCount);
        }
        ImGui::PopID();

        // ===== Capsule 섹션 =====
        int32 capsuleCount = Body->AggGeom.SphylElems.Num();
        ImGui::PushID("Capsules");
        if (ImGui::TreeNode("Capsule"))
        {
            ImGui::SameLine(200);
            ImGui::TextDisabled("Elements: %d", capsuleCount);

            for (int32 i = 0; i < capsuleCount; ++i)
            {
                FKSphylElem& Capsule = Body->AggGeom.SphylElems[i];
                ImGui::PushID(i);
                if (ImGui::TreeNode("##CapsuleElem", "Index [%d]", i))
                {
                    ImGui::SameLine(200);
                    ImGui::TextDisabled("%s", Capsule.Name.ToString().c_str());

                    // Center
                    float center[3] = { Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z };
                    ImGui::Text("Center");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##Center", center, 0.1f))
                    {
                        Capsule.Center = FVector(center[0], center[1], center[2]);
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                        }
                    }

                    // Rotation (FQuat -> Euler for UI display)
                    FVector CapsuleEuler = Capsule.Rotation.ToEulerZYXDeg();
                    float rotation[3] = { CapsuleEuler.X, CapsuleEuler.Y, CapsuleEuler.Z };
                    ImGui::Text("Rotation");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat3("##Rotation", rotation, 0.5f, -180.0f, 180.0f, "%.1f"))
                    {
                        Capsule.Rotation = FQuat::MakeFromEulerZYX(FVector(rotation[0], rotation[1], rotation[2]));
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                        }
                    }

                    // Radius
                    ImGui::Text("Radius");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##Radius", &Capsule.Radius, 0.1f, 0.1f, 1000.0f, "%.2f"))
                    {
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                            UpdateBodyMassFromVolume(Body);
                        }
                    }

                    // Length
                    ImGui::Text("Length");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::DragFloat("##Length", &Capsule.Length, 0.1f, 0.1f, 1000.0f, "%.2f"))
                    {
                        if (PhysState)
                        {
                            PhysState->bIsDirty = true;
                            PhysState->bShapesDirty = true;
                            if (PhysState->ShapeGizmoAnchor) PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
                            UpdateBodyMassFromVolume(Body);
                        }
                    }

                    // Name
                    char nameBuffer[256];
                    strcpy_s(nameBuffer, Capsule.Name.ToString().c_str());
                    ImGui::Text("Name");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::InputText("##Name", nameBuffer, sizeof(nameBuffer)))
                    {
                        Capsule.Name = FName(nameBuffer);
                        if (PhysState) PhysState->bIsDirty = true;
                    }

                    // CollisionEnabled
                    int collisionMode = static_cast<int>(Capsule.CollisionEnabled);
                    ImGui::Text("Collision Enabled");
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##Collision", &collisionMode, collisionModes, 4))
                    {
                        Capsule.CollisionEnabled = static_cast<ECollisionEnabled>(collisionMode);
                        if (PhysState) PhysState->bIsDirty = true;
                    }

                    ImGui::TreePop();
                }
                else
                {
                    ImGui::SameLine(200);
                    ImGui::TextDisabled("%s", Capsule.Name.ToString().c_str());
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }
        else
        {
            ImGui::SameLine(200);
            ImGui::TextDisabled("Elements: %d", capsuleCount);
        }
        ImGui::PopID();

        // ===== Convex 섹션 (읽기 전용 - 개수만 표시) =====
        int32 convexCount = Body->AggGeom.ConvexElems.Num();
        ImGui::Text("Convex");
        ImGui::SameLine(200);
        ImGui::TextDisabled("Elements: %d", convexCount);

        ImGui::Unindent();
    }

    ImGui::PopStyleColor();
}

void SPhysicsAssetEditorWindow::RenderConstraintDetails(FConstraintInstance* Constraint)
{
    if (!Constraint) return;

    PhysicsAssetEditorState* PhysState = GetPhysicsState();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.3f, 0.8f));

    if (ImGui::CollapsingHeader("Constraint", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        ImGui::Text("Bone 1 (Child)");
        ImGui::TextDisabled("%s", Constraint->ConstraintBone1.ToString().c_str());
        ImGui::Text("Bone 2 (Parent)");
        ImGui::TextDisabled("%s", Constraint->ConstraintBone2.ToString().c_str());
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Linear Limits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        const char* motionModes[] = { "Free", "Limited", "Locked" };

        ImGui::Text("X Motion");
        int xMotion = static_cast<int>(Constraint->LinearXMotion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##XMotion", &xMotion, motionModes, 3))
        {
            Constraint->LinearXMotion = static_cast<ELinearConstraintMotion>(xMotion);
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Text("Y Motion");
        int yMotion = static_cast<int>(Constraint->LinearYMotion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##YMotion", &yMotion, motionModes, 3))
        {
            Constraint->LinearYMotion = static_cast<ELinearConstraintMotion>(yMotion);
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Text("Z Motion");
        int zMotion = static_cast<int>(Constraint->LinearZMotion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##ZMotion", &zMotion, motionModes, 3))
        {
            Constraint->LinearZMotion = static_cast<ELinearConstraintMotion>(zMotion);
            if (PhysState) PhysState->bIsDirty = true;
        }

        float limit = Constraint->LinearLimit;
        ImGui::Text("Limit");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::DragFloat("##LinearLimit", &limit, 0.1f, 0.0f, 1000.0f, "%.1f"))
        {
            Constraint->LinearLimit = limit;
            if (PhysState) PhysState->bIsDirty = true;
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Angular Limits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();
        const char* angularMotionModes[] = { "Free", "Limited", "Locked" };

        ImGui::Text("Swing 1 Motion");
        int swing1 = static_cast<int>(Constraint->Swing1Motion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##Swing1Motion", &swing1, angularMotionModes, 3))
        {
            Constraint->Swing1Motion = static_cast<EAngularConstraintMotion>(swing1);
            if (PhysState) PhysState->bIsDirty = true;
        }

        float swing1Limit = Constraint->Swing1LimitAngle;
        ImGui::Text("Swing 1 Limit");
        ImGui::SetNextItemWidth(-1);
        // FREE나 LOCKED 모드일 때는 Limit 값 수정 비활성화
        bool bSwing1LimitDisabled = (Constraint->Swing1Motion != EAngularConstraintMotion::Limited);
        if (bSwing1LimitDisabled)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::DragFloat("##Swing1Limit", &swing1Limit, 0.5f, 0.0f, 180.0f, "%.1f"))
        {
            Constraint->Swing1LimitAngle = swing1Limit;
            if (PhysState) PhysState->bIsDirty = true;
        }
        if (bSwing1LimitDisabled)
        {
            ImGui::EndDisabled();
        }

        ImGui::Text("Swing 2 Motion");
        int swing2 = static_cast<int>(Constraint->Swing2Motion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##Swing2Motion", &swing2, angularMotionModes, 3))
        {
            Constraint->Swing2Motion = static_cast<EAngularConstraintMotion>(swing2);
            if (PhysState) PhysState->bIsDirty = true;
        }

        float swing2Limit = Constraint->Swing2LimitAngle;
        ImGui::Text("Swing 2 Limit");
        ImGui::SetNextItemWidth(-1);
        // FREE나 LOCKED 모드일 때는 Limit 값 수정 비활성화
        bool bSwing2LimitDisabled = (Constraint->Swing2Motion != EAngularConstraintMotion::Limited);
        if (bSwing2LimitDisabled)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::DragFloat("##Swing2Limit", &swing2Limit, 0.5f, 0.0f, 180.0f, "%.1f"))
        {
            Constraint->Swing2LimitAngle = swing2Limit;
            if (PhysState) PhysState->bIsDirty = true;
        }
        if (bSwing2LimitDisabled)
        {
            ImGui::EndDisabled();
        }

        ImGui::Text("Twist Motion");
        int twist = static_cast<int>(Constraint->TwistMotion);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##TwistMotion", &twist, angularMotionModes, 3))
        {
            Constraint->TwistMotion = static_cast<EAngularConstraintMotion>(twist);
            if (PhysState) PhysState->bIsDirty = true;
        }

        float twistLimit = Constraint->TwistLimitAngle;
        ImGui::Text("Twist Limit");
        ImGui::SetNextItemWidth(-1);
        // FREE나 LOCKED 모드일 때는 Limit 값 수정 비활성화
        bool bTwistLimitDisabled = (Constraint->TwistMotion != EAngularConstraintMotion::Limited);
        if (bTwistLimitDisabled)
        {
            ImGui::BeginDisabled();
        }
        if (ImGui::DragFloat("##TwistLimit", &twistLimit, 0.5f, 0.0f, 180.0f, "%.1f"))
        {
            Constraint->TwistLimitAngle = twistLimit;
            if (PhysState) PhysState->bIsDirty = true;
        }
        if (bTwistLimitDisabled)
        {
            ImGui::EndDisabled();
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Indent();

        // Position1 (Child 본 공간에서의 조인트 위치)
        ImGui::Text("Position 1 (Child Frame)");
        float pos1[3] = { Constraint->Position1.X, Constraint->Position1.Y, Constraint->Position1.Z };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputFloat3("##Position1", pos1, "%.3f"))
        {
            Constraint->Position1 = FVector(pos1[0], pos1[1], pos1[2]);
            if (PhysState)
            {
                PhysState->bIsDirty = true;
                PhysState->bConstraintsDirty = true;
            }
        }

        // Rotation1 (Child 본 공간에서의 조인트 회전)
        ImGui::Text("Rotation 1 (Euler)");
        float rot1[3] = { Constraint->Rotation1.X, Constraint->Rotation1.Y, Constraint->Rotation1.Z };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputFloat3("##Rotation1", rot1, "%.1f"))
        {
            Constraint->Rotation1 = FVector(rot1[0], rot1[1], rot1[2]);
            if (PhysState)
            {
                PhysState->bIsDirty = true;
                PhysState->bConstraintsDirty = true;
            }
        }

        ImGui::Separator();

        // Position2 (Parent 본 공간에서의 조인트 위치 - 언리얼 규칙)
        // 숫자 입력만 가능 (드래그 불가)
        ImGui::Text("Position 2 (Parent Frame)");
        float pos2[3] = { Constraint->Position2.X, Constraint->Position2.Y, Constraint->Position2.Z };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputFloat3("##Position2", pos2, "%.3f"))
        {
            Constraint->Position2 = FVector(pos2[0], pos2[1], pos2[2]);
            if (PhysState)
            {
                PhysState->bIsDirty = true;
                PhysState->bConstraintsDirty = true;
            }
        }

        // Rotation2 (Parent 본 공간에서의 조인트 회전 - 언리얼 규칙)
        // 숫자 입력만 가능 (드래그 불가)
        ImGui::Text("Rotation 2 (Euler)");
        float rot2[3] = { Constraint->Rotation2.X, Constraint->Rotation2.Y, Constraint->Rotation2.Z };
        ImGui::SetNextItemWidth(-1);
        if (ImGui::InputFloat3("##Rotation2", rot2, "%.1f"))
        {
            Constraint->Rotation2 = FVector(rot2[0], rot2[1], rot2[2]);
            if (PhysState)
            {
                PhysState->bIsDirty = true;
                PhysState->bConstraintsDirty = true;
            }
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Collision"))
    {
        ImGui::Indent();

        // 인접 본 간 충돌 비활성화
        bool bDisableCollision = Constraint->bDisableCollision;
        if (ImGui::Checkbox("Disable Collision", &bDisableCollision))
        {
            Constraint->bDisableCollision = bDisableCollision;
            if (PhysState) PhysState->bIsDirty = true;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Disable collision between connected bodies");
        }

        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("Angular Motor"))
    {
        ImGui::Indent();

        // 모터 활성화
        bool bMotorEnabled = Constraint->bAngularMotorEnabled;
        if (ImGui::Checkbox("Enable Motor", &bMotorEnabled))
        {
            Constraint->bAngularMotorEnabled = bMotorEnabled;
            if (PhysState) PhysState->bIsDirty = true;
        }

        // 모터가 활성화된 경우에만 설정 표시
        if (Constraint->bAngularMotorEnabled)
        {
            float motorStrength = Constraint->AngularMotorStrength;
            ImGui::Text("Strength");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##MotorStrength", &motorStrength, 1.0f, 0.0f, 10000.0f, "%.1f"))
            {
                Constraint->AngularMotorStrength = motorStrength;
                if (PhysState) PhysState->bIsDirty = true;
            }

            float motorDamping = Constraint->AngularMotorDamping;
            ImGui::Text("Damping");
            ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##MotorDamping", &motorDamping, 0.1f, 0.0f, 1000.0f, "%.2f"))
            {
                Constraint->AngularMotorDamping = motorDamping;
                if (PhysState) PhysState->bIsDirty = true;
            }
        }

        ImGui::Unindent();
    }

    ImGui::PopStyleColor();
}

void SPhysicsAssetEditorWindow::RenderViewportOverlay()
{
    // TODO
}

void SPhysicsAssetEditorWindow::RenderPhysicsBodies()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World) return;
    if (!PhysState->EditingAsset) return;
    if (!PhysState->bShowBodies) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    UWorld* World = PhysState->World;

    // 색상 정의
    const FLinearColor UnselectedColor(0.5f, 0.5f, 0.5f, 0.6f);      // 회색: 선택 안됨
    const FLinearColor SiblingColor(0.6f, 0.4f, 0.8f, 0.7f);         // 보라색: 같은 Body의 다른 Shape
    const FLinearColor SelectedShapeColor(0.3f, 0.7f, 1.0f, 0.8f);   // 파란색: 직접 피킹된 Shape

    // 모든 Body를 순회하며 렌더링
    for (int32 BodyIndex = 0; BodyIndex < PhysState->EditingAsset->Bodies.Num(); ++BodyIndex)
    {
        UBodySetup* Body = PhysState->EditingAsset->Bodies[BodyIndex];
        if (!Body) continue;

        // Body의 BoneName이 유효한지 확인 (dangling 방지)
        if (Body->BoneName.IsNone()) continue;

        // 본 인덱스 찾기
        int32 BoneIndex = -1;
        for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Body->BoneName.ToString())
            {
                BoneIndex = i;
                break;
            }
        }
        if (BoneIndex < 0) continue;

        FTransform BoneWorldTransform = MeshComp->GetBoneWorldTransform(BoneIndex);

        // 선택된 Body인지 확인
        bool bIsBodySelected = (BodyIndex == PhysState->SelectedBodyIndex);

        // Box Shape
        for (int32 i = 0; i < Body->AggGeom.BoxElems.Num(); ++i)
        {
            const FKBoxElem& Box = Body->AggGeom.BoxElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Box.Center);
            // Box.Rotation은 이미 FQuat
            FQuat FinalRotation = BoneWorldTransform.Rotation * Box.Rotation;
            // Box.X/Y/Z는 half extent
            FVector HalfExtent(Box.X, Box.Y, Box.Z);
            FMatrix Transform = FMatrix::FromTRS(ShapeCenter, FinalRotation, HalfExtent);

            // 색상 결정: 직접 피킹된 Shape > 같은 Body의 Shape > 미선택
            FLinearColor ShapeColor = UnselectedColor;
            if (bIsBodySelected)
            {
                bool bIsThisShapeSelected = (PhysState->SelectedShapeType == EShapeType::Box &&
                                             PhysState->SelectedShapeIndex == i);
                ShapeColor = bIsThisShapeSelected ? SelectedShapeColor : SiblingColor;
            }
            World->AddDebugBox(Transform, ShapeColor, 0);
        }

        // Sphere Shape
        for (int32 i = 0; i < Body->AggGeom.SphereElems.Num(); ++i)
        {
            const FKSphereElem& Sphere = Body->AggGeom.SphereElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Sphere.Center);
            FMatrix Transform = FMatrix::FromTRS(ShapeCenter, FQuat::Identity(), FVector(Sphere.Radius, Sphere.Radius, Sphere.Radius));

            FLinearColor ShapeColor = UnselectedColor;
            if (bIsBodySelected)
            {
                bool bIsThisShapeSelected = (PhysState->SelectedShapeType == EShapeType::Sphere &&
                                             PhysState->SelectedShapeIndex == i);
                ShapeColor = bIsThisShapeSelected ? SelectedShapeColor : SiblingColor;
            }
            World->AddDebugSphere(Transform, ShapeColor, 0);
        }

        // Capsule Shape
        for (int32 i = 0; i < Body->AggGeom.SphylElems.Num(); ++i)
        {
            const FKSphylElem& Capsule = Body->AggGeom.SphylElems[i];
            FVector ShapeCenter = BoneWorldTransform.Translation +
                BoneWorldTransform.Rotation.RotateVector(Capsule.Center);
            // 기본 축 회전: 엔진 캡슐(Z축) → PhysX 캡슐(X축) - BodyInstance/RagdollDebugRenderer와 동일
            FQuat BaseRotation = FQuat::MakeFromEulerZYX(FVector(-90.0f, 0.0f, 0.0f));
            // Capsule.Rotation은 이미 FQuat
            FQuat UserRotation = Capsule.Rotation;
            FQuat FinalLocalRotation = UserRotation * BaseRotation;
            FQuat FinalRotation = BoneWorldTransform.Rotation * FinalLocalRotation;
            FMatrix Transform = FMatrix::FromTRS(ShapeCenter, FinalRotation, FVector::One());
            // 언리얼 방식: HalfHeight = CylinderHalfHeight + Radius
            float HalfHeight = (Capsule.Length * 0.5f) + Capsule.Radius;

            FLinearColor ShapeColor = UnselectedColor;
            if (bIsBodySelected)
            {
                bool bIsThisShapeSelected = (PhysState->SelectedShapeType == EShapeType::Capsule &&
                                             PhysState->SelectedShapeIndex == i);
                ShapeColor = bIsThisShapeSelected ? SelectedShapeColor : SiblingColor;
            }
            World->AddDebugCapsule(Transform, Capsule.Radius, HalfHeight, ShapeColor, 0);
        }
    }
}

void SPhysicsAssetEditorWindow::RenderConstraintVisuals()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World) return;
    if (!PhysState->EditingAsset) return;
    if (!PhysState->bShowConstraints) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    UWorld* World = PhysState->World;

    // 색상 정의 - 언리얼 스타일
    const FLinearColor SwingConeColor(1.0f, 0.3f, 0.3f, 0.4f);      // 빨간색 반투명 (Swing 원뿔)
    const FLinearColor SwingConeSelectedColor(1.0f, 0.3f, 0.3f, 0.6f);  // 선택 시 더 진하게
    const FLinearColor TwistArcColor(0.3f, 1.0f, 0.3f, 0.4f);       // 녹색 반투명 (Twist 부채꼴)
    const FLinearColor TwistArcSelectedColor(0.3f, 1.0f, 0.3f, 0.6f);   // 선택 시 더 진하게
    const FLinearColor MarkerColor(1.0f, 1.0f, 1.0f, 0.8f);         // 흰색 (마커)

    // 크기 설정 (축소됨)
    const float NormalConeHeight = 0.1f;
    const float SelectedConeHeight = 0.15f;  // 선택 시 확대
    const float NormalArcRadius = 0.1f;
    const float SelectedArcRadius = 0.15f;   // 선택 시 확대
    const float MarkerRadius = 0.015f;

    // 모든 Constraint 순회
    for (int32 ConstraintIndex = 0; ConstraintIndex < PhysState->EditingAsset->Constraints.Num(); ++ConstraintIndex)
    {
        const FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[ConstraintIndex];

        // Constraint의 BoneName이 유효한지 확인 (dangling 방지)
        if (Constraint.ConstraintBone1.IsNone() || Constraint.ConstraintBone2.IsNone()) continue;

        bool bIsSelected = (ConstraintIndex == PhysState->SelectedConstraintIndex);

        // Bone1, Bone2 인덱스 찾기
        int32 Bone1Index = -1, Bone2Index = -1;
        for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
        {
            if (Skeleton->Bones[i].Name == Constraint.ConstraintBone1.ToString())
                Bone1Index = i;
            if (Skeleton->Bones[i].Name == Constraint.ConstraintBone2.ToString())
                Bone2Index = i;
        }
        if (Bone1Index < 0 || Bone2Index < 0) continue;

        // 본 월드 트랜스폼 (언리얼 규칙: Bone1=Child, Bone2=Parent)
        FTransform Bone1World = MeshComp->GetBoneWorldTransform(Bone1Index);  // Child
        FTransform Bone2World = MeshComp->GetBoneWorldTransform(Bone2Index);  // Parent

        // Joint 위치 계산 - Parent 본(Bone2) 위치 기준 (Position1은 항상 0,0,0)
        FVector JointPos = Bone2World.Translation +
            Bone2World.Rotation.RotateVector(Constraint.Position2);

        // Joint 회전 계산 - Parent 본(Bone2) 기준 (Rotation1은 항상 0,0,0)
        FQuat JointRot2 = FQuat::MakeFromEulerZYX(Constraint.Rotation2);
        FQuat JointWorldRot = Bone2World.Rotation * JointRot2;

        // 선택 상태에 따른 크기
        float ConeHeight = bIsSelected ? SelectedConeHeight : NormalConeHeight;
        float ArcRadius = bIsSelected ? SelectedArcRadius : NormalArcRadius;

        // 1. Joint 마커 (작은 구)
        FMatrix MarkerTransform = FMatrix::FromTRS(JointPos, FQuat::Identity(), FVector(MarkerRadius, MarkerRadius, MarkerRadius));
        World->AddDebugSphere(MarkerTransform, MarkerColor);

        // 2. Swing 원뿔 (양방향 다이아몬드 형태, 빨간색)
        // Locked-Locked인 경우 시각화 안 함 (완전히 잠긴 상태)
        bool bSwing1Locked = (Constraint.Swing1Motion == EAngularConstraintMotion::Locked);
        bool bSwing2Locked = (Constraint.Swing2Motion == EAngularConstraintMotion::Locked);
        if (!(bSwing1Locked && bSwing2Locked))
        {
            // Free인 경우 90도(PI/2), Limited인 경우 설정값, Locked인 경우 최소값(거의 없음)
            float Swing1Rad, Swing2Rad;

            if (Constraint.Swing1Motion == EAngularConstraintMotion::Free)
                Swing1Rad = PI_CONST * 0.5f;  // 90도 (자유 회전)
            else if (Constraint.Swing1Motion == EAngularConstraintMotion::Limited)
                Swing1Rad = Constraint.Swing1LimitAngle * PI_CONST / 180.0f;
            else  // Locked
                Swing1Rad = 0.01f;  // 거의 없음 (잠김 표시)

            if (Constraint.Swing2Motion == EAngularConstraintMotion::Free)
                Swing2Rad = PI_CONST * 0.5f;  // 90도 (자유 회전)
            else if (Constraint.Swing2Motion == EAngularConstraintMotion::Limited)
                Swing2Rad = Constraint.Swing2LimitAngle * PI_CONST / 180.0f;
            else  // Locked
                Swing2Rad = 0.01f;  // 거의 없음 (잠김 표시)

            // Swing 각도가 너무 작으면 최소값 설정
            Swing1Rad = FMath::Max(Swing1Rad, 0.01f);
            Swing2Rad = FMath::Max(Swing2Rad, 0.01f);

            FLinearColor ConeColor = bIsSelected ? SwingConeSelectedColor : SwingConeColor;

            // 양방향 원뿔 (메시 자체가 양방향)
            // JointWorldRot의 Y축이 Twist 방향이므로, Cone(X축 기준)을 위해 Y→X 회전 적용
            // +90도 Z축 회전: X → Y (즉, 원래 Y 방향이 X가 됨)
            FQuat YtoXRot = FQuat::FromAxisAngle(FVector(0, 0, 1), PI_CONST * 0.5f);
            // PhysX 좌표계와 일치시키기 위해 Twist 축(로컬 X축) 기준 +90도 추가 회전
            FQuat TwistRot90 = FQuat::FromAxisAngle(FVector(1, 0, 0), PI_CONST * 0.5f);
            FQuat ConeRot = JointWorldRot * YtoXRot * TwistRot90;
            FMatrix ConeTransform = FMatrix::FromTRS(JointPos, ConeRot, FVector::One());
            World->AddDebugCone(ConeTransform, Swing1Rad, Swing2Rad, ConeHeight, ConeColor);
        }

        // 3. Twist 부채꼴 (YZ 평면, 녹색)
        // 반지름은 고정, TwistAngle에 따라 부채꼴 각도만 변함
        // JointWorldRot의 Y축이 Twist 방향이므로, Arc(X축 기준)을 위해 Y→X 회전 적용
        FQuat YtoXRotArc = FQuat::FromAxisAngle(FVector(0, 0, 1), PI_CONST * 0.5f);
        // PhysX 좌표계와 일치시키기 위해 Twist 축(로컬 X축) 기준 +90도 추가 회전
        FQuat TwistRot90Arc = FQuat::FromAxisAngle(FVector(1, 0, 0), PI_CONST * 0.5f);
        FQuat ArcRot = JointWorldRot * YtoXRotArc * TwistRot90Arc;

        if (Constraint.TwistMotion == EAngularConstraintMotion::Limited)
        {
            float TwistRad = Constraint.TwistLimitAngle * PI_CONST / 180.0f;
            TwistRad = FMath::Max(TwistRad, 0.05f);

            FMatrix ArcTransform = FMatrix::FromTRS(JointPos, ArcRot, FVector::One());
            FLinearColor ArcColor = bIsSelected ? TwistArcSelectedColor : TwistArcColor;
            World->AddDebugArc(ArcTransform, TwistRad, ArcRadius, ArcColor);
        }
        else if (Constraint.TwistMotion == EAngularConstraintMotion::Free)
        {
            // Free면 전체 원 (PI = 180도, 최대 크기)
            FMatrix ArcTransform = FMatrix::FromTRS(JointPos, ArcRot, FVector::One());
            FLinearColor ArcColor = bIsSelected ? TwistArcSelectedColor : TwistArcColor;
            World->AddDebugArc(ArcTransform, PI_CONST, ArcRadius, ArcColor);
        }

        // 4. Child Frame 방향 화살표
        // 언리얼 규칙: Rotation1 = (0,0,0) → Child 본의 로컬 좌표계 = Joint 좌표계
        FQuat JointRot1 = FQuat::MakeFromEulerZYX(Constraint.Rotation1);
        FQuat ChildFrameWorldRot = Bone1World.Rotation * JointRot1;

        // 화살표 크기 설정 (얇고 길게)
        const float ArrowLength = bIsSelected ? 0.08f : 0.05f;
        const float ArrowHeadSize = ArrowLength * 0.15f;

        // 초록색 화살표: Child frame -Z축 (Swing 중심 방향)
        const FLinearColor SwingCenterArrowColor(0.2f, 1.0f, 0.2f, 1.0f);  // 진한 초록색
        {
            // DrawDebugArrow는 +X 방향으로 그리므로, X→-Z 회전 필요 (Y축 기준 -90도)
            FQuat XtoNegZRot = FQuat::FromAxisAngle(FVector(0, 1, 0), -PI_CONST * 0.5f);
            FQuat ArrowRot = ChildFrameWorldRot * XtoNegZRot;
            FMatrix ArrowTransform = FMatrix::FromTRS(JointPos, ArrowRot, FVector::One());
            World->AddDebugArrow(ArrowTransform, ArrowLength, ArrowHeadSize, SwingCenterArrowColor);
        }

        // 빨간색 화살표: Child frame -Y축 (Twist 방향, 180도 뒤집음)
        const FLinearColor TwistArrowColor(1.0f, 0.2f, 0.2f, 1.0f);  // 진한 빨간색
        {
            // DrawDebugArrow는 +X 방향으로 그리므로, X→-Y 회전 필요 (Z축 기준 +90도)
            FQuat XtoNegYRot = FQuat::FromAxisAngle(FVector(0, 0, 1), PI_CONST * 0.5f);  // X→-Y
            FQuat ArrowRot = ChildFrameWorldRot * XtoNegYRot;
            FMatrix ArrowTransform = FMatrix::FromTRS(JointPos, ArrowRot, FVector::One());
            World->AddDebugArrow(ArrowTransform, ArrowLength, ArrowHeadSize, TwistArrowColor);
        }
    }
}

void SPhysicsAssetEditorWindow::SelectBody(int32 Index, PhysicsAssetEditorState::ESelectionSource Source)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedBodyIndex = Index;
    PhysState->SelectedConstraintIndex = -1;  // Constraint 선택 해제
    PhysState->SelectionSource = Source;
    PhysState->bShapesDirty = true;  // Shape 라인 재구성 필요

    // Body의 첫 번째 Shape 자동 선택
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;
    if (PhysState->EditingAsset && Index >= 0 && Index < PhysState->EditingAsset->Bodies.Num())
    {
        UBodySetup* Body = PhysState->EditingAsset->Bodies[Index];
        if (Body)
        {
            // 우선순위: Sphere > Box > Capsule
            if (Body->AggGeom.SphereElems.Num() > 0)
            {
                PhysState->SelectedShapeIndex = 0;
                PhysState->SelectedShapeType = EShapeType::Sphere;
            }
            else if (Body->AggGeom.BoxElems.Num() > 0)
            {
                PhysState->SelectedShapeIndex = 0;
                PhysState->SelectedShapeType = EShapeType::Box;
            }
            else if (Body->AggGeom.SphylElems.Num() > 0)
            {
                PhysState->SelectedShapeIndex = 0;
                PhysState->SelectedShapeType = EShapeType::Capsule;
            }
        }
    }

    // 기즈모 업데이트
    UpdateShapeGizmo();
    UpdateConstraintGizmo();
}

void SPhysicsAssetEditorWindow::SelectConstraint(int32 Index, PhysicsAssetEditorState::ESelectionSource Source)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedConstraintIndex = Index;
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;
    PhysState->SelectionSource = Source;

    // Shape 기즈모 숨김
    UpdateShapeGizmo();
    // Constraint 기즈모 업데이트
    UpdateConstraintGizmo();
}

void SPhysicsAssetEditorWindow::ClearSelection()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;
    PhysState->SelectedBodyIndex = -1;
    PhysState->SelectedConstraintIndex = -1;
    PhysState->SelectedShapeIndex = -1;
    PhysState->SelectedShapeType = EShapeType::None;
    PhysState->bShapesDirty = true;  // Shape 라인 클리어 필요
    UpdateShapeGizmo();  // Shape 기즈모 숨김
    UpdateConstraintGizmo();  // Constraint 기즈모 숨김
}

void SPhysicsAssetEditorWindow::DeleteSelectedShape()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (PhysState->SelectedBodyIndex < 0 || PhysState->SelectedShapeIndex < 0) return;
    if (PhysState->SelectedShapeType == EShapeType::None) return;

    UBodySetup* Body = PhysState->EditingAsset->Bodies[PhysState->SelectedBodyIndex];
    if (!Body) return;

    bool bDeleted = false;

    switch (PhysState->SelectedShapeType)
    {
    case EShapeType::Sphere:
        if (PhysState->SelectedShapeIndex < Body->AggGeom.SphereElems.Num())
        {
            Body->AggGeom.SphereElems.RemoveAt(PhysState->SelectedShapeIndex);
            bDeleted = true;
        }
        break;
    case EShapeType::Box:
        if (PhysState->SelectedShapeIndex < Body->AggGeom.BoxElems.Num())
        {
            Body->AggGeom.BoxElems.RemoveAt(PhysState->SelectedShapeIndex);
            bDeleted = true;
        }
        break;
    case EShapeType::Capsule:
        if (PhysState->SelectedShapeIndex < Body->AggGeom.SphylElems.Num())
        {
            Body->AggGeom.SphylElems.RemoveAt(PhysState->SelectedShapeIndex);
            bDeleted = true;
        }
        break;
    default:
        break;
    }

    if (bDeleted)
    {
        PhysState->bIsDirty = true;

        // Body에 Shape가 하나도 없으면 Body도 삭제
        if (Body->AggGeom.SphereElems.Num() == 0 &&
            Body->AggGeom.BoxElems.Num() == 0 &&
            Body->AggGeom.SphylElems.Num() == 0)
        {
            RemoveBody(PhysState->SelectedBodyIndex);
        }

        // 모든 선택 해제 (회색으로 돌아감)
        ClearSelection();
    }
}

int32 SPhysicsAssetEditorWindow::GetShapeCountByType(UBodySetup* Body, EShapeType ShapeType)
{
    if (!Body) return 0;

    switch (ShapeType)
    {
    case EShapeType::Sphere:  return Body->AggGeom.SphereElems.Num();
    case EShapeType::Box:     return Body->AggGeom.BoxElems.Num();
    case EShapeType::Capsule: return Body->AggGeom.SphylElems.Num();
    default:                  return 0;
    }
}

void SPhysicsAssetEditorWindow::UpdateShapeGizmo()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World || !PhysState->ShapeGizmoAnchor) return;
    if (!PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    // Shape가 선택되지 않았으면 기즈모 숨김
    if (PhysState->SelectedBodyIndex < 0 ||
        PhysState->SelectedShapeIndex < 0 ||
        PhysState->SelectedShapeType == EShapeType::None)
    {
        PhysState->ShapeGizmoAnchor->ClearTarget();
        PhysState->ShapeGizmoAnchor->SetVisibility(false);
        PhysState->ShapeGizmoAnchor->SetEditability(false);
        // Constraint 기즈모가 활성화되지 않은 경우에만 SelectionManager 클리어
        if (PhysState->SelectedConstraintIndex < 0)
        {
            PhysState->World->GetSelectionManager()->ClearSelection();
        }
        return;
    }

    UBodySetup* Body = PhysState->EditingAsset->Bodies[PhysState->SelectedBodyIndex];
    if (!Body) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    // 본 인덱스 찾기
    int32 BoneIndex = -1;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].Name == Body->BoneName.ToString())
        {
            BoneIndex = i;
            break;
        }
    }
    if (BoneIndex < 0) return;

    // ShapeAnchorComponent에 타겟 설정 및 위치 업데이트
    PhysState->ShapeGizmoAnchor->SetTarget(Body, PhysState->SelectedShapeType,
                                            PhysState->SelectedShapeIndex, MeshComp, BoneIndex);
    PhysState->ShapeGizmoAnchor->UpdateAnchorFromShape();
    PhysState->ShapeGizmoAnchor->SetVisibility(true);
    PhysState->ShapeGizmoAnchor->SetEditability(true);

    // SelectionManager에 등록하여 기즈모 표시
    PhysState->World->GetSelectionManager()->SelectActor(PreviewActor);
    PhysState->World->GetSelectionManager()->SelectComponent(PhysState->ShapeGizmoAnchor);
}

void SPhysicsAssetEditorWindow::UpdateConstraintGizmo()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->World || !PhysState->ConstraintGizmoAnchor) return;
    if (!PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    // Constraint가 선택되지 않았으면 기즈모 숨김
    if (PhysState->SelectedConstraintIndex < 0 ||
        PhysState->SelectedConstraintIndex >= PhysState->EditingAsset->Constraints.Num())
    {
        PhysState->ConstraintGizmoAnchor->ClearTarget();
        PhysState->ConstraintGizmoAnchor->SetVisibility(false);
        PhysState->ConstraintGizmoAnchor->SetEditability(false);
        return;
    }

    FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[PhysState->SelectedConstraintIndex];

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (!PreviewActor) return;
    USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
    if (!MeshComp) return;

    // Bone1 (Child 본), Bone2 (Parent 본) 인덱스 찾기 (언리얼 규칙)
    int32 Bone1Index = -1;  // Child
    int32 Bone2Index = -1;  // Parent
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].Name == Constraint.ConstraintBone1.ToString())
            Bone1Index = i;
        if (Skeleton->Bones[i].Name == Constraint.ConstraintBone2.ToString())
            Bone2Index = i;
    }
    if (Bone1Index < 0) return;  // Child 본 없으면 리턴

    // ConstraintAnchorComponent에 타겟 설정 및 위치 업데이트 (Child, Parent 순)
    PhysState->ConstraintGizmoAnchor->SetTarget(&Constraint, MeshComp, Bone1Index, Bone2Index);
    PhysState->ConstraintGizmoAnchor->UpdateAnchorFromConstraint();
    PhysState->ConstraintGizmoAnchor->SetVisibility(true);
    PhysState->ConstraintGizmoAnchor->SetEditability(true);

    // SelectionManager에 등록하여 기즈모 표시
    PhysState->World->GetSelectionManager()->SelectActor(PreviewActor);
    PhysState->World->GetSelectionManager()->SelectComponent(PhysState->ConstraintGizmoAnchor);
}

void SPhysicsAssetEditorWindow::RenderBoneContextMenu(int32 BoneIndex, bool bHasBody, int32 BodyIndex)
{
    if (ImGui::BeginPopup("BoneContextMenu"))
    {
        if (!ActiveState || !ActiveState->CurrentMesh)
        {
            ImGui::EndPopup();
            return;
        }

        const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
        if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size())
        {
            ImGui::EndPopup();
            return;
        }

        const FString& BoneName = Skeleton->Bones[BoneIndex].Name;
        ImGui::TextDisabled("%s", BoneName.c_str());
        ImGui::Separator();

        // Shape 추가 서브메뉴 (Body 유무와 관계없이 항상 표시)
        if (ImGui::BeginMenu("Add Shape"))
        {
            if (ImGui::MenuItem("Box"))
            {
                AddShapeToBone(BoneIndex, EShapeType::Box);
                ContextMenuBoneIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Sphere"))
            {
                AddShapeToBone(BoneIndex, EShapeType::Sphere);
                ContextMenuBoneIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Capsule"))
            {
                AddShapeToBone(BoneIndex, EShapeType::Capsule);
                ContextMenuBoneIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndMenu();
        }

        // Body가 있으면 Constraint 및 삭제 옵션
        if (bHasBody)
        {
            ImGui::Separator();

            PhysicsAssetEditorState* PhysState = GetPhysicsState();

            // Constraint 생성 옵션
            if (PhysState && PhysState->ConstraintStartBodyIndex < 0)
            {
                // Constraint 시작점 설정
                if (ImGui::MenuItem("Start Constraint"))
                {
                    PhysState->ConstraintStartBodyIndex = BodyIndex;
                    ContextMenuBoneIndex = -1;
                    UE_LOG("[PhysicsAssetEditor] Constraint start: %s", BoneName.c_str());
                }
            }
            else if (PhysState && PhysState->ConstraintStartBodyIndex != BodyIndex)
            {
                // 시작점이 설정된 상태에서 다른 Body 우클릭
                UBodySetup* StartBody = PhysState->EditingAsset->Bodies[PhysState->ConstraintStartBodyIndex];
                FString startBoneName = StartBody ? StartBody->BoneName.ToString() : "Unknown";

                FString menuText = FString("Complete Constraint from ") + startBoneName;
                if (ImGui::MenuItem(menuText.c_str()))
                {
                    AddConstraintBetweenBodies(PhysState->ConstraintStartBodyIndex, BodyIndex);
                    PhysState->ConstraintStartBodyIndex = -1;
                    ContextMenuBoneIndex = -1;
                }

                if (ImGui::MenuItem("Cancel Constraint"))
                {
                    PhysState->ConstraintStartBodyIndex = -1;
                    ContextMenuBoneIndex = -1;
                }
            }

            // 부모 Body로 자동 Constraint 추가
            if (ImGui::MenuItem("Add Constraint to Parent"))
            {
                AddConstraintToParentBody(BodyIndex);
                ContextMenuBoneIndex = -1;
            }

            ImGui::Separator();

            if (ImGui::MenuItem("Remove Body"))
            {
                RemoveBody(BodyIndex);
                ContextMenuBoneIndex = -1;
            }
        }

        ImGui::EndPopup();
    }
}

void SPhysicsAssetEditorWindow::AddBodyToSelectedBone()
{
    if (!ActiveState || ActiveState->SelectedBoneIndex < 0)
        return;
    AddBodyToBone(ActiveState->SelectedBoneIndex);
}

void SPhysicsAssetEditorWindow::AddBodyToBone(int32 BoneIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size())
        return;

    const FString& BoneName = Skeleton->Bones[BoneIndex].Name;

    // 이미 Body가 있는지 확인
    for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
    {
        if (PhysState->EditingAsset->Bodies[i] &&
            PhysState->EditingAsset->Bodies[i]->BoneName == BoneName)
        {
            UE_LOG("[PhysicsAssetEditor] Body already exists for bone: %s", BoneName.c_str());
            return;
        }
    }

    // Body 생성
    UBodySetup* NewBody = CreateDefaultBodySetup(BoneName);
    if (NewBody)
    {
        PhysState->EditingAsset->Bodies.Add(NewBody);
        int32 NewIndex = PhysState->EditingAsset->Bodies.Num() - 1;
        SelectBody(NewIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->bIsDirty = true;
        UE_LOG("[PhysicsAssetEditor] Added body for bone: %s", BoneName.c_str());
    }
}

void SPhysicsAssetEditorWindow::RemoveBody(int32 BodyIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (BodyIndex < 0 || BodyIndex >= PhysState->EditingAsset->Bodies.Num()) return;

    UBodySetup* Body = PhysState->EditingAsset->Bodies[BodyIndex];
    if (!Body) return;

    FName BoneName = Body->BoneName;
    UE_LOG("[PhysicsAssetEditor] Removed body for bone: %s", BoneName.ToString().c_str());

    // 이 Body와 연결된 모든 Constraint 삭제 (역순으로 삭제해야 인덱스 문제 없음)
    for (int32 i = PhysState->EditingAsset->Constraints.Num() - 1; i >= 0; --i)
    {
        const FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[i];
        if (Constraint.ConstraintBone1 == BoneName || Constraint.ConstraintBone2 == BoneName)
        {
            UE_LOG("[PhysicsAssetEditor] Removed constraint: %s - %s",
                Constraint.ConstraintBone1.ToString().c_str(),
                Constraint.ConstraintBone2.ToString().c_str());
            PhysState->EditingAsset->Constraints.RemoveAt(i);
        }
    }

    PhysState->EditingAsset->Bodies.RemoveAt(BodyIndex);
    ClearSelection();
    PhysState->bIsDirty = true;
    PhysState->bShapesDirty = true;
}

UBodySetup* SPhysicsAssetEditorWindow::CreateDefaultBodySetup(const FString& BoneName)
{
    return CreateBodySetupWithShape(BoneName, EShapeType::Capsule);
}

void SPhysicsAssetEditorWindow::AddShapeToBone(int32 BoneIndex, EShapeType ShapeType)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size())
        return;

    const FString& BoneName = Skeleton->Bones[BoneIndex].Name;

    // 이미 Body가 있는지 확인
    UBodySetup* ExistingBody = nullptr;
    int32 ExistingBodyIndex = -1;
    for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
    {
        if (PhysState->EditingAsset->Bodies[i] &&
            PhysState->EditingAsset->Bodies[i]->BoneName == BoneName)
        {
            ExistingBody = PhysState->EditingAsset->Bodies[i];
            ExistingBodyIndex = i;
            break;
        }
    }

    if (ExistingBody)
    {
        // 기존 Body에 Shape 추가 - 추가 전 인덱스 기록
        int32 NewShapeIndex = GetShapeCountByType(ExistingBody, ShapeType);
        AddShapeToBody(ExistingBody, ShapeType);
        SelectBody(ExistingBodyIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->SelectedShapeType = ShapeType;
        PhysState->SelectedShapeIndex = NewShapeIndex;
    }
    else
    {
        // 새 Body 생성 후 Shape 추가 (MassInKg는 AddShapeToBody에서 부피 기반으로 계산됨)
        UBodySetup* NewBody = NewObject<UBodySetup>();
        NewBody->BoneName = FName(BoneName);
        NewBody->LinearDamping = 0.01f;
        NewBody->AngularDamping = 0.0f;

        AddShapeToBody(NewBody, ShapeType);

        PhysState->EditingAsset->Bodies.Add(NewBody);
        int32 NewIndex = PhysState->EditingAsset->Bodies.Num() - 1;

        // 자동 Constraint 생성: 부모 방향으로 Body를 탐색하여 연결
        AddConstraintToParentBody(NewIndex);

        SelectBody(NewIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->SelectedShapeType = ShapeType;
        PhysState->SelectedShapeIndex = 0;  // 새 Body의 첫 번째 Shape
    }

    PhysState->bIsDirty = true;
    UpdateShapeGizmo();  // 기즈모 표시
    const char* ShapeNames[] = { "None", "Sphere", "Box", "Capsule" };
    UE_LOG("[PhysicsAssetEditor] Added %s shape to bone: %s", ShapeNames[(int)ShapeType], BoneName.c_str());
}

void SPhysicsAssetEditorWindow::AddShapeToBody(UBodySetup* Body, EShapeType ShapeType)
{
    if (!Body) return;

    FString BoneName = Body->BoneName.ToString();

    switch (ShapeType)
    {
    case EShapeType::Box:
    {
        FKBoxElem Box;
        Box.Name = FName(BoneName + "_box");
        Box.Center = FVector(0.0f, 0.0f, 0.0f);
        Box.Rotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, 0.0f));
        Box.X = 0.15f;  // Half extent
        Box.Y = 0.15f;
        Box.Z = 0.15f;
        Body->AggGeom.BoxElems.Add(Box);
        break;
    }
    case EShapeType::Sphere:
    {
        FKSphereElem Sphere;
        Sphere.Name = FName(BoneName + "_sphere");
        Sphere.Center = FVector(0.0f, 0.0f, 0.0f);
        Sphere.Radius = 0.15f;
        Body->AggGeom.SphereElems.Add(Sphere);
        break;
    }
    case EShapeType::Capsule:
    default:
    {
        FKSphylElem Capsule;
        Capsule.Name = FName(BoneName + "_sphyl");
        Capsule.Center = FVector(0.0f, 0.0f, 0.0f);
        Capsule.Rotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, 0.0f));
        Capsule.Radius = 0.1f;
        Capsule.Length = 0.3f;  // 전체 실린더 길이
        Body->AggGeom.SphylElems.Add(Capsule);
        break;
    }
    }

    // Shape 추가 후 부피 기반 질량 재계산
    UpdateBodyMassFromVolume(Body);
}

void SPhysicsAssetEditorWindow::AddBodyToBoneWithShape(int32 BoneIndex, EShapeType ShapeType)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton || BoneIndex < 0 || BoneIndex >= (int32)Skeleton->Bones.size())
        return;

    const FString& BoneName = Skeleton->Bones[BoneIndex].Name;

    // 이미 Body가 있는지 확인
    for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
    {
        if (PhysState->EditingAsset->Bodies[i] &&
            PhysState->EditingAsset->Bodies[i]->BoneName == BoneName)
        {
            UE_LOG("[PhysicsAssetEditor] Body already exists for bone: %s", BoneName.c_str());
            return;
        }
    }

    // Body 생성
    UBodySetup* NewBody = CreateBodySetupWithShape(BoneName, ShapeType);
    if (NewBody)
    {
        PhysState->EditingAsset->Bodies.Add(NewBody);
        int32 NewIndex = PhysState->EditingAsset->Bodies.Num() - 1;
        SelectBody(NewIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
        PhysState->bIsDirty = true;

        const char* ShapeNames[] = { "Box", "Sphere", "Capsule" };
        UE_LOG("[PhysicsAssetEditor] Added %s body for bone: %s", ShapeNames[(int)ShapeType], BoneName.c_str());
    }
}

UBodySetup* SPhysicsAssetEditorWindow::CreateBodySetupWithShape(const FString& BoneName, EShapeType ShapeType)
{
    UBodySetup* Body = NewObject<UBodySetup>();
    Body->BoneName = FName(BoneName);

    // Shape 추가 (AddShapeToBody가 부피 기반 질량도 계산함)
    AddShapeToBody(Body, ShapeType);

    // 기본 물리 속성 (MassInKg는 AddShapeToBody에서 부피 기반으로 이미 계산됨)
    Body->LinearDamping = 0.01f;
    Body->AngularDamping = 0.05f;

    return Body;
}

// ========== Constraint 생성/삭제 함수들 ==========

FConstraintInstance SPhysicsAssetEditorWindow::CreateDefaultConstraint(const FName& ChildBone, const FName& ParentBone)
{
    // 언리얼 규칙: ConstraintBone1 = Child, ConstraintBone2 = Parent
    FConstraintInstance Constraint;
    Constraint.ConstraintBone1 = ChildBone;   // Child
    Constraint.ConstraintBone2 = ParentBone;  // Parent

    // Linear Limits: 기본적으로 모두 잠금 (뼈가 빠지면 안됨)
    Constraint.LinearXMotion = ELinearConstraintMotion::Locked;
    Constraint.LinearYMotion = ELinearConstraintMotion::Locked;
    Constraint.LinearZMotion = ELinearConstraintMotion::Locked;
    Constraint.LinearLimit = 0.0f;

    // Angular Limits: 기본적으로 Limited
    Constraint.TwistMotion = EAngularConstraintMotion::Limited;
    Constraint.Swing1Motion = EAngularConstraintMotion::Limited;
    Constraint.Swing2Motion = EAngularConstraintMotion::Limited;

    Constraint.TwistLimitAngle = 45.0f;
    Constraint.Swing1LimitAngle = 45.0f;
    Constraint.Swing2LimitAngle = 45.0f;

    // 충돌 비활성화 (인접 본 간 충돌 무시)
    Constraint.bDisableCollision = true;

    return Constraint;
}

void SPhysicsAssetEditorWindow::AddConstraintBetweenBodies(int32 BodyIndex1, int32 BodyIndex2)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    // 유효성 검사
    if (BodyIndex1 < 0 || BodyIndex1 >= PhysState->EditingAsset->Bodies.Num()) return;
    if (BodyIndex2 < 0 || BodyIndex2 >= PhysState->EditingAsset->Bodies.Num()) return;
    if (BodyIndex1 == BodyIndex2) return;

    UBodySetup* Body1 = PhysState->EditingAsset->Bodies[BodyIndex1];
    UBodySetup* Body2 = PhysState->EditingAsset->Bodies[BodyIndex2];
    if (!Body1 || !Body2) return;

    // 중복 Constraint 체크
    for (const auto& Constraint : PhysState->EditingAsset->Constraints)
    {
        if ((Constraint.ConstraintBone1 == Body1->BoneName && Constraint.ConstraintBone2 == Body2->BoneName) ||
            (Constraint.ConstraintBone1 == Body2->BoneName && Constraint.ConstraintBone2 == Body1->BoneName))
        {
            UE_LOG("[PhysicsAssetEditor] Constraint already exists between %s and %s",
                   Body1->BoneName.ToString().c_str(), Body2->BoneName.ToString().c_str());
            return;
        }
    }

    // 사용자 선택 순서 기준으로 Parent/Child 결정
    // - BodyIndex1 (Start Constraint) → Parent
    // - BodyIndex2 (Complete Constraint) → Child
    FName ParentBoneName = Body1->BoneName;
    FName ChildBoneName = Body2->BoneName;

    UE_LOG("[PhysicsAssetEditor] Creating constraint: Parent=%s (Start), Child=%s (Complete)",
           ParentBoneName.ToString().c_str(), ChildBoneName.ToString().c_str());

    // Constraint 생성 (언리얼 규칙: Bone1=Child, Bone2=Parent)
    FConstraintInstance NewConstraint = CreateDefaultConstraint(ChildBoneName, ParentBoneName);

    // === Rotation2 계산 (본 방향 기반, 언리얼 방식) ===
    // Joint는 Child 본 위치에 생성됨
    // - Child 입장 (Rotation1): Joint가 자기 원점에 있음 → 기본값 (0,0,0)
    // - Parent 입장 (Rotation2): Child 본 방향을 향하는 회전 계산 필요
    ASkeletalMeshActor* PreviewActor = static_cast<ASkeletalMeshActor*>(ActiveState->PreviewActor);
    if (PreviewActor)
    {
        USkeletalMeshComponent* MeshComp = PreviewActor->GetSkeletalMeshComponent();
        if (MeshComp)
        {
            // Child/Parent 본 인덱스 찾기
            int32 ChildBoneIdx = -1, ParentBoneIdx = -1;
            for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
            {
                if (Skeleton->Bones[i].Name == ChildBoneName.ToString())
                    ChildBoneIdx = i;
                if (Skeleton->Bones[i].Name == ParentBoneName.ToString())
                    ParentBoneIdx = i;
            }

            if (ChildBoneIdx >= 0 && ParentBoneIdx >= 0)
            {
                FTransform ChildWorld = MeshComp->GetBoneWorldTransform(ChildBoneIdx);
                FTransform ParentWorld = MeshComp->GetBoneWorldTransform(ParentBoneIdx);

                // 본 방향 계산 (Parent → Child 방향 = 자식 본 방향)
                FVector BoneDirection = (ChildWorld.Translation - ParentWorld.Translation);
                if (BoneDirection.SizeSquared() > KINDA_SMALL_NUMBER)
                {
                    FVector Dir = BoneDirection.GetNormalized();
                    // 좌표계 변환: Engine Y축 = PhysX X축 (Twist)
                    FVector DefaultAxis(0, 1, 0);  // Engine Y축 → PhysX X축 (Twist)

                    // DefaultAxis를 Dir로 회전시키는 쿼터니언 계산
                    FQuat JointWorldRot;
                    float Dot = FVector::Dot(DefaultAxis, Dir);
                    if (Dot > 0.9999f)
                    {
                        JointWorldRot = FQuat::Identity();
                    }
                    else if (Dot < -0.9999f)
                    {
                        JointWorldRot = FQuat::FromAxisAngle(FVector(0, 0, 1), PI_CONST);
                    }
                    else
                    {
                        FVector Axis = FVector::Cross(DefaultAxis, Dir).GetNormalized();
                        float Angle = acos(Dot);
                        JointWorldRot = FQuat::FromAxisAngle(Axis, Angle);
                    }

                    // === Rotation1: Child 본 로컬에서 Joint 프레임 회전 ===
                    FQuat ChildWorldRotInv = ChildWorld.Rotation.Inverse();
                    FQuat LocalRot1 = ChildWorldRotInv * JointWorldRot;
                    NewConstraint.Rotation1 = LocalRot1.ToEulerZYXDeg();

                    // === Rotation2: Parent 본 로컬에서 Joint 프레임 회전 ===
                    FQuat ParentWorldRotInv = ParentWorld.Rotation.Inverse();
                    FQuat LocalRot2 = ParentWorldRotInv * JointWorldRot;
                    NewConstraint.Rotation2 = LocalRot2.ToEulerZYXDeg();

                    // Position2도 계산 (Parent 로컬에서 Joint 위치)
                    FVector LocalPos2 = ParentWorldRotInv.RotateVector(ChildWorld.Translation - ParentWorld.Translation);
                    NewConstraint.Position2 = LocalPos2;

                    // Position1은 Child 본 원점 기준 (0,0,0)
                    NewConstraint.Position1 = FVector::Zero();
                }
            }
        }
    }

    PhysState->EditingAsset->Constraints.Add(NewConstraint);

    int32 NewIndex = PhysState->EditingAsset->Constraints.Num() - 1;
    SelectConstraint(NewIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
    PhysState->bIsDirty = true;
    PhysState->bConstraintsDirty = true;

    UE_LOG("[PhysicsAssetEditor] Added constraint: Child=%s, Parent=%s",
           ChildBoneName.ToString().c_str(), ParentBoneName.ToString().c_str());
}

void SPhysicsAssetEditorWindow::AddConstraintToParentBody(int32 ChildBodyIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!ActiveState || !ActiveState->CurrentMesh) return;

    const FSkeleton* Skeleton = ActiveState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    if (ChildBodyIndex < 0 || ChildBodyIndex >= PhysState->EditingAsset->Bodies.Num()) return;

    UBodySetup* ChildBody = PhysState->EditingAsset->Bodies[ChildBodyIndex];
    if (!ChildBody) return;

    // Child Body의 본 인덱스 찾기
    int32 ChildBoneIndex = -1;
    for (int32 i = 0; i < (int32)Skeleton->Bones.size(); ++i)
    {
        if (Skeleton->Bones[i].Name == ChildBody->BoneName.ToString())
        {
            ChildBoneIndex = i;
            break;
        }
    }
    if (ChildBoneIndex < 0) return;

    // 부모 본 체인을 따라가며 Body가 있는 본 찾기
    int32 ParentBoneIndex = Skeleton->Bones[ChildBoneIndex].ParentIndex;
    while (ParentBoneIndex >= 0)
    {
        const FString& ParentBoneName = Skeleton->Bones[ParentBoneIndex].Name;

        for (int32 i = 0; i < PhysState->EditingAsset->Bodies.Num(); ++i)
        {
            UBodySetup* ParentBody = PhysState->EditingAsset->Bodies[i];
            if (ParentBody && ParentBody->BoneName.ToString() == ParentBoneName)
            {
                // 부모 Body 발견 - Constraint 생성
                AddConstraintBetweenBodies(i, ChildBodyIndex);
                return;
            }
        }

        ParentBoneIndex = Skeleton->Bones[ParentBoneIndex].ParentIndex;
    }

    UE_LOG("[PhysicsAssetEditor] No parent body found for %s", ChildBody->BoneName.ToString().c_str());
}

void SPhysicsAssetEditorWindow::RemoveConstraint(int32 ConstraintIndex)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (ConstraintIndex < 0 || ConstraintIndex >= PhysState->EditingAsset->Constraints.Num()) return;

    FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[ConstraintIndex];
    UE_LOG("[PhysicsAssetEditor] Removed constraint between %s and %s",
           Constraint.ConstraintBone1.ToString().c_str(),
           Constraint.ConstraintBone2.ToString().c_str());

    PhysState->EditingAsset->Constraints.RemoveAt(ConstraintIndex);
    ClearSelection();
    PhysState->bIsDirty = true;
    PhysState->bConstraintsDirty = true;
}

void SPhysicsAssetEditorWindow::RenderConstraintTreeNode(int32 ConstraintIndex, const FName& CurrentBoneName)
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (ConstraintIndex < 0 || ConstraintIndex >= PhysState->EditingAsset->Constraints.Num()) return;

    FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[ConstraintIndex];

    // 언리얼 스타일: [ Bone1 -> Bone2 ] 컨스트레인트
    bool bSelected = (PhysState->SelectedConstraintIndex == ConstraintIndex);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (bSelected) flags |= ImGuiTreeNodeFlags_Selected;

    // 트리 라벨: [ Parent -> Child ] 형식 (ConstraintBone2=Parent, ConstraintBone1=Child)
    FString label = FString("[ ") + Constraint.ConstraintBone2.ToString() +
                    " -> " + Constraint.ConstraintBone1.ToString() + " ]";

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.4f, 1.0f)); // 주황색
    if (ImGui::TreeNodeEx((void*)(intptr_t)(30000 + ConstraintIndex), flags, "%s", label.c_str()))
    {
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();

    if (ImGui::IsItemClicked())
    {
        SelectConstraint(ConstraintIndex, PhysicsAssetEditorState::ESelectionSource::TreeOrViewport);
    }

    // 우클릭 컨텍스트 메뉴
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
    {
        ContextMenuConstraintIndex = ConstraintIndex;
        ImGui::OpenPopup("ConstraintContextMenu");
    }

    RenderConstraintContextMenu(ConstraintIndex);
}

void SPhysicsAssetEditorWindow::RenderConstraintContextMenu(int32 ConstraintIndex)
{
    if (ContextMenuConstraintIndex != ConstraintIndex) return;

    if (ImGui::BeginPopup("ConstraintContextMenu"))
    {
        PhysicsAssetEditorState* PhysState = GetPhysicsState();
        if (PhysState && PhysState->EditingAsset &&
            ConstraintIndex >= 0 && ConstraintIndex < PhysState->EditingAsset->Constraints.Num())
        {
            FConstraintInstance& Constraint = PhysState->EditingAsset->Constraints[ConstraintIndex];

            ImGui::TextDisabled("%s <-> %s",
                Constraint.ConstraintBone1.ToString().c_str(),
                Constraint.ConstraintBone2.ToString().c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Delete Constraint"))
            {
                RemoveConstraint(ConstraintIndex);
                ContextMenuConstraintIndex = -1;
            }
        }
        ImGui::EndPopup();
    }
}

// ===== Tools 패널 (언리얼 피직스 에셋 에디터 스타일) =====
// 본 타입 감지 함수들은 BoneUtils.h에서 제공 (BoneUtils:: 네임스페이스)

void SPhysicsAssetEditorWindow::RenderToolsPanel()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState) return;

    if (ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Body Creation");
        ImGui::Separator();

        // 프리미티브 타입 선택 (콤보박스)
        // EShapeType: None=0, Sphere=1, Box=2, Capsule=3
        // 콤보박스 배열 인덱스: 0=Sphere, 1=Box, 2=Capsule
        const char* PrimitiveTypes[] = { "Sphere", "Box", "Capsule" };
        int CurrentType = (int)SelectedPrimitiveType - 1;  // Sphere(1)->0, Box(2)->1, Capsule(3)->2
        if (ImGui::Combo("Primitive Type", &CurrentType, PrimitiveTypes, 3))
        {
            SelectedPrimitiveType = (EShapeType)(CurrentType + 1);  // 0->Sphere(1), 1->Box(2), 2->Capsule(3)
        }

        ImGui::Spacing();

        // FBX가 로드되어 있어야 버튼 활성화
        bool bCanGenerate = PhysState->CurrentMesh != nullptr;

        if (!bCanGenerate)
        {
            ImGui::BeginDisabled();
        }

        // "모든 바디 생성" 버튼
        if (ImGui::Button("Generate All Bodies", ImVec2(-1, 30)))
        {
            GenerateAllBodies();
        }

        if (!bCanGenerate)
        {
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::SetTooltip("Load an FBX file first");
            }
        }
        else if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Generate bodies for all bones based on skeleton structure");
        }
    }

    // ===== Simulation 섹션 =====
    if (ImGui::CollapsingHeader("Simulation", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 시뮬레이션 체크박스
        if (ImGui::Checkbox("Simulate", &bSimulateInEditor))
        {
            // PreviewActor에서 SkeletalMeshComponent 가져오기
            ASkeletalMeshActor* PreviewActor = PhysState->PreviewActor;
            USkeletalMeshComponent* PreviewComp = PreviewActor ? PreviewActor->GetSkeletalMeshComponent() : nullptr;
            if (PreviewComp)
            {
                if (bSimulateInEditor)
                {
                    // 이전 시뮬레이션 상태가 남아있을 수 있으므로 먼저 정리
                    if (PreviewComp->bRagdollInitialized)
                    {
                        PreviewComp->TermRagdoll();
                    }

                    PreviewComp->ResetToRefPose();
                    PreviewComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

                    // PhysicsAsset 강제 재설정 (같은 에셋이라도)
                    UPhysicsAsset* AssetToSet = PhysState->EditingAsset;
                    PreviewComp->PhysicsAsset = nullptr;
                    PreviewComp->SetPhysicsAsset(AssetToSet);

                    FPhysScene* PhysScene = PhysState->World->GetPhysicsScene();
                    if (PhysScene)
                    {
                        PhysScene->FlushDeferredReleases();
                        PreviewComp->InitRagdoll(PhysScene);
                        PreviewComp->SetPhysicsMode(EPhysicsMode::Ragdoll);
                    }
                }
                else
                {
                    // 시뮬레이션 중지 + 래그돌 해제 + 포즈 복원
                    PreviewComp->TermRagdoll();
                    PreviewComp->SetPhysicsMode(EPhysicsMode::Animation);
                    PreviewComp->ResetToRefPose();

                    // 본 라인 캐시 리셋
                    ActiveState->bBoneLinesDirty = true;
                }
            }
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Toggle physics simulation in preview");
        }
    }

    // 확인 팝업 렌더링
    RenderGenerateConfirmPopup();
}

void SPhysicsAssetEditorWindow::RenderGenerateConfirmPopup()
{
    if (!bShowGenerateConfirmPopup) return;

    ImGui::OpenPopup("Confirm Generate");

    if (ImGui::BeginPopupModal("Confirm Generate", &bShowGenerateConfirmPopup,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Existing bodies and constraints will be deleted.");
        ImGui::Text("Do you want to continue?");
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120, 0)))
        {
            bShowGenerateConfirmPopup = false;
            DoGenerateAllBodies();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            bShowGenerateConfirmPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void SPhysicsAssetEditorWindow::GenerateAllBodies()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset) return;
    if (!PhysState->CurrentMesh) return;

    const FSkeleton* Skeleton = PhysState->CurrentMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.empty()) return;

    UPhysicsAsset* Asset = PhysState->EditingAsset;

    // 기존 데이터가 있으면 확인 팝업 표시
    if (Asset->Bodies.Num() > 0 || Asset->Constraints.Num() > 0)
    {
        bShowGenerateConfirmPopup = true;
        return;
    }

    // 기존 데이터 없으면 바로 생성
    DoGenerateAllBodies();
}

float SPhysicsAssetEditorWindow::CalculateBoneLengthForGenerate(const FSkeleton& Skeleton, int32 BoneIndex, FVector& OutBoneLocalDir)
{
    const FBone& CurrentBone = Skeleton.Bones[BoneIndex];
    FVector CurrentPos = BoneUtils::GetBonePosition(CurrentBone.BindPose);

    // Hand 본인지 확인 - 여러 자식(손가락)의 평균 방향 사용
    bool bIsHandBone = BoneUtils::IsHandBone(CurrentBone.Name);

    if (bIsHandBone)
    {
        // Hand 본: 모든 자식 손가락들의 평균 방향 계산
        FVector AvgDirGlobal = FVector::Zero();
        float MaxLength = 0.0f;
        int32 ChildCount = 0;

        for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
        {
            if (Skeleton.Bones[i].ParentIndex == BoneIndex)
            {
                const FBone& ChildBone = Skeleton.Bones[i];
                FVector ChildPos = BoneUtils::GetBonePosition(ChildBone.BindPose);
                FVector DirToChild = ChildPos - CurrentPos;
                float Length = DirToChild.Size();

                if (Length > KINDA_SMALL_NUMBER)
                {
                    AvgDirGlobal = AvgDirGlobal + DirToChild.GetNormalized();
                    ChildCount++;
                    if (Length > MaxLength) MaxLength = Length;
                }
            }
        }

        if (ChildCount > 0 && AvgDirGlobal.SizeSquared() > KINDA_SMALL_NUMBER)
        {
            AvgDirGlobal = AvgDirGlobal.GetNormalized();
            OutBoneLocalDir = BoneUtils::TransformDirection(CurrentBone.InverseBindPose, AvgDirGlobal);
            OutBoneLocalDir = OutBoneLocalDir.GetNormalized();
            return MaxLength;
        }
    }

    // 일반 본: 첫 번째 자식 본 방향 사용
    for (int32 i = 0; i < Skeleton.Bones.Num(); ++i)
    {
        if (Skeleton.Bones[i].ParentIndex == BoneIndex)
        {
            const FBone& ChildBone = Skeleton.Bones[i];
            FVector ChildPos = BoneUtils::GetBonePosition(ChildBone.BindPose);

            // 글로벌 방향 계산
            FVector BoneDirGlobal = ChildPos - CurrentPos;
            float BoneLength = BoneDirGlobal.Size();

            if (BoneLength > KINDA_SMALL_NUMBER)
            {
                BoneDirGlobal = BoneDirGlobal / BoneLength;  // 정규화

                // 글로벌 방향 → 본의 로컬 좌표계로 변환
                // InverseBindPose의 회전 부분만 사용하여 방향 변환
                OutBoneLocalDir = BoneUtils::TransformDirection(CurrentBone.InverseBindPose, BoneDirGlobal);
                OutBoneLocalDir = OutBoneLocalDir.GetNormalized();
            }
            else
            {
                OutBoneLocalDir = FVector(0, 0, 1);  // 기본 방향
            }

            return BoneLength;
        }
    }

    // 자식이 없으면 (말단 본) 기본값
    OutBoneLocalDir = FVector(0, 0, 1);
    return 0.0f;
}

void SPhysicsAssetEditorWindow::AdjustShapeForBoneType(UBodySetup* Body, const FString& BoneName, float BoneLength)
{
    if (!Body) return;

    // BoneUtils 네임스페이스의 상수 사용
    using namespace BoneUtils;

    // 본 타입 감지
    bool bIsHandBone = IsHandBone(BoneName);
    bool bIsHeadBone = IsHeadBone(BoneName);
    bool bIsSpineBone = IsSpineOrPelvisBone(BoneName);
    bool bIsNeckBone = IsNeckBone(BoneName);
    bool bIsShoulderBone = IsShoulderBone(BoneName);
    bool bIsFootBone = IsFootBone(BoneName);

    // Hand 본은 고정 크기
    if (bIsHandBone)
    {
        BoneLength = HandBoneSize;
    }

    // Head 본은 길이 제한
    if (bIsHeadBone)
    {
        BoneLength = HeadBoneSize;
    }

    // 클램프
    float ClampedLength = std::min(std::max(BoneLength, MinBoneSize), MaxBoneSize);

    // 비율 결정
    float EffectiveRadiusRatio = RadiusRatio;
    float EffectiveLengthRatio = LengthRatio;

    if (bIsHandBone) EffectiveRadiusRatio = HandRadiusRatio;
    else if (bIsHeadBone) EffectiveRadiusRatio = HeadRadiusRatio;
    else if (bIsSpineBone) { EffectiveRadiusRatio = SpineRadiusRatio; EffectiveLengthRatio = SpineLengthRatio; }
    else if (bIsNeckBone) { EffectiveRadiusRatio = NeckRadiusRatio; EffectiveLengthRatio = NeckLengthRatio; }
    else if (bIsShoulderBone) EffectiveRadiusRatio = ShoulderRadiusRatio;
    else if (bIsFootBone) EffectiveRadiusRatio = FootRadiusRatio;

    float ShapeRadius = ClampedLength * EffectiveRadiusRatio;
    float ShapeLength = ClampedLength * EffectiveLengthRatio;

    // Shape 타입별 크기 조정
    if (Body->AggGeom.SphylElems.Num() > 0)
    {
        // 캡슐
        FKSphylElem& Capsule = Body->AggGeom.SphylElems[0];
        Capsule.Radius = ShapeRadius;
        Capsule.Length = ShapeLength;
        Capsule.Center = FVector(0, 0, ClampedLength * 0.5f);

        if (bIsHeadBone)
        {
            Capsule.Center.Z += HeadCenterOffset;
        }

        if (bIsSpineBone)
        {
            Capsule.Rotation = FQuat::MakeFromEulerZYX(FVector(180.0f, 90.0f, 90.0f));
        }
        else
        {
            Capsule.Rotation = FQuat::MakeFromEulerZYX(FVector(180.0f, 0.0f, 0.0f));
        }
    }
    else if (Body->AggGeom.SphereElems.Num() > 0)
    {
        // 구
        FKSphereElem& Sphere = Body->AggGeom.SphereElems[0];
        Sphere.Radius = ShapeRadius * 1.5f;  // 구는 좀 더 크게
        Sphere.Center = FVector(0, 0, ClampedLength * 0.5f);

        if (bIsHeadBone)
        {
            Sphere.Center.Z += HeadCenterOffset;
        }
    }
    else if (Body->AggGeom.BoxElems.Num() > 0)
    {
        // 박스
        FKBoxElem& Box = Body->AggGeom.BoxElems[0];
        Box.X = ShapeRadius;
        Box.Y = ShapeRadius;
        Box.Z = ShapeLength * 0.5f;
        Box.Center = FVector(0, 0, ClampedLength * 0.5f);

        if (bIsHeadBone)
        {
            Box.Center.Z += HeadCenterOffset;
        }

        if (bIsSpineBone)
        {
            Box.Rotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, 90.0f));
        }
    }
}

void SPhysicsAssetEditorWindow::GenerateConstraintsForBodies()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset || !PhysState->CurrentMesh) return;

    const FSkeleton* Skeleton = PhysState->CurrentMesh->GetSkeleton();
    if (!Skeleton) return;

    UPhysicsAsset* Asset = PhysState->EditingAsset;

    // Body가 있는 본들의 이름 집합 생성
    TSet<FString> BonesWithBodies;
    for (const UBodySetup* Body : Asset->Bodies)
    {
        if (Body)
        {
            BonesWithBodies.insert(Body->BoneName.ToString());
        }
    }
    // 각 본의 자식 인덱스 맵 생성 (손자 본 찾기용)
    TMap<int32, TArray<int32>> BoneChildrenMap;
    for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
    {
        int32 ParentIdx = Skeleton->Bones[i].ParentIndex;
        if (ParentIdx >= 0)
        {
            BoneChildrenMap[ParentIdx].push_back(i);
        }
    }

    // 각 자식 본과 부모 본 사이에 Constraint 생성
    for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
    {
        const FBone& ChildBone = Skeleton->Bones[i];
        int32 ParentIndex = ChildBone.ParentIndex;

        if (ParentIndex < 0 || ParentIndex >= Skeleton->Bones.Num())
            continue;

        const FBone& ParentBone = Skeleton->Bones[ParentIndex];

        // 두 본 모두 Body가 있어야 Constraint 생성
        if (BonesWithBodies.find(ChildBone.Name) == BonesWithBodies.end() ||
            BonesWithBodies.find(ParentBone.Name) == BonesWithBodies.end())
        {
            continue;
        }

        // 언리얼 규칙: Bone1=Child, Bone2=Parent
        FConstraintInstance CI = CreateDefaultConstraint(FName(ChildBone.Name), FName(ParentBone.Name));

        // Joint 위치는 Child 본 위치에 생성됨 (언리얼 방식)
        // Position1 = Child 로컬에서 Joint 위치 = (0,0,0)
        // Position2 = Parent 로컬에서 Joint 위치 = Child까지의 오프셋
        // Rotation1 = Child 로컬에서 Joint 방향 = 자식 본 방향(Child→Grandchild) 기준
        // Rotation2 = Parent 로컬에서 Joint 방향 = 자식 본 방향 기준

        FVector ParentPosGlobal = BoneUtils::GetBonePosition(ParentBone.BindPose);
        FVector ChildPosGlobal = BoneUtils::GetBonePosition(ChildBone.BindPose);
        FVector DirToChildGlobal = ChildPosGlobal - ParentPosGlobal;
        float DistanceToChild = DirToChildGlobal.Size();

        // === 자식 본 방향 계산 (Child → Grandchild) ===
        // 언리얼 규칙: Twist 축 = 자식 본의 방향
        FVector ChildBoneDirection = FVector::Zero();
        auto it = BoneChildrenMap.find(i);  // 현재 자식 본(i)의 자식들(손자) 찾기
        if (it != BoneChildrenMap.end() && !it->second.empty())
        {
            // 모든 손자 본들의 방향 평균 (손가락 등 여러 자식이 있는 경우)
            FVector AvgDirection = FVector::Zero();
            for (int32 GrandchildIndex : it->second)
            {
                const FBone& GrandchildBone = Skeleton->Bones[GrandchildIndex];
                FVector GrandchildPosGlobal = BoneUtils::GetBonePosition(GrandchildBone.BindPose);
                FVector DirToGrandchild = GrandchildPosGlobal - ChildPosGlobal;
                if (DirToGrandchild.SizeSquared() > KINDA_SMALL_NUMBER)
                {
                    AvgDirection += DirToGrandchild.GetNormalized();
                }
            }
            if (AvgDirection.SizeSquared() > KINDA_SMALL_NUMBER)
            {
                ChildBoneDirection = AvgDirection.GetNormalized();
            }
            else
            {
                // fallback: 부모→자식 방향
                ChildBoneDirection = DistanceToChild > KINDA_SMALL_NUMBER ? DirToChildGlobal.GetNormalized() : FVector(0, 1, 0);
            }
        }
        else
        {
            // 손자가 없으면 부모→자식 방향 사용 (끝 본)
            if (DistanceToChild > KINDA_SMALL_NUMBER)
            {
                ChildBoneDirection = DirToChildGlobal.GetNormalized();
            }
            else
            {
                ChildBoneDirection = FVector(0, 1, 0);  // 기본값
            }
        }

        // Head 본 감지
        bool bIsHeadBone = false;
        {
            FString BoneNameLower = ChildBone.Name;
            std::transform(BoneNameLower.begin(), BoneNameLower.end(), BoneNameLower.begin(), ::tolower);
            bIsHeadBone = (BoneNameLower.find("head") != std::string::npos);
        }

        // Head 본은 방향을 반대로 (목→머리 대신 머리→위쪽 방향)
        if (bIsHeadBone)
        {
            ChildBoneDirection = -ChildBoneDirection;
        }

        // Position1: Child 본 로컬에서 Joint 위치 = 원점 (언리얼 규칙)
        CI.Position1 = FVector::Zero();

        // === Joint 월드 회전 계산 (본 방향 → Y축) ===
        // Twist 축(Y축)이 본 방향을 따라가도록 하는 월드 회전
        FVector DefaultAxis(0, 1, 0);
        FQuat JointWorldRot;

        float DotBoneDir = FVector::Dot(DefaultAxis, ChildBoneDirection);
        if (DotBoneDir > 0.9999f)
        {
            JointWorldRot = FQuat::Identity();
        }
        else if (DotBoneDir < -0.9999f)
        {
            JointWorldRot = FQuat::FromAxisAngle(FVector(0, 0, 1), PI);
        }
        else
        {
            FVector Axis = FVector::Cross(DefaultAxis, ChildBoneDirection).GetNormalized();
            float Angle = acos(FMath::Clamp(DotBoneDir, -1.0f, 1.0f));
            JointWorldRot = FQuat::FromAxisAngle(Axis, Angle);
        }

        // === 본 월드 회전 추출 ===
        FQuat ChildBoneWorldRot = FQuat(ChildBone.BindPose).GetNormalized();
        FQuat ParentBoneWorldRot = FQuat(ParentBone.BindPose).GetNormalized();

        // === Rotation1: Child 본 로컬에서 Joint 프레임 회전 ===
        // Rotation1 = ChildBoneWorldRot.Inverse() * JointWorldRot
        FQuat Rot1 = ChildBoneWorldRot.Inverse() * JointWorldRot;
        CI.Rotation1 = Rot1.ToEulerZYXDeg();

        // === Rotation2: Parent 본 로컬에서 Joint 프레임 회전 ===
        // Rotation2 = ParentBoneWorldRot.Inverse() * JointWorldRot
        FQuat Rot2 = ParentBoneWorldRot.Inverse() * JointWorldRot;
        CI.Rotation2 = Rot2.ToEulerZYXDeg();

        if (DistanceToChild > KINDA_SMALL_NUMBER)
        {
            // Position2: Parent 로컬에서 Joint(=Child) 위치
            FVector DirNormalized = DirToChildGlobal / DistanceToChild;
            FVector DirLocal = BoneUtils::TransformDirection(ParentBone.InverseBindPose, DirNormalized);
            DirLocal = DirLocal.GetNormalized();
            CI.Position2 = DirLocal * DistanceToChild;
        }
        else
        {
            CI.Position2 = FVector::Zero();
        }

        Asset->Constraints.Add(CI);
    }
}

void SPhysicsAssetEditorWindow::DoGenerateAllBodies()
{
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || !PhysState->EditingAsset || !PhysState->CurrentMesh) return;

    const FSkeleton* Skeleton = PhysState->CurrentMesh->GetSkeleton();
    if (!Skeleton || Skeleton->Bones.empty()) return;

    UPhysicsAsset* Asset = PhysState->EditingAsset;

    // 기존 데이터 클리어 (UObject는 ObjectFactory로 삭제)
    for (UBodySetup* Body : Asset->Bodies)
    {
        if (Body) ObjectFactory::DeleteObject(Body);
    }
    Asset->Bodies.Empty();
    Asset->Constraints.Empty();

    // BoneUtils 상수 사용
    using namespace BoneUtils;

    // === Phase 1: Bodies 생성 ===
    for (int32 i = 0; i < Skeleton->Bones.Num(); ++i)
    {
        const FBone& Bone = Skeleton->Bones[i];

        // 손가락/발가락/말단 본 스킵
        if (ShouldSkipBone(Bone.Name))
        {
            continue;
        }

        // 본 길이 및 로컬 방향 계산
        FVector BoneLocalDir;
        float BoneLength = CalculateBoneLengthForGenerate(*Skeleton, i, BoneLocalDir);

        // 본 타입 감지
        bool bIsHandBone = IsHandBone(Bone.Name);
        bool bIsHeadBone = IsHeadBone(Bone.Name);
        bool bIsSpineBone = IsSpineOrPelvisBone(Bone.Name);
        bool bIsNeckBone = IsNeckBone(Bone.Name);
        bool bIsShoulderBone = IsShoulderBone(Bone.Name);
        bool bIsFootBone = IsFootBone(Bone.Name);

        // Hand 본은 고정 크기 사용
        if (bIsHandBone)
        {
            BoneLength = HandBoneSize;
        }

        // Head 본은 고정 크기 사용 (자식이 없어도 생성)
        if (bIsHeadBone)
        {
            BoneLength = HeadBoneSize;
        }

        // 너무 작은 본은 스킵 (Hand, Head 제외)
        if (BoneLength < MinBoneSize && !bIsHandBone && !bIsHeadBone)
        {
            continue;
        }

        // 클램프
        float ClampedLength = std::min(BoneLength, MaxBoneSize);

        // 비율 결정
        float EffectiveRadiusRatio = RadiusRatio;
        float EffectiveLengthRatio = LengthRatio;

        if (bIsHandBone) EffectiveRadiusRatio = HandRadiusRatio;
        else if (bIsHeadBone) EffectiveRadiusRatio = HeadRadiusRatio;
        else if (bIsSpineBone) { EffectiveRadiusRatio = SpineRadiusRatio; EffectiveLengthRatio = SpineLengthRatio; }
        else if (bIsNeckBone) { EffectiveRadiusRatio = NeckRadiusRatio; EffectiveLengthRatio = NeckLengthRatio; }
        else if (bIsShoulderBone) EffectiveRadiusRatio = ShoulderRadiusRatio;
        else if (bIsFootBone) EffectiveRadiusRatio = FootRadiusRatio;

        float ShapeRadius = ClampedLength * EffectiveRadiusRatio;
        float ShapeLength = ClampedLength * EffectiveLengthRatio;

        // BodySetup 생성
        UBodySetup* BodySetup = NewObject<UBodySetup>();
        BodySetup->BoneName = FName(Bone.Name);

        // 선택된 프리미티브 타입에 따라 Shape 생성
        switch (SelectedPrimitiveType)
        {
        case EShapeType::Capsule:
        {
            // === 캡슐: 본 방향 기반 자동 정렬 ===
            FKSphylElem Capsule;
            Capsule.Name = FName(Bone.Name + "_sphyl");
            Capsule.Radius = ShapeRadius;
            Capsule.Length = ShapeLength;

            // Center: 본 방향을 따라 본 길이의 절반 위치
            if (bIsHeadBone)
            {
                // Head: 월드 Z축(위쪽)을 본 로컬 좌표로 변환하여 오프셋
                FVector WorldUp(0.0f, 0.0f, 1.0f);
                FVector LocalUp = BoneUtils::TransformDirection(Bone.InverseBindPose, WorldUp).GetNormalized();
                Capsule.Center = LocalUp * HeadCenterOffset;
                // 캡슐이 위쪽을 향하도록 회전
                Capsule.Rotation = FQuat::MakeFromEulerZYX(CalculateCapsuleRotationFromBoneDir(LocalUp));
            }
            else if (bIsSpineBone)
            {
                // Spine/Pelvis/Chest: 월드 Y축(좌우)을 본 로컬 좌표로 변환하여 가로 배치
                FVector WorldRight(0.0f, 1.0f, 0.0f);
                FVector LocalRight = BoneUtils::TransformDirection(Bone.InverseBindPose, WorldRight).GetNormalized();
                Capsule.Center = FVector::Zero();  // 본 원점에 배치
                // 캡슐이 좌우 방향으로 눕도록 회전
                Capsule.Rotation = FQuat::MakeFromEulerZYX(CalculateCapsuleRotationFromBoneDir(LocalRight));
            }
            else
            {
                Capsule.Center = BoneLocalDir * (ClampedLength * 0.5f);
                // 본의 로컬 방향에서 캡슐 회전값 자동 계산
                Capsule.Rotation = FQuat::MakeFromEulerZYX(CalculateCapsuleRotationFromBoneDir(BoneLocalDir));
            }

            BodySetup->AggGeom.SphylElems.Add(Capsule);
            break;
        }
        case EShapeType::Sphere:
        {
            // === 구: 본 방향 기반 자동 정렬 ===
            FKSphereElem Sphere;
            Sphere.Name = FName(Bone.Name + "_sphere");
            Sphere.Radius = ShapeRadius * 1.2f;

            // Center: 본 방향을 따라 본 길이의 절반 위치
            if (bIsHeadBone)
            {
                // Head: 월드 Z축(위쪽)을 본 로컬 좌표로 변환하여 오프셋
                FVector WorldUp(0.0f, 0.0f, 1.0f);
                FVector LocalUp = BoneUtils::TransformDirection(Bone.InverseBindPose, WorldUp).GetNormalized();
                Sphere.Center = LocalUp * HeadCenterOffset;
            }
            else
            {
                Sphere.Center = BoneLocalDir * (ClampedLength * 0.5f);
            }

            BodySetup->AggGeom.SphereElems.Add(Sphere);
            break;
        }
        case EShapeType::Box:
        {
            // === 박스: 본 방향 기반 자동 정렬 ===
            FKBoxElem Box;
            Box.Name = FName(Bone.Name + "_box");

            // Center: 본 방향을 따라 본 길이의 절반 위치
            if (bIsHeadBone)
            {
                // Head: 정육면체에 가까운 박스
                float HeadBoxSize = ShapeRadius;
                Box.X = HeadBoxSize;
                Box.Y = HeadBoxSize;
                Box.Z = HeadBoxSize;

                // Head: 월드 Z축(위쪽)을 본 로컬 좌표로 변환하여 오프셋
                FVector WorldUp(0.0f, 0.0f, 1.0f);
                FVector LocalUp = BoneUtils::TransformDirection(Bone.InverseBindPose, WorldUp).GetNormalized();
                Box.Center = LocalUp * HeadCenterOffset;
                Box.Rotation = FQuat::MakeFromEulerZYX(FVector(0.0f, 0.0f, 0.0f));  // 회전 없음
            }
            else if (bIsSpineBone)
            {
                // 박스 크기: Z축이 길이 방향
                Box.X = ShapeRadius;
                Box.Y = ShapeRadius;
                Box.Z = ShapeLength * 0.5f;

                // Spine/Pelvis/Chest: 월드 Y축(좌우)을 본 로컬 좌표로 변환하여 가로 배치
                FVector WorldRight(0.0f, 1.0f, 0.0f);
                FVector LocalRight = BoneUtils::TransformDirection(Bone.InverseBindPose, WorldRight).GetNormalized();
                Box.Center = FVector::Zero();  // 본 원점에 배치
                // 박스가 좌우 방향으로 눕도록 회전
                Box.Rotation = FQuat::MakeFromEulerZYX(CalculateBoxRotationFromBoneDir(LocalRight));
            }
            else
            {
                // 박스 크기: Z축이 길이 방향
                Box.X = ShapeRadius;
                Box.Y = ShapeRadius;
                Box.Z = ShapeLength * 0.5f;

                Box.Center = BoneLocalDir * (ClampedLength * 0.5f);
                // 본의 로컬 방향에서 박스 회전값 자동 계산
                Box.Rotation = FQuat::MakeFromEulerZYX(CalculateBoxRotationFromBoneDir(BoneLocalDir));
            }

            BodySetup->AggGeom.BoxElems.Add(Box);
            break;
        }
        default:
            continue;
        }

        // 기본 물리 속성 (MassInKg는 부피 기반으로 계산)
        UpdateBodyMassFromVolume(BodySetup);
        // PhysMaterial은 None (nullptr) - 필요 시 에셋 선택
        BodySetup->PhysMaterial = nullptr;
        BodySetup->PhysMaterialPath = "";
        BodySetup->LinearDamping = 0.1f;
        BodySetup->AngularDamping = 0.1f;

        Asset->Bodies.Add(BodySetup);
    }

    // === Phase 2: Constraints 생성 ===
    GenerateConstraintsForBodies();

    // Dirty 플래그 설정
    PhysState->bIsDirty = true;

    // 선택 초기화
    ClearSelection();

    UE_LOG("Generated %d bodies and %d constraints", Asset->Bodies.Num(), Asset->Constraints.Num());
}

// ============================================================================
// Physics Asset 에디터 월드 새로고침
// ============================================================================

void SPhysicsAssetEditorWindow::RefreshPhysicsAssetInWorld(UPhysicsAsset* Asset)
{
    // Week_Final_5에는 GetEffectivePhysicsAssetPath, SetPhysicsAssetPathOverride 등이 없음
    // 에디터 전용 기능이므로 제거
    PhysicsAssetEditorState* PhysState = GetPhysicsState();
    if (!PhysState || PhysState->CurrentFilePath.empty()) return;

    UE_LOG("[PhysicsAsset] RefreshPhysicsAssetInWorld - Week_Final_5: 자동 갱신 비활성화");
}
