#include "pch.h"
#include "OBB.h"
#include "DecalComponent.h"
#include "StaticMeshComponent.h"
#include "JsonSerializer.h"
#include "BillboardComponent.h"
#include "Gizmo/GizmoArrowComponent.h"

UDecalComponent::UDecalComponent()
{
	UResourceManager::GetInstance().Load<UMaterial>("Shaders/Effects/Decal.hlsl");
	DecalTexture = UResourceManager::GetInstance().Load<UTexture>(GDataDir + "/Textures/grass.jpg");
	bTickEnabled = true;
	bCanEverTick = true;
}

void UDecalComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

}

void UDecalComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
	DirectionGizmo = nullptr;
	SpriteComponent = nullptr;
}

void UDecalComponent::StartFadeIn(float Duration, float Delay, int InFadeStyle)
{
	FadeProperty.FadeInDuration = Duration;
	FadeProperty.FadeInStartDelay = Delay;
	FadeProperty.StartFadeIn(InFadeStyle);
}

void UDecalComponent::StartFadeOut(float Duration, float Delay, bool bDestroyOwner, int InFadeStyle)
{
	FadeProperty.FadeOutDuration = Duration;
	FadeProperty.FadeStartDelay = Delay;
	FadeProperty.bDestroyedAfterFade = bDestroyOwner;
	FadeProperty.StartFadeOut(InFadeStyle);
}

void UDecalComponent::TickComponent(float DeltaTime)
{
	// Public 멤버를 Internal FadeProperty에 동기화
	FadeProperty.FadeSpeed = FadeSpeed;
	FadeProperty.FadeInDuration = FadeInDuration;
	FadeProperty.FadeInStartDelay = FadeInStartDelay;
	FadeProperty.FadeOutDuration = FadeOutDuration;
	FadeProperty.FadeStartDelay = FadeStartDelay;
	FadeProperty.bDestroyedAfterFade = bDestroyedAfterFade;

	// FadeStyle은 StartFadeIn/Out에서 설정되므로 덮어쓰지 않음
	// (페이드가 진행 중이 아닐 때만 public 멤버에서 동기화)
	if (!FadeProperty.bIsFadingIn && !FadeProperty.bIsFadingOut)
	{
		FadeProperty.FadeStyle = FadeStyle;
	}

	// FadeProperty 업데이트
	if (FadeProperty.Update(DeltaTime))
	{
		// 페이드 완료 후 삭제 처리 (World에 지연 삭제 요청)
		if (FadeProperty.bDestroyedAfterFade && FadeProperty.bFadeCompleted)
		{
			if (UWorld* World = GetWorld())
			{
				World->AddPendingKillActor(GetOwner());
			}
		}
	}

	// 업데이트된 FadeStyle을 에디터에 표시
	FadeStyle = FadeProperty.FadeStyle;
}

void UDecalComponent::OnRegister(UWorld* InWorld)
{
	Super::OnRegister(InWorld);

	if (!SpriteComponent && !InWorld->bPie)
	{
		CREATE_EDITOR_COMPONENT(SpriteComponent, UBillboardComponent);
		SpriteComponent->SetTexture(GDataDir + "/UI/Icons/S_DecalActorIcon.dds");

		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);
		// Set gizmo mesh (using the same mesh as GizmoActor's arrow)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// Use world-space scale (not screen-constant scale like typical gizmos)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// Set default scale
		DirectionGizmo->SetDefaultScale(FVector(0.5f, 0.3f, 0.3f));
		DirectionGizmo->SetDirection(FVector(0.5f, 0.0f, 0.0f));
		DirectionGizmo->SetColor(FVector(0.62f, 0.125f, 0.94f));
	}
}

