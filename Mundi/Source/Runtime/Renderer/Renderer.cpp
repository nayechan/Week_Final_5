#include "pch.h"
#include "TextRenderComponent.h"
#include "Shader.h"
#include "StaticMesh.h"
#include "Quad.h"
#include "StaticMeshComponent.h"
#include "BillboardComponent.h"
#include "FViewportClient.h"
#include "FViewport.h"
#include "World.h"
#include "RenderManager.h"
#include "WorldPartitionManager.h"
#include "Renderer.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "PrimitiveComponent.h"
#include "StaticMeshActor.h"
#include "Gizmo/GizmoActor.h"
#include "Grid/GridActor.h"
#include "Octree.h"
#include "BVHierarchy.h"
#include "Occlusion.h"
#include "Frustum.h"
#include "ResourceManager.h"
#include "RHIDevice.h"
#include "Material.h"
#include "Texture.h"
#include "RenderSettings.h"
#include "EditorEngine.h"
#include "DecalComponent.h"
#include "DecalStatManager.h"
#include "SceneRenderer.h"
#include "SceneView.h"
#include "SkinningStats.h"
#include "PlatformTime.h"

#include <Windows.h>
#include "DirectionalLightComponent.h"
URenderer::URenderer(D3D11RHI* InDevice) : RHIDevice(InDevice)
{
	InitializeLineBatch();
	InitializeTriangleBatch();
	InitializeDebugPrimitiveBatch();

	// GPU 타이머 초기화 (스키닝 성능 측정용)
	FSkinningStatManager::GetInstance().InitializeGPUTimer(RHIDevice->GetDevice());
}

URenderer::~URenderer()
{
	if (LineBatchData)
	{
		delete LineBatchData;
	}

	if (TriangleBatchData)
	{
		delete TriangleBatchData;
	}

	// 지연 해제 큐에 남아있는 모든 버퍼 해제
	for (FDeferredRelease& Entry : DeferredReleaseQueue)
	{
		if (Entry.Buffer)
		{
			Entry.Buffer->Release();
		}
	}
	DeferredReleaseQueue.Empty();
}

void URenderer::BeginFrame()
{
	RHIDevice->IASetPrimitiveTopology();

	RHIDevice->OMSetRenderTargets(ERTVMode::BackBufferWithDepth);

	// 지연 해제 큐 처리 (GPU 안전성 확보)
	ProcessDeferredReleases();

	// 프레임별 통계 초기화 (데칼, 스키닝)
	FDecalStatManager::GetInstance().ResetFrameStats();

	// 이전 프레임의 GPU draw 시간 가져오기 (비동기, N-7 프레임 결과)
	double LastGPUDrawTimeMS = FSkinningStatManager::GetInstance().GetGPUDrawTimeMS(RHIDevice->GetDeviceContext());

	// TimeProfile 시스템에 GPU Draw Time 추가 (프로파일링 통합)
	if (LastGPUDrawTimeMS >= 0.0)
	{
		FScopeCycleCounter::AddTimeProfile(TStatId("GPUDrawTime"), LastGPUDrawTimeMS);
	}

	// 프레임 단위 스키닝 통계 리셋
	FSkinningStatManager::GetInstance().ResetFrameStats();

	// GPU draw 시간을 통계에 추가
	// CPU 모드: CPU 본 계산 + 버텍스 스키닝 + 버퍼 업로드 + GPU draw
	// GPU 모드: CPU 본 계산 + 본 버퍼 업로드 + GPU draw(셰이더 스키닝 포함)
	FSkinningStatManager::GetInstance().AddDrawTime(LastGPUDrawTimeMS);

	// GPU 타이머 시작 - 전체 프레임의 Draw Time 측정 (모든 뷰어 포함)
	FSkinningStatManager::GetInstance().BeginGPUTimer(RHIDevice->GetDeviceContext());

	RHIDevice->ClearAllBuffer();
}

void URenderer::EndFrame()
{
	// GPU 타이머 종료 - 전체 프레임의 Draw Time 측정 완료
	FSkinningStatManager::GetInstance().EndGPUTimer(RHIDevice->GetDeviceContext());

	RHIDevice->Present();
}

void URenderer::RenderSceneForView(UWorld* World, FSceneView* View, FViewport* Viewport)
{
	// 씬을 그리는 FSceneRenderer 를 생성합니다.
	FSceneRenderer SceneRenderer(World, View, this);

	// 실제로 렌더를 수행합니다.
	SceneRenderer.Render();
}

