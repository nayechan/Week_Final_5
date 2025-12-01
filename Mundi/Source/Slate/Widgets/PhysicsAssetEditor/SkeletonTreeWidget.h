#pragma once

#include "Source/Slate/Widgets/Widget.h"
#include "Delegates.h"
#include <unordered_map>
#include <vector>

struct PhysicsAssetEditorState;

// 델리게이트 타입 정의
DECLARE_DELEGATE_TYPE(FOnAddBody, int32);                    // void(int32 BoneIndex)
DECLARE_DELEGATE_TYPE(FOnRemoveBody, int32);                 // void(int32 BodyIndex)
DECLARE_DELEGATE_TYPE(FOnAddPrimitive, int32, int32);        // void(int32 BodyIndex, int32 PrimitiveType)
DECLARE_DELEGATE_TYPE(FOnAddConstraint, int32, int32);       // void(int32 BodyIndex1, int32 BodyIndex2)
DECLARE_DELEGATE_TYPE(FOnRemoveConstraint, int32);           // void(int32 ConstraintIndex)

/**
 * USkeletonTreeWidget
 *
 * Physics Asset Editor의 좌측 패널
 * 본/바디 계층 구조 트리 렌더링
 *
 * UWidget 기반 인스턴스로 TreeCache를 내부에 캐싱하여
 * 매 프레임 재구축 오버헤드 제거
 */
class USkeletonTreeWidget : public UWidget
{
public:
	DECLARE_CLASS(USkeletonTreeWidget, UWidget)

	USkeletonTreeWidget();
	virtual ~USkeletonTreeWidget() = default;

	// UWidget 인터페이스
	virtual void Initialize() override;
	virtual void Update() override;
	virtual void RenderWidget() override;

	// 외부 상태 연결
	void SetEditorState(PhysicsAssetEditorState* InState) { EditorState = InState; }
	PhysicsAssetEditorState* GetEditorState() const { return EditorState; }

	// 캐시 무효화 요청 (바디/제약조건 추가/제거 시)
	void InvalidateCache() { bCacheValid = false; }

private:
	// ────────────────────────────────────────────────
	// 캐시된 데이터 구조 (매 프레임 O(n²) 방지)
	// ────────────────────────────────────────────────
	struct FTreeCache
	{
		std::unordered_map<int32, std::vector<int32>> ChildrenMap;         // 부모 -> 자식들
		std::unordered_map<int32, int32> BoneToBodyMap;                    // 본 인덱스 -> 바디 인덱스
		std::unordered_map<int32, std::vector<int32>> BoneToConstraintMap; // 본 인덱스 -> 제약조건 인덱스들

		void Clear()
		{
			ChildrenMap.clear();
			BoneToBodyMap.clear();
			BoneToConstraintMap.clear();
		}
	};

	// ────────────────────────────────────────────────
	// 멤버 변수
	// ────────────────────────────────────────────────
	PhysicsAssetEditorState* EditorState = nullptr;
	FTreeCache Cache;
	bool bCacheValid = false;

	// 캐시된 본/바디/제약조건 수 (변경 감지용)
	size_t CachedBoneCount = 0;
	size_t CachedBodyCount = 0;
	size_t CachedConstraintCount = 0;

	// ────────────────────────────────────────────────
	// 내부 메서드
	// ────────────────────────────────────────────────
	void RebuildCache();
	void RenderBoneNode(int32 BoneIndex, int32 Depth);
	void RenderBodyNode(int32 BodyIndex);
	void RenderConstraintNode(int32 ConstraintIndex);

	// Constraint 생성 시 타겟 Body 선택 메뉴
	void RenderConstraintTargetMenu(int32 SourceBodyIndex);

public:
	// ────────────────────────────────────────────────
	// 델리게이트 (컨텍스트 메뉴 액션)
	// ────────────────────────────────────────────────
	FOnAddBody OnAddBody;              // 본에 바디 추가
	FOnRemoveBody OnRemoveBody;        // 바디 삭제
	FOnAddPrimitive OnAddPrimitive;    // 바디에 프리미티브 추가 (0=Box, 1=Sphere, 2=Capsule)
	FOnAddConstraint OnAddConstraint;  // 두 바디 간 제약조건 추가
	FOnRemoveConstraint OnRemoveConstraint;  // 제약조건 삭제
};
