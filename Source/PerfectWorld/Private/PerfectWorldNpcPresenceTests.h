#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "ContentRegistry.h"

class UWorld;
class UPWNpcPresenceSubsystem;

namespace NpPresenceTest
{
struct FNpWorld
{
    UWorld* World = nullptr;
    UPWNpcPresenceSubsystem* Presence = nullptr;
    PerfectWorld::ContentDb Db;
    FNpWorld();
    ~FNpWorld();
    FNpWorld(const FNpWorld&) = delete;
    FNpWorld& operator=(const FNpWorld&) = delete;
    bool RegisterCare() const;
    bool BindCare() const;
};
}
#endif
