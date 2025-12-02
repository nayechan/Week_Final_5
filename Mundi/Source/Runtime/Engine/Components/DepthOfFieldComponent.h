#pragma once

#include "SceneComponent.h"
#include "UDepthOfFieldComponent.generated.h"

struct FDepthOfFieldSettings
{
    float FocalDistance = 5000.0f;
    float FocalLength = 50.0f;
    float Aperture = 4.0f;
    float MaxCocRadius = 12.0f;

    float NearTransitionRange = 500.0f;
    float FarTransitionRange = 500.0f;
    float NearBlurScale = 1.0f;
    float FarBlurScale = 1.0f;

    int32 BlurSampleCount = 12;
    float BokehRotationRadians = 0.0f;
};

UCLASS(DisplayName="Depth of Field Component", Description="Controls depth of field effects.")
class UDepthOfFieldComponent : public USceneComponent
{
public:

    GENERATED_REFLECTION_BODY()

    UDepthOfFieldComponent();
    ~UDepthOfFieldComponent() override = default;

public:
    UPROPERTY(EditAnywhere, Category="DepthOfField | Activation",
        Tooltip = "해당 컴포넌트의 기능 활성화 여부를 결정합니다.")
    bool bEnableDepthOfField = true;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Activation",
        Tooltip = "복수의 피사계 심도 컴포넌트 중에서 우선순위를 결정합니다.\n",
                  "우선순위가 낮을수록 먼저 적용하며, 같다면 가중치를 혼합합니다.")
    int32 Priority = 0;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Activation", Range="0.0, 1.0", 
        Tooltip = "동일한 우선순위를 가지면서 여러 볼륨에 걸친\n",
                  "피사계 심도 컴포넌트와의 효과 혼합 비율을 결정합니다.")
    float BlendWeight = 1.0f;

    // 무한 범위 (전체 씬에 적용)
    UPROPERTY(EditAnywhere, Category="DepthOfField | Volume",
        Tooltip = "피사계 심도의 효과 범위 종류를 지정합니다.\n",
                  "활성화를 하면 Scene에 적용하며, 비활성화를 하면 볼륨 내에서 적용합니다.")
    bool bUnbounded = true;

    // 유한 범위일 때 Volume 크기
    UPROPERTY(EditAnywhere, Category="DepthOfField | Volume",
        Tooltip = "피사계 심도 컴포넌트의 볼륨 박스 크기를 조절합니다.\n",
                  "볼륨 박스 내에 카메라가 있으면, 피사계 심도 효과가 활성화합니다.")
    FVector VolumeExtent = FVector(10.0f, 10.0f, 10.0f);

    UPROPERTY(EditAnywhere, Category="DepthOfField | Focus", Range="0.0, 1000.0", 
        Tooltip = "카메라로부터 가장 선명한 거리를 조절합니다.\n",
                  "Transition 범위보다 작다면 아티팩트가 발생합니다.")
    float FocalDistance = 50.0f;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Focus", Range="1.0, 300.0",
        Tooltip = "카메라 렌즈의 초점거리를 조절합니다. (단위 mm)\n",
                  "초점거리가 멀어질수록 배경의 흐려짐이 커집니다.\n",
                  "초점거리가 가까울수록 배경의 선명함이 커집니다.")
    float FocalLength = 50.0f;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Focus", Range="0.1, 32.0",
        Tooltip = "조리개의 값을 조절합니다.\n",
                  "조리개 값이 작을수록 조리개가 열린다는 것이며, 블러 효과가 강해집니다.\n",
                  "조리개 값이 클수록 조리개가 닫혀지며, 블러 효과가 약해집니다.")
    float Aperture = 4.0f;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Blur", Range="0.5, 64.0",
        Tooltip = "CoC의 최대 반경, 블러 샘플의 최대 크기를 조절합니다.\n",
                  "값이 클수록 피사계 심도 효과가 커집니다.\n",
                  "값이 작을수록 자연스럽고 부드러워지는 효과를 가집니다.\n",
                  "지나치게 값이 커지면 halo 아티팩트가 발생합니다.")
    float MaxCocRadius = 12.0f;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Blur", Range="0.0, 1500.0",
        Tooltip = "카메라 초점에서 Near Blur로 넘어가는 거리 완충 구간입니다.\n",
                  "값이 클수록 블러 효과가 자연스럽지만 강도가 약해집니다.\n",
                  "값이 작을수록 블러 효과가 급격하게 시작합니다.")
    float NearTransitionRange = 25.0f;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Blur", Range="0.0, 1000.0",
        Tooltip = "카메라 초점에서 Far Blur로 넘어가는 거리 완충 구간입니다.\n",
                  "값이 클수록 블러 효과가 자연스럽지만 강도가 약해집니다.\n",
                  "값이 작을수록 블러 효과가 급격하게 시작합니다.")
    float FarTransitionRange = 100.0f;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Blur", Range="0.1, 8.0",
        Tooltip = "Near Blur의 강도를 조절하는 값입니다.")
    float NearBlurScale = 1.0f;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Blur", Range="0.1, 8.0",
        Tooltip = "Far Blur의 강도를 조절하는 값입니다.")
    float FarBlurScale = 1.0f;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Blur", Range="3.0, 64.0",
        Tooltip = "블러 샘플의 개수를 조절합니다.\n",
                  "값이 클수록 블러 효과가 증가합니다.\n",
                  "단, MaxCoCRadius 값과 비례하여 증가해야 아티팩트가 발생하지 않습니다.")
    int32 BlurSampleCount = 12;

    UPROPERTY(EditAnywhere, Category="DepthOfField | Blur", Range="0.0, 360.0",
        Tooltip = "렌즈의 조리개 회전 방향을 재현하는 데 사용됩니다.")
    float BokehRotationDegrees = 0.0f;

    // Component Lifecycle
    void OnRegister(UWorld* InWorld) override;

    // Depth of Field parameters
    bool IsDepthOfFieldEnabled() const { return bEnableDepthOfField; }
    int32 GetDofPriority() const { return Priority; }
    float GetBlendWeight() const;
    FDepthOfFieldSettings GetDepthOfFieldSettings() const;

    // Volume check
    bool IsInsideVolume(const FVector& TestLocation) const;

    // Debug Rendering
    void RenderDebugVolume(class URenderer* Renderer) const;

private:
    float ClampPositive(float Value, float DefaultValue) const;
};

