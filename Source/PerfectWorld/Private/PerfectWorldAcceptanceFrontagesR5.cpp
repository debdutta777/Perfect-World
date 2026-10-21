#include "PerfectWorldAcceptanceFrontagesR5.h"

#include "Doors.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LaneFrontageCatalogue.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceHomes.h"
#include "PerfectWorldAcceptanceStationery.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneKitStalls.h"
#include "PerfectWorldPlayerCharacter.h"

#include <algorithm>
#include <vector>

namespace
{
namespace R53LI = PerfectWorld::LaneInterior;
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct R53Restart
{
    bool bContinue = false;
    PerfectWorld::WorldSaveData Saved;
};

const R53LI::FrontageEntry* R53Row(const FString& Name)
{
    return R53LI::FindFrontageById(std::string("building.nabapara.") + TCHAR_TO_UTF8(*Name));
}

FVector R53Point(const R53LI::FrontageEntry& Row, double U, double V)
{
    // World Z of the finished ground floor: the road top (PWLane::GroundZ, -200) plus the 15 cm floor. A bare 15 would drop the declared start 3 m.
    return FVector(Row.OriginXCm + U, Row.Side * (Row.FrontAbsYCm + V), PWLane::GroundZ + 15);
}

APWDoorActor* R53Door(UPWAcceptanceSubsystem& Sub, const FString& Name)
{
    const auto* Row = R53Row(Name);
    if (!Row || !Sub.GetPlayWorld()) return nullptr;
    const FName Id(UTF8_TO_TCHAR(std::string(Row->EntranceDoorId).c_str()));
    for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
        if (It->GetDoorId() == Id) return *It;
    return nullptr;
}

bool R53ReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot;
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out).HashVerified;
}

FStepDef R53Action(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepDef R53Phase(FStepDef Def, TSharedRef<R53Restart> State, bool bContinue)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("other fresh-process phase"));
    };
    return Def;
}

FStepDef R53Walk(const FString& Name, const FVector& Target)
{
    return PWAcceptance::MakeWalkStep(Name, [Target](UPWAcceptanceSubsystem&, FVector& Out, FString&)
        { Out = Target; return true; }, 8, 80);
}

FStepDef R53Leaf(const FString& Name, bool bOpen, const FString& Suffix)
{
    const auto Done = [Name, bOpen](UPWAcceptanceSubsystem& Sub)
    {
        const auto* Door = R53Door(Sub, Name);
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        return Door && Life && !Door->IsMoving() && Door->IsOpen() == bOpen &&
            (bOpen ? Door->GetOpenFraction() >= 0.99f : Door->GetOpenFraction() <= 0.01f) &&
            Life->IsDoorOpen(Door->GetDoorId(), !bOpen) == bOpen;
    };
    return PWAcceptance::MakeInteractStep(Name + Suffix,
        [Name](UPWAcceptanceSubsystem& Sub) -> AActor* { return R53Door(Sub, Name); },
        [Done](UPWAcceptanceSubsystem& Sub, FString& Detail)
        { Detail = TEXT("leaf and authoritative state settled"); return Done(Sub); }, Done, 15);
}

