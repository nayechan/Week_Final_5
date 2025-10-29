#include "pch.h"
#include "LightManager.h"
#include "AmbientLightComponent.h"
#include "DirectionalLightComponent.h"
#include "SpotLightComponent.h"
#include "PointLightComponent.h"
#include "D3D11RHI.h"

#define NUM_POINT_LIGHT_MAX 256
#define NUM_SPOT_LIGHT_MAX 256
FLightManager::~FLightManager()
{
	Release();
}
void FLightManager::Initialize(D3D11RHI* RHIDevice)
{
	// --- 1. Structured Buffers (t17, t18) ---
	if (!PointLightBuffer)
	{
		RHIDevice->CreateStructuredBuffer(sizeof(FPointLightInfo), NUM_POINT_LIGHT_MAX, nullptr, &PointLightBuffer);
		RHIDevice->CreateStructuredBufferSRV(PointLightBuffer, &PointLightBufferSRV);
	}
	if (!SpotLightBuffer)
	{
		RHIDevice->CreateStructuredBuffer(sizeof(FSpotLightInfo), NUM_SPOT_LIGHT_MAX, nullptr, &SpotLightBuffer);
		RHIDevice->CreateStructuredBufferSRV(SpotLightBuffer, &SpotLightBufferSRV);
	}

	// --- 2. 2D Atlas (t9) ---
	if (!ShadowAtlasTexture2D)
	{
		// 헬퍼 함수를 사용하는 대신, 여기서 직접 생성합니다.
		ID3D11Device* Device = RHIDevice->GetDevice(); // RHI에서 Device 가져오기

		// 2.1. 텍스처 생성 (TYPELESS 포맷)
		D3D11_TEXTURE2D_DESC TexDesc = {};
		TexDesc.Width = ShadowAtlasSize2D;
		TexDesc.Height = ShadowAtlasSize2D;
		TexDesc.MipLevels = 1;
		TexDesc.ArraySize = 1;
		TexDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // DSV/SRV 바인딩을 위해 Typeless
		TexDesc.SampleDesc.Count = 1;
		TexDesc.SampleDesc.Quality = 0;
		TexDesc.Usage = D3D11_USAGE_DEFAULT;
		TexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

		HRESULT hr = Device->CreateTexture2D(&TexDesc, nullptr, &ShadowAtlasTexture2D);
		if (FAILED(hr))
		{
			UE_LOG("FLightManager::Initialize: CreateTexture2D for ShadowAtlas2D failed!");
			// 실패 시 리소스들을 nullptr로 안전하게 처리
			ShadowAtlasTexture2D = nullptr;
			ShadowAtlasDSV2D = nullptr;
			ShadowAtlasSRV2D = nullptr;
			return; // 혹은 적절한 오류 처리
		}

		// 2.2. DSV (DepthStencilView) 생성
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // 뎁스 쓰기용 포맷
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		hr = Device->CreateDepthStencilView(ShadowAtlasTexture2D, &dsvDesc, &ShadowAtlasDSV2D);
		if (FAILED(hr))
		{
			UE_LOG("FLightManager::Initialize: CreateDepthStencilView for ShadowAtlas2D failed!");
		}

		// 2.3. SRV (ShaderResourceView) 생성
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // 뎁스 읽기용 포맷
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		hr = Device->CreateShaderResourceView(ShadowAtlasTexture2D, &srvDesc, &ShadowAtlasSRV2D);
		if (FAILED(hr))
		{
			UE_LOG("FLightManager::Initialize: CreateShaderResourceView for ShadowAtlas2D failed!");
		}
	}

	// --- 3. Cube Map Atlas (t8) ---
	if (!ShadowAtlasTextureCube)
	{
		// 3.1. 큐브맵 배열 리소스 생성 (TextureCubeArray)
		D3D11_TEXTURE2D_DESC CubeDesc = {};
		CubeDesc.Width = AtlasSizeCube;
		CubeDesc.Height = AtlasSizeCube;
		CubeDesc.MipLevels = 1;
		CubeDesc.ArraySize = CubeArrayCount * 6; // D3D11에서 큐브맵 배열은 ArraySize = N * 6
		CubeDesc.Format = DXGI_FORMAT_R24G8_TYPELESS; // 뎁스 포맷
		CubeDesc.SampleDesc.Count = 1;
		CubeDesc.Usage = D3D11_USAGE_DEFAULT;
		CubeDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		CubeDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE; // 큐브맵 플래그

		RHIDevice->GetDevice()->CreateTexture2D(&CubeDesc, nullptr, &ShadowAtlasTextureCube);

		// 3.2. SRV 생성 (t8 바인딩용)
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		srvDesc.TextureCubeArray.MostDetailedMip = 0;
		srvDesc.TextureCubeArray.MipLevels = 1;
		srvDesc.TextureCubeArray.First2DArrayFace = 0;
		srvDesc.TextureCubeArray.NumCubes = CubeArrayCount;

		RHIDevice->GetDevice()->CreateShaderResourceView(ShadowAtlasTextureCube, &srvDesc, &ShadowAtlasSRVCube);

		// 3.3. 각 면(Face)에 대한 DSV 생성 (Pass 1 렌더링용)
		ShadowCubeFaceDSVs.SetNum(CubeArrayCount * 6);
		ShadowCubeFaceSRVs.SetNum(CubeArrayCount * 6);
		for (uint32 SliceIndex = 0; SliceIndex < CubeArrayCount; ++SliceIndex)
		{
			for (uint32 FaceIndex = 0; FaceIndex < 6; ++FaceIndex)
			{
				uint32 Index = (SliceIndex * 6) + FaceIndex;
				
				// DSV 생성
				D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
				dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
				dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				dsvDesc.Texture2DArray.MipSlice = 0;
				dsvDesc.Texture2DArray.FirstArraySlice = Index;
				dsvDesc.Texture2DArray.ArraySize = 1; // 각 DSV는 1개의 면만 참조

				RHIDevice->GetDevice()->CreateDepthStencilView(ShadowAtlasTextureCube, &dsvDesc, &ShadowCubeFaceDSVs[Index]);
				
				// SRV 생성 (각 면을 2D 텍스처로 읽을 수 있도록)
				D3D11_SHADER_RESOURCE_VIEW_DESC srvFaceDesc = {};
				srvFaceDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
				srvFaceDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				srvFaceDesc.Texture2DArray.MostDetailedMip = 0;
				srvFaceDesc.Texture2DArray.MipLevels = 1;
				srvFaceDesc.Texture2DArray.FirstArraySlice = Index;
				srvFaceDesc.Texture2DArray.ArraySize = 1; // 각 SRV는 1개의 면만 참조
				
				RHIDevice->GetDevice()->CreateShaderResourceView(ShadowAtlasTextureCube, &srvFaceDesc, &ShadowCubeFaceSRVs[Index]);
			}
		}
	}
}

