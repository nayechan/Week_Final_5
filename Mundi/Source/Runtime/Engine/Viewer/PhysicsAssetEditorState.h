#pragma once

#include "ViewerState.h"

class UPhysicsAsset;
class ULineComponent;

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

	/** Shape 프리뷰 재구성 필요 플래그 */
	bool bShapePreviewDirty = true;

	// ────────────────────────────────────────────────
	// 헬퍼 메서드
	// ────────────────────────────────────────────────

	/** 바디 선택 */
	void SelectBody(int32 BodyIndex)
	{
		SelectedBodyIndex = BodyIndex;
		SelectedConstraintIndex = -1;
		bBodySelectionMode = true;
		bShapePreviewDirty = true;
	}

	/** 제약 조건 선택 */
	void SelectConstraint(int32 ConstraintIndex)
	{
		SelectedConstraintIndex = ConstraintIndex;
		SelectedBodyIndex = -1;
		bBodySelectionMode = false;
		bShapePreviewDirty = true;
	}

	/** 선택 해제 */
	void ClearSelection()
	{
		SelectedBodyIndex = -1;
		SelectedConstraintIndex = -1;
		bShapePreviewDirty = true;
	}

	/** 유효한 선택이 있는지 확인 */
	bool HasSelection() const
	{
		return SelectedBodyIndex >= 0 || SelectedConstraintIndex >= 0;
	}
};
