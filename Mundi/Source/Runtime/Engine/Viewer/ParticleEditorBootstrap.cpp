#include "pch.h"
#include "ParticleEditorBootstrap.h"
#include "ViewerState.h"
#include "CameraActor.h"
#include "FViewport.h"
#include "FViewportClient.h"
#include "ParticleSystem.h"
#include "ParticleEmitter.h"
#include "ParticleLODLevel.h"
#include "ParticleModuleRequired.h"
#include "ParticleSystemComponent.h"
#include "Actor.h"
#include "ObjectFactory.h"
#include "World.h"

ViewerState* ParticleEditorBootstrap::CreateViewerState(const char* Name, UWorld* InWorld,
	ID3D11Device* InDevice, UEditorAssetPreviewContext* Context)
{
	if (!InDevice) return nullptr;

	// ParticleEditorState 생성
	ParticleEditorState* State = new ParticleEditorState();
	State->Name = Name ? Name : "Particle Editor";

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

	// 카메라 설정
	Client->GetCamera()->SetActorLocation(FVector(-2.f, 0.f, 1.f));
	Client->GetCamera()->SetRotationFromEulerAngles(FVector(0.f, 20.f, 0.f));

	State->Client = Client;
	State->Viewport->SetViewportClient(Client);
	State->World->SetEditorCameraActor(Client->GetCamera());

	// 프리뷰 액터 생성 (파티클 시스템을 담을 액터)
	if (State->World)
	{
		AActor* PreviewActor = State->World->SpawnActor<AActor>();
		if (PreviewActor)
		{
			// ParticleSystemComponent 생성 및 연결
			UParticleSystemComponent* ParticleComp = NewObject<UParticleSystemComponent>();
			PreviewActor->AddOwnedComponent(ParticleComp);
			ParticleComp->RegisterComponent(State->World);
			ParticleComp->SetWorldLocation(FVector(0.f, 0.f, 0.f));

			State->PreviewActor = PreviewActor;
			State->PreviewComponent = ParticleComp;

			// 기본 파티클 시스템 생성
			UParticleSystem* DefaultTemplate = NewObject<UParticleSystem>();

			// 기본 이미터 1개 생성
			UParticleEmitter* DefaultEmitter = NewObject<UParticleEmitter>();
			UParticleLODLevel* LOD = NewObject<UParticleLODLevel>();
			LOD->RequiredModule = NewObject<UParticleModuleRequired>();
			LOD->bEnabled = true;

			DefaultEmitter->LODLevels.Add(LOD);
			DefaultTemplate->Emitters.Add(DefaultEmitter);

			State->EditingTemplate = DefaultTemplate;
			State->PreviewComponent->SetTemplate(DefaultTemplate);
		}
	}

	return State;
}

void ParticleEditorBootstrap::DestroyViewerState(ViewerState*& State)
{
	if (!State) return;

	// ParticleEditorState로 캐스팅
	ParticleEditorState* ParticleState = static_cast<ParticleEditorState*>(State);

	// 리소스 정리
	if (ParticleState->Viewport)
	{
		delete ParticleState->Viewport;
		ParticleState->Viewport = nullptr;
	}

	if (ParticleState->Client)
	{
		delete ParticleState->Client;
		ParticleState->Client = nullptr;
	}

	if (ParticleState->World)
	{
		ObjectFactory::DeleteObject(ParticleState->World);
		ParticleState->World = nullptr;
	}

	// 파티클 관련 포인터는 World가 정리되면서 자동으로 정리됨
	ParticleState->EditingTemplate = nullptr;
	ParticleState->PreviewComponent = nullptr;
	ParticleState->PreviewActor = nullptr;
	ParticleState->SelectedModule = nullptr;

	delete State;
	State = nullptr;
}
