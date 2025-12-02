#pragma once
#include "Vector.h"
#include "Color.h"
#include "UEContainer.h"

// 전방 선언
class ULineComponent;
class UDynamicMesh;
class FMeshData;

/**
 * FPrimitiveDrawInterface
 *
 * 디버그 렌더링을 위한 인터페이스.
 * Wire 렌더링은 ULineComponent, Solid 렌더링은 UDynamicMesh를 사용.
 */
class FPrimitiveDrawInterface
{
public:
    FPrimitiveDrawInterface();
    ~FPrimitiveDrawInterface();

    // 초기화/정리
    void Initialize(ULineComponent* InLineComp);
    void Clear();

    // =====================================================
    // Wire 프리미티브 (라인 기반)
    // =====================================================

    /** 단일 라인 */
    void DrawLine(const FVector& Start, const FVector& End, const FLinearColor& Color);

    /** 원 (평면 지정) */
    void DrawCircle(const FVector& Center, const FVector& XAxis, const FVector& YAxis,
                    float Radius, int32 Segments, const FLinearColor& Color);

    /** XY 평면 원 */
    void DrawCircleXY(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color);

    /** XZ 평면 원 */
    void DrawCircleXZ(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color);

    /** YZ 평면 원 */
    void DrawCircleYZ(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color);

    /** Wire Sphere (3개 원) */
    void DrawWireSphere(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color);

    /** Wire Sphere (Transform 적용) */
    void DrawWireSphere(const FTransform& Transform, float Radius, int32 Segments, const FLinearColor& Color);

    /** Wire Box (12개 에지) */
    void DrawWireBox(const FVector& Center, const FVector& Extent, const FLinearColor& Color);

    /** Wire Box (Transform 적용) */
    void DrawWireBox(const FTransform& Transform, const FVector& Extent, const FLinearColor& Color);

    /** Wire Capsule (2개 반구 + 4개 세로선) */
    void DrawWireCapsule(const FVector& Center, const FQuat& Rotation, float Radius, float HalfHeight,
                         int32 Segments, const FLinearColor& Color);

    /** Wire Capsule (Transform 적용) */
    void DrawWireCapsule(const FTransform& Transform, float Radius, float HalfHeight,
                         int32 Segments, const FLinearColor& Color);

    // =====================================================
    // Solid 프리미티브 (메시 기반) - 추후 구현
    // =====================================================

    /** Solid Sphere */
    void DrawSolidSphere(const FVector& Center, float Radius, int32 Segments, const FLinearColor& Color);

    /** Solid Box */
    void DrawSolidBox(const FTransform& Transform, const FVector& Extent, const FLinearColor& Color);

    /** Solid Capsule */
    void DrawSolidCapsule(const FTransform& Transform, float Radius, float HalfHeight,
                          int32 Segments, const FLinearColor& Color);

    // =====================================================
    // 유틸리티
    // =====================================================

    /** LineComponent 설정 */
    void SetLineComponent(ULineComponent* InLineComp) { LineComp = InLineComp; }
    ULineComponent* GetLineComponent() const { return LineComp; }

    /** 기본 세그먼트 수 */
    static constexpr int32 DefaultCircleSegments = 24;
    static constexpr int32 DefaultSphereSegments = 16;

private:
    ULineComponent* LineComp = nullptr;

    // Solid 렌더링용 (추후 구현)
    // UDynamicMesh* SolidMesh = nullptr;
    // TArray<FMeshData> SolidBatches;
};
