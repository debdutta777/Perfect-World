#include "UI/PWMapData.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ConfigCacheIni.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldReadingPresenter.h"

#define LOCTEXT_NAMESPACE "PerfectWorldMap"

namespace
{
struct FMpLabel
{
    const char* Key;
    FText Text;
};

/** Every key LaneMap.cpp can produce, in English. A key not listed here falls back to the portable map's own English label. */
const TArray<FMpLabel>& MpLabels()
{
    static const TArray<FMpLabel> Labels = {
        {"map.building.care_home", LOCTEXT("BuildingCareHome", "Children's home")},
        {"map.building.opening_home", LOCTEXT("BuildingOpeningHome", "Family flat")},
        {"map.building.stationery", LOCTEXT("BuildingStationery", "Stationery shop")},
        {"map.building.shanti_chai", LOCTEXT("BuildingShantiChai", "Shanti Chai Corner")},
        {"map.building.caretaker_flat", LOCTEXT("BuildingCaretakerFlat", "Caretaker's flat")},
        {"map.building.tall_flats", LOCTEXT("BuildingTallFlats", "Tall flats")},
        {"map.building.tea_row_house", LOCTEXT("BuildingRowHouse", "Row house")},
        {"map.building.repair_workshop", LOCTEXT("BuildingRepairWorkshop", "Repair workshop")},
        {"map.building.hardware_store", LOCTEXT("BuildingHardwareStore", "Hardware store")},
        {"map.building.old_house", LOCTEXT("BuildingOldHouse", "Old house")},
        {"map.building.market_stalls_a", LOCTEXT("BuildingMarketStalls", "Market stalls")},
        {"map.building.market_stalls_b", LOCTEXT("BuildingMarketStallsB", "Market stalls")},
        {"map.building.end_shop", LOCTEXT("BuildingEndShop", "End shop")},
        {"map.building.cloth_merchant", LOCTEXT("BuildingClothMerchant", "Cloth merchant")},
        {"map.building.narrow_shop", LOCTEXT("BuildingNarrowShop", "Narrow shop")},
        {"map.building.tailor_shop", LOCTEXT("BuildingTailor", "Tailor")},
        {"map.building.school", LOCTEXT("BuildingSchool", "School")},
        {"map.poi.alley", LOCTEXT("PoiAlley", "Side alley")},
        {"map.poi.lane_end_west", LOCTEXT("PoiLaneEndWest", "West end of the lane")},
        {"map.poi.lane_end_east", LOCTEXT("PoiLaneEndEast", "East end of the lane")},
    };
    return Labels;
}

constexpr double MpHomeCheckIntervalSeconds = 1.0;
} // namespace

namespace PWMap
{
FText LabelText(const std::string& Key, const std::string& Fallback)
{
    if (Key.empty() && Fallback.empty())
    {
        return FText::GetEmpty();
    }
    for (const FMpLabel& Label : MpLabels())
    {
        if (Key == Label.Key)
        {
            return Label.Text;
        }
    }
    return FText::FromString(UTF8_TO_TCHAR(Fallback.c_str()));
}

FText KindText(PerfectWorld::LaneMap::BuildingKind Kind)
{
    using PerfectWorld::LaneMap::BuildingKind;
    switch (Kind)
    {
    case BuildingKind::Home: return LOCTEXT("KindHome", "Home");
    case BuildingKind::Shop: return LOCTEXT("KindShop", "Shop");
    case BuildingKind::School: return LOCTEXT("KindSchool", "School");
    case BuildingKind::Workshop: return LOCTEXT("KindWorkshop", "Workshop");
    case BuildingKind::Sealed: return LOCTEXT("KindSealed", "Closed up");
    }
    return FText::GetEmpty();
}

FText ClockText(int32 MinuteOfDay)
{
    const int32 Minute = FMath::Clamp(MinuteOfDay, 0, 1439);
    return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minute / 60, Minute % 60));
}

