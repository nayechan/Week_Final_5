#pragma once

class ViewerState;
class UWorld;
struct ID3D11Device;
class UEditorAssetPreviewContext;
class UPhysicsAsset;

/**
 * PhysicsAssetEditorBootstrap
 *
 * Physics Asset Editor의 ViewerState 생성/소멸 및 파일 I/O 담당
 * ParticleEditorBootstrap 패턴 준수
 */
class PhysicsAssetEditorBootstrap
{
public:
	/**
	 * ViewerState 생성
	 * @param Name 탭 이름
	 * @param InWorld 월드 (unused, 내부에서 프리뷰 월드 생성)
	 * @param InDevice D3D11 디바이스
	 * @param Context 에셋 프리뷰 컨텍스트 (AssetPath 포함)
	 * @return 생성된 PhysicsAssetEditorState
	 */
	static ViewerState* CreateViewerState(const char* Name, UWorld* InWorld,
		ID3D11Device* InDevice, UEditorAssetPreviewContext* Context);

	/**
	 * ViewerState 소멸
	 * @param State 소멸할 ViewerState (nullptr로 설정됨)
	 */
	static void DestroyViewerState(ViewerState*& State);

	/**
	 * 기본 Physics Asset 생성 (빈 에셋)
	 * @return 새로 생성된 UPhysicsAsset
	 */
	static UPhysicsAsset* CreateDefaultPhysicsAsset();

	/**
	 * Physics Asset을 JSON 파일로 저장
	 * @param Asset 저장할 Physics Asset
	 * @param FilePath 저장 경로
	 * @return 성공 여부
	 */
	static bool SavePhysicsAsset(UPhysicsAsset* Asset, const FString& FilePath);

	/**
	 * JSON 파일에서 Physics Asset 로드
	 * @param FilePath 로드할 파일 경로
	 * @return 로드된 UPhysicsAsset (실패 시 nullptr)
	 */
	static UPhysicsAsset* LoadPhysicsAsset(const FString& FilePath);
};
