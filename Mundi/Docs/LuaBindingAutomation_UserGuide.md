# Lua 바인딩 자동화 시스템 - 사용 가이드

## 개요

Mundi 엔진에 Lua 바인딩 자동화 시스템이 구현되었습니다!
이제 `UPROPERTY`와 `UFUNCTION` 매크로만 추가하면 자동으로 바인딩 코드가 생성됩니다.

## 빠른 시작

### 1. 의존성 설치

```bash
pip install jinja2
```

### 2. 코드 생성기 실행

```bash
python Tools/CodeGenerator/generate.py --source-dir Source/Runtime --output-dir Generated
```

### 3. 생성된 파일을 프로젝트에 추가

Visual Studio 프로젝트에 `Generated/*.generated.cpp` 파일들을 추가합니다.

---

## 사용법

### 헤더 파일에서 매크로 사용

```cpp
// MyComponent.h
#pragma once

#include "Core/Object/Object.h"
#include "Components/SceneComponent.h"

class UMyComponent : public USceneComponent
{
    DECLARE_CLASS(UMyComponent, USceneComponent)
    GENERATED_REFLECTION_BODY()  // 필수!

public:
    // ===== 프로퍼티 자동 바인딩 =====

    UPROPERTY(EditAnywhere, Category="MyCategory")
    float Intensity = 1.0f;

    UPROPERTY(EditAnywhere, Category="MyCategory", Range="0,100", Tooltip="설명")
    float Percentage = 50.0f;

    UPROPERTY(EditAnywhere, Category="Settings")
    bool bEnabled = true;

    UPROPERTY(EditAnywhere, Category="Transform")
    FVector Position = FVector(0, 0, 0);

    // ===== 함수 자동 바인딩 =====

    UFUNCTION(LuaBind, DisplayName="SetValue")
    void SetIntensity(float Value)
    {
        Intensity = Value;
    }

    UFUNCTION(LuaBind, DisplayName="GetValue")
    float GetIntensity() const
    {
        return Intensity;
    }

    UFUNCTION(LuaBind, DisplayName="Enable")
    void EnableComponent()
    {
        bEnabled = true;
    }
};
```

### 생성된 파일 예시

코드 생성기는 `Generated/UMyComponent.generated.cpp` 파일을 자동으로 생성합니다:

```cpp
// Auto-generated file - DO NOT EDIT!

#include "MyComponent.h"
#include "Core/Object/ObjectMacros.h"
#include "Engine/Scripting/LuaBindHelpers.h"

// ===== Property Reflection =====

BEGIN_PROPERTIES(UMyComponent)
    MARK_AS_COMPONENT("UMyComponent", "Auto-generated UMyComponent")
    ADD_PROPERTY(float, Intensity, "MyCategory", true)
    ADD_PROPERTY_RANGE(float, Percentage, "MyCategory", 0.0f, 100.0f, true, "설명")
    ADD_PROPERTY(bool, bEnabled, "Settings", true)
    ADD_PROPERTY(FVector, Position, "Transform", true)
END_PROPERTIES()

// ===== Lua Binding =====

extern "C" void LuaBind_Anchor_UMyComponent() {}

LUA_BIND_BEGIN(UMyComponent)
{
    AddAlias<UMyComponent, float>(
        T, "SetValue", &UMyComponent::SetIntensity);
    AddMethodR<float, UMyComponent>(
        T, "GetValue", &UMyComponent::GetIntensity);
    AddAlias<UMyComponent>(
        T, "Enable", &UMyComponent::EnableComponent);
}
LUA_BIND_END()
```

---

## UPROPERTY 매크로 옵션

### 기본 옵션

| 옵션 | 설명 | 예시 |
|------|------|------|
| `EditAnywhere` | 에디터에서 편집 가능 | `UPROPERTY(EditAnywhere)` |
| `Category="..."` | 에디터 카테고리 | `Category="Rendering"` |
| `Tooltip="..."` | 툴팁 설명 | `Tooltip="광원 강도"` |
| `Range="min,max"` | 값 범위 제한 | `Range="0,100"` |

