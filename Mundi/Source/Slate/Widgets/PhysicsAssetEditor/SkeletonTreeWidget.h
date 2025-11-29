#pragma once

struct PhysicsAssetEditorState;

/**
 * SkeletonTreeWidget
 *
 * Physics Asset Editor의 좌측 패널
 * 본/바디 계층 구조 트리 렌더링
 */
class SkeletonTreeWidget
{
public:
	// 본/바디 트리 전체 렌더링
	static void Render(PhysicsAssetEditorState* State);

private:
	// 개별 본 노드 렌더링 (재귀)
	static void RenderBoneNode(PhysicsAssetEditorState* State, int32 BoneIndex, int32 Depth);

	// 바디 노드 렌더링
	static void RenderBodyNode(PhysicsAssetEditorState* State, int32 BodyIndex);

	// 제약 조건 노드 렌더링
	static void RenderConstraintNode(PhysicsAssetEditorState* State, int32 ConstraintIndex);
};
