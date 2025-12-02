#include "pch.h"
#include "RenderTexture.h"
#include "RHIDevice.h"

IMPLEMENT_CLASS(URenderTexture)


void URenderTexture::InitResolution(D3D11RHI* RHIDevice, const float InResolution)
{
	InitResolution(RHIDevice->GetDevice(), InResolution, RHIDevice->GetFrameBufferTex());
}
void URenderTexture::InitResolution(ID3D11Device* Device, const float InResolution, ID3D11Texture2D* FrameBufferTex)
{
	if (InResolution <= 0)
	{
		return;
	}		
	D3D11_TEXTURE2D_DESC TexDesc;
	FrameBufferTex->GetDesc(&TexDesc);
	uint32 ResizeWidth = static_cast<uint32>(roundf(TexDesc.Width * InResolution));
	uint32 ResizeHeight = static_cast<uint32>(roundf(TexDesc.Height * InResolution));
	if (Resolution != InResolution || TexWidth != ResizeWidth || TexHeight != ResizeHeight)
	{
		Resolution = InResolution;
		RenderTexSizeType = ERenderTexSizeType::Resolution;

		TexWidth = ResizeWidth;
		TexHeight = ResizeHeight;
		CreateResources(Device, TexWidth, TexHeight);
	}
}
void URenderTexture::InitFixedSize(D3D11RHI* RHIDevice, const uint32 InWidth, const uint32 InHeight)
{
	InitFixedSize(RHIDevice->GetDevice(), InWidth, InHeight);
}
void URenderTexture::InitFixedSize(ID3D11Device* Device, const uint32 InWidth, const uint32 InHeight)
{
	if (TexWidth != InWidth || TexHeight != InHeight)
	{
		TexWidth = InWidth;
		TexHeight = InHeight;
		RenderTexSizeType = ERenderTexSizeType::Fixed;
		CreateResources(Device, TexWidth, TexHeight);
	}
}

void URenderTexture::CreateResources(ID3D11Device* Device, const uint32 Width, const uint32 Height)
{
	D3D11_TEXTURE2D_DESC TexDesc = {};
	TexDesc.Width = Width;
	TexDesc.Height = Height;
	TexDesc.MipLevels = 1;
	TexDesc.ArraySize = 1;
	TexDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;

	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	RTVDesc.Texture2D.MipSlice = 0;

	if (TexResource.Get() != nullptr) 
	{
		TexResource.Get()->Release();
		SRV.Get()->Release();
		RTV.Get()->Release();
	}

	Device->CreateTexture2D(&TexDesc, nullptr, TexResource.GetAddressOf());
	Device->CreateShaderResourceView(TexResource.Get(), &SRVDesc, SRV.GetAddressOf());
	Device->CreateRenderTargetView(TexResource.Get(), &RTVDesc, RTV.GetAddressOf());
}
