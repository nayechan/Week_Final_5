#include "pch.h"
#include "SkinningStats.h"
#include "GPUTimer.h"
#include <d3d11.h>

// 소멸자를 명시적으로 정의
FSkinningStatManager::~FSkinningStatManager()
{
	if (GPUDrawTimer)
	{
		delete GPUDrawTimer;
		GPUDrawTimer = nullptr;
	}
}

void FSkinningStatManager::InitializeGPUTimer(void* Device)
{
	if (!GPUDrawTimer)
	{
		GPUDrawTimer = new FGPUTimer();
	}

	ID3D11Device* D3DDevice = static_cast<ID3D11Device*>(Device);
	GPUDrawTimer->Initialize(D3DDevice);
}

void FSkinningStatManager::BeginGPUTimer(void* DeviceContext)
{
	if (GPUDrawTimer && GPUDrawTimer->IsInitialized())
	{
		ID3D11DeviceContext* D3DContext = static_cast<ID3D11DeviceContext*>(DeviceContext);
		GPUDrawTimer->Begin(D3DContext);
	}
}

void FSkinningStatManager::EndGPUTimer(void* DeviceContext)
{
	if (GPUDrawTimer && GPUDrawTimer->IsInitialized())
	{
		ID3D11DeviceContext* D3DContext = static_cast<ID3D11DeviceContext*>(DeviceContext);
		GPUDrawTimer->End(D3DContext);
	}
}

double FSkinningStatManager::GetGPUDrawTimeMS(void* DeviceContext)
{
	if (GPUDrawTimer && GPUDrawTimer->IsInitialized())
	{
		ID3D11DeviceContext* D3DContext = static_cast<ID3D11DeviceContext*>(DeviceContext);
		float GPUTimeMS = GPUDrawTimer->GetElapsedTimeMS(D3DContext);
		if (GPUTimeMS >= 0.0f)
		{
			LastGPUDrawTimeMS = static_cast<double>(GPUTimeMS);
		}
	}
	return LastGPUDrawTimeMS;
}
