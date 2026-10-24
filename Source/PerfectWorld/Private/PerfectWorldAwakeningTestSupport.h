// Shared helpers of the R9-06 Awakening floor tests (PerfectWorldAwakeningFloorTests.cpp, ...WorldTests.cpp, ...MotionTests.cpp).
#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "PerfectWorldAwakeningFloor.h"

namespace PWAwakeningTest
{
using namespace PWAwakening;

/** A throw-away game world (play never begins), as the school and kit tests use. */
struct FAwTestWorld
{
    UWorld* World = nullptr;

    FAwTestWorld() { World = UWorld::CreateWorld(EWorldType::Game, false); }
    ~FAwTestWorld()
    {
        AwBridge::Remove(World);
        if (World != nullptr)
        {
            World->DestroyWorld(false);
        }
    }
    FAwTestWorld(const FAwTestWorld&) = delete;
    FAwTestWorld& operator=(const FAwTestWorld&) = delete;
};

/** A fixed site, so the geometry tests do not depend on the campus plan (that is tested separately). */
inline FAwFloorSite AwTSite()
{
    FAwFloorSite Site;
    Site.Origin = FVector(10000.0, 7000.0, -4185.0);
    Site.EntryStand = FVector(7300.0, 5870.0, -185.0);
    Site.EntryKiosk = FVector(7300.0, 5960.0, -185.0);
    Site.EntryYaw = 90.0;
    Site.bValid = true;
    return Site;
}

/** The configuration every world test uses: no viewport, transfers complete in the call. */
inline FAwFloorConfig AwTConfig()
{
    FAwFloorConfig Config;
    Config.Site = AwTSite();
    Config.bInstantTransitions = true;
    Config.bUseHud = false;
    Config.ReducedMotion = false;
    return Config;
}

/** A bridge that refuses every choice, to prove the floor does not decide anything itself. */
class FAwRefusingBridge final : public IAwFloorBridge
{
public:
    virtual bool CanEnter(FString&) override { return true; }
    virtual FAwEnterResult Enter() override
    {
        FAwEnterResult Result;
        Result.bAccepted = true;
        Result.Code = TEXT("refusing.enter");
        return Result;
    }
    virtual FAwFloorStatus GetStatus() override { return FAwFloorStatus(); }
    virtual FAwChoiceResult SubmitChoice(FName, FName) override
    {
        ++Submits;
        FAwChoiceResult Result;
        Result.Code = TEXT("refusing.choice");
        Result.Message = TEXT("The core says no.");
        return Result;
    }
    virtual void NoteAssistance(FName, FName) override { ++Assists; }
    virtual void Leave() override {}

    int32 Submits = 0;
    int32 Assists = 0;
};

/** Spawns the floor (instant transfers, no HUD) with a scripted bridge installed. */
struct FAwTestFloor
{
    FAwTestWorld Holder;
    FAwSpawnResult Spawned;
    TSharedPtr<FAwScriptedBridge> Bridge;

    bool Init(FAutomationTestBase& Test, const FAwFloorConfig& Config)
    {
        Spawned = SpawnAwakeningFloor(*Holder.World, Config);
        Bridge = MakeShared<FAwScriptedBridge>();
        AwBridge::Install(Holder.World, Bridge);
        return Test.TestTrue(TEXT("the floor spawned"), Spawned.bBuilt) && Test.TestNotNull(TEXT("the director"), Spawned.Floor);
    }

    APWAwakeningFloorActor* Floor() const { return Spawned.Floor; }
};
} // namespace PWAwakeningTest

#endif // WITH_DEV_AUTOMATION_TESTS
