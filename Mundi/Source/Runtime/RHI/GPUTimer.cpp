#include "pch.h"
#include "GPUTimer.h"

FGPUTimer::FGPUTimer()
{
}

FGPUTimer::~FGPUTimer()
{
	Release();
}

bool FGPUTimer::Initialize(ID3D11Device* Device)
{
	if (!Device)
	{
		return false;
	}

	Release(); // 기존 리소스 해제

	// 링버퍼용 쿼리 세트 생성
	for (int i = 0; i < NUM_QUERIES; ++i)
	{
		// 타임스탬프 쿼리 생성 (시작 시점)
		D3D11_QUERY_DESC QueryDesc = {};
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP;
		QueryDesc.MiscFlags = 0;

		HRESULT hr = Device->CreateQuery(&QueryDesc, &QueryBegin[i]);
		if (FAILED(hr))
		{
			Release();
			return false;
		}

		// 타임스탬프 쿼리 생성 (종료 시점)
		hr = Device->CreateQuery(&QueryDesc, &QueryEnd[i]);
		if (FAILED(hr))
		{
			Release();
			return false;
		}

		// Disjoint 쿼리 생성 (주파수 및 유효성 확인용)
		QueryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		hr = Device->CreateQuery(&QueryDesc, &QueryDisjoint[i]);
		if (FAILED(hr))
		{
			Release();
			return false;
		}
	}

	CurrentQueryIndex = 0;
	bInitialized = true;
	return true;
}

void FGPUTimer::Begin(ID3D11DeviceContext* DeviceContext)
{
	if (!bInitialized || !DeviceContext)
	{
		return;
	}

	// 현재 쿼리 인덱스의 쿼리 사용
	int Index = CurrentQueryIndex;

	// Disjoint 쿼리 시작 (주파수 측정 시작)
	DeviceContext->Begin(QueryDisjoint[Index]);

	// 시작 타임스탬프 기록
	DeviceContext->End(QueryBegin[Index]);
}

void FGPUTimer::End(ID3D11DeviceContext* DeviceContext)
{
	if (!bInitialized || !DeviceContext)
	{
		return;
	}

	// 현재 쿼리 인덱스의 쿼리 사용
	int Index = CurrentQueryIndex;

	// 종료 타임스탬프 기록
	DeviceContext->End(QueryEnd[Index]);

	// Disjoint 쿼리 종료
	DeviceContext->End(QueryDisjoint[Index]);

	// 다음 프레임을 위해 인덱스 증가 (링버퍼)
	CurrentQueryIndex = (CurrentQueryIndex + 1) % NUM_QUERIES;
}

float FGPUTimer::GetElapsedTimeMS(ID3D11DeviceContext* DeviceContext)
{
	if (!bInitialized || !DeviceContext)
	{
		return -1.0f;
	}

	// N-3 프레임의 쿼리 결과 읽기 (비동기 처리, GPU가 충분히 완료할 시간 제공)
	// CurrentQueryIndex는 다음에 쓸 인덱스이므로, -3이 3프레임 전에 완료된 쿼리
	int ReadIndex = (CurrentQueryIndex - 3 + NUM_QUERIES) % NUM_QUERIES;

	// 비동기 쿼리 결과 대기 (제한된 재시도)
	UINT64 BeginTime = 0;
	UINT64 EndTime = 0;
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};

	// Disjoint 데이터가 준비될 때까지 대기 (최대 100회 시도)
	HRESULT hr = S_FALSE;
	for (int i = 0; i < 100 && hr != S_OK; ++i)
	{
		hr = DeviceContext->GetData(QueryDisjoint[ReadIndex], &DisjointData, sizeof(DisjointData), 0);
	}
	if (hr != S_OK)
	{
		// 데이터가 아직 준비되지 않음
		return LastElapsedMS; // 이전 값 반환
	}

	// GPU가 타임스탬프 측정 중 일시 중단되지 않았는지 확인
	if (DisjointData.Disjoint)
	{
		// 타임스탬프가 불연속적임 (신뢰할 수 없음)
		return LastElapsedMS;
	}

	// 시작/종료 타임스탬프 가져오기 (최대 100회 시도)
	hr = S_FALSE;
	for (int i = 0; i < 100 && hr != S_OK; ++i)
	{
		hr = DeviceContext->GetData(QueryBegin[ReadIndex], &BeginTime, sizeof(UINT64), 0);
	}
	if (hr != S_OK)
	{
		return LastElapsedMS;
	}

	hr = S_FALSE;
	for (int i = 0; i < 100 && hr != S_OK; ++i)
	{
		hr = DeviceContext->GetData(QueryEnd[ReadIndex], &EndTime, sizeof(UINT64), 0);
	}
	if (hr != S_OK)
	{
		return LastElapsedMS;
	}

	// 타임스탬프 차이를 밀리초로 변환
	// 주파수: Ticks per second
	UINT64 Delta = EndTime - BeginTime;
	double FrequencyInverse = 1.0 / static_cast<double>(DisjointData.Frequency);
	double ElapsedSeconds = static_cast<double>(Delta) * FrequencyInverse;
	float ElapsedMS = static_cast<float>(ElapsedSeconds * 1000.0);

	// 캐시 업데이트
	LastElapsedMS = ElapsedMS;

	return ElapsedMS;
}

void FGPUTimer::Release()
{
	// 모든 쿼리 해제
	for (int i = 0; i < NUM_QUERIES; ++i)
	{
		if (QueryBegin[i])
		{
			QueryBegin[i]->Release();
			QueryBegin[i] = nullptr;
		}

		if (QueryEnd[i])
		{
			QueryEnd[i]->Release();
			QueryEnd[i] = nullptr;
		}

		if (QueryDisjoint[i])
		{
			QueryDisjoint[i]->Release();
			QueryDisjoint[i] = nullptr;
		}
	}

	bInitialized = false;
	CurrentQueryIndex = 0;
	LastElapsedMS = 0.0f;
}
