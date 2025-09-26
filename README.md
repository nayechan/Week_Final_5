# 📘 KRAFTON TechLab Week03 – Unreal Engine Style 3D Editor & Rendering System
📌 프로젝트 개요

### 프로젝트명: Unreal Engine Style 3D Editor & Rendering System

#### 개발 기간: 1 week

#### 개발 환경: Visual Studio, DirectX 11, Windows 10/11

## 아키텍처: C++ 기반 Actor-Component 시스템
<img width="1911" height="1104" alt="image" src="https://github.com/user-attachments/assets/2aa029db-2fca-451d-aeaf-607d3256ccee" />

https://drive.google.com/file/d/1bAa-ZufdUz7_mbSy236LDdI_DdKsFZ1b/view?usp=drive_link

## 🚀 구현 완료 사항
### 🎨 1. Editor & Rendering System (눈에 보이는 세상)
#### 1.1 실시간 텍스트 렌더링 시스템

파일: TextBillboard.hlsl, Week02/UI/

기능

ASCII 문자 → 텍스처 아틀라스 베이킹

UV 좌표 조작을 통한 임의 문자열 생성

Billboard 효과 (항상 카메라를 향함)

월드 공간의 UObject UUID 실시간 표시

기술

ViewInverse 행렬을 이용한 카메라 정렬

Alpha Testing 기반 텍스트 윤곽 처리

#### 1.2 Batch Line 렌더링 시스템


파일: ShaderLine.hlsl

기능

모든 Line을 하나의 Vertex/Index Buffer로 관리

D3D11_PRIMITIVE_TOPOLOGY_LINELIST 기반

World Grid, Bounding Box 시각화

성능 최적화

단일 Draw Call 처리

동적 버퍼 업데이트 지원

#### 1.3 좌표계 변환 시스템

DirectX 좌표계 (Y-Up, Z-Depth) → UE 좌표계 (Z-Up, X-Depth) 변환

카메라, 트랜스폼, 렌더링 파이프라인 전반에 적용

#### 1.4 동적 윈도우 리사이징

RTV/DSV 해제 → SwapChain Resize → ImGui DisplaySize 갱신 → RTV/DSV 재생성

#### 1.5 View Mode 시스템

Lit / Unlit / Wireframe 모드 지원

#### 1.6 Show Flag 시스템

비트 플래그 기반 토글 기능

예: Primitive 표시, UUID 텍스트, Bounding Box, Grid 등

ImGui 연동으로 실시간 제어

⚙️ 2. Engine Core System (눈에 안 보이는 세상)
#### 2.1 FName 시스템

문자열 → Pool 관리, 인덱스로 비교

strcmp() 대신 정수 비교 (O(n) → O(1))

대소문자 구분 없는 비교 ("Test" == "test")

메모리 절약 + 성능 최적화

#### 2.2 UObject 시스템 & RTTI

구현: Object.h

특징

UUID, FName 기반 이름 관리

컴파일 타임 타입 정보 생성 (DECLARE_CLASS)

안전한 런타임 타입 검사 & 캐스팅

ObjectFactory와 연동된 메모리 관리

#### 2.3 메모리 관리 시스템

UObject 전용 Allocator / Deallocator

중앙 집중식 객체 관리

자동 정리로 메모리 누수 방지

#### 2.4 Selection Manager

선택된 Actor 관리

안전성 강화 (CleanupInvalidActors)

## 🏗 아키텍처 및 설계 패턴

싱글톤 패턴: UUIManager, USelectionManager, UInputManager

컴포넌트 시스템: Actor-Component 구조 (Camera, Cube, Gizmo 등)

팩토리 패턴: ObjectFactory + NewObject<T>() 템플릿

Observer 패턴: Show Flag 시스템과 UI/렌더링 간 느슨한 결합

## 🖥 렌더링 파이프라인

World Grid (Show Flag)

Primitive Geometry (Lit/Unlit/Wireframe)

Bounding Boxes (Show Flag)

Billboard Text (UUID) (Show Flag)

Gizmo (선택된 객체)

셰이더 구성