void FLightManager::Release()
{
	if (PointLightBuffer)
	{
		PointLightBuffer->Release();
		PointLightBuffer = nullptr;
	}
	if (PointLightBufferSRV)
	{
		PointLightBufferSRV->Release();
		PointLightBufferSRV = nullptr;
	}

	if (SpotLightBuffer)
	{
		SpotLightBuffer->Release();
		SpotLightBuffer = nullptr;
	}
	if (SpotLightBufferSRV)
	{
		SpotLightBufferSRV->Release();
		SpotLightBufferSRV = nullptr;
	}
	
	// 2D Atlas Release
	if (ShadowAtlasSRV2D) { ShadowAtlasSRV2D->Release(); ShadowAtlasSRV2D = nullptr; }
	if (ShadowAtlasDSV2D) { ShadowAtlasDSV2D->Release(); ShadowAtlasDSV2D = nullptr; }
	if (ShadowAtlasTexture2D) { ShadowAtlasTexture2D->Release(); ShadowAtlasTexture2D = nullptr; }

	// Cube Atlas Release
	if (ShadowAtlasSRVCube) { ShadowAtlasSRVCube->Release(); ShadowAtlasSRVCube = nullptr; }
	for (auto* dsv : ShadowCubeFaceDSVs)
	{
		if (dsv) dsv->Release();
	}
	ShadowCubeFaceDSVs.clear();
	for (auto* srv : ShadowCubeFaceSRVs)
	{
		if (srv) srv->Release();
	}
	ShadowCubeFaceSRVs.clear();
	if (ShadowAtlasTextureCube) { ShadowAtlasTextureCube->Release(); ShadowAtlasTextureCube = nullptr; }
}

