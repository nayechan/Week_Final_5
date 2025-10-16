#include "pch.h"
#include "SceneIOWidget.h"
#include "ObjectFactory.h"
#include "GlobalConsole.h"
#include "ImGui/imgui.h"
#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <exception>
#include "UIManager.h"
#include "Object.h"
#include "Level.h"
#include "CameraActor.h"
#include "CameraComponent.h"
#include "JsonSerializer.h"

IMPLEMENT_CLASS(USceneIOWidget)

USceneIOWidget::USceneIOWidget()
	: UWidget("Scene IO Widget")
	, StatusMessageTimer(0.0f)
	, bIsStatusError(false)
{
	strcpy_s(NewLevelNameBuffer, "Default");
}

USceneIOWidget::~USceneIOWidget() = default;

void USceneIOWidget::Initialize()
{
	UE_LOG("SceneIOWidget: Successfully Initialized");
}

void USceneIOWidget::Update()
{
	// Update status message timer
	if (StatusMessageTimer > 0.0f)
	{
		// Use a fixed delta time for now
		float DeltaTime = 1.0f / 60.0f; // Assume 60 FPS
		UpdateStatusMessage(DeltaTime);
	}
}

void USceneIOWidget::RenderWidget()
{
	ImGui::Text("Scene Management");
	ImGui::Separator();
	
	RenderSaveLoadSection();
	ImGui::Spacing();
	
	RenderStatusMessage();
}

