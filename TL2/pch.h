#pragma once

// Linker
#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

// Standard Library (MUST come before UEContainer.h)
#include <vector>
#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <list>
#include <deque>
#include <string>
#include <array>
#include <algorithm>
#include <functional>
#include <memory>
#include <cmath>
#include <limits>
#include <iostream>
#include <fstream>
#include <utility>
#include <filesystem>
#include <sstream>
#include <iterator>

// Windows & DirectX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <cassert>

// Core Project Headers
#include "VertexData.h"
#include "UEContainer.h"
#include "Vector.h"
#include "Name.h"
#include "Object.h"
#include "Enums.h"
#include "UI/GlobalConsole.h"
#include "D3D11RHI.h"
#include "ObjectFactory.h"
#include "World.h"
// d3dtk
#include "d3dtk/SimpleMath.h"

// ImGui
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

// nlohmann
#include "nlohmann/json.hpp"

//Manager
#include "Renderer.h"
#include "InputManager.h"
#include "UI/UIManager.h"
#include "ResourceManager.h"


#define RESOURCE UResourceManager::GetInstance()
#define UI UUIManager::GetInstance()
#define INPUT UInputManager::GetInstance()
#define RENDER URenderManager::GetInstance()
#define SLATE USlateManager::GetInstance()

//(월드 별 소유)
//#define PARTITION UWorldPartitionManager::GetInstance()
//#define SELECTION (GEngine.GetDefaultWorld()->GetSelectionManager())

extern TMap<FString, FString> EditorINI;

//Editor
#include "EditorEngine.h"

//CUR ENGINE MODE
#define _EDITOR

#ifdef _EDITOR
extern UEditorEngine GEngine;
#endif

#ifdef _GAME
extern UGameEngine GEngine;
#endif

extern UWorld* GWorld;