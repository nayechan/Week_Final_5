#pragma once
#include "PrimitiveComponent.h"

class UShader;

class UMeshComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UMeshComponent, UPrimitiveComponent)

    UMeshComponent();

protected:
    ~UMeshComponent() override;

public:
    // ViewMode에 따른 셰이더 설정 (SceneRenderer에서 호출)
    // 파생 클래스에서 필요에 따라 override 가능
    virtual void SetViewModeShader(UShader* InShader);

    void DuplicateSubObjects() override;
    DECLARE_DUPLICATE(UMeshComponent)
};