#include "pch.h"

TMap<FString, FString> EditorINI;

// Global client area size used by various modules
float CLIENTWIDTH = 1024.0f;
float CLIENTHEIGHT = 1024.0f;

#ifdef _EDITOR
UEditorEngine GEngine;
#endif

#ifdef _GAME
UGameEngine GEngine;
#endif

UWorld* GWorld = nullptr;