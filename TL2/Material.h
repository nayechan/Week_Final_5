#pragma once
#include "ResourceBase.h"
#include "Enums.h"

class UShader;
class UTexture;
class UMaterial : public UResourceBase
{
	DECLARE_CLASS(UMaterial, UResourceBase)
public:
    UMaterial() = default;
    void Load(const FString& InFilePath, ID3D11Device* InDevice);

protected:
    ~UMaterial() override = default;

public:
    void SetShader(UShader* ShaderResource);
    UShader* GetShader();

    void SetTexture(UTexture* TextureResource);
    void SetTexture(const FString& TexturePath);
    UTexture* GetTexture();

    void SetMaterialInfo(const FObjMaterialInfo& InMaterialInfo) { MaterialInfo = InMaterialInfo; }
    const FObjMaterialInfo& GetMaterialInfo() const { return MaterialInfo; }

    // TEST
    FString& GetTextName() { return TextureName; }
    void SetTextName(FString& InName) { TextureName = InName; };

private:
	UShader* Shader = nullptr;
	UTexture* Texture= nullptr;
    FObjMaterialInfo MaterialInfo;


    // TEST 
    FString TextureName;
};

