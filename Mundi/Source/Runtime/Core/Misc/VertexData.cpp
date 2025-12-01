#include "pch.h"
#include "VertexData.h"
#include "BodySetup.h"

void FBillboardVertex::FillFrom(const FMeshData& mesh, size_t i)
{
    WorldPosition = mesh.Vertices[i];
    UV = (i < mesh.UV.size()) ? mesh.UV[i] : FVector2D(0.0f, 0.0f);
}

void FBillboardVertex::FillFrom(const FNormalVertex& src)
{
    WorldPosition = src.pos;
    UV = src.tex;
}

void FVertexSimple::FillFrom(const FMeshData& mesh, size_t i)
{
    Position = mesh.Vertices[i];
    Color = (i < mesh.Color.size()) ? mesh.Color[i] : FVector4(1, 1, 1, 1);
}

void FVertexSimple::FillFrom(const FNormalVertex& src)
{
    Position = src.pos;
    Color = src.color;
}

void FVertexDynamic::FillFrom(const FMeshData& mesh, size_t i)
{
    Position = mesh.Vertices[i];
    Color = (i < mesh.Color.size()) ? mesh.Color[i] : FVector4(1, 1, 1, 1);
    UV = (i < mesh.UV.size()) ? mesh.UV[i] : FVector2D(0, 0);
    Normal = (i < mesh.Normal.size()) ? mesh.Normal[i] : FVector(0, 0, 1);
}

void FVertexDynamic::FillFrom(const FNormalVertex& src)
{
    Position = src.pos;
    Color = src.color;
    UV = src.tex;
    Tangent = src.Tangent;
    Normal = FVector{ src.normal.X, src.normal.Y, src.normal.Z };
}

void FBillboardVertexInfo_GPU::FillFrom(const FMeshData& mesh, size_t i)
{
    Position[0] = mesh.Vertices[i].X;
    Position[1] = mesh.Vertices[i].Y;
    Position[2] = mesh.Vertices[i].Z;
    CharSize[0] = (i < mesh.UV.size()) ? mesh.UV[i].X : 1.0f;
    CharSize[1] = (i < mesh.UV.size()) ? mesh.UV[i].Y : 1.0f;
    if (i < mesh.Color.size()) {
        UVRect[0] = mesh.Color[i].X;
        UVRect[1] = mesh.Color[i].Y;
        UVRect[2] = mesh.Color[i].Z;
        UVRect[3] = mesh.Color[i].W;
    }
    else {
        UVRect[0] = 0; UVRect[1] = 0; UVRect[2] = 1; UVRect[3] = 1;
    }
}

void FBillboardVertexInfo_GPU::FillFrom(const FNormalVertex& src)
{
    Position[0] = src.pos.X;
    Position[1] = src.pos.Y;
    Position[2] = src.pos.Z;

    CharSize[0] = src.tex.X;
    CharSize[1] = src.tex.Y;

    UVRect[0] = src.color.X;
    UVRect[1] = src.color.Y;
    UVRect[2] = src.color.Z;
    UVRect[3] = src.color.W;
}

FArchive& operator<<(FArchive& Ar, FStaticMesh& Mesh)
{
    if (Ar.IsSaving())
    {
        Serialization::WriteString(Ar, Mesh.PathFileName);
        Serialization::WriteArray(Ar, Mesh.Vertices);
        Serialization::WriteArray(Ar, Mesh.Indices);

        uint32_t gCount = (uint32_t)Mesh.GroupInfos.size();
        Ar << gCount;
        for (auto& g : Mesh.GroupInfos) Ar << g;

        Ar << Mesh.bHasMaterial;

        // *** BodySetup 저장 ***
        uint8 bHasBodySetup = (Mesh.BodySetup != nullptr) ? 1 : 0;
        Ar << bHasBodySetup;
        if (bHasBodySetup && Mesh.BodySetup)
        {
            Ar << *Mesh.BodySetup;
        }
    }
    else if (Ar.IsLoading())
    {
        Serialization::ReadString(Ar, Mesh.PathFileName);
        Serialization::ReadArray(Ar, Mesh.Vertices);
        Serialization::ReadArray(Ar, Mesh.Indices);

        uint32_t gCount;
        Ar << gCount;
        Mesh.GroupInfos.resize(gCount);
        for (auto& g : Mesh.GroupInfos) Ar << g;

        Ar << Mesh.bHasMaterial;

        // *** BodySetup 로드 ***
        uint8 bHasBodySetup = 0;
        Ar << bHasBodySetup;
        if (bHasBodySetup)
        {
            Mesh.BodySetup = ObjectFactory::NewObject<UBodySetup>();
            Ar << *Mesh.BodySetup;
        }
    }
    return Ar;
}

FArchive& operator<<(FArchive& Ar, FSkeletalMeshData& Data)
{
    if (Ar.IsSaving())
    {
        // 1. Vertices 저장
        Serialization::WriteArray(Ar, Data.Vertices);

        // 2. Indices 저장
        Serialization::WriteArray(Ar, Data.Indices);

        // 3. Skeleton 저장
        Ar << Data.Skeleton;

        // 4. GroupInfos 저장
        uint32 gCount = static_cast<uint32>(Data.GroupInfos.size());
        Ar << gCount;
        for (auto& g : Data.GroupInfos)
        {
            Ar << g;
        }

        // 5. Material 플래그 저장
        Ar << Data.bHasMaterial;

        // 6. CacheFilePath 저장
        Serialization::WriteString(Ar, Data.CacheFilePath);

        // *** BodySetup 저장 ***
        uint8 bHasBodySetup = (Data.BodySetup != nullptr) ? 1 : 0;
        Ar << bHasBodySetup;
        if (bHasBodySetup && Data.BodySetup)
        {
            Ar << *Data.BodySetup;
        }
    }
    else if (Ar.IsLoading())
    {
        // 1. Vertices 로드
        Serialization::ReadArray(Ar, Data.Vertices);

        // 2. Indices 로드
        Serialization::ReadArray(Ar, Data.Indices);

        // 3. Skeleton 로드
        Ar << Data.Skeleton;

        // 4. GroupInfos 로드
        uint32 gCount;
        Ar << gCount;
        Data.GroupInfos.resize(gCount);
        for (auto& g : Data.GroupInfos)
        {
            Ar << g;
        }

        // 5. Material 플래그 로드
        Ar << Data.bHasMaterial;

        // 6. CacheFilePath 로드
        Serialization::ReadString(Ar, Data.CacheFilePath);

        // *** BodySetup 로드 ***
        uint8 bHasBodySetup = 0;
        Ar << bHasBodySetup;
        if (bHasBodySetup)
        {
            Data.BodySetup = ObjectFactory::NewObject<UBodySetup>();
            Ar << *Data.BodySetup;
        }
    }
    return Ar;
}