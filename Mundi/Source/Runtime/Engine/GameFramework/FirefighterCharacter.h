#pragma once
#include "Character.h"
#include "AFirefighterCharacter.generated.h"

class UAudioComponent;

UCLASS(DisplayName = "파이어 파이터 캐릭터", Description = "렛츠고 파이어 파이터")
class AFirefighterCharacter : public ACharacter
{
    GENERATED_REFLECTION_BODY()

public:
    AFirefighterCharacter();

protected:
    ~AFirefighterCharacter() override;

    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void HandleAnimNotify(const FAnimNotifyEvent& NotifyEvent) override;

private:
    //USound* SorrySound;
    //USound* HitSound;
};
