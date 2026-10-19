#include "PerfectWorldAcceptanceSubsystem.h"

#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifePersistence.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNirmalBindings.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSubrataBindings.h"

// AW-02 launches this once with Start=new, exits, then launches a FRESH process with Start=continue.
// No in-process restart or second load is used. Expected state comes from the actual slot before Continue.
namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;
constexpr const TCHAR* NbFact = TEXT("fact.nirmal_bose.book_evening_agreed");
constexpr const TCHAR* NbPaid = TEXT("choice.nirmal_bose.books");

struct NbRestart
{
    PerfectWorld::WorldSaveData Saved;
    bool bContinue = false;
    FVector Front = FVector::ZeroVector;
    FVector Unit = FVector::ZeroVector;
};

APWDoorActor* NbDoor(UPWAcceptanceSubsystem& Sub, const TCHAR* Id)
{
    for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
        if (It->GetDoorId() == FName(Id)) { return *It; }
    return nullptr;
}

bool NbReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out).Succeeded();
}

FStepDef NbPhase(FStepDef Def, TSharedRef<NbRestart> State, bool bContinue)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("belongs to the other fresh-process phase"));
    };
    return Def;
}

FStepDef NbAction(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 20;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepDef NbLeaf(const TCHAR* Name, const TCHAR* Id, bool bOpen)
{
    return PWAcceptance::MakeInteractStep(Name,
        [Id](UPWAcceptanceSubsystem& Sub) -> AActor* { return NbDoor(Sub, Id); },
        [Id, bOpen](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Door = NbDoor(Sub, Id);
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            if (!Door || !Life || Door->IsMoving() || Life->IsDoorOpen(FName(Id), !bOpen) != bOpen ||
                (bOpen ? Door->GetOpenFraction() < 0.99f : Door->GetOpenFraction() > 0.01f)) { return false; }
            Detail = FString::Printf(TEXT("%s settled %s, side %d"), Id, bOpen ? TEXT("open") : TEXT("closed"), Door->GetSwingSide());
            return true;
        }, nullptr, 15);
}

// First use must knock and leave the closed leaf unchanged; the next use can open it. This exercises the bridge's
// transient approach permission, not an invented DoorUseContext. In phase 2 this distinguishes a fresh process from stale admission.
FStepDef NbKnock(const TCHAR* Name)
{
    return PWAcceptance::MakeInteractStep(Name,
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return NbDoor(Sub, PWLane::NirmalDoorId); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Door = NbDoor(Sub, PWLane::NirmalDoorId);
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            const auto* Interaction = Sub.GetInteraction();
            if (!Door || !Life || !Interaction || Door->IsMoving() || Life->IsDoorOpen(FName(PWLane::NirmalDoorId), true) ||
                !Interaction->GetNotice().ToString().Contains(TEXT("Someone answers. You may come in."))) { return false; }
            Detail = TEXT("admitted knock, leaf still closed; opening requires a second use");
            return true;
        }, nullptr, 10);
}

FStepOutcome NbHistory(UPWAcceptanceSubsystem& Sub, const NbRestart& State)
{
    auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    const auto* Current = Life ? Life->FindConversation(FName(PWLane::NirmalPersonId)) : nullptr;
    const auto Old = State.Saved.World.Conversations.find("person.nirmal_bose");
    if (!Current || Old == State.Saved.World.Conversations.end() || Current->Open ||
        !PerfectWorld::HasConversationFact(*Current, TCHAR_TO_UTF8(NbFact)) || Current->Receipts.size() != 1 ||
        Current->Facts != Old->second.Facts || Current->Receipts != Old->second.Receipts ||
        Current->TakenChoices != Old->second.TakenChoices || Current->SessionCount != Old->second.SessionCount)
    { return FStepOutcome::Fail(TEXT("saved memory, closed session or exactly one original receipt did not survive")); }
    return FStepOutcome::Pass(TEXT("agreement and original paid receipt survived unchanged; saved session closed"));
}
}

