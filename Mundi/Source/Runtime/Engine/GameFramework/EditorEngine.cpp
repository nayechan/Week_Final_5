#include "pch.h"
#include "EditorEngine.h"
#include "USlateManager.h"
#include "SelectionManager.h"
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
    // Cleanup is now handled in Shutdown()
    // Do not call FObjManager::Clear() here due to static destruction order
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
        WPARAM SizeType = wParam;
        if (SizeType != SIZE_MINIMIZED)
        {
            GetViewportSize(hWnd);

            UINT NewWidth = static_cast<UINT>(ClientWidth);
            UINT NewHeight = static_cast<UINT>(ClientHeight);
            GEngine.RHIDevice.OnResize(NewWidth, NewHeight);

            // Save CLIENT AREA size (will be converted back to window size on load)
            EditorINI["WindowWidth"] = std::to_string(NewWidth);
            EditorINI["WindowHeight"] = std::to_string(NewHeight);

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
    WCHAR Title[] = L"Mundi Engine";
    HICON hIcon = (HICON)LoadImageW(NULL, L"Data\\Icon\\MND.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
    WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, hIcon, 0, 0, 0, WindowClass };
    RegisterClassW(&wndclass);

    // Load client area size from INI
    int clientWidth = 1620, clientHeight = 1024;
    if (EditorINI.count("WindowWidth"))
    {
        try { clientWidth = stoi(EditorINI["WindowWidth"]); } catch (...) {}
    }
    if (EditorINI.count("WindowHeight"))
    {
        try { clientHeight = stoi(EditorINI["WindowHeight"]); } catch (...) {}
    }

    // Validate minimum window size to prevent unusable windows
    if (clientWidth < 800) clientWidth = 1620;
    if (clientHeight < 600) clientHeight = 1024;

    // Convert client area size to window size (including title bar and borders)
    DWORD windowStyle = WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW;
    RECT windowRect = { 0, 0, clientWidth, clientHeight };
    AdjustWindowRect(&windowRect, windowStyle, FALSE);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    HWnd = CreateWindowExW(0, WindowClass, Title, windowStyle,
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
    GWorld = WorldContexts[0].World;
    WorldContexts[0].World->Initialize();
    ///////////////////////////////////

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
            if (Renderer) Renderer->GetRHIDevice()->UpdateUVScrollConstantBuffers(UVScrollSpeed, UVScrollTime);
        }
    }
    if (!bUVScrollPaused)
    {
        UVScrollTime += DeltaSeconds;
        if (Renderer) Renderer->GetRHIDevice()->UpdateUVScrollConstantBuffers(UVScrollSpeed, UVScrollTime);
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

        if (bChangedPieToEditor)
        {
            if (GWorld && bPIEActive)
            {
                WorldContexts.pop_back();
                ObjectFactory::DeleteObject(GWorld);
            }

            GWorld = WorldContexts[0].World;
            GWorld->GetSelectionManager()->ClearSelection();
            GWorld->GetLightManager()->SetDirtyFlag();
            SLATE.SetPIEWorld(GWorld);

            bPIEActive = false;
            UE_LOG("END PIE CLICKED");

            bChangedPieToEditor = false;
        }

        Tick(DeltaSeconds);
        Render();
        
        // Shader Hot Reloading - Call AFTER render to avoid mid-frame resource conflicts
        // This ensures all GPU commands are submitted before we check for shader updates
        UResourceManager::GetInstance().CheckAndReloadShaders(DeltaSeconds);
    }
}

void UEditorEngine::Shutdown()
{
    // Release ImGui first (it may hold D3D11 resources)
    UUIManager::GetInstance().Release();

    // Delete all UObjects (Components, Actors, Resources)
    // Resource destructors will properly release D3D resources
    ObjectFactory::DeleteAll(true);

    // Clear FObjManager's static map BEFORE static destruction
    // This must be done in Shutdown() (before main() exits) rather than ~UEditorEngine()
    // because ObjStaticMeshMap is a static member variable that may be destroyed
    // before the global GEngine variable's destructor runs
    FObjManager::Clear();

    // IMPORTANT: Explicitly release Renderer before RHIDevice destructor runs
    // Renderer may hold references to D3D resources
    Renderer.reset();

    // Explicitly release D3D11RHI resources before global destruction
    RHIDevice.Release();

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
    bChangedPieToEditor = true;

    /*if (GWorld && bPIEActive)
    {
        WorldContexts.pop_back();
        ObjectFactory::DeleteObject(GWorld);
    }

    GWorld = WorldContexts[0].World;
    SLATE.SetWorld(GWorld);

    bPIEActive = false;
    UE_LOG("END PIE CLICKED");*/
}
