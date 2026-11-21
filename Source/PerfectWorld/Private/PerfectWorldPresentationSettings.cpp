#include "PerfectWorldPresentationSettings.h"

#include "Misc/ConfigCacheIni.h"

UPerfectWorldPresentationSettings& UPerfectWorldPresentationSettings::Get()
{
    return *GetMutableDefault<UPerfectWorldPresentationSettings>();
}

void UPerfectWorldPresentationSettings::SetReducedMotion(bool bValue)
{
    if (bReducedMotion == bValue) return;
    bReducedMotion = bValue;
    OnMotionChanged.Broadcast();
    // Explicit user destination: never writes project defaults or a character save.
    SaveConfig(CPF_Config, *GGameUserSettingsIni);
}
