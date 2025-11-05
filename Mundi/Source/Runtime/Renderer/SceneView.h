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

struct FPostProcessInput
{
    TArray<FPostProcessModifier> Modifiers; // PCM이 채워 넣음
};

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

/**
 * @class FSceneView
 * @brief 단일 관점에서 씬을 렌더링하는 데 필요한 모든 데이터를 소유합니다.
 * (ViewMatrix, ProjectionMatrix, Frustum, ViewportRect 등)
 */

class FSceneView
{
public:
    // 메인 뷰(카메라)를 위한 생성자
    FSceneView() = default;
    FSceneView(UCameraComponent* InCamera, FViewport* InViewport, URenderSettings* InRenderSettings);

    void InitRenderSetting(FViewport* InViewport, URenderSettings* InRenderSettings);

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

    UCameraComponent* Camera;
    FViewport* Viewport;
    URenderSettings* RenderSettings;

    // 렌더링 설정
    ECameraProjectionMode ProjectionMode = ECameraProjectionMode::Perspective;
    TArray<FShaderMacro> ViewShaderMacros;
    float ZNear{}, ZFar{};

    // 이번 프레임 Postprocess
    FPostProcessInput PostProcessInput;
};