#include "pch.h"

#include <d2d1_1.h>
#include <dwrite.h>
#include <dxgi1_2.h>

#include "StatsOverlayD2D.h"
#include "UIManager.h"
#include "MemoryManager.h"
#include "Picking.h"
#include "PlatformTime.h"
#include "DecalStatManager.h"
#include "TileCullingStats.h"
#include "LightStats.h"
#include "ShadowStats.h"
#include "SkinningStats.h"
#include "SkinnedMeshComponent.h"
#include "ParticleStats.h"

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

static inline void SafeRelease(IUnknown* p) { if (p) p->Release(); }

UStatsOverlayD2D& UStatsOverlayD2D::Get()
{
	static UStatsOverlayD2D Instance;
	return Instance;
}

void UStatsOverlayD2D::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, IDXGISwapChain* InSwapChain)
{
	D3DDevice = InDevice;
	D3DContext = InContext;
	SwapChain = InSwapChain;
	bInitialized = (D3DDevice && D3DContext && SwapChain);
}

void UStatsOverlayD2D::Shutdown()
{
	// D2D 리소스 해제
	SafeRelease(CachedBrush);
	SafeRelease(TextFormat);
	SafeRelease(Dwrite);
	SafeRelease(D2dCtx);
	SafeRelease(D2dDevice);
	SafeRelease(D2dFactory);
	bD2DInitialized = false;

	D3DDevice = nullptr;
	D3DContext = nullptr;
	SwapChain = nullptr;
	bInitialized = false;
}

void UStatsOverlayD2D::EnsureInitialized()
{
	if (bD2DInitialized || !bInitialized || !D3DDevice)
		return;

	// D2D Factory 생성 (한 번만)
	D2D1_FACTORY_OPTIONS Opts{};
#ifdef _DEBUG
	Opts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &Opts, (void**)&D2dFactory)))
		return;

	// DXGI Device 쿼리
	IDXGIDevice* DxgiDevice = nullptr;
	if (FAILED(D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DxgiDevice)))
	{
		SafeRelease(D2dFactory);
		return;
	}

	// D2D Device 생성 (한 번만)
	if (FAILED(D2dFactory->CreateDevice(DxgiDevice, &D2dDevice)))
	{
		SafeRelease(DxgiDevice);
		SafeRelease(D2dFactory);
		return;
	}
	SafeRelease(DxgiDevice);

	// D2D DeviceContext 생성 (한 번만)
	if (FAILED(D2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2dCtx)))
	{
		SafeRelease(D2dDevice);
		SafeRelease(D2dFactory);
		return;
	}

	// DWrite Factory 생성 (한 번만)
	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&Dwrite)))
	{
		SafeRelease(D2dCtx);
		SafeRelease(D2dDevice);
		SafeRelease(D2dFactory);
		return;
	}

	// TextFormat 생성 (한 번만, 모든 텍스트에 재사용)
	if (FAILED(Dwrite->CreateTextFormat(
		L"Segoe UI",
		nullptr,
		DWRITE_FONT_WEIGHT_NORMAL,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		16.0f,
		L"en-us",
		&TextFormat)))
	{
		SafeRelease(Dwrite);
		SafeRelease(D2dCtx);
		SafeRelease(D2dDevice);
		SafeRelease(D2dFactory);
		return;
	}

	TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
	TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

	// Brush 생성 (한 번만, 색상은 SetColor로 변경)
	if (FAILED(D2dCtx->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f), &CachedBrush)))
	{
		SafeRelease(TextFormat);
		SafeRelease(Dwrite);
		SafeRelease(D2dCtx);
		SafeRelease(D2dDevice);
		SafeRelease(D2dFactory);
		return;
	}

	bD2DInitialized = true;
}

void UStatsOverlayD2D::ReleaseD2DTarget()
{
}

static void DrawTextBlock(
	ID2D1DeviceContext* InD2dCtx,
	ID2D1SolidColorBrush* InBrush,
	IDWriteTextFormat* InTextFormat,
	const wchar_t* InText,
	const D2D1_RECT_F& InRect,
	D2D1::ColorF InBgColor,
	D2D1::ColorF InTextColor)
{
	if (!InD2dCtx || !InBrush || !InTextFormat || !InText) return;

	// 배경 그리기 (Brush 색상 변경하여 재사용)
	InBrush->SetColor(InBgColor);
	InD2dCtx->FillRectangle(InRect, InBrush);

	// 텍스트 그리기 (Brush 색상 변경하여 재사용)
	InBrush->SetColor(InTextColor);
	InD2dCtx->DrawTextW(
		InText,
		static_cast<UINT32>(wcslen(InText)),
		InTextFormat,
		InRect,
		InBrush);
}

