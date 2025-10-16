#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>

#include "UEContainer.h"
#include "Vector.h"
#include "Enums.h"
#include "StaticMesh.h" // FStaticMesh, FObjMaterialInfo 등이 정의된 헤더로 가정

// Raw Data
struct FObjInfo
{
	TArray<FVector> Positions;
	TArray<FVector2D> TexCoords;
	TArray<FVector> Normals;

	TArray<uint32> PositionIndices;
	TArray<uint32> TexCoordIndices;
	TArray<uint32> NormalIndices;

	TArray<FString> MaterialNames; // == 강의 글의 meshMaterials
	TArray<uint32> GroupIndexStartArray; // i번째 Group이 사용하는 indices 범위: GroupIndexStartArray[i] ~ GroupIndexStartArray[i+1]
	TArray<uint32> GroupMaterialArray; // i번쩨 Group이 사용하는 MaterialInfos 인덱스 넘버

	FString ObjFileName;

	bool bHasMtl = true;
};

struct FObjImporter
{
public:
	/**
	 * .obj 파일을 파싱하여 FObjInfo 구조체에 원시 데이터를 저장합니다.
	 * @param InFileName      - .obj 파일의 전체 경로
	 * @param OutObjInfo      - 파싱된 데이터가 저장될 구조체
	 * @param OutMaterialInfos- 파싱된 머티리얼 데이터가 저장될 배열
	 * @param bIsRightHanded - obj 파일이 오른손 좌표계인지 (오른손 좌표계면 강제로 왼손 좌표계로 변환), 블렌더 등 대부분 3D 파일은 오른손 좌표계를 사용하기에 기본값이 true
	 * @return 로드 성공 시 true, 실패 시 false
	 */
	static bool LoadObjModel(const FString& InFileName, FObjInfo* const OutObjInfo, TArray<FObjMaterialInfo>& OutMaterialInfos, bool bIsRightHanded = true)
	{
		uint32 subsetCount = 0;
		FString MtlFileName;

		bool bHasTexcoord = false;
		bool bHasNormal = false;
		FString MaterialNameTemp;

		FString Face;
		uint32 VIndex = 0;
		uint32 MeshTriangles = 0;

		size_t pos = InFileName.find_last_of("/\\");
		FString objDir = (pos == FString::npos) ? "" : InFileName.substr(0, pos + 1);

		// [안정성] .obj 파일이 존재하지 않으면 로드 실패를 반환합니다.
		// 이는 필수 데이터이므로 더 이상 진행할 수 없습니다.
		std::ifstream FileIn(InFileName.c_str());
		if (!FileIn)
		{
			UE_LOG("Error: The file '%s' does not exist!", InFileName.c_str());
			return false;
		}

		OutObjInfo->ObjFileName = FString(InFileName.begin(), InFileName.end());

		FString line;
		while (std::getline(FileIn, line))
		{
			if (line.empty()) continue;

			line.erase(0, line.find_first_not_of(" \t\n\r"));

			if (line[0] == '#')
				continue;

			if (line.rfind("v ", 0) == 0) // 정점 좌표 (v x y z)
			{
				std::stringstream wss(line.substr(2));
				float vx, vy, vz;
				wss >> vx >> vy >> vz;
				if (bIsRightHanded)
				{
					OutObjInfo->Positions.push_back(FVector(vx, -vy, vz));
				}
				else
				{
					OutObjInfo->Positions.push_back(FVector(vx, vy, vz));
				}
			}
			else if (line.rfind("vt ", 0) == 0) // 텍스처 좌표 (vt u v)
			{
				std::stringstream wss(line.substr(3));
				float u, v;
				wss >> u >> v;
				// obj의 vt는 좌하단이 (0,0) -> DirectX UV는 좌상단이 (0,0) (상하 반전으로 컨버팅)
				v = 1.0f - v;
				OutObjInfo->TexCoords.push_back(FVector2D(u, v));
				bHasTexcoord = true;
			}
			else if (line.rfind("vn ", 0) == 0) // 법선 (vn x y z)
			{
				std::stringstream wss(line.substr(3));
				float nx, ny, nz;
				wss >> nx >> ny >> nz;
				if (bIsRightHanded)
				{
					OutObjInfo->Normals.push_back(FVector(nx, -ny, nz));
				}
				else
				{
					OutObjInfo->Normals.push_back(FVector(nx, ny, nz));
				}
				bHasNormal = true;
			}
			else if (line.rfind("g ", 0) == 0) // 그룹 (g groupName)
			{
				// 현재 'usemtl'을 기준으로 그룹을 나누므로 'g' 태그는 무시합니다.
			}
			else if (line.rfind("f ", 0) == 0) // 면 (f v1/vt1/vn1 v2/vt2/vn2 ...)
			{
				Face = line.substr(2);
				if (Face.length() <= 0)
				{
					continue;
				}

				// Parse face line and trim at '#' or newline
				std::stringstream wss(Face);
				FString VertexDef;

				TArray<FFaceVertex> LineFaceVertices;
				while (wss >> VertexDef)
				{
					// '#'을 만나면 주석 처리 (이후 데이터 무시)
					if (VertexDef[0] == '#')
					{
						break;
					}
					
					FFaceVertex FaceVertex = ParseVertexDef(VertexDef);
					LineFaceVertices.push_back(FaceVertex);
				}

				// 4각형 이상의 폴리곤도 처리하기 위해서 for문으로 처리
				for (uint32 i = 1; i < LineFaceVertices.size() - 1; ++i)
				{
					if (bIsRightHanded)
					{
						OutObjInfo->PositionIndices.push_back(LineFaceVertices[0].PositionIndex);
						OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[0].TexCoordIndex);
						OutObjInfo->NormalIndices.push_back(LineFaceVertices[0].NormalIndex);

						OutObjInfo->PositionIndices.push_back(LineFaceVertices[i + 1].PositionIndex);
						OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[i + 1].TexCoordIndex);
						OutObjInfo->NormalIndices.push_back(LineFaceVertices[i + 1].NormalIndex);

						OutObjInfo->PositionIndices.push_back(LineFaceVertices[i].PositionIndex);
						OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[i].TexCoordIndex);
						OutObjInfo->NormalIndices.push_back(LineFaceVertices[i].NormalIndex);
					}
					else
					{
						OutObjInfo->PositionIndices.push_back(LineFaceVertices[0].PositionIndex);
						OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[0].TexCoordIndex);
						OutObjInfo->NormalIndices.push_back(LineFaceVertices[0].NormalIndex);

						OutObjInfo->PositionIndices.push_back(LineFaceVertices[i].PositionIndex);
						OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[i].TexCoordIndex);
						OutObjInfo->NormalIndices.push_back(LineFaceVertices[i].NormalIndex);

