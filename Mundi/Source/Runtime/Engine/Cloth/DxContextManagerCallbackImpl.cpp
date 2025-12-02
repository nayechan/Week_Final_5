/*
* Copyright (c) 2008-2017, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#include "pch.h"
#include "DxContextManagerCallbackImpl.h"

//#if USE_DX11
DxContextManagerCallbackImpl::DxContextManagerCallbackImpl(ID3D11Device* InDevice, bool synchronizeResources)
	:
	Device(InDevice),
	mSynchronizeResources(synchronizeResources)
{
	Device->AddRef();
	Device->GetImmediateContext(&Context);
}
DxContextManagerCallbackImpl::~DxContextManagerCallbackImpl()
{
	Context->Release();

#if _DEBUG
	ID3D11Debug* debugDevice;
	Context->QueryInterface(&debugDevice);
	if (debugDevice)
	{
		debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
		debugDevice->Release();
	}
#endif

	Context->Release();

}

void DxContextManagerCallbackImpl::acquireContext()
{

}
void DxContextManagerCallbackImpl::releaseContext()
{

}
ID3D11Device* DxContextManagerCallbackImpl::getDevice() const
{
	return Device;
}
ID3D11DeviceContext* DxContextManagerCallbackImpl::getContext() const
{
	return Context;
}
bool DxContextManagerCallbackImpl::synchronizeResources() const
{
	return mSynchronizeResources;
}
//#endif
