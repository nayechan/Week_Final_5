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
	
	RootComponent = FireBallComponent;

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

	// 로딩 시 Native 컴포넌트 중복 방지 로직:
	// 
	// 1. 생성자에서 생성된 native StaticMeshComponent들이 IsNative() 플래그로 표시됨
	// 2. 씬 파일에서 직렬화된 컴포넌트들이 로드되면 OwnedComponents에 추가됨
	// 3. 이 로직은 다음 순서로 동작:
	//    a) IsNative() 플래그가 true인 모든 StaticMeshComponent를 수집
	//    b) 직렬화된 FireBallComponent로 포인터 갱신 (RootComponent 또는 SceneComponents에서 검색)
	//    c) Native StaticMeshComponent들을 detach하고 제거
	// 4. 이를 통해 PIE나 씬 로드 시 생성자에서 만든 컴포넌트와 직렬화된 컴포넌트가 중복되는 것을 방지
	// 
	// 주의사항:
	// - IsNative() 플래그가 정확하게 설정되어야 함
	// - 플래그가 잘못 설정되면 필요한 컴포넌트가 잘못 제거될 수 있음
	if (bInIsLoading)
	{
		// 1단계: Native StaticMeshComponent 수집
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

		// 2단계: 씬에서 역직렬화된 FireBallComponent를 우선시하여 포인터를 갱신
		if (UFireBallComponent* LoadedFireBall = Cast<UFireBallComponent>(RootComponent))
		{
			FireBallComponent = LoadedFireBall;
		}
		else
		{
			// RootComponent가 아니면 SceneComponents에서 검색
			for (USceneComponent* SceneComp : SceneComponents)
			{
				if (UFireBallComponent* Candidate = Cast<UFireBallComponent>(SceneComp))
				{
					FireBallComponent = Candidate;
					break;
				}
			}
		}

		// 3단계: 생성자에서 만든 Native StaticMeshComponent들을 제거하여 중복 방지
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
