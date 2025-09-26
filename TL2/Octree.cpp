#include "pch.h"
#include "Octree.h"
FOctree::FOctree(const FBound& InBounds, int InDepth, int InMaxDepth, int InMaxObjects)
	: Bounds(InBounds), Depth(InDepth), MaxDepth(InMaxDepth), MaxObjects(InMaxObjects)
{
	// 안전하게 nullpter 
	for (int i = 0; i < 8; i++)
	{
		Children[i] = nullptr;
	}
}

FOctree::~FOctree()
{
	for (int i = 0; i < 8; i++)
	{
		if (Children[i])
		{
			delete Children[i];
			Children[i] = nullptr;
		}
	}
}

// 런타임 중에 옥트리 초기화 해야할 때 !
void FOctree::Clear()  
{
	// 액터 배열 정리
	Actors.Empty();

	// 자식들도 재귀적으로 정리
	for (int i = 0; i < 8; i++)
	{
		if (Children[i])
		{
			delete Children[i];
			Children[i] = nullptr;
		}
	}
}

void FOctree::Insert(AActor* InActor, const FBound& ActorBounds)
{
    // 현재 노드가 자식이 있는 경우 → 자식으로 분배
    if (Children[0])
    {
        for (int i = 0; i < 8; i++)
        {
            // 완전히 자식에 포함될 경우만 들어간다. 
            if (Children[i]->Contains(ActorBounds))
            {
                Children[i]->Insert(InActor, ActorBounds);
                return;
            }
        }
    }

    // 자식에 못 들어가면 현재 노드에 저장
    // 자식에 넣지 못하는 객체를 가질 수 도 있다. 
    Actors.push_back(InActor);

    // 분할 조건 체크
    // 현재 노드에 들어있는 객체 수가 Max초과 , 최대 깊이 보다 얕은 레벨이면 스플릿 
    if (Actors.size() > MaxObjects && Depth < MaxDepth)
    {
        if (!Children[0])
        {
            Split();
        }

        // 재분배
        auto It = Actors.begin();
        while (It != Actors.end())
        {
            AActor* ActorPtr = *It;
            //  StaticMeshActor에서 이뤄진 것이 호출
            FBound Box = ActorPtr->GetBounds();

            bool bMoved = false;
            for (int i = 0; i < 8; i++)
            {
                if (Children[i]->Contains(Box))
                {
                    Children[i]->Insert(ActorPtr, Box);
                    It = Actors.erase(It);
                    bMoved = true;
                    break;
                }
            }

            if (!bMoved) ++It;
        }
    }
}
bool FOctree::Contains(const FBound& Box) const
{
    return Bounds.Contains(Box);
}

bool FOctree::Remove(AActor* InActor, const FBound& ActorBounds)
{
    // 현재 노드에 있는 경우
    auto It = std::find(Actors.begin(), Actors.end(), InActor);
    if (It != Actors.end())
    {
        Actors.erase(It);
        return true;
    }

    // 자식 노드에 위임
    if (Children[0])
    {
        for (int i = 0; i < 8; i++)
        {
            if (Children[i]->Contains(ActorBounds))
            {
                return Children[i]->Remove(InActor, ActorBounds);
            }
        }
    }
    return false;
}

void FOctree::Update(AActor* InActor, const FBound& OldBounds, const FBound& NewBounds)
{
    Remove(InActor, OldBounds);
    Insert(InActor, NewBounds);
}


void FOctree::Split()
{
    FVector Center = Bounds.GetCenter();
    FVector Extent = Bounds.GetExtent() * 0.5f;

    for (int i = 0; i < 8; i++)
    {
        FBound ChildBounds = Bounds.CreateOctant(i);

        Children[i] = new FOctree(ChildBounds, Depth + 1, MaxDepth, MaxObjects);
    }
}

int FOctree::TotalNodeCount() const
{
    // 자신 노드 
    int Count = 1; 
    if (Children[0])
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i]) Count += Children[i]->TotalNodeCount();
        }
    }
    return Count;
}

int FOctree::TotalActorCount() const
{
    int Count = Actors.Num();
    if (Children[0])
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i]) Count += Children[i]->TotalActorCount();
        }
    }
    return Count;
}

