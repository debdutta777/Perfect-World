#include "PerfectWorldAcceptanceSubsystem.h"

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
#include "PerfectWorldPlayerCharacter.h"

#include <algorithm>
#include <vector>

namespace
{
namespace R1LI = PerfectWorld::LaneInterior;
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct R1Restart
{
    bool bContinue = false;
    PerfectWorld::WorldSaveData Saved;
};

const R1LI::FrontageEntry* R1Row(const FString& Name)
{
    return R1LI::FindFrontageById(std::string("building.nabapara.") + TCHAR_TO_UTF8(*Name));
}

FVector R1Point(const R1LI::FrontageEntry& Row, double U, double V)
{
    return FVector(Row.OriginXCm + U, Row.Side * (Row.FrontAbsYCm + V), 15);
}

APWDoorActor* R1Door(UPWAcceptanceSubsystem& Sub, const FString& Name)
{
    const auto* Row = R1Row(Name);
    if (!Row || !Sub.GetPlayWorld()) return nullptr;
    const FName Id(UTF8_TO_TCHAR(std::string(Row->EntranceDoorId).c_str()));
    for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
        if (It->GetDoorId() == Id) return *It;
    return nullptr;
}

bool R1ReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot;
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out).HashVerified;
}

FStepDef R1Action(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepDef R1Phase(FStepDef Def, TSharedRef<R1Restart> State, bool bContinue)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("other fresh-process phase"));
    };
    return Def;
}

FStepDef R1Walk(const FString& Name, const FVector& Target)
{
    return PWAcceptance::MakeWalkStep(Name, [Target](UPWAcceptanceSubsystem&, FVector& Out, FString&)
        { Out = Target; return true; }, 8, 80);
}

FStepDef R1Leaf(const FString& Name, bool bOpen, const FString& Suffix)
{
    const auto Done = [Name, bOpen](UPWAcceptanceSubsystem& Sub)
    {
        const auto* Door = R1Door(Sub, Name);
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        return Door && Life && !Door->IsMoving() && Door->IsOpen() == bOpen &&
            (bOpen ? Door->GetOpenFraction() >= 0.99f : Door->GetOpenFraction() <= 0.01f) &&
            Life->IsDoorOpen(Door->GetDoorId(), !bOpen) == bOpen;
    };
    return PWAcceptance::MakeInteractStep(Name + Suffix,
        [Name](UPWAcceptanceSubsystem& Sub) -> AActor* { return R1Door(Sub, Name); },
        [Done](UPWAcceptanceSubsystem& Sub, FString& Detail)
        { Detail = TEXT("leaf and authoritative state settled"); return Done(Sub); }, Done, 15);
}

FStepDef R1Inside(const FString& Name, bool bExit)
{
    const auto* Row = R1Row(Name);
    std::vector<R1LI::RoutePoint> Points;
    R1LI::Anchor End;
    if (Row)
    {
        const auto Layout = R1LI::GenerateInteriorLayout(R1LI::MakeFrontageInput(*Row, 15, 300));
        const std::string Prefix = std::string(Row->BuildingId) + ".";
        for (const char* Suffix : {"browse_shelf", "browse_bins"})
        {
            const auto* Route = R1LI::FindRoute(Layout, Prefix + "route.door_to_" + Suffix);
            const auto* Anchor = R1LI::FindAnchor(Layout, Prefix + "anchor." + Suffix);
            if (Layout.Ok() && Route && Anchor) { Points = Route->Points; End = *Anchor; break; }
        }
        if (bExit) { std::reverse(Points.begin(), Points.end()); Points.push_back({Row->EntranceCentreU, -110}); }
    }
    auto Def = R1Action(Name + (bExit ? TEXT("_walk_out") : TEXT("_walk_to_browse")),
        [Row, Points, End, bExit](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Index = static_cast<std::size_t>(Rt.Phase);
        if (!Row || End.Id.empty() || Index >= Points.size()) return FStepOutcome::Fail(TEXT("no authored browse route"));
        const auto Point = Points[Index];
        const auto Outcome = Sub.TickWalk(R1Point(*Row, Point.U, Point.V), 8, Rt);
        if (Outcome.State != PWAcceptance::EStepState::Passed) return Outcome;
        if (Index + 1 < Points.size())
        {
            Rt.NextPhase();
            Rt.LastProbe = Rt.StuckSince = 0;
            return FStepOutcome::Wait();
        }
        if (!bExit) Sub.FaceWorldPoint(R1Point(*Row, End.U + End.FaceU * 100, End.V + End.FaceV * 100));
        return FStepOutcome::Pass(bExit ? TEXT("walked through entrance to pavement") : TEXT("reached furnished browse anchor"));
    });
    Def.TimeoutSeconds = 100;
    return Def;
}

