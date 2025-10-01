#include "pch.h"
#include "GridActor.h"

using std::to_string;

AGridActor::AGridActor()
{
    LineComponent = NewObject<ULineComponent>();
    LineComponent->SetupAttachment(RootComponent);
    AddOwnedComponent(LineComponent);
}

void AGridActor::Initialize()
{
    RegenerateGrid();
    if (EditorINI.count("GridSpacing"))
    {
        try
        {
            float temp = std::stof(EditorINI["GridSpacing"]);
            SetLineSize(temp);
        }
        catch (...)
        {
            SetLineSize(0.1f);
        }
    }
    else
    {
        SetLineSize(0.1f);
    }
}

AGridActor::~AGridActor()
{

}

namespace
{
    FVector4 GetGridLineColor(int index)
    {
        if (index % 10 == 0)
            return FVector4(1.0f, 1.0f, 1.0f, 1.0f); // 흰색
        if (index % 5 == 0)
            return FVector4(0.4f, 0.4f, 0.4f, 1.0f); // 밝은 회색
        return FVector4(0.1f, 0.1f, 0.1f, 1.0f);     // 어두운 회색
    }
}

void AGridActor::CreateGridLines(int32 InGridSize, float InCellSize, const FVector& Center)
{
    if (!LineComponent) return;

    const float gridTotalSize = InGridSize * InCellSize;

    for (int i = -InGridSize; i <= InGridSize; i++)
    {
        if (i == 0) continue;

        const float pos = i * InCellSize;
        const FVector4 color = GetGridLineColor(i);

        // Z축 방향 라인
        LineComponent->AddLine(FVector(pos, 0.0f, -gridTotalSize), FVector(pos, 0.0f, gridTotalSize), color);
        // X축 방향 라인
        LineComponent->AddLine(FVector(-gridTotalSize, 0.0f, pos), FVector(gridTotalSize, 0.0f, pos), color);
    }

    // 중앙 축 (Z, X)
    LineComponent->AddLine(FVector(0.0f, 0.0f, -gridTotalSize), FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    LineComponent->AddLine(FVector(-gridTotalSize, 0.0f, 0.0f), FVector(0.0f, 0.0f, 0.0f), FVector4(1.0f, 1.0f, 1.0f, 1.0f));
}

void AGridActor::CreateAxisLines(float Length, const FVector& Origin)
{
    if (!LineComponent) return;
        
    // 변환 전 X축 - 원점에서 +X 방향 => 변환 후 Y축
    LineComponent->AddLine(Origin, 
                          Origin + FVector(Length * CellSize, 0.0f, 0.0f),
                          FVector4(0.0f, 1.0f, 0.0f, 1.0f));
    
    // 변환 전 Y축 - 원점에서 +Y 방향 => 변환 후 Z축
    LineComponent->AddLine(Origin, 
                          Origin + FVector(0.0f, Length * CellSize, 0.0f),
                          FVector4(0.0f, 0.0f, 1.0f, 1.0f));
    
    // 변환 전 Z축 - 원점에서 +Z 방향 => 변환 후 X축
    LineComponent->AddLine(Origin, 
                          Origin + FVector(0.0f, 0.0f, Length * CellSize),
                          FVector4(1.0f, 0.0f, 0.0f, 1.0f));
}

void AGridActor::ClearLines()
{
    if (LineComponent)
    {
        LineComponent->ClearLines();
    }
}

void AGridActor::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    LineComponent = LineComponent->Duplicate();
}

void AGridActor::RegenerateGrid()
{
    // Clear existing lines
    ClearLines();
    
    // Generate new grid and axis lines with current settings
    CreateAxisLines(AxisLength, FVector());
    CreateGridLines(GridSize, CellSize, FVector());
}

