#include "pch.h"
#include "PhysicalMaterial.h"
#include "PhysicsSystem.h"
using namespace physx;

void UPhysicalMaterial::CreateMaterial()
{
    if (MatHandle) { return; }

    // PhysX 재질 생성
    MatHandle = GEngine.GetPhysicsSystem()->GetPhysics()->createMaterial(
        StaticFriction,
        DynamicFriction,
        Restitution
    );

    if (!MatHandle) { return; }

    // UserData 연결
    MatHandle->userData = static_cast<void*>(this);

    // 합산 방식 설정
    MatHandle->setFrictionCombineMode(ToPxCombineMode(FrictionCombineMode));
    MatHandle->setRestitutionCombineMode(ToPxCombineMode(RestitutionCombineMode));
}

void UPhysicalMaterial::UpdateMaterial()
{
    if (!MatHandle) { return; }

    // 속성 업데이트
    MatHandle->setStaticFriction(StaticFriction);
    MatHandle->setDynamicFriction(DynamicFriction);
    MatHandle->setRestitution(Restitution);
    MatHandle->setFrictionCombineMode(ToPxCombineMode(FrictionCombineMode));
    MatHandle->setRestitutionCombineMode(ToPxCombineMode(RestitutionCombineMode));
}

void UPhysicalMaterial::Release()
{
    if (MatHandle)
    {
        MatHandle->release();
        MatHandle = nullptr;
    }
}

void UPhysicalMaterial::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
    // FString을 FWideString으로 변환
    FWideString WidePath = UTF8ToWide(InFilePath);

    // 파일에서 JSON 로드
    JSON JsonHandle;
    if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
    {
        UE_LOG("[UPhysicsAsset] Load 실패: %s", InFilePath.c_str());
        return;
    }

    // 역직렬화
    Serialize(true, JsonHandle);
    SetFilePath(InFilePath);
}

void UPhysicalMaterial::Save(const FString& InFilePath)
{
    // FString을 FWideString으로 변환
    FWideString WidePath = UTF8ToWide(InFilePath);

    JSON JsonData = JSON::Make(JSON::Class::Object);
    Serialize(false, JsonData);

    // 파일에 JSON 저장
    if (!FJsonSerializer::SaveJsonToFile(JsonData, WidePath))
    {
        UE_LOG("[UPhysicsAsset] Save 실패: %s", InFilePath.c_str());
        return;
    }

    SetFilePath(InFilePath);
}

PxCombineMode::Enum UPhysicalMaterial::ToPxCombineMode(EFrictionCombineMode Mode) const
{
    switch (Mode)
    {
    case EFrictionCombineMode::Average:  return PxCombineMode::eAVERAGE;
    case EFrictionCombineMode::Min:      return PxCombineMode::eMIN;
    case EFrictionCombineMode::Multiply: return PxCombineMode::eMULTIPLY;
    case EFrictionCombineMode::Max:      return PxCombineMode::eMAX;
    default:                             return PxCombineMode::eAVERAGE;
    }
}

// --- 프리셋 생성 함수 for Claude ---

UPhysicalMaterial* UPhysicalMaterial::CreateDefaultMaterial()
{
    UPhysicalMaterial* Mat = new UPhysicalMaterial();
    Mat->StaticFriction = 0.5f;
    Mat->DynamicFriction = 0.5f;
    Mat->Restitution = 0.3f;
    Mat->Density = 1000.0f;
    Mat->CreateMaterial();
    return Mat;
}

UPhysicalMaterial* UPhysicalMaterial::CreateMetalMaterial()
{
    UPhysicalMaterial* Mat = new UPhysicalMaterial();
    Mat->StaticFriction = 0.4f;
    Mat->DynamicFriction = 0.3f;
    Mat->Restitution = 0.4f;
    Mat->Density = 7800.0f;  // 철 밀도
    Mat->CreateMaterial();
    return Mat;
}

UPhysicalMaterial* UPhysicalMaterial::CreateWoodMaterial()
{
    UPhysicalMaterial* Mat = new UPhysicalMaterial();
    Mat->StaticFriction = 0.5f;
    Mat->DynamicFriction = 0.4f;
    Mat->Restitution = 0.3f;
    Mat->Density = 600.0f;   // 나무 밀도
    Mat->CreateMaterial();
    return Mat;
}

UPhysicalMaterial* UPhysicalMaterial::CreateRubberMaterial()
{
    UPhysicalMaterial* Mat = new UPhysicalMaterial();
    Mat->StaticFriction = 0.9f;
    Mat->DynamicFriction = 0.8f;
    Mat->Restitution = 0.8f;   // 높은 반발력
    Mat->Density = 1100.0f;
    Mat->CreateMaterial();
    return Mat;
}

UPhysicalMaterial* UPhysicalMaterial::CreateIceMaterial()
{
    UPhysicalMaterial* Mat = new UPhysicalMaterial();
    Mat->StaticFriction = 0.05f;
    Mat->DynamicFriction = 0.03f;
    Mat->Restitution = 0.1f;
    Mat->Density = 920.0f;   // 얼음 밀도
    Mat->CreateMaterial();
    return Mat;
}
