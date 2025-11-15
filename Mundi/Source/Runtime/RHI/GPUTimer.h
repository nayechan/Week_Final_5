#pragma once

#include <d3d11.h>
#include <stdint.h>

/**
 * D3D11 GPU 타이머 쿼리 클래스
 * GPU에서 실제로 실행된 시간을 측정합니다 (셰이더 실행 시간 등)
 *
 * 사용 예시:
 *   FGPUTimer Timer;
 *   Timer.Initialize(Device);
 *
 *   // 렌더링 프레임 시작
 *   Timer.Begin(DeviceContext);
 *   ... GPU 작업 (Draw calls 등)
 *   Timer.End(DeviceContext);
 *
 *   // 다음 프레임에서 결과 조회
 *   float ElapsedMS = Timer.GetElapsedTimeMS(DeviceContext);
 */
class FGPUTimer
{
public:
	FGPUTimer();
	~FGPUTimer();

	/**
	 * GPU 타이머 초기화
	 * @param Device D3D11 디바이스
	 * @return 성공 여부
	 */
	bool Initialize(ID3D11Device* Device);

	/**
	 * GPU 시간 측정 시작
	 * @param DeviceContext D3D11 디바이스 컨텍스트
	 */
	void Begin(ID3D11DeviceContext* DeviceContext);

	/**
	 * GPU 시간 측정 종료
	 * @param DeviceContext D3D11 디바이스 컨텍스트
	 */
	void End(ID3D11DeviceContext* DeviceContext);

	/**
	 * 측정된 시간을 밀리초 단위로 반환 (비동기)
	 * @param DeviceContext D3D11 디바이스 컨텍스트
	 * @return 경과 시간 (ms). 데이터가 아직 준비되지 않았으면 -1.0f 반환
	 */
	float GetElapsedTimeMS(ID3D11DeviceContext* DeviceContext);

	/**
	 * 리소스 해제
	 */
	void Release();

	/**
	 * 초기화 여부 확인
	 */
	bool IsInitialized() const { return bInitialized; }

private:
	// 링버퍼 크기 (비동기 GPU 타이머 처리를 위한 충분한 레이턴시 제공)
	static constexpr int NUM_QUERIES = 4;

	// GPU 타임스탬프 쿼리 링버퍼 (시작/끝)
	ID3D11Query* QueryBegin[NUM_QUERIES] = {};
	ID3D11Query* QueryEnd[NUM_QUERIES] = {};

	// 주파수 및 disjoint 체크용 쿼리 링버퍼
	ID3D11Query* QueryDisjoint[NUM_QUERIES] = {};

	// 현재 쿼리 인덱스 (쓰기용)
	int CurrentQueryIndex = 0;

	// 초기화 여부
	bool bInitialized = false;

	// 마지막으로 측정된 시간 (캐시)
	float LastElapsedMS = 0.0f;
};


/**
 * 스코프 기반 GPU 타이머
 * RAII 패턴으로 Begin/End 자동 호출
 *
 * 사용 예시:
 *   {
 *       FScopedGPUTimer ScopedTimer(GPUTimer, DeviceContext);
 *       ... GPU 작업
 *   } // 스코프 종료 시 자동으로 End() 호출
 */
class FScopedGPUTimer
{
public:
	FScopedGPUTimer(FGPUTimer& InTimer, ID3D11DeviceContext* InContext)
		: Timer(InTimer)
		, Context(InContext)
	{
		Timer.Begin(Context);
	}

	~FScopedGPUTimer()
	{
		Timer.End(Context);
	}

private:
	FGPUTimer& Timer;
	ID3D11DeviceContext* Context;
};
