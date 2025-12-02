#pragma once
#include "ClothUtil.h"
//#if USE_DX11
class DxContextManagerCallbackImpl : public nv::cloth::DxContextManagerCallback
{
public:
	DxContextManagerCallbackImpl(ID3D11Device* InDevice, bool synchronizeResources = false);
	~DxContextManagerCallbackImpl();
	virtual void acquireContext() override;
	virtual void releaseContext() override;
	virtual ID3D11Device* getDevice() const override;
	virtual ID3D11DeviceContext* getContext() const override;
	virtual bool synchronizeResources() const override;

private:
	ID3D11Device* Device;
	ID3D11DeviceContext* Context;
	bool mSynchronizeResources;
};