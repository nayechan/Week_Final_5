// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "ContentBrowserWindow.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "SlateManager.h"
#include "ThumbnailManager.h"
#include "Source/Runtime/Engine/Viewer/EditorAssetPreviewContext.h"
#include <algorithm>

IMPLEMENT_CLASS(UContentBrowserWindow)

UContentBrowserWindow::UContentBrowserWindow()
	: SelectedFile(nullptr)
	, SelectedIndex(-1)
	, LastClickTime(0.0)
	, LastClickedIndex(-1)
	, ThumbnailSize(80.0f)
	, ColumnsCount(4)
	, LeftPanelWidth(250.0f)
	, SplitterWidth(4.0f)
	, bIsDraggingSplitter(false)
	, bShowFolders(true)
	, bShowFiles(true)
	, bUseThumbnails(true)
{
	FUIWindowConfig Config;
	Config.WindowTitle = "Content Browser";
	Config.DefaultSize = ImVec2(800, 400);
	Config.DefaultPosition = ImVec2(100, 500);
	Config.MinSize = ImVec2(400, 300);
	Config.bResizable = true;
	Config.bMovable = true;
	Config.bCollapsible = true;
	Config.DockDirection = EUIDockDirection::Bottom;

	Config.UpdateWindowFlags();
	SetConfig(Config);

	// 루트 경로를 Data 폴더로 설정
	RootPath = std::filesystem::current_path() / "Data";
	CurrentPath = RootPath;
}

UContentBrowserWindow::~UContentBrowserWindow()
{
	Cleanup();
}

void UContentBrowserWindow::Initialize()
{
	UE_LOG("ContentBrowserWindow: Successfully Initialized");

	// UIWindow 부모 Initialize 호출
	UUIWindow::Initialize();

	RefreshCurrentDirectory();
}

