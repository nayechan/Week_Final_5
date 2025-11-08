#include "pch.h"
#include "ActorComponent.h"
#include "Actor.h"
#include "World.h"
#include "SelectionManager.h"

//BEGIN_PROPERTIES(UActorComponent)
//    ADD_PROPERTY(FName, ObjectName, "[컴포넌트]", true, "컴포넌트의 이름입니다")
//    ADD_PROPERTY(bool, bIsActive, "[컴포넌트]", true, "컴포넌트를 활성화합니다")
//    ADD_PROPERTY(bool, bTickEnabled, "[컴포넌트]", true, "틱을 확성화합니다. 기본적으로 틱이 가능한 컴포넌트만 영향이 있습니다.")
//END_PROPERTIES()
    
UActorComponent::UActorComponent()
{
}

UActorComponent::~UActorComponent()
{
}

UWorld* UActorComponent::GetWorld() const
{
    return Owner ? Owner->GetWorld() : nullptr;
}

// 외부에서 호출되는 컴포넌트 삭제 요청 (= 이 함수 끝나고 너 월드에서 지울 거임)
void UActorComponent::DestroyComponent()
{
    // TODO: 실제로도 컴포넌트 PendingDestroy 적용 필요 (액터는 지연 삭제 처리됨)
    if (bPendingDestroy)
    {
        return;
    }
    bPendingDestroy = true;

    // 스스로 등록 해제
    UnregisterComponent();

    // UObject 메모리 해제
    DeleteObject(this);
}

// ─────────────── Registration

// 외부에서 호출해줌 (왜냐하면 외부에서 설정이 끝난 후 호출해야 하기 때문에)
void UActorComponent::RegisterComponent(UWorld* InWorld)
{
    if (bRegistered)
    {
        return;
    }

    bRegistered = true;
    OnRegister(InWorld);
}

// DestroyComponent에서 스스로 호출됨 (내부에서도 처리 가능하기 때문에)
void UActorComponent::UnregisterComponent()
{
    if (!bRegistered)
    {
        return;
    }

    OnUnregister();
    bRegistered = false;
}

// Override시 Super::OnRegister() 권장
void UActorComponent::OnRegister(UWorld* InWorld)
{
    // 리소스/핸들 생성, 메시/버퍼 생성 등(프레임 비의존)
}

// Override시 Super::OnUnregister() 권장
void UActorComponent::OnUnregister()
{
    // 리소스/핸들 반납
}

// ─────────────── Lifecycle (게임 수명)

void UActorComponent::InitializeComponent()
{
    // BeginPlay 이전 초기화(게임 수명 관련 초기화)
}

// Override시 Super::BeginPlay() 권장
void UActorComponent::BeginPlay()
{
    // 외부에서 정확하게 호출해줘야 함
    // PIE 시작 후 월드 등록 시 1회
    // 필요하다면 Override
}

void UActorComponent::TickComponent(float DeltaTime)
{
    // 매 프레임 처리
}

// Override시 Super::EndPlay() 권장
void UActorComponent::EndPlay()
{
    // 외부에서 정확하게 호출해줘야 함
    // PIE 종료 or PIE 중 파괴 시
    // PIE 종료 시에는 World에서 일괄적으로 호출해줌
    // 
    // 필요하다면 Override
}

void UActorComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    Owner = nullptr; // Actor에서 이거 설정해 줌
}

void UActorComponent::PostDuplicate()
{
    Super::PostDuplicate(); // 상위 UObject::PostDuplicate 호출

    bRegistered = false;
}

void UActorComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    // UActorComponent는 기본적으로 직렬화할 추가 데이터가 없음
    // 파생 클래스에서 필요한 데이터를 직렬화하도록 오버라이드
}