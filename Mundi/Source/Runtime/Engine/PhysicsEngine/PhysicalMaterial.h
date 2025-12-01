#pragma once

#include "PhysXPublic.h"

#include "UPhysicalMaterial.generated.h"

/**
 * 마찰/반발력 계산 시 두 물체의 값을 어떻게 섞을지 지정
 * PhysX의 PxCombineMode와 1:1 대응함
 */
UENUM()
enum class EFrictionCombineMode : uint8
{
    Average  = 0, // (A + B) / 2
    Min      = 1, // Min(A, B)
    Multiply = 2, // A * B
    Max      = 3  // Max(A, B)
};

UCLASS(DisplayName="물리 재질", Description="물리적 표면 성질을 정의하는 리소스입니다.")
class UPhysicalMaterial : public UResourceBase
{
public:
    GENERATED_REFLECTION_BODY()

    UPhysicalMaterial();
    virtual ~UPhysicalMaterial();

public:
    // ==============================================================================
    // Surface Properties (표면 속성)
    // ==============================================================================

    /** 운동 마찰 계수 (0.0 = 마찰 없음, 미끄러지는 도중의 저항) */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    float Friction;

    /** 정지 마찰 계수 (0.0 = 마찰 없음, 움직이기 시작할 때의 저항) */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    float StaticFriction;

    /** 마찰 결합 모드 (두 물체가 부딪힐 때 마찰력을 어떻게 합칠지 결정) */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    EFrictionCombineMode FrictionCombineMode;

    /** 이 재질의 마찰 결합 모드를 강제로 사용할지 여부 (기본값 대신 사용) */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    bool bOverrideFrictionCombineMode;

    /** 반발 계수 (0.0 = 튀지 않음, 1.0 = 완전 탄성 충돌) */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    float Restitution;

    /** 반발 결합 모드 (두 물체가 부딪힐 때 반발력을 어떻게 합칠지 결정) */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    EFrictionCombineMode RestitutionCombineMode;

    /** 이 재질의 반발 결합 모드를 강제로 사용할지 여부 */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    bool bOverrideRestitutionCombineMode;

    // ==============================================================================
    // Object Properties (객체 속성)
    // ==============================================================================

    /** * 물체의 밀도 (kg/m^3)
     * 쉐이프의 부피와 함께 질량을 자동 계산하는 데 사용됩니다.
     * (예: 물 = 1000, 나무 = 700, 철 = 7800)
     */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    float Density;

    /** * Sleep(수면) 상태로 전환되기 위한 선형 속도 임계값 
     * 이 값보다 느리게 움직이면 물리 연산을 멈춥니다. (-1이면 엔진 기본값 사용)
     */
    UPROPERTY(EditAnywhere, Category = "PhysicalMaterial")
    float SleepLinearVelocityThreshold;

    // ==============================================================================
    // PhysX Interface
    // ==============================================================================

    /** 이 설정으로 생성된 PhysX 재질 객체를 반환합니다 */
    PxMaterial* GetPxMaterial();

    /** 런타임에 속성 값이 변경되면 PhysX 재질 객체도 갱신합니다 */
    void UpdatePhysicsMaterial();

    // ==============================================================================
    // 직렬화
    // ==============================================================================

    /** 파일에서 로드 (ResourceManager 호출) */
    void Load(const FString& InFilePath, ID3D11Device* InDevice);

    /** 직렬화 (저장/로드 공용) */
    virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    /** 복제 (Deep Copy) */
    virtual void DuplicateSubObjects() override;
    
    // ==============================================================================
    virtual void OnPropertyChanged(const FProperty& Prop) override;

protected:
    /** PhysX 재질 인스턴스 (런타임 캐싱용) */
    PxMaterial* PxMat;
};