int FOctree::MaxOccupiedDepth() const
{
    int MaxD = Depth;
    if (Children[0])
    {
        for (int i = 0; i < 8; ++i)
        {
            if (Children[i])
            {
                int ChildDepth = Children[i]->MaxOccupiedDepth();
                MaxD = (ChildDepth > MaxD) ? ChildDepth : MaxD;
            }
        }
    }
    return MaxD;
}
void FOctree::DebugDump() const
{
    UE_LOG("===== OCTREE DUMP BEGIN =====\r\n");
    // iterative DFS to access private members safely inside class method
    struct StackItem { const FOctree* Node; int D; };
    TArray<StackItem> stack;
    stack.push_back({ this, Depth });
    while (!stack.empty())
    {
        StackItem it = stack.back();
        stack.pop_back();

        const FOctree* N = it.Node;
        char buf[256];
        std::snprintf(buf, sizeof(buf),
            "[Octree] depth=%d, actors=%zu, bounds=[(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f)]\r\n",
            it.D,
            N->Actors.size(),
            N->Bounds.Min.X, N->Bounds.Min.Y, N->Bounds.Min.Z,
            N->Bounds.Max.X, N->Bounds.Max.Y, N->Bounds.Max.Z);
        UE_LOG(buf);

        if (N->Children[0])
        {
            for (int i = 7; i >= 0; --i)
            {
                if (N->Children[i])
                {
                    stack.push_back({ N->Children[i], it.D + 1 });
                }
            }
        }
    }
    UE_LOG("===== OCTREE DUMP END =====\r\n");
}

static void CreateLineDataFromAABB(
    const FVector& Min, const FVector& Max,
    OUT TArray<FVector>& Start,
    OUT TArray<FVector>& End,
    OUT TArray<FVector4>& Color,
    const FVector4& LineColor)
{
    const FVector v0(Min.X, Min.Y, Min.Z);
    const FVector v1(Max.X, Min.Y, Min.Z);
    const FVector v2(Max.X, Max.Y, Min.Z);
    const FVector v3(Min.X, Max.Y, Min.Z);
    const FVector v4(Min.X, Min.Y, Max.Z);
    const FVector v5(Max.X, Min.Y, Max.Z);
    const FVector v6(Max.X, Max.Y, Max.Z);
    const FVector v7(Min.X, Max.Y, Max.Z);

    Start.Add(v0); End.Add(v1); Color.Add(LineColor);
    Start.Add(v1); End.Add(v2); Color.Add(LineColor);
    Start.Add(v2); End.Add(v3); Color.Add(LineColor);
    Start.Add(v3); End.Add(v0); Color.Add(LineColor);

    Start.Add(v4); End.Add(v5); Color.Add(LineColor);
    Start.Add(v5); End.Add(v6); Color.Add(LineColor);
    Start.Add(v6); End.Add(v7); Color.Add(LineColor);
    Start.Add(v7); End.Add(v4); Color.Add(LineColor);

    Start.Add(v0); End.Add(v4); Color.Add(LineColor);
    Start.Add(v1); End.Add(v5); Color.Add(LineColor);
    Start.Add(v2); End.Add(v6); Color.Add(LineColor);
    Start.Add(v3); End.Add(v7); Color.Add(LineColor);
}

void FOctree::DebugDraw(URenderer* Renderer) const
{
    if (!Renderer) return;

    struct Item { const FOctree* Node; int D; };
    TArray<Item> st; st.push_back({ this, Depth });
    while (!st.empty())
    {
        Item it = st.back(); st.pop_back();
        const FOctree* N = it.Node;

        const int di = it.D % 3;
        FVector4 color(0.0f, 1.0f, 0.0f, 1.0f);
        if (di == 1) color = FVector4(0.2f, 0.8f, 1.0f, 1.0f);
        else if (di == 2) color = FVector4(1.0f, 0.6f, 0.1f, 1.0f);

        TArray<FVector> S, E; TArray<FVector4> C;
        CreateLineDataFromAABB(N->Bounds.Min, N->Bounds.Max, S, E, C, color);
        Renderer->AddLines(S, E, C);

        if (N->Children[0])
        {
            for (int i = 7; i >= 0; --i)
            {
                if (N->Children[i]) st.push_back({ N->Children[i], it.D + 1 });
            }
        }
    }
}
