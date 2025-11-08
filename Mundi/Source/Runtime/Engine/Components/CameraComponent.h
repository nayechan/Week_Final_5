#pragma once

#include "SceneComponent.h"
#include "UCameraComponent.generated.h"

class UCameraComponent : public USceneComponent
{
public:

    GENERATED_REFLECTION_BODY()

    UCameraComponent();

protected:
    ~UCameraComponent() override;

public:
    void OnRegister(UWorld* InWorld) override;
    void OnUnregister() override;

    // Projection settings
    void SetFOV(float NewFOV) { FieldOfView = NewFOV; }
    void SetAspectRatio(float NewAspect) { AspectRatio = NewAspect; }
    void SetClipPlanes(float NewNear, float NewFar) { NearClip = NewNear; FarClip = NewFar; }
    void SetNearClipPlane(float NewNear) { NearClip = NewNear; }
    void SetFarClipPlane(float NewFar) { FarClip = NewFar; }
    void SetProjectionMode(ECameraProjectionMode Mode) { ProjectionMode = Mode; }
    void SetZoomFactor(float InZoomFactor) { ZoomFactor = InZoomFactor; };
    
    float GetFOV() const { return FieldOfView; }
    float GetAspectRatio() const { return AspectRatio; }
    float GetNearClip() const { return NearClip; }
    float GetFarClip() const { return FarClip; }
    float GetZoomFactor()const { return ZoomFactor; };
    ECameraProjectionMode GetProjectionMode() const { return ProjectionMode; }

    // Matrices
    FMatrix GetViewMatrix() const;
    FMatrix GetProjectionMatrix() const;
    FMatrix GetProjectionMatrix(float ViewportAspectRatio) const; //사용 x
    FMatrix GetProjectionMatrix(float ViewportAspectRatio, FViewport* Viewport) const; //ViewportAspectRatio는 Viewport에서 얻어올 수 있음

    void SetViewGizmo()
    {
        bSetViewGizmo = true;
    }
    const TArray<FVector>& GetViewGizmo() const
    {
        return ViewGizmo;
    }
    // Directions in world space
    FVector GetForward() const;
    FVector GetRight() const;
    FVector GetUp() const;

    // ───── 복사 관련 ────────────────────────────
    void DuplicateSubObjects() override;

    // Serialization
    void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
    UPROPERTY(EditAnywhere, Category="Camera", Range="1.0, 179.0")
    float FieldOfView;   // degrees

    UPROPERTY(EditAnywhere, Category="Camera", Range="0.1, 10.0")
    float AspectRatio;  //사용x

    UPROPERTY(EditAnywhere, Category="Camera", Range="0.01, 1000.0")
    float NearClip;

    UPROPERTY(EditAnywhere, Category="Camera", Range="1.0, 100000.0")
    float FarClip;

    UPROPERTY(EditAnywhere, Category="Camera", Range="0.1, 10.0")
    float ZoomFactor;

    ECameraProjectionMode ProjectionMode;

    bool bSetViewGizmo;
    TArray<FVector> ViewGizmo;

    class UStaticMeshComponent* CameraGizmo = nullptr;
};