Primitive.hlsl: 기본 메시

ShaderLine.hlsl: 라인/그리드

TextBillboard.hlsl: 빌보드 텍스트

## ⚡ 안전성 및 성능 최적화

배치 렌더링: Draw Call 최소화

텍스처 아틀라스: 폰트 통합 관리

Show Flag: 불필요한 렌더링 제거

FName 시스템: 문자열 비교 최적화

## 🏆 기술적 성취

언리얼 스타일 아키텍처 구현

UObject 시스템, RTTI, FName 최적화

Actor-Component 패러다임 적용

고급 렌더링 기술

Billboard 텍스트

Batch Line 렌더링

멀티 셰이더 파이프라인 관리

실용적인 에디터 도구

Show Flag 실시간 제어

다중 View Mode 지원

동적 윈도우 리사이징


# Week04 프로젝트 보고서

# Week04 프로젝트 - UE 스타일 3D 에디터 구현
https://drive.google.com/file/d/1bAa-ZufdUz7_mbSy236LDdI_DdKsFZ1b/view?usp=drive_link
## 개요

이 프로젝트는 Unreal Engine 4의 에디터 구조를 모방하여 구현한 3D 렌더링 엔진 및 에디터입니다. DirectX 11을 기반으로 하며, 정적 메시 렌더링, 다중 뷰포트, 오브젝트 관리 시스템 등의 핵심 기능들을 구현했습니다.
![Adobe Express - 제목 없는 비디오 - Clipchamp로 제작 (2)](https://github.com/user-attachments/assets/1af2b319-b0ae-41eb-9b94-4a9697a79b8b)
## 주요 특징

### 🎮 정적 메시 (Static Mesh) 시스템

- **OBJ 파일 로딩**: Wavefront OBJ 파일을 파싱하여 3D 모델 로드
- **바이너리 캐싱**: OBJ 파일을 바이너리 형식으로 변환하여 성능 최적화
- **다중 재질 지원**: 여러 Material Section을 지원하는 머티리얼 시스템
- **UV 스크롤링**: 텍스처 애니메이션 효과 지원

### 🖼️ 다중 뷰포트 시스템

- **4분할 뷰포트**: Perspective, Front, Side, Top 뷰 동시 렌더링
- **동적 크기 조절**: 드래그 가능한 스플리터로 뷰포트 크기 조절
- **UE 스타일 구조**: `FViewport`와 `FViewportClient` 클래스로 관리
- **설정 저장**: Editor.ini 파일에 스플리터 상태 저장/복원

### 🎯 오브젝트 관리 시스템

- **TObjectIterator**: 타입 안전하게 특정 타입에 대해서 순회 가능한 반복자
- **Scene Manager**: 월드 아웃라이너와 유사한 오브젝트 관리 UI
- **선택 시스템**: 3D 뷰포트와 연동된 오브젝트 선택
- **직렬화**: StaticMeshComponent 정보, UUID 정보, 카메라 정보 등을 씬 파일에 직렬화

## 핵심 구현 내용

### 📁 아키텍처 구조

### 1. 오브젝트 시스템 (`ObjectIterator.h`)

```cpp
template<typename TObject>
class TObjectIterator
{
    // UE4와 동일한 방식의 타입 안전한 오브젝트 순회
    // GUObjectArray에서 특정 타입의 오브젝트만 필터링
};
```

### 2. 정적 메시 관리 (`ObjManager.cpp`)

- **Preload()**: Data 폴더의 모든 OBJ 파일을 스캔하여 미리 로드
- **Binary Caching**: `.obj` → `.bin` + `Mat.bin` 변환으로 로딩 속도 향상
- **Resource Management**: 중복 로딩 방지 및 리소스 중앙 관리를 위한 캐싱 시스템

### 3. 뷰포트 시스템 (`FViewport.h`)

```cpp
class FViewport
{
    // D3D11 렌더 타겟 관리
    // 마우스/키보드 입력 처리
    // 뷰포트별 독립적인 렌더링
};
```

### 4. SWindow(SWindow`.h`)

```cpp
class SWindow
{

 Direct3D11 렌더 타겟(Render Target)을 보유/관리
 자신이 차지하는 Rect 영역에 대한 마우스/키보드 입력 처리
 자식 위젯들을 가질 수 있으며, 공통된 창(Window) 기능 제공
 뷰포트, 패널, 스플리터 등 모든 UI 요소의 기반 클래스

*};

class SSplitter
{*
 영역을 분할하는 윈도우 (수직/수평)

 SWindow를 상속하여 좌/우 혹은 상/하로 화면을 나누는 역할
 D3D11 렌더 타겟은 자식 윈도우(SWindow, SViewportWindow 등)로 전달
 마우스 드래그로 분할 비율을 조정 가능
 입력 처리 후 자식에게 이벤트를 위임 
 뷰포트와 패널들을 배치하는 레이아웃 관리의 핵심

*};

class SViewportWindow
{*

 실제 게임/에디터 씬을 그려주는 뷰포트 윈도우
 D3D11 렌더 타겟을 직접 생성하고, Scene/World를 렌더링
 카메라, 조명, 오브젝트를 포함한 3D 씬 표현
 마우스 입력을 받아 카메라 이동/회전, 오브젝트 선택(Picking) 등 처리
 각 뷰포트는 독립적인 카메라 상태를 가짐
 에디터에서 여러 개(4분할 등)로 배치 가능
};
```

### 4. 직렬화 시스템 (`Archive.h`)

```cpp
class FArchive
{
    // 바이너리 데이터 읽기/쓰기 추상화
    // 문자열, 배열 직렬화 헬퍼 함수
};
```

### 🎨 렌더링 시스템

### HLSL 셰이더 (`StaticMeshShader.hlsl`)

- **Vertex Shader**: 월드-뷰-프로젝션 변환
- **Pixel Shader**: 텍스처 샘플링, 머티리얼 적용용
- **하이라이트 시스템**: 선택된 오브젝트 시각적 표시
- **기즈모 렌더링**: X(빨강), Y(초록), Z(파랑) 축별 색상 구분

### 다중 뷰포트 렌더링

- **Orthogonal Views**: Front, Side, Top 직교 투영
- **Perspective View**: 자유로운 카메라 시점
- **동기화된 선택**: 모든 뷰포트에서 동일한 오브젝트 상태 유지

### 💾 파일 시스템

### OBJ 파일 처리

1. **파싱**: Wavefront OBJ 형식 지원 (정점, 법선, UV, 머티리얼)
2. **최적화**: 바이너리 캐싱으로 재로딩 성능 향상
3. **머티리얼**: MTL 파일 파싱으로 다중 재질 지원

### 씬 저장/로드

- **FPrimitiveData**: 오브젝트 정보 직렬화 구조체
- **카메라 상태**: Perspective 뷰의 카메라 위치/회전 저장
- **스플리터 설정**: UI 레이아웃 상태 유지

## 🔧 사용 방법

### 오브젝트 생성

1. Scene Manager에서 Primitive Type, 사용할 Static Mesh를 지정한 후 Spawn Actor 클릭
2. 자동으로 씬에 추가되고 기즈모 표시

### 뷰포트 조작

- **마우스 드래그**: 뷰포트 경계에서 크기 조절
- **오브젝트 선택**: 3D 뷰포트 또는 Scene Manager에서 클릭
- **카메라 이동**: WASD 키와 마우스로 자유 시점 이동

### 씬 관리

- **저장**: File → Save Scene
- **로드**: File → Load Scene
- **검색**: Scene Manager의 Search Bar로 오브젝트 필터링

## 📊 성능 최적화

### 바이너리 캐싱

```
.obj 파일 (텍스트) → .bin 파일 (바이너리)
- 파싱 시간 단축
- 메모리 사용량 감소
- 로딩 속도 향상
```

### 오브젝트 풀링

- TObjectIterator로 메모리 효율적인 오브젝트 순회
- 중복 로딩 방지를 위한 리소스 캐싱

### 렌더링 최적화

- 뷰포트별 독립적인 렌더 타겟
