#include "pch.h"
#include "PhysicsAssetEditorBootstrap.h"
#include "ViewerState.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "PhysicsAsset.h"
#include "SkeletalBodySetup.h"
#include "PhysicsConstraintTemplate.h"
#include "SkeletalMesh.h"
#include "SkeletalMeshActor.h"
#include "Grid/GridActor.h"
#include "JsonSerializer.h"
#include "EditorAssetPreviewContext.h"
#include "Source/Runtime/Engine/Components/LineComponent.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "Source/Runtime/Physics/PrimitiveDrawInterface.h"
#include "ResourceManager.h"

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

	// Viewport 생성
	State->Viewport = new FViewport();
	State->Viewport->Initialize(0.f, 0.f, 1.f, 1.f, InDevice);
	State->Viewport->SetUseRenderTarget(true);

	// ViewportClient 생성
	FViewportClient* Client = new FViewportClient();
	Client->SetWorld(State->World);
	Client->SetViewportType(EViewportType::Perspective);
	Client->SetViewMode(EViewMode::VMI_Lit_Phong);

	// 카메라 설정 (SkeletalViewer와 동일)
	Client->GetCamera()->SetActorLocation(FVector(3, 0, 1));
	Client->GetCamera()->SetRotationFromEulerAngles(FVector(0.f, 0.f, 180.f));

	State->Client = Client;
	State->Viewport->SetViewportClient(Client);
	State->World->SetEditorCameraActor(Client->GetCamera());

	// 프리뷰 액터 생성 (SkeletalMesh를 담을 액터)
	if (State->World)
	{
		ASkeletalMeshActor* PreviewActor = State->World->SpawnActor<ASkeletalMeshActor>();
		if (PreviewActor)
		{
			State->PreviewActor = PreviewActor;

			UPhysicsAsset* PhysAsset = nullptr;

			// Case 3: ContentBrowser에서 .physicsasset 더블클릭 (AssetPath 사용)
			if (Context && !Context->AssetPath.empty() && Context->AssetPath.find(".physicsasset") != FString::npos)
			{
				PhysAsset = LoadPhysicsAsset(Context->AssetPath);
				if (PhysAsset)
				{
					State->CurrentFilePath = Context->AssetPath;
					State->SkeletalMeshPath = PhysAsset->SkeletalMeshPath;
					State->bIsDirty = false;
					UE_LOG("[PhysicsAssetEditorBootstrap] PhysicsAsset 로드: %s", Context->AssetPath.c_str());

					// PhysicsAsset에서 메시 경로 추출하여 프리뷰 메시 로드
					if (!PhysAsset->SkeletalMeshPath.empty())
					{
						USkeletalMesh* PreviewMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(PhysAsset->SkeletalMeshPath.c_str());
						if (PreviewMesh && PreviewActor->GetSkeletalMeshComponent())
						{
							PreviewActor->GetSkeletalMeshComponent()->SetSkeletalMesh(PreviewMesh->GetPathFileName());
						}
					}
					else
					{
						State->PendingWarningMessage = "메시 정보가 없는 PhysicsAsset입니다.\n프리뷰를 표시할 수 없습니다.";
					}
				}
			}
			// Case 1 & 2: PropertyRenderer에서 버튼 클릭 (SkeletalMeshPath, PhysicsAssetPath 사용)
			else if (Context)
			{
				// 스켈레탈 메시 경로 설정
				State->SkeletalMeshPath = Context->SkeletalMeshPath;

				// PhysicsAsset 경로가 있으면 로드 (Case 2)
				if (!Context->PhysicsAssetPath.empty())
				{
					PhysAsset = LoadPhysicsAsset(Context->PhysicsAssetPath);
					if (PhysAsset)
					{
						State->CurrentFilePath = Context->PhysicsAssetPath;
						State->bIsDirty = false;
						UE_LOG("[PhysicsAssetEditorBootstrap] PhysicsAsset 로드: %s", Context->PhysicsAssetPath.c_str());
					}
				}

				// 프리뷰 메시 로드
				if (!State->SkeletalMeshPath.empty())
				{
					USkeletalMesh* PreviewMesh = UResourceManager::GetInstance().Load<USkeletalMesh>(State->SkeletalMeshPath.c_str());
					if (PreviewMesh && PreviewActor->GetSkeletalMeshComponent())
					{
						PreviewActor->GetSkeletalMeshComponent()->SetSkeletalMesh(PreviewMesh->GetPathFileName());
					}
				}
			}

			// PhysicsAsset이 없으면 기본 생성 (Case 1)
			if (!PhysAsset)
			{
				PhysAsset = CreateDefaultPhysicsAsset();
				PhysAsset->SkeletalMeshPath = State->SkeletalMeshPath;  // 메시 경로 설정
				State->CurrentFilePath = "";  // 새 파일이므로 경로 없음
				UE_LOG("[PhysicsAssetEditorBootstrap] 기본 PhysicsAsset 생성");
			}

			State->EditingPhysicsAsset = PhysAsset;

			// 바디 Shape 시각화용 LineComponent 생성
			ULineComponent* BodyLineComp = NewObject<ULineComponent>();
			BodyLineComp->SetAlwaysOnTop(true);
			PreviewActor->AddOwnedComponent(BodyLineComp);
			BodyLineComp->RegisterComponent(State->World);
			BodyLineComp->SetLineVisible(true);
			State->BodyShapeLineComponent = BodyLineComp;

			// Constraint 시각화용 LineComponent 생성
			ULineComponent* ConstraintLineComp = NewObject<ULineComponent>();
			ConstraintLineComp->SetAlwaysOnTop(true);
			PreviewActor->AddOwnedComponent(ConstraintLineComp);
			ConstraintLineComp->RegisterComponent(State->World);
			ConstraintLineComp->SetLineVisible(true);
			State->ConstraintLineComponent = ConstraintLineComp;

			// PrimitiveDrawInterface 생성 및 초기화
			State->PDI = new FPrimitiveDrawInterface();
			State->PDI->Initialize(BodyLineComp);
			State->bShapeLinesDirty = true;  // 초기 렌더링 필요
		}
	}

	return State;
}

