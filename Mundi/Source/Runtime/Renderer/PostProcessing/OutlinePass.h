#pragma once
#include "PostProcessing.h"

/**
 * FOutlinePass
 *
 * 하이라이트된 오브젝트 주변에 아웃라인을 그리는 포스트 프로세스 패스.
 * 깊이 버퍼 기반 에지 검출을 사용하여 외곽선을 생성합니다.
 *
 * Payload 사용:
 * - Color: 아웃라인 색상 (RGBA)
 * - Params0.X: 아웃라인 두께 (픽셀 단위)
 * - Params0.Y: 깊이 감도 (에지 검출 임계값)
 * - Params0.Z: 아웃라인 강도 (0.0 ~ 1.0)
 */
class FOutlinePass final : public IPostProcessPass
{
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;
};
