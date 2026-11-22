#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PerfectWorldPresentationSettings.generated.h"

/** User comfort preference, independent of a life/save slot and available before a world exists. */
UCLASS(Config=GameUserSettings)
class PERFECTWORLD_API UPerfectWorldPresentationSettings : public UObject
{
    GENERATED_BODY()
public:
    static UPerfectWorldPresentationSettings& Get();
    bool IsReducedMotion() const { return bReducedMotion; }
    void SetReducedMotion(bool bValue);
    DECLARE_MULTICAST_DELEGATE(FMotionChanged);
    FMotionChanged OnMotionChanged;

private:
    UPROPERTY(Config)
    bool bReducedMotion = false;
};
