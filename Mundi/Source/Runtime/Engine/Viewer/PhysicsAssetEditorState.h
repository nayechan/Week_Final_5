#pragma once

#include "ViewerState.h"
#include "LinesBatch.h"

class UPhysicsAsset;
class ULineComponent;
class ASkeletalMeshActor;
class USkeletalMeshComponent;

/**
 * PhysicsAssetEditorState
 *
 * Physics Asset Editor 전용 상태
 * 바디/제약 조건 편집을 위한 UI 상태 관리
 */
struct PhysicsAssetEditorState : public ViewerState
{
	// ────────────────────────────────────────────────
	// Physics Asset 데이터
	// ────────────────────────────────────────────────

	/** 편집 중인 Physics Asset */
	UPhysicsAsset* EditingAsset = nullptr;

	/** 파일 경로 */
	FString CurrentFilePath;

	/** 수정 여부 */
	bool bIsDirty = false;

	// ────────────────────────────────────────────────
	// 선택 상태
	// ────────────────────────────────────────────────

	/** 선택된 바디 인덱스 (-1이면 선택 없음) */
	int32 SelectedBodyIndex = -1;

	/** 선택된 제약 조건 인덱스 (-1이면 선택 없음) */
	int32 SelectedConstraintIndex = -1;

	/** 선택 모드 (true: Body, false: Constraint) */
	bool bBodySelectionMode = true;

	// ────────────────────────────────────────────────
	// UI 상태
	// ────────────────────────────────────────────────
	// ExpandedBoneIndices는 부모 클래스 ViewerState에서 상속

	/** 제약 조건 그래프 표시 여부 */
	bool bShowConstraintGraph = false;

	/** 그래프의 기준 바디 인덱스 (더블클릭으로만 변경됨) */
	int32 GraphPivotBodyIndex = -1;

	/** 디버그 시각화 옵션 */
	bool bShowBodies = true;
	bool bShowConstraints = true;
	bool bShowBoneNames = false;
	bool bShowMassProperties = false;

	// ────────────────────────────────────────────────
	// Shape 프리뷰 컴포넌트 (에디터용 시각화)
	// ────────────────────────────────────────────────

	/** 바디별 Shape 프리뷰 라인 컴포넌트 */
	ULineComponent* BodyPreviewLineComponent = nullptr;

	/** 제약 조건 시각화 라인 컴포넌트 */
	ULineComponent* ConstraintPreviewLineComponent = nullptr;

	/** Shape 라인 재구성 필요 (바디 추가/제거 시) */
	bool bShapeLinesNeedRebuild = true;

	/** 선택된 바디의 라인 좌표만 업데이트 필요 (속성 변경 시) */
	bool bSelectedBodyLinesNeedUpdate = false;

	/** 선택 색상만 업데이트 필요 */
	bool bSelectionColorDirty = false;

	/** 이전 선택 상태 (색상 업데이트용) */
	int32 CachedSelectedBodyIndex = -1;
	int32 CachedSelectedConstraintIndex = -1;

	/** 기즈모 드래그 상태 (첫 프레임 감지용) */
	bool bWasGizmoDragging = false;

	/** 바디별 라인 인덱스 범위 */
	struct FBodyLineRange
	{
		int32 StartIndex = 0;
		int32 Count = 0;
	};
	TArray<FBodyLineRange> BodyLineRanges;

	/** 바디 라인 배치 (DOD 기반) */
	FLinesBatch BodyLinesBatch;

	/** 제약 조건 라인 배치 (DOD 기반) */
	FLinesBatch ConstraintLinesBatch;

	/** 제약 조건 삼각형 배치 (면 기반 시각화) */
	FTrianglesBatch ConstraintTrianglesBatch;

	// ────────────────────────────────────────────────
	// 헬퍼 메서드
	// ────────────────────────────────────────────────

	/** 바디 선택 (색상만 업데이트) */
	void SelectBody(int32 BodyIndex)
	{
		SelectedBodyIndex = BodyIndex;
		SelectedConstraintIndex = -1;
		bBodySelectionMode = true;
		bSelectionColorDirty = true;  // 색상만 업데이트
	}

	/** 제약 조건 선택 (색상만 업데이트) */
	void SelectConstraint(int32 ConstraintIndex)
	{
		SelectedConstraintIndex = ConstraintIndex;
		SelectedBodyIndex = -1;
		bBodySelectionMode = false;
		bSelectionColorDirty = true;  // 색상만 업데이트
	}

	/** 선택 해제 (색상만 업데이트) */
	void ClearSelection()
	{
		SelectedBodyIndex = -1;
		SelectedConstraintIndex = -1;
		bSelectionColorDirty = true;  // 색상만 업데이트
	}

	/** 라인 재구성 요청 (바디/제약조건 추가/제거 시) */
	void RequestLinesRebuild()
	{
		bShapeLinesNeedRebuild = true;
	}

	/** 선택된 바디의 라인 좌표 업데이트 요청 (속성 변경 시 - new/delete 없음) */
	void RequestSelectedBodyLinesUpdate()
	{
		bSelectedBodyLinesNeedUpdate = true;
	}

	/** 유효한 선택이 있는지 확인 */
	bool HasSelection() const
	{
		return SelectedBodyIndex >= 0 || SelectedConstraintIndex >= 0;
	}

	/** 프리뷰 액터를 SkeletalMeshActor로 캐스팅 */
	ASkeletalMeshActor* GetPreviewSkeletalActor() const;

	/** 프리뷰 액터의 SkeletalMeshComponent 획득 */
	USkeletalMeshComponent* GetPreviewMeshComponent() const;

	/** 기즈모 숨기기 및 선택 해제 */
	void HideGizmo();
};