FStepDef R53Inside(const FString& Name, bool bExit, const FString& Camera)
{
    const auto* Row = R53Row(Name);
    std::vector<R53LI::RoutePoint> Points;
    R53LI::Anchor End;
    if (Row)
    {
        const auto Layout = R53LI::GenerateInteriorLayout(R53LI::MakeFrontageInput(*Row, 15, 300));
        const std::string Prefix = std::string(Row->BuildingId) + ".";
        for (const char* Suffix : {"browse_shelf", "browse_bins", "customer_counter"})
        {
            const auto* Route = R53LI::FindRoute(Layout, Prefix + "route.door_to_" + Suffix);
            const auto* Anchor = R53LI::FindAnchor(Layout, Prefix + "anchor." + Suffix);
            if (Layout.Ok() && Route && Anchor) { Points = Route->Points; End = *Anchor; break; }
        }
        if (bExit) { std::reverse(Points.begin(), Points.end()); Points.push_back({Row->EntranceCentreU, -110}); }
    }
    auto Def = R53Action(Name + Camera + (bExit ? TEXT("_walk_out") : TEXT("_walk_to_browse")),
        [Row, Points, End, bExit](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto Index = static_cast<std::size_t>(Rt.Phase);
        if (!Row || End.Id.empty() || Index >= Points.size()) return FStepOutcome::Fail(TEXT("no authored browse route"));
        const auto Point = Points[Index];
        const auto Outcome = Sub.TickWalk(R53Point(*Row, Point.U, Point.V), 8, Rt);
        if (Outcome.State != PWAcceptance::EStepState::Passed) return Outcome;
        if (Index + 1 < Points.size())
        {
            Rt.NextPhase();
            Rt.LastProbe = Rt.StuckSince = 0;
            return FStepOutcome::Wait();
        }
        if (!bExit) Sub.FaceWorldPoint(R53Point(*Row, End.U + End.FaceU * 100, End.V + End.FaceV * 100));
        return FStepOutcome::Pass(bExit ? TEXT("walked through entrance to pavement") : TEXT("reached furnished browse anchor"));
    });
    Def.TimeoutSeconds = 100;
    return Def;
}

// Production binding contract for the integrator: tag the browse interactable with interaction.nabapara.<name>.browse.
// Its notice must disclose both browsing windows and the absence of purchasing. No actor is spawned by this scenario.
FStepDef R53Browse(const FString& Name, const FString& Suffix)
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
            const auto* Row = R53Row(Name);
            const auto* Door = Row ? PerfectWorld::FindDoor(Row->EntranceDoorId) : nullptr;
            return Door && Notice.Contains(PWLane::PostedHoursText(*Door)) &&
                Notice.Contains(PWLane::KitPublic::UnstaffedPhrase);
        }, nullptr, 12);
}

