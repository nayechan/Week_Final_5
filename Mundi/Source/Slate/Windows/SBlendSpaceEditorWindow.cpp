#include "pch.h"
#include "SBlendSpaceEditorWindow.h"
#include "SlateManager.h"
#include "Source/Runtime/Engine/Viewer/BlendSpaceEditorBootstrap.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/GameFramework/SkeletalMeshActor.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Renderer/FViewport.h"
#include "Source/Editor/PlatformProcess.h"
#include "AnimBlendSpaceInstance.h"
#include "AnimSequence.h"
#include "ResourceManager.h"

SBlendSpaceEditorWindow::SBlendSpaceEditorWindow()
{
    CenterRect = FRect(0, 0, 0, 0);
    LeftPanelRatio = 0.2f;   // Match animation viewer
    RightPanelRatio = 0.2f;  // Match animation viewer
    CanvasHeightRatio = 0.35f;
}

SBlendSpaceEditorWindow::~SBlendSpaceEditorWindow()
{
    for (int i = 0; i < Tabs.Num(); ++i)
    {
        ViewerState* State = Tabs[i];
        BlendSpaceEditorBootstrap::DestroyViewerState(State);
    }
    Tabs.Empty();
    ActiveState = nullptr;
}

void SBlendSpaceEditorWindow::OnRender()
{
    if (!ImGui::GetCurrentContext()) return;

    if (!bInitialPlacementDone)
    {
        ImGui::SetNextWindowPos(ImVec2(Rect.Left, Rect.Top));
        ImGui::SetNextWindowSize(ImVec2(Rect.GetWidth(), Rect.GetHeight()));
        bInitialPlacementDone = true;
    }
    if (bRequestFocus)
    {
        ImGui::SetNextWindowFocus();
        bRequestFocus = false;
    }

    char UniqueTitle[256];
    const FString Title = GetWindowTitle();
    sprintf_s(UniqueTitle, sizeof(UniqueTitle), "%s###%p", Title.c_str(), this);

    bool bVisible = false;
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    if (ImGui::Begin(UniqueTitle, &bIsOpen, flags))
    {
        bVisible = true;
        bIsWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        bIsWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // Render tabs and toolbar (synced with other viewers)
        RenderTabsAndToolbar(EViewerType::BlendSpace);

        ImVec2 pos = ImGui::GetWindowPos();
        ImVec2 size = ImGui::GetWindowSize();
        Rect.Left = pos.x; Rect.Top = pos.y; Rect.Right = pos.x + size.x; Rect.Bottom = pos.y + size.y;
        Rect.UpdateMinMax();

        ImVec2 avail = ImGui::GetContentRegionAvail();
        const float totalW = avail.x;
        const float totalH = avail.y;
        const float splitter = 4.f;

        // Panel widths (full height for left and right)
        float leftW = totalW * LeftPanelRatio;
        float rightW = totalW * RightPanelRatio;
        float centerW = totalW - leftW - rightW - (splitter * 2);
        if (centerW < 0.f) centerW = 0.f;

        // Center panel heights (viewport on top, canvas on bottom)
        const float canvasH = totalH * CanvasHeightRatio;
        const float viewportH = totalH - canvasH - splitter;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        // ===== Left Panel (full height) =====
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("LeftPanel", ImVec2(leftW, totalH), true, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleVar();
        RenderLeftPanel(leftW);
        ImGui::EndChild();

        ImGui::SameLine(0, 0);

        // Left splitter
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##LeftSplitter", ImVec2(splitter, totalH));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            LeftPanelRatio = FMath::Clamp(LeftPanelRatio + delta / totalW, 0.1f, 0.45f);
        }

        ImGui::SameLine(0, 0);

        // ===== Center Panel (viewport + canvas) =====
        if (centerW > 0.0f)
        {
            ImGui::BeginGroup();

            // Top: Viewport
            ImGui::BeginChild("CenterViewport", ImVec2(centerW, viewportH), true, ImGuiWindowFlags_NoScrollbar);
            if (ActiveState)
            {
                PreRenderViewportUpdate();
            }
            RenderCenterViewport(centerW, viewportH);
            ImGui::EndChild();

            // Horizontal splitter between viewport and canvas
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
            ImGui::Button("##CanvasSplitter", ImVec2(centerW, splitter));
            ImGui::PopStyleColor(3);

            if (ImGui::IsItemHovered())
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

            if (ImGui::IsItemActive())
            {
                float delta = ImGui::GetIO().MouseDelta.y;
                CanvasHeightRatio = FMath::Clamp(CanvasHeightRatio - delta / totalH, 0.2f, 0.6f);
            }

            // Bottom: Canvas
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
            ImGui::BeginChild("BlendCanvas", ImVec2(centerW, canvasH), true, ImGuiWindowFlags_NoScrollbar);
            ImGui::PopStyleVar();
            ImGui::TextUnformatted("Blend Space Canvas");
            ImGui::Separator();
            ImVec2 canvasRegion = ImGui::GetContentRegionAvail();
            float canvasContentW = canvasRegion.x;
            float canvasContentH = canvasRegion.y;
            if (canvasContentH < 150.0f) canvasContentH = 150.0f;
            if (canvasContentW < 150.0f) canvasContentW = 150.0f;
            RenderBlendCanvas(canvasContentW, canvasContentH);
            ImGui::EndChild();

            ImGui::EndGroup();
        }
        else
        {
            // Panel is collapsed - clear viewport rect
            CenterRect = FRect(0, 0, 0, 0);
            CenterRect.UpdateMinMax();
        }

        ImGui::SameLine(0, 0);

        // Right splitter
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.4f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 0.9f));
        ImGui::Button("##RightSplitter", ImVec2(splitter, totalH));
        ImGui::PopStyleColor(3);

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

        if (ImGui::IsItemActive())
        {
            float delta = ImGui::GetIO().MouseDelta.x;
            RightPanelRatio = FMath::Clamp(RightPanelRatio - delta / totalW, 0.1f, 0.45f);
        }

        ImGui::SameLine(0, 0);

        // ===== Right Panel (full height) =====
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::BeginChild("RightPanel", ImVec2(rightW, totalH), true);
        ImGui::PopStyleVar();

        // BlendSpace-specific controls
        ImGui::Dummy(ImVec2(0,6));
        ImGui::TextUnformatted("Preview Binding");
        ImGui::Separator();
        if (ImGui::Button("Attach to Preview"))
        {
            if (ActiveState && ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
            {
                USkeletalMeshComponent* Comp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
                Comp->UseBlendSpace2D();
                BlendInst = Comp->GetOrCreateBlendSpace2D();
            }
        }
        if (BlendInst)
        {
            ImGui::SameLine(); ImGui::TextColored(ImVec4(0.4f,1.0f,0.4f,1.0f), "Attached");
        }

        ImGui::Dummy(ImVec2(0,6));
        ImGui::TextUnformatted("Blend Controls");
        ImGui::Separator();
        ImGui::DragFloat("Param X", &UI_BlendX, 0.01f, -FLT_MAX, FLT_MAX, "%.2f");
        ImGui::DragFloat("Param Y", &UI_BlendY, 0.01f, -FLT_MAX, FLT_MAX, "%.2f");
        ImGui::DragFloat("Play Rate", &UI_PlayRate, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::Checkbox("Looping", &UI_Looping);
        ImGui::DragInt("Driver Index", &UI_DriverIndex, 0.1f, 0, 100);
        if (BlendInst)
        {
            BlendInst->SetBlendPosition(FVector2D(UI_BlendX, UI_BlendY));
            BlendInst->SetPlayRate(UI_PlayRate);
            BlendInst->SetLooping(UI_Looping);
            BlendInst->SetDriverIndex(UI_DriverIndex);
        }

        ImGui::Separator();

        // Animation browser with height constraint
        ImGui::BeginChild("AnimBrowserArea", ImVec2(0, 300.0f), false);
        RenderAnimationBrowser(
            // OnAnimationSelected callback
            [this](UAnimSequence* Anim) {
                if (Anim)
                {
                    // Get the animation file path and copy to UI_SamplePath
                    FString animPath = Anim->GetFilePath();
                    strncpy_s(UI_SamplePath, animPath.c_str(), sizeof(UI_SamplePath) - 1);
                }
            },
            // IsAnimationSelected predicate
            [this](UAnimSequence* Anim) -> bool {
                if (!Anim || UI_SamplePath[0] == '\0') return false;
                // Check if this animation's path matches UI_SamplePath
                return Anim->GetFilePath() == FString(UI_SamplePath);
            }
        );
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 6));

        ImGui::DragFloat("Rate", &UI_SampleRate, 0.01f, 0.0f, 10.0f, "%.2f");
        // Auto-attach a blend space instance if not attached yet
        if (!BlendInst && ActiveState && ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
        {
            USkeletalMeshComponent* Comp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
            Comp->UseBlendSpace2D();
            BlendInst = Comp->GetOrCreateBlendSpace2D();
        }
        ImGui::DragFloat("X", &UI_SampleX, 0.01f, -FLT_MAX, FLT_MAX, "%.2f");
        ImGui::DragFloat("Y", &UI_SampleY, 0.01f, -FLT_MAX, FLT_MAX, "%.2f");
        ImGui::Checkbox("Loop", &UI_SampleLoop);
        if (ImGui::Button("Add Sample") && BlendInst && UI_SamplePath[0] != '\0')
        {
            // Use preloaded resources (no Load here) – matches AnimationViewerBootstrap pattern
            UAnimSequence* Seq = UResourceManager::GetInstance().Get<UAnimSequence>(UI_SamplePath);
            if (!Seq)
            {
                // Log available animations to help users find the correct key
                const auto& AllAnims = UResourceManager::GetInstance().GetAnimations();
                UE_LOG("[BlendSpaceEditor] Failed to get animation: %s", UI_SamplePath);
                UE_LOG("[BlendSpaceEditor] Available animations:");
                for (const UAnimSequence* Anim : AllAnims)
                {
                    if (Anim)
                    {
                        UE_LOG("  - %s", Anim->GetFilePath().c_str());
                    }
                }
            }
            else
            {
                BlendInst->AddSample(FVector2D(UI_SampleX, UI_SampleY), Seq, UI_SampleRate, UI_SampleLoop);
                RebuildTriangles();
            }
        }
        
        ImGui::EndChild();

        // End the ItemSpacing override
        ImGui::PopStyleVar();
    }
    ImGui::End();

    // If collapsed or not visible, clear the center rect so we don't render a floating viewport
    if (!bVisible)
    {
        CenterRect = FRect(0, 0, 0, 0);
        CenterRect.UpdateMinMax();
        bIsWindowHovered = false;
        bIsWindowFocused = false;
    }

    // If window was closed via X button, notify the manager to clean up
    if (!bIsOpen)
    {
        USlateManager::GetInstance().RequestCloseDetachedWindow(this);
    }

    bRequestFocus = false;
}

