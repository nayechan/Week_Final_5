#pragma once
#include "ResourceBase.h"
#include "Enums.h"

class UShader;
class UTexture;

// 텍스처 슬롯을 명확하게 구분하기 위한 Enum (선택 사항이지만 권장)
enum class EMaterialTextureSlot : uint8
{
    Diffuse = 0,
    Normal,
    //Specular,
    //Emissive,
    // ... 기타 슬롯 ...
    Max // 배열 크기 지정용
};

class UMaterial : public UResourceBase
{
	DECLARE_CLASS(UMaterial, UResourceBase)
public:
    UMaterial();
    void Load(const FString& InFilePath, ID3D11Device* InDevice);

protected:
    ~UMaterial() override = default;

public:
    void SetShader(UShader* ShaderResource);
    UShader* GetShader();

    void SetDiffuseTexture(const FString& TexturePath);
    UTexture* GetDiffuseTexture();

    // MaterialInfo의 텍스처 경로들을 기반으로 ResolvedTextures 배열을 채웁니다.
    void ResolveTextures();

    void SetMaterialInfo(const FMaterialParameters& InMaterialInfo)
    { 
        MaterialInfo = InMaterialInfo;
        ResolveTextures();
    }
    
    void SetTexture(EMaterialTextureSlot Slot, const FString& TexturePath);
    const FMaterialParameters& GetMaterialInfo() const { return MaterialInfo; }

    UTexture* GetTexture(EMaterialTextureSlot Slot) const;

private:
    // 이 머티리얼이 사용할 셰이더 프로그램 (예: UberLit.hlsl)
	UShader* Shader = nullptr;
    FMaterialParameters MaterialInfo;
    // MaterialInfo 이름 기반으로 찾은 (Textures[0] = Diffuse, Textures[1] = Normal)
    TArray<UTexture*> ResolvedTextures;
};

