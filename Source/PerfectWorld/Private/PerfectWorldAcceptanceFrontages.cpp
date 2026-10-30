#include "PerfectWorldAcceptanceFrontages.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LaneFrontageCatalogue.h"
#include "PerfectWorldAcceptanceStationery.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"

#include <algorithm>
#include <vector>

namespace
{
namespace F5LI = PerfectWorld::LaneInterior;
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

const F5LI::FrontageEntry* F5Row(const FString& Name)
{
    return F5LI::FindFrontageById(std::string("building.nabapara.") + TCHAR_TO_UTF8(*Name));
}

FVector F5Point(const F5LI::FrontageEntry& Row, double U, double V)
{
    return FVector(Row.OriginXCm + U, Row.Side * (Row.FrontAbsYCm + V), 15.0);
}

APWDoorActor* F5Door(UWorld* World, const FString& Name)
{
    const F5LI::FrontageEntry* Row = F5Row(Name);
    if (World == nullptr || Row == nullptr) { return nullptr; }
    const FName Id(UTF8_TO_TCHAR(std::string(Row->PlannedDoorId).c_str()));
    for (TActorIterator<APWDoorActor> It(World); It; ++It)
        if (It->GetDoorId() == Id) { return *It; }
    return nullptr;
}

FStepDef F5MigrationStep()
{
    FStepDef Def;
    Def.Name = TEXT("frontage_migration_ready");
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime&) -> FStepOutcome
    {
        for (const FString Name : {FString(TEXT("repair_workshop")), FString(TEXT("hardware_store")), FString(TEXT("old_house"))})
        {
            const auto* Row = F5Row(Name);
            if (Row == nullptr || Row->EntranceDoorId.empty() || Row->EntranceDoorId != Row->PlannedDoorId ||
                PerfectWorld::FindDoor(Row->EntranceDoorId) == nullptr || F5Door(Sub.GetPlayWorld(), Name) == nullptr)
                return FStepOutcome::Fail(TEXT("requires the 16->19 door migration, hung catalogue leaves and live door actors"));
        }
        return FStepOutcome::Pass(TEXT("all three migrated front doors exist"));
    };
    return Def;
}

