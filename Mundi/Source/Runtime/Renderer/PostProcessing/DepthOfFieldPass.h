#pragma once
#include "PostProcessing.h"

/* *
 *  DOF는 원칙적으로 3단계로 구성됩니다.
 *   
 *  1) CoC 계산  - Blur 반경을 결정하는 핵심 단계
 *  2) Blur 단계 - CoC 기반 Blur 생성 (Downscale/HexBlur 포함)
 *  3) Composite - 원본과 Blur 이미지를 CoC로 섞어 최종 결과 생성
 *
 *   ※ 필수 Pass: DOF 효과가 작동하는 데 반드시 필요
 *   ※ 선택 Pass: 품질 향상 / 최적화 / 아티팩트 제거용
 */

class FDepthOfFieldPass : public IPostProcessPass
{
public:
    virtual void Execute(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice) override;

private:
    // ============================================================
    // (1) CoC 단계
    // ============================================================

    // [필수] B. CoC 계산 - 초점거리/조리개/피사체 거리로 Blur 반경(CoC)을 생성.
    void ExecuteCoCPass(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice);

    // [선택] C. Foreground Dilation - 전경 픽셀을 외곽으로 확대하여 경계 씹힘/배경 침투 제거.
    void ExecuteDilationPass(FSceneView* View, D3D11RHI* RHIDevice);

    // ============================================================
    // (2) Blur 단계
    // ============================================================

    // [선택] D. Prefilter - 빛 강조, 마스크 정제, 하이라이트 보정 등 Blur 이전 단계 품질 향상.
    void ExecutePreFilterPass(FSceneView* View, D3D11RHI* RHIDevice, RHI_SRV_Index InputSRV, ERTVMode OutputRTV);

    // [필수] E. Near/Far Split - 전경과 배경을 분리하여 Bleeding(Halo) 아티팩트를 제거하여 품질 향상.
    void ExecuteNearFarSplitPass(FSceneView* View, D3D11RHI* RHIDevice);

    // [필수] F. Hex Blur - Hexagonal kernel을 3-pass로 분해하여 Near와 Far Blur 텍스처 생성.
    void ExecuteHexBlurPass(FSceneView* View, D3D11RHI* RHIDevice, RHI_SRV_Index InputSRV, 
        ERTVMode FinalRTV, RHI_SRV_Index FinalSRV);

    // [필수] G. Merge Near/Far -  분리된 Near와 Far 블러 텍스처 결과를 병합
    void ExecuteMergeNearFarPass(FSceneView* View, D3D11RHI* RHIDevice);

    // [선택] I. Temporal Blend - 이전 프레임 DOF와 현재 프레임 DOF를 블렌딩하여 노이즈/깜빡임 제거.
    void ExecuteTemporalBlendPass(FSceneView* View, D3D11RHI* RHIDevice);

    // ============================================================
    // (3) Composite 단계
    // ============================================================

    // [필수] J. Composite
    // 원본 SceneColor와 Blur를 CoC 기반으로 혼합하여 최종 DOF 결과 생성
    void ExecuteCompositePass(const FPostProcessModifier& M, FSceneView* View, D3D11RHI* RHIDevice);
};