void USceneIOWidget::RenderSaveLoadSection()
{
	// 공용 씬 이름 입력 (Scene/Name.Scene 으로 강제 저장/로드)
	ImGui::SetNextItemWidth(220);
	ImGui::InputText("Scene Name", NewLevelNameBuffer, sizeof(NewLevelNameBuffer));

	// 버튼들: Save / Quick Save / Load / Quick Load / New Scene
	if (ImGui::Button("Save Scene", ImVec2(110, 25)))
	{
		// 유효성 검사
		FString SceneName = NewLevelNameBuffer;
		bool bValid = false;
		for (char c : SceneName)
		{
			if (c != ' ' && c != '\t')
			{
				bValid = true;
				break;
			}
		}

		if (!bValid)
		{
			SetStatusMessage("Please enter a valid scene name!", true);
		}
		else
		{
			// 파일 다이얼로그 없이 이름만 전달 → FSceneLoader::Save가 Scene/Name.Scene으로 저장
			SaveLevel(SceneName);
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Quick Save", ImVec2(110, 25)))
	{
		SaveLevel("");
	}

	
	if (ImGui::Button("Load Scene", ImVec2(110, 25)))
	{
		// 유효성 검사
		FString SceneName = NewLevelNameBuffer;
		bool bValid = false;
		for (char c : SceneName)
		{
			if (c != ' ' && c != '\t')
			{
				bValid = true;
				break;
			}
		}

		if (!bValid)
		{
			SetStatusMessage("Please enter a valid scene name!", true);
		}
		else
		{
			// Scene/Name.Scene 경로 구성 및 존재 확인
			namespace fs = std::filesystem;
			fs::path path = fs::path("Scene") / SceneName;
			if (path.extension().string() != ".Scene")
			{
				path.replace_extension(".Scene");
			}

			if (!fs::exists(path))
			{
				SetStatusMessage("Scene file not found: " + path.string(), true);
			}
			else
			{
				// 절대/정규 경로 전달 → LoadLevel에서 NextUUID 읽기/로드 처리
				LoadLevel(path.make_preferred().string());
			}
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Quick Load", ImVec2(110, 25)))
	{
		namespace fs = std::filesystem;
		fs::path quick = fs::path("Scene") / "QuickSave";
		if (quick.extension().string() != ".Scene")
		{
			quick.replace_extension(".Scene");
		}

		if (!fs::exists(quick))
		{
			SetStatusMessage("QuickSave not found: " + quick.make_preferred().string(), true);
		}
		else
		{
			LoadLevel(quick.make_preferred().string());
		}
	}

	if (ImGui::Button("New Scene", ImVec2(110, 25)))
	{
		CreateNewLevel();
	}
}

void USceneIOWidget::RenderStatusMessage()
{
	if (StatusMessageTimer > 0.0f)
	{
		ImVec4 color = bIsStatusError ? 
			ImVec4(1.0f, 0.4f, 0.4f, 1.0f) :  // Red for errors
			ImVec4(0.0f, 1.0f, 0.0f, 1.0f);   // Green for success
			
		ImGui::TextColored(color, "%s", StatusMessage.c_str());
	}
	else
	{
		// Reserve space to prevent UI jumping
		ImGui::Text(" "); // Empty line
	}
}

void USceneIOWidget::SetStatusMessage(const FString& Message, bool bIsError)
{
	StatusMessage = Message;
	StatusMessageTimer = STATUS_MESSAGE_DURATION;
	bIsStatusError = bIsError;
}

void USceneIOWidget::UpdateStatusMessage(float DeltaTime)
{
	StatusMessageTimer -= DeltaTime;
	if (StatusMessageTimer < 0.0f)
	{
		StatusMessageTimer = 0.0f;
	}
}

/**
 * @brief Save current scene to specified file path
 * @param InFilePath File path to save to (empty for quick save)
 */
void USceneIOWidget::SaveLevel(const FString& InSceneName)
{
	try
	{
		UWorld* CurrentWorld = UUIManager::GetInstance().GetWorld();
		if (!CurrentWorld)
		{
			SetStatusMessage("Cannot find World!", true);
			return;
		}

		if (InSceneName.empty())
		{
			// Quick Save: 이름만 넘김. Scene 경로/확장자는 FSceneLoader::Save가 처리
			/*ULevelService::SaveLevel(CurrentWorld->GetLevel(), CurrentWorld->GetCameraActor(), "QuickSave");
			UE_LOG("SceneIO: Quick Save executed to Scene/QuickSave.Scene");
			SetStatusMessage("Quick Save completed: Scene/QuickSave.Scene");*/
		}
		else
		{
			// 파일 경로에서 베이스 이름만 추출하여 넘김
			/*FString SceneName = InSceneName;
			size_t LastSlash = SceneName.find_last_of("\\/");
			if (LastSlash != std::string::npos) 
			{
				SceneName = SceneName.substr(LastSlash + 1);
			}
			size_t LastDot = SceneName.find_last_of(".");
			{
				if (LastDot != std::string::npos) SceneName = SceneName.substr(0, LastDot);
			}*/

			FString FilePath = "Scene/" + InSceneName + ".Scene";

			//ULevelService::SaveLevel(CurrentWorld->GetLevel(), CurrentWorld->GetCameraActor(), SceneName);
			JSON LevelJson;
			CurrentWorld->GetLevel()->Serialize(false, LevelJson);
			bool bSuccess = FJsonSerializer::SaveJsonToFile(LevelJson, FilePath);

			UE_LOG("SceneIO: Scene saved: %s", InSceneName.c_str());
			SetStatusMessage("Scene saved: Scene/" + InSceneName + ".Scene");
		}
	}
	catch (const std::exception& Exception)
	{
		FString ErrorMsg = FString("Save Error: ") + Exception.what();
		SetStatusMessage(ErrorMsg, true);
		UE_LOG("SceneIO: Save Error: %s", Exception.what());
	}
}

/**
 * @brief Load scene from specified file path
 * @param InFilePath File path to load from
 */
void USceneIOWidget::LoadLevel(const FString& InFilePath)
{
	try
	{
		// 파일명에서 씬 이름 추출
		FString SceneName = InFilePath;
		size_t LastSlash = SceneName.find_last_of("\\/");
		if (LastSlash != std::string::npos)
		{
			SceneName = SceneName.substr(LastSlash + 1);
		}
		size_t LastDot = SceneName.find_last_of(".");
		if (LastDot != std::string::npos)
		{
			SceneName = SceneName.substr(0, LastDot);
		}

		// World 가져오기
		UWorld* CurrentWorld = UUIManager::GetInstance().GetWorld();
		if (!CurrentWorld)
		{
			SetStatusMessage("Cannot find World!", true);
			return;
		}

		// 로드 직전: Transform 위젯/선택 초기화
		UUIManager::GetInstance().ClearTransformWidgetSelection();
		UUIManager::GetInstance().ResetPickedActor();

		// 2) 레벨 서비스로 로드 후 월드에 적용

		//FLoadedLevel Loaded = ULevelService::LoadLevel(SceneName);

		std::unique_ptr<ULevel> NewLevel = ULevelService::CreateNewLevel();
		//FString FilePath = "Scene/" + InFilePath + ".Scene";
		JSON LevelJsonData;
		if (FJsonSerializer::LoadJsonFromFile(LevelJsonData, InFilePath))
		{
			NewLevel->Serialize(true, LevelJsonData);
		}
		else
		{
			UE_LOG("LevelManager: Failed To Load Level From: %s", InFilePath.c_str());
			return;
		}
		CurrentWorld->SetLevel(std::move(NewLevel));

		UE_LOG("SceneIO: Scene loaded successfully: %s", InFilePath.c_str());
		SetStatusMessage("Scene loaded successfully: " + InFilePath);
	}
	catch (const std::exception& Exception)
	{
		FString ErrorMsg = FString("Load Error: ") + Exception.what();
		SetStatusMessage(ErrorMsg, true);
		UE_LOG("SceneIO: Load Error: %s", Exception.what());
	}
}

/**
 * @brief Create a new blank scene
 */
void USceneIOWidget::CreateNewLevel()
{
	try
	{
		// GetInstance World reference
		UWorld* CurrentWorld = UUIManager::GetInstance().GetWorld();
		if (!CurrentWorld)
		{
			SetStatusMessage("Cannot find World!", true);
			return;
		}

		// 로드 직전: Transform 위젯/선택 초기화
		UUIManager::GetInstance().ClearTransformWidgetSelection();
		UUIManager::GetInstance().ResetPickedActor();

		// 새 레벨 생성 후 월드에 적용
		CurrentWorld->SetLevel(ULevelService::CreateNewLevel());

		UE_LOG("SceneIO: New scene created");
		SetStatusMessage("New scene created");
	}
	catch (const std::exception& Exception)
	{
		FString ErrorMsg = FString("Create Error: ") + Exception.what();
		SetStatusMessage(ErrorMsg, true);
		UE_LOG("SceneIO: Create Error: %s", Exception.what());
	}
}

/**
 * @brief Windows API를 활용한 파일 저장 Dialog Modal을 생성하는 UI Window 기능
 * @return 선택된 파일 경로 (취소 시 빈 문자열)
 */
path USceneIOWidget::OpenSaveFileDialog()
{
     OPENFILENAMEW ofn;
     wchar_t szFile[260] = {};
   
     // 기본 파일명 설정
     wcscpy_s(szFile, L"");
   
     // Initialize OPENFILENAME
     ZeroMemory(&ofn, sizeof(ofn));
     ofn.lStructSize = sizeof(ofn);
     ofn.hwndOwner = GetActiveWindow();  // 현재 활성 윈도우를 부모로 설정
     ofn.lpstrFile = szFile;
     ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
     ofn.lpstrFilter = L"JSON Files\0*.scene\0All Files\0*.*\0";
     ofn.nFilterIndex = 1;
     ofn.lpstrFileTitle = nullptr;
     ofn.nMaxFileTitle = 0;
     ofn.lpstrInitialDir = nullptr;
     ofn.lpstrTitle = L"Save Level File";
	 ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
     ofn.lpstrDefExt = L"json";
   
     // Modal 다이얼로그 표시 - 이 함수가 리턴될 때까지 다른 입력 차단
     UE_LOG("SceneIO: Opening Save Dialog (Modal)...");
   
     if (GetSaveFileNameW(&ofn) == TRUE)
     {
         UE_LOG("SceneIO: Save Dialog Closed");
         return path(szFile);
     }
   
    UE_LOG("SceneIO: Save Dialog Closed");
   return L"";
}

/**
 * @brief Windows API를 사용하여 파일 열기 다이얼로그를 엽니다
 * @return 선택된 파일 경로 (취소 시 빈 문자열)
 */
path USceneIOWidget::OpenLoadFileDialog()
{
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {};

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();  // 현재 활성 윈도우를 부모로 설정
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = L"JSON Files\0*.scene\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.lpstrTitle = L"Load Level File";
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXPLORER | OFN_HIDEREADONLY | OFN_NOCHANGEDIR;

    UE_LOG("SceneIO: Opening Load Dialog (Modal)...");

    if (GetOpenFileNameW(&ofn) == TRUE)
    {
        UE_LOG("SceneIO: Load Dialog Closed");
        return path(szFile);
    }

    UE_LOG("SceneIO: Load Dialog Closed");
    return L"";
}
