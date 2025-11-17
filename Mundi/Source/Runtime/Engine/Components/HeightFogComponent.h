#pragma once

#include "Object.h"

#include "SceneComponent.h"
#include "UHeightFogComponent.generated.h"

struct FLinearColor;

UCLASS(DisplayName="높이 안개 컴포넌트", Description="높이 기반 안개 컴포넌트입니다")
class UHeightFogComponent : public USceneComponent
{
public:

    GENERATED_REFLECTION_BODY()

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====

    UPROPERTY(EditAnywhere, Category="Fog", Range="0.0, 10.0")
    float FogDensity = 0.2f;

    UPROPERTY(EditAnywhere, Category="Fog", Range="0.0, 10.0")
    float FogHeightFalloff = 0.2f;

    UPROPERTY(EditAnywhere, Category="Fog", Range="0.0, 10000.0")
    float StartDistance = 0.0f;

    UPROPERTY(EditAnywhere, Category="Fog", Range="0.0, 100000.0")
    float FogCutoffDistance = 6000.0f;

    UPROPERTY(EditAnywhere, Category="Fog", Range="0.0, 1.0")
    float FogMaxOpacity = 1.0f;

    UPROPERTY(EditAnywhere, Category="Fog")
    FLinearColor FogInscatteringColor;

    UHeightFogComponent();
    ~UHeightFogComponent() override;
    
    // Component Lifecycle
    /*void InitializeComponent() override;
    void TickComponent(float DeltaTime) override;*/
    
    // Fog Parameters Getters
    float GetFogDensity() const { return FogDensity; }
    float GetFogHeightFalloff() const { return FogHeightFalloff; }
    float GetStartDistance() const { return StartDistance; }
    float GetFogCutoffDistance() const { return FogCutoffDistance; }
    float GetFogMaxOpacity() const { return FogMaxOpacity; }
    FLinearColor GetFogInscatteringColor() const { return FogInscatteringColor; }
    float GetFogHeight() const { return GetWorldLocation().Z; }
    
    // Fog Parameters Setters
    void SetFogDensity(float InDensity) { FogDensity = InDensity; }
    void SetFogHeightFalloff(float InFalloff) { FogHeightFalloff = InFalloff; }
    void SetStartDistance(float InDistance) { StartDistance = InDistance; }
    void SetFogCutoffDistance(float InDistance) { FogCutoffDistance = InDistance; }
    void SetFogMaxOpacity(float InOpacity) { FogMaxOpacity = InOpacity; }
    void SetFogInscatteringColor(FLinearColor InColor) { FogInscatteringColor = InColor; }
    
    // Rendering
    void RenderHeightFog(URenderer* Renderer);

    void OnRegister(UWorld* InWorld) override;
	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;

private:






    // Full Screen Quad Resources
    class UShader* HeightFogShader = nullptr;
};
