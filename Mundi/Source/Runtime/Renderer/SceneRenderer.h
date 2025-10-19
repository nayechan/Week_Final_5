#pragma once
#include "Frustum.h"

// 전방 선언 (헤더 파일 의존성 최소화)
class UWorld;
class ACameraActor;
class FViewport;
class URenderer;
class D3D11RHI;
class UPrimitiveComponent;
class UDecalComponent;
class UFireBallComponent;
class UHeightFogComponent;
class UAmbientLightComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotLightComponent;
struct FMeshBatchElement;
class UMeshComponent;
class UBillboardComponent;
class UTextRenderComponent;
class FSceneView;
class FTileLightCuller;

struct FCandidateDrawable;

// 렌더링할 대상들의 집합을 담는 구조체
struct FVisibleRenderProxySet
{
	TArray<UMeshComponent*> Meshes;
	TArray<UBillboardComponent*> Billboards;
	TArray<UTextRenderComponent*> Texts;

	TArray<UDecalComponent*> Decals;
	TArray<UFireBallComponent*> FireBalls;
};

struct FSceneLocals
{
	TArray<UPointLightComponent*> PointLights;
	TArray<USpotLightComponent*> SpotLights;
};

// NOTE: 추후 UWorld로 이동해서 등록/해지 방식으로 변경?
// 전역 효과 및 설정을 담는 구조체
struct FSceneGlobals
{
	TArray<UDirectionalLightComponent*> DirectionalLights;
	TArray<UAmbientLightComponent*> AmbientLights;
	TArray<UHeightFogComponent*> Fogs;	// 첫 번째로 찾은 Fog를 사용함
};

/**
 * @class FSceneRenderer
 * @brief 한 프레임의 특정 뷰(View)에 대한 씬 렌더링을 총괄하는 임시(transient) 클래스.
 */
class FSceneRenderer
{
public:
	FSceneRenderer(UWorld* InWorld, FSceneView* InView, URenderer* InOwnerRenderer);
	~FSceneRenderer();

	/** @brief 이 씬 렌더러의 모든 렌더링 파이프라인을 실행합니다. */
	void Render();

private:
	// Render Path
	void RenderLitPath();
	void RenderWireframePath();
	void RenderSceneDepthPath();

	/** @brief 렌더링에 필요한 포인터들이 유효한지 확인합니다. */
	bool IsValid() const;

	/** @brief 렌더링에 필요한 뷰 행렬, 절두체 등 프레임 데이터를 준비합니다. */
	void PrepareView();

	/** @brief 월드의 모든 액터를 대상으로 절두체 컬링을 수행합니다. */
	void PerformFrustumCulling();

	/** @brief 씬을 순회하며 컬링을 통과한 모든 렌더링 대상을 수집합니다. */
	void GatherVisibleProxies();

	/** @brief 수집한 라이트 정보들로부터 상수 버퍼를 업데이트합니다.*/
	void UpdateLightConstant();

	/** @brief 타일 기반 라이트 컬링을 수행하고 Structured Buffer를 업데이트합니다. */
	void PerformTileLightCulling();

	/** @brief 불투명(Opaque) 객체들을 렌더링하는 패스입니다. */
	void RenderOpaquePass();

	void DrawMeshBatches(TArray<FMeshBatchElement>& InMeshBatches, bool bClearListAfterDraw);

	/** @brief 데칼(Decal)을 렌더링하는 패스입니다. */
	void RenderDecalPass();

	/** @brief FireBall component들을 렌더링하는 패스입니다. */
	void RenderFireBallPass();

	void RenderPostProcessingPasses();
	void RenderSceneDepthPostProcess();

	/** @brief 그리드 등 에디터 전용 객체들을 렌더링하는 패스입니다. */
	void RenderEditorPrimitivesPass();
	void RenderOverayEditorPrimitivesPass();

	/** @brief BVH 등 디버그 시각화 요소를 렌더링하는 패스입니다. */
	void RenderDebugPass();
	
	/** @brief FXAA 등 화면에서 최종 이미지 품질을 위해 적용되는 효과를 적용하는 패스입니다. */
	void ApplyScreenEffectsPass();

	void CompositeToBackBuffer();

	/** @brief 프레임 렌더링의 마무리 작업을 수행합니다. (예: 로그 출력) */
	void FinalizeFrame();
private:
	// --- 렌더링 컨텍스트 (외부에서 주입받음) ---
	UWorld* World;
	FSceneView* View;
	URenderer* OwnerRenderer;
	D3D11RHI* RHIDevice;

	// 수집된 렌더링 대상 목록
	FVisibleRenderProxySet Proxies;

	// 씬 지역 설정
	FSceneLocals SceneLocals;

	// 씬 전역 설정
	FSceneGlobals SceneGlobals;

	// 컬링을 거친 가시성 목록, NOTE: 추후 컴포넌트 단위로 수정
	TArray<UPrimitiveComponent*> PotentiallyVisibleComponents;

	// 각 패스에서 수집된 드로우 콜 정보 리스트
	TArray<FMeshBatchElement> MeshBatchElements;

	// 타일 기반 라이트 컬링 시스템
	std::unique_ptr<FTileLightCuller> TileLightCuller;
};