void SBlendSpaceEditorWindow::RenderCenterViewport(float Width, float Height)
{
    // Render viewer toolbar (synced with other viewers)
    RenderViewerToolbar();

    // Get position where viewport will be rendered
    ImVec2 Pos = ImGui::GetCursorScreenPos();

    // Get actual available space inside the child window
    ImVec2 availRegion = ImGui::GetContentRegionAvail();
    float actualWidth = availRegion.x;
    float actualHeight = availRegion.y;

    // Reserve space (don't render anything here)
    ImGui::Dummy(ImVec2(actualWidth, actualHeight));

    // Update viewport rect for input handling
    CenterRect.Left = Pos.x;
    CenterRect.Top = Pos.y;
    CenterRect.Right = Pos.x + actualWidth;
    CenterRect.Bottom = Pos.y + actualHeight;
    CenterRect.UpdateMinMax();

    // Register D3D viewport rendering callback
    // This callback executes during ImGui rendering to draw the 3D viewport
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddCallback(ViewportRenderCallback, this);

    // Restore ImGui render state after D3D viewport rendering
    drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
}

ViewerState* SBlendSpaceEditorWindow::CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context)
{
    return BlendSpaceEditorBootstrap::CreateViewerState(Name, World, Device);
}

void SBlendSpaceEditorWindow::DestroyViewerState(ViewerState*& State)
{
    BlendSpaceEditorBootstrap::DestroyViewerState(State);
}

