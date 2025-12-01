#pragma once
#include "Object.h"
#include "RHIDevice.h"

enum ERenderTexSizeType
{
	Resolution,		//해상도 비례
	Fixed			//고정
};

class D3D11RHI;
using namespace Microsoft::WRL;
class URenderTexture : public UObject
{
	DECLARE_CLASS(URenderTexture, UObject)
public:
	URenderTexture() = default;
	~URenderTexture() = default;
	
	void InitResolution(ID3D11Device* Device, const float InResolution, ID3D11Texture2D* FrameBufferTex);
	void InitResolution(D3D11RHI* RHIDevice, const float InResolution);
	void InitFixedSize(ID3D11Device* Device, const uint32 InWidth, const uint32 InHeight);
	void InitFixedSize(D3D11RHI* RHIDevice, const uint32 InWidth, const uint32 InHeight);

	float GetResolution() const { return Resolution; }
	ERenderTexSizeType GetType() const { return RenderTexSizeType; }
	ID3D11ShaderResourceView* GetSRV() { return SRV.Get(); }
	ID3D11RenderTargetView* GetRTV() { return RTV.Get(); }
private:
	friend class D3D11RHI;
	ComPtr<ID3D11Texture2D> TexResource;
	ComPtr<ID3D11ShaderResourceView> SRV;
	ComPtr<ID3D11RenderTargetView> RTV;

	float Resolution = 0.0f;
	uint32 TexWidth = 0;
	uint32 TexHeight = 0;
	ERenderTexSizeType RenderTexSizeType = ERenderTexSizeType::Resolution;

	void CreateResources(ID3D11Device* Device, const uint32 Width, const uint32 Height);
};