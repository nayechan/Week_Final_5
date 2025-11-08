#pragma once

#include "DecalComponent.h"
#include "AABB.h"
#include "Vector.h"
#include "UPerspectiveDecalComponent.generated.h"

// Forward declarations
struct FOBB;
class UTexture;
struct FDecalProjectionData;

class UPerspectiveDecalComponent : public UDecalComponent
{
public:

	GENERATED_REFLECTION_BODY()

	UPerspectiveDecalComponent();
	~UPerspectiveDecalComponent() override = default;

	// Render API
	void RenderDebugVolume(URenderer* Renderer) const override;

	// Projection & UV Mapping API
	FMatrix GetDecalProjectionMatrix() const override;

	float GetFovY() { return FovY; };
	void SetFovY(float InFov);

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;

	// Serialize
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

private:
	UPROPERTY(EditAnywhere, Category="Decal", Range="1.0, 179.0", Tooltip="수직 시야각 (FOV, Degrees)입니다.")
	float FovY = 60;
};