// ImGui draw callback - Direct3D viewport rendering
void SBlendSpaceEditorWindow::ViewportRenderCallback(const ImDrawList* parent_list, const ImDrawCmd* cmd)
{
    // Get this pointer from UserCallbackData
    SBlendSpaceEditorWindow* window = (SBlendSpaceEditorWindow*)cmd->UserCallbackData;

    if (window && window->ActiveState && window->ActiveState->Viewport)
    {
        FViewport* viewport = window->ActiveState->Viewport;

        // Get D3D device context
        ID3D11Device* device = window->Device;
        ID3D11DeviceContext* context = nullptr;
        device->GetImmediateContext(&context);

        if (context)
        {
            // Restore D3D state for 3D rendering (ImGui changes it)
            // 1. Set viewport
            D3D11_VIEWPORT d3dViewport = {};
            d3dViewport.Width = static_cast<float>(viewport->GetSizeX());
            d3dViewport.Height = static_cast<float>(viewport->GetSizeY());
            d3dViewport.MinDepth = 0.0f;
            d3dViewport.MaxDepth = 1.0f;
            d3dViewport.TopLeftX = static_cast<float>(viewport->GetStartX());
            d3dViewport.TopLeftY = static_cast<float>(viewport->GetStartY());
            context->RSSetViewports(1, &d3dViewport);

            // 2. Restore D3D render state (ImGui → 3D rendering)
            float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            context->OMSetBlendState(nullptr, blendFactor, 0xffffffff);  // Disable blending
            context->OMSetDepthStencilState(nullptr, 0);                  // Default depth test
            context->RSSetState(nullptr);                                 // Default rasterizer
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // 3. Execute viewport rendering
            window->OnRenderViewport();

            context->Release();
        }
    }
}

