#pragma once

struct PhysicsAssetEditorState;
struct FConstraintSetup;

/**
 * ConstraintPropertiesWidget
 *
 * Physics Asset Editor의 우측 패널 (제약 조건 선택 시)
 * 제약 조건 타입 및 각도 제한 편집 UI
 */
class ConstraintPropertiesWidget
{
public:
	// 제약 조건 속성 전체 렌더링
	// 반환값: 속성이 변경되었으면 true
	static bool Render(PhysicsAssetEditorState* State, FConstraintSetup& Constraint);

private:
	// 각도 제한 렌더링
	static bool RenderLimitProperties(PhysicsAssetEditorState* State, FConstraintSetup& Constraint);
};
