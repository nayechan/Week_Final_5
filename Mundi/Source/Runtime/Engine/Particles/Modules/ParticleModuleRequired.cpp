#include "pch.h"
#include "ParticleModuleRequired.h"
#include "ParticleDefinitions.h"
#include "JsonSerializer.h"
#include "ResourceManager.h"
#include "Material.h"

void UParticleModuleRequired::DuplicateSubObjects()
{
	UParticleModule::DuplicateSubObjects();

	// bOwnsMaterial이 true이고 Material이 있으면 복제
	// (false면 공유 Asset이므로 그대로 둠)
	if (bOwnsMaterial && Material)
	{
		UMaterial* OldMat = Cast<UMaterial>(Material);
		if (OldMat)
		{
			UMaterial* NewMat = NewObject<UMaterial>();
			NewMat->SetShader(OldMat->GetShader());
			NewMat->SetMaterialInfo(OldMat->GetMaterialInfo());
			NewMat->ResolveTextures();
			Material = NewMat;
			// bOwnsMaterial은 이미 true로 복사되어 있음 - 새 Material도 소유
		}
	}
}

UParticleModuleRequired::~UParticleModuleRequired()
{
	// 로드 시 직접 생성한 Material만 정리
	if (bOwnsMaterial && Material)
	{
		DeleteObject(Material);
		Material = nullptr;
	}
}

// 언리얼 엔진 호환: 렌더 스레드용 데이터로 변환
FParticleRequiredModule UParticleModuleRequired::ToRenderThreadData() const
{
	FParticleRequiredModule RenderData;
	RenderData.Material = Material;
	RenderData.EmitterName = EmitterName;
	RenderData.ScreenAlignment = ScreenAlignment;
	RenderData.bOrientZAxisTowardCamera = bOrientZAxisTowardCamera;

	// Sub-UV 설정 복사
	RenderData.SubImages_Horizontal = SubImages_Horizontal;
	RenderData.SubImages_Vertical = SubImages_Vertical;
	RenderData.SubUV_MaxElements = SubUV_MaxElements;

	return RenderData;
}

void UParticleModuleRequired::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UParticleModule::Serialize(bInIsLoading, InOutHandle);
	// UPROPERTY 속성은 자동으로 직렬화됨 (Material 제외 - 수동 처리)

	if (bInIsLoading)
	{
		// === Material 로드 (Shader + Texture 경로 기반) ===
		FString ShaderPath;
		FJsonSerializer::ReadString(InOutHandle, "MaterialShader", ShaderPath);

		if (!ShaderPath.empty())
		{
			// 각 이미터별로 독립적인 Material 인스턴스 생성
			// (ResourceManager::Load는 캐싱된 동일 인스턴스를 반환하므로 사용하지 않음)
			UMaterial* NewMaterial = NewObject<UMaterial>();
			UShader* Shader = UResourceManager::GetInstance().Load<UShader>(ShaderPath);
			NewMaterial->SetShader(Shader);
			Material = NewMaterial;
			bOwnsMaterial = true;  // 직접 생성했으므로 소멸자에서 정리

			// 텍스처 경로 읽기 및 설정
			FString TexturePath;
			FJsonSerializer::ReadString(InOutHandle, "MaterialTexture", TexturePath);

			if (!TexturePath.empty())
			{
				FMaterialInfo MatInfo;
				MatInfo.DiffuseTextureFileName = TexturePath;
				NewMaterial->SetMaterialInfo(MatInfo);
				NewMaterial->ResolveTextures();
			}
		}
	}
	else
	{
		// === Material 저장 (Shader + Texture 경로 별도 저장) ===
		if (Material)
		{
			UMaterial* MatPtr = Cast<UMaterial>(Material);
			if (MatPtr)
			{
				// Shader 경로 저장
				UShader* Shader = MatPtr->GetShader();
				if (Shader)
				{
					InOutHandle["MaterialShader"] = Shader->GetFilePath().c_str();
				}
				else
				{
					InOutHandle["MaterialShader"] = "";
				}

				// Texture 경로 저장
				const FMaterialInfo& MatInfo = MatPtr->GetMaterialInfo();
				InOutHandle["MaterialTexture"] = MatInfo.DiffuseTextureFileName.c_str();
			}
			else
			{
				InOutHandle["MaterialShader"] = "";
				InOutHandle["MaterialTexture"] = "";
			}
		}
		else
		{
			InOutHandle["MaterialShader"] = "";
			InOutHandle["MaterialTexture"] = "";
		}
	}
}