### 예시

```cpp
// 기본 프로퍼티
UPROPERTY(EditAnywhere, Category="Test")
float SimpleValue;

// 범위 제한
UPROPERTY(EditAnywhere, Category="Light", Range="0,10")
float Intensity;

// 툴팁 포함
UPROPERTY(EditAnywhere, Category="Audio", Tooltip="Volume in decibels")
float Volume;

// 리소스 타입
UPROPERTY(EditAnywhere, Category="Rendering")
UTexture* MyTexture;

UPROPERTY(EditAnywhere, Category="Mesh")
UStaticMesh* MyMesh;

// 배열
UPROPERTY(EditAnywhere, Category="Materials")
TArray<UMaterialInterface*> Materials;
```

---

## UFUNCTION 매크로 옵션

### 기본 옵션

| 옵션 | 설명 | 예시 |
|------|------|------|
| `LuaBind` | Lua에 바인딩 (필수) | `UFUNCTION(LuaBind)` |
| `DisplayName="..."` | Lua에서 사용할 함수 이름 | `DisplayName="SetColor"` |

### 지원되는 함수 타입

```cpp
// void 반환 함수 (AddAlias 사용)
UFUNCTION(LuaBind, DisplayName="SetValue")
void SetIntensity(float Value);

// 값 반환 함수 (AddMethodR 사용)
UFUNCTION(LuaBind, DisplayName="GetValue")
float GetIntensity() const;

// 파라미터 없는 함수
UFUNCTION(LuaBind, DisplayName="Reset")
void ResetState();

// 여러 파라미터
UFUNCTION(LuaBind, DisplayName="SetColor")
void SetMaterialColor(uint32 SlotIndex, const FString& ParamName, const FLinearColor& Color);
```

---

## Visual Studio 프로젝트 통합

### 방법 1: Pre-Build Event (추천)

1. 프로젝트 우클릭 → **Properties**
2. **Build Events** → **Pre-Build Event**
3. **Command Line**에 다음 추가:

```batch
python "$(ProjectDir)Tools\CodeGenerator\generate.py" --source-dir "$(ProjectDir)Source\Runtime" --output-dir "$(ProjectDir)Generated"
```

4. **Description**: `Generating Lua binding code...`

### 방법 2: 수동 실행

빌드 전에 수동으로 실행:

```bash
python Tools/CodeGenerator/generate.py --source-dir Source/Runtime --output-dir Generated
```

### 생성된 파일을 프로젝트에 추가

1. Visual Studio에서 `Generated` 폴더를 프로젝트에 추가
2. 모든 `.generated.cpp` 파일을 포함
3. 빌드

---

## 기존 코드 마이그레이션

### Before (수동)

```cpp
// StaticMeshComponent.h
class UStaticMeshComponent : public UMeshComponent
{
    GENERATED_REFLECTION_BODY()
protected:
    UStaticMesh* StaticMesh;
public:
    void SetMaterialColorByUser(uint32 Slot, const FString& Param, const FLinearColor& Color);
};

// StaticMeshComponent.cpp
BEGIN_PROPERTIES(UStaticMeshComponent)
    ADD_PROPERTY_STATICMESH(UStaticMesh*, StaticMesh, "Mesh", true)
END_PROPERTIES()

LUA_BIND_BEGIN(UStaticMeshComponent)
{
    AddAlias<UStaticMeshComponent, uint32, const FString&, const FLinearColor&>(
        T, "SetColor", &UStaticMeshComponent::SetMaterialColorByUser);
}
LUA_BIND_END()
```

### After (자동)

