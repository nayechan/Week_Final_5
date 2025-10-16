#pragma once
#include "StaticMeshComponent.h"
class USphereComponent : public UStaticMeshComponent
{
public:
    DECLARE_CLASS(USphereComponent, UStaticMeshComponent)
    USphereComponent() {};
    ~USphereComponent() {};
};

