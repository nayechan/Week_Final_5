#pragma once
#include "UEContainer.h"

struct FPlatformProcess
{
    // 기본 연결 프로그램으로 파일 열기
    static void OpenFileInDefaultEditor(const FWideString& RelativePath);

    // 파일 저장 위치 반환
    static std::filesystem::path OpenSaveFileDialog(const FWideString BaseDir, const FWideString Extension, const FWideString Description, const FWideString DefaultFileName = L"");
    
    static std::filesystem::path OpenLoadFileDialog(const FWideString BaseDir, const FWideString Extension, const FWideString Description);

    // 나중에 추가될 수 있는 기능들...
    // static void OpenDirectory(const FString& DirectoryPath);
    // static void CopyToClipboard(const FString& Text);
};