// Production binding contract for the integrator: tag the browse interactable with interaction.nabapara.<name>.browse.
// Its notice must disclose both browsing windows and the absence of purchasing. No actor is spawned by this scenario.
FStepDef R1Browse(const FString& Name, const FString& Suffix)
{
    return PWAcceptance::MakeInteractStep(Name + Suffix,
        [Name](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            const FName Tag(*(TEXT("interaction.nabapara.") + Name + TEXT(".browse")));
            for (TActorIterator<AActor> It(Sub.GetPlayWorld()); It; ++It)
                if (It->ActorHasTag(Tag)) return *It;
            return nullptr;
        },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Interaction = Sub.GetInteraction();
            if (!Interaction) return false;
            const FString Notice = Interaction->GetNotice().ToString();
            Detail = Notice;
            return Notice.Contains(TEXT("09:00-13:00")) && Notice.Contains(TEXT("15:00-19:30")) &&
                Notice.Contains(TEXT("Unstaffed")) && Notice.Contains(TEXT("Purchasing unavailable"));
        }, nullptr, 12);
}

FStepOutcome R1SavedDoors(UPWAcceptanceSubsystem& Sub, const R1Restart& State)
{
    const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    if (!Life || Life->HasMovingDoors() || Life->GetWorldState().Doors != State.Saved.World.Doors)
        return FStepOutcome::Fail(TEXT("authoritative doors differ from original saved state"));
    int32 Index = 0;
    for (const FString Name : {FString(TEXT("market_stalls_a")), FString(TEXT("market_stalls_b")), FString(TEXT("end_shop"))})
    {
        const auto* Door = R1Door(Sub, Name);
        const auto* Row = R1Row(Name);
        const bool bExpected = Index++ != 1;
        if (!Door || !Row || Door->IsMoving() || Door->IsOpen() != bExpected || Door->GetSwingSide() != (bExpected ? Row->Side : 0) ||
            (bExpected ? Door->GetOpenFraction() < 0.99f : Door->GetOpenFraction() > 0.01f))
            return FStepOutcome::Fail(TEXT("mixed open/closed leaves or inward swing did not survive"));
    }
    return FStepOutcome::Pass(TEXT("all authoritative door pairs match slot; three presented leaves match open/closed/side"));
}
} // namespace

namespace PWAcceptance
{
// Integrator: declare AppendFrontagesRound4Steps in PerfectWorldAcceptanceSubsystem.h and route frontages_round4
// to this function in BuildScenario. AW-02 must run --restart-pair with the same explicit slot.
void AppendFrontagesRound4Steps(TArray<FStepDef>& Out)
{
    const auto State = MakeShared<R1Restart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(R1Action(TEXT("round4_restart_phase"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Slot < 0 || (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue")))
            return FStepOutcome::Fail(TEXT("frontages_round4 requires --restart-pair with an explicit slot"));
        State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (State->bContinue && !R1ReadSlot(Sub, State->Saved)) return FStepOutcome::Fail(TEXT("original slot unreadable before Continue"));
        return FStepOutcome::Pass(TEXT("explicit fresh-process phase"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(R1Phase(R1Action(TEXT("round4_continued_doors"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
        { return R1SavedDoors(Sub, *State); }), State, true));
    Out.Add(R1Phase(Stationery::JumpClockStep(TEXT("round4_browsing_1030"), 10, 30), State, false));
    Out.Add(R1Phase(R1Action(TEXT("round4_declared_start"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Row = R1Row(TEXT("market_stalls_a"));
        auto* Pawn = Sub.GetPawn();
        if (!Row || !Pawn) return FStepOutcome::Fail(TEXT("missing pawn or catalogue row"));
        if (Rt.Phase == 0)
        {
            const FVector Start = R1Point(*Row, Row->EntranceCentreU, -110) + FVector(0, 0, PWPlayer::CapsuleHalfHeight + 5);
            if (!Pawn->TeleportTo(Start, FRotator(0, -90, 0))) return FStepOutcome::Fail(TEXT("declared start blocked"));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) return FStepOutcome::Wait();
        return FStepOutcome::Pass(TEXT("sole declared teleport; remaining movement walked"));
    }), State, false));
    double PreviousX = 1950;
    int32 PreviousSide = -1;
    for (const FString Name : {FString(TEXT("market_stalls_a")), FString(TEXT("market_stalls_b")), FString(TEXT("end_shop"))})
    {
        const auto* Row = R1Row(Name);
        if (!Row)
        {
            Out.Add(R1Action(Name + TEXT("_missing"), [](UPWAcceptanceSubsystem&, FStepRuntime&)
                { return FStepOutcome::Fail(TEXT("catalogue row missing")); }));
            continue;
        }
        const double X = Row->OriginXCm + Row->EntranceCentreU;
        if (PreviousSide != Row->Side)
        {
            Out.Add(R1Phase(R1Walk(Name + TEXT("_road_edge"), FVector(PreviousX, PreviousSide * 300, 15)), State, false));
            Out.Add(R1Phase(R1Walk(Name + TEXT("_cross_lane"), FVector(PreviousX, Row->Side * 300, 15)), State, false));
            Out.Add(R1Phase(R1Walk(Name + TEXT("_along_pavement"), FVector(X, Row->Side * 300, 15)), State, false));
        }
        Out.Add(R1Phase(R1Walk(Name + TEXT("_approach"), R1Point(*Row, Row->EntranceCentreU, -110)), State, false));
        Out.Add(R1Phase(R1Leaf(Name, false, TEXT("_initial_close")), State, false));
        Out.Add(R1Phase(R1Leaf(Name, true, TEXT("_open")), State, false));
        Out.Add(R1Phase(R1Inside(Name, false), State, false));
        for (const bool bThird : {false, true})
        {
            const FString Camera = bThird ? TEXT("_third") : TEXT("_first");
            Out.Add(R1Phase(R1Action(Name + Camera, [bThird](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
            {
                if (!Sub.GetPawn()) return FStepOutcome::Fail(TEXT("no pawn"));
                Sub.GetPawn()->SetCameraMode(bThird ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
                return FStepOutcome::Pass(TEXT("camera selected"));
            }), State, false));
            Out.Add(R1Phase(R1Browse(Name, Camera + TEXT("_browse")), State, false));
            Out.Add(R1Phase(MakeShotStep(Name + Camera + TEXT("_interior")), State, false));
        }
        Out.Add(R1Phase(R1Inside(Name, true), State, false));
        Out.Add(R1Phase(R1Leaf(Name, false, TEXT("_close_after_exit")), State, false));
        if (Name != TEXT("market_stalls_b")) Out.Add(R1Phase(R1Leaf(Name, true, TEXT("_saved_open")), State, false));
        PreviousX = X;
        PreviousSide = Row->Side;
    }
    Out.Add(R1Phase(R1Action(TEXT("round4_save_mixed_leaves"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (!Sub.GetHud()) return FStepOutcome::Fail(TEXT("no HUD save interface"));
        bool bSaved = false;
        const FText Why = Sub.GetHud()->SaveActiveGame(bSaved);
        if (!bSaved || !R1ReadSlot(Sub, State->Saved)) return FStepOutcome::Fail(Why.ToString());
        return R1SavedDoors(Sub, *State);
    }), State, false));
    Out.Add(R1Phase(MakeShotStep(TEXT("round4_continued_leaves")), State, true));
}
} // namespace PWAcceptance
