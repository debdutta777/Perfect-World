#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldAcceptanceSubsystem.h"
#include "WorldState.h"

namespace PerfectWorld { class ContentDb; struct DoorDef; struct NpcSchedule; }
class APWDoorActor;

// Shared pieces of the resident_homes scenario (NR-03): the door and clock steps live in PerfectWorldAcceptanceHomesDoors.cpp, the
// script and the resident checks in PerfectWorldAcceptanceHomes.cpp.
namespace PWAcceptance::Homes
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

inline constexpr double HmReachSpotCm = 110.0;
inline constexpr double HmLeafHalfCm = 45.0;
inline constexpr double HmWalkTolerance = 45.0;
inline const FName HmSahana(TEXT("person.sahana_mitra"));
inline const FName HmNirmal(TEXT("person.nirmal_bose"));
inline const TCHAR* const HmHomeDoors[] = {TEXT("door.nabapara.caretaker_flat_front"), TEXT("door.nabapara.tall_flats_unit_a"),
                                    TEXT("door.nabapara.tall_flats_unit_b"), TEXT("door.nabapara.tea_row_house_front")};
inline const TCHAR* const HmUnitA = TEXT("door.nabapara.tall_flats_unit_a");
inline const TCHAR* const HmUnitB = TEXT("door.nabapara.tall_flats_unit_b");
inline const TCHAR* const HmFront = TEXT("door.nabapara.tall_flats_front");
inline const TCHAR* const HmFact = TEXT("fact.sahana_mitra.sewing_evening_agreed");

enum class EHmAnswer : uint8 { None, Admitted, NoAnswer, NotReceiving };

struct FHmLedger
{
    TArray<FString> Mismatches;
    EHmAnswer LastObserved = EHmAnswer::None;
    int32 Probes = 0;
};

FHmLedger& HmLedger();
const PerfectWorld::ContentDb* HmDb(UWorld* World);
TArray<PerfectWorld::NpcSchedule> HmResidentsOf(UWorld* World, const PerfectWorld::DoorDef& Def);
PerfectWorld::GameInstant HmInstant(int32 Day, int32 Hour, int32 Minute);
FName HmName(const std::string& Id);
bool HmAnyHomeBySchedule(UWorld* World, const PerfectWorld::DoorDef& Def, PerfectWorld::GameInstant At);
FStepDef HmClockStep(const FString& Name, int32 Day, int32 Hour, int32 Minute);
FStepDef HmWalkToDoor(const FString& Name, const TCHAR* DoorId, bool bInside);
FStepDef HmStartStep();
FStepDef HmDoorStep(const FString& Name, const TCHAR* DoorId, bool bWantOpen, const TCHAR* Say, TFunction<bool()> ExtraSkip = nullptr);
/** Knock (E), judge the answer against the schedules, press again, for unit B's door. */
void HmAddKnock(TArray<FStepDef>& Out, const FString& Tag);

inline FStepDef HmAction(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&)> Body)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 20.0;
    Def.Run = [Body](UPWAcceptanceSubsystem& Sub, FStepRuntime&) { return Body(Sub); };
    return Def;
}
}