void PhysicsAssetEditorBootstrap::DestroyViewerState(ViewerState*& State)
{
	if (!State) return;

	PhysicsAssetEditorState* PhysState = static_cast<PhysicsAssetEditorState*>(State);

	// PDI 정리
	if (PhysState->PDI)
	{
		delete PhysState->PDI;
		PhysState->PDI = nullptr;
	}

	// Viewport/Client 정리
	if (PhysState->Viewport)
	{
		delete PhysState->Viewport;
		PhysState->Viewport = nullptr;
	}

	if (PhysState->Client)
	{
		delete PhysState->Client;
		PhysState->Client = nullptr;
	}

	// World 정리 (PreviewActor와 LineComponent들은 World가 소유하므로 자동 정리)
	if (PhysState->World)
	{
		ObjectFactory::DeleteObject(PhysState->World);
		PhysState->World = nullptr;
	}

	delete PhysState;
	State = nullptr;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::CreateDefaultPhysicsAsset()
{
	UPhysicsAsset* PhysAsset = NewObject<UPhysicsAsset>();
	// 기본적으로 빈 PhysicsAsset 생성
	return PhysAsset;
}

bool PhysicsAssetEditorBootstrap::SavePhysicsAsset(UPhysicsAsset* Asset, const FString& FilePath)
{
	if (!Asset || FilePath.empty()) return false;

	Asset->Save(FilePath);
	return true;
}

UPhysicsAsset* PhysicsAssetEditorBootstrap::LoadPhysicsAsset(const FString& FilePath)
{
	if (FilePath.empty()) return nullptr;

	UPhysicsAsset* PhysAsset = UResourceManager::GetInstance().Load<UPhysicsAsset>(FilePath.c_str());
	return PhysAsset;
}
