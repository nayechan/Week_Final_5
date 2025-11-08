#include "pch.h"
#include "StaticMeshActor.h"
#include "StaticMeshComponent.h"
#include "ObjectFactory.h"
#include "BillboardComponent.h"
#include "ShapeComponent.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(AStaticMeshActor)
//	MARK_AS_SPAWNABLE("스태틱 메시", "정적 메시를 렌더링하는 액터입니다.")
//END_PROPERTIES()

AStaticMeshActor::AStaticMeshActor()
{
    ObjectName = "Static Mesh Actor";
    StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("StaticMeshComponent");
    
    // 루트 교체
    RootComponent = StaticMeshComponent;
}
 
void AStaticMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

AStaticMeshActor::~AStaticMeshActor()
{
   
}

FAABB AStaticMeshActor::GetBounds() const
{
    // 멤버 변수를 직접 사용하지 않고, 현재의 RootComponent를 확인하도록 수정
    // 기본 컴포넌트가 제거되는 도중에 어떤 로직에 의해 GetBounds() 함수가 호출
    // 이 시점에 AStaticMeshActor의 멤버변수인 UStaticMeshComponent는 아직 새로운 컴포넌트
    // ID 751로 업데이트되기 전. 제거된 기본 컴포넌트를 여전히 가리키고 있음. 
    // 유효하지 않은 staticmeshcomponent 포인터의 getworldaabb 함수를 호출 시도.

    UStaticMeshComponent* CurrentSMC = Cast<UStaticMeshComponent>(RootComponent);
    if (CurrentSMC)
    {
        return CurrentSMC->GetWorldAABB();
    }

    return FAABB();
}

void AStaticMeshActor::SetStaticMeshComponent(UStaticMeshComponent* InStaticMeshComponent)
{
    StaticMeshComponent = InStaticMeshComponent;
}

void AStaticMeshActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();
    for (UActorComponent* Component : OwnedComponents)
    {
        if (UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component))
        {
            StaticMeshComponent = StaticMeshComp;
            break;
        }
    }
}

void AStaticMeshActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        StaticMeshComponent = Cast<UStaticMeshComponent>(RootComponent);
    }
}
