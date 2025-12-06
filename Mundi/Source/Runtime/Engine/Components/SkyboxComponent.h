#pragma once

#include "SceneComponent.h"
#include "USkyboxComponent.generated.h"

class UTexture;
class UShader;
class UStaticMesh;

// 6면 스카이박스 면 인덱스 (LH Z-up: +X=Forward, +Y=Right, +Z=Up)
enum class ESkyboxFace : uint8
{
	Front = 0,   // +X
	Back = 1,    // -X
	Top = 2,     // +Z
	Bottom = 3,  // -Z
	Right = 4,   // +Y
	Left = 5     // -Y
};

UCLASS(DisplayName="스카이박스 컴포넌트", Description="6면 스카이박스를 렌더링합니다")
class USkyboxComponent : public USceneComponent
{
public:
	GENERATED_REFLECTION_BODY()

public:
	USkyboxComponent();
	virtual ~USkyboxComponent();

	// 텍스처 설정
	void SetTexture(ESkyboxFace Face, const FString& TexturePath);
	void SetTexture(ESkyboxFace Face, UTexture* InTexture);
	UTexture* GetTexture(ESkyboxFace Face) const;

	// 전체 텍스처 설정 (편의 함수)
	void SetAllTextures(const FString& BasePath, const FString& Extension = ".png");

	// 셰이더/메시 접근
	UShader* GetShader() const { return SkyboxShader; }
	UStaticMesh* GetMesh() const { return SkyboxMesh; }

	// 활성화 여부
	UPROPERTY(EditAnywhere, Category="Skybox")
	bool bEnabled = true;

	// 밝기 조절
	UPROPERTY(EditAnywhere, Category="Skybox", Range="0.0, 10.0")
	float Intensity = 1.0f;

	// 컴포넌트 라이프사이클
	void OnRegister(UWorld* InWorld) override;

	// 직렬화
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// 복사
	void DuplicateSubObjects() override;

	// 6면 텍스처 (Front, Back, Top, Bottom, Right, Left)
	UPROPERTY(EditAnywhere, Category="Skybox|Textures")
	UTexture* TextureFront = nullptr;

	UPROPERTY(EditAnywhere, Category="Skybox|Textures")
	UTexture* TextureBack = nullptr;

	UPROPERTY(EditAnywhere, Category="Skybox|Textures")
	UTexture* TextureTop = nullptr;

	UPROPERTY(EditAnywhere, Category="Skybox|Textures")
	UTexture* TextureBottom = nullptr;

	UPROPERTY(EditAnywhere, Category="Skybox|Textures")
	UTexture* TextureRight = nullptr;

	UPROPERTY(EditAnywhere, Category="Skybox|Textures")
	UTexture* TextureLeft = nullptr;

private:
	// 렌더링 리소스
	UShader* SkyboxShader = nullptr;
	UStaticMesh* SkyboxMesh = nullptr;

	void InitializeRenderingResources();
};