						OutObjInfo->PositionIndices.push_back(LineFaceVertices[i + 1].PositionIndex);
						OutObjInfo->TexCoordIndices.push_back(LineFaceVertices[i + 1].TexCoordIndex);
						OutObjInfo->NormalIndices.push_back(LineFaceVertices[i + 1].NormalIndex);
					}
					VIndex += 3;
					++MeshTriangles;
				}
			}
			else if (line.rfind("mtllib ", 0) == 0)
			{
				MtlFileName = objDir + line.substr(7);
			}
			else if (line.rfind("usemtl ", 0) == 0)
			{
				MaterialNameTemp = line.substr(7);
				OutObjInfo->MaterialNames.push_back(MaterialNameTemp);
				OutObjInfo->GroupIndexStartArray.push_back(VIndex);
				subsetCount++;
			}
			else
			{
				UE_LOG("While parsing the filename %s, the following unknown symbol was encountered: \'%s\'", InFileName.c_str(), line.c_str());
			}
		}

		if (subsetCount == 0)
		{
			OutObjInfo->GroupIndexStartArray.push_back(0);
			subsetCount++;
		}
		OutObjInfo->GroupIndexStartArray.push_back(VIndex);

		if (OutObjInfo->GroupIndexStartArray.size() > 1 && OutObjInfo->GroupIndexStartArray[1] == 0)
		{
			OutObjInfo->GroupIndexStartArray.erase(OutObjInfo->GroupIndexStartArray.begin() + 1);
			subsetCount--;
		}

		if (!bHasNormal)
		{
			OutObjInfo->Normals.push_back(FVector(0.0f, 0.0f, 0.0f));
		}
		if (!bHasTexcoord)
		{
			OutObjInfo->TexCoords.push_back(FVector2D(0.0f, 0.0f));
		}

		FileIn.close();

		// Material 파싱 시작
		FileIn.open(MtlFileName.c_str());

		if (MtlFileName.empty())
		{
			OutObjInfo->bHasMtl = false;
			return true;
		}

		// .mtl 파일이 존재하지 않더라도 로딩을 중단하지 않습니다.
		// 경고를 로깅하고, 머티리얼이 없는 모델로 처리를 계속합니다.
		if (!FileIn)
		{
			UE_LOG("Warning: Material file '%s' not found for obj '%s'. Loading model without materials.", MtlFileName.c_str(), InFileName.c_str());
			OutObjInfo->bHasMtl = false;
			return true;
		}

		uint32 MatCount = 0;
		while (std::getline(FileIn, line))
		{
			if (line.empty()) continue;

			line.erase(0, line.find_first_not_of(" \t\n\r"));
			if (line[0] == '#')
				continue;

			if (line.rfind("newmtl ", 0) == 0)
			{
				FObjMaterialInfo TempMatInfo;
				TempMatInfo.MaterialName = line.substr(7);
				OutMaterialInfos.push_back(TempMatInfo);
				++MatCount;
			}
			else if (MatCount > 0)
			{
				// (머티리얼 속성 파싱 로직은 변경 없음)
				if (line.rfind("Kd ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].DiffuseColor = FVector(vx, vy, vz); }
				else if (line.rfind("Ka ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].AmbientColor = FVector(vx, vy, vz); }
				else if (line.rfind("Ke ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].EmissiveColor = FVector(vx, vy, vz); }
				else if (line.rfind("Ks ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].SpecularColor = FVector(vx, vy, vz); }
				else if (line.rfind("Tf ", 0) == 0) { std::stringstream wss(line.substr(3)); float vx, vy, vz; wss >> vx >> vy >> vz; OutMaterialInfos[MatCount - 1].TransmissionFilter = FVector(vx, vy, vz); }
				else if (line.rfind("Tr ", 0) == 0) { std::stringstream wss(line.substr(3)); float value; wss >> value; OutMaterialInfos[MatCount - 1].Transparency = value; }
				else if (line.rfind("d ", 0) == 0) { std::stringstream wss(line.substr(3)); float value; wss >> value; OutMaterialInfos[MatCount - 1].Transparency = 1.0f - value; }
				else if (line.rfind("Ni ", 0) == 0) { std::stringstream wss(line.substr(3)); float value; wss >> value; OutMaterialInfos[MatCount - 1].OpticalDensity = value; }
				else if (line.rfind("Ns ", 0) == 0) { std::stringstream wss(line.substr(3)); float value; wss >> value; OutMaterialInfos[MatCount - 1].SpecularExponent = value; }
				else if (line.rfind("illum ", 0) == 0) { std::stringstream wss(line.substr(6)); float value; wss >> value; OutMaterialInfos[MatCount - 1].IlluminationModel = static_cast<int32>(value); }
				else if (line.rfind("map_Kd ", 0) == 0) { FString TextureRel = line.substr(7); std::replace(TextureRel.begin(), TextureRel.end(), '\\', '/'); OutMaterialInfos[MatCount - 1].DiffuseTextureFileName = TextureRel; }
				else if (line.rfind("map_d ", 0) == 0) { FString TextureRel = line.substr(7); std::replace(TextureRel.begin(), TextureRel.end(), '\\', '/'); OutMaterialInfos[MatCount - 1].TransparencyTextureFileName = TextureRel; }
				else if (line.rfind("map_Ka ", 0) == 0) { FString TextureRel = line.substr(7); std::replace(TextureRel.begin(), TextureRel.end(), '\\', '/'); OutMaterialInfos[MatCount - 1].AmbientTextureFileName = TextureRel; }
				else if (line.rfind("map_Ks ", 0) == 0) { FString TextureRel = line.substr(7); std::replace(TextureRel.begin(), TextureRel.end(), '\\', '/'); OutMaterialInfos[MatCount - 1].SpecularTextureFileName = TextureRel; }
				else if (line.rfind("map_Ns ", 0) == 0) { FString TextureRel = line.substr(7); std::replace(TextureRel.begin(), TextureRel.end(), '\\', '/'); OutMaterialInfos[MatCount - 1].SpecularExponentTextureFileName = TextureRel; }
				else if (line.rfind("map_Ke ", 0) == 0) { FString TextureRel = line.substr(7); std::replace(TextureRel.begin(), TextureRel.end(), '\\', '/'); OutMaterialInfos[MatCount - 1].EmissiveTextureFileName = TextureRel; }
			}
		}
		FileIn.close();

		for (uint32 i = 0; i < OutObjInfo->MaterialNames.size(); ++i)
		{
			bool bHasMat = false;
			for (uint32 j = 0; j < OutMaterialInfos.size(); ++j)
			{
				if (OutObjInfo->MaterialNames[i] == OutMaterialInfos[j].MaterialName)
				{
					OutObjInfo->GroupMaterialArray.push_back(j);
					bHasMat = true;
					break;
				}
			}

			if (!bHasMat && !OutMaterialInfos.empty())
			{
				OutObjInfo->GroupMaterialArray.push_back(0);
			}
		}

		return true;
	}

	struct VertexKey
	{
		uint32 PosIndex, TexIndex, NormalIndex;
		bool operator==(const VertexKey& Other) const { return PosIndex == Other.PosIndex && TexIndex == Other.TexIndex && NormalIndex == Other.NormalIndex; }
	};

	struct VertexKeyHash
	{
		size_t operator()(const VertexKey& Key) const { return std::hash<uint32>()(Key.PosIndex) ^ (std::hash<uint32>()(Key.TexIndex) << 1) ^ (std::hash<uint32>()(Key.NormalIndex) << 2); }
	};

	static void ConvertToStaticMesh(const FObjInfo& InObjInfo, const TArray<FObjMaterialInfo>& InMaterialInfos, FStaticMesh* const OutStaticMesh)
	{
		OutStaticMesh->PathFileName = InObjInfo.ObjFileName;
		uint32 NumDuplicatedVertex = static_cast<uint32>(InObjInfo.PositionIndices.size());

		std::unordered_map<VertexKey, uint32, VertexKeyHash> VertexMap;
		for (uint32 CurIndex = 0; CurIndex < NumDuplicatedVertex; ++CurIndex)
		{
			VertexKey Key{ InObjInfo.PositionIndices[CurIndex], InObjInfo.TexCoordIndices[CurIndex], InObjInfo.NormalIndices[CurIndex] };
			auto It = VertexMap.find(Key);
			if (It != VertexMap.end())
			{
				OutStaticMesh->Indices.push_back(It->second);
			}
			else
			{
				FNormalVertex NormalVertex(
					InObjInfo.Positions[Key.PosIndex],
					InObjInfo.Normals[Key.NormalIndex],
					FVector4(1, 1, 1, 1),
					InObjInfo.TexCoords[Key.TexIndex]
				);
				OutStaticMesh->Vertices.push_back(NormalVertex);
				uint32 NewIndex = static_cast<uint32>(OutStaticMesh->Vertices.size() - 1);
				OutStaticMesh->Indices.push_back(NewIndex);
				VertexMap[Key] = NewIndex;
			}
		}

		if (!InObjInfo.bHasMtl)
		{
			OutStaticMesh->bHasMaterial = false;
			return;
		}

		OutStaticMesh->bHasMaterial = true;
		uint32 NumGroup = static_cast<uint32>(InObjInfo.MaterialNames.size());
		if (NumGroup > 0)
		{
			OutStaticMesh->GroupInfos.resize(NumGroup);
		}

		if (InMaterialInfos.size() == 0 && NumGroup > 0)
		{
			return;
		}

		for (uint32 i = 0; i < NumGroup; ++i)
		{
			OutStaticMesh->GroupInfos[i].StartIndex = InObjInfo.GroupIndexStartArray[i];
			OutStaticMesh->GroupInfos[i].IndexCount = InObjInfo.GroupIndexStartArray[i + 1] - InObjInfo.GroupIndexStartArray[i];

			// [안정성] GroupMaterialArray가 비어있거나 인덱스가 범위를 벗어나는 경우를 대비합니다.
			if (i < InObjInfo.GroupMaterialArray.size())
			{
				uint32 matIndex = InObjInfo.GroupMaterialArray[i];
				if (matIndex < InMaterialInfos.size())
				{
					OutStaticMesh->GroupInfos[i].InitialMaterialName = InMaterialInfos[matIndex].MaterialName;
				}
			}
		}
	}
private:
	struct FFaceVertex { uint32 PositionIndex, TexCoordIndex, NormalIndex; };

	static FFaceVertex ParseVertexDef(const FString& InVertexDef)
	{
		FFaceVertex Result{ 0, 0, 0 };
		std::stringstream ss(InVertexDef);
		FString part;
		uint32 temp_val;

		if (std::getline(ss, part, '/')) { if (!part.empty()) { std::stringstream conv(part); if (conv >> temp_val) Result.PositionIndex = temp_val - 1; } }
		if (std::getline(ss, part, '/')) { if (!part.empty()) { std::stringstream conv(part); if (conv >> temp_val) Result.TexCoordIndex = temp_val - 1; } }
		if (std::getline(ss, part, '/')) { if (!part.empty()) { std::stringstream conv(part); if (conv >> temp_val) Result.NormalIndex = temp_val - 1; } }

		return Result;
	}
};

class UStaticMesh;

class FObjManager
{
private:
	static TMap<FString, FStaticMesh*> ObjStaticMeshMap;
public:
	static void Preload();
	static void Clear();
	static FStaticMesh* LoadObjStaticMeshAsset(const FString& PathFileName);
	static UStaticMesh* LoadObjStaticMesh(const FString& PathFileName);
};