// ==== Blend Space Canvas Rendering & Interaction ====
void SBlendSpaceEditorWindow::RenderBlendCanvas(float Width, float Height)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size(Width, Height);

    // Reserve space for the canvas
    ImGui::InvisibleButton("CanvasArea", size);

    // Background and frame
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImColor(28, 28, 28, 255), 6.0f);
    dl->AddRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), ImColor(60, 60, 60, 255), 6.0f, 0, 1.5f);

    const ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;
    bool hovered = ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + size.x, origin.y + size.y));

    auto ParamToScreen = [&](const FVector2D& P) -> ImVec2
    {
        float sx = origin.x + (P.X - CanvasPan.X) * CanvasZoom;
        float sy = origin.y + (-(P.Y - CanvasPan.Y)) * CanvasZoom;
        return ImVec2(sx, sy);
    };
    auto ScreenToParam = [&](const ImVec2& S) -> FVector2D
    {
        float px = ((S.x - origin.x) / CanvasZoom) + CanvasPan.X;
        float py = -((S.y - origin.y) / CanvasZoom) + CanvasPan.Y;
        return FVector2D(px, py);
    };

    // Pan with middle mouse
    if (hovered && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        ImVec2 delta = io.MouseDelta;
        CanvasPan.X -= delta.x / CanvasZoom;
        CanvasPan.Y += delta.y / CanvasZoom;
    }
    // Zoom with Ctrl + wheel
    if (hovered && (io.KeyCtrl) && io.MouseWheel != 0.0f)
    {
        float prevZoom = CanvasZoom;
        CanvasZoom *= (io.MouseWheel > 0.0f) ? 1.1f : 0.9f;
        CanvasZoom = FMath::Clamp(CanvasZoom, 60.0f, 400.0f);
        // Keep mouse stationary in param space (approx)
        FVector2D before = ScreenToParam(mouse);
        FVector2D after = ScreenToParam(mouse);
        CanvasPan.X += before.X - after.X;
        CanvasPan.Y += before.Y - after.Y;
    }

    // Grid
    const float step = 0.2f;
    ImU32 gridCol = ImColor(50, 50, 50, 255);
    auto S2P = [&](float sx, float sy){ return ScreenToParam(ImVec2(sx, sy)); };
    FVector2D pTL = S2P(origin.x, origin.y);
    FVector2D pBR = S2P(origin.x + size.x, origin.y + size.y);
    int minX = (int)std::floor(std::min(pTL.X, pBR.X) / step) - 1;
    int maxX = (int)std::ceil(std::max(pTL.X, pBR.X) / step) + 1;
    int minY = (int)std::floor(std::min(pTL.Y, pBR.Y) / step) - 1;
    int maxY = (int)std::ceil(std::max(pTL.Y, pBR.Y) / step) + 1;
    for (int gx = minX; gx <= maxX; ++gx)
    {
        float x = gx * step;
        ImVec2 s0 = ParamToScreen(FVector2D(x, pTL.Y));
        ImVec2 s1 = ParamToScreen(FVector2D(x, pBR.Y));
        dl->AddLine(s0, s1, gridCol, 1.0f);
    }
    for (int gy = minY; gy <= maxY; ++gy)
    {
        float y = gy * step;
        ImVec2 s0 = ParamToScreen(FVector2D(pTL.X, y));
        ImVec2 s1 = ParamToScreen(FVector2D(pBR.X, y));
        dl->AddLine(s0, s1, gridCol, 1.0f);
    }

    // Axes
    ImU32 axisCol = ImColor(110, 110, 110, 255);
    ImVec2 ax0 = ParamToScreen(FVector2D(0, pTL.Y));
    ImVec2 ax1 = ParamToScreen(FVector2D(0, pBR.Y));
    dl->AddLine(ax0, ax1, axisCol, 2.0f);
    ImVec2 ay0 = ParamToScreen(FVector2D(pTL.X, 0));
    ImVec2 ay1 = ParamToScreen(FVector2D(pBR.X, 0));
    dl->AddLine(ay0, ay1, axisCol, 2.0f);

    // Refresh BlendInst pointer to ensure it's still valid after mesh changes
    if (ActiveState && ActiveState->PreviewActor && ActiveState->PreviewActor->GetSkeletalMeshComponent())
    {
        USkeletalMeshComponent* Comp = ActiveState->PreviewActor->GetSkeletalMeshComponent();
        // Only use the blend space instance if it's still the current anim instance
        if (Comp->GetAnimInstance() && Comp->GetAnimInstance() == BlendInst)
        {
            // BlendInst is still valid
        }
        else
        {
            // AnimInstance changed - need to get the new blend space instance if it exists
            BlendInst = Cast<UAnimBlendSpaceInstance>(Comp->GetAnimInstance());
        }
    }
    else
    {
        BlendInst = nullptr;
    }

    if (!BlendInst)
        return;

    const auto& Samples = BlendInst->GetSamples();
    const auto& Tris = BlendInst->GetTriangles();
    // Triangles
    for (const auto& T : Tris)
    {
        if (T.I0 < 0 || T.I1 < 0 || T.I2 < 0) continue;
        if (T.I0 >= Samples.Num() || T.I1 >= Samples.Num() || T.I2 >= Samples.Num()) continue;
        ImVec2 s0 = ParamToScreen(Samples[T.I0].Position);
        ImVec2 s1 = ParamToScreen(Samples[T.I1].Position);
        ImVec2 s2 = ParamToScreen(Samples[T.I2].Position);
        dl->AddTriangle(s0, s1, s2, ImColor(90, 120, 190, 180), 1.5f);
    }

    // Samples
    const float r = 5.0f;
    HoveredSample = -1;
    for (int i = 0; i < Samples.Num(); ++i)
    {
        ImVec2 sp = ParamToScreen(Samples[i].Position);
        bool isHover = hovered && ImGui::IsMouseHoveringRect(ImVec2(sp.x - 8, sp.y - 8), ImVec2(sp.x + 8, sp.y + 8));
        if (isHover) HoveredSample = i;
        ImU32 col = (i == SelectedSample) ? ImColor(255, 200, 60) : (isHover ? ImColor(255, 150, 60) : ImColor(230, 230, 230));
        dl->AddCircleFilled(sp, r, col);
        char buf[16]; sprintf_s(buf, "%d", i);
        dl->AddText(ImVec2(sp.x + 8, sp.y - 8), ImColor(230, 230, 230), buf);
    }

    // Interactions
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        if (HoveredSample != -1)
        {
            SelectedSample = HoveredSample;
            bDraggingSample = true;
        }
        else
        {
            bDraggingBlendPoint = true;
        }
    }
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        bDraggingSample = false;
        bDraggingBlendPoint = false;
    }
    if (hovered && bDraggingSample && SelectedSample != -1)
    {
        FVector2D p = ScreenToParam(mouse);
        if (io.KeyShift)
        {
            const float snap = 0.1f;
            p.X = std::round(p.X / snap) * snap;
            p.Y = std::round(p.Y / snap) * snap;
        }
        BlendInst->SetSamplePosition(SelectedSample, p);
        RebuildTriangles();
    }
    if (hovered && bDraggingBlendPoint)
    {
        FVector2D p = ScreenToParam(mouse);
        if (io.KeyShift)
        {
            const float snap = 0.1f;
            p.X = std::round(p.X / snap) * snap;
            p.Y = std::round(p.Y / snap) * snap;
        }
        UI_BlendX = p.X; UI_BlendY = p.Y;
    }

    // Draw current blend position
    ImVec2 bp = ParamToScreen(FVector2D(UI_BlendX, UI_BlendY));
    dl->AddCircle(bp, 6.0f, ImColor(100, 220, 120), 0, 2.0f);
    dl->AddCircleFilled(bp, 3.0f, ImColor(100, 220, 120));
}

