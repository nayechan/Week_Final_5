#include "pch.h"
#include "ATileMapActor.h"
#include "StaticMeshComponent.h"
#include "Source/Runtime/Core/Object/ObjectFactory.h"

IMPLEMENT_CLASS(ATileMapActor)

BEGIN_PROPERTIES(ATileMapActor)
	MARK_AS_SPAWNABLE("타일맵", "타일 기반의 맵을 생성하는 액터입니다.");
END_PROPERTIES()

ATileMapActor::ATileMapActor()
	: MapWidth(10)
	, MapHeight(10)
	, TileSize(20.0f)
{
	GenerateGrid();
}

void ATileMapActor::GenerateGrid()
{
	for (UStaticMeshComponent* Tile : Tiles)
	{
		if (Tile)
		{
			Tile->DestroyComponent();
		}
	}
	Tiles.clear();
	Tiles.reserve(MapWidth * MapHeight);

	FString MeshPath = GDataDir + "/Model/Cube.obj";
	UStaticMesh* TileMesh = UResourceManager::GetInstance().Load<UStaticMesh>(MeshPath);
	
	const FAABB& MeshBounds = TileMesh->GetLocalBound();
	TileSize = MeshBounds.Max - MeshBounds.Min;

	for (int32 Y = 0; Y < MapHeight; Y++)
	{
		for (int32 X = 0; X < MapWidth; X++)
		{
			UStaticMeshComponent* TileComp = CreateDefaultSubobject<UStaticMeshComponent>("TileComponent");
			TileComp->SetRelativeLocation(FVector(X * TileSize.X, Y * TileSize.Y, 0.0f));
			TileComp->SetStaticMesh(GDataDir + "/Model/Cube.obj");
			TileComp->SetupAttachment(RootComponent);
			Tiles.push_back(TileComp);
		}
	}
}

bool ATileMapActor::WorldToTile(const FVector& WorldPos, int32& OutX, int32& OutY) const
{
	const FTransform& ActorTransform = GetActorTransform();
	FTransform InverseTransform = ActorTransform.Inverse();
	FVector LocalPos = InverseTransform.TransformPosition(WorldPos);

	OutX = static_cast<int32>(floor(LocalPos.X / TileSize.X));
	OutY = static_cast<int32>(floor(LocalPos.X / TileSize.Y));

	return (OutX >= 0 && OutX < MapWidth && OutY >= 0 && OutY < MapHeight);
}

void ATileMapActor::DestroyTileAt(int32 X, int32 Y)
{
	if (X >= 0 && X < MapWidth && Y >= 0 && Y < MapHeight)
	{
		int32 Index = Y * MapWidth + X;
		if (Index < Tiles.size() && Tiles[Index] != nullptr)
		{
			Tiles[Index]->DestroyComponent();
			Tiles[Index] = nullptr;
		}
	}
}