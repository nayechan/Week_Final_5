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

	
	if (!bInitialized)
	{
		return;
	}

	D2D1_FACTORY_OPTIONS FactoryOpts{};
#ifdef _DEBUG
	FactoryOpts.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
	if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &FactoryOpts, (void**)&D2DFactory)))
	{
		return;
	}

	IDXGIDevice* DxgiDevice = nullptr;
	if (FAILED(D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&DxgiDevice)))
	{
		return;
	}

	if (FAILED(D2DFactory->CreateDevice(DxgiDevice, &D2DDevice)))
	{
		SafeRelease(DxgiDevice);
		return;
	}
	SafeRelease(DxgiDevice);

	if (FAILED(D2DDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &D2DContext)))
	{
		return;
	}

	if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&DWriteFactory)))
	{
		return;
	}

	if (DWriteFactory)
	{
		DWriteFactory->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_NORMAL,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			16.0f,
			L"en-us",
			&TextFormat);

		if (TextFormat)
		{
			TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
		}
	}

	EnsureInitialized();
}

void UStatsOverlayD2D::Shutdown()
{
	ReleaseD2DResources();

	D3DDevice = nullptr;
	D3DContext = nullptr;
	SwapChain = nullptr;
	bInitialized = false;
}

void UStatsOverlayD2D::EnsureInitialized()
{
	if (!D2DContext)
	{
		return;
	}

	SafeRelease(BrushYellow);
	SafeRelease(BrushSkyBlue);
	SafeRelease(BrushLightGreen);
	SafeRelease(BrushOrange);
	SafeRelease(BrushCyan);
	SafeRelease(BrushViolet);
	SafeRelease(BrushDeepPink);
	SafeRelease(BrushBlack);

	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow), &BrushYellow);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::SkyBlue), &BrushSkyBlue);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGreen), &BrushLightGreen);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Orange), &BrushOrange);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Cyan), &BrushCyan);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Violet), &BrushViolet);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DeepPink), &BrushDeepPink);
	D2DContext->CreateSolidColorBrush(D2D1::ColorF(0, 0, 0, 0.6f), &BrushBlack);

}

void UStatsOverlayD2D::ReleaseD2DResources()
{
	SafeRelease(BrushBlack);
	SafeRelease(BrushDeepPink);
	SafeRelease(BrushViolet);
	SafeRelease(BrushCyan);
	SafeRelease(BrushOrange);
	SafeRelease(BrushLightGreen);
	SafeRelease(BrushSkyBlue);
	SafeRelease(BrushYellow);

	SafeRelease(TextFormat);
	SafeRelease(DWriteFactory);
	SafeRelease(D2DContext);
	SafeRelease(D2DDevice);
	SafeRelease(D2DFactory);
}

static void DrawTextBlock(
	ID2D1DeviceContext* InD2dCtx,
	IDWriteTextFormat* InTextFormat,
	const wchar_t* InText,
	const D2D1_RECT_F& InRect,
	ID2D1SolidColorBrush* InBgBrush,
	ID2D1SolidColorBrush* InTextBrush)
{
	if (!InD2dCtx || !InTextFormat || !InText || !InBgBrush || !InTextBrush)
	{
		return;
	}

	InD2dCtx->FillRectangle(InRect, InBgBrush);
	InD2dCtx->DrawTextW(
		InText,
		static_cast<UINT32>(wcslen(InText)),
		InTextFormat,
		InRect,
		InTextBrush);
}


