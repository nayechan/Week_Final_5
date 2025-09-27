#pragma once
#include "RHIDevice.h"
#include "LineDynamicMesh.h"

class UStaticMeshComponent;
class UTextRenderComponent;
class UMeshComponent;
class URHIDevice;
class UShader;
class UStaticMesh;
struct FMaterialSlot;

class URenderer
{
public:
    URenderer(URHIDevice* InDevice);

    ~URenderer();

public:
	void BeginFrame();

    void PrepareShader(FShader& InShader);

    void PrepareShader(UShader* InShader);

    void OMSetBlendState(bool bIsChecked);

    void RSSetState(EViewModeIndex ViewModeIndex);

    void UpdateConstantBuffer(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix);

    void UpdateHighLightConstantBuffer(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo);

    void UpdateBillboardConstantBuffers(const FVector& pos, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix, const FVector& CameraRight, const FVector& CameraUp);

    void UpdatePixelConstantBuffers(const FObjMaterialInfo& InMaterialInfo, bool bHasMaterial, bool bHasTexture);

    void UpdateColorBuffer(const FVector4& Color);

    void DrawIndexedPrimitiveComponent(UStaticMesh* InMesh, D3D11_PRIMITIVE_TOPOLOGY InTopology, const TArray<FMaterialSlot>& InComponentMaterialSlots);

    void UpdateUVScroll(const FVector2D& Speed, float TimeSec);

    void DrawIndexedPrimitiveComponent(UTextRenderComponent* Comp, D3D11_PRIMITIVE_TOPOLOGY InTopology);

    void SetViewModeType(EViewModeIndex ViewModeIndex);
    // Batch Line Rendering System
    void BeginLineBatch();
    void AddLine(const FVector& Start, const FVector& End, const FVector4& Color = FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    void AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors);
    void EndLineBatch(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix);
    void ClearLineBatch();

	void EndFrame();

    void OMSetDepthStencilState(EComparisonFunc Func);

    URHIDevice* GetRHIDevice() { return RHIDevice; }


	// ========================= 오클루전 관련 메서드들 ===========================
    /*
    // Occlusion helpers
    void BeginDepthOnly();
    void EndDepthOnly();
    void SetPredication(ID3D11Predicate* Pred, BOOL OpEqualTrue);

    // AABB를 박스로 깊이만 렌더
    void DrawOcclusionBox(const FBound& B, const FMatrix& View, const FMatrix& Proj);

    // 깊이 전용 셰이더 주입(엔진 셰이더 매니저 사용 시)
    void SetDepthOnlyShaders(ID3D11VertexShader* VS, ID3D11PixelShader* PS);

    ID3D11Device* GetDevice();
    ID3D11DeviceContext* GetDeviceContext();
    void SetDepthOnlyInputLayout(ID3D11InputLayout* IL);
    */
    // ===========================================================================
private:
	URHIDevice* RHIDevice;

    // Batch Line Rendering System using UDynamicMesh for efficiency
    ULineDynamicMesh* DynamicLineMesh = nullptr;
    FMeshData* LineBatchData = nullptr;
    UShader* LineShader = nullptr;
    bool bLineBatchActive = false;
    static const uint32 MAX_LINES = 200000;  // Maximum lines per batch (safety headroom)

    void InitializeLineBatch();


    // ========================= 오클루전 관련 멤버들 ===========================
    // Depth-only states
    /*
    ID3D11DepthStencilState* DepthLEqual = nullptr;
    ID3D11RasterizerState* RS_Solid = nullptr;
    ID3D11BlendState* ColorMaskOff = nullptr;

    // Unit cube for occlusion draw
    ID3D11Buffer* UnitCubeVB = nullptr;
    ID3D11Buffer* UnitCubeIB = nullptr;

    // Per-object CB (gModel/gView/gProj)
    ID3D11Buffer* OcclusionCB = nullptr;

    // Depth-only VS/PS
    ID3D11VertexShader* DepthOnlyVS = nullptr;
    ID3D11PixelShader* DepthOnlyPS = nullptr;

    void CreateDepthOnlyStates();
    void CreateUnitCube();
    void CreateOcclusionCB();
    ID3D11DepthStencilState* DepthLEqualNoWrite = nullptr;
    ID3D11InputLayout* DepthOnlyIL = nullptr;
    */
	// ===========================================================================
};

