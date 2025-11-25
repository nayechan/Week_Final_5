#include "pch.h"
#include "ParticleStats.h"

void FParticleStatManager::UpdateStats(const FParticleStats& InStats)
{
    CurrentStats = InStats;

    int32 Total = InStats.TotalParticleCount();

    // 링 버퍼에 저장
    RecentCounts[WindowIndex] = Total;
    WindowIndex = (WindowIndex + 1) % WINDOW_SIZE;
    if (WindowIndex == 0) bWindowFilled = true;

    // Min/Max/Avg 계산 (윈도우 내 스캔)
    int32 Count = bWindowFilled ? WINDOW_SIZE : WindowIndex;
    if (Count == 0) Count = 1;  // 0으로 나누기 방지

    MinParticles = INT_MAX;
    MaxParticles = 0;
    int64 Sum = 0;

    for (int32 i = 0; i < Count; i++)
    {
        int32 Val = RecentCounts[i];
        if (Val < MinParticles) MinParticles = Val;
        if (Val > MaxParticles) MaxParticles = Val;
        Sum += Val;
    }

    if (MinParticles == INT_MAX) MinParticles = 0;
    AvgParticles = static_cast<float>(Sum) / Count;
}

void FParticleStatManager::ResetStats()
{
    CurrentStats.Reset();
    for (int32 i = 0; i < WINDOW_SIZE; i++) RecentCounts[i] = 0;
    WindowIndex = 0;
    bWindowFilled = false;
    MinParticles = 0;
    MaxParticles = 0;
    AvgParticles = 0.0f;
}
