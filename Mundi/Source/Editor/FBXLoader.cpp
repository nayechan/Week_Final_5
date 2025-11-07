#include "pch.h"
#include "ObjectFactory.h"
#include "FBXLoader.h"


UFBXLoader::UFBXLoader()
{

}

UFBXLoader::~UFBXLoader()
{
}
UFBXLoader& UFBXLoader::GetInstance()
{
	static UFBXLoader* FBXLoader = nullptr;
	if (!FBXLoader)
	{
		FBXLoader = ObjectFactory::NewObject<UFBXLoader>();
	}
	return *FBXLoader;
}