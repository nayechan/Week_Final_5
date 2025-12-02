#pragma once

#include "PrimitiveComponent.h"
#include "AABB.h"
#include "Vector.h"
#include "UDecalComponent.generated.h"

// Forward declarations
struct FOBB;
class UTexture;
struct FDecalProjectionData;

// Fade style 상수 
// 0:Standard, 1:WipeLeftToRight, 2:Dissolve, 3:Iris
namespace EDecalFadeStyle
{
	constexpr int Standard = 0;           // 기본 알파 페이드
	constexpr int WipeLeftToRight = 1;    // 왼쪽에서 오른쪽으로 와이프
	constexpr int Dissolve = 2;           // 랜덤 디졸브
	constexpr int Iris = 3;               // 중앙에서 확장/축소
}

// Decal Fade Property 구조체
struct FDecalFadeProperty
{
public:
	float FadeSpeed = 0.5f;				// 페이드 속도 (초당 변화량)
	float FadeInDuration = 0.f;			// 페이드 인 완료까지 걸리는 시간
	float FadeInStartDelay = 0.f;		// 페이드 인 시작 전 대기 시간
	float FadeOutDuration = 0.f;		// 페이드 아웃 완료까지 걸리는 시간
	float FadeStartDelay = 0.f;			// 페이드 아웃 시작 전 대기 시간
	float FadeAlpha = 1.f;				// 현재 알파값 (0 ~ 1)
	float ElapsedFadeTime = 0.f;		// 현재 페이드 구간의 경과 시간
	bool bIsFadingIn = false;			// 현재 페이드 인 중인가?
	bool bIsFadingOut = false;			// 현재 페이드 아웃 중인가?
	bool bFadeCompleted = false;		// 전체 페이드 사이클 완료 여부
	bool bDestroyedAfterFade = false;	// 페이드 아웃 완료 후 제거 여부
	int FadeStyle = EDecalFadeStyle::Standard; // 페이드 비주얼 스타일 (0~3)

	void StartFadeIn(int InFadeStyle = EDecalFadeStyle::Standard)
	{
		bIsFadingIn = true;
		bIsFadingOut = false;
		bFadeCompleted = false;
		FadeAlpha = 0.f;
		ElapsedFadeTime = -FadeInStartDelay;
		FadeStyle = InFadeStyle;
	}

	void StartFadeOut(int InFadeStyle = EDecalFadeStyle::Standard)
	{
		bIsFadingOut = true;
		bIsFadingIn = false;
		FadeAlpha = 1.f;
		ElapsedFadeTime = -FadeStartDelay;
		FadeStyle = InFadeStyle;
	}

	// 매 프레임 갱신 (DeltaTime 단위로)
	bool Update(float DeltaTime)
	{
		// 자동으로 FadeIn/Out 반복
		if (!bIsFadingIn && !bIsFadingOut)
		{
			// 페이드 완료 후 반대 방향으로 다시 시작
			if (bFadeCompleted)
			{
				if (FadeAlpha >= 1.f)
				{
					StartFadeOut(FadeStyle);
				}
				else if (FadeAlpha <= 0.f)
				{
					StartFadeIn(FadeStyle);
				}
			}
			else
			{
				// 최초 시작: FadeOut부터 시작
				StartFadeOut(FadeStyle);
			}
		}

		// FadeIn/Out 업데이트
		if (!bIsFadingIn && !bIsFadingOut) { return false; }

		ElapsedFadeTime += DeltaTime;

		// 아직 시작 딜레이 중이면 무시
		if (ElapsedFadeTime < 0.f) { return false; }

		if (bIsFadingIn)
		{
			FadeAlpha = FMath::Clamp(ElapsedFadeTime / FadeInDuration, 0.f, 1.f);
			if (FadeAlpha >= 1.f)
			{
				bIsFadingIn = false;
				bFadeCompleted = true;
			}
		}
		else if (bIsFadingOut)
		{
			FadeAlpha = 1.f - FMath::Clamp(ElapsedFadeTime / FadeOutDuration, 0.f, 1.f);
			if (FadeAlpha <= 0.f)
			{
				bIsFadingOut = false;
				bFadeCompleted = true;
			}
		}

		return true;
	}
};

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

	UPROPERTY(EditAnywhere, Category = "Decal", Tooltip = "데칼 가시성")
	bool bIsVisible = true;

	UPROPERTY(EditAnywhere, Category = "Decal", Range = "0.0, 10.0", Tooltip = "페이드 속도 (초당 변화량)")
	float FadeSpeed = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Decal", Tooltip = "페이드 인 완료까지 걸리는 시간")
	float FadeInDuration = 0.f;

	UPROPERTY(EditAnywhere, Category = "Decal", Tooltip = "페이드 인 시작 전 대기 시간")
	float FadeInStartDelay = 0.f;

	UPROPERTY(EditAnywhere, Category = "Decal", Tooltip = "페이드 아웃 완료까지 걸리는 시간")
	float FadeOutDuration = 0.f;

	UPROPERTY(EditAnywhere, Category = "Decal", Tooltip = "페이드 아웃 시작 전 대기 시간")
	float FadeStartDelay = 0.f;

	UPROPERTY(EditAnywhere, Category = "Decal", Tooltip = "페이드 아웃 완료 후 액터 제거 여부")
	bool bDestroyedAfterFade = false;

	UPROPERTY(EditAnywhere, Category = "Decal", Range = "0, 3", 
		Tooltip = "페이드 스타일 (0:Standard, 1:Wipe, 2:Dissolve, 3:Iris)")
	int FadeStyle = EDecalFadeStyle::Standard;

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
	void StartFadeIn(float Duration, float Delay = 0.f, int InFadeStyle = EDecalFadeStyle::Standard);
	void StartFadeOut(float Duration, float Delay = 0.f, bool bDestroyOwner = false, 
		int InFadeStyle = EDecalFadeStyle::Standard);
	float GetFadeAlpha() const { return FadeProperty.FadeAlpha; }
	uint32_t GetFadeStyle() const { return static_cast<uint32_t>(FadeStyle); }
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

	// Internal Fade Property (public 멤버와 동기화됨)
	mutable FDecalFadeProperty FadeProperty;
};