#pragma once

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "PerfectWorldDoorBell.h"

// F5: the acceptance scenarios that used to knock by pressing E on a private door now press E on its BELL (the real interaction path:
// the prompt must name the bell, then TryInteract). These two helpers are all the scenarios share; each scenario file keeps its own
// step builders, and only their target changes.
namespace PWAcceptance::DoorBell
{
/** The bell hung beside a door, or null. The door actor spawns it at BeginPlay for every private dwelling entrance. */
inline APWDoorBellActor* FindBell(UWorld* World, const FName DoorId)
{
    if (World == nullptr)
    {
        return nullptr;
    }
    for (TActorIterator<APWDoorBellActor> It(World); It; ++It)
    {
        if (It->GetDoorId() == DoorId)
        {
            return *It;
        }
    }
    return nullptr;
}

/** The notice a ring answered with, the words of the core's own knock (the presenter's text for Admitted, NoAnswer, NotReceiving). */
inline bool IsAnsweredNotice(const FString& Text)
{
    return Text.Contains(TEXT("Someone is coming"));
}
} // namespace PWAcceptance::DoorBell
