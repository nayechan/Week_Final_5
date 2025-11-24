#pragma once
#include "Vector.h" // FMatrix
#include "Enums.h"
#include "CameraComponent.h"
#include "PostProcessing/PostProcessing.h"

#include "Frustum.h"
// 전방 선언
class ACameraActor;
class UCameraComponent;
class FViewport;
struct FPostProcessModifier;

/**
 * @struct FViewportRect
 * @brief 뷰포트 내의 사각 영역
 */
struct FViewportRect
{
    uint32 MinX = 0, MinY = 0, MaxX = 0, MaxY = 0;
    uint32 Width() const { return MaxX - MinX; }
    uint32 Height() const { return MaxY - MinY; }
};


struct FMinimalViewInfo
{
    FVector ViewLocation;
    FQuat ViewRotation;
    float FieldOfView = 90.0f;
    float ZoomFactor = 1.0f;
    float NearClip = 10.0f;
    float FarClip = 1000.0f;
    float AspectRatio = 1.7777f;
    FViewportRect ViewRect;
    ECameraProjectionMode ProjectionMode;
};

/**
 * @class FSceneView
 * @brief 단일 관점에서 씬을 렌더링하는 데 필요한 모든 데이터를 소유합니다.
 * (ViewMatrix, ProjectionMatrix, Frustum, ViewportRect 등)
 */

class FSceneView
{
public:
    // 메인 뷰(카메라)를 위한 생성자
    FSceneView(FMinimalViewInfo* InMinimalViewInfo, URenderSettings* InRenderSettings);
    FSceneView(UCameraComponent* InCamera, FViewport* InViewport, URenderSettings* InRenderSettings);

private:
    TArray<FShaderMacro> CreateViewShaderMacros();

public:
    // 렌더링 데이터
    FMatrix ViewMatrix{};
    FMatrix ProjectionMatrix{};
    FFrustum ViewFrustum{};
    FVector ViewLocation{};
    FQuat ViewRotation{};
    FViewportRect ViewRect{}; // 이 뷰가 그려질 뷰포트상의 영역

    TArray<FVector> FrustumVertices;

    URenderSettings* RenderSettings;

    // 렌더링 설정
    ECameraProjectionMode ProjectionMode = ECameraProjectionMode::Perspective;
    TArray<FShaderMacro> ViewShaderMacros;
    float NearClip = 0.0f;
    float FarClip = 0.0f;
    float FieldOfView = 0.0f;
    float AspectRatio = 0.0f;
    float ZoomFactor = 0.0f;

    // 배경색 설정 (기본값: 검은색)
    FLinearColor BackgroundColor = FLinearColor(0.0f, 0.0f, 0.0f, 1.0f);

    TArray<FPostProcessModifier> Modifiers;
};