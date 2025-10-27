#pragma once

class UAmbientLightComponent;
class UDirectionalLightComponent;
class UPointLightComponent;
class USpotLightComponent;
class ULightComponent;
class D3D11RHI;

enum class ELightType
{
    AmbientLight,
    DirectionalLight,
    PointLight,
    SpotLight,
};

// -----------------------------------------------------------------------------
// 1. Pass 1 -> Pass 2 데이터 전달용 CPU 구조체
// -----------------------------------------------------------------------------

// FSceneRenderer(Pass 1)가 계산하여 FLightManager(Pass 2)로 전달할 섀도우 데이터. 2D / CSM 아틀라스에서 사용됩니다.
struct FShadowMapData
{
    FMatrix ShadowViewProjMatrix;
    FVector4 AtlasScaleOffset;
    int32 SampleCount;
    float Padding[3];
};

struct FShadowRenderRequest
{
    ULightComponent* LightOwner;
    FMatrix ViewMatrix;
    FMatrix ProjectionMatrix;
    uint32 Size;
    int32 SubViewIndex; // Point(0~5), CSM(0~N), Spot(0)
    FVector4 AtlasScaleOffset; // 패킹 알고리즘이 채워줄 UV
    int32 SampleCount;

    bool operator>(const FShadowRenderRequest& Other) const
    {
        return Size > Other.Size;
    }
};

// -----------------------------------------------------------------------------
// 2. Pass 2 (GPU) 셰이더용 구조체
// -----------------------------------------------------------------------------

struct FAmbientLightInfo
{
    FLinearColor Color;     // 16 bytes - Color already includes Intensity and Temperature
    // Total: 16 bytes
};

struct FDirectionalLightInfo
{
    FLinearColor Color;      // 16 bytes
    FVector Direction;       // 12 bytes
    uint32 bCastShadows;     // 4 bytes (0 or 1)

    uint32 CascadeCount;     // 4 bytes
    float Padding[3];        // 12 bytes (정렬용)

    // CSM은 데이터가 크므로 CBuffer가 아닌 별도 Structured Buffer(예: t19)로 전달하는 것이
    // 가장 이상적이지만, CBuffer에 고정 크기 배열로 담는 것도 간단한 엔진에서는 가능합니다.
    // 여기서는 최대 4개의 캐스케이드를 CBuffer에 포함하는 방식을 사용합니다.
    FShadowMapData Cascades[4]; // (64+16)*4 = 320 bytes

    // Total: 32 + 16 + 320 = 368 bytes
};

struct FPointLightInfo
{
    FLinearColor Color;      // 16 bytes
    FVector Position;        // 12 bytes
    uint32 bCastShadows;     // 4 bytes (0 or 1)

    float AttenuationRadius; // 4 bytes
    float FalloffExponent;   // 4 bytes
    uint32 bUseInverseSquareFalloff; // 4 bytes
    int32 ShadowArrayIndex;  // 4 bytes (t8 TextureCubeArray의 슬라이스 인덱스, -1=섀도우 없음)
    // Total: 48 bytes
};

struct FSpotLightInfo
{
    FLinearColor Color;      // 16 bytes
    FVector Position;        // 12 bytes
    uint32 bCastShadows;     // 4 bytes (0 or 1)

    FVector Direction;       // 12 bytes
    float InnerConeAngle;    // 4 bytes
    float OuterConeAngle;    // 4 bytes
    float AttenuationRadius; // 4 bytes

    float FalloffExponent;   // 4 bytes
    uint32 bUseInverseSquareFalloff; // 4 bytes

    // Spot Light는 1개의 2D 섀도우 뷰만 가짐
    FShadowMapData ShadowData; // 80 bytes (FMatrix(64) + FVector4(16))

    // Total: 64 + 80 = 144 bytes
};

class FLightManager
{
public:
    FLightManager() = default;
    ~FLightManager();

    void Initialize(D3D11RHI* RHIDevice);
    void Release();

    void UpdateLightBuffer(D3D11RHI* RHIDevice);
    void SetDirtyFlag();
    

    // --- 섀도우 데이터 수신 함수 (FSceneRenderer가 호출) ---
    void SetShadowMapData(ULightComponent* Light, int32 SubViewIndex, const FShadowMapData& Data);
    void SetShadowCubeMapData(ULightComponent* Light, int32 SliceIndex);

    // --- 섀도우 리소스 접근자 (FSceneRenderer가 사용) ---
    ID3D11DepthStencilView* GetShadowAtlasDSV2D() const { return ShadowAtlasDSV2D; }
    ID3D11Texture2D* GetShadowAtlasTexture2D() const { return ShadowAtlasTexture2D; }
    ID3D11ShaderResourceView* GetShadowAtlasSRV2D() const { return ShadowAtlasSRV2D; }
    float GetShadowAtlasSize2D() const { return ShadowAtlasSize2D; }
    uint32 GetShadowCubeArraySize() const { return AtlasSizeCube; }
    uint32 GetShadowCubeArrayCount() const { return CubeArrayCount; }
    ID3D11DepthStencilView* GetShadowCubeFaceDSV(UINT SliceIndex, UINT FaceIndex) const; // (구현 필요)
    bool GetCachedShadowData(ULightComponent* Light, int32 SubViewIndex, FShadowMapData& OutData) const;

    TArray<UAmbientLightComponent*> GetAmbientLightList() { return AmbientLightList; }
    TArray<UDirectionalLightComponent*> GetDirectionalLightList() { return DIrectionalLightList; }
    TArray<UPointLightComponent*> GetPointLightList() { return PointLightList; }
    TArray<USpotLightComponent*> GetSpotLightList() { return SpotLightList; }

    TArray<FPointLightInfo>& GetPointLightInfoList() { return PointLightInfoList; }
    TArray<FSpotLightInfo>& GetSpotLightInfoList() { return SpotLightInfoList; }

    template<typename T>
    void RegisterLight(T* LightComponent);
    template<typename T>
    void DeRegisterLight(T* LightComponent);
    template<typename T>
    void UpdateLight(T* LightComponent);

    void ClearAllLightList();

private:
    bool bHaveToUpdate = true;
    bool bPointLightDirty = true;
    bool bSpotLightDirty = true;
    bool bShadowDataDirty = true;

    // --- 섀도우 리소스 ---
    // Atlas 1: 2D 아틀라스 (Spot/Dir용)
    ID3D11Texture2D* ShadowAtlasTexture2D = nullptr;
    ID3D11DepthStencilView* ShadowAtlasDSV2D = nullptr;
    ID3D11ShaderResourceView* ShadowAtlasSRV2D = nullptr; // t9
    uint32 ShadowAtlasSize2D = 16384;

    // Atlas 2: 큐브맵 아틀라스 (Point Light용)
    ID3D11Texture2D* ShadowAtlasTextureCube = nullptr; // TextureCubeArray 리소스
    ID3D11ShaderResourceView* ShadowAtlasSRVCube = nullptr; // t8
    // 큐브맵의 각 면(Slice*6)에 대한 DSV 배열
    TArray<ID3D11DepthStencilView*> ShadowCubeFaceDSVs;
    uint32 AtlasSizeCube = 1024;
    uint32 CubeArrayCount = 8;

    // --- 섀도우 데이터 캐시 (CPU) ---
    // Key: 라이트, Value: 2D 섀도우 데이터 배열 (CSM의 경우 여러 개)
    TMap<ULightComponent*, TArray<FShadowMapData>> ShadowDataCache2D;
    // Key: 라이트, Value: 할당된 큐브맵 슬라이스 인덱스
    TMap<ULightComponent*, int32> ShadowDataCacheCube;


    //structured buffer
    ID3D11Buffer* PointLightBuffer = nullptr;
    ID3D11Buffer* SpotLightBuffer = nullptr;
    ID3D11ShaderResourceView* PointLightBufferSRV = nullptr;
    ID3D11ShaderResourceView* SpotLightBufferSRV = nullptr;


    TArray<UAmbientLightComponent*> AmbientLightList;
    TArray<UDirectionalLightComponent*> DIrectionalLightList;
    TArray<UPointLightComponent*> PointLightList;
    TArray<USpotLightComponent*> SpotLightList;

    // Pass 1에서 Pass 2로 데이터를 넘기기 위한 임시 저장소
    // 키: ULightComponent 포인터, 값: 해당 라이트의 섀도우 데이터
    TMap<ULightComponent*, FShadowMapData> ShadowDataCache;

    //업데이트 시에만 clear하고 다시 수집할 실제 데이터 리스트
    TArray<FPointLightInfo> PointLightInfoList;
    TArray<FSpotLightInfo> SpotLightInfoList;

    //이미 레지스터된 라이트인지 확인하는 용도
    TSet<ULightComponent*> LightComponentList;
    uint32 PointLightNum = 0;
    uint32 SpotLightNum = 0;
};

template<> void FLightManager::RegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::RegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent);

template<> void FLightManager::DeRegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::DeRegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent);

template<> void FLightManager::UpdateLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<UPointLightComponent>(UPointLightComponent* LightComponent);
template<> void FLightManager::UpdateLight<USpotLightComponent>(USpotLightComponent* LightComponent);
