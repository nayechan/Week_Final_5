#pragma once
#include <PxPhysicsAPI.h>

class UPrimitiveComponent;
struct FBodyInstance;
using namespace physx;

// ==================================================================================
// SCENE LOCK
// ==================================================================================

/**
 * @note NULL 체크를 위해서 PxSceneReadLock 대신에 이 RAII 클래스를 활용한다.
 */
class FPhysXSceneReadLock
{
public:
    FPhysXSceneReadLock(PxScene* PInScene, const char* filename, PxU32 lineno)
        : PScene(PInScene)
    {
        if (PScene)
        {
            PScene->lockRead(filename, lineno);
        }
    }

    ~FPhysXSceneReadLock()
    {
        if (PScene)
        {
            PScene->unlockRead();
        }
    }

private:
    PxScene* PScene;
};

/**
 * @note NULL 체크를 위해서 PxSceneWriteLock 대신에 이 RAII 클래스를 활용한다.
 */
class FPhysXSceneWriteLock
{
public:
    FPhysXSceneWriteLock(PxScene* PInScene, const char* filename, PxU32 lineno)
        : PScene(PInScene)
    {
        if (PScene)
        {
            PScene->lockWrite(filename, lineno);
        }
    }

    ~FPhysXSceneWriteLock()
    {
        if (PScene)
        {
            PScene->unlockWrite();
        }
    }

private:
    PxScene* PScene;
};

#define PREPROCESSOR_JOIN(x, y) PREPROCESSOR_JOIN_INNER(x, y)
#define PREPROCESSOR_JOIN_INNER(x, y) x##y

#define SCOPED_SCENE_READ_LOCK( _scene ) FPhysXSceneReadLock PREPROCESSOR_JOIN(_rlock, __LINE__)(_scene, __FILE__, __LINE__)
#define SCOPED_SCENE_WRITE_LOCK( _scene ) FPhysXSceneWriteLock PREPROCESSOR_JOIN(_wlock, __LINE__)(_scene, __FILE__, __LINE__)

// ==================================================================================
// BASIC TYPE CONVERSIONS
// ==================================================================================

inline PxVec3 U2PVector(const FVector& UVec)
{
    return PxVec3(UVec.X, UVec.Y, UVec.Z); 
}

inline PxVec4 U2PVector(const FVector4& UVec)
{
    return PxVec4(UVec.X, UVec.Y, UVec.Z, UVec.W);
}

inline PxQuat U2PQuat(const FQuat& UQuat)
{
    return PxQuat(UQuat.X, UQuat.Y, UQuat.Z, UQuat.W);
}

inline PxTransform U2PTransform(const FTransform& UTransform)
{
    return PxTransform(U2PVector(UTransform.Translation), U2PQuat(UTransform.Rotation));
}

/**
 * @note PhysX는 right-multiplication & column-major를 사용하고, 퓨처 엔진은 left-multiplication & row-major를 사용한다.
 *       따라서, 두 행렬은 메모리 상에서 동일하게 저장되므로 별도의 행렬 변환이 필요하지 않다.
 */
inline PxMat44 U2PMatrix(const FMatrix& UMat)
{
    PxMat44 Result;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            Result[r][c] = UMat.M[r][c];
        }
    }
    return Result;
}

inline FVector P2UVector(const PxVec3& PVec)
{
    return FVector(PVec.x, PVec.y, PVec.z);
}

inline FVector4 P2UVector(const PxVec4& PVec)
{
    return FVector4(PVec.x, PVec.y, PVec.z, PVec.w);
}

inline FQuat P2UQuat(const PxQuat& PQuat)
{
    return FQuat(PQuat.x, PQuat.y, PQuat.z, PQuat.w);
}

inline FTransform P2UTransform(const PxTransform& PTransform)
{
    return FTransform(
        P2UVector(PTransform.p),           // Translation
        P2UQuat(PTransform.q),             // Rotation
        FVector(1.f, 1.f, 1.f) // Scale (PhysX는 스케일 정보 없음)
    );
}

inline FMatrix P2UMatrix(const PxMat44& PMat)
{
    FMatrix Result;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            Result.M[r][c] = PMat[r][c];
        }
    }
    return Result;
}

// ==================================================================================

struct FHitResult
{
    /** 충돌 여부 (true면 충돌함) */
    bool bBlockingHit = false;

    /** 시작점으로부터 거리 */
    float Distance = 0.0f;

    /** 실제 표면과 닿은 접점 (Location과 다를 수 있음) */
    FVector ImpactPoint = FVector::Zero();

    /** 충돌 표면의 법선 벡터 */
    FVector ImpactNormal = FVector::Zero(); 

    /** 충돌한 액터 */
    AActor* Actor = nullptr;

    /** 충돌한 컴포넌트 */
    UPrimitiveComponent* Component = nullptr;

    /** 충돌한 본 이름 (헤드샷 판정 등에 필수) */
    FName BoneName;

    /** 물리 재질 (발소리, 피격 이펙트 구분용 - 필요시 추가) */
    // TWeakObjectPtr<UPhysicalMaterial> PhysMaterial; 

    void Init()
    {
        bBlockingHit = false;
        Distance = 0.0f;
        ImpactPoint = FVector::Zero();
        ImpactNormal = FVector::Zero();
        Actor = nullptr;
        Component = nullptr;
    }

    FHitResult() { Init(); }
};

/**
 * 강체 충돌 정보
 */
struct FRigidBodyCollisionInfo
{
    /** 충돌한 액터 */
    AActor* Actor;

    /** 충돌한 컴포넌트 */
    UPrimitiveComponent* Component;

    /** PhysicsAsset 내의 바디 인덱스 */
    int32 BodyIndex = -1;

    /** PhysicsAsset 내의 본 이름 */
    FName BoneName;

    /** FBodyInstance로부터 정보를 가져옴 */
    void SetFrom(FBodyInstance* BodyInst);

    /** 유효성 검사 */
    bool IsValid() const;
};

/** 물리 이벤트 타입 정의 */
enum class ECollisionNotifyType : uint8
{
    None,
    Hit,            // OnComponentHit 
    BeginOverlap,   // OnComponentBeginOverlap 
    EndOverlap,     // OnComponentEndOverlap 
    Wake,           // OnComponentWake 
    Sleep           // OnComponentSleep 
};

/** 
 * 대기 중인 충돌 알림 정보 (큐에 저장될 항목)
 */
struct FCollisionNotifyInfo
{
    /** 이벤트 타입 */
    ECollisionNotifyType Type = ECollisionNotifyType::None;
    
    /** 첫 번째 객체(Info0)에게 이벤트를 호출할지 여부 */
    bool bCallEvent0 = false;

    /** 두 번째 객체(Info1)에게 이벤트를 호출할지 여부 */
    bool bCallEvent1 = false;

    /** 첫 번째 객체 정보 */
    FRigidBodyCollisionInfo Info0;

    /** 두 번째 객체 정보 */
    FRigidBodyCollisionInfo Info1;

    /** 충돌 지점 (World Space) */
    FVector ImpactPoint = FVector::Zero();

    /**
     * 충돌 법선 (World Space)
     * @todo Info1에서 Info0를 향하는 방향 등 기준 통일 필요
     */
    FVector ImpactNormal = FVector::Zero();

    /** 충격량 크기 (물리적 반응, 사운드, 데미지 계산용) */
    float TotalImpulse = 0.0f;

    FCollisionNotifyInfo() = default;

    /** * 알림을 보낼 수 있는지 유효성 검사 
     * (양쪽 액터/컴포넌트가 삭제되지 않았는지 확인)
     */
    bool IsValidForNotify() const;
};