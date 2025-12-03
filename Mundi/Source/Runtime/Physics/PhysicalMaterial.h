#pragma once
#include <PxPhysicsAPI.h>
#include "ResourceBase.h"
#include "UPhysicalMaterial.generated.h"

// 마찰/반발력 합산 방식
UENUM()
enum class EFrictionCombineMode : uint8
{
    Average,    // 평균
    Min,        // 최소값
    Multiply,   // 곱
    Max         // 최대값
};

UCLASS(DisplayName="물리 재질", Description="물리 재질 속성 정의")
class UPhysicalMaterial : public UResourceBase
{
    GENERATED_REFLECTION_BODY()

public:
    // --- 실제 PhysX 객체 ---
    physx::PxMaterial* MatHandle = nullptr;

    // --- 기본 물리 속성 ---
    UPROPERTY(EditAnywhere, Category="Friction")
    float StaticFriction = 0.5f;    // 정지 마찰력

    UPROPERTY(EditAnywhere, Category="Friction")
    float DynamicFriction = 0.5f;   // 운동 마찰력

    UPROPERTY(EditAnywhere, Category="Physics")
    float Restitution = 0.3f;       // 반발 계수 (0=완전 비탄성, 1=완전 탄성)

    UPROPERTY(EditAnywhere, Category="Physics")
    float Density = 1000.0f;        // 밀도 (kg/m^3) - 물 = 1000

    // --- 합산 방식 ---
    UPROPERTY(EditAnywhere, Category="Advanced")
    EFrictionCombineMode FrictionCombineMode = EFrictionCombineMode::Average;

    UPROPERTY(EditAnywhere, Category="Advanced")
    EFrictionCombineMode RestitutionCombineMode = EFrictionCombineMode::Average;

    // --- 생성자/소멸자 ---
    UPhysicalMaterial() = default;
    ~UPhysicalMaterial() override { Release(); }

    // 생성 함수
    void CreateMaterial();

    // 속성 업데이트 (런타임 중 변경 시)
    void UpdateMaterial();

    // 해제 함수
    void Release();

    // --- 리소스 로드/저장 ---
    void Load(const FString& InFilePath, ID3D11Device* InDevice = nullptr);
    void Save(const FString& InFilePath);

    // --- 프리셋 생성 함수 ---
    static UPhysicalMaterial* CreateDefaultMaterial();
    static UPhysicalMaterial* CreateMetalMaterial();
    static UPhysicalMaterial* CreateWoodMaterial();
    static UPhysicalMaterial* CreateRubberMaterial();
    static UPhysicalMaterial* CreateIceMaterial();

private:
    physx::PxCombineMode::Enum ToPxCombineMode(EFrictionCombineMode Mode) const;
};
