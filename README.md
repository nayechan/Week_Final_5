# 📘 KRAFTON TechLab Week05 – Unreal Engine Style 3D Editor & Rendering System

## 프로젝트 개요 ✨
TL2는 Direct3D 11과 ImGui를 기반으로 한 실시간 렌더링 & 씬 편집기입니다.
엔진 레이어(`Renderer`, `D3D11RHI`)와 에디터 레이어(`UWorld`, `UUIManager`)가 분리되어 있어
렌더링 파이프라인과 편집 UI를 독립적으로 확장할 수 있습니다.
씬 편집, 멀티 뷰포트, 기즈모 조작, 씬 저장/불러오기 등 학습용·프로토타이핑에 필요한 기능을 담고 있습니다.

## 주요 기능 🧩
- 💡 Direct3D 11 렌더러: `D3D11RHI`가 스왑체인/버퍼/셰이더를 관리하고 `URenderer`가 드로우 호출을 조율합니다.
- 🧱 컴포넌트 기반 액터 시스템: `AActor`, `USceneComponent` 파생 클래스로 메시·카메라·텍스트 등을 구성합니다.
- 🧭 공간 분할 & 컬링: BVH(`BVHierachy`), 옥트리(`FOctree`), 프러스텀 컬링으로 효율적인 월드 관리.
- 🪟 멀티 뷰포트 & 편집 UI: ImGui 기반 창(`SMultiViewportWindow`, `SViewportWindow`)과 다양한 위젯 제공.
- 🗂️ 씬 자산 관리: `Scene/*.Scene` JSON을 `FSceneLoader`가 읽고 쓰며, `Data/` 폴더 자산과 연동됩니다.
- 🎯 입력 & 기즈모: `UInputManager`가 키보드/마우스를 통합하고, 이동/회전/스케일 기즈모와 UV 스크롤을 제어합니다.
- 🎮 PIE(Play In Editor): 에디터에서 Start PIE를 누르면 에디터 창에서서 바로 게임이 시작됩니다.
- 🧩 컴포넌트 추가/삭제/수정: 액터를 고르고 ImGui 기반 창에서 디테일 패널에서 컴포넌트를 제어합니다.

## 프로젝트 구조 🗃️
TL2<br>
├─ Data/                 # OBJ/텍스처/미리 빌드된 바이너리 자산<br>
├─ Font/                 # ImGui에 한글 패치를 위한 폰트<br>
├─ Scene/                # JSON 기반 장면 스냅샷 (Default.scene, QuickSave 등)<br>
├─ UI/                   # 에디터 창, 위젯, ImGui 헬퍼 및 Direct2D 오버레이<br>
├─ d3dtk/                # DirectX Tool Kit 헤더 (DDSTextureLoader 등)<br>
├─ ImGui/                # ImGui 소스 및 Win32/DX11 바인딩<br>
├─ nlohmann/             # JSON 파서 (single-header)<br>
├─ *.cpp, *.h            # 엔진, 렌더러, 액터, 컴포넌트 구현<br>
├─ TL2.vcxproj           # Visual Studio 2022 C++ 프로젝트 파일<br>
└─ editor.ini            # 창 배치 및 사용자 설정 (자동 생성/저장)

## 개발 환경 & 의존성 🛠️

- Visual Studio 2022 (v143), Windows 10 SDK 10.0 이상.
- DirectX 11 런타임 (Windows 10 기본 포함).
- 외부 라이브러리: ImGui, nlohmann::json, DirectX Tool Kit (저장소에 포함되어 별도 설치 불필요).

## 빌드 & 실행 방법 ▶️

1. Visual Studio에서 TL2.vcxproj를 열고, 구성은 Debug/Release, 플랫폼은 x64를 선택합니다.
2. 솔루션을 빌드하면 실행 파일이 x64/<Configuration>/TL2.exe로 생성됩니다.
3. 첫 실행 시 editor.ini가 생성되며, 이후 창 레이아웃과 편집 옵션이 자동으로 저장됩니다.
4. 에디터를 종료하려면 ESC 키를 누르거나 창을 닫습니다.

> ⚙️ Microsoft Visual C++ 재배포 패키지가 설치되어 있지 않으면 실행 시 DLL 로드 오류가 발생할 수 있습니다.

## 기본 조작 🎮

- 🖱️ 마우스 우클릭 + 드래그: 카메라 회전(피치/요우), UIManager 저장 롤 유지.
- ⌨️ W / A / S / D: 전후좌우 이동, Q / E: 하강/상승.
- 🔁 T: UV 스크롤 토글 (Renderer::UpdateUVScroll 참조).
- 🔍 마우스: ImGui 윈도우
- 🚪 ESC: 프로그램 종료.

카메라 속도는 Camera Control 위젯에서 조절하며, 마우스 감도는 CameraActor.cpp의 MouseSensitivity로 정의됩니다.

## 에디터 주요 패널 🖼️

- 🗂️ Scene Manager: 액터 계층, 뷰포트 스위처, ShowFlag 토글 관리 (UI/Window/SceneWindow.cpp).
- 🎛️ Control Panel: 기즈모 모드, 선택 액터 트랜스폼, 삭제 기능 제공.
- 🧱 Primitive Spawn: Data/ 자산을 골라 정적 메시를 랜덤 위치/스케일로 배치.
- 💾 Scene IO: 새 씬 생성, 저장/불러오기, 퀵세이브/로드 제공 (SceneIOWidget).
- 💬 Console: UE_LOG 출력과 디버그 메시지 확인.
- 📊 Stats Overlay: Direct2D로 프레임 타임 및 GPU 상태 오버레이 (UI/StatsOverlayD2D) stat fps, stat memory, stat all, stat none 으로 콘솔에 입력하여 제어.
- 🔧 Detail Panel: 컴포넌트 추가/삭제/속성에 따른 수정 (트랜스폼, 메쉬 변경 등).

## 장면 파일 & 자산 관리 📁

- 씬은 Scene/*.Scene JSON으로 관리되며, FSceneLoader가 카메라 정보·액터 UUID·머티리얼 슬롯 등을 파싱합니다.
- 퀵세이브 시 Scene/QuickSave.Scene이 갱신되고, 이름을 지정하면 Scene/<Name>.Scene으로 저장됩니다.
- 메시/머티리얼 바이너리는 Data/의 .obj, .bin, .dds 등을 UResourceManager가 로드합니다.
- editor.ini는 창 크기, 카메라 속도 등 사용자 설정을 보존합니다. 초기화하려면 파일을 삭제하면 됩니다.

## 문제 해결 가이드 🛟

- D3D11 디바이스 생성 실패: 구형 GPU나 원격 세션에서 발생 가능. 최신 그래픽 드라이버와 DirectX 런타임 설치 후 재시도하
세요.
- 씬 로드 실패: 콘솔에 Scene load failed 로그가 보이면 JSON 구문 오류 가능성이 큽니다. SceneIOWidget으로 만든 파일을
참고하거나 Scene/Default.scene으로 초기화하세요.
- 기즈모/뷰포트 미표시: ShowFlag 위젯에서 Primitives, Gizmo 토글이 켜져 있는지 확인하고, 카메라가 씬 내부를 바라보는지
점검하세요.

## 확장 아이디어 🌱

- 디퍼드 렌더링, 포스트 프로세스, 라이트 컴포넌트 등 그래픽 기능 추가.
- 씬 히스토리/언두 시스템, 사용자 지정 단축키 지원.
- FBX/GLTF 등 다양한 포맷의 자산 파이프라인 확장.
- 네비게이션 메시, 물리 시뮬레이션과 통합해 게임플레이 편집 기능 강화.