UPrimitiveComponent* URenderer::GetPrimitiveCollided(int MouseX, int MouseY) const
{
	//GPU와 동기화 문제 때문에 Map이 호출될때까지 기다려야해서 피킹 하는 프레임에 엄청난 프레임 드랍이 일어남.
   //******비동기 방식으로 무조건 바꿔야함****************
	uint32 PickedId = 0;

	ID3D11DeviceContext* DeviceContext = RHIDevice->GetDeviceContext();
	//스테이징 버퍼를 가져와야 하는데 이걸 Device 추상 클래스가 Getter로 가지고 있는게 좋은 설계가 아닌 것 같아서 일단 캐스팅함


	D3D11_BOX Box{};
	Box.left = MouseX;
	Box.right = MouseX + 1;
	Box.top = MouseY;
	Box.bottom = MouseY + 1;
	Box.front = 0;
	Box.back = 1;

	DeviceContext->CopySubresourceRegion(
		RHIDevice->GetIdStagingBuffer(),
		0,
		0, 0, 0,
		RHIDevice->GetIdBuffer(),
		0,
		&Box);
	D3D11_MAPPED_SUBRESOURCE MapResource{};
	if (SUCCEEDED(DeviceContext->Map(RHIDevice->GetIdStagingBuffer(), 0, D3D11_MAP_READ, 0, &MapResource)))
	{
		PickedId = *static_cast<uint32*>(MapResource.pData);
		DeviceContext->Unmap(RHIDevice->GetIdStagingBuffer(), 0);
	}

	if (PickedId == 0)
		return nullptr;
	auto it = GUObjectArray.find(PickedId);
	if (it == GUObjectArray.end())
		return nullptr;
	return Cast<UPrimitiveComponent>(it->second);
}

void URenderer::InitializeLineBatch()
{
	// Create UDynamicMesh for efficient line batching
	DynamicLineMesh = UResourceManager::GetInstance().Load<ULineDynamicMesh>("Line");

	// Initialize with maximum capacity (MAX_LINES * 2 vertices, MAX_LINES * 2 indices)
	uint32 maxVertices = MAX_LINES * 2;
	uint32 maxIndices = MAX_LINES * 2;
	DynamicLineMesh->Load(maxVertices, maxIndices, RHIDevice->GetDevice());

	// Create FMeshData for accumulating line data
	LineBatchData = new FMeshData();

	// Load line shader
	LineShader = UResourceManager::GetInstance().Load<UShader>("Shaders/UI/ShaderLine.hlsl");
}

void URenderer::BeginLineBatch()
{
	if (!LineBatchData) return;

	bLineBatchActive = true;

	// Clear previous batch data
	LineBatchData->Vertices.clear();
	LineBatchData->Color.clear();
	LineBatchData->Indices.clear();
}

void URenderer::AddLine(const FVector& Start, const FVector& End, const FVector4& Color)
{
	if (!bLineBatchActive || !LineBatchData) return;

	uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());

	// Add vertices
	LineBatchData->Vertices.push_back(Start);
	LineBatchData->Vertices.push_back(End);

	// Add colors
	LineBatchData->Color.push_back(Color);
	LineBatchData->Color.push_back(Color);

	// Add indices for line (2 vertices per line)
	LineBatchData->Indices.push_back(startIndex);
	LineBatchData->Indices.push_back(startIndex + 1);
}
void URenderer::AddLines(const TArray<FVector>& Lines, const FVector4& Color)
{
	if (!bLineBatchActive || !LineBatchData) return;
	size_t VerticesCount = Lines.size();
	if (VerticesCount % 2 != 0)
	{
		return;
	}

	uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());
	// Reserve space for efficiency

	LineBatchData->Vertices.reserve(LineBatchData->Vertices.size() + VerticesCount);
	LineBatchData->Color.reserve(LineBatchData->Color.size() + VerticesCount);
	LineBatchData->Indices.reserve(LineBatchData->Indices.size() + VerticesCount);

	// Add all lines at once
	for (size_t i = 0; i < VerticesCount; ++i)
	{
		// Add vertices
		LineBatchData->Vertices.push_back(Lines[i]);

		// Add colors
		LineBatchData->Color.push_back(Color);

		// Add indices for line (2 vertices per line)
		LineBatchData->Indices.push_back(startIndex + i);
	}
}
void URenderer::AddLinesRange(const TArray<FVector>& Lines, int startIdx, int Count, const FVector4& Color)
{
	if (!bLineBatchActive || !LineBatchData) return;
	size_t VerticesCount = Lines.size();
	if (VerticesCount % 2 != 0 || Count % 2 != 0)
	{
		return;
	}

	uint32 AddVerticesCount = Count;
	uint32 LineBatchStartIndex = static_cast<uint32>(LineBatchData->Vertices.size());
	// Reserve space for efficiency

	LineBatchData->Vertices.reserve(LineBatchData->Vertices.size() + AddVerticesCount);
	LineBatchData->Color.reserve(LineBatchData->Color.size() + AddVerticesCount);
	LineBatchData->Indices.reserve(LineBatchData->Indices.size() + AddVerticesCount);

	// Add all lines at once
	for (size_t i = 0; i < AddVerticesCount; ++i)
	{
		// Add vertices
		LineBatchData->Vertices.push_back(Lines[i + startIdx]);

		// Add colors
		LineBatchData->Color.push_back(Color);

		// Add indices for line (2 vertices per line)
		LineBatchData->Indices.push_back(LineBatchStartIndex + i);
	}
}

void URenderer::AddLines(const TArray<FVector>& StartPoints, const TArray<FVector>& EndPoints, const TArray<FVector4>& Colors)
{
	if (!bLineBatchActive || !LineBatchData) return;

	// Validate input arrays have same size
	if (StartPoints.size() != EndPoints.size() || StartPoints.size() != Colors.size())
		return;

	uint32 startIndex = static_cast<uint32>(LineBatchData->Vertices.size());

	// Reserve space for efficiency
	size_t lineCount = StartPoints.size();
	LineBatchData->Vertices.reserve(LineBatchData->Vertices.size() + lineCount * 2);
	LineBatchData->Color.reserve(LineBatchData->Color.size() + lineCount * 2);
	LineBatchData->Indices.reserve(LineBatchData->Indices.size() + lineCount * 2);

	// Add all lines at once
	for (size_t i = 0; i < lineCount; ++i)
	{
		uint32 currentIndex = startIndex + static_cast<uint32>(i * 2);

		// Add vertices
		LineBatchData->Vertices.push_back(StartPoints[i]);
		LineBatchData->Vertices.push_back(EndPoints[i]);

		// Add colors
		LineBatchData->Color.push_back(Colors[i]);
		LineBatchData->Color.push_back(Colors[i]);

		// Add indices for line (2 vertices per line)
		LineBatchData->Indices.push_back(currentIndex);
		LineBatchData->Indices.push_back(currentIndex + 1);
	}
}

void URenderer::EndLineBatch(const FMatrix& ModelMatrix)
{
	if (!bLineBatchActive || !LineBatchData || !DynamicLineMesh || LineBatchData->Vertices.empty())
	{
		bLineBatchActive = false;
		return;
	}

	// Clamp to GPU buffer capacity to avoid full drop when overflowing
	const uint32 totalLines = static_cast<uint32>(LineBatchData->Indices.size() / 2);
	if (totalLines > MAX_LINES)
	{
		const uint32 clampedLines = MAX_LINES;
		const uint32 clampedVerts = clampedLines * 2;
		const uint32 clampedIndices = clampedLines * 2;
		LineBatchData->Vertices.resize(clampedVerts);
		LineBatchData->Color.resize(clampedVerts);
		LineBatchData->Indices.resize(clampedIndices);
	}

	// Efficiently update dynamic mesh data (no buffer recreation!)
	if (!DynamicLineMesh->UpdateData(LineBatchData, RHIDevice->GetDeviceContext()))
	{
		bLineBatchActive = false;
		return;
	}

	// Set up rendering state
	FMatrix ModelInvTranspose = ModelMatrix.InverseAffine().Transpose();
	RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(ModelMatrix, ModelInvTranspose));
	RHIDevice->PrepareShader(LineShader);

	// Render using dynamic mesh
	if (DynamicLineMesh->GetCurrentVertexCount() > 0 && DynamicLineMesh->GetCurrentIndexCount() > 0)
	{
		UINT stride = sizeof(FVertexSimple);
		UINT offset = 0;

		ID3D11Buffer* vertexBuffer = DynamicLineMesh->GetVertexBuffer();
		ID3D11Buffer* indexBuffer = DynamicLineMesh->GetIndexBuffer();

		RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
		RHIDevice->GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		// Overlay 스텐실(=1) 영역은 그리지 않도록 스텐실 테스트 설정
		RHIDevice->OMSetDepthStencilState_StencilRejectOverlay();
		RHIDevice->GetDeviceContext()->DrawIndexed(DynamicLineMesh->GetCurrentIndexCount(), 0, 0);
		// 상태 복구
		RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
	}

	bLineBatchActive = false;
}