void FLightManager::UpdateLightBuffer(D3D11RHI* RHIDevice)
{
	// 1. 아무것도 변경되지 않았으면 모든 작업을 건너뜀
	if (!bHaveToUpdate && !bPointLightDirty && !bSpotLightDirty && !bShadowDataDirty)
	{
		return;
	}

	// 2. 초기화 확인
	if (!PointLightBuffer || !SpotLightBuffer)
	{
		Initialize(RHIDevice);
	}

	// 3. CBuffer 업데이트 (Ambient, Directional)
	FLightBufferType LightBuffer{}; // 셰이더의 CBuffer 'b1'과 일치해야 함

	if (AmbientLightList.Num() > 0 && AmbientLightList[0]->IsVisible() && AmbientLightList[0]->GetOwner()->IsActorVisible())
	{
		LightBuffer.AmbientLight = AmbientLightList[0]->GetLightInfo();
	}

	if (DIrectionalLightList.Num() > 0 && DIrectionalLightList[0]->IsVisible() && DIrectionalLightList[0]->GetOwner()->IsActorVisible())
	{
		UDirectionalLightComponent* Light = DIrectionalLightList[0];
		LightBuffer.DirectionalLight = Light->GetLightInfo(); // 기본 정보 (색상, 방향)

		// 섀도우 데이터 (CSM) 병합
		if (Light->IsCastShadows() && ShadowDataCache2D.Contains(Light))
		{
			const TArray<FShadowMapData>& Cascades = ShadowDataCache2D[Light];
			int32 CascadeCount = FMath::Min(Cascades.Num(), CASCADED_MAX); // 최대 4개까지만 복사

			for (int32 i = 0; i < CascadeCount; ++i)
			{
				LightBuffer.DirectionalLight.Cascades[i] = Cascades[i];
			}
		}
	}

	// 4. Point Light Structured Buffer 업데이트 (t3)
	if (bPointLightDirty || bShadowDataDirty)
	{
		PointLightInfoList.clear();
		for (UPointLightComponent* Light : PointLightList)
		{
			if (Light->IsVisible() && Light->GetOwner()->IsActorVisible())
			{
				FPointLightInfo Info = Light->GetLightInfo(); // 기본 정보

				// 섀도우 데이터 (큐브맵 인덱스) 병합
				if (Light->IsCastShadows() && ShadowDataCacheCube.Contains(Light))
				{
					Info.ShadowArrayIndex = ShadowDataCacheCube[Light];
					Info.bCastShadows = (Info.ShadowArrayIndex != -1);
				}
				PointLightInfoList.Add(Info);
			}
		}
		PointLightNum = PointLightInfoList.Num();
		RHIDevice->UpdateStructuredBuffer(PointLightBuffer, PointLightInfoList.data(), PointLightNum * sizeof(FPointLightInfo));
		bPointLightDirty = false;
	}

	// 5. Spot Light Structured Buffer 업데이트 (t4)
	if (bSpotLightDirty || bShadowDataDirty)
	{
		SpotLightInfoList.clear();
		for (USpotLightComponent* Light : SpotLightList)
		{
			if (Light->IsVisible() && Light->GetOwner()->IsActorVisible())
			{
				FSpotLightInfo Info = Light->GetLightInfo(); // 기본 정보

				// 섀도우 데이터 (2D 아틀라스) 병합
				if (Light->IsCastShadows() && ShadowDataCache2D.Contains(Light) && ShadowDataCache2D[Light].Num() > 0)
				{
					Info.ShadowData = ShadowDataCache2D[Light][0]; // 스포트라이트는 0번 인덱스 사용
					Info.bCastShadows = 1;
				}
				SpotLightInfoList.Add(Info);
			}
		}
		SpotLightNum = SpotLightInfoList.Num();
		RHIDevice->UpdateStructuredBuffer(SpotLightBuffer, SpotLightInfoList.data(), SpotLightNum * sizeof(FSpotLightInfo));
		bSpotLightDirty = false;
	}

	// 6. CBuffer에 라이트 개수 업데이트 및 바인딩
	LightBuffer.PointLightCount = PointLightNum;
	LightBuffer.SpotLightCount = SpotLightNum;
	RHIDevice->SetAndUpdateConstantBuffer(LightBuffer);

	// --- 7. 모든 SRV 바인딩 (매 프레임) ---

	// 7.1. 섀도우 아틀라스 (t8, t9)
	if (ShadowAtlasSRVCube)
	{
		RHIDevice->GetDeviceContext()->PSSetShaderResources(8, 1, &ShadowAtlasSRVCube);
	}
	if (ShadowAtlasSRV2D)
	{
		RHIDevice->GetDeviceContext()->PSSetShaderResources(9, 1, &ShadowAtlasSRV2D);
	}

	// 7.2. 라이트 버퍼 (t3, t4)
	ID3D11ShaderResourceView* LightSRVs[2] = { PointLightBufferSRV, SpotLightBufferSRV };
	RHIDevice->GetDeviceContext()->PSSetShaderResources(3, 2, LightSRVs);
	RHIDevice->GetDeviceContext()->VSSetShaderResources(3, 2, LightSRVs); // Gouraud용

	// 8. 모든 Dirty Flag 클리어
	bHaveToUpdate = false;
	bShadowDataDirty = false;
	// (bPointLightDirty, bSpotLightDirty는 이미 위에서 개별 클리어됨)
}

