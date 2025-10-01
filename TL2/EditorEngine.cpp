#include "pch.h"
#include "EditorEngine.h"
#include "USlateManager.h"
#include <ObjManager.h>

float UEditorEngine::ClientWidth = 1024.0f;
float UEditorEngine::ClientHeight = 1024.0f;

static void LoadIniFile()
{
    std::ifstream infile("editor.ini");
    if (!infile.is_open()) return;

    std::string line;
    while (std::getline(infile, line))
    {
        if (line.empty() || line[0] == ';') continue;
        size_t delimiterPos = line.find('=');
        if (delimiterPos != FString::npos)
        {
            FString key = line.substr(0, delimiterPos);
            std::string value = line.substr(delimiterPos + 1);

            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            EditorINI[key] = value;
        }
    }
}

static void SaveIniFile()
{
    std::ofstream outfile("editor.ini");
    for (const auto& pair : EditorINI)
        outfile << pair.first << " = " << pair.second << std::endl;
}

UEditorEngine::UEditorEngine()
{

}

UEditorEngine::~UEditorEngine()
{
    // ObjManager 정리
    FObjManager::Clear();
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void UEditorEngine::GetViewportSize(HWND hWnd)
{
    RECT clientRect{};
    GetClientRect(hWnd, &clientRect);

    ClientWidth = static_cast<float>(clientRect.right - clientRect.left);
    ClientHeight = static_cast<float>(clientRect.bottom - clientRect.top);

    if (ClientWidth <= 0) ClientWidth = 1;
    if (ClientHeight <= 0) ClientHeight = 1;

    //레거시
    extern float CLIENTWIDTH;
    extern float CLIENTHEIGHT;
    
    CLIENTWIDTH = ClientWidth;
    CLIENTHEIGHT = ClientHeight;
}

LRESULT CALLBACK UEditorEngine::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Input first
    INPUT.ProcessMessage(hWnd, message, wParam, lParam);

    // ImGui next
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    switch (message)
    {
    case WM_SIZE:
    {
        WPARAM sizeType = wParam;
        if (sizeType != SIZE_MINIMIZED)
        {
            GetViewportSize(hWnd);

            UINT newWidth = static_cast<UINT>(ClientWidth);
            UINT newHeight = static_cast<UINT>(ClientHeight);
            GEngine.RHIDevice.ResizeSwapChain(newWidth, newHeight);
            EditorINI["WindowWidth"] = std::to_string(newWidth);
            EditorINI["WindowHeight"] = std::to_string(newHeight);

            if (ImGui::GetCurrentContext() != nullptr) 
            {
                ImGuiIO& io = ImGui::GetIO();
                if (io.DisplaySize.x > 0 && io.DisplaySize.y > 0) 
                {
                    UI.RepositionImGuiWindows();
                }
            }
        }
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

UWorld* UEditorEngine::GetDefaultWorld()
{
    if (!WorldContexts.IsEmpty() && WorldContexts[0].World)
    {
        return WorldContexts[0].World;
    }
    return nullptr;
}

bool UEditorEngine::CreateMainWindow(HINSTANCE hInstance)
{
    // 윈도우 생성
    WCHAR WindowClass[] = L"JungleWindowClass";
    WCHAR Title[] = L"Game Tech Lab";

    WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };
    RegisterClassW(&wndclass);

    int windowWidth = 1620, windowHeight = 1024;
    if (EditorINI.count("WindowWidth"))
    {
        try { windowWidth = stoi(EditorINI["WindowWidth"]); } catch (...) {}
    }
    if (EditorINI.count("WindowHeight"))
    {
        try { windowHeight = stoi(EditorINI["WindowHeight"]); } catch (...) {}
    }

    HWnd = CreateWindowExW(0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, nullptr);

    if (!HWnd)
        return false;

    //종횡비 계산
    GetViewportSize(HWnd);
    return true;
}

bool UEditorEngine::Startup(HINSTANCE hInstance)
{
    LoadIniFile();

    if (!CreateMainWindow(hInstance))
        return false;

    //디바이스 리소스 및 렌더러 생성
    RHIDevice.Initialize(HWnd);
    Renderer = std::make_unique<URenderer>(&RHIDevice);

    //매니저 초기화
    UI.Initialize(HWnd, RHIDevice.GetDevice(), RHIDevice.GetDeviceContext());
    INPUT.Initialize(HWnd);

    FObjManager::Preload();

    ///////////////////////////////////
    WorldContexts.Add(FWorldContext(NewObject<UWorld>(), EWorldType::Editor));
    ///////////////////////////////////

    GWorld = WorldContexts[0].World;

    // 슬레이트 매니저 (singleton)
    FRect ScreenRect(0, 0, ClientWidth, ClientHeight);
    SLATE.Initialize(RHIDevice.GetDevice(), GWorld, ScreenRect);

    //스폰을 위한 월드셋
    UI.SetWorld(WorldContexts[0].World);

    bRunning = true;
    return true;
}

void UEditorEngine::Tick(float DeltaSeconds)
{
    //@TODO UV 스크롤 입력 처리 로직 이동
    HandleUVInput(DeltaSeconds);

    for (auto& WorldContext : WorldContexts)
    {
        WorldContext.World->Tick(DeltaSeconds, WorldContext.WorldType);
        //// 테스트용으로 분기해놨음
        //if (WorldContext.World && bPIEActive && WorldContext.WorldType == EWorldType::Game)
        //{
        //    WorldContext.World->Tick(DeltaSeconds, WorldContext.WorldType);
        //}
        //else if (WorldContext.World && !bPIEActive && WorldContext.WorldType == EWorldType::Editor)
        //{
        //    WorldContext.World->Tick(DeltaSeconds, WorldContext.WorldType);
        //}
    }
    
    SLATE.Update(DeltaSeconds);
    UI.Update(DeltaSeconds);
    INPUT.Update();
}

void UEditorEngine::Render()
{
    Renderer->BeginFrame();

    UI.Render();
    SLATE.Render();
    UI.EndFrame();

    Renderer->EndFrame();
}

void UEditorEngine::HandleUVInput(float DeltaSeconds)
{
    UInputManager& InputMgr = UInputManager::GetInstance();
    if (InputMgr.IsKeyPressed('T'))
    {
        bUVScrollPaused = !bUVScrollPaused;
        if (bUVScrollPaused)
        {
            UVScrollTime = 0.0f;
            if (Renderer) Renderer->UpdateUVScroll(UVScrollSpeed, UVScrollTime);
        }
    }
    if (!bUVScrollPaused)
    {
        UVScrollTime += DeltaSeconds;
        if (Renderer) Renderer->UpdateUVScroll(UVScrollSpeed, UVScrollTime);
    }

}

void UEditorEngine::MainLoop()
{
    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);

    LARGE_INTEGER PrevTime, CurrTime;
    QueryPerformanceCounter(&PrevTime);

    MSG msg;

    while (bRunning)
    {
        QueryPerformanceCounter(&CurrTime);
        float DeltaSeconds = static_cast<float>((CurrTime.QuadPart - PrevTime.QuadPart) / double(Frequency.QuadPart));
        PrevTime = CurrTime;

        // 처리할 메시지가 더 이상 없을때 까지 수행
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
            {
                bRunning = false;
                break;
            }
        }

        if (!bRunning) break;

        if (UInputManager::GetInstance().IsKeyPressed(VK_ESCAPE))
        {
            UE_LOG("ESC Key Pressed - Exiting!\n");
            bRunning = false;
        }

        Tick(DeltaSeconds);
        Render();
    }
}