void UContentBrowserWindow::RenderWindow()
{
	// 숨겨진 상태면 렌더링하지 않음
	if (!IsVisible())
	{
		return;
	}

	// ImGui 윈도우 시작
	ImGui::SetNextWindowSize(GetConfig().DefaultSize, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowPos(GetConfig().DefaultPosition, ImGuiCond_FirstUseEver);

	bool bIsOpen = true;

	if (ImGui::Begin(GetConfig().WindowTitle.c_str(), &bIsOpen, GetConfig().WindowFlags))
	{
		// 실제 UI 컨텐츠 렌더링
		RenderPathBar();
		RenderContentGrid();
	}
	ImGui::End();

	// 윈도우가 닫혔는지 확인
	if (!bIsOpen)
	{
		SetWindowState(EUIWindowState::Hidden);
	}
}

void UContentBrowserWindow::Cleanup()
{
	DisplayedFiles.clear();
	SelectedFile = nullptr;
}

void UContentBrowserWindow::NavigateToPath(const std::filesystem::path& NewPath)
{
	if (std::filesystem::exists(NewPath) && std::filesystem::is_directory(NewPath))
	{
		CurrentPath = NewPath;
		RefreshCurrentDirectory();
		SelectedIndex = -1;
		SelectedFile = nullptr;
	}
}

void UContentBrowserWindow::RefreshCurrentDirectory()
{
	DisplayedFiles.clear();

	if (!std::filesystem::exists(CurrentPath))
	{
		UE_LOG("ContentBrowserWindow: Path does not exist: %s", WideToUTF8(CurrentPath.wstring()).c_str());
		CurrentPath = RootPath;
		return;
	}

	try
	{
		// 오른쪽 패널에는 파일만 표시 (폴더는 왼쪽 트리에 표시됨)
		if (bShowFiles)
		{
			for (const auto& entry : std::filesystem::directory_iterator(CurrentPath))
			{
				if (entry.is_regular_file())
				{
					FFileEntry FileEntry;
					FileEntry.Path = entry.path();
					FileEntry.FileName = WideToUTF8(entry.path().filename().wstring());
					FileEntry.Extension = WideToUTF8(entry.path().extension().wstring());
					FileEntry.bIsDirectory = false;
					FileEntry.FileSize = std::filesystem::file_size(entry.path());
					DisplayedFiles.push_back(FileEntry);
				}
			}
		}

		UE_LOG("ContentBrowserWindow: Loaded %d items from %s", DisplayedFiles.size(), WideToUTF8(CurrentPath.wstring()).c_str());
	}
	catch (const std::exception& e)
	{
		UE_LOG("ContentBrowserWindow: Error reading directory: %s", e.what());
	}
}

void UContentBrowserWindow::RenderContent()
{
	ImVec2 contentSize = ImGui::GetContentRegionAvail();
	ImGuiStyle& style = ImGui::GetStyle();

	// 왼쪽 패널 (폴더 트리)
	ImGui::BeginChild("LeftPanel", ImVec2(LeftPanelWidth, contentSize.y), true);
	RenderFolderTree();
	ImGui::EndChild();

	ImGui::SameLine();

	// 스플리터
	ImGui::Button("##Splitter", ImVec2(SplitterWidth, contentSize.y));
	if (ImGui::IsItemActive())
	{
		bIsDraggingSplitter = true;
	}
	if (bIsDraggingSplitter)
	{
		float delta = ImGui::GetIO().MouseDelta.x;
		LeftPanelWidth += delta;
		LeftPanelWidth = std::max(150.0f, std::min(LeftPanelWidth, contentSize.x - 300.0f));

		if (!ImGui::IsMouseDown(0))
		{
			bIsDraggingSplitter = false;
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}

	ImGui::SameLine();

	// 오른쪽 패널 (파일 그리드)
	float rightPanelWidth = contentSize.x - LeftPanelWidth - SplitterWidth - style.ItemSpacing.x * 2;
	ImGui::BeginChild("RightPanel", ImVec2(rightPanelWidth, contentSize.y), true);

	// 경로 바
	RenderPathBar();

	ImGui::Separator();

	// 파일 그리드
	RenderContentGrid();

	ImGui::EndChild();
}

void UContentBrowserWindow::RenderFolderTree()
{
	ImGui::Text("Folders");
	ImGui::Separator();

	// 루트 폴더부터 재귀적으로 렌더링
	if (std::filesystem::exists(RootPath))
	{
		RenderFolderTreeNode(RootPath);
	}
}

void UContentBrowserWindow::RenderFolderTreeNode(const std::filesystem::path& FolderPath)
{
	try
	{
		// 폴더 이름 가져오기
		FString folderName = FolderPath == RootPath ? "Data" : WideToUTF8(FolderPath.filename().wstring());

		// 하위 폴더 확인
		bool hasSubFolders = false;
		for (const auto& entry : std::filesystem::directory_iterator(FolderPath))
		{
			if (entry.is_directory())
			{
				hasSubFolders = true;
				break;
			}
		}

		// 트리 노드 플래그
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
		if (!hasSubFolders)
		{
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		if (FolderPath == CurrentPath)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// 트리 노드 렌더링
		bool nodeOpen = ImGui::TreeNodeEx(folderName.c_str(), flags);

		// 클릭 시 해당 폴더로 이동
		if (ImGui::IsItemClicked())
		{
			NavigateToPath(FolderPath);
		}

		// 하위 폴더 렌더링
		if (nodeOpen && hasSubFolders)
		{
			for (const auto& entry : std::filesystem::directory_iterator(FolderPath))
			{
				if (entry.is_directory())
				{
					RenderFolderTreeNode(entry.path());
				}
			}
			ImGui::TreePop();
		}
	}
	catch (const std::exception& e)
	{
		UE_LOG("Error rendering folder tree node: %s", e.what());
	}
}

void UContentBrowserWindow::RenderPathBar()
{
	ImGui::Text("Path: ");
	ImGui::SameLine();

	// 뒤로 가기 버튼
	if (CurrentPath != RootPath)
	{
		if (ImGui::Button("<- Back"))
		{
			NavigateToPath(CurrentPath.parent_path());
		}
		ImGui::SameLine();
	}

	// 현재 경로 표시
	FString PathStr = WideToUTF8(CurrentPath.wstring());
	ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "%s", PathStr.c_str());

	// 새로고침 버튼
	ImGui::SameLine(ImGui::GetWindowWidth() - 100);
	if (ImGui::Button("Refresh"))
	{
		RefreshCurrentDirectory();
	}

	ImGui::Separator();
}

void UContentBrowserWindow::RenderContentGrid()
{
	ImGuiStyle& style = ImGui::GetStyle();
	float windowWidth = ImGui::GetContentRegionAvail().x;
	float cellSize = ThumbnailSize + style.ItemSpacing.x * 2;

	// 동적으로 컬럼 수 계산 (최소 2개 이상)
	ColumnsCount = std::max(2, (int)(windowWidth / cellSize));

	ImGui::BeginChild("ContentArea", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

	// 그리드 레이아웃으로 아이템 배치
	for (int i = 0; i < DisplayedFiles.size(); ++i)
	{
		FFileEntry& entry = DisplayedFiles[i];

		ImGui::PushID(i);

		// 같은 줄에 배치 (첫 번째 아이템이 아닌 경우)
		if (i > 0)
		{
			int columnIndex = i % ColumnsCount;
			if (columnIndex != 0)
			{
				ImGui::SameLine();
			}
		}

		RenderFileItem(entry, i, bUseThumbnails);
		ImGui::PopID();
	}

	ImGui::EndChild();
}

void UContentBrowserWindow::RenderFileItem(FFileEntry& Entry, int Index, bool bUseThumbnails)
{
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec2 buttonSize(ThumbnailSize, ThumbnailSize);

	// 그룹 시작 - 버튼과 텍스트를 하나의 단위로 묶음
	ImGui::BeginGroup();

	// 선택 상태에 따라 색상 변경
	bool isSelected = (Index == SelectedIndex);
	if (isSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.6f, 0.9f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
	}

	bool clicked = false;

	// 썸네일 사용 여부에 따라 다른 방식으로 렌더링
	if (bUseThumbnails)
	{
		// 썸네일 가져오기
		std::string pathStr = WideToUTF8(Entry.Path.wstring());
		ID3D11ShaderResourceView* thumbnailSRV = FThumbnailManager::GetInstance().GetThumbnail(pathStr);

		if (thumbnailSRV)
		{
			// ImGui::ImageButton으로 썸네일 표시
			char buttonID[32];
			sprintf_s(buttonID, "##thumbnail_%d", Index);
			clicked = ImGui::ImageButton(buttonID, (void*)thumbnailSRV, buttonSize);
		}
		else
		{
			// 썸네일이 없으면 텍스트 버튼으로 대체
			const char* icon = GetIconForFile(Entry);
			clicked = ImGui::Button(icon, buttonSize);
		}
	}
	else
	{
		// 텍스트 아이콘으로 표시
		const char* icon = GetIconForFile(Entry);
		clicked = ImGui::Button(icon, buttonSize);
	}

	// 클릭 처리
	if (clicked)
	{
		SelectedIndex = Index;
		SelectedFile = &Entry;

		// 더블클릭 감지
		double currentTime = ImGui::GetTime();
		if (LastClickedIndex == Index && (currentTime - LastClickTime) < 0.3)
		{
			HandleDoubleClick(Entry);
			LastClickTime = 0.0; // 더블클릭 처리 후 리셋
		}
		else
		{
			LastClickTime = currentTime;
			LastClickedIndex = Index;
		}
	}

	if (isSelected)
	{
		ImGui::PopStyleColor(3);
	}

	// 파일 이름 표시 (텍스트 줄바꿈)
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ThumbnailSize);
	ImGui::TextWrapped("%s", Entry.FileName.c_str());
	ImGui::PopTextWrapPos();

	// 그룹 종료
	ImGui::EndGroup();

	// 그룹 전체를 드래그 소스로 처리 (EndGroup 이후에 호출)
	HandleDragSource(Entry);
}

void UContentBrowserWindow::HandleDragSource(FFileEntry& Entry)
{
	if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
	{
		// 파일 경로를 페이로드로 전달
		std::string pathStr = WideToUTF8(Entry.Path.wstring());
		ImGui::SetDragDropPayload("ASSET_FILE", pathStr.c_str(), pathStr.size() + 1);

		// 드래그 중 표시할 툴팁
		ImGui::Text("Move %s", Entry.FileName.c_str());
		if (!Entry.Extension.empty())
		{
			ImGui::Text("Type: %s", Entry.Extension.c_str());
		}

		ImGui::EndDragDropSource();
	}
}

void UContentBrowserWindow::HandleDoubleClick(FFileEntry& Entry)
{
	// 파일인 경우 뷰어 열기
	UE_LOG("ContentBrowserWindow: Double-clicked file: %s", Entry.FileName.c_str());

	// 확장자에 따라 적절한 뷰어 열기
	std::string ext = Entry.Extension.c_str();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".fbx")
	{
		// SkeletalMeshViewer 열기
		std::string pathStr = WideToUTF8(Entry.Path.wstring());
		//USlateManager::GetInstance().OpenSkeletalMeshViewerWithFile(pathStr.c_str());
		UE_LOG("Opening SkeletalMeshViewer for: %s", Entry.FileName.c_str());
	}
	else if (ext == ".obj")
	{
		// StaticMesh는 현재 전용 뷰어가 없으므로 로그만 출력
		UE_LOG("StaticMesh file clicked: %s (No dedicated viewer yet)", Entry.FileName.c_str());
	}
	else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga")
	{
		// 텍스처 뷰어 (향후 구현)
		UE_LOG("Texture file clicked: %s (Texture viewer not implemented yet)", Entry.FileName.c_str());
	}
	else if (ext == ".particle")
	{
		// 파티클 에디터 열기
		std::string pathStr = WideToUTF8(Entry.Path.wstring());
		UEditorAssetPreviewContext* Context = NewObject<UEditorAssetPreviewContext>();
		Context->ViewerType = EViewerType::Particle;
		Context->AssetPath = pathStr.c_str();
		USlateManager::GetInstance().OpenAssetViewer(Context);
		UE_LOG("Opening ParticleEditor for: %s", Entry.FileName.c_str());
	}
	else if (ext == ".phxmtl")
	{
		// @note 에디터 등에서는 UResourceManager::Load()를 상대경로로 사용한다.
		// 따라서, 일관된 방식을 사용하지 않는다면 캐싱된 객체가 아닌 새로 생성된 객체를 참조하게 되는 문제가 발생한다.
		std::filesystem::path relativePath = std::filesystem::relative(Entry.Path, std::filesystem::current_path());
		std::string pathStr = WideToUTF8(relativePath.wstring());
        
		std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

		UEditorAssetPreviewContext* Context = NewObject<UEditorAssetPreviewContext>();
		Context->ViewerType = EViewerType::PhysicalMaterial;
		Context->AssetPath = pathStr; 
        
		USlateManager::GetInstance().OpenAssetViewer(Context);
		UE_LOG("Opening PhysicalMaterialEditor for: %s", pathStr.c_str());
	}
	else
	{
		UE_LOG("Unsupported file type: %s", ext.c_str());
	}
}

const char* UContentBrowserWindow::GetIconForFile(const FFileEntry& Entry) const
{
	if (Entry.bIsDirectory)
	{
		return "[DIR]";
	}

	// 확장자에 따라 아이콘 반환
	std::string ext = Entry.Extension.c_str();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == ".prefab")
	{
		return "[PREFAB]";
	}
	else if (ext == ".fbx" || ext == ".obj")
	{
		return "[MESH]";
	}
	else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga")
	{
		return "[IMG]";
	}
	else if (ext == ".hlsl" || ext == ".glsl" || ext == ".fx")
	{
		return "[SHDR]";
	}
	else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg")
	{
		return "[SND]";
	}
	else if (ext == ".mat")
	{
		return "[MAT]";
	}
	else if (ext == ".particle")
	{
		return "[PTCL]";
	}
	else if (ext == ".level" || ext == ".json")
	{
		return "[DATA]";
	}

	return "[FILE]";
}

FString UContentBrowserWindow::FormatFileSize(uintmax_t Size) const
{
	const char* units[] = { "B", "KB", "MB", "GB" };
	int unitIndex = 0;
	double size = static_cast<double>(Size);

	while (size >= 1024.0 && unitIndex < 3)
	{
		size /= 1024.0;
		unitIndex++;
	}

	char buffer[64];
	sprintf_s(buffer, "%.2f %s", size, units[unitIndex]);
	return FString(buffer);
}
