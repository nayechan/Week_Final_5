#include "pch.h"
#include "EmptyActor.h"
// IMPLEMENT_CLASS is now auto-generated in .generated.cpp
//BEGIN_PROPERTIES(AEmptyActor)
//	MARK_AS_SPAWNABLE("액터", "액터는 월드에 배치 또는 스폰할 수 있는 오브젝트입니다.")
//END_PROPERTIES()

AEmptyActor::AEmptyActor()
{
	ObjectName = "Actor";
	// RootComponent는 AActor 생성자에서 이미 기본 USceneComponent로 생성됨
	// 별도로 컴포넌트를 추가하지 않음
}

void AEmptyActor::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void AEmptyActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);


}