void UEditorEngine::Shutdown()
{
    UUIManager::GetInstance().Release();
    ObjectFactory::DeleteAll(true);
    SaveIniFile();
}


void UEditorEngine::StartPIE()
{
    //UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

    //UWorld* PIEWorld = UWorld::DuplicateWorldForPIE(EditorWorld, ...);

    //GWorld = PIEWorld;

    //// AActor::BeginPlay()
    //PIEWorld->InitializeActorsForPlay();

    UWorld* EditorWorld = WorldContexts[0].World;
    UWorld* PIEWorld = UWorld::DuplicateWorldForPIE(EditorWorld);

    GWorld = PIEWorld;
    SLATE.SetPIEWorld(GWorld);

    bPIEActive = true;

    //// 슬레이트 매니저 (singleton)
    //FRect ScreenRect(0, 0, ClientWidth, ClientHeight);
    //SLATE.Initialize(RHIDevice.GetDevice(), PIEWorld, ScreenRect);

    ////스폰을 위한 월드셋
    //UI.SetWorld(PIEWorld);

    UE_LOG("START PIE CLICKED");
}

void UEditorEngine::EndPIE()
{
    //if (GWorld && GWorld->IsPIEWorld())
    //{
    //    GWorld->CleanupWorld();
    //    delete GWorld;
    //}
    //SLATE.SetPIEWorld(WorldContexts[0].World);
    //GWorld = GEditor->GetEditorWorldContext().World();

    bPIEActive = false;
    UE_LOG("END PIE CLICKED");
}