void URenderer::EndLineBatchAlwaysOnTop(const FMatrix& ModelMatrix)
{
    if (!bLineBatchActive || !LineBatchData || !DynamicLineMesh || LineBatchData->Vertices.empty())
    {
        bLineBatchActive = false;
        return;
    }

    const uint32 totalLines = static_cast<uint32>(LineBatchData->Indices.size() / 2);
    if (totalLines > MAX_LINES)
    {
        const uint32 clampedLines = MAX_LINES;
        const uint32 clampedVerts = clampedLines * 2;
        const uint32 clampedIndices = clampedLines * 2;
        LineBatchData->Vertices.resize(clampedVerts);
        LineBatchData->Color.resize(clampedVerts);
        LineBatchData->Indices.resize(clampedIndices);
    }

    if (!DynamicLineMesh->UpdateData(LineBatchData, RHIDevice->GetDeviceContext()))
    {
        bLineBatchActive = false;
        return;
    }

    FMatrix ModelInvTranspose = ModelMatrix.InverseAffine().Transpose();
    RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(ModelMatrix, ModelInvTranspose));
    RHIDevice->PrepareShader(LineShader);

    if (DynamicLineMesh->GetCurrentVertexCount() > 0 && DynamicLineMesh->GetCurrentIndexCount() > 0)
    {
        UINT stride = sizeof(FVertexSimple);
        UINT offset = 0;
        ID3D11Buffer* vertexBuffer = DynamicLineMesh->GetVertexBuffer();
        ID3D11Buffer* indexBuffer = DynamicLineMesh->GetIndexBuffer();

        RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        RHIDevice->GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
        RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

        // Disable depth test so lines render on top
        RHIDevice->OMSetDepthStencilState(EComparisonFunc::Disable);
        RHIDevice->OMSetBlendState(true);
        RHIDevice->GetDeviceContext()->DrawIndexed(DynamicLineMesh->GetCurrentIndexCount(), 0, 0);
        // Restore state
        RHIDevice->OMSetBlendState(false);
        RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
    }

    bLineBatchActive = false;
}

void URenderer::ClearLineBatch()
{
	if (!LineBatchData) return;

	LineBatchData->Vertices.clear();
	LineBatchData->Color.clear();
	LineBatchData->Indices.clear();

	bLineBatchActive = false;
}

void URenderer::DeferredReleaseBuffer(ID3D11Buffer* Buffer)
{
	if (!Buffer)
	{
		return;
	}

	// GPU 타이머 링버퍼 크기(8)와 동일하게 8프레임 대기
	// N-7 프레임의 쿼리 결과를 읽으므로, 8프레임 후면 GPU 작업 완료 보장
	constexpr int FRAMES_TO_WAIT = 8;

	DeferredReleaseQueue.Add(FDeferredRelease(Buffer, FRAMES_TO_WAIT));
}

void URenderer::ProcessDeferredReleases()
{
	// 역순으로 순회하며 제거 (인덱스 안정성)
	for (int32 i = DeferredReleaseQueue.Num() - 1; i >= 0; --i)
	{
		FDeferredRelease& Entry = DeferredReleaseQueue[i];
		Entry.FramesToWait--;

		if (Entry.FramesToWait <= 0)
		{
			// GPU 작업이 완료되었으므로 안전하게 해제
			if (Entry.Buffer)
			{
				Entry.Buffer->Release();
			}
			DeferredReleaseQueue.RemoveAt(i);
		}
	}
}

// =============================================
// Triangle Batch Rendering System
// =============================================

void URenderer::InitializeTriangleBatch()
{
	// Create UDynamicMesh for efficient triangle batching
	DynamicTriangleMesh = UResourceManager::GetInstance().Load<ULineDynamicMesh>("Triangle");

	// Initialize with maximum capacity (MAX_TRIANGLES * 3 vertices, MAX_TRIANGLES * 3 indices)
	uint32 maxVertices = MAX_TRIANGLES * 3;
	uint32 maxIndices = MAX_TRIANGLES * 3;
	DynamicTriangleMesh->Load(maxVertices, maxIndices, RHIDevice->GetDevice());

	// Create FMeshData for accumulating triangle data
	TriangleBatchData = new FMeshData();
}

