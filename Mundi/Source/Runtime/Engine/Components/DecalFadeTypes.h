#pragma once

#include "FDecalFadeProperty.generated.h"

// Fade style 상수
UENUM(DisplayName="데칼 페이드 스타일")
enum class EDecalFadeStyle : uint8
{
	Standard = 0,			// 기본 알파 페이드
	WipeLeftToRight = 1,    // 왼쪽에서 오른쪽으로 와이프
	Dissolve = 2,           // 랜덤 디졸브
	Iris = 3				// 중앙에서 확장/축소
};

// Decal Fade Property 구조체
USTRUCT(DisplayName="데칼 제어용 변수")
struct FDecalFadeProperty
{
	GENERATED_REFLECTION_BODY()

public:
	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="페이드 속도 (초당 변화량)")
	float FadeSpeed = 0.5f;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="페이드 인 완료까지 걸리는 시간")
	float FadeInDuration = 0.f;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="페이드 인 시작 전 대기 시간")
	float FadeInStartDelay = 0.f;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="페이드 아웃 완료까지 걸리는 시간")
	float FadeOutDuration = 0.f;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="페이드 아웃 시작 전 대기 시간")
	float FadeStartDelay = 0.f;

	UPROPERTY(EditAnywhere, Category="Fade", Range="0.0, 1.0", Tooltip="현재 알파값 (0 ~ 1)")
	float FadeAlpha = 1.f;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="현재 페이드 구간의 경과 시간")
	float ElapsedFadeTime = 0.f;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="현재 페이드 인 중인가?")
	bool bIsFadingIn = false;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="현재 페이드 아웃 중인가?")
	bool bIsFadingOut = false;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="전체 페이드 사이클 완료 여부")
	bool bFadeCompleted = false;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="페이드 아웃 완료 후 제거 여부")
	bool bDestroyedAfterFade = false;

	UPROPERTY(EditAnywhere, Category="Fade", Tooltip="페이드 비주얼 스타일 (0~3)")
	EDecalFadeStyle FadeStyle = EDecalFadeStyle::Standard;

	void StartFadeIn(EDecalFadeStyle InFadeStyle = EDecalFadeStyle::Standard)
	{
		bIsFadingIn = true;
		bIsFadingOut = false;
		bFadeCompleted = false;
		FadeAlpha = 0.f;
		ElapsedFadeTime = -FadeInStartDelay;
		FadeStyle = InFadeStyle;
	}

	void StartFadeOut(EDecalFadeStyle InFadeStyle = EDecalFadeStyle::Standard)
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
