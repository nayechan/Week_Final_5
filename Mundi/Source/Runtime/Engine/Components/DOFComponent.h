#pragma once

#include "Object.h"

#include "SceneComponent.h"
#include "UDOFComponent.generated.h"


UCLASS(DisplayName = "Depth Of Field", Description = "피사계 심도")
class UDOFComponent : public USceneComponent
{
public:

    GENERATED_REFLECTION_BODY()

public:

    UDOFComponent() = default;
    ~UDOFComponent() override = default;


    // Rendering
    void RenderHeightFog(URenderer* Renderer);

    void OnRegister(UWorld* InWorld) override;
    // Serialize
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

public:

    UPROPERTY(EditAnywhere, Category = "DepthOfField", Range = "0.01, 1000.0")
    float FocusDistance = 0.0f;

    UPROPERTY(EditAnywhere, Category = "DepthOfField", Range = "0.01, 1000.0")
    float FocusRange = 0.0f;

    UPROPERTY(EditAnywhere, Category = "DepthOfField", Range = "1.0, 30.0")
    float GaussianBlurWeight = 1;

    UPROPERTY(EditAnywhere, Category = "DepthOfField")
    bool bUseDownSampling = false;
private:

};
