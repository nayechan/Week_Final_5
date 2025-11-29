#include "pch.h"
#include "PhysicsAssetEditorBootstrap.h"
#include "ViewerState.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "Source/Runtime/Engine/Physics/PhysicsAsset.h"
#include "Source/Runtime/Engine/Physics/PhysicsTypes.h"
#include "SkeletalMeshActor.h"
#include "SkeletalMesh.h"
#include "Grid/GridActor.h"
#include "JsonSerializer.h"
#include "EditorAssetPreviewContext.h"
#include "PathUtils.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"

ViewerState* PhysicsAssetEditorBootstrap::CreateViewerState(const char* Name, UWorld* InWorld,
	ID3D11Device* InDevice, UEditorAssetPreviewContext* Context)
{
	if (!InDevice) return nullptr;

	// PhysicsAssetEditorState 생성
	PhysicsAssetEditorState* State = new PhysicsAssetEditorState();
	State->Name = Name ? Name : "Physics Asset Editor";

	// Preview world 생성
	State->World = NewObject<UWorld>();
	State->World->SetWorldType(EWorldType::PreviewMinimal);
	State->World->Initialize();
	State->World->GetRenderSettings().DisableShowFlag(EEngineShowFlags::SF_EditorIcon);
	// 그리드는 Physics Asset Editor에서는 유지

	// Viewport 생성
	State->Viewport = new FViewport();
	State->Viewport->Initialize(0.f, 0.f, 1.f, 1.f, InDevice);
	State->Viewport->SetUseRenderTarget(true);

	// ViewportClient 생성
	FViewportClient* Client = new FViewportClient();
	Client->SetWorld(State->World);
	Client->SetViewportType(EViewportType::Perspective);
	Client->SetViewMode(EViewMode::VMI_Lit_Phong);

	// 카메라 설정 (스켈레탈 메시를 볼 수 있는 위치)
	Client->GetCamera()->SetActorLocation(FVector(200.f, 0.f, 100.f));
	Client->GetCamera()->SetRotationFromEulerAngles(FVector(0.f, 15.f, 180.f));

	State->Client = Client;
	State->Viewport->SetViewportClient(Client);
	State->World->SetEditorCameraActor(Client->GetCamera());

	// 프리뷰 액터 생성 (스켈레탈 메시 렌더링용)
	if (State->World)
	{
		ASkeletalMeshActor* PreviewActor = State->World->SpawnActor<ASkeletalMeshActor>();
		if (PreviewActor)
		{
			PreviewActor->SetActorLocation(FVector(0.f, 0.f, 0.f));
			State->PreviewActor = PreviewActor;

			// Context에 AssetPath가 있으면 파일에서 로드, 없으면 기본 에셋 생성
			UPhysicsAsset* Asset = nullptr;
			if (Context && !Context->AssetPath.empty())
			{
				// 파일에서 Physics Asset 로드
				Asset = LoadPhysicsAsset(Context->AssetPath);
				if (Asset)
				{
					State->CurrentFilePath = Context->AssetPath;
					State->bIsDirty = false;
					UE_LOG("[PhysicsAssetEditorBootstrap] Physics Asset 로드: %s", Context->AssetPath.c_str());

					// 연결된 스켈레탈 메시 로드
					if (!Asset->SkeletalMeshPath.empty())
					{
						// SetSkeletalMesh는 경로를 받아 내부에서 로드함
						PreviewActor->GetSkeletalMeshComponent()->SetSkeletalMesh(Asset->SkeletalMeshPath);
						State->CurrentMesh = PreviewActor->GetSkeletalMeshComponent()->GetSkeletalMesh();
						State->LoadedMeshPath = Asset->SkeletalMeshPath;
					}
				}
			}

			// 로드 실패하거나 AssetPath가 없으면 기본 에셋 생성
			if (!Asset)
			{
				Asset = CreateDefaultPhysicsAsset();
				UE_LOG("[PhysicsAssetEditorBootstrap] 기본 Physics Asset 생성");
			}

			State->EditingAsset = Asset;

			// Body 프리뷰 LineComponent 생성
			ULineComponent* BodyLineComp = NewObject<ULineComponent>();
			BodyLineComp->SetAlwaysOnTop(true);
			PreviewActor->AddOwnedComponent(BodyLineComp);
			BodyLineComp->RegisterComponent(State->World);
			BodyLineComp->SetLineVisible(true);
			State->BodyPreviewLineComponent = BodyLineComp;

			// Constraint 프리뷰 LineComponent 생성
			ULineComponent* ConstraintLineComp = NewObject<ULineComponent>();
			ConstraintLineComp->SetAlwaysOnTop(true);
			PreviewActor->AddOwnedComponent(ConstraintLineComp);
			ConstraintLineComp->RegisterComponent(State->World);
			ConstraintLineComp->SetLineVisible(true);
			State->ConstraintPreviewLineComponent = ConstraintLineComp;
		}
	}

	return State;
}

