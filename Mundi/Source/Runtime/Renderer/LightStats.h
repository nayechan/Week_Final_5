#pragma once
#include "UEContainer.h"

// 라이트 통계 구조체
// 씬에 존재하는 라이트들의 타입별 개수를 추적
struct FLightStats
{
	// 라이트 개수 (타입별)
	uint32 TotalPointLights = 0;
	uint32 TotalSpotLights = 0;
	uint32 TotalDirectionalLights = 0;
	uint32 TotalAmbientLights = 0;
	uint32 TotalLights = 0;

	// 모든 통계를 0으로 리셋
	void Reset()
	{
		TotalPointLights = 0;
		TotalSpotLights = 0;
		TotalDirectionalLights = 0;
		TotalAmbientLights = 0;
		TotalLights = 0;
	}

	// 전체 라이트 수 계산
	void CalculateTotal()
	{
		TotalLights = TotalPointLights + TotalSpotLights + TotalDirectionalLights + TotalAmbientLights;
	}
};

// 라이트 통계 전역 매니저 (싱글톤)
// UStatsOverlayD2D에서 접근할 수 있도록 전역 통계 제공
class FLightStatManager
{
public:
	static FLightStatManager& GetInstance()
	{
		static FLightStatManager Instance;
		return Instance;
	}

	// 통계 업데이트
	void UpdateStats(const FLightStats& InStats)
	{
		CurrentStats = InStats;
	}

	// 통계 조회
	const FLightStats& GetStats() const
	{
		return CurrentStats;
	}

	// 통계 리셋
	void ResetStats()
	{
		CurrentStats.Reset();
	}

private:
	FLightStatManager() = default;
	~FLightStatManager() = default;
	FLightStatManager(const FLightStatManager&) = delete;
	FLightStatManager& operator=(const FLightStatManager&) = delete;

	FLightStats CurrentStats;
};