```cpp
// StaticMeshComponent.h - 매크로만 추가!
class UStaticMeshComponent : public UMeshComponent
{
    GENERATED_REFLECTION_BODY()

    UPROPERTY(EditAnywhere, Category="Mesh")
    UStaticMesh* StaticMesh;

    UFUNCTION(LuaBind, DisplayName="SetColor")
    void SetMaterialColorByUser(uint32 Slot, const FString& Param, const FLinearColor& Color);
};

// StaticMeshComponent.cpp - BEGIN_PROPERTIES, LUA_BIND 블록 모두 삭제!
// (이제 .generated.cpp에 자동 생성됨)
```

---

## 주의사항

### 1. GENERATED_REFLECTION_BODY() 필수

모든 자동 바인딩 클래스는 반드시 `GENERATED_REFLECTION_BODY()`를 포함해야 합니다.

```cpp
class UMyComponent : public USceneComponent
{
    DECLARE_CLASS(UMyComponent, USceneComponent)
    GENERATED_REFLECTION_BODY()  // ← 필수!
```

### 2. 헤더에 함수 구현 가능

함수를 헤더에 인라인으로 구현해도 파싱됩니다:

```cpp
UFUNCTION(LuaBind)
void SetValue(float V) { Value = V; }  // ✅ OK

UFUNCTION(LuaBind)
void SetValue(float V);  // ✅ OK (선언만)
```

### 3. const 함수 지원

```cpp
UFUNCTION(LuaBind)
float GetValue() const;  // ✅ const 지원
```

### 4. DisplayName 생략 가능

DisplayName을 생략하면 함수 이름을 그대로 사용합니다:

```cpp
UFUNCTION(LuaBind)  // Lua에서 "SetValue"로 호출
void SetValue(float V);
```

---

## 파일 구조

```
Mundi/
├── Source/
│   └── Runtime/
│       ├── Core/
│       │   └── Object/
│       │       └── ObjectMacros.h        # UPROPERTY, UFUNCTION 매크로 정의
│       └── Engine/
│           └── Components/
│               └── MyComponent.h         # 헤더에만 매크로 추가
│
├── Generated/                            # 자동 생성된 파일
│   └── UMyComponent.generated.cpp
│
└── Tools/
    └── CodeGenerator/
        ├── generate.py                   # 메인 스크립트
        ├── parser.py                     # 헤더 파서
        ├── property_generator.py         # 프로퍼티 코드 생성
        ├── lua_generator.py              # Lua 바인딩 코드 생성
        └── requirements.txt              # Python 의존성
```

---

## 트러블슈팅

### Q: 생성된 파일이 없어요

**A:** 다음을 확인하세요:
1. `GENERATED_REFLECTION_BODY()` 매크로가 헤더에 있는지
2. Python 스크립트가 올바른 경로로 실행되었는지
3. 출력 디렉토리가 존재하는지

### Q: 함수가 바인딩되지 않아요

**A:** `UFUNCTION(LuaBind)` 매크로가 있는지 확인하세요.

### Q: 프로퍼티가 파싱되지 않아요

**A:** `UPROPERTY()` 매크로가 있는지, 변수가 세미콜론 `;` 또는 `=`로 끝나는지 확인하세요.

### Q: 빌드 에러가 나요

**A:**
1. `Generated` 폴더의 모든 `.generated.cpp` 파일이 프로젝트에 포함되었는지 확인
2. 기존 수동 바인딩 코드를 제거했는지 확인 (중복 정의 방지)

---

## 다음 단계

구현이 완료된 기능:
- ✅ UPROPERTY, UFUNCTION 매크로 정의
- ✅ Python 코드 생성기
- ✅ 프로퍼티 자동 등록
- ✅ Lua 함수 자동 바인딩
- ✅ 범위, 툴팁 지원
- ✅ void/반환값 함수 지원

향후 개선 사항:
- [ ] Pre-Build Event 자동 설정 스크립트
- [ ] Watch 모드 (파일 변경 감지)
- [ ] 더 나은 에러 메시지
- [ ] 타입 변환 코드 자동 생성
- [ ] LSP 통합 (자동완성)

---

**작성일**: 2025-11-07
**버전**: 1.0
**프로젝트**: Mundi Engine