void PhysicsAssetEditorBootstrap::DestroyViewerState(ViewerState*& State)
{
	if (!State) return;

	// PhysicsAssetEditorState로 캐스팅
	PhysicsAssetEditorState* PhysicsState = static_cast<PhysicsAssetEditorState*>(State);

	// 리소스 정리
	if (PhysicsState->Viewport)
	{
		delete PhysicsState->Viewport;
		PhysicsState->Viewport = nullptr;
	}

	if (PhysicsState->Client)
	{
		delete PhysicsState->Client;
		PhysicsState->Client = nullptr;
	}

	if (PhysicsState->World)
	{
		ObjectFactory::DeleteObject(PhysicsState->World);
		PhysicsState->World = nullptr;
	}

	// Physics Asset 관련 포인터 정리
	// EditingAsset은 ResourceManager가 관리할 수 있으므로 delete 하지 않음
	PhysicsState->EditingAsset = nullptr;
	PhysicsState->BodyPreviewLineComponent = nullptr;
	PhysicsState->ConstraintPreviewLineComponent = nullptr;

	delete State;
	State = nullptr;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::CreateDefaultPhysicsAsset()
{
	// 빈 Physics Asset 생성
	UPhysicsAsset* DefaultAsset = NewObject<UPhysicsAsset>();

	// 기본값만 설정, 바디와 제약 조건은 빈 상태로 유지
	// 사용자가 스켈레탈 메시를 로드하면 해당 본에 바디를 추가할 수 있음

	return DefaultAsset;
}

bool PhysicsAssetEditorBootstrap::SavePhysicsAsset(UPhysicsAsset* Asset, const FString& FilePath)
{
	// 입력 검증
	if (!Asset)
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: Asset이 nullptr입니다");
		return false;
	}

	if (FilePath.empty())
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: FilePath가 비어있습니다");
		return false;
	}

	// JSON 객체 생성
	JSON JsonHandle = JSON::Make(JSON::Class::Object);

	// PhysicsAsset 직렬화 (false = 저장 모드)
	Asset->Serialize(false, JsonHandle);

	// FString을 FWideString으로 변환 (UTF-8 → UTF-16)
	FWideString WidePath = UTF8ToWide(FilePath);

	// 파일로 저장
	if (!FJsonSerializer::SaveJsonToFile(JsonHandle, WidePath))
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: 파일 저장 실패: %s", FilePath.c_str());
		return false;
	}

	UE_LOG("[PhysicsAssetEditorBootstrap] SavePhysicsAsset: 저장 성공: %s", FilePath.c_str());
	return true;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::LoadPhysicsAsset(const FString& FilePath)
{
	// 입력 검증
	if (FilePath.empty())
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: FilePath가 비어있습니다");
		return nullptr;
	}

	// 경로를 상대 경로로 정규화
	FString NormalizedFilePath = ResolveAssetRelativePath(NormalizePath(FilePath), "");

	// ResourceManager에서 이미 로드된 Physics Asset 확인
	UPhysicsAsset* ExistingAsset = UResourceManager::GetInstance().Get<UPhysicsAsset>(NormalizedFilePath);
	if (ExistingAsset)
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 캐시된 에셋 반환: %s", NormalizedFilePath.c_str());
		return ExistingAsset;
	}

	// FString을 FWideString으로 변환 (UTF-8 → UTF-16)
	FWideString WidePath = UTF8ToWide(NormalizedFilePath);

	// 파일에서 JSON 로드
	JSON JsonHandle;
	if (!FJsonSerializer::LoadJsonFromFile(JsonHandle, WidePath))
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 파일 로드 실패: %s", NormalizedFilePath.c_str());
		return nullptr;
	}

	// 새로운 PhysicsAsset 객체 생성
	UPhysicsAsset* LoadedAsset = NewObject<UPhysicsAsset>();
	if (!LoadedAsset)
	{
		UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: PhysicsAsset 객체 생성 실패");
		return nullptr;
	}

	// PhysicsAsset 역직렬화 (true = 로딩 모드)
	LoadedAsset->Serialize(true, JsonHandle);

	// ResourceManager에 등록
	UResourceManager::GetInstance().Add<UPhysicsAsset>(NormalizedFilePath, LoadedAsset);

	UE_LOG("[PhysicsAssetEditorBootstrap] LoadPhysicsAsset: 로드 성공: %s", NormalizedFilePath.c_str());
	return LoadedAsset;
}