FText DurationText(int32 Minutes)
{
    const int32 Total = FMath::Max(0, Minutes);
    if (Total < 60)
    {
        return FText::Format(LOCTEXT("DurationMinutes", "{0} min"), FText::AsNumber(Total));
    }
    if (Total % 60 == 0)
    {
        return FText::Format(LOCTEXT("DurationHours", "{0} h"), FText::AsNumber(Total / 60));
    }
    return FText::Format(LOCTEXT("DurationHoursMinutes", "{0} h {1} min"), FText::AsNumber(Total / 60), FText::AsNumber(Total % 60));
}

FText DescribeDoor(const PerfectWorld::LaneMap::DoorStatus& Status)
{
    using PerfectWorld::LaneMap::DoorAvailability;
    switch (Status.Availability)
    {
    case DoorAvailability::OpenToAll:
        if (Status.WindowCloseMinute >= 0)
        {
            return FText::Format(LOCTEXT("DoorOpenUntil", "Open now, until {0}."), ClockText(Status.WindowCloseMinute));
        }
        return LOCTEXT("DoorOpenToAll", "Open to everyone.");
    case DoorAvailability::ClosedForNow:
        return FText::Format(LOCTEXT("DoorClosedUntil", "Closed. Opens in {0}, at {1}."), DurationText(Status.MinutesUntilOpening),
                             ClockText(Status.WindowOpenMinute));
    case DoorAvailability::PrivateKnock:
        return LOCTEXT("DoorPrivate", "Private. Residents and invited guests go in; anyone else may knock.");
    case DoorAvailability::Locked:
        return LOCTEXT("DoorLocked", "Locked.");
    }
    return FText::GetEmpty();
}

FText DescribeBuilding(const FPWMapScene& Scene, const FPWMapSnapshot& Snapshot, int32 BuildingIndex)
{
    if (!Scene.Map.Buildings.empty() && BuildingIndex >= 0 && static_cast<std::size_t>(BuildingIndex) < Scene.Map.Buildings.size())
    {
        const PerfectWorld::LaneMap::Building& Building = Scene.Map.Buildings[static_cast<std::size_t>(BuildingIndex)];
        const FText Name = Scene.BuildingLabels.IsValidIndex(BuildingIndex) && !Scene.BuildingLabels[BuildingIndex].IsEmpty()
                               ? Scene.BuildingLabels[BuildingIndex]
                               : LOCTEXT("UnnamedBuilding", "Shuttered building");
        FText Result = FText::Format(LOCTEXT("BuildingLine", "{0} ({1})"), Name, KindText(Building.Kind));
        if (Snapshot.PlayerHomeBuilding == BuildingIndex)
        {
            Result = FText::Format(LOCTEXT("YourHomeLine", "Your home. {0}"), Result);
        }
        for (std::size_t Index = 0; Index < Scene.Map.Doors.size(); ++Index)
        {
            if (Scene.Map.Doors[Index].BuildingId == Building.Id && Snapshot.Doors.IsValidIndex(static_cast<int32>(Index)))
            {
                return FText::Format(LOCTEXT("BuildingWithDoor", "{0} {1}"), Result, DescribeDoor(Snapshot.Doors[static_cast<int32>(Index)]));
            }
        }
        return Result;
    }
    return FText::GetEmpty();
}
} // namespace PWMap

TSharedRef<FPWMapScene> FPWMapScene::Build(const PerfectWorld::LaneMap::MapOptions& Options)
{
    TSharedRef<FPWMapScene> Scene = MakeShared<FPWMapScene>();
    Scene->Map = PerfectWorld::LaneMap::BuildLaneMap(Options);
    for (const PerfectWorld::LaneMap::Building& Building : Scene->Map.Buildings)
    {
        Scene->BuildingLabels.Add(PWMap::LabelText(Building.LabelKey, Building.Label));
    }
    for (const PerfectWorld::LaneMap::PointOfInterest& Poi : Scene->Map.Pois)
    {
        Scene->PoiLabels.Add(PWMap::LabelText(Poi.LabelKey, Poi.Label));
    }
    return Scene;
}