void UStatsOverlayD2D::Draw()
{
	if (!bInitialized || (!bShowFPS && !bShowMemory && !bShowPicking && !bShowDecal && !bShowTileCulling && !bShowLights && !bShowShadow && !bShowSkinning) || !SwapChain)
	{
		return;
	}

	if (!D2DContext || !TextFormat)
	{
		return;
	}

	IDXGISurface* Surface = nullptr;
	if (FAILED(SwapChain->GetBuffer(0, __uuidof(IDXGISurface), (void**)&Surface)))
	{
		return;
	}

	D2D1_BITMAP_PROPERTIES1 BmpProps = {};
	BmpProps.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
	BmpProps.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
	BmpProps.dpiX = 96.0f;
	BmpProps.dpiY = 96.0f;
	BmpProps.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

	ID2D1Bitmap1* TargetBmp = nullptr;
	if (FAILED(D2DContext->CreateBitmapFromDxgiSurface(Surface, &BmpProps, &TargetBmp)))
	{
		SafeRelease(Surface);
		return;
	}

	D2DContext->SetTarget(TargetBmp);

	D2DContext->BeginDraw();

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
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushYellow);

		NextY += PanelHeight + Space;
	}

	if (bShowPicking)
	{
		wchar_t Buf[256];
		double LastMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetLastPickTime());
		double TotalMs = FWindowsPlatformTime::ToMilliseconds(CPickingSystem::GetTotalPickTime());
		uint32 Count = CPickingSystem::GetPickCount();
		double AvgMs = (Count > 0) ? (TotalMs / (double)Count) : 0.0;
		swprintf_s(Buf, L"Pick Count: %u\nLast: %.3f ms\nAvg: %.3f ms\nTotal: %.3f ms", Count, LastMs, AvgMs, TotalMs);

		const float PickPanelHeight = 96.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PickPanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushSkyBlue);

		NextY += PickPanelHeight + Space;
	}

	if (bShowMemory)
	{
		double Mb = static_cast<double>(FMemoryManager::TotalAllocationBytes) / (1024.0 * 1024.0);

		wchar_t Buf[128];
		swprintf_s(Buf, L"Memory: %.1f MB\nAllocs: %u", Mb, FMemoryManager::TotalAllocationCount);

		D2D1_RECT_F Rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + PanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, Rc, BrushBlack, BrushLightGreen);

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

		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushOrange);

		NextY += decalPanelHeight + Space;
	}

	if (bShowTileCulling)
	{
		const FTileCullingStats& TileStats = FTileCullingStatManager::GetInstance().GetStats();

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

		const float tilePanelHeight = 160.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + tilePanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushCyan);

		NextY += tilePanelHeight + Space;
	}

	if (bShowLights)
	{
		const FLightStats& LightStats = FLightStatManager::GetInstance().GetStats();

		wchar_t Buf[512];
		swprintf_s(Buf, L"[Light Stats]\nTotal Lights: %u\n  Point: %u\n  Spot: %u\n  Directional: %u\n  Ambient: %u",
			LightStats.TotalLights,
			LightStats.TotalPointLights,
			LightStats.TotalSpotLights,
			LightStats.TotalDirectionalLights,
			LightStats.TotalAmbientLights);

		const float lightPanelHeight = 140.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + lightPanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushViolet);

		NextY += lightPanelHeight + Space;
	}

	if (bShowShadow)
	{
		const FShadowStats& ShadowStats = FShadowStatManager::GetInstance().GetStats();

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

		const float shadowPanelHeight = 260.0f;
		D2D1_RECT_F rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + shadowPanelHeight);
		DrawTextBlock(D2DContext, TextFormat, Buf, rc, BrushBlack, BrushDeepPink);

		NextY += shadowPanelHeight + Space;

		rc = D2D1::RectF(Margin, NextY, Margin + PanelWidth, NextY + 40);
		DrawTextBlock(D2DContext, TextFormat, FScopeCycleCounter::GetTimeProfile("ShadowMapPass").GetConstWChar_tWithKey("ShadowMapPass"), rc, BrushBlack, BrushDeepPink);

		NextY += shadowPanelHeight + Space;
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

		DrawTextBlock(D2DContext, TextFormat, SkinningBuf, skinningRc, BrushBlack, BrushDeepPink);
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
		DrawTextBlock(D2DContext, TextFormat, ParticleBuf, particleRc, BrushBlack, BrushCyan);


		NextY += particlePanelHeight + Space;
	}

	D2DContext->EndDraw();
	D2DContext->SetTarget(nullptr);

	FScopeCycleCounter::TimeProfileInit();

	// 매 프레임 생성한 리소스만 해제 (캐싱된 D2D 리소스는 유지)
	SafeRelease(TargetBmp);
	SafeRelease(Surface);
}