FStepOutcome R53SavedDoors(UPWAcceptanceSubsystem& Sub, const R53Restart& State)
{
    const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    if (!Life || Life->HasMovingDoors() || Life->GetWorldState().Doors != State.Saved.World.Doors)
        return FStepOutcome::Fail(TEXT("authoritative doors differ from original saved state"));
    int32 Index = 0;
    for (const FString Name : {FString(TEXT("cloth_merchant")), FString(TEXT("narrow_shop")), FString(TEXT("tailor_shop"))})
    {
        const auto* Door = R53Door(Sub, Name);
        const auto* Row = R53Row(Name);
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
// Integrator: declare AppendFrontagesRound5Steps in PerfectWorldAcceptanceSubsystem.h and route frontages_round5
// to this function in BuildScenario. AW-02 must run --restart-pair with the same explicit slot.
void AppendFrontagesRound5Steps(TArray<FStepDef>& Out)
{
    const auto State = MakeShared<R53Restart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(R53Action(TEXT("round5_restart_phase"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Slot < 0 || (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue")))
            return FStepOutcome::Fail(TEXT("frontages_round5 requires --restart-pair with an explicit slot"));
        State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (State->bContinue && !R53ReadSlot(Sub, State->Saved)) return FStepOutcome::Fail(TEXT("original slot unreadable before Continue"));
        return FStepOutcome::Pass(TEXT("explicit fresh-process phase"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(R53Phase(R53Action(TEXT("round5_continued_doors"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
        { return R53SavedDoors(Sub, *State); }), State, true));
    Out.Add(R53Phase(Stationery::JumpClockStep(TEXT("round5_browsing_1030"), 10, 30), State, false));
    Out.Add(R53Phase(R53Action(TEXT("round5_declared_start"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Row = R53Row(TEXT("cloth_merchant"));
        auto* Pawn = Sub.GetPawn();
        if (!Row || !Pawn) return FStepOutcome::Fail(TEXT("missing pawn or catalogue row"));
        if (Rt.Phase == 0)
        {
            const FVector Start = R53Point(*Row, Row->EntranceCentreU, -110) + FVector(0, 0, PWPlayer::CapsuleHalfHeight + 5);
            if (!Pawn->TeleportTo(Start, FRotator(0, -90, 0))) return FStepOutcome::Fail(TEXT("declared start blocked"));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        if (Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround()) return FStepOutcome::Wait();
        return FStepOutcome::Pass(TEXT("sole declared teleport; remaining movement walked"));
    }), State, false));
    double PreviousX = -2150;
    int32 PreviousSide = -1;
    for (const FString Name : {FString(TEXT("cloth_merchant")), FString(TEXT("narrow_shop")), FString(TEXT("tailor_shop"))})
    {
        const auto* Row = R53Row(Name);
        if (!Row)
        {
            Out.Add(R53Action(Name + TEXT("_missing"), [](UPWAcceptanceSubsystem&, FStepRuntime&)
                { return FStepOutcome::Fail(TEXT("catalogue row missing")); }));
            continue;
        }
        const double X = Row->OriginXCm + Row->EntranceCentreU;
        // The lane clock runs one game minute per real second and the player walks 1.4 m/s, so the three shops cannot all be finished
        // inside the 10:30-13:00 morning window that the first jump gives (the last leaf re-open would fall after 13:00, when all three
        // doors are closed to visitors). Re-declare the same Tuesday at 15:30, inside every afternoon window (15:00-19:30 at least),
        // once the first shop is done and the world is at rest; the leaves keep their authored state across the jump.
        if (Name == TEXT("narrow_shop"))
            Out.Add(R53Phase(PWAcceptance::Homes::HmClockStep(TEXT("round5_browsing_1530"), 1, 15, 30), State, false));
        // Keep clear of the kerb poles/trunks even on the same side; no direct diagonal through another frontage.
        Out.Add(R53Phase(R53Walk(Name + TEXT("_road_edge"), FVector(PreviousX, PreviousSide * 340, 15)), State, false));
        if (PreviousSide != Row->Side)
            Out.Add(R53Phase(R53Walk(Name + TEXT("_cross_lane"), FVector(PreviousX, Row->Side * 340, 15)), State, false));
        Out.Add(R53Phase(R53Walk(Name + TEXT("_along_pavement"), FVector(X, Row->Side * 340, 15)), State, false));
        Out.Add(R53Phase(R53Walk(Name + TEXT("_approach"), R53Point(*Row, Row->EntranceCentreU, -110)), State, false));
        Out.Add(R53Phase(R53Leaf(Name, false, TEXT("_initial_close")), State, false));
        Out.Add(R53Phase(R53Leaf(Name, true, TEXT("_open")), State, false));
        for (const bool bThird : {false, true})
        {
            const FString Camera = bThird ? TEXT("_third") : TEXT("_first");
            Out.Add(R53Phase(R53Action(Name + Camera, [bThird](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
            {
                if (!Sub.GetPawn()) return FStepOutcome::Fail(TEXT("no pawn"));
                Sub.GetPawn()->SetCameraMode(bThird ? EPWCameraMode::ThirdPerson : EPWCameraMode::FirstPerson);
                return FStepOutcome::Pass(TEXT("camera selected"));
            }), State, false));
            Out.Add(R53Phase(R53Inside(Name, false, Camera), State, false));
            Out.Add(R53Phase(R53Browse(Name, Camera + TEXT("_browse")), State, false));
            Out.Add(R53Phase(MakeShotStep(Name + Camera + TEXT("_interior")), State, false));
            Out.Add(R53Phase(R53Inside(Name, true, Camera), State, false));
        }
        Out.Add(R53Phase(R53Leaf(Name, false, TEXT("_close_after_exit")), State, false));
        if (Name != TEXT("narrow_shop")) Out.Add(R53Phase(R53Leaf(Name, true, TEXT("_saved_open")), State, false));
        PreviousX = X;
        PreviousSide = Row->Side;
    }
    Out.Add(R53Phase(R53Action(TEXT("round5_save_mixed_leaves"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (!Sub.GetHud()) return FStepOutcome::Fail(TEXT("no HUD save interface"));
        bool bSaved = false;
        const FText Why = Sub.GetHud()->SaveActiveGame(bSaved);
        if (!bSaved || !R53ReadSlot(Sub, State->Saved)) return FStepOutcome::Fail(Why.ToString());
        return R53SavedDoors(Sub, *State);
    }), State, false));
    Out.Add(R53Phase(MakeShotStep(TEXT("round5_continued_leaves")), State, true));
}
} // namespace PWAcceptance
