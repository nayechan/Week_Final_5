#pragma once
#include "SViewerWindow.h"
#include "Source/Runtime/Engine/Viewer/ViewerState.h"

class FViewport;
class FViewportClient;
class UWorld;
struct ID3D11Device;
class UParticleEmitter;
class UParticleModule;

class SParticleEditorWindow : public SViewerWindow
{
public:
	SParticleEditorWindow();
	virtual ~SParticleEditorWindow();

	virtual void OnRender() override;
	virtual void OnUpdate(float DeltaSeconds) override;
	virtual void PreRenderViewportUpdate() override;

protected:
	virtual ViewerState* CreateViewerState(const char* Name, UEditorAssetPreviewContext* Context) override;
	virtual void DestroyViewerState(ViewerState*& State) override;
	virtual FString GetWindowTitle() const override { return "Particle Editor"; }
	virtual void RenderTabsAndToolbar(EViewerType CurrentViewerType) override;

	// 패널 렌더링
	virtual void RenderLeftPanel(float PanelWidth) override;   // 뷰포트 + 디테일
	virtual void RenderRightPanel() override;                  // 이미터 패널
	virtual void RenderBottomPanel() override;                 // 커브 에디터

private:
	// 툴바
	void RenderToolbar();

	// 디테일 패널
	void RenderDetailsPanel(float PanelWidth);

	// 이미터 패널 하위 렌더링
	void RenderEmitterColumn(int32 EmitterIndex, UParticleEmitter* Emitter);
	void RenderModuleBlock(int32 EmitterIdx, int32 ModuleIdx, UParticleModule* Module);

	// 뷰포트
	void RenderViewportArea(float width, float height);

	// 파일 작업
	void CreateNewParticleSystem();
	void SaveParticleSystem();
	void SaveParticleSystemAs();
	void LoadParticleSystem();

	// 레이아웃 비율 및 크기
	float BottomPanelHeight = 250.f;
	float ViewportDetailsRatio = 0.6f;  // 왼쪽 패널 내에서 뷰포트:디테일 비율

	// 헬퍼 함수
	ParticleEditorState* GetActiveParticleState() const
	{
		return static_cast<ParticleEditorState*>(ActiveState);
	}
};