void FLightManager::SetDirtyFlag()
{
	bHaveToUpdate = true;
	bPointLightDirty = true;
	bSpotLightDirty = true;
	bShadowDataDirty = true;
}

void FLightManager::SetShadowMapData(ULightComponent* Light, int32 SubViewIndex, const FShadowMapData& Data)
{
	if (!Light) return;

	// 라이트에 해당하는 TArray를 찾거나 생성
	TArray<FShadowMapData>& Cascades = ShadowDataCache2D[Light];

	// TArray 크기를 SubViewIndex + 1로 확장 (필요한 경우)
	if (Cascades.Num() <= SubViewIndex)
	{
		Cascades.SetNum(SubViewIndex + 1);
	}

	// 데이터 저장
	Cascades[SubViewIndex] = Data;

	bShadowDataDirty = true;
	bHaveToUpdate = true;
}

void FLightManager::SetShadowCubeMapData(ULightComponent* Light, int32 SliceIndex)
{
	if (!Light) return;
	if (SliceIndex < 0 || CubeArrayCount <= SliceIndex) return;

	// TMap에 슬라이스 인덱스 저장
	ShadowDataCacheCube[Light] = SliceIndex;

	bShadowDataDirty = true;
	bHaveToUpdate = true;
}

ID3D11DepthStencilView* FLightManager::GetShadowCubeFaceDSV(UINT SliceIndex, UINT FaceIndex) const
{
	UINT Index = (SliceIndex * 6) + FaceIndex;
	if (Index < ShadowCubeFaceDSVs.Num())
	{
		return ShadowCubeFaceDSVs[Index];
	}
	return nullptr;
}

ID3D11ShaderResourceView* FLightManager::GetShadowCubeFaceSRV(UINT SliceIndex, UINT FaceIndex) const
{
	UINT Index = (SliceIndex * 6) + FaceIndex;
	if (Index < ShadowCubeFaceSRVs.Num())
	{
		return ShadowCubeFaceSRVs[Index];
	}
	return nullptr;
}

bool FLightManager::GetCachedShadowData(ULightComponent* Light, int32 SubViewIndex, FShadowMapData& OutData) const
{
	// 1. 유효성 검사
	if (!Light || SubViewIndex < 0)
	{
		return false;
	}

	// 2. 해당 라이트에 대한 캐시 데이터(TArray) 찾기
	const TArray<FShadowMapData>* FoundDataArray = ShadowDataCache2D.Find(Light);
	if (!FoundDataArray)
	{
		// 이 라이트에 대한 캐시 데이터가 없음
		return false;
	}

	// 3. 요청된 SubViewIndex가 TArray 범위 내에 있는지 확인
	if (SubViewIndex >= FoundDataArray->Num())
	{
		// 유효하지 않은 인덱스
		return false;
	}

	// 4. 데이터 복사 및 성공 반환
	OutData = (*FoundDataArray)[SubViewIndex];
	return true;
}

