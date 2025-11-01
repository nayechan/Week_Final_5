#pragma once
#include "StaticMeshActor.h"

class ATileMapActor : public AStaticMeshActor
{
public:
	DECLARE_CLASS(ATileMapActor, AStaticMeshActor)
	GENERATED_REFLECTION_BODY()

	ATileMapActor();

	void GenerateGrid();
	bool WorldToTile(const FVector& WorldPos, int32& OutX, int32& OutY) const;
	void DestroyTileAt(int32 X, int32 Y);
	void ResizeMap(int32 NewWidth, int32 NewHeight);
	void ClearAllTiles();

private:
	TArray<UStaticMeshComponent*> Tiles;

	int32 MapWidth;
	int32 MapHeight;
	FVector TileSize;
};