namespace PWAcceptance
{
// Integrator hook: declare this beside AppendStationeryShiftSteps and dispatch save_restart in BuildScenario.
void AppendSaveRestartSteps(TArray<FStepDef>& Out)
{
    const auto State = MakeShared<NbRestart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(NbAction(TEXT("restart_phase_and_slot"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("save_restart requires AW-02 --restart-pair (explicit new then continue)"));
        State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        PWLane::FLaneBuilding Tall;
        if (!PWLane::MakeSubrataBuilding(PWLane::SubrataTallFlatsId, Tall))
            return FStepOutcome::Fail(TEXT("tall flats catalogue row missing"));
        const auto Provider = PWLane::NpNirmalBindings(Tall);
        const auto* Front = Provider.Anchors.Find(FName(PWLane::NirmalFrontId));
        const auto* Unit = Provider.Anchors.Find(FName(PWLane::NirmalDoorstepId));
        if (!Front || !Unit) return FStepOutcome::Fail(TEXT("unit A passage anchors missing"));
        State->Front = Front->Feet.GetLocation();
        State->Unit = Unit->Feet.GetLocation();
        if (State->bContinue && !NbReadSlot(Sub, State->Saved))
            return FStepOutcome::Fail(TEXT("phase 2 cannot read the phase 1 slot before Continue"));
        return FStepOutcome::Pass(State->bContinue ? TEXT("fresh-process Continue phase; original slot captured") : TEXT("new-life phase"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(NbPhase(NbAction(TEXT("continued_placement_doors_history"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Pawn = Sub.GetPawn();
        const auto* Player = Sub.GetController();
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Pawn || !Player || !Life) return FStepOutcome::Fail(TEXT("missing restored world"));
        const auto& Saved = State->Saved.Player;
        const FVector Desired(Saved.PositionCm[0], Saved.PositionCm[1], Saved.PositionCm[2]);
        const FRotator Look(Saved.PitchCdeg / 100.0, Saved.YawCdeg / 100.0, 0);
        if (!Pawn->GetActorLocation().Equals(Desired, 3) || !Player->GetControlRotation().Equals(Look, 0.1) ||
            Pawn->IsFirstPerson() != (Saved.Camera == PerfectWorld::CameraMode::FirstPerson))
            return FStepOutcome::Fail(TEXT("Continue did not restore saved position, view and camera"));
        if (Life->HasMovingDoors() || Life->GetWorldState().Doors != State->Saved.World.Doors)
            return FStepOutcome::Fail(TEXT("door authority differs from the saved settled state"));
        for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
        {
            const std::string Id(TCHAR_TO_UTF8(*It->GetDoorId().ToString()));
            for (const auto& Entry : State->Saved.World.Doors.Doors)
                if (Entry.Id == Id && (It->IsMoving() || It->GetSwingSide() != Entry.SwingSide ||
                    (Entry.Open ? It->GetOpenFraction() < 0.99f : It->GetOpenFraction() > 0.01f)))
                    return FStepOutcome::Fail(TEXT("a presented leaf disagrees with saved side/open state"));
        }
        return NbHistory(Sub, *State);
    }), State, true));
    Out.Add(NbPhase(NbAction(TEXT("declared_start_at_tall_flats_1710"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        if (!Life || !Sub.GetPawn() || !Presence || !Presence->IsRegistered(FName(PWLane::NirmalPersonId)))
            return FStepOutcome::Fail(TEXT("Nirmal must be registered by the production lane builder"));
        auto Sim = MakeUnique<PerfectWorld::WorldSim>(Life->GetWorldState());
        const auto Target = PerfectWorld::GameClock::FromCalendar(PerfectWorld::CampaignStartYear,
            PerfectWorld::CampaignStartMonth, PerfectWorld::CampaignStartDay, 17, 10);
        for (int32 Round = 0; Round < 64; ++Round)
        {
            if (!PerfectWorld::IsWorldQuiescent(Sim->State()))
            {
                if (!Sim->Submit({PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}}).Accepted)
                    return FStepOutcome::Fail(TEXT("time settlement refused"));
            }
            else if (Sim->State().Clock.Now() < Target)
            {
                if (!Sim->Submit({PerfectWorld::CommandSource::System, PerfectWorld::AdvanceTime{Target - Sim->State().Clock.Now()}}).Accepted)
                    return FStepOutcome::Fail(TEXT("time advance refused"));
            }
            else break;
        }
        if (Sim->State().Clock.Now() != Target || !PerfectWorld::IsWorldQuiescent(Sim->State()))
            return FStepOutcome::Fail(TEXT("17:10 target not reached"));
        Life->AdoptSavedWorld(MoveTemp(Sim));
        // Sole scenario-start teleport. All travel below is real walking through the public front door.
        const FVector Start = State->Front + FVector(0, -100, PWPlayer::CapsuleHalfHeight + 2);
        if (!Sub.GetPawn()->TeleportTo(Start, FRotator::ZeroRotator, false, false))
            return FStepOutcome::Fail(TEXT("declared start obstructed"));
        Sub.GetPawn()->SetCameraMode(EPWCameraMode::ThirdPerson);
        return FStepOutcome::Pass(TEXT("17:10 through core time commands; declared start outside tall flats"));
    }), State, false));
    Out.Add(NbPhase(NbLeaf(TEXT("open_public_front"), PWLane::SubrataTallFlatsDoorId, true), State, false));
    Out.Add(NbPhase(MakeWalkStep(TEXT("walk_passage_to_unit_a"), [State](UPWAcceptanceSubsystem&, FVector& Target, FString&)
    { Target = State->Unit; return true; }, 12, 25), State, false));
    Out.Add(NbPhase(NbKnock(TEXT("first_unit_a_knock")), State, false));
    Out.Add(NbPhase(NbLeaf(TEXT("open_unit_a"), PWLane::NirmalDoorId, true), State, false));
    Out.Add(NbPhase(NbLeaf(TEXT("close_unit_a"), PWLane::NirmalDoorId, false), State, false));
    Out.Add(NbPhase(NbAction(TEXT("remember_agreement_and_paid_receipt"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        // Off-lane home has no rendered resident. Use the production conversation-panel seam after reaching his door.
        if (!Life || !Talk || !Talk->BeginConversationWith(FName(PWLane::NirmalPersonId), FText::FromString(TEXT("Nirmal"))))
            return FStepOutcome::Fail(TEXT("Nirmal conversation refused"));
        Talk->ChooseOption(FName(TEXT("choice.nirmal_bose.agree")));
        const auto Before = Life->GetNow();
        Talk->ChooseOption(FName(NbPaid));
        const auto* Memory = Life->FindConversation(FName(PWLane::NirmalPersonId));
        const bool Good = Memory && PerfectWorld::HasConversationFact(*Memory, TCHAR_TO_UTF8(NbFact)) &&
            Memory->Receipts.size() == 1 && Life->GetNow() == Before + 10;
        Talk->LeaveConversation();
        return Good ? FStepOutcome::Pass(TEXT("agreement earned and ten minutes charged once through the panel")) :
            FStepOutcome::Fail(TEXT("agreement/paid choice did not produce expected history"));
    }), State, false));
    Out.Add(NbPhase(NbKnock(TEXT("hold_permission_before_save")), State, false));
    Out.Add(NbPhase(MakeShotStep(TEXT("shot_before_save")), State, false));
    Out.Add(NbPhase(NbAction(TEXT("save_placement_and_settled_doors"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        bool Saved = false;
        const FText Why = Sub.GetHud()->SaveActiveGame(Saved);
        if (!Saved || !NbReadSlot(Sub, State->Saved)) return FStepOutcome::Fail(Why.ToString());
        return NbHistory(Sub, *State);
    }), State, false));
    // Phase 1 now reaches normal FinishScenario, atomic result completion and the driver's clean process exit.
    Out.Add(NbPhase(NbKnock(TEXT("continue_requires_fresh_knock")), State, true));
    Out.Add(NbPhase(NbLeaf(TEXT("continue_second_use_opens"), PWLane::NirmalDoorId, true), State, true));
    Out.Add(NbPhase(NbLeaf(TEXT("continue_close_unit"), PWLane::NirmalDoorId, false), State, true));
    Out.Add(NbPhase(NbAction(TEXT("stale_paid_callback_costs_nothing"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Life) return FStepOutcome::Fail(TEXT("no live world"));
        const auto Before = Life->GetNow();
        const auto Old = State->Saved.World.Conversations.find("person.nirmal_bose");
        if (Old == State->Saved.World.Conversations.end()) return FStepOutcome::Fail(TEXT("original session absent"));
        if (Life->ChooseConversation(FName(PWLane::NirmalPersonId), FName(NbPaid), Old->second.SessionCount).bAccepted || Life->GetNow() != Before)
            return FStepOutcome::Fail(TEXT("stale callback charged again"));
        return NbHistory(Sub, *State);
    }), State, true));
    Out.Add(NbPhase(MakeShotStep(TEXT("shot_continued_placement")), State, true));
    Out.Add(NbPhase(NbAction(TEXT("blocked_resume_uses_safe_origin"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        // Placement-only fault fixture: no in-process reload, no world/receipt reset, no save rewrite.
        UWorld* World = Sub.GetPlayWorld();
        const auto& Saved = State->Saved.Player;
        AActor* Obstacle = World->SpawnActor<AActor>();
        if (!Obstacle) return FStepOutcome::Fail(TEXT("cannot create blocked-placement fixture"));
        auto* Box = NewObject<UBoxComponent>(Obstacle);
        Obstacle->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(50, 50, 100));
        Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Box->SetCollisionResponseToAllChannels(ECR_Block);
        Box->RegisterComponent();
        Obstacle->SetActorLocation(FVector(Saved.PositionCm[0], Saved.PositionCm[1], Saved.PositionCm[2]));
        bool UsedOrigin = false;
        const bool Placed = PWLaneLifePersistence::PlacePlayerAtSavedPlacement(*Sub.GetController(), Saved, UsedOrigin);
        Obstacle->Destroy();
        if (!Placed || !UsedOrigin || FVector::Dist(Sub.GetPawn()->GetActorLocation(), State->Unit) < 300)
            return FStepOutcome::Fail(TEXT("blocked saved capsule did not fall back to a safe origin"));
        PerfectWorld::WorldSaveData Unchanged;
        if (!NbReadSlot(Sub, Unchanged) || Unchanged.Player != State->Saved.Player ||
            Unchanged.World.Conversations != State->Saved.World.Conversations || Unchanged.World.Doors != State->Saved.World.Doors)
            return FStepOutcome::Fail(TEXT("fallback rewrote the original slot"));
        return NbHistory(Sub, *State);
    }), State, true));
    Out.Add(NbPhase(MakeShotStep(TEXT("shot_blocked_resume_origin")), State, true));
}
}
