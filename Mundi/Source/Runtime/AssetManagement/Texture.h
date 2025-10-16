#pragma once
#include "ResourceBase.h"
#include <d3d11.h>

class UTexture : public UResourceBase
{
public:
	DECLARE_CLASS(UTexture, UResourceBase)

	UTexture();
	virtual ~UTexture();

	void Load(const FString& InFilePath, ID3D11Device* InDevice);

	ID3D11ShaderResourceView* GetShaderResourceView() const { return ShaderResourceView; }
	ID3D11Texture2D* GetTexture2D() const { return Texture2D; }

	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }
	DXGI_FORMAT GetFormat() const { return Format; }
	void SetTextureName(const FString& InTextureName) { TextureName = InTextureName; }
	const FString& GetTextureName() { return  TextureName; }
	void ReleaseResources();

private:
	FString TextureName;
	ID3D11Texture2D* Texture2D;
	ID3D11ShaderResourceView* ShaderResourceView;

	uint32 Width = 0;
	uint32 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};
