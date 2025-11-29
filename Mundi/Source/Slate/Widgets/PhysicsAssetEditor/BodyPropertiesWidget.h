#pragma once

struct PhysicsAssetEditorState;
struct FBodySetup;

/**
 * BodyPropertiesWidget
 *
 * Physics Asset Editor의 우측 패널 (바디 선택 시)
 * 바디 Shape 및 물리 속성 편집 UI
 */
class BodyPropertiesWidget
{
public:
	// 바디 속성 전체 렌더링
	// 반환값: 속성이 변경되었으면 true
	static bool Render(PhysicsAssetEditorState* State, FBodySetup& Body);

private:
	// Shape 속성 렌더링
	static bool RenderShapeProperties(PhysicsAssetEditorState* State, FBodySetup& Body);

	// 물리 속성 렌더링
	static bool RenderPhysicsProperties(PhysicsAssetEditorState* State, FBodySetup& Body);
};
