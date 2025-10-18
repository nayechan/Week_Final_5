#pragma once

// pch.h가 FMatrix, d3d11.h 등 공통 핵심 타입을 포함한다고 가정합니다.
#include "pch.h" 

// FMeshBatchElement가 직접 포함하기엔 무거운 헤더들을 전방 선언합니다.
// (UShader, UMaterial, UStaticMesh는 포인터로만 참조)
class UShader;
class UMaterial;
class UStaticMesh;

/**
 * @struct FMeshBatchElement
 * @brief 단일 드로우 콜(Draw Call)을 위한 모든 렌더링 정보를 집계하는 원자 단위 구조체입니다.
 *
 * FSceneRenderer는 렌더링할 프리미티브들을 순회하며 이 구조체의 배열을 '수집(Collect)'합니다.
 * 이후 배열을 셰이더, 머티리얼 등 '정렬 키(Sorting Key)' 기준으로 '정렬(Sort)'합니다.
 * 마지막으로 정렬된 배열을 순회하며 '그리기(Draw)'를 수행하여,
 * 불필요한 GPU 파이프라인 상태 변경을 최소화합니다.
 */
struct FMeshBatchElement
{
	// --- 정렬 키 (Sorting Keys) ---
	// 렌더링 파이프라인 상태를 결정하는 핵심 요소들입니다.
	// 정렬의 1순위 기준이 됩니다.

	/** 렌더링에 사용될 Vertex Shader입니다. */
	UShader* VertexShader = nullptr;

	/** 렌더링에 사용될 Pixel Shader입니다. */
	UShader* PixelShader = nullptr;

	/** 텍스처, 샘플러, 재질 상수 버퍼를 제공하는 머티리얼입니다. */
	UMaterial* Material = nullptr;

	/** 정점/인덱스 버퍼를 제공하는 스태틱 메시입니다. */
	UStaticMesh* Mesh = nullptr;

	// --- 드로우 데이터 (Draw Data) ---
	// Mesh 내에서 이 드로우 콜이 사용할 구체적인 범위입니다.

	/** 이 드로우 콜이 렌더링할 인덱스의 수입니다. */
	uint32 IndexCount = 0;

	/** 인덱스 버퍼에서 읽기 시작할 위치(오프셋)입니다. */
	uint32 StartIndex = 0;

	/** 정점 버퍼의 시작 정점에 추가할 오프셋입니다. (일반적으로 0) */
	uint32 BaseVertexIndex = 0;

	// --- 인스턴스 데이터 (Instance Data) ---
	// 드로우 콜마다 고유하게 변경되는 데이터입니다.
	// (정렬 기준에 포함되지 않음)

	/** 이 오브젝트의 월드 변환 행렬입니다. (Model Matrix) */
	FMatrix WorldMatrix;

	/** 피킹(Picking) 등에 사용될 고유 ID입니다. */
	uint32 ObjectID = 0;

	// --- 파이프라인 상태 (Pipeline State) ---

	/** 이 드로우 콜에 사용할 프리미티브 토폴로지입니다. (TriangleList, LineList 등) */
	D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	/**
	 * 기본 생성자.
	 */
	FMeshBatchElement() = default;
};