void URenderer::BeginTriangleBatch()
{
	if (!TriangleBatchData) return;

	bTriangleBatchActive = true;

	// Clear previous batch data
	TriangleBatchData->Indices.clear();
	TriangleInterleavedVertices.clear();
}

void URenderer::AddTriangle(const FVector& V0, const FVector& V1, const FVector& V2, const FVector4& Color)
{
	if (!bTriangleBatchActive || !TriangleBatchData) return;

	uint32 startIndex = static_cast<uint32>(TriangleBatchData->Vertices.size());

	// Add vertices
	TriangleBatchData->Vertices.push_back(V0);
	TriangleBatchData->Vertices.push_back(V1);
	TriangleBatchData->Vertices.push_back(V2);

	// Add colors
	TriangleBatchData->Color.push_back(Color);
	TriangleBatchData->Color.push_back(Color);
	TriangleBatchData->Color.push_back(Color);

	// Add indices for triangle (3 vertices per triangle)
	TriangleBatchData->Indices.push_back(startIndex);
	TriangleBatchData->Indices.push_back(startIndex + 1);
	TriangleBatchData->Indices.push_back(startIndex + 2);
}

void URenderer::AddTriangles(const TArray<FVector>& Vertices, const TArray<uint32>& Indices, const FVector4& Color)
{
	if (!bTriangleBatchActive || !TriangleBatchData) return;
	if (Indices.size() % 3 != 0) return;

	uint32 startIndex = static_cast<uint32>(TriangleBatchData->Vertices.size());

	// Reserve space for efficiency
	TriangleBatchData->Vertices.reserve(TriangleBatchData->Vertices.size() + Vertices.size());
	TriangleBatchData->Color.reserve(TriangleBatchData->Color.size() + Vertices.size());
	TriangleBatchData->Indices.reserve(TriangleBatchData->Indices.size() + Indices.size());

	// Add all vertices
	for (const FVector& V : Vertices)
	{
		TriangleBatchData->Vertices.push_back(V);
		TriangleBatchData->Color.push_back(Color);
	}

	// Add indices (offset by startIndex)
	for (uint32 Idx : Indices)
	{
		TriangleBatchData->Indices.push_back(startIndex + Idx);
	}
}

void URenderer::AddTriangles(const TArray<FVector>& Vertices, const TArray<uint32>& Indices, const TArray<FVector4>& Colors)
{
	if (!bTriangleBatchActive || !TriangleBatchData) return;
	if (Indices.size() % 3 != 0)
	{
		UE_LOG("[AddTriangles] FAIL: Indices %% 3 != 0 (%zu)", Indices.size());
		return;
	}
	if (Vertices.size() != Colors.size())
	{
		UE_LOG("[AddTriangles] FAIL: Vertices(%zu) != Colors(%zu)", Vertices.size(), Colors.size());
		return;
	}

	uint32 startIndex = static_cast<uint32>(TriangleInterleavedVertices.size());

	// 인터리브된 버텍스 배열에 직접 추가 (GPU 업로드 시 memcpy 가능)
	size_t prevSize = TriangleInterleavedVertices.size();
	TriangleInterleavedVertices.resize(prevSize + Vertices.size());
	for (size_t i = 0; i < Vertices.size(); ++i)
	{
		TriangleInterleavedVertices[prevSize + i].Position = Vertices[i];
		TriangleInterleavedVertices[prevSize + i].Color = Colors[i];
	}

	// 인덱스는 offset을 적용해야 하므로 reserve 후 추가
	TriangleBatchData->Indices.reserve(TriangleBatchData->Indices.size() + Indices.size());
	for (uint32 Idx : Indices)
	{
		TriangleBatchData->Indices.push_back(startIndex + Idx);
	}

}

