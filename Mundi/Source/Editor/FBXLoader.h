#pragma once
#include "Object.h"
#include "fbxsdk.h"

class UFBXLoader : public UObject
{
public:

	static UFBXLoader& GetInstance();

private:
	UFBXLoader();
	~UFBXLoader();

	UFBXLoader(const UFBXLoader&) = delete;
	UFBXLoader& operator=(const UFBXLoader&) = delete;

	FbxManager* Manager = nullptr;

	
};