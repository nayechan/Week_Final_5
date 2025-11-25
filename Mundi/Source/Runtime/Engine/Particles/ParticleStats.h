#pragma once

#include <climits>

// 파티클 시스템 통계 데이터
struct FParticleStats
{
    int32 ParticleSystemCount = 0;  // 파티클 시스템 수
    int32 EmitterCount = 0;          // 이미터 수

    // 타입별 파티클 카운트
    int32 SpriteParticleCount = 0;   // 스프라이트 파티클 수
    int32 MeshParticleCount = 0;     // 메시 파티클 수
    int32 BeamParticleCount = 0;     // 빔 파티클 수
    int32 RibbonParticleCount = 0;   // 리본 파티클 수

    // 총 파티클 수 (타입별 합계)
    int32 TotalParticleCount() const
    {
        return SpriteParticleCount + MeshParticleCount + BeamParticleCount + RibbonParticleCount;
    }

    int32 SpawnedThisFrame = 0;      // 이번 프레임 생성 수
    int32 KilledThisFrame = 0;       // 이번 프레임 사망 수
    uint64 MemoryBytes = 0;          // 총 메모리 (바이트)

    void Reset() { *this = FParticleStats(); }
};

// 파티클 통계 매니저 (싱글톤)
class FParticleStatManager
{
public:
    static FParticleStatManager& GetInstance()
    {
        static FParticleStatManager Instance;
        return Instance;
    }

    void UpdateStats(const FParticleStats& InStats);
    const FParticleStats& GetStats() const { return CurrentStats; }

    // Min/Max/Avg 접근자 (최근 60프레임 기준)
    int32 GetMinParticles() const { return MinParticles; }
    int32 GetMaxParticles() const { return MaxParticles; }
    float GetAvgParticles() const { return AvgParticles; }

    void ResetStats();

private:
    FParticleStatManager() = default;
    ~FParticleStatManager() = default;
    FParticleStatManager(const FParticleStatManager&) = delete;
    FParticleStatManager& operator=(const FParticleStatManager&) = delete;

    FParticleStats CurrentStats;

    // 슬라이딩 윈도우 (최근 60프레임 = 약 1초)
    static constexpr int32 WINDOW_SIZE = 60;
    int32 RecentCounts[WINDOW_SIZE] = {0};
    int32 WindowIndex = 0;
    bool bWindowFilled = false;  // 윈도우가 한 번 이상 채워졌는지

    // 계산된 통계
    int32 MinParticles = 0;
    int32 MaxParticles = 0;
    float AvgParticles = 0.0f;
};
