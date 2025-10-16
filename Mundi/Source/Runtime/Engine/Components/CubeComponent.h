#pragma once
#include "StaticMeshComponent.h"

class UCubeComponent : public UStaticMeshComponent
{
public:
    DECLARE_CLASS(UCubeComponent, UStaticMeshComponent)
    UCubeComponent() {};
    ~UCubeComponent() {};
};

