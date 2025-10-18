#include "pch.h"
#include "FireBallActor.h"
#include "StaticMeshComponent.h"
#include "WorldPartitionManager.h"

IMPLEMENT_CLASS(AFireBallActor)

BEGIN_PROPERTIES(AFireBallActor)
	MARK_AS_SPAWNABLE("파이어볼", "구 형태의 발광 효과를 가진 액터입니다.")
END_PROPERTIES()

AFireBallActor::AFireBallActor()
{
	Name = "Fire Ball Actor";
	FireBallComponent = CreateDefaultSubobject<UFireBallComponent>("FireBallComponent");
	// 루트 교체
	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = FireBallComponent;
	RemoveOwnedComponent(TempRootComponent);

	// TargetActorTransformWidget.cpp의 TryAttachComponentToActor 메소드 참고해 구체 스태틱메시 컴포넌트 부착하는 코드 구현
	// TryAttachComponentToActor는 익명 네임스페이스에 있어 직접 사용 불가능
	UStaticMeshComponent* FireBallSMC = CreateDefaultSubobject<UStaticMeshComponent>("FireBallStaticMeshComponent");
	FireBallSMC->SetStaticMesh("Data/Model/Sphere.obj");
	FireBallSMC->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
}

AFireBallActor::~AFireBallActor()
{
}

void AFireBallActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();
	// 자식을 순회하면서 UDecalComponent를 찾음
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UFireBallComponent* FireBall = Cast<UFireBallComponent>(Component))
		{
			FireBallComponent = FireBall;
			break;
		}
	}
}

void AFireBallActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		TArray<UStaticMeshComponent*> NativeStaticMeshes;
		for (UActorComponent* Component : OwnedComponents)
		{
			if (UStaticMeshComponent* StaticMesh = Cast<UStaticMeshComponent>(Component))
			{
				if (StaticMesh->IsNative())
				{
					NativeStaticMeshes.push_back(StaticMesh);
				}
			}
		}

		// 씬에서 역직렬화된 FireBallComponent를 우선시하여 포인터를 갱신.
		if (UFireBallComponent* LoadedFireBall = Cast<UFireBallComponent>(RootComponent))
		{
			FireBallComponent = LoadedFireBall;
		}
		else
		{
			for (USceneComponent* SceneComp : SceneComponents)
			{
				if (UFireBallComponent* Candidate = Cast<UFireBallComponent>(SceneComp))
				{
					FireBallComponent = Candidate;
					break;
				}
			}
		}

		// 생성자에서 만든 기본 스태틱메시 컴포넌트를 제거해 중복을 방지.
		for (UStaticMeshComponent* NativeMesh : NativeStaticMeshes)
		{
			if (!NativeMesh)
			{
				continue;
			}
			NativeMesh->DetachFromParent();
			RemoveOwnedComponent(NativeMesh);
		}
	}
}
