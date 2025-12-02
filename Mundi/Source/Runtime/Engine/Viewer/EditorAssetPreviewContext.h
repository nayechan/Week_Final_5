#pragma once
#include "Object.h"

class USkeletalMesh;
class SWindow;
class UEditorAssetPreviewContext : public UObject
{
public:
	DECLARE_CLASS(UEditorAssetPreviewContext, UObject)

	UEditorAssetPreviewContext();

	USkeletalMesh* SkeletalMesh;
	TArray<SWindow*> ListeningWindows;
	EViewerType ViewerType = EViewerType::None;
	FString AssetPath;

	// Physics Asset Editor 전용: 스켈레탈 메시 경로와 피직스 에셋 경로 분리
	FString SkeletalMeshPath;    // 프리뷰용 스켈레탈 메시 경로
	FString PhysicsAssetPath;    // 저장/불러오기용 피직스 에셋 경로
};