FStepDef F5StartStep()
{
    FStepDef Def;
    Def.Name = TEXT("declared_start_outside_workshop");
    Def.Run = [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        auto* Pawn = Sub.GetPawn();
        const auto* Row = F5Row(TEXT("repair_workshop"));
        if (Pawn == nullptr || Row == nullptr) { return FStepOutcome::Fail(TEXT("no pawn or workshop row")); }
        if (Rt.Phase == 0)
        {
            const FVector Start = F5Point(*Row, Row->EntranceCentreU, -110) + FVector(0, 0, PWPlayer::CapsuleHalfHeight + 5);
            if (!Pawn->TeleportTo(Start, FRotator(0, -90, 0))) { return FStepOutcome::Fail(TEXT("declared start blocked")); }
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(TEXT("one declared start on the workshop footpath; every subsequent move is walked"));
    };
    return Def;
}

FStepDef F5LeafStep(const FString& Name, bool bOpen, const FString& Suffix)
{
    auto Def = PWAcceptance::MakeInteractStep(Name + Suffix,
        [Name](UPWAcceptanceSubsystem& Sub) -> AActor* { return F5Door(Sub.GetPlayWorld(), Name); },
        [Name, bOpen](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Door = F5Door(Sub.GetPlayWorld(), Name);
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            if (Door == nullptr || Life == nullptr || Door->IsMoving() || Door->IsOpen() != bOpen ||
                (bOpen && Door->GetOpenFraction() < 0.99f) || Life->IsDoorOpen(Door->GetDoorId(), !bOpen) != bOpen) { return false; }
            Detail = bOpen ? TEXT("leaf fully open and settled in the core") : TEXT("leaf closed and settled in the core");
            return true;
        },
        [Name, bOpen](UPWAcceptanceSubsystem& Sub)
        {
            const auto* Door = F5Door(Sub.GetPlayWorld(), Name);
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            return Door != nullptr && Life != nullptr && !Door->IsMoving() && Door->IsOpen() == bOpen &&
                (!bOpen || Door->GetOpenFraction() >= 0.99f) && Life->IsDoorOpen(Door->GetDoorId(), !bOpen) == bOpen;
        }, 12.0);
    const auto Run = Def.Run;
    Def.Run = [Run, Name, bOpen](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        const auto Outcome = Run(Sub, Rt);
        const auto* Interaction = Sub.GetInteraction();
        if (Name == TEXT("old_house") && bOpen && Rt.Phase > 0 && Interaction != nullptr &&
            Interaction->GetNotice().ToString().Contains(TEXT("Nobody answers")))
            return FStepOutcome::Fail(TEXT("NoAnswer: old_house is unoccupied. A legitimate invitation must reach the production access bridge before this walk can enter; no permission is fabricated."));
        return Outcome;
    };
    return Def;
}

// Use the core's reserved shop routes. The residence has a bed-side browse stand, reached along its entrance/partition stem.
bool F5InteriorPath(const F5LI::FrontageEntry& Row, std::vector<F5LI::RoutePoint>& Points, F5LI::Anchor& End)
{
    const auto Layout = F5LI::GenerateInteriorLayout(F5LI::MakeFrontageInput(Row, 15, 300));
    if (!Layout.Ok()) { return false; }
    const std::string Prefix = std::string(Row.BuildingId) + ".";
    const auto Add = [&Points](const F5LI::Route& Route, bool bReverse)
    {
        if (bReverse) { Points.insert(Points.end(), Route.Points.rbegin(), Route.Points.rend()); }
        else { Points.insert(Points.end(), Route.Points.begin(), Route.Points.end()); }
    };
    const F5LI::Anchor* Anchor = nullptr;
    if (Row.Furniture == F5LI::FurnitureSet::Workshop)
    {
        const auto* DoorRoute = F5LI::FindRoute(Layout, Prefix + "route.counter_to_door");
        const auto* WorkRoute = F5LI::FindRoute(Layout, Prefix + "route.counter_to_workbench");
        Anchor = F5LI::FindAnchor(Layout, Prefix + "anchor.workbench");
        if (DoorRoute == nullptr || WorkRoute == nullptr) { return false; }
        Add(*DoorRoute, true);
        Add(*WorkRoute, false);
    }
    else if (Row.Furniture == F5LI::FurnitureSet::GeneralGoods)
    {
        for (const char* Suffix : {"browse_shelf", "browse_bins"})
        {
            const auto* Route = F5LI::FindRoute(Layout, Prefix + "route.door_to_" + Suffix);
            if (Route == nullptr) { continue; }
            Anchor = F5LI::FindAnchor(Layout, Prefix + "anchor." + Suffix);
            Add(*Route, false);
            break;
        }
    }
    else
    {
        Anchor = F5LI::FindAnchor(Layout, Prefix + "anchor.bed_side");
        if (Anchor == nullptr) { return false; }
        Points = {{Row.EntranceCentreU, -45}, {Row.EntranceCentreU, Anchor->V}, {Anchor->U, Anchor->V}};
    }
    if (Anchor == nullptr || Points.empty()) { return false; }
    // Refuse changed furniture/walls that would obstruct this standing capsule. Dynamic obstacles remain TickWalk's responsibility.
    for (size_t I = 1; I < Points.size(); ++I)
    {
        const auto A = Points[I - 1];
        const auto B = Points[I];
        const F5LI::Rect Sweep{std::min(A.U, B.U) - 34, std::max(A.U, B.U) + 34, std::min(A.V, B.V) - 34, std::max(A.V, B.V) + 34};
        for (const auto& Piece : Layout.Pieces)
            if (Piece.Solid && Piece.Footprint.Overlaps(Sweep)) { return false; }
        for (const auto& Wall : Layout.Walls)
            if (Wall.W0 < 165 && Wall.W1 > 0 && Wall.Plan.Overlaps(Sweep)) { return false; }
    }
    End = *Anchor;
    return true;
}

FStepDef F5InteriorStep(const FString& Name, bool bExit)
{
    FStepDef Def;
    Def.Name = Name + (bExit ? TEXT("_walk_out") : TEXT("_walk_to_anchor"));
    Def.TimeoutSeconds = 80;
    const auto* Row = F5Row(Name);
    std::vector<F5LI::RoutePoint> Points;
    F5LI::Anchor End;
    const bool bValid = Row != nullptr && F5InteriorPath(*Row, Points, End);
    if (bExit) { std::reverse(Points.begin(), Points.end()); }
    if (bValid && bExit) { Points.push_back({Row->EntranceCentreU, -110}); }
    Def.Run = [Row, Points, End, bValid, bExit](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (!bValid) { return FStepOutcome::Fail(TEXT("furnished anchor route no longer resolves with capsule clearance")); }
        const size_t Index = static_cast<size_t>(Rt.Phase);
        if (Index >= Points.size()) { return FStepOutcome::Fail(TEXT("route phase outside points")); }
        const auto Point = Points[Index];
        const auto Outcome = Sub.TickWalk(F5Point(*Row, Point.U, Point.V), 8.0, Rt);
        if (Outcome.State != PWAcceptance::EStepState::Passed) { return Outcome; }
        if (Index + 1 < Points.size())
        {
            Rt.NextPhase();
            Rt.LastProbe = Rt.StuckSince = 0;
            return FStepOutcome::Wait();
        }
        if (!bExit) { Sub.FaceWorldPoint(F5Point(*Row, End.U + 100 * End.FaceU, End.V + 100 * End.FaceV)); }
        return FStepOutcome::Pass(bExit ? TEXT("walked back through the real entrance to the footpath") : UTF8_TO_TCHAR(End.Id.c_str()));
    };
    return Def;
}

FStepDef F5OutsideStep(const FString& Name, const FString& Label, double X, double Y)
{
    return PWAcceptance::MakeWalkStep(Name + Label, [X, Y](UPWAcceptanceSubsystem&, FVector& Target, FString&)
        { Target = FVector(X, Y, 15); return true; }, 15, 70);
}
} // namespace

namespace PWAcceptance
{
void AppendFrontageThreeSteps(TArray<FStepDef>& OutSteps)
{
    OutSteps.Add(MakeWorldReadyStep());
    OutSteps.Add(MakeEnterWorldStep());
    OutSteps.Add(F5MigrationStep());
    OutSteps.Add(F5StartStep());
    OutSteps.Add(Stationery::JumpClockStep(TEXT("frontages_tuesday_1030"), 10, 30));
    for (const FString Name : {FString(TEXT("repair_workshop")), FString(TEXT("hardware_store")), FString(TEXT("old_house"))})
    {
        const auto* Row = F5Row(Name);
        if (Row == nullptr) { continue; } // migration_ready fails before any movement if a row is absent
        const double X = Row->OriginXCm + Row->EntranceCentreU;
        // After the workshop, cross the road at its entrance, then walk along the +Y footpath. No alley or frontage shortcut.
        if (Name == TEXT("hardware_store"))
        {
            OutSteps.Add(F5OutsideStep(Name, TEXT("_to_south_road_edge"), 1050, -340));
            OutSteps.Add(F5OutsideStep(Name, TEXT("_cross_lane"), 1050, 300));
            OutSteps.Add(F5OutsideStep(Name, TEXT("_along_north_footpath"), X, 300));
        }
        if (Name == TEXT("old_house")) { OutSteps.Add(F5OutsideStep(Name, TEXT("_along_north_footpath"), X, 300)); }
        OutSteps.Add(F5OutsideStep(Name, TEXT("_approach_entrance"), X, Row->Side * (Row->FrontAbsYCm - 110)));
        OutSteps.Add(F5LeafStep(Name, false, TEXT("_settle_closed")));
        OutSteps.Add(F5LeafStep(Name, true, TEXT("_open_front_door")));
        OutSteps.Add(F5InteriorStep(Name, false));
        for (const bool bThird : {false, true})
        {
            FStepDef Camera;
            Camera.Name = Name + (bThird ? TEXT("_third_person") : TEXT("_first_person"));
            Camera.Run = [bThird](UPWAcceptanceSubsystem& Sub, FStepRuntime&) -> FStepOutcome
            {
                if (Sub.GetPawn() == nullptr) { return FStepOutcome::Fail(TEXT("no pawn")); }
                Sub.GetPawn()->SetCameraMode(bThird ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
                return FStepOutcome::Pass(TEXT("camera selected"));
            };
            OutSteps.Add(Camera);
            OutSteps.Add(MakeShotStep(Name + (bThird ? TEXT("_interior_third") : TEXT("_interior_first"))));
        }
        OutSteps.Add(F5InteriorStep(Name, true));
        OutSteps.Add(F5LeafStep(Name, false, TEXT("_close_after_exit")));
    }
}
} // namespace PWAcceptance