void URenderer::EndTriangleBatch(const FMatrix& ModelMatrix)
{
	if (!bTriangleBatchActive || !TriangleBatchData || !DynamicTriangleMesh || TriangleInterleavedVertices.empty())
	{
		bTriangleBatchActive = false;
		return;
	}

	// Clamp to GPU buffer capacity
	const uint32 totalTriangles = static_cast<uint32>(TriangleBatchData->Indices.size() / 3);
	if (totalTriangles > MAX_TRIANGLES)
	{
		const uint32 clampedVerts = MAX_TRIANGLES * 3;
		const uint32 clampedIndices = MAX_TRIANGLES * 3;
		TriangleInterleavedVertices.resize(clampedVerts);
		TriangleBatchData->Indices.resize(clampedIndices);
	}

	// 인터리브된 배열로 GPU 업로드 (memcpy 사용 - 고속)
	if (!DynamicTriangleMesh->UpdateDataInterleaved(TriangleInterleavedVertices, TriangleBatchData->Indices, RHIDevice->GetDeviceContext()))
	{
		bTriangleBatchActive = false;
		return;
	}

	// Set up rendering state
	FMatrix ModelInvTranspose = ModelMatrix.InverseAffine().Transpose();
	RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(ModelMatrix, ModelInvTranspose));
	RHIDevice->PrepareShader(LineShader);

	// Render using dynamic mesh
	if (DynamicTriangleMesh->GetCurrentVertexCount() > 0 && DynamicTriangleMesh->GetCurrentIndexCount() > 0)
	{
		UINT stride = sizeof(FVertexSimple);
		UINT offset = 0;

		ID3D11Buffer* vertexBuffer = DynamicTriangleMesh->GetVertexBuffer();
		ID3D11Buffer* indexBuffer = DynamicTriangleMesh->GetIndexBuffer();

		RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
		RHIDevice->GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Disable backface culling for debug triangles (양면 렌더링)
		RHIDevice->RSSetState(ERasterizerMode::Solid_NoCull);

		// Enable blending for semi-transparent triangles
		RHIDevice->OMSetBlendState(true);
		// 깊이 테스트는 하지만 쓰지 않음 (반투명 메시 중첩 방지)
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqualReadOnly);
		RHIDevice->GetDeviceContext()->DrawIndexed(DynamicTriangleMesh->GetCurrentIndexCount(), 0, 0);

		// Restore state
		RHIDevice->OMSetBlendState(false);
		RHIDevice->RSSetState(ERasterizerMode::Solid);  // Restore culling
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);  // Restore depth write
	}

	bTriangleBatchActive = false;
}

void URenderer::EndTriangleBatchAlwaysOnTop(const FMatrix& ModelMatrix)
{
	if (!bTriangleBatchActive || !TriangleBatchData || !DynamicTriangleMesh || TriangleInterleavedVertices.empty())
	{
		bTriangleBatchActive = false;
		return;
	}

	// Clamp to GPU buffer capacity
	const uint32 totalTriangles = static_cast<uint32>(TriangleBatchData->Indices.size() / 3);
	if (totalTriangles > MAX_TRIANGLES)
	{
		const uint32 clampedVerts = MAX_TRIANGLES * 3;
		const uint32 clampedIndices = MAX_TRIANGLES * 3;
		TriangleInterleavedVertices.resize(clampedVerts);
		TriangleBatchData->Indices.resize(clampedIndices);
	}

	// 인터리브된 배열로 GPU 업로드 (memcpy 사용 - 고속)
	if (!DynamicTriangleMesh->UpdateDataInterleaved(TriangleInterleavedVertices, TriangleBatchData->Indices, RHIDevice->GetDeviceContext()))
	{
		bTriangleBatchActive = false;
		return;
	}

	// Set up rendering state
	FMatrix ModelInvTranspose = ModelMatrix.InverseAffine().Transpose();
	RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(ModelMatrix, ModelInvTranspose));
	RHIDevice->PrepareShader(LineShader);

	// Render using dynamic mesh
	if (DynamicTriangleMesh->GetCurrentVertexCount() > 0 && DynamicTriangleMesh->GetCurrentIndexCount() > 0)
	{
		UINT stride = sizeof(FVertexSimple);
		UINT offset = 0;

		ID3D11Buffer* vertexBuffer = DynamicTriangleMesh->GetVertexBuffer();
		ID3D11Buffer* indexBuffer = DynamicTriangleMesh->GetIndexBuffer();

		RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
		RHIDevice->GetDeviceContext()->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
		RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// 양면 렌더링 + 깊이 테스트 비활성화
		RHIDevice->RSSetState(ERasterizerMode::Solid_NoCull);
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::Disable);
		RHIDevice->OMSetBlendState(true);
		RHIDevice->GetDeviceContext()->DrawIndexed(DynamicTriangleMesh->GetCurrentIndexCount(), 0, 0);

		// Restore state
		RHIDevice->RSSetState(ERasterizerMode::Solid);
		RHIDevice->OMSetBlendState(false);
		RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
	}
	bTriangleBatchActive = false;
}

