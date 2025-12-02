#pragma once
#include "Object.h"
#include "ClothUtil.h"

class UClothManager : public UObject
{
    DECLARE_CLASS(UClothManager, UObject)
public:
	UClothManager() = default;
	~UClothManager() override = default;

	void Init(ID3D11Device* InDevice);
	void Tick(float deltaTime);

public:
    DxContextManagerCallback* GraphicsContextManager;
	Factory* Factory;
	Solver* Solver;

	//test
	Cloth* Cloth;
	Fabric* Fabric;
	PhaseConfig* Phases;

    // 메쉬 데이터
    TArray<physx::PxVec4> Particles;  // 로컬에서 멤버 변수로 변경 필요!
    TArray<uint32_t> Indices;
    TArray<FNormalVertex> RenderVertices;

    int ClothWidth;
    int ClothHeight;

    ID3D11Buffer* VertexBuffer;
    ID3D11Buffer* IndexBuffer;

    // 셰이더 관련
    ID3D11VertexShader* VertexShader;
    ID3D11PixelShader* PixelShader;
    ID3D11InputLayout* InputLayout;

    // 렌더 스테이트
    ID3D11RasterizerState* RasterizerState;
    ID3D11DepthStencilState* DepthStencilState;
    ID3D11BlendState* BlendState;
    ID3D11SamplerState* SamplerState;
};