#include "PerfectWorldAcceptanceFrontagesR6.h"

#include "Doors.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LaneFrontageCatalogue.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceStationery.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneKitStalls.h"
#include "PerfectWorldPlayerCharacter.h"

#include <algorithm>
#include <vector>

namespace
{
namespace R61LI = PerfectWorld::LaneInterior;
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct R61Restart
{
    bool bContinue = false;
    PerfectWorld::WorldSaveData Saved;
};

const R61LI::FrontageEntry* R61Row(const FString& Name)
{
    return R61LI::FindFrontageById(std::string("building.nabapara.") + TCHAR_TO_UTF8(*Name));
}

FVector R61Point(const R61LI::FrontageEntry& Row, double U, double V)
{
    return FVector(Row.OriginXCm + U, Row.Side * (Row.FrontAbsYCm + V), PWLane::GroundZ + 15);
}

APWDoorActor* R61Door(UPWAcceptanceSubsystem& Sub, const FString& Name)
{
    const auto* Row = R61Row(Name);
    if (!Row || !Sub.GetPlayWorld()) return nullptr;
    const FName Id(UTF8_TO_TCHAR(std::string(Row->EntranceDoorId).c_str()));
    for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
        if (It->GetDoorId() == Id) return *It;
    return nullptr;
}

bool R61ReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot;
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out).HashVerified;
}

FStepDef R61Action(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepDef R61Phase(FStepDef Def, TSharedRef<R61Restart> State, bool bContinue)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("other fresh-process phase"));
    };
    return Def;
}

FStepDef R61Walk(const FString& Name, const FVector& Target)
{
    return PWAcceptance::MakeWalkStep(Name, [Target](UPWAcceptanceSubsystem&, FVector& Out, FString&)
        { Out = Target; return true; }, 8, 80);
}

FStepDef R61Leaf(const FString& Name, bool bOpen, const FString& Suffix)
{
    const auto Done = [Name, bOpen](UPWAcceptanceSubsystem& Sub)
    {
        const auto* Door = R61Door(Sub, Name);
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        return Door && Life && !Door->IsMoving() && Door->IsOpen() == bOpen &&
            (bOpen ? Door->GetOpenFraction() >= 0.99f : Door->GetOpenFraction() <= 0.01f) &&
            Life->IsDoorOpen(Door->GetDoorId(), !bOpen) == bOpen;
    };
    return PWAcceptance::MakeInteractStep(Name + Suffix,
        [Name](UPWAcceptanceSubsystem& Sub) -> AActor* { return R61Door(Sub, Name); },
        [Done](UPWAcceptanceSubsystem& Sub, FString& Detail)
        { Detail = TEXT("leaf and authoritative state settled"); return Done(Sub); }, Done, 15);
}

FStepDef R61Inside(const FString& Name, bool bExit, const FString& Camera)
{
    const auto* Row = R61Row(Name);
    std::vector<R61LI::RoutePoint> Points;
    R61LI::Anchor End;
    if (Row)
    {
        const auto Layout = R61LI::GenerateInteriorLayout(R61LI::MakeFrontageInput(*Row, 15, 300));
        const std::string Prefix = std::string(Row->BuildingId) + ".";
        for (const char* Suffix : {"browse_shelf", "browse_bins", "customer_counter"})
        {
            const auto* Route = R61LI::FindRoute(Layout, Prefix + "route.door_to_" + Suffix);
            const auto* Anchor = R61LI::FindAnchor(Layout, Prefix + "anchor." + Suffix);
            if (Layout.Ok() && Route && Anchor) { Points = Route->Points; End = *Anchor; break; }
        }
        if (bExit) { std::reverse(Points.begin(), Points.end()); Points.push_back({Row->EntranceCentreU, -110}); }
    }
    auto Def = R61Action(Name + Camera + (bExit ? TEXT("_walk_out") : TEXT("_walk_to_browse")),
        [Row, Points, End, bExit](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Index = static_cast<std::size_t>(Rt.Phase);
        if (!Row || End.Id.empty() || Index >= Points.size()) return FStepOutcome::Fail(TEXT("no authored browse route"));
        const auto Point = Points[Index];
        const auto Outcome = Sub.TickWalk(R61Point(*Row, Point.U, Point.V), 8, Rt);
        if (Outcome.State != PWAcceptance::EStepState::Passed) return Outcome;
        if (Index + 1 < Points.size())
        {
            Rt.NextPhase();
            Rt.LastProbe = Rt.StuckSince = 0;
            return FStepOutcome::Wait();
        }
        if (!bExit) Sub.FaceWorldPoint(R61Point(*Row, End.U + End.FaceU * 100, End.V + End.FaceV * 100));
        return FStepOutcome::Pass(bExit ? TEXT("walked through entrance to pavement") : TEXT("reached furnished browse anchor"));
    });
    Def.TimeoutSeconds = 100;
    return Def;
}

// Production binding contract for the integrator: tag the browse interactable with interaction.nabapara.<name>.browse.
// Its notice must disclose both browsing windows and the absence of purchasing. No actor is spawned by this scenario.
FStepDef R61Browse(const FString& Name, const FString& Suffix)
{
    return PWAcceptance::MakeInteractStep(Name + Suffix,
        [Name](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            const FName Tag(*(TEXT("interaction.nabapara.") + Name + TEXT(".browse")));
            for (TActorIterator<AActor> It(Sub.GetPlayWorld()); It; ++It)
                if (It->ActorHasTag(Tag)) return *It;
            return nullptr;
        },
        [Name](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Interaction = Sub.GetInteraction();
            if (!Interaction) return false;
            const FString Notice = Interaction->GetNotice().ToString();
            Detail = Notice;
            const auto* Row = R61Row(Name);
            const auto* Door = Row ? PerfectWorld::FindDoor(Row->EntranceDoorId) : nullptr;
            return Door && Notice.Contains(PWLane::PostedHoursText(*Door)) &&
                Notice.Contains(PWLane::KitPublic::UnstaffedPhrase);
        }, nullptr, 12);
}

FStepOutcome R61SavedDoors(UPWAcceptanceSubsystem& Sub, const R61Restart& State)
{
    const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    if (!Life || Life->HasMovingDoors() || Life->GetWorldState().Doors != State.Saved.World.Doors)
        return FStepOutcome::Fail(TEXT("authoritative doors differ from original saved state"));
    for (const FString Name : {FString(TEXT("alley_house")), FString(TEXT("stair_flats")), FString(TEXT("corner_stalls"))})
    {
        const auto* Door = R61Door(Sub, Name);
        const auto* Row = R61Row(Name);
        const bool bExpected = Name == TEXT("corner_stalls");
        if (!Door || !Row || Door->IsMoving() || Door->IsOpen() != bExpected || Door->GetSwingSide() != (bExpected ? Row->Side : 0) ||
            (bExpected ? Door->GetOpenFraction() < 0.99f : Door->GetOpenFraction() > 0.01f))
            return FStepOutcome::Fail(TEXT("mixed open/closed leaves or inward swing did not survive"));
    }
    return FStepOutcome::Pass(TEXT("all authoritative door pairs match slot; corner open, both private homes closed"));
}
} // namespace

namespace
{
FStepDef R61PrivateDoor(const FString& Name, const FString& Camera)
{
    const auto R61Before = MakeShared<PerfectWorld::DoorState>();
    auto R61Step = PWAcceptance::MakeInteractStep(Name + Camera + TEXT("_private_knock"),
        [Name](UPWAcceptanceSubsystem& Sub) -> AActor* { return R61Door(Sub, Name); },
        [Name, R61Before](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Door = R61Door(Sub, Name);
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            const auto* Interaction = Sub.GetInteraction();
            Detail = Interaction ? Interaction->GetNotice().ToString() : FString();
            return Door && Life && Interaction && Detail == TEXT("Nobody answers.") && !Door->IsMoving() && !Door->IsOpen() &&
                Door->GetOpenFraction() <= 0.01f && Door->GetSwingSide() == 0 && Life->GetWorldState().Doors == *R61Before;
        }, nullptr, 12);
    auto R61Run = MoveTemp(R61Step.Run);
    R61Step.Run = [Name, R61Before, R61Run = MoveTemp(R61Run)](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        if (Rt.Phase == 0)
        {
            const auto* Row = R61Row(Name);
            const auto* Def = Row ? PerfectWorld::FindDoor(Row->EntranceDoorId) : nullptr;
            const auto* Door = R61Door(Sub, Name);
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            auto* Interaction = Sub.GetInteraction();
            if (!Def || !Door || !Life || !Interaction || Def->Access != PerfectWorld::DoorAccess::Private ||
                !Def->VisitingWindows.empty() || Life->IsHostHomeForAcceptance(*Def) || Door->IsOpen() || Door->IsMoving() ||
                Life->IsDoorOpen(Door->GetDoorId(), true) ||
                PerfectWorld::EvaluateDoorPassage(*Def, Life->GetMinuteOfDay(), {}) != PerfectWorld::DoorResult::Private ||
                PerfectWorld::EvaluateDoorKnock(*Def, Life->GetMinuteOfDay(), {}).Answer != PerfectWorld::DoorKnockAnswer::NoAnswer)
                return FStepOutcome::Fail(TEXT("home is not unoccupied, closed and private with no visiting windows"));
            *R61Before = Life->GetWorldState().Doors;
            Interaction->ShowNotice(FText::GetEmpty()); // do not accept a stale notice from the previous home/camera
        }
        return R61Run(Sub, Rt);
    };
    return R61Step;
}
} // namespace

namespace PWAcceptance
{
void AppendFrontagesRound6Steps(TArray<FStepDef>& Out)
{
    const auto R61State = MakeShared<R61Restart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(R61Action(TEXT("round6_restart_phase"), [R61State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Slot < 0 || (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue")))
            return FStepOutcome::Fail(TEXT("frontages_round6 requires --restart-pair with an explicit slot"));
        R61State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (R61State->bContinue && !R61ReadSlot(Sub, R61State->Saved)) return FStepOutcome::Fail(TEXT("original slot unreadable before Continue"));
        return FStepOutcome::Pass(TEXT("explicit fresh-process phase"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(R61Phase(R61Action(TEXT("round6_continued_doors"), [R61State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
        { return R61SavedDoors(Sub, *R61State); }), R61State, true));
    Out.Add(R61Phase(Stationery::JumpClockStep(TEXT("round6_browsing_1030"), 10, 30), R61State, false));
    Out.Add(R61Phase(R61Action(TEXT("round6_declared_start"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Row = R61Row(TEXT("corner_stalls"));
        auto* Pawn = Sub.GetPawn();
        if (!Row || !Pawn) return FStepOutcome::Fail(TEXT("missing pawn or corner catalogue row"));
        if (Rt.Phase == 0)
        {
            const FVector Start = R61Point(*Row, Row->EntranceCentreU, -110) + FVector(0, 0, PWPlayer::CapsuleHalfHeight + 5);
            if (!Pawn->TeleportTo(Start, FRotator(0, 90, 0))) return FStepOutcome::Fail(TEXT("declared start blocked"));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) return FStepOutcome::Wait();
        return FStepOutcome::Pass(TEXT("sole declared teleport; remaining movement walked"));
    }), R61State, false));
    double R61PreviousX = -2150;
    int32 R61PreviousSide = 1;
    for (const FString R61Name : {FString(TEXT("corner_stalls")), FString(TEXT("alley_house")), FString(TEXT("stair_flats"))})
    {
        const auto* R61Entry = R61Row(R61Name);
        if (!R61Entry)
        {
            Out.Add(R61Action(R61Name + TEXT("_missing"), [](UPWAcceptanceSubsystem&, FStepRuntime&)
                { return FStepOutcome::Fail(TEXT("catalogue row missing")); }));
            continue;
        }
        const double R61X = R61Entry->OriginXCm + R61Entry->EntranceCentreU;
        // Y=325 clears kerb props and the corner's 90 cm stall face with the actual 34 cm capsule radius.
        Out.Add(R61Phase(R61Walk(R61Name + TEXT("_road_edge"), FVector(R61PreviousX, R61PreviousSide * 325, 15)), R61State, false));
        if (R61PreviousSide != R61Entry->Side)
            Out.Add(R61Phase(R61Walk(R61Name + TEXT("_cross_lane"), FVector(R61PreviousX, R61Entry->Side * 325, 15)), R61State, false));
        Out.Add(R61Phase(R61Walk(R61Name + TEXT("_along_pavement"), FVector(R61X, R61Entry->Side * 325, 15)), R61State, false));
        Out.Add(R61Phase(R61Walk(R61Name + TEXT("_approach"), R61Point(*R61Entry, R61Entry->EntranceCentreU, -110)), R61State, false));
        const bool R61Shop = R61Name == TEXT("corner_stalls");
        if (R61Shop)
        {
            Out.Add(R61Phase(R61Leaf(R61Name, false, TEXT("_initial_close")), R61State, false));
            Out.Add(R61Phase(R61Leaf(R61Name, true, TEXT("_open")), R61State, false));
        }
        for (const bool R61Third : {false, true})
        {
            const FString R61Camera = R61Third ? TEXT("_third") : TEXT("_first");
            Out.Add(R61Phase(R61Action(R61Name + R61Camera, [R61Third](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
            {
                if (!Sub.GetPawn()) return FStepOutcome::Fail(TEXT("no pawn"));
                Sub.GetPawn()->SetCameraMode(R61Third ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
                return FStepOutcome::Pass(TEXT("camera selected"));
            }), R61State, false));
            if (R61Shop)
            {
                Out.Add(R61Phase(R61Inside(R61Name, false, R61Camera), R61State, false));
                Out.Add(R61Phase(R61Browse(R61Name, R61Camera + TEXT("_browse")), R61State, false));
                Out.Add(R61Phase(MakeShotStep(R61Name + R61Camera + TEXT("_interior")), R61State, false));
                Out.Add(R61Phase(R61Inside(R61Name, true, R61Camera), R61State, false));
            }
            else
            {
                Out.Add(R61Phase(R61PrivateDoor(R61Name, R61Camera), R61State, false));
                Out.Add(R61Phase(MakeShotStep(R61Name + R61Camera + TEXT("_private_refusal")), R61State, false));
            }
        }
        if (R61Shop)
        {
            Out.Add(R61Phase(R61Leaf(R61Name, false, TEXT("_close_after_exit")), R61State, false));
            Out.Add(R61Phase(R61Leaf(R61Name, true, TEXT("_saved_open")), R61State, false));
        }
        R61PreviousX = R61X;
        R61PreviousSide = R61Entry->Side;
    }
    Out.Add(R61Phase(R61Action(TEXT("round6_save_mixed_leaves"), [R61State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (!Sub.GetHud()) return FStepOutcome::Fail(TEXT("no HUD save interface"));
        bool R61Saved = false;
        const FText R61Why = Sub.GetHud()->SaveActiveGame(R61Saved);
        if (!R61Saved || !R61ReadSlot(Sub, R61State->Saved)) return FStepOutcome::Fail(R61Why.ToString());
        return R61SavedDoors(Sub, *R61State);
    }), R61State, false));
    Out.Add(R61Phase(MakeShotStep(TEXT("round6_continued_leaves")), R61State, true));
}
} // namespace PWAcceptance

