#pragma once
#include "UEContainer.h"

struct FPlatformProcess
{
    // 기본 연결 프로그램으로 파일 열기
    static void OpenFileInDefaultEditor(const FString& RelativePath);

    // 나중에 추가될 수 있는 기능들...
    // static void OpenDirectory(const FString& DirectoryPath);
    // static void CopyToClipboard(const FString& Text);
};