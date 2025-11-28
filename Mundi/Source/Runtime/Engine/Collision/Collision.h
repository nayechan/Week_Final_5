#pragma once
struct FAABB;
struct FOBB;
struct FBoundingSphere;
struct FShape;

class UShapeComponent;

namespace Collision
{
    bool Intersects(const FAABB& Aabb, const FOBB& Obb);

    bool Intersects(const FAABB& Aabb, const FBoundingSphere& Sphere);

	bool Intersects(const FOBB& Obb, const FBoundingSphere& Sphere);

    // ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡShapeComponent Helper 함수ㅡㅡㅡㅡㅡㅡㅡㅡㅡㅡ
    FVector AbsVec(const FVector& v);
    float UniformScaleMax(const FVector& S);

    void  BuildOBB(const FShape& BoxShape, const FTransform& T, FOBB& Out);
    bool Overlap_OBB_OBB(const FOBB& A, const FOBB& B);
    bool Overlap_Sphere_OBB(const FVector& Center, float Radius, const FOBB& B);

    bool OverlapSphereAndSphere(const FShape& ShapeA, const FTransform& TransformA, const FShape& ShapeB, const FTransform& TransformB);
    
    void BuildCapsule(const FShape& CapsuleShape, const FTransform& Xform, FVector& OutP0, FVector& OutP1, float& OutRadius);
    void BuildCapsuleCoreOBB(const FShape& CapsuleShape, const FTransform& Transform, FOBB& Out);

    bool OverlapCapsuleAndSphere(const FShape& Capsule, const FTransform& TransformCapsule, const FShape& Sphere, const FTransform& TransformSphere);

    bool OverlapCapsuleAndBox(const FShape& Capsule, const FTransform& TransformCapsule, const FShape& Box, const FTransform& TransformBox);

    bool OverlapCapsuleAndCapsule(const FShape& CapsuleA, const FTransform& TransformCapsule, const FShape& CapsuleB, const FTransform& TransformB);



    using OverlapFunc = bool(*) (const FShape&, const FTransform&, const FShape&, const FTransform&);

    extern OverlapFunc OverlapLUT[3][3];
    
    
    bool CheckOverlap(const UShapeComponent* A, const UShapeComponent* B);
}