// Build a simple Delaunay triangulation (naive O(n^4)) for small sample counts
void SBlendSpaceEditorWindow::RebuildTriangles()
{
    if (!BlendInst) return;
    const auto& Samples = BlendInst->GetSamples();
    const int n = Samples.Num();
    BlendInst->ClearTriangles();
    if (n < 3) return;

    auto CircumCircle = [](const FVector2D& A, const FVector2D& B, const FVector2D& C, FVector2D& Center, float& Radius2)->bool
    {
        // Compute circumcircle via perpendicular bisectors
        float ax = A.X, ay = A.Y;
        float bx = B.X, by = B.Y;
        float cx = C.X, cy = C.Y;

        float d = 2.0f * (ax*(by - cy) + bx*(cy - ay) + cx*(ay - by));
        if (std::fabs(d) < 1e-8f) return false; // Degenerate

        float ax2ay2 = ax*ax + ay*ay;
        float bx2by2 = bx*bx + by*by;
        float cx2cy2 = cx*cx + cy*cy;

        float ux = (ax2ay2*(by - cy) + bx2by2*(cy - ay) + cx2cy2*(ay - by)) / d;
        float uy = (ax2ay2*(cx - bx) + bx2by2*(ax - cx) + cx2cy2*(bx - ax)) / d;
        Center = FVector2D(ux, uy);
        float dx = ux - ax, dy = uy - ay;
        Radius2 = dx*dx + dy*dy;
        return true;
    };

    struct TriKey { int i,j,k; bool operator<(const TriKey& o) const { if (i!=o.i) return i<o.i; if (j!=o.j) return j<o.j; return k<o.k; } };
    std::set<TriKey> triSet;

    for (int i=0;i<n-2;++i)
    for (int j=i+1;j<n-1;++j)
    for (int k=j+1;k<n;++k)
    {
        FVector2D center; float r2;
        if (!CircumCircle(Samples[i].Position, Samples[j].Position, Samples[k].Position, center, r2))
            continue;

        bool anyInside = false;
        for (int m=0;m<n;++m)
        {
            if (m==i || m==j || m==k) continue;
            float dx = Samples[m].Position.X - center.X;
            float dy = Samples[m].Position.Y - center.Y;
            float d2 = dx*dx + dy*dy;
            if (d2 <= r2 - 1e-6f) { anyInside = true; break; }
        }
        if (!anyInside)
        {
            TriKey key{ i,j,k };
            triSet.insert(key);
        }
    }

    for (const auto& key : triSet)
    {
        BlendInst->AddTriangle(key.i, key.j, key.k);
    }
}