void UStatsOverlayD2D::Draw()
{
	if (!bInitialized || (!bShowFPS && !bShowMemory && !bShowPicking && !bShowDecal && !bShowTileCulling && !bShowLights && !bShowShadow && !bShowSkinning && !bShowParticles) || !SwapChain)
		return;

	// D2D 리소스 초기화 (최초 1회만 실행)
	EnsureInitialized();
	if (!bD2DInitialized)
		return;

	// 매 프레임마다 생성/파괴가 필요한 리소스 (스왑체인 백버퍼)
	IDXGISurface* Surface = nullptr;
	if (FAILED(SwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&Surface)))
		return;

	D2D1_BITMAP_PROPERTIES1 BmpProps = {};
	BmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	BmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	BmpProps.dpiX = 96.0f;
	BmpProps.dpiY = 96.0f;
	BmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

	ID2D1Bitmap1* TargetBmp = nullptr;
	if (FAILED(D2dCtx->CreateBitmapFromDxgiSurface(Surface, &BmpProps, &TargetBmp)))
	{
		SafeRelease(Surface);
		return;
	}

	D2dCtx->SetTarget(TargetBmp);

	D2dCtx->BeginDraw();
	const float Margin = 12.0f;
	const float Space = 8.0f;   // 패널간의 간격
	const float PanelWidth = 200.0f;
	const float SkinningPanelWidth = 340.0f;  // 스키닝 통계용 넓은 패널
	const float PanelHeight = 48.0f;
	float NextY = 70.0f;

	if (bShowFPS)
	{
		float Dt = UUIManager::GetInstance().GetDeltaTime();
		float Fps = Dt > 0.0f ? (1.0f / Dt) : 0.0f;
		float Ms = Dt * 1000.0f;

		wchar_t Buf[128];
		swprintf_s(Buf, L"FPS: %.1f\nFrame time: %.2f ms", Fps, Ms);

		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PanelHeight);
		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, Buf, rc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Yellow));

		NextY += PanelHeight + Space;
	}

	if (bShowPicking)
	{
		// Build the entire block in one Buffer to avoid overwriting previous lines
		wchar_t Buf[256];
		double LastMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetLastPickTime());
		double TotalMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetTotalPickTime());
		uint32 Count = CPickingSystem::GetPickCount();
		double AvgMs = (Count > 0) ? (TotalMs / (double)Count) : 0.0;
		swprintf_s(Buf, L"Pick Count: %u\nLast: %.3f ms\nAvg: %.3f ms\nTotal: %.3f ms", Count, LastMs, AvgMs, TotalMs);

		// Increase panel height to fit multiple lines
		const float PickPanelHeight = 96.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PickPanelHeight);
		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, Buf, rc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::SkyBlue));

		NextY += PickPanelHeight + Space;
	}

	if (bShowMemory)
	{
		double Mb = static_cast<double>(FMemoryManager::TotalAllocationBytes) / (1024.0 * 1024.0);

		wchar_t Buf[128];
		swprintf_s(Buf, L"Memory: %.1f MB\nAllocs: %u", Mb, FMemoryManager::TotalAllocationCount);

		D2D1_RECT_F Rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PanelHeight);
		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, Buf, Rc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::LightGreen));

		NextY += PanelHeight + Space;
	}

	if (bShowDecal)
	{
		// 1. FDecalStatManager로부터 통계 데이터를 가져옵니다.
		uint32_t TotalCount = FDecalStatManager::GetInstance().GetTotalDecalCount();
		//uint32_t VisibleDecalCount = FDecalStatManager::GetInstance().GetVisibleDecalCount();
		uint32_t AffectedMeshCount = FDecalStatManager::GetInstance().GetAffectedMeshCount();
		double TotalTime = FDecalStatManager::GetInstance().GetDecalPassTimeMS();
		double AverageTimePerDecal = FDecalStatManager::GetInstance().GetAverageTimePerDecalMS();
		double AverageTimePerDraw = FDecalStatManager::GetInstance().GetAverageTimePerDrawMS();

		// 2. 출력할 문자열 버퍼를 만듭니다.
		wchar_t Buf[256];
		swprintf_s(Buf, L"[Decal Stats]\nTotal: %u\nAffectedMesh: %u\n전체 소요 시간: %.3f ms\nAvg/Decal: %.3f ms\nAvg/Mesh: %.3f ms",
			TotalCount,
			AffectedMeshCount,
			TotalTime,
			AverageTimePerDecal,
			AverageTimePerDraw);

		// 3. 텍스트를 여러 줄 표시해야 하므로 패널 높이를 늘립니다.
		const float decalPanelHeight = 140.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + decalPanelHeight);

		// 4. DrawTextBlock 함수를 호출하여 화면에 그립니다. 색상은 구분을 위해 주황색(Orange)으로 설정합니다.
		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, Buf, rc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Orange));

		NextY += decalPanelHeight + Space;
	}

	if (bShowTileCulling)
	{
		// 1. FTileCullingStatManager로부터 통계 데이터를 가져옵니다.
		const FTileCullingStats& TileStats = FTileCullingStatManager::GetInstance().GetStats();

		// 2. 출력할 문자열 버퍼를 만듭니다.
		wchar_t Buf[512];
		swprintf_s(Buf, L"[Tile Culling Stats]\nTiles: %u x %u (%u)\nLights: %u (P:%u S:%u)\nMin/Avg/Max: %u / %.1f / %u\nCulling Eff: %.1f%%\nBuffer: %u KB",
			TileStats.TileCountX,
			TileStats.TileCountY,
			TileStats.TotalTileCount,
			TileStats.TotalLights,
			TileStats.TotalPointLights,
			TileStats.TotalSpotLights,
			TileStats.MinLightsPerTile,
			TileStats.AvgLightsPerTile,
			TileStats.MaxLightsPerTile,
			TileStats.CullingEfficiency,
			TileStats.LightIndexBufferSizeBytes / 1024);

		// 3. 텍스트를 여러 줄 표시해야 하므로 패널 높이를 늘립니다.
		const float tilePanelHeight = 160.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + tilePanelHeight);

		// 4. DrawTextBlock 함수를 호출하여 화면에 그립니다. 색상은 구분을 위해 cyan으로 설정합니다.
		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, Buf, rc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Cyan));

		NextY += tilePanelHeight + Space;
	}

	if (bShowLights)
	{
		// 1. FLightStatManager로부터 통계 데이터를 가져옵니다.
		const FLightStats& LightStats = FLightStatManager::GetInstance().GetStats();

		// 2. 출력할 문자열 버퍼를 만듭니다.
		wchar_t Buf[512];
		swprintf_s(Buf, L"[Light Stats]\nTotal Lights: %u\n  Point: %u\n  Spot: %u\n  Directional: %u\n  Ambient: %u",
			LightStats.TotalLights,
			LightStats.TotalPointLights,
			LightStats.TotalSpotLights,
			LightStats.TotalDirectionalLights,
			LightStats.TotalAmbientLights);

		// 3. 텍스트를 여러 줄 표시해야 하므로 패널 높이를 늘립니다.
		const float lightPanelHeight = 140.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + lightPanelHeight);

		// 4. DrawTextBlock 함수를 호출하여 화면에 그립니다. 색상은 구분을 위해 보라색(Purple)으로 설정합니다.
		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, Buf, rc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Violet));

		NextY += lightPanelHeight + Space;
	}

	if (bShowShadow)
	{
		// 1. FShadowStatManager로부터 통계 데이터를 가져옵니다.
		const FShadowStats& ShadowStats = FShadowStatManager::GetInstance().GetStats();

		// 2. 출력할 문자열 버퍼를 만듭니다.
		wchar_t Buf[512];
		swprintf_s(Buf, L"[Shadow Stats]\nShadow Lights: %u\n  Point: %u\n  Spot: %u\n  Directional: %u\n\nAtlas 2D: %u x %u (%.1f MB)\nAtlas Cube: %u x %u x %u (%.1f MB)\n\nTotal Memory: %.1f MB",
			ShadowStats.TotalShadowCastingLights,
			ShadowStats.ShadowCastingPointLights,
			ShadowStats.ShadowCastingSpotLights,
			ShadowStats.ShadowCastingDirectionalLights,
			ShadowStats.ShadowAtlas2DSize,
			ShadowStats.ShadowAtlas2DSize,
			ShadowStats.ShadowAtlas2DMemoryMB,
			ShadowStats.ShadowAtlasCubeSize,
			ShadowStats.ShadowAtlasCubeSize,
			ShadowStats.ShadowCubeArrayCount,
			ShadowStats.ShadowAtlasCubeMemoryMB,
			ShadowStats.TotalShadowMemoryMB);

		// 3. 텍스트를 여러 줄 표시해야 하므로 패널 높이를 늘립니다.
		const float shadowPanelHeight = 260.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + shadowPanelHeight);

		// 4. DrawTextBlock 함수를 호출하여 화면에 그립니다. 색상은 구분을 위해 한색(Magenta)으로 설정합니다.
		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, Buf, rc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::DeepPink));

		NextY += shadowPanelHeight + Space;


		const float shadowMapPassHeight = 40.0f;
		rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + shadowMapPassHeight);

		// 4. DrawTextBlock 함수를 호출하여 화면에 그립니다. 색상은 구분을 위해 한색(Magenta)으로 설정합니다.
		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, FScopeCycleCounter::GetTimeProfile("ShadowMapPass").GetConstWChar_tWithKey("ShadowMapPass"), rc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::DeepPink));

		NextY += shadowMapPassHeight + Space;
	}

	if (bShowSkinning)
	{
		// 스키닝 통계 매니저에서 데이터 가져오기
		const FSkinningStats& Stats = FSkinningStatManager::GetInstance().GetStats();

		// 전역 스키닝 모드 확인
		UWorld* World = GEngine.GetDefaultWorld();
		ESkinningMode GlobalMode = ESkinningMode::ForceCPU;
		if (World)
		{
			GlobalMode = World->GetRenderSettings().GetGlobalSkinningMode();
		}
		bool bUseGPU = (GlobalMode == ESkinningMode::ForceGPU);

		// 통합 스키닝 통계 표시 (현재 모드에 따라 내용 변경)
		wchar_t SkinningBuf[1024];
		/**
		* TODO
		* @note Skinning 측정에서 추후에 신경 써야하는 점:
		* GPU Draw Time은 현재 프레임의 값이 아닌 N-7 프레임의 값을 사용한다.
		* 비동기 GPU 측정이기 때문에 실제로는 N 프레임의 결과가 나오려면 어느 정도 시간이 걸리기 때문이다.
		* CPU 스키닝일 때 병목 현상 때문에 링버퍼를 활용해서 N-7 프레임 값을 사용하고 있기 때문이다.
		* 
		* 나머지 측정 값들은 현재 프레임인 N 프레임의 값을 사용하고 있다.
		*/
		if (bUseGPU)
		{
			// GPU 모드
			swprintf_s(SkinningBuf,
				L"[Skinning Performance]\n"
				L"GPU Skinning\n"
				L"Bone Matrix Calc:        %.3f ms\n"
				L"CPU Vertex Skinning:     %.3f ms\n"
				L"Bone Buffer Upload:      %.3f ms\n"
				L"GPU Draw Time:           %.3f ms\n"
				L"Total Skinning Time:     %.3f ms\n"
				L"\n"
				L"Vertices: %d | Bones: %d\n"
				L"Bone Buffer: %.2f KB\n"
				L"Buffer Updates: %d",
				Stats.BoneMatrixCalcTimeMS,
				Stats.VertexSkinningTimeMS, // GPU 모드에서는 0
				Stats.BufferUploadTimeMS,   // 본 버퍼 업로드 시간
				Stats.DrawTimeMS,
				Stats.GetTotalTimeMS(),
				Stats.TotalVertices,
				Stats.TotalBones,
				Stats.BufferMemory / 1024.0, // 본 버퍼 메모리
				Stats.BufferUpdateCount);
		}
		else
		{
			// CPU 모드
			swprintf_s(SkinningBuf,
				L"[Skinning Performance]\n"
				L"CPU Skinning\n"
				L"Bone Matrix Calc:        %.3f ms\n"
				L"CPU Vertex Skinning:     %.3f ms\n"
				L"Vertex Buffer Upload:    %.3f ms\n"
				L"GPU Draw Time:           %.3f ms\n"
				L"Total Skinning Time:     %.3f ms\n"
				L"\n"
				L"Vertices: %d | Bones: %d\n"
				L"Vertex Buffer: %.2f KB\n"
				L"Buffer Updates: %d",
				Stats.BoneMatrixCalcTimeMS,
				Stats.VertexSkinningTimeMS, // CPU 모드에서만 값이 있음
				Stats.BufferUploadTimeMS,   // 버텍스 버퍼 업로드 시간
				Stats.DrawTimeMS,
				Stats.GetTotalTimeMS(),
				Stats.TotalVertices,
				Stats.TotalBones,
				Stats.BufferMemory / 1024.0, // 버텍스 버퍼 메모리
				Stats.BufferUpdateCount);
		}

		const float skinningPanelHeight = 250.0f;
		D2D1_RECT_F skinningRc = D2D1::RectF(Margin, NextY, Margin + SkinningPanelWidth, NextY + skinningPanelHeight);

		// 현재 모드에 따라 색상 변경 (CPU: 파란색, GPU: 연두색)
		D2D1::ColorF textColor = bUseGPU ? D2D1::ColorF(0.5f, 1.0f, 0.5f) : D2D1::ColorF(0.4f, 0.7f, 1.0f);

		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, SkinningBuf, skinningRc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			textColor);
		NextY += skinningPanelHeight + Space;
	}

	if (bShowParticles)
	{
		const FParticleStats& Stats = FParticleStatManager::GetInstance().GetStats();
		const FParticleStatManager& Mgr = FParticleStatManager::GetInstance();

		// 메모리를 KB 또는 MB로 표시
		wchar_t MemoryStr[64];
		if (Stats.MemoryBytes >= 1024 * 1024)
		{
			swprintf_s(MemoryStr, L"%.2f MB", Stats.MemoryBytes / (1024.0 * 1024.0));
		}
		else
		{
			swprintf_s(MemoryStr, L"%.2f KB", Stats.MemoryBytes / 1024.0);
		}

		wchar_t ParticleBuf[768];
		swprintf_s(ParticleBuf,
			L"[Particles]\n"
			L"Systems: %d\n"
			L"Emitters: %d\n"
			L"Sprite: %d\n"
			L"Mesh: %d\n"
			L"Beam: %d\n"
			L"Ribbon: %d\n"
			L"Total: %d\n"
			L"Max/Min: (%d/%d)\n"
			L"Avg: %.1f\n"
			L"Spawned/Killed: %d/%d\n"
			L"Memory: %s",
			Stats.ParticleSystemCount,
			Stats.EmitterCount,
			Stats.SpriteParticleCount,
			Stats.MeshParticleCount,
			Stats.BeamParticleCount,
			Stats.RibbonParticleCount,
			Stats.TotalParticleCount(),
			Mgr.GetMaxParticles(),
			Mgr.GetMinParticles(),
			Mgr.GetAvgParticles(),
			Stats.SpawnedThisFrame,
			Stats.KilledThisFrame,
			MemoryStr);

		const float particlePanelHeight = 260.0f;
		D2D1_RECT_F particleRc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + particlePanelHeight);

		DrawTextBlock(
			D2dCtx, CachedBrush, TextFormat, ParticleBuf, particleRc,
			D2D1::ColorF(0, 0, 0, 0.6f),
			D2D1::ColorF(D2D1::ColorF::Orange));

		NextY += particlePanelHeight + Space;
	}

	D2dCtx->EndDraw();
	D2dCtx->SetTarget(nullptr);

	FScopeCycleCounter::TimeProfileInit();

	// 매 프레임 생성한 리소스만 해제 (캐싱된 D2D 리소스는 유지)
	SafeRelease(TargetBmp);
	SafeRelease(Surface);
}

void UStatsOverlayD2D::SetShowFPS(bool b)
{
	bShowFPS = b;
}

void UStatsOverlayD2D::SetShowMemory(bool b)
{
	bShowMemory = b;
}

void UStatsOverlayD2D::SetShowPicking(bool b)
{
	bShowPicking = b;
}

void UStatsOverlayD2D::SetShowDecal(bool b)
{
	bShowDecal = b;
}

void UStatsOverlayD2D::ToggleFPS()
{
	bShowFPS = !bShowFPS;
}

void UStatsOverlayD2D::ToggleMemory()
{
	bShowMemory = !bShowMemory;
}

void UStatsOverlayD2D::TogglePicking()
{
	bShowPicking = !bShowPicking;
}

void UStatsOverlayD2D::ToggleDecal()
{
	bShowDecal = !bShowDecal;
}

void UStatsOverlayD2D::SetShowTileCulling(bool b)
{
	bShowTileCulling = b;
}

void UStatsOverlayD2D::ToggleTileCulling()
{
	bShowTileCulling = !bShowTileCulling;
}

void UStatsOverlayD2D::SetShowLights(bool b)
{
	bShowLights = b;
}

void UStatsOverlayD2D::ToggleLights()
{
	bShowLights = !bShowLights;
}

void UStatsOverlayD2D::SetShowShadow(bool b)
{
	bShowShadow = b;
}

void UStatsOverlayD2D::ToggleShadow()
{
	bShowShadow = !bShowShadow;
}

void UStatsOverlayD2D::SetShowSkinning(bool b)
{
	bShowSkinning = b;
}

void UStatsOverlayD2D::ToggleSkinning()
{
	bShowSkinning = !bShowSkinning;
}

void UStatsOverlayD2D::SetShowParticles(bool b)
{
	bShowParticles = b;
}

void UStatsOverlayD2D::ToggleParticles()
{
	bShowParticles = !bShowParticles;
}