void URenderer::ClearTriangleBatch()
{
	if (!TriangleBatchData) return;

	TriangleBatchData->Vertices.clear();
	TriangleBatchData->Color.clear();
	TriangleBatchData->Indices.clear();

	bTriangleBatchActive = false;
}

// ===== Debug Primitive Rendering System =====

void URenderer::InitializeDebugPrimitiveBatch()
{
	// Load debug primitive shader
	DebugPrimitiveShader = UResourceManager::GetInstance().Load<UShader>("Shaders/Debug/DebugPrimitive.hlsl");
}

void URenderer::BeginDebugPrimitiveBatch()
{
	bDebugPrimitiveBatchActive = true;

	// Set render target with ID buffer for picking support
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTargetWithId);

	// Set blend state for transparency (SrcAlpha, InvSrcAlpha)
	RHIDevice->OMSetBlendState(true);

	// Set depth stencil state: depth test disabled for X-ray effect (always render on top)
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::Always);

	// Set rasterizer state to solid fill
	RHIDevice->RSSetState(ERasterizerMode::Solid);
}

void URenderer::DrawPrimitiveMesh(UStaticMesh* Mesh, const FMatrix& Transform, const FLinearColor& Color, uint32 UUID)
{
	if (!bDebugPrimitiveBatchActive || !Mesh || !DebugPrimitiveShader)
		return;

	// Set model matrix
	FMatrix ModelInvTranspose = Transform.InverseAffine().Transpose();
	RHIDevice->SetAndUpdateConstantBuffer(ModelBufferType(Transform, ModelInvTranspose));

	// Set color (with alpha for transparency)
	RHIDevice->SetAndUpdateConstantBuffer(ColorBufferType(Color, UUID));

	// Prepare shader
	RHIDevice->PrepareShader(DebugPrimitiveShader);

	// Get vertex/index buffers
	ID3D11Buffer* VertexBuffer = Mesh->GetVertexBuffer();
	ID3D11Buffer* IndexBuffer = Mesh->GetIndexBuffer();

	if (!VertexBuffer || !IndexBuffer)
		return;

	// Set vertex/index buffers
	UINT stride = sizeof(FVertexDynamic);
	UINT offset = 0;
	RHIDevice->GetDeviceContext()->IASetVertexBuffers(0, 1, &VertexBuffer, &stride, &offset);
	RHIDevice->GetDeviceContext()->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Draw
	RHIDevice->GetDeviceContext()->DrawIndexed(Mesh->GetIndexCount(), 0, 0);
}

void URenderer::DrawDebugSphere(const FMatrix& Transform, const FLinearColor& Color, uint32 UUID)
{
	if (!bDebugPrimitiveBatchActive)
		return;

	UStaticMesh* SphereMesh = UResourceManager::GetInstance().GetOrCreatePrimitiveMesh("Sphere");
	if (SphereMesh)
	{
		DrawPrimitiveMesh(SphereMesh, Transform, Color, UUID);
	}
}

void URenderer::DrawDebugBox(const FMatrix& Transform, const FLinearColor& Color, uint32 UUID)
{
	if (!bDebugPrimitiveBatchActive)
		return;

	UStaticMesh* BoxMesh = UResourceManager::GetInstance().GetOrCreatePrimitiveMesh("Box");
	if (BoxMesh)
	{
		DrawPrimitiveMesh(BoxMesh, Transform, Color, UUID);
	}
}

void URenderer::DrawDebugCapsule(const FMatrix& Transform, float Radius, float HalfHeight, const FLinearColor& Color, uint32 UUID)
{
	if (!bDebugPrimitiveBatchActive)
		return;

	FVector Position(Transform.M[3][0], Transform.M[3][1], Transform.M[3][2]);
	FQuat Rotation(Transform);

	// 실린더 반높이 계산
	float CylinderHalfHeight = HalfHeight - Radius;
	if (CylinderHalfHeight < 0.0f) CylinderHalfHeight = 0.0f;

	UStaticMesh* CapsuleMesh = UResourceManager::GetInstance().GetOrCreateDynamicCapsuleMesh(CylinderHalfHeight, Radius);
	if (CapsuleMesh)
	{
		FMatrix CapsuleTransform = FMatrix::FromTRS(Position, Rotation, FVector::One());
		DrawPrimitiveMesh(CapsuleMesh, CapsuleTransform, Color, UUID);
	}
}