bool FLightManager::GetCachedShadowCubeSliceIndex(ULightComponent* Light, int32& OutSliceIndex) const
{
	// 1. 유효성 검사
	if (!Light)
	{
		OutSliceIndex = -1;
		return false;
	}

	// 2. 해당 라이트에 대한 캐시 데이터(int32) 찾기
	const int32* FoundIndex = ShadowDataCacheCube.Find(Light);
	if (!FoundIndex)
	{
		// 이 라이트에 대한 캐시 데이터가 없음
		OutSliceIndex = -1;
		return false;
	}

	// 3. 데이터 복사 및 성공 반환
	OutSliceIndex = *FoundIndex;
	return true;
}

// 단순한 아틀라스 로직
void FLightManager::AllocateAtlasRegions2D(TArray<FShadowRenderRequest>& InOutRequests2D)
{
	// 요청 정렬 (가장 큰 것부터)
	InOutRequests2D.Sort(std::greater<FShadowRenderRequest>());

	// 동적 패킹(Shelf Algorithm)
	uint32 CurrentAtlasX = 0;
	uint32 CurrentAtlasY = 0;
	uint32 CurrentShelfMaxHeight = 0;

	for (FShadowRenderRequest& Request : InOutRequests2D)
	{
		if (CurrentAtlasX + Request.Size > ShadowAtlasSize2D)
		{
			CurrentAtlasY += CurrentShelfMaxHeight;
			CurrentAtlasX = 0;
			CurrentShelfMaxHeight = 0;
		}
		if (CurrentAtlasY + Request.Size > ShadowAtlasSize2D)
		{
			Request.Size = 0; // 꽉 참 (렌더링 실패)
			UE_LOG("그림자 맵 아틀라스가 가득차서 더 이상 그림자를 추가할 수 없습니다.");
			continue;
		}

		Request.AtlasViewportOffset = FVector2D((float)CurrentAtlasX, (float)CurrentAtlasY);

		// Pass 2 데이터 (UV) 저장
		Request.AtlasScaleOffset = FVector4(
			Request.Size / (float)ShadowAtlasSize2D,    // ScaleX
			Request.Size / (float)ShadowAtlasSize2D,    // ScaleY
			CurrentAtlasX / (float)ShadowAtlasSize2D,   // OffsetX
			CurrentAtlasY / (float)ShadowAtlasSize2D    // OffsetY
		);

		CurrentAtlasX += Request.Size;
		CurrentShelfMaxHeight = FMath::Max(CurrentShelfMaxHeight, Request.Size);
	}
}

void FLightManager::AllocateAtlasCubeSlices(TArray<FShadowRenderRequest>& InOutRequestsCube)
{
	// 슬라이스 개수가 유효하지 않으면 모든 요청 실패 처리
	if (CubeArrayCount == 0)
	{
		for (FShadowRenderRequest& Request : InOutRequestsCube)
		{
			Request.Size = 0; // 할당 불가
			Request.AssignedSliceIndex = -1; // 할당 인덱스를 -1로 설정
		}
		return;
	}

	int AllocatorCube_CurrentSliceIndex = 0;

	ULightComponent* CurrentLightOwner = nullptr; // 현재 처리 중인 라이트 추적용
	bool bCurrentLightFailed = false; // 현재 라이트가 슬라이스 할당에 실패했는지 여부
	// 로컬 변수 대신 FLightManager의 멤버 변수 AllocatorCube_CurrentSliceIndex 사용

	// 요청 리스트 순회 (각 라이트당 6개의 요청이 들어옴)
	for (FShadowRenderRequest& Request : InOutRequestsCube)
	{
		// 유효하지 않은 요청은 건너뜀 (예: Size가 0인 경우)
		if (Request.Size == 0)
		{
			Request.AssignedSliceIndex = -1; // 실패 상태 명시
			continue;
		}

		// 라이트가 변경되었는지 확인
		if (Request.LightOwner != CurrentLightOwner)
		{
			// 첫 라이트가 아니면서 이전 라이트가 성공적으로 할당되었을 경우, 다음 슬라이스로 이동
			if (CurrentLightOwner != nullptr && !bCurrentLightFailed)
			{
				AllocatorCube_CurrentSliceIndex++; // 다음 슬라이스 인덱스로 증가 (멤버 변수 사용)
			}

			CurrentLightOwner = Request.LightOwner; // 현재 라이트 갱신

			// 새 라이트가 슬라이스 개수 제한을 초과하는지 확인
			if (AllocatorCube_CurrentSliceIndex >= CubeArrayCount)
			{
				bCurrentLightFailed = true; // 이 라이트는 할당 불가
			}
			else
			{
				bCurrentLightFailed = false; // 이 라이트는 할당 가능
			}
		}

		// 현재 라이트가 할당 실패 상태인지 확인
		if (bCurrentLightFailed)
		{
			Request.Size = 0; // 할당 실패 처리
			Request.AssignedSliceIndex = -1; // 할당 인덱스를 -1로 설정
		}
		else
		{
			// --- [핵심 수정] ---
			// 성공: 현재 슬라이스 인덱스를 'AssignedSliceIndex' 멤버에 기록
			Request.AssignedSliceIndex = AllocatorCube_CurrentSliceIndex;
			// OriginalSubViewIndex는 건드리지 않음
		}
	}
}

