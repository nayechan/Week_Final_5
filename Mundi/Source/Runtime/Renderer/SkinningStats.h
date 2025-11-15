#pragma once

#include <cstdint>

// 전방 선언
class FGPUTimer;

/**
 * 스키닝 통계 구조체
 * CPU/GPU 스키닝의 성능 메트릭을 저장합니다.
 */
struct FSkinningStats
{
	// === CPU 스키닝 메트릭 ===

	// CPU 스키닝 메시 개수
	uint32_t CPUSkinnedMeshCount = 0;

	// CPU 스키닝 시간 (밀리초)
	double CPUBoneMatrixCalcTimeMS = 0.0;     // 본 행렬 계산 시간
	double CPUVertexSkinningTimeMS = 0.0;      // 버텍스 스키닝 계산 시간
	double CPUVertexBufferUploadTimeMS = 0.0;  // 버텍스 버퍼 업로드 시간 (Map/Unmap)
	double CPUGPUDrawTimeMS = 0.0;             // CPU 모드에서의 GPU draw 시간

	// CPU 스키닝 카운트
	uint32_t CPUTotalVertices = 0;             // 처리한 총 버텍스 수
	uint32_t CPUTotalBones = 0;                // 처리한 총 본 수
	uint32_t CPUBufferUpdateCount = 0;         // 버퍼 업데이트 횟수

	// CPU 메모리 사용량 (바이트)
	uint64_t CPUVertexBufferMemory = 0;        // 동적 버텍스 버퍼 메모리

	// === GPU 스키닝 메트릭 ===

	// GPU 스키닝 메시 개수
	uint32_t GPUSkinnedMeshCount = 0;

	// GPU 스키닝 시간 (밀리초)
	double GPUBoneMatrixCalcTimeMS = 0.0;      // 본 행렬 계산 시간 (CPU 측)
	double GPUBoneBufferUploadTimeMS = 0.0;    // 본 버퍼 업로드 시간 (Map/Unmap)
	double GPUDrawTimeMS = 0.0;                // GPU draw 시간 (전체 RenderOpaquePass)

	// GPU 스키닝 카운트
	uint32_t GPUTotalVertices = 0;             // 처리한 총 버텍스 수
	uint32_t GPUTotalBones = 0;                // 처리한 총 본 수
	uint32_t GPUBufferUpdateCount = 0;         // 본 버퍼 업데이트 횟수

	// GPU 메모리 사용량 (바이트)
	uint64_t GPUBoneBufferMemory = 0;          // 본 행렬 상수 버퍼 메모리

	// === 공통 메트릭 ===

	uint32_t TotalSkeletalMeshCount = 0;       // 전체 스켈레탈 메시 개수

	/**
	 * 모든 통계를 0으로 리셋
	 */
	void Reset()
	{
		CPUSkinnedMeshCount = 0;
		CPUBoneMatrixCalcTimeMS = 0.0;
		CPUVertexSkinningTimeMS = 0.0;
		CPUVertexBufferUploadTimeMS = 0.0;
		CPUGPUDrawTimeMS = 0.0;
		CPUTotalVertices = 0;
		CPUTotalBones = 0;
		CPUBufferUpdateCount = 0;
		CPUVertexBufferMemory = 0;

		GPUSkinnedMeshCount = 0;
		GPUBoneMatrixCalcTimeMS = 0.0;
		GPUBoneBufferUploadTimeMS = 0.0;
		GPUDrawTimeMS = 0.0;
		GPUTotalVertices = 0;
		GPUTotalBones = 0;
		GPUBufferUpdateCount = 0;
		GPUBoneBufferMemory = 0;

		TotalSkeletalMeshCount = 0;
	}

	/**
	 * CPU 스키닝 총 시간 계산
	 * @return CPU 본 계산 + 버텍스 스키닝 + 버퍼 업로드 + GPU draw 시간
	 */
	double GetCPUTotalTimeMS() const
	{
		return CPUBoneMatrixCalcTimeMS + CPUVertexSkinningTimeMS + CPUVertexBufferUploadTimeMS + CPUGPUDrawTimeMS;
	}

	/**
	 * GPU 스키닝 총 시간 계산
	 * @return GPU 본 계산 + 본 버퍼 업로드 + GPU draw 시간
	 */
	double GetGPUTotalTimeMS() const
	{
		return GPUBoneMatrixCalcTimeMS + GPUBoneBufferUploadTimeMS + GPUDrawTimeMS;
	}

	/**
	 * CPU 스키닝 메시당 평균 시간 계산
	 */
	double GetCPUAverageTimePerMeshMS() const
	{
		if (CPUSkinnedMeshCount == 0) return 0.0;
		return GetCPUTotalTimeMS() / static_cast<double>(CPUSkinnedMeshCount);
	}

	/**
	 * GPU 스키닝 메시당 평균 시간 계산
	 */
	double GetGPUAverageTimePerMeshMS() const
	{
		if (GPUSkinnedMeshCount == 0) return 0.0;
		return GetGPUTotalTimeMS() / static_cast<double>(GPUSkinnedMeshCount);
	}

	/**
	 * 전체 카운트 업데이트
	 * @param bIsCPUMode 현재 CPU 모드인지 여부 (true: CPU 모드, false: GPU 모드)
	 */
	void UpdateTotalCounts(bool bIsCPUMode)
	{
		// 현재 활성 모드의 카운트만 Total로 설정 (CPU + GPU 합산 X)
		TotalSkeletalMeshCount = bIsCPUMode ? CPUSkinnedMeshCount : GPUSkinnedMeshCount;
	}
};


/**
 * 스키닝 통계 전역 매니저 (싱글톤)
 * UStatsOverlayD2D에서 접근할 수 있도록 전역 통계 제공
 */
