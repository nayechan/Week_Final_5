#pragma once
#include "UEContainer.h"

// 섀도우 통계 구조체
// 씬의 섀도우 맵 관련 정보를 추적
struct FShadowStats
{
	// 섀도우를 캐스팅하는 라이트 개수
	uint32 TotalShadowCastingLights = 0;
	uint32 ShadowCastingPointLights = 0;
	uint32 ShadowCastingSpotLights = 0;
	uint32 ShadowCastingDirectionalLights = 0;

	// 섀도우 맵 아틀라스 정보
	uint32 ShadowAtlas2DSize = 0;         // 2D 아틀라스 해상도 (Spot/Directional용)
	uint32 ShadowAtlasCubeSize = 0;       // 큐브맵 아틀라스 해상도 (Point Light용)
	uint32 ShadowCubeArrayCount = 0;      // 큐브맵 배열 개수

	// 메모리 사용량 (MB)
	float ShadowAtlas2DMemoryMB = 0.0f;
	float ShadowAtlasCubeMemoryMB = 0.0f;
	float TotalShadowMemoryMB = 0.0f;

	// 모든 통계를 0으로 리셋
	void Reset()
	{
		TotalShadowCastingLights = 0;
		ShadowCastingPointLights = 0;
		ShadowCastingSpotLights = 0;
		ShadowCastingDirectionalLights = 0;
		ShadowAtlas2DSize = 0;
		ShadowAtlasCubeSize = 0;
		ShadowCubeArrayCount = 0;
		ShadowAtlas2DMemoryMB = 0.0f;
		ShadowAtlasCubeMemoryMB = 0.0f;
		TotalShadowMemoryMB = 0.0f;
	}

	// 전체 섀도우 캐스팅 라이트 수 계산
	void CalculateTotal()
	{
		TotalShadowCastingLights = ShadowCastingPointLights + ShadowCastingSpotLights + ShadowCastingDirectionalLights;
		TotalShadowMemoryMB = ShadowAtlas2DMemoryMB + ShadowAtlasCubeMemoryMB;
	}

	// 메모리 계산 (2D 아틀라스)
	void Calculate2DAtlasMemory()
	{
		// DXGI_FORMAT_R32_TYPELESS (32-bit depth) = 4 bytes per pixel
		if (ShadowAtlas2DSize > 0)
		{
			uint64 BytesUsed = (uint64)ShadowAtlas2DSize * ShadowAtlas2DSize * 4;
			ShadowAtlas2DMemoryMB = (float)BytesUsed / (1024.0f * 1024.0f);
		}
	}

	// 메모리 계산 (큐브맵 아틀라스)
	void CalculateCubeAtlasMemory()
	{
		// TextureCubeArray: Size x Size x 6 faces x ArrayCount
		// DXGI_FORMAT_R32_TYPELESS (32-bit depth) = 4 bytes per pixel
		if (ShadowAtlasCubeSize > 0 && ShadowCubeArrayCount > 0)
		{
			uint64 BytesUsed = (uint64)ShadowAtlasCubeSize * ShadowAtlasCubeSize * 6 * ShadowCubeArrayCount * 4;
			ShadowAtlasCubeMemoryMB = (float)BytesUsed / (1024.0f * 1024.0f);
		}
	}
};

// 섀도우 통계 전역 매니저 (싱글톤)
// UStatsOverlayD2D에서 접근할 수 있도록 전역 통계 제공
class FShadowStatManager
{
public:
	static FShadowStatManager& GetInstance()
	{
		static FShadowStatManager Instance;
		return Instance;
	}

	// 통계 업데이트
	void UpdateStats(const FShadowStats& InStats)
	{
		CurrentStats = InStats;
	}

	// 통계 조회
	const FShadowStats& GetStats() const
	{
		return CurrentStats;
	}

	// 통계 리셋
	void ResetStats()
	{
		CurrentStats.Reset();
	}

private:
	FShadowStatManager() = default;
	~FShadowStatManager() = default;
	FShadowStatManager(const FShadowStatManager&) = delete;
	FShadowStatManager& operator=(const FShadowStatManager&) = delete;

	FShadowStats CurrentStats;
};