void FLightManager::ClearAllLightList()
{
	AmbientLightList.clear();
	DIrectionalLightList.clear();
	PointLightList.clear();
	SpotLightList.clear();

	//업데이트 시에만 clear하고 다시 수집할 실제 데이터 리스트
	PointLightInfoList.clear();
	SpotLightInfoList.clear();

	//이미 레지스터된 라이트인지 확인하는 용도
	LightComponentList.clear();
	PointLightNum = 0;
	SpotLightNum = 0;

	ShadowDataCache2D.clear();
	ShadowDataCacheCube.clear();
}

template<typename T>
void FLightManager::RegisterLight(T* LightComponent)
{
}

template<>
void FLightManager::RegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	AmbientLightList.Add(LightComponent);
	bHaveToUpdate = true;
}

template<>
void FLightManager::RegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	DIrectionalLightList.Add(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::RegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent)||
		Cast<USpotLightComponent>(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	PointLightList.Add(LightComponent);
	PointLightInfoList.Add(LightComponent->GetLightInfo());
	PointLightNum++;
	bPointLightDirty = true;
	bHaveToUpdate = true;
}

template<>
void FLightManager::RegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Add(LightComponent);
	SpotLightList.Add(LightComponent);
	SpotLightInfoList.Add(LightComponent->GetLightInfo());
	SpotLightNum++;
	bSpotLightDirty = true;
	bHaveToUpdate = true;
}

template<>
void FLightManager::DeRegisterLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	AmbientLightList.Remove(LightComponent);
	bHaveToUpdate = true;
}
template<>
void FLightManager::DeRegisterLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	DIrectionalLightList.Remove(LightComponent);
	bHaveToUpdate = true;

	ShadowDataCache2D.Remove(LightComponent);
}
template<>
void FLightManager::DeRegisterLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent) ||
		Cast<USpotLightComponent>(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	PointLightList.Remove(LightComponent);
	PointLightNum--;
	bPointLightDirty = true;
	bHaveToUpdate = true;

	ShadowDataCacheCube.Remove(LightComponent);
}
template<>
void FLightManager::DeRegisterLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	LightComponentList.Remove(LightComponent);
	SpotLightList.Remove(LightComponent);
	SpotLightNum--;
	bSpotLightDirty = true;
	bHaveToUpdate = true;

	ShadowDataCache2D.Remove(LightComponent);
}


template<> void FLightManager::UpdateLight<UAmbientLightComponent>(UAmbientLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<UDirectionalLightComponent>(UDirectionalLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<UPointLightComponent>(UPointLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent) ||
		Cast<USpotLightComponent>(LightComponent))
	{
		return;
	}
	bPointLightDirty = true;
	bHaveToUpdate = true;
}
template<> void FLightManager::UpdateLight<USpotLightComponent>(USpotLightComponent* LightComponent)
{
	if (!LightComponentList.Contains(LightComponent))
	{
		return;
	}
	bSpotLightDirty = true;
	bHaveToUpdate = true;
}