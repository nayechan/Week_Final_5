#pragma once

#include "Source/Slate/Widgets/Widget.h"

struct PhysicsAssetEditorState;
class SPhysicsAssetEditorWindow;

/**
 * UToolsWidget
 *
 * Physics Asset Editor의 Tools 패널 (우측 하단)
 * Body 생성 관련 버튼 및 옵션 제공
 *
 * 선택 상태에 따라 동적으로 버튼 텍스트 변경:
 * - 선택 없음        -> "Generate All Bodies"
 * - 본 선택 (바디 X) -> "Add Bodies"
 * - 바디 선택        -> "Re-generate Bodies"
 */
class UToolsWidget : public UWidget
{
public:
	DECLARE_CLASS(UToolsWidget, UWidget)

	UToolsWidget();
	virtual ~UToolsWidget() = default;

	// UWidget 인터페이스
	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

	// 외부 상태 연결
	void SetEditorState(PhysicsAssetEditorState* InState) { EditorState = InState; }
	void SetEditorWindow(SPhysicsAssetEditorWindow* InWindow) { EditorWindow = InWindow; }

	PhysicsAssetEditorState* GetEditorState() const { return EditorState; }

private:
	// 섹션 렌더링
	void RenderGenerationSection();

	// 선택 상태에 따른 버튼 텍스트
	const char* GetGenerateButtonText() const;
	bool CanGenerate() const;

	PhysicsAssetEditorState* EditorState = nullptr;
	SPhysicsAssetEditorWindow* EditorWindow = nullptr;
};