void UDecalComponent::RenderDebugVolume(URenderer* Renderer) const
{
	// 라인 색상
	const FVector4 BoxColor(0.5f, 0.8f, 0.9f, 1.0f); // 하늘색

	// AddLines 함수에 전달할 데이터 배열들을 준비
	TArray<FVector> StartPoints;
	TArray<FVector> EndPoints;
	TArray<FVector4> Colors;
	
	TArray<FVector> Coners = GetWorldOBB().GetCorners();

	const int Edges[12][2] = {
		{6, 4}, {7, 5}, {6, 7}, {4, 5}, // 앞면
		{4, 0}, {5, 1}, {6, 2}, {7, 3}, // 옆면
		{0, 2}, {1, 3}, {0, 1}, {2, 3}  // 뒷면
	};

	// 12개의 선 데이터를 배열에 채워 넣습니다.
	for (int i = 0; i < 12; ++i)
	{
		// 월드 좌표로 변환
		const FVector WorldStart = Coners[Edges[i][0]];
		const FVector WorldEnd = Coners[Edges[i][1]];

		StartPoints.Add(WorldStart);
		EndPoints.Add(WorldEnd);
		Colors.Add(BoxColor);
	}

	// 모든 데이터를 준비한 뒤, 단 한 번의 호출로 렌더러에 전달합니다.
	Renderer->AddLines(StartPoints, EndPoints, Colors);
}

void UDecalComponent::SetDecalTexture(UTexture* InTexture)
{
	DecalTexture = InTexture;
}

void UDecalComponent::SetDecalTexture(const FString& TexturePath)
{
	DecalTexture = UResourceManager::GetInstance().Load<UTexture>(TexturePath);
}

FAABB UDecalComponent::GetWorldAABB() const
{
    // Step 1: Build the decal's oriented box so we can inspect its world-space corners.
    const FOBB DecalOBB = GetWorldOBB();

    // Step 2: Initialize min/max accumulators that will grow to the final axis-aligned bounds.
    FVector MinBounds(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxBounds(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    // Step 3: Evaluate all 8 OBB corners in world-space.
    const FVector& Center = DecalOBB.Center;
    const FVector& HalfExtent = DecalOBB.HalfExtent;
    const FVector (&Axes)[3] = DecalOBB.Axes;

    for (int8 sx = -1; sx <= 1; sx += 2)
    {
        for (int8 sy = -1; sy <= 1; sy += 2)
        {
            for (int8 sz = -1; sz <= 1; sz += 2)
            {
                const FVector Corner = Center
                    + Axes[0] * (HalfExtent.X * static_cast<float>(sx))
                    + Axes[1] * (HalfExtent.Y * static_cast<float>(sy))
                    + Axes[2] * (HalfExtent.Z * static_cast<float>(sz));

                MinBounds.X = std::min(MinBounds.X, Corner.X);
                MinBounds.Y = std::min(MinBounds.Y, Corner.Y);
                MinBounds.Z = std::min(MinBounds.Z, Corner.Z);

                MaxBounds.X = std::max(MaxBounds.X, Corner.X);
                MaxBounds.Y = std::max(MaxBounds.Y, Corner.Y);
                MaxBounds.Z = std::max(MaxBounds.Z, Corner.Z);
            }
        }
    }

    // Step 4: Package the accumulated extremes into a world-space AABB.
    return FAABB(MinBounds, MaxBounds);
}

FOBB UDecalComponent::GetWorldOBB() const
{
    const FVector Center = GetWorldLocation();
    const FVector HalfExtent = GetWorldScale() / 2.0f;

    const FQuat Quat = GetWorldRotation();

    FVector Axes[3];
    Axes[0] = Quat.GetForwardVector();
    Axes[1] = Quat.GetRightVector();
    Axes[2] = Quat.GetUpVector();

    FOBB Obb(Center, HalfExtent, Axes);

    return Obb;
}

FMatrix UDecalComponent::GetDecalProjectionMatrix() const
{
	// 샘플 코드 방식: 데칼의 월드 역행렬 반환
	// 이 행렬은 월드 좌표를 데칼의 로컬 공간 [-0.5, 0.5] 범위로 변환
	// 스케일을 포함한 전체 월드 변환의 역행렬
	const FMatrix DecalWorld = FMatrix::FromTRS(GetWorldLocation(), GetWorldRotation(), GetWorldScale());
	const FMatrix DecalInverseWorld = DecalWorld.InverseAffine();

    return DecalInverseWorld;
}