void URenderer::DrawDebugCone(const FMatrix& Transform, float Swing1Angle, float Swing2Angle, float Height, const FLinearColor& Color, uint32 UUID)
{
	if (!bDebugPrimitiveBatchActive)
		return;

	FVector Position(Transform.M[3][0], Transform.M[3][1], Transform.M[3][2]);
	FQuat Rotation(Transform);

	UStaticMesh* ConeMesh = UResourceManager::GetInstance().GetOrCreateEllipticalConeMesh(Swing1Angle, Swing2Angle);
	if (ConeMesh)
	{
		FMatrix ConeTransform = FMatrix::FromTRS(Position, Rotation, FVector(Height, Height, Height));
		DrawPrimitiveMesh(ConeMesh, ConeTransform, Color, UUID);
	}
}

void URenderer::DrawDebugArc(const FMatrix& Transform, float TwistAngle, float Radius, const FLinearColor& Color, uint32 UUID)
{
	if (!bDebugPrimitiveBatchActive)
		return;

	FVector Position(Transform.M[3][0], Transform.M[3][1], Transform.M[3][2]);
	FQuat Rotation(Transform);

	UStaticMesh* ArcMesh = UResourceManager::GetInstance().GetOrCreateDynamicArcMesh(TwistAngle);
	if (ArcMesh)
	{
		FMatrix ArcTransform = FMatrix::FromTRS(Position, Rotation, FVector(1.0f, Radius, Radius));
		DrawPrimitiveMesh(ArcMesh, ArcTransform, Color, UUID);
	}
}

void URenderer::DrawDebugArrow(const FMatrix& Transform, float Length, float HeadSize, const FLinearColor& Color, uint32 UUID)
{
	if (!bDebugPrimitiveBatchActive)
		return;

	FVector Position(Transform.M[3][0], Transform.M[3][1], Transform.M[3][2]);
	FQuat Rotation(Transform);

	UStaticMesh* BoxMesh = UResourceManager::GetInstance().GetOrCreatePrimitiveMesh("Box");
	UStaticMesh* ConeMesh = UResourceManager::GetInstance().GetOrCreatePrimitiveMesh("Cone");
	if (!BoxMesh || !ConeMesh)
		return;

	FVector ArrowDir = Rotation.RotateVector(FVector(1, 0, 0));

	// 1. 축 (Shaft): 길이의 70%
	float ShaftLength = Length * 0.7f;
	float ShaftThickness = HeadSize * 0.15f;
	{
		FVector ShaftCenter = Position + ArrowDir * (ShaftLength * 0.5f);
		FMatrix ShaftTransform = FMatrix::FromTRS(ShaftCenter, Rotation, FVector(ShaftLength * 0.5f, ShaftThickness, ShaftThickness));
		DrawPrimitiveMesh(BoxMesh, ShaftTransform, Color, UUID);
	}

	// 2. 머리 (Head): 길이의 30%
	float HeadLength = Length * 0.3f;
	{
		FVector HeadPosition = Position + ArrowDir * (ShaftLength + HeadLength);
		FQuat FlipRot = FQuat::FromAxisAngle(FVector(0, 1, 0), PI);
		FQuat HeadRot = Rotation * FlipRot;
		FMatrix HeadTransform = FMatrix::FromTRS(HeadPosition, HeadRot, FVector(HeadLength, HeadSize, HeadSize));
		DrawPrimitiveMesh(ConeMesh, HeadTransform, Color, UUID);
	}
}

void URenderer::EndDebugPrimitiveBatch()
{
	if (!bDebugPrimitiveBatchActive)
		return;

	// Restore default states
	RHIDevice->OMSetBlendState(false);
	RHIDevice->OMSetDepthStencilState(EComparisonFunc::LessEqual);
	RHIDevice->GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Restore render target (without ID buffer)
	RHIDevice->OMSetRenderTargets(ERTVMode::SceneColorTarget);

	bDebugPrimitiveBatchActive = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Highlight System (아이템 하이라이트용)
// ═══════════════════════════════════════════════════════════════════════════

void URenderer::AddHighlight(uint32 ObjectID, const FLinearColor& OutlineColor)
{
	if (ObjectID == 0) return; // 0은 무효 ID
	HighlightedObjects[ObjectID] = OutlineColor;
}

void URenderer::RemoveHighlight(uint32 ObjectID)
{
	HighlightedObjects.erase(ObjectID);
}

void URenderer::ClearHighlights()
{
	HighlightedObjects.clear();
}
