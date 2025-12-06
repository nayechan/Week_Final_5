#pragma once
#include "Archive.h"
#include <d3d11.h>

struct FMaterialInfo
{
    int32 IlluminationModel = 2;  // illum. Default illumination model to Phong for non-Pbr materials

    FVector DiffuseColor = FVector(0.8f, 0.8f, 0.8f);   // Kd - 표준: 0.8
    FVector AmbientColor = FVector(0.2f, 0.2f, 0.2f);   // Ka - 표준: 0.2
    FVector SpecularColor = FVector::One();             // Ks - 표준: 1.0 (유지)
    FVector EmissiveColor = FVector::Zero();            // Ke - 표준: 0.0 (중요!)

    FString DiffuseTextureFileName;
    FString NormalTextureFileName;
    FString AmbientTextureFileName;
    FString SpecularTextureFileName;
    FString EmissiveTextureFileName;
    FString TransparencyTextureFileName;
    FString SpecularExponentTextureFileName;

    FVector TransmissionFilter = FVector::One(); // Tf

    float OpticalDensity = 1.0f; // Ni
    float Transparency = 0.0f; // Tr Or d
    float SpecularExponent = 32.0f; // Ns
    float BumpMultiplier = 1.0f; // map_Bump -bm

    // UV Tiling
    float TileU = 1.0f;
    float TileV = 1.0f;

    FString MaterialName;

    friend FArchive& operator<<(FArchive& Ar, FMaterialInfo& Info)
    {
        Ar << Info.IlluminationModel;
        Ar << Info.DiffuseColor;
        Ar << Info.AmbientColor;
        Ar << Info.SpecularColor;
        Ar << Info.EmissiveColor;

        if (Ar.IsSaving())
        {
            Serialization::WriteString(Ar, Info.DiffuseTextureFileName);
            Serialization::WriteString(Ar, Info.NormalTextureFileName);
            Serialization::WriteString(Ar, Info.AmbientTextureFileName);
            Serialization::WriteString(Ar, Info.SpecularTextureFileName);
            Serialization::WriteString(Ar, Info.EmissiveTextureFileName);
            Serialization::WriteString(Ar, Info.TransparencyTextureFileName);
            Serialization::WriteString(Ar, Info.SpecularExponentTextureFileName);
        }
        else if (Ar.IsLoading())
        {
            Serialization::ReadString(Ar, Info.DiffuseTextureFileName);
            Serialization::ReadString(Ar, Info.NormalTextureFileName);
            Serialization::ReadString(Ar, Info.AmbientTextureFileName);
            Serialization::ReadString(Ar, Info.SpecularTextureFileName);
            Serialization::ReadString(Ar, Info.EmissiveTextureFileName);
            Serialization::ReadString(Ar, Info.TransparencyTextureFileName);
            Serialization::ReadString(Ar, Info.SpecularExponentTextureFileName);
        }

        Ar << Info.TransmissionFilter;
        Ar << Info.OpticalDensity;
        Ar << Info.Transparency;
        Ar << Info.SpecularExponent;

        // UV Tiling
        Ar << Info.TileU;
        Ar << Info.TileV;

        if (Ar.IsSaving())
            Serialization::WriteString(Ar, Info.MaterialName);
        else if (Ar.IsLoading())
            Serialization::ReadString(Ar, Info.MaterialName);

        return Ar;
    }
};

namespace Serialization {
    template<>
    inline void ReadAsset<FMaterialInfo>(FArchive& Ar, FMaterialInfo* Arr)
    { 
        Ar << *Arr;
    }
    template<>
    inline void WriteAsset<FMaterialInfo>(FArchive& Ar, FMaterialInfo* Arr)
    {
        Ar << *Arr;
    }
    template<>
    inline void WriteArray<FMaterialInfo>(FArchive& Ar, const TArray<FMaterialInfo>& Arr) {
        uint32 Count = (uint32)Arr.size();
        Ar << Count;
        for (auto& Mat : Arr) Ar << const_cast<FMaterialInfo&>(Mat);
    }

    template<>
    inline void ReadArray<FMaterialInfo>(FArchive& Ar, TArray<FMaterialInfo>& Arr) {
        uint32 Count;
        Ar << Count;
        Arr.resize(Count);
        for (auto& Mat : Arr) Ar << Mat;
    }
}

struct FResourceData
{
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 VertexCount = 0;     // 정점 개수
    uint32 IndexCount = 0;     // 버텍스 점의 개수 
    uint32 ByteWidth = 0;       // 전체 버텍스 데이터 크기 (sizeof(FVertexSimple) * VertexCount)
    uint32 Stride = 0;
    uint32 Offset = 0;
    D3D11_PRIMITIVE_TOPOLOGY Topol = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
};

struct FTextureData
{
    ID3D11Resource* Texture = nullptr;
    ID3D11ShaderResourceView* TextureSRV = nullptr;
    ID3D11BlendState* BlendState = nullptr;
};