FPWMapPreferences FPWMapPreferences::Load()
{
    FPWMapPreferences Result;
    if (GConfig != nullptr)
    {
        GConfig->GetBool(TEXT("PerfectWorld.Map"), TEXT("MinimapEnabled"), Result.bMinimapEnabled, GGameUserSettingsIni);
        GConfig->GetBool(TEXT("PerfectWorld.Map"), TEXT("MinimapRotates"), Result.bMinimapRotates, GGameUserSettingsIni);
    }
    return Result;
}

void FPWMapPreferences::Save() const
{
    if (GConfig != nullptr)
    {
        // Explicit user destination, like the other comfort settings: never a project default, never a character save.
        GConfig->SetBool(TEXT("PerfectWorld.Map"), TEXT("MinimapEnabled"), bMinimapEnabled, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("PerfectWorld.Map"), TEXT("MinimapRotates"), bMinimapRotates, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
}

PerfectWorld::LaneMap::MinimapOrientation FPWMapPreferences::Orientation(bool bReducedMotion) const
{
    using PerfectWorld::LaneMap::MinimapOrientation;
    return PerfectWorld::LaneMap::EffectiveOrientation(bMinimapRotates ? MinimapOrientation::RotateWithCamera : MinimapOrientation::NorthUp,
                                                       bReducedMotion);
}

FPWMapLive::FPWMapLive(const TWeakObjectPtr<UWorld>& InWorld)
    : World(InWorld)
    , Scene(FPWMapScene::Build())
    , Preferences(FPWMapPreferences::Load())
{
    Snapshot.Doors.SetNum(static_cast<int32>(Scene->Map.Doors.size()));
}

TSharedRef<FPWMapLive> FPWMapLive::MakeFixed(const TSharedRef<FPWMapScene>& InScene, const FPWMapSnapshot& InSnapshot)
{
    TSharedRef<FPWMapLive> Live = MakeShared<FPWMapLive>(TWeakObjectPtr<UWorld>());
    Live->Scene = InScene;
    Live->Snapshot = InSnapshot;
    Live->Preferences = FPWMapPreferences();
    Live->bFixed = true;
    return Live;
}

void FPWMapLive::SetPreferences(const FPWMapPreferences& NewPreferences)
{
    Preferences = NewPreferences;
    if (!bFixed)
    {
        Preferences.Save();
    }
}

void FPWMapLive::Refresh()
{
    if (bFixed || LastFrame == GFrameCounter)
    {
        return;
    }
    LastFrame = GFrameCounter;
    UWorld* Current = World.Get();
    if (Current == nullptr)
    {
        Snapshot.bHasPlayer = false;
        Snapshot.People.Reset();
        return;
    }
    GatherPlayer(*Current);
    GatherPeople(*Current);
    GatherDoors(*Current);
    GatherHome(*Current);
}

void FPWMapLive::GatherPlayer(UWorld& InWorld)
{
    const APlayerController* Controller = InWorld.GetFirstPlayerController();
    const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
    Snapshot.bHasPlayer = Pawn != nullptr;
    if (Pawn == nullptr)
    {
        return;
    }
    const FVector Location = Pawn->GetActorLocation();
    Snapshot.PlayerCm = {Location.X, Location.Y};
    // The map turns with what the player sees, so it follows the camera, not the body.
    const FRotator ViewRotation = Controller->PlayerCameraManager != nullptr ? Controller->PlayerCameraManager->GetCameraRotation()
                                                                              : Controller->GetControlRotation();
    Snapshot.PlayerYawDegrees = FRotator::NormalizeAxis(ViewRotation.Yaw);
}

void FPWMapLive::GatherPeople(UWorld& InWorld)
{
    Snapshot.People.Reset();
    // Only people who are standing in the lane now: the presenter destroys an actor when its person is off the lane, so a person
    // without an actor (asleep elsewhere, behind a threshold) has no dot.
    for (TActorIterator<APWNpcCharacter> It(&InWorld); It; ++It)
    {
        const APWNpcCharacter* Npc = *It;
        if (Npc == nullptr || Npc->IsHidden() || Npc->IsActorBeingDestroyed())
        {
            continue;
        }
        FPWMapMarker& Marker = Snapshot.People.AddDefaulted_GetRef();
        Marker.PersonId = Npc->GetPersonId();
        Marker.Name = Npc->GetDisplayName().IsEmpty() ? LOCTEXT("Someone", "Someone") : Npc->GetDisplayName();
        const FVector Location = Npc->GetActorLocation();
        Marker.WorldCm = {Location.X, Location.Y};
    }
}

void FPWMapLive::GatherDoors(UWorld& InWorld)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&InWorld);
    const PerfectWorld::DoorState* LiveDoors = Life != nullptr ? &Life->GetWorldState().Doors : nullptr;
    if (Life != nullptr)
    {
        Snapshot.MinuteOfDay = Life->GetMinuteOfDay();
    }
    const int32 Count = static_cast<int32>(Scene->Map.Doors.size());
    Snapshot.Doors.SetNum(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Snapshot.Doors[Index] = PerfectWorld::LaneMap::QueryDoorStatus(Scene->Map.Doors[static_cast<std::size_t>(Index)], LiveDoors, Snapshot.MinuteOfDay);
    }
}