class FSkinningStatManager
{
public:
	static FSkinningStatManager& GetInstance()
	{
		static FSkinningStatManager Instance;
		return Instance;
	}

	/**
	 * 매 프레임 렌더링 시작 시 호출하여 프레임 단위 통계 데이터를 초기화합니다.
	 * @param bResetCPU true: CPU 모드 통계 리셋 (GPU는 마지막 값 유지)
	 *                  false: GPU 모드 통계 리셋 (CPU는 마지막 값 유지)
	 */
	void ResetFrameStats(bool bResetCPU)
	{
		if (bResetCPU)
		{
			// CPU 모드: CPU 통계만 리셋, GPU는 마지막 값 유지
			CurrentStats.CPUSkinnedMeshCount = 0;
			CurrentStats.CPUBoneMatrixCalcTimeMS = 0.0;
			CurrentStats.CPUVertexSkinningTimeMS = 0.0;
			CurrentStats.CPUVertexBufferUploadTimeMS = 0.0;
			CurrentStats.CPUGPUDrawTimeMS = 0.0;
			CurrentStats.CPUTotalVertices = 0;
			CurrentStats.CPUTotalBones = 0;
			CurrentStats.CPUBufferUpdateCount = 0;
			CurrentStats.CPUVertexBufferMemory = 0;
		}
		else
		{
			// GPU 모드: GPU 통계만 리셋, CPU는 마지막 값 유지
			CurrentStats.GPUSkinnedMeshCount = 0;
			CurrentStats.GPUBoneMatrixCalcTimeMS = 0.0;
			CurrentStats.GPUBoneBufferUploadTimeMS = 0.0;
			CurrentStats.GPUDrawTimeMS = 0.0;
			CurrentStats.GPUTotalVertices = 0;
			CurrentStats.GPUTotalBones = 0;
			CurrentStats.GPUBufferUpdateCount = 0;
			CurrentStats.GPUBoneBufferMemory = 0;
		}

		CurrentStats.TotalSkeletalMeshCount = 0;
	}

	/**
	 * 통계 조회
	 */
	const FSkinningStats& GetStats() const
	{
		return CurrentStats;
	}

	/**
	 * 수정 가능한 통계 참조 반환 (데이터 수집용)
	 */
	FSkinningStats& GetMutableStats()
	{
		return CurrentStats;
	}

	// === CPU 스키닝 데이터 추가 메서드 ===

	void AddCPUMesh(uint32_t VertexCount, uint32_t BoneCount, uint64_t VertexBufferSize)
	{
		CurrentStats.CPUSkinnedMeshCount++;
		CurrentStats.CPUTotalVertices += VertexCount;
		CurrentStats.CPUTotalBones += BoneCount;
		CurrentStats.CPUVertexBufferMemory += VertexBufferSize;
	}

	void AddCPUBoneMatrixCalcTime(double TimeMS)
	{
		CurrentStats.CPUBoneMatrixCalcTimeMS += TimeMS;
	}

	void AddCPUVertexSkinningTime(double TimeMS)
	{
		CurrentStats.CPUVertexSkinningTimeMS += TimeMS;
	}

	void AddCPUBufferUploadTime(double TimeMS)
	{
		CurrentStats.CPUVertexBufferUploadTimeMS += TimeMS;
		CurrentStats.CPUBufferUpdateCount++;
	}

	void AddCPUGPUDrawTime(double TimeMS)
	{
		CurrentStats.CPUGPUDrawTimeMS += TimeMS;
	}

	// === GPU 스키닝 데이터 추가 메서드 ===

	void AddGPUMesh(uint32_t VertexCount, uint32_t BoneCount, uint64_t BoneBufferSize)
	{
		CurrentStats.GPUSkinnedMeshCount++;
		CurrentStats.GPUTotalVertices += VertexCount;
		CurrentStats.GPUTotalBones += BoneCount;
		CurrentStats.GPUBoneBufferMemory += BoneBufferSize;
	}

	void AddGPUBoneMatrixCalcTime(double TimeMS)
	{
		CurrentStats.GPUBoneMatrixCalcTimeMS += TimeMS;
	}

	void AddGPUBoneBufferUploadTime(double TimeMS)
	{
		CurrentStats.GPUBoneBufferUploadTimeMS += TimeMS;
		CurrentStats.GPUBufferUpdateCount++;
	}

	void AddGPUDrawTime(double TimeMS)
	{
		CurrentStats.GPUDrawTimeMS += TimeMS;
	}

	// === GPU 타이머 관리 ===

	/**
	 * GPU 타이머 초기화 (Renderer에서 한 번만 호출)
	 */
	void InitializeGPUTimer(void* Device);

	/**
	 * GPU 타이머 시작 (RenderOpaquePass 전)
	 */
	void BeginGPUTimer(void* DeviceContext);

	/**
	 * GPU 타이머 종료 (RenderOpaquePass 후)
	 */
	void EndGPUTimer(void* DeviceContext);

	/**
	 * GPU 타이머 결과 가져오기 (다음 프레임에서 호출)
	 * @return GPU draw 시간 (밀리초), 준비되지 않았으면 -1.0
	 */
	double GetGPUDrawTimeMS(void* DeviceContext);

private:
	FSkinningStatManager() = default;
	~FSkinningStatManager(); // cpp에서 정의 (unique_ptr 완전한 타입 필요)
	FSkinningStatManager(const FSkinningStatManager&) = delete;
	FSkinningStatManager& operator=(const FSkinningStatManager&) = delete;

	FSkinningStats CurrentStats;

	// GPU 타이머 (전역에서 지속)
	FGPUTimer* GPUDrawTimer = nullptr;
	double LastGPUDrawTimeMS = 0.0;
};
