#pragma once
#include "RHIDevice.h"
#include "LineDynamicMesh.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMeshComponent;
class URHIDevice;
class UShader;
class UStaticMesh;
class UBillboardComponent;
class UPrimitiveComponent;
class UCameraComponent;
class FSceneView;

struct FMaterialSlot;
struct FLinearColor;

class URenderer
{
public:
	URenderer(D3D11RHI* InDevice);

	~URenderer();

public:
	void RenderSceneForView(UWorld* InWorld, FSceneView* InView, FViewport* InViewport);

	void BeginFrame();
	void EndFrame();

	// Viewport size for current draw context (used by overlay/gizmo scaling)
	void SetCurrentViewportSize(uint32 InWidth, uint32 InHeight) { CurrentViewportWidth = InWidth; CurrentViewportHeight = InHeight; }
	uint32 GetCurrentViewportWidth() const { return CurrentViewportWidth; }
	uint32 GetCurrentViewportHeight() const { return CurrentViewportHeight; }
	UPrimitiveComponent* GetPrimitiveCollided(int MouseX, int MouseY) const;

	// Batch Line Rendering System
	void BeginLineBatch();
	void AddLine(const FVector& Start, const FVector& End, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	void AddLines(const TArray<FVector>& Lines, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	void AddLinesRange(const TArray<FVector>& Lines,int startIdx, int Count, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
	void AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors);
	void EndLineBatch(const FMatrix& ModelMatrix);
	void EndLineBatchAlwaysOnTop(const FMatrix& ModelMatrix);
	void ClearLineBatch();

	// Batch Triangle Rendering System (for debug meshes like constraint cones)
	void BeginTriangleBatch();
	void AddTriangle(const FVector& V0, const FVector& V1, const FVector& V2, const FVector4& Color);
	void AddTriangles(const TArray<FVector>& Vertices, const TArray<uint32>& Indices, const FVector4& Color);
	void AddTriangles(const TArray<FVector>& Vertices, const TArray<uint32>& Indices, const TArray<FVector4>& Colors);
	void EndTriangleBatch(const FMatrix& ModelMatrix);
	void EndTriangleBatchAlwaysOnTop(const FMatrix& ModelMatrix);
	void ClearTriangleBatch();

	D3D11RHI* GetRHIDevice() { return RHIDevice; }

	void SetCurrentCamera(ACameraActor* InCamera) { CurrentCamera = InCamera; }
	ACameraActor* GetCurrentCamera() const { return CurrentCamera; }

	// ===== Debug Primitive Rendering System =====
	// 반투명 프리미티브 렌더링 (Physics Body 시각화용)
	// Transform: Scale * Rotation * Translation 매트릭스
	void BeginDebugPrimitiveBatch();
	void DrawDebugSphere(const FMatrix& Transform, const FLinearColor& Color, uint32 UUID = 0);
	void DrawDebugBox(const FMatrix& Transform, const FLinearColor& Color, uint32 UUID = 0);
	void DrawDebugCapsule(const FMatrix& Transform, float Radius, float HalfHeight, const FLinearColor& Color, uint32 UUID = 0);
	void DrawDebugCone(const FMatrix& Transform, float Swing1Angle, float Swing2Angle, float Height, const FLinearColor& Color, uint32 UUID = 0);
	void DrawDebugArc(const FMatrix& Transform, float TwistAngle, float Radius, const FLinearColor& Color, uint32 UUID = 0);
	void DrawDebugArrow(const FMatrix& Transform, float Length, float HeadSize, const FLinearColor& Color, uint32 UUID = 0);
	void EndDebugPrimitiveBatch();

	// Deferred buffer release system (GPU-safe resource management)
	void DeferredReleaseBuffer(ID3D11Buffer* Buffer);

	// ===== Highlight System (아이템 하이라이트용) =====
	/** 오브젝트에 하이라이트 추가 (ObjectID 기반) */
	void AddHighlight(uint32 ObjectID, const FLinearColor& OutlineColor = FLinearColor(1.0f, 0.8f, 0.2f, 1.0f));
	/** 오브젝트의 하이라이트 제거 */
	void RemoveHighlight(uint32 ObjectID);
	/** 모든 하이라이트 제거 */
	void ClearHighlights();
	/** 현재 하이라이트된 오브젝트 목록 반환 */
	const TMap<uint32, FLinearColor>& GetHighlightedObjects() const { return HighlightedObjects; }
	/** 하이라이트가 있는지 확인 */
	bool HasHighlights() const { return !HighlightedObjects.empty(); }

private:
	// Deferred release structure
	struct FDeferredRelease
	{
		ID3D11Buffer* Buffer;
		int FramesToWait;

		FDeferredRelease(ID3D11Buffer* InBuffer, int InFrames)
			: Buffer(InBuffer), FramesToWait(InFrames) {}
	};

	TArray<FDeferredRelease> DeferredReleaseQueue;
	void ProcessDeferredReleases();
	D3D11RHI* RHIDevice;    // NOTE: 개발 편의성을 위해서 DX11를 종속적으로 사용한다 (URHIDevice를 사용하지 않음)

	// Current viewport size (per FViewport draw); 0 if unset

	uint32 CurrentViewportWidth = 0;
	uint32 CurrentViewportHeight = 0;

	// Batch Line Rendering System using UDynamicMesh for efficiency
	ULineDynamicMesh* DynamicLineMesh = nullptr;
	FMeshData* LineBatchData = nullptr;
	UShader* LineShader = nullptr;
	bool bLineBatchActive = false;
	static const uint32 MAX_LINES = 200000;  // Maximum lines per batch (safety headroom)

	void InitializeLineBatch();
	void InitializeTriangleBatch();

	// Debug Primitive Batch System
	void InitializeDebugPrimitiveBatch();
	void DrawPrimitiveMesh(UStaticMesh* Mesh, const FMatrix& Transform, const FLinearColor& Color, uint32 UUID);
	UShader* DebugPrimitiveShader = nullptr;
	bool bDebugPrimitiveBatchActive = false;

	// Triangle batch data
	ULineDynamicMesh* DynamicTriangleMesh = nullptr;
	FMeshData* TriangleBatchData = nullptr;
	TArray<FVertexSimple> TriangleInterleavedVertices;  // GPU 업로드용 인터리브 캐시
	bool bTriangleBatchActive = false;
	static const uint32 MAX_TRIANGLES = 100000;  // Maximum triangles per batch

	// 이전 drawCall에서 이미 썼던 RnderState면, 다시 Set 하지 않기 위해 만든 변수들
	EViewMode PreViewModeIndex = EViewMode::VMI_Wireframe; // RSSetState, UpdateColorConstantBuffers
	//UMaterial* PreUMaterial = nullptr; // SRV, UpdatePixelConstantBuffers
	//UStaticMesh* PreStaticMesh = nullptr; // VertexBuffer, IndexBuffer
	/*ID3D11Buffer* PreVertexBuffer = nullptr;
	ID3D11ShaderResourceView* PreSRV = nullptr;*/

	ACameraActor* CurrentCamera = nullptr;

	// Highlight system (ObjectID -> OutlineColor)
	TMap<uint32, FLinearColor> HighlightedObjects;
};