void FPWMapLive::GatherHome(UWorld& InWorld)
{
    const double Now = FPlatformTime::Seconds();
    if (Now < NextHomeCheckSeconds)
    {
        return;
    }
    NextHomeCheckSeconds = Now + MpHomeCheckIntervalSeconds;
    Snapshot.PlayerHomeBuilding = INDEX_NONE;
    const UGameInstance* GameInstance = InWorld.GetGameInstance();
    const UPerfectWorldCharacterCreatorSubsystem* Creator = GameInstance != nullptr ? GameInstance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>() : nullptr;
    if (Creator == nullptr)
    {
        return;
    }
    constexpr std::string_view BuildingPrefix = "building.nabapara.";
    for (std::size_t Index = 0; Index < Scene->Map.Buildings.size(); ++Index)
    {
        const PerfectWorld::LaneMap::Building& Building = Scene->Map.Buildings[Index];
        if (Building.Kind != PerfectWorld::LaneMap::BuildingKind::Home || !Building.bEnterable || !std::string_view(Building.Id).starts_with(BuildingPrefix))
        {
            continue;
        }
        // The core's resolved origin of the current life decides which location is the player's home (LaneOrigin).
        const FString Location = TEXT("location.nabapara.") + PWLaneLife::ToFString(std::string_view(Building.Id).substr(BuildingPrefix.size()));
        if (Creator->HasLaneOriginResidency(FName(*Location)))
        {
            Snapshot.PlayerHomeBuilding = static_cast<int32>(Index);
            return;
        }
    }
}

PerfectWorld::LaneMap::HudState FPWMapLive::ReadHudState(bool bWorldMapOpen) const
{
    PerfectWorld::LaneMap::HudState State;
    State.bMinimapEnabled = Preferences.bMinimapEnabled;
    State.bWorldMapOpen = bWorldMapOpen;
    if (bFixed)
    {
        State.bHasLocalPlayer = Snapshot.bHasPlayer;
        return State;
    }
    const UWorld* Current = World.Get();
    const APlayerController* Controller = Current != nullptr ? Current->GetFirstPlayerController() : nullptr;
    State.bHasLocalPlayer = Controller != nullptr && Controller->GetPawn() != nullptr;
    if (Controller == nullptr)
    {
        return State;
    }
    if (const APerfectWorldFrontEndHUD* Hud = Cast<APerfectWorldFrontEndHUD>(Controller->GetHUD()))
    {
        State.bMenuOpen = Hud->IsMenuOpen();
    }
    if (const UPWConversationPresenter* Talk = UPWConversationPresenter::Get(Current))
    {
        State.bConversationOpen = Talk->IsOpen();
    }
    if (const UPWReadingPresenter* Reading = UPWReadingPresenter::Get(Current))
    {
        State.bReadingOpen = Reading->IsOpen();
    }
    return State;
}

#undef LOCTEXT_NAMESPACE
