#include "pch.h"
#include "Material.h"
#include "Shader.h"
#include "Texture.h"
#include "ResourceManager.h"

IMPLEMENT_CLASS(UMaterial)

UMaterial::UMaterial()
{
    // 배열 크기를 미리 할당 (Enum 값 사용)
    ResolvedTextures.resize(static_cast<size_t>(EMaterialTextureSlot::Max));

    // 기본 Material 생성 (기본 Phong 셰이더 사용)
    FString ShaderPath = "Shaders/Materials/UberLit.hlsl";
    TArray<FShaderMacro> DefaultMacros;
    DefaultMacros.push_back(FShaderMacro{ "LIGHTING_MODEL_PHONG", "1" });

    UShader* DefaultShader = UResourceManager::GetInstance().Load<UShader>(ShaderPath, DefaultMacros);
    if (DefaultShader)
    {
        SetShader(DefaultShader);
    }
}

void UMaterial::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    // 기본 쉐이더 로드 (LayoutType에 따라)
    // dds 의 경우 
    if (InFilePath.find(".dds") != std::string::npos)
    {
        FString shaderName = UResourceManager::GetInstance().GetProperShader(InFilePath);

        Shader = UResourceManager::GetInstance().Load<UShader>(shaderName);
        UResourceManager::GetInstance().Load<UTexture>(InFilePath);
        MaterialInfo.DiffuseTextureFileName = InFilePath;
    } // hlsl 의 경우 
    else if (InFilePath.find(".hlsl") != std::string::npos)
    {
        Shader = UResourceManager::GetInstance().Load<UShader>(InFilePath);
    }
    else
    {
        throw std::runtime_error(".dds나 .hlsl만 입력해주세요. 현재 입력 파일명 : " + InFilePath);
    }
}

void UMaterial::SetShader( UShader* ShaderResource) {
    
	Shader = ShaderResource;
}

UShader* UMaterial::GetShader()
{
	return Shader;
}

void UMaterial::SetDiffuseTexture(const FString& TexturePath)
{
    MaterialInfo.DiffuseTextureFileName = TexturePath;
}

UTexture* UMaterial::GetDiffuseTexture()
{
    return UResourceManager::GetInstance().Load<UTexture>(MaterialInfo.DiffuseTextureFileName);
}

void UMaterial::SetTexture(EMaterialTextureSlot Slot, const FString& TexturePath)
{
    // 1. 슬롯 인덱스 계산
    int32 Index = static_cast<int32>(Slot); // TArray 사용 시
    // size_t Index = static_cast<size_t>(Slot); // std::vector 사용 시

    // 2. 인덱스 유효성 검사 및 배열 크기 확인/조정
    if (Index >= ResolvedTextures.size())
    {
        if (Index >= ResolvedTextures.Num())
        {
            ResolvedTextures.resize(Index + 1, nullptr);
        }
        else
        {
            UE_LOG("SetTexture: Invalid texture slot index: %d", Index);
            return;
        }
    }

    // 3. [핵심] ResourceManager를 통해 텍스처 로드
    UTexture* LoadedTexture = nullptr;
    if (!TexturePath.empty()) // 경로가 비어있지 않으면 로드 시도
    {
        LoadedTexture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
        if (!LoadedTexture)
        {
            UE_LOG("SetTexture: Failed to load texture from path: %s", TexturePath.c_str());
            // 로드 실패 시 null 또는 기본 텍스처로 설정할 수 있음
            // LoadedTexture = UResourceManager::GetInstance().GetDefaultTexture();
        }
    }
    // 경로가 비어 있으면 LoadedTexture는 nullptr 유지 (텍스처 제거 의미)

    // 4. 배열에 로드된 포인터 저장
    ResolvedTextures[Index] = LoadedTexture;

    // 5. MaterialInfo의 파일 이름도 동기화 (로드 성공 여부와 관계없이 경로 저장)
    switch (Slot)
    {
    case EMaterialTextureSlot::Diffuse:
        MaterialInfo.DiffuseTextureFileName = TexturePath;
        break;
    case EMaterialTextureSlot::Normal:
        MaterialInfo.NormalTextureFileName = TexturePath;
        break;
    //case EMaterialTextureSlot::Specular:
    //    MaterialInfo.SpecularTextureFileName = TexturePath;
    //    break;
    //case EMaterialTextureSlot::Emissive:
    //    MaterialInfo.EmissiveTextureFileName = TexturePath;
    //    break;
        // ... 다른 슬롯 ...
    default:
        UE_LOG("SetTexture: Unknown texture slot: %d", Index);
        break;
    }
}

UTexture* UMaterial::GetTexture(EMaterialTextureSlot Slot) const
{
    size_t Index = static_cast<size_t>(Slot);
    // std::vector의 유효 인덱스 검사
    if (Index < ResolvedTextures.size())
    {
        return ResolvedTextures[Index];
    }
    return nullptr;
}

void UMaterial::ResolveTextures()
{
    auto& RM = UResourceManager::GetInstance();
    size_t MaxSlots = static_cast<size_t>(EMaterialTextureSlot::Max);

    // 배열 크기가 Enum 크기와 맞는지 확인 (안전장치)
    if (ResolvedTextures.size() != MaxSlots)
    {
        ResolvedTextures.resize(MaxSlots);
    }

    // 각 슬롯에 해당하는 텍스처 경로로 UTexture* 찾아서 배열에 저장
    if (!MaterialInfo.DiffuseTextureFileName.empty())
        ResolvedTextures[static_cast<int32>(EMaterialTextureSlot::Diffuse)] = RM.Load<UTexture>(MaterialInfo.DiffuseTextureFileName);
    else
        ResolvedTextures[static_cast<int32>(EMaterialTextureSlot::Diffuse)] = nullptr; // 또는 기본 텍스처

    if (!MaterialInfo.NormalTextureFileName.empty())
        ResolvedTextures[static_cast<int32>(EMaterialTextureSlot::Normal)] = RM.Load<UTexture>(MaterialInfo.NormalTextureFileName);
    else
        ResolvedTextures[static_cast<int32>(EMaterialTextureSlot::Normal)] = nullptr; // 또는 기본 노멀 텍스처
}
