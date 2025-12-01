#pragma once

class AActor;
class UPrimitiveComponent;
class UPhysicalMaterial;

struct FHitResult
{
    // --- 충돌한 대상 ---
    AActor* Actor = nullptr;
    UPrimitiveComponent* Component = nullptr;
    class UPhysicalMaterial* PhysMaterial = nullptr;
    
    // --- 충돌 정보 ---
    float    Distance = 0.0f;
    FVector  ImpactPoint;           // 충돌 지점 (World)
    FVector  ImpactNormal;          // 충돌 표면의 법선 벡터
    FVector  Location;              // 충돌 이후 내 중심점
    
    // --- 디버깅용 ---
    FVector  TraceStart;            // 레이 시작점
    FVector  TraceEnd;              // 레이 끝점
    bool     bBlockingHit = false;

    // --- 맞춘 곳 ---
    FName BoneName;   // 내가 맞춘 상대방의 뼈 이름
    FName MyBoneName; // (선택) 부딪힌 나의 뼈 이름 (충돌 콜백용)
    int32 Item = -1;  // (참고) FaceIndex는 스태틱 메시용, BoneName은 스켈레탈 메시용

    FHitResult() : ImpactPoint(0,0,0), ImpactNormal(0,0,1) {}
    bool IsValid() const { return bBlockingHit && Actor != nullptr; }
};

struct FOverlapResult
{
    AActor* Actor = nullptr;
    UPrimitiveComponent* Component = nullptr;

    bool IsValid() const { return Actor != nullptr; }
};

struct FSweepResult : public FHitResult
{
    // 필요하면 Sweep 전용 데이터 추가
};
