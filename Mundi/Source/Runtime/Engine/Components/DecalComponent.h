#pragma once

#include "PrimitiveComponent.h"
#include "AABB.h"
#include "Vector.h"
#include "DecalFadeTypes.h"
#include "UDecalComponent.generated.h"

// Forward declarations
struct FOBB;
class UTexture;
struct FDecalProjectionData;

/**
 * UDecalComponent - Projection Decal implementation
 *
 * Decal volume은 USceneComponent의 Location, Rotation, Scale로 정의됩니다:
 * - Location: Decal의 중심 위치
 * - Rotation: Decal의 투영 방향 (Forward = -Z 방향으로 투영)
 * - Scale: Decal volume의 크기 (X=Width, Y=Height, Z=Depth)
 */
UCLASS(DisplayName = "데칼 컴포넌트", Description = "2D텍스쳐를 덧 씌워그리는 데칼 컴포넌트입니다.")
class UDecalComponent : public UPrimitiveComponent
{
public:
	GENERATED_REFLECTION_BODY()

	UDecalComponent();

protected:
	~UDecalComponent() override = default;

public:

    // ===== Lua-Bindable Properties (Auto-moved from protected/private) =====
	UPROPERTY(EditAnywhere, Category = "Decal", Tooltip = "데칼 텍스처입니다")
	UTexture* DecalTexture = nullptr;

	UPROPERTY(EditAnywhere, Category = "Decal", Tooltip = "데칼 페이드 설정")
	FDecalFadeProperty FadeProperty;

	virtual void RenderDebugVolume(URenderer* Renderer) const override;

	// Decal Resource API
	void SetDecalTexture(UTexture* InTexture);
	void SetDecalTexture(const FString& TexturePath);
	UTexture* GetDecalTexture() const { return DecalTexture; }

	// Decal Property API
	void SetVisibility(bool bVisible) { bIsVisible = bVisible; }
	bool IsVisible() const { return bIsVisible; }
	void SetOpacity(float Opacity) { FadeProperty.FadeAlpha = FMath::Clamp(Opacity, 0.0f, 1.0f); }
	float GetOpacity() const { return FadeProperty.FadeAlpha; }

	// Fade 제어 API 
	void StartFadeIn(float Duration, float Delay = 0.f, EDecalFadeStyle InFadeStyle = EDecalFadeStyle::Standard);
	void StartFadeOut(float Duration, float Delay = 0.f, bool bDestroyOwner = false, 
		EDecalFadeStyle InFadeStyle = EDecalFadeStyle::Standard);
	float GetFadeAlpha() const { return FadeProperty.FadeAlpha; }
	uint32_t GetFadeStyle() const { return static_cast<uint32_t>(FadeProperty.FadeStyle); }
	bool IsFadeCompleted() const { return FadeProperty.bFadeCompleted; }

	// Decal Volume & Bounds API
	FAABB GetWorldAABB() const override;
	FOBB GetWorldOBB() const;

	// Projection & UV Mapping API
	virtual FMatrix GetDecalProjectionMatrix() const;   // NOTE: FakeSpotLight 를 위해서 가상 함수로 선언

	// Serialization API
	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// ───── 복사 관련 ────────────────────────────
	void DuplicateSubObjects() override;

	// Tick
	virtual void TickComponent(float DeltaTime) override;

	void OnRegister(UWorld* InWorld) override;
	
private:
	UGizmoArrowComponent* DirectionGizmo = nullptr;
	class UBillboardComponent* SpriteComponent = nullptr;
};