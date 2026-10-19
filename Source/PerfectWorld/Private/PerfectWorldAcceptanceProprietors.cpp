#include "PerfectWorldAcceptanceSubsystem.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameClock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "NpcBehaviour.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldProprietorBindings.h"
#include "WorldSave.h"

// proprietors_workday (R4-02): Monday at the two working shops. It is a fresh-process pair like save_restart (AW-02 --restart-pair:
// Start=new, then a FRESH process with Start=continue); each process lists the same planned steps and the steps of the other phase pass
// as "belongs to the other phase".
//
// New-life phase, at the repair workshop (Debashis Pal): the clock moves to 07:50 through the core's time commands and he is at the
// workbench preparing; at 08:00 he is at the staff counter (the door's opening); the player walks in through the real door, Talks,
// agrees to bring something in (a free choice, no time passes), Leaves, and the agreement is SAVED. He resumes the counter. The player
// then stands in the staff gap on his route to the workbench: he is held back (yield) and never walks through the player; when the
// player steps aside he completes the walk and is working at the workbench (blocked-route recovery). The player leaves the threshold
// clear, the clock moves to 11:55, he is at the counter and at 12:00 he is SEEN walking out and leaving the lane. At 12:30 the hardware
// store's Mahua Saha is observed at her counter and at 13:15 in the stock room at her meal, while Debashis is off the lane for the
// closure. Mahua is observed through the presence subsystem only (the lane's two shops are about five kilometres apart and the
// scenario has one declared start), so her step proves position, not a visit.
// Continue phase: the saved agreement and its (zero) receipts survive unchanged, he is back at the counter in the fresh process, the
// conversation remembers the agreement, the one paid choice charges ten minutes once and a duplicate costs nothing.
//
// Integrator hook (PerfectWorldAcceptanceSubsystem.h and PerfectWorldAcceptanceLaneScenario.cpp):
//     void AppendProprietorsWorkdaySteps(TArray<FStepDef>& OutSteps);       // beside AppendResidentHomesSteps
//     if (Name == TEXT("proprietors_workday")) { AppendProprietorsWorkdaySteps(OutSteps); return true; }   // in BuildScenario
namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

const FName PwDebashis(PWLane::DebashisPersonId);
const FName PwMahua(PWLane::MahuaPersonId);
constexpr const TCHAR* PwFact = TEXT("fact.debashis_pal.bring_promised");
constexpr const TCHAR* PwPromise = TEXT("choice.debashis_pal.promise_bring");
constexpr const TCHAR* PwPaid = TEXT("choice.debashis_pal.hold_lamp");
constexpr double PwAtAnchorCm = 150.0;
constexpr double PwNearCm = 300.0;
constexpr double PwCapsuleOverlapCm = 50.0;
constexpr double PwHoldSeconds = 3.0;

/** The workshop in world space, from the same provider the lane registers. */
struct FPwPlan
{
    FVector Door, Counter, Customer, Workbench, Junction, Beyond, DownDir, Out, Side;
};

bool PwMakePlan(FPwPlan& Plan, FString& Why)
{
    PWLane::FLaneBuilding Building;
    if (!PWLane::MakeProprietorBuilding(PWLane::DebashisBuildingId, Building)) { Why = TEXT("the workshop catalogue row does not resolve"); return false; }
    const FNpBindingProvider Provider = PWLane::NpDebashisBindings(Building);
    const FNpAnchorBinding* Door = Provider.Anchors.Find(FName(PWLane::DebashisAnchorFrontDoor));
    const FNpAnchorBinding* Counter = Provider.Anchors.Find(FName(PWLane::DebashisAnchorCounter));
    const FNpAnchorBinding* Customer = Provider.Anchors.Find(FName(PWLane::DebashisAnchorCustomer));
    const FNpAnchorBinding* Bench = Provider.Anchors.Find(FName(PWLane::DebashisAnchorWorkbench));
    const FNpRouteBinding* ToBench = Provider.Routes.Find(FName(TEXT("route.repair_workshop.counter_to_workbench")));
    const FNpRouteBinding* ToDoor = Provider.Routes.Find(FName(TEXT("route.repair_workshop.counter_to_door")));
    if (Door == nullptr || Counter == nullptr || Customer == nullptr || Bench == nullptr || ToBench == nullptr || ToDoor == nullptr ||
        ToBench->Points.Num() < 4 || ToDoor->Points.Num() < 5)
    {
        Why = TEXT("the workshop provider is missing an anchor or a route");
        return false;
    }
    Plan.Door = Door->Feet.GetLocation();
    Plan.Counter = Counter->Feet.GetLocation();
    Plan.Customer = Customer->Feet.GetLocation();
    Plan.Workbench = Bench->Feet.GetLocation();
    Plan.Junction = ToBench->Points[1];   // where the staff route turns into the gap column
    Plan.Beyond = ToDoor->Points[2];      // up the gap column toward the front: off the counter-to-bench line
    Plan.DownDir = (ToBench->Points[2] - ToBench->Points[1]).GetSafeNormal2D();
    Plan.Out = (Plan.Door - Plan.Counter).GetSafeNormal2D();
    Plan.Side = FVector(-Plan.Out.Y, Plan.Out.X, 0.0);
    return true;
}

struct FPwState
{
    PerfectWorld::WorldSaveData Saved;
    bool bContinue = false;
};

FStepDef PwAction(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run, double Timeout = 20.0)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = Timeout;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepDef PwPhase(FStepDef Def, TSharedRef<FPwState> State, bool bContinue)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("belongs to the other fresh-process phase"));
    };
    return Def;
}

APWNpcCharacter* PwFind(UPWAcceptanceSubsystem& Sub, const FName Person)
{
    UWorld* World = Sub.GetPlayWorld();
    const UPWNpcPresenceSubsystem* Presence = World != nullptr ? World->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
    return Presence != nullptr ? Presence->FindPerson(Person) : nullptr;
}

int32 PwMinuteOfDay(const UPWLaneLifeSubsystem& Life) { return static_cast<int32>(((Life.GetNow() % 1440) + 360) % 1440); }

FString PwClock(UPWAcceptanceSubsystem& Sub)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    return Life != nullptr ? FString::Printf(TEXT("clock %s, hold %d"), *Life->GetClockText().ToString(), static_cast<int32>(Life->GetClockHold())) : FString();
}

TUniquePtr<PerfectWorld::WorldSim> PwAdvancedCopy(const PerfectWorld::WorldState& Live, const PerfectWorld::GameInstant Target, FString& Why)
{
    auto Sim = MakeUnique<PerfectWorld::WorldSim>(Live);
    for (int32 Round = 0; Round < 64; ++Round)
    {
        if (!PerfectWorld::IsWorldQuiescent(Sim->State()))
        {
            if (!Sim->Submit({PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}}).Accepted) { Why = TEXT("time settlement refused"); return nullptr; }
        }
        else if (Sim->State().Clock.Now() < Target)
        {
            if (!Sim->Submit({PerfectWorld::CommandSource::System, PerfectWorld::AdvanceTime{Target - Sim->State().Clock.Now()}}).Accepted) { Why = TEXT("time advance refused"); return nullptr; }
        }
        else { break; }
    }
    if (Sim->State().Clock.Now() != Target || !PerfectWorld::IsWorldQuiescent(Sim->State())) { Why = TEXT("the target instant was not reached"); return nullptr; }
    return Sim;
}

/** Moves the lane clock to Monday (the campaign's first day) at Hour:Minute through time.advance and AdoptSavedWorld; waits while a door moves or someone talks. */
FStepDef PwJumpClock(const TCHAR* Name, const int32 Hour, const int32 Minute)
{
    return PwAction(Name, [Hour, Minute](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Life == nullptr || Pawn == nullptr) { return FStepOutcome::Fail(TEXT("no lane-life subsystem or no pawn")); }
        if (Pawn->IsSeated() || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty() || Life->HasMovingDoors())
        {
            Rt.Note = TEXT("waiting for the world to be at rest (a door moving or a conversation open)");
            return FStepOutcome::Wait();
        }
        const PerfectWorld::GameInstant Target = PerfectWorld::GameClock::FromCalendar(
            PerfectWorld::CampaignStartYear, PerfectWorld::CampaignStartMonth, PerfectWorld::CampaignStartDay, Hour, Minute);
        if (PerfectWorld::GameClock::ToCalendar(Target).DayOfWeek != PerfectWorld::Weekday::Monday) { return FStepOutcome::Fail(TEXT("the target day is not a Monday")); }
        if (Life->GetNow() >= Target) { return FStepOutcome::Fail(FString::Printf(TEXT("the clock already stands at %s"), *Life->GetClockText().ToString())); }
        FString Why;
        TUniquePtr<PerfectWorld::WorldSim> Moved = PwAdvancedCopy(Life->GetWorldState(), Target, Why);
        if (!Moved.IsValid()) { return FStepOutcome::Fail(Why); }
        Life->AdoptSavedWorld(MoveTemp(Moved));
        return FStepOutcome::Pass(FString::Printf(TEXT("clock moved to %s through time.advance and AdoptSavedWorld"), *Life->GetClockText().ToString()));
    }, 30.0);
}

/** Passes when the person is presented within Tolerance of the point and the clock is at or after MinMinute (minutes of the day; -1 for any). */
FStepDef PwObserve(const TCHAR* Name, const FName Person, TFunction<bool(FVector&)> Where, const int32 MinMinute, const double Timeout)
{
    return PwAction(Name, [Person, Where, MinMinute](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        FVector Point;
        if (Life == nullptr || !Where(Point)) { return FStepOutcome::Fail(TEXT("the anchor does not resolve")); }
        const APWNpcCharacter* Npc = PwFind(Sub, Person);
        if (Npc == nullptr) { Rt.Note = FString::Printf(TEXT("%s is not on the lane; %s"), *Person.ToString(), *PwClock(Sub)); return FStepOutcome::Wait(); }
        const double Away = FVector::Dist2D(Npc->GetActorLocation(), Point);
        Rt.Note = FString::Printf(TEXT("%s %.0f cm from the anchor; %s"), *Person.ToString(), Away, *PwClock(Sub));
        if (Away > PwAtAnchorCm || PwMinuteOfDay(*Life) < MinMinute) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("%s (%.0f cm tall) %.0f cm from the anchor; %s"), *Person.ToString(), Npc->GetStandingHeightCm(), Away, *PwClock(Sub)));
    }, Timeout);
}

FStepDef PwAbsent(const TCHAR* Name, const FName Person)
{
    return PwAction(Name, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (PwFind(Sub, Person) != nullptr) { Rt.Note = FString::Printf(TEXT("%s is still on the lane; %s"), *Person.ToString(), *PwClock(Sub)); return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("%s is off the lane (the closure); %s"), *Person.ToString(), *PwClock(Sub)));
    }, 60.0);
}

bool PwConversationIs(UPWAcceptanceSubsystem& Sub, const FName Person)
{
    UWorld* World = Sub.GetPlayWorld();
    const UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(World);
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
    return Presenter != nullptr && Presenter->IsOpen() && Life != nullptr &&
           FName(UTF8_TO_TCHAR(std::string(PerfectWorld::OpenConversationPersonId(Life->GetWorldState())).c_str())) == Person;
}

bool PwReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
           PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out).Succeeded();
}

FStepOutcome PwHistory(UPWAcceptanceSubsystem& Sub, const FPwState& State)
{
    const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    const auto* Current = Life != nullptr ? Life->FindConversation(PwDebashis) : nullptr;
    const auto Old = State.Saved.World.Conversations.find("person.debashis_pal");
    if (Current == nullptr || Old == State.Saved.World.Conversations.end() || Current->Open || !PerfectWorld::HasConversationFact(*Current, TCHAR_TO_UTF8(PwFact)) ||
        Current->Facts != Old->second.Facts || Current->Receipts != Old->second.Receipts || Current->SessionCount != Old->second.SessionCount)
    {
        return FStepOutcome::Fail(TEXT("the saved agreement, its closed session or its receipts did not survive unchanged"));
    }
    return FStepOutcome::Pass(FString::Printf(TEXT("agreement survived unchanged: %d receipts, session %d"), static_cast<int32>(Current->Receipts.size()), static_cast<int32>(Current->SessionCount)));
}

FStepDef PwWalkTo(const TCHAR* Name, TFunction<FVector(const FPwPlan&)> Pick, const double Tolerance, const double Timeout)
{
    return PWAcceptance::MakeWalkStep(Name, [Pick](UPWAcceptanceSubsystem&, FVector& Target, FString& Why)
    {
        FPwPlan Plan;
        if (!PwMakePlan(Plan, Why)) { return false; }
        Target = Pick(Plan);
        return true;
    }, Tolerance, Timeout);
}

APWDoorActor* PwDoor(UWorld* World)
{
    for (TActorIterator<APWDoorActor> It(World); It; ++It)
        if (It->GetDoorId() == FName(PWLane::DebashisDoorId)) { return *It; }
    return nullptr;
}

/** The player holds the staff gap while he comes down it: he must stay on the counter side and never overlap the player. */
FStepDef PwYieldStep()
{
    return PwAction(TEXT("debashis_yields_in_staff_gap"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        FPwPlan Plan;
        FString Why;
        const APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr || !PwMakePlan(Plan, Why)) { return FStepOutcome::Fail(Pawn == nullptr ? FString(TEXT("no pawn")) : Why); }
        if (!Rt.bHaveTarget) { Rt.Target = FVector(1.0e9, 0.0, 0.0); Rt.bHaveTarget = true; }
        if (FVector::Dist2D(Pawn->GetActorLocation(), Plan.Junction) > 80.0) { return FStepOutcome::Fail(TEXT("the player is no longer standing in the staff gap")); }
        const APWNpcCharacter* Npc = PwFind(Sub, PwDebashis);
        if (Npc == nullptr) { Rt.Note = TEXT("Debashis is not on the lane"); return FStepOutcome::Wait(); }
        const FVector At = Npc->GetActorLocation();
        const double Apart = FVector::Dist2D(At, Pawn->GetActorLocation());
        Rt.Target.X = FMath::Min(Rt.Target.X, Apart);
        Rt.Note = FString::Printf(TEXT("Debashis %.0f cm from the player, closest %.0f, near for %.1f s, yielding %d; %s"), Apart, Rt.Target.X, Rt.Target.Y,
                                  Npc->GetNpcMovement()->IsYielding(), *PwClock(Sub));
        if (FVector::DotProduct(At - Plan.Junction, Plan.DownDir) > 30.0) { return FStepOutcome::Fail(FString::Printf(TEXT("he is past the player in the gap: %s"), *Rt.Note)); }
        if (Apart < PwCapsuleOverlapCm) { return FStepOutcome::Fail(FString::Printf(TEXT("he is inside the player's capsule: %s"), *Rt.Note)); }
        if (Apart <= PwNearCm) { Rt.Target.Y += Rt.Dt; }
        if (Rt.Target.Y < PwHoldSeconds) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("held back %.1f s within %.0f cm of the player, never closer than %.0f cm; %s"), Rt.Target.Y, PwNearCm, Rt.Target.X, *Rt.Note));
    }, 150.0);
}

/** After a pause (conversation or block): back within PwAtAnchorCm of the anchor, out of the Chat pose and settled for 2 s. */
FStepDef PwResume(const TCHAR* Name, TFunction<FVector(const FPwPlan&)> Pick)
{
    return PwAction(Name, [Pick](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        FPwPlan Plan;
        FString Why;
        if (!PwMakePlan(Plan, Why)) { return FStepOutcome::Fail(Why); }
        const APWNpcCharacter* Npc = PwFind(Sub, PwDebashis);
        if (Npc == nullptr) { Rt.Phase = 0; Rt.Note = TEXT("Debashis is not on the lane"); return FStepOutcome::Wait(); }
        const double Away = FVector::Dist2D(Npc->GetActorLocation(), Pick(Plan));
        const EPWNpcState State = Npc->GetNpcMovement()->GetPresentedState();
        Rt.Note = FString::Printf(TEXT("Debashis %.0f cm from the anchor, state %d; %s"), Away, static_cast<int32>(State), *PwClock(Sub));
        if (Away > PwAtAnchorCm || State == EPWNpcState::Chat) { Rt.Phase = 0; return FStepOutcome::Wait(); }
        if (Rt.Phase == 0) { Rt.NextPhase(); return FStepOutcome::Wait(); }
        return Rt.InPhase() >= 2.0 ? FStepOutcome::Pass(Rt.Note) : FStepOutcome::Wait();
    }, 90.0);
}

/** At closing he is seen at least 150 cm from the counter and within 250 cm of the threshold, then gone from the lane: walked out, not removed. */
FStepDef PwDeparture()
{
    return PwAction(TEXT("debashis_walks_out_at_closing"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        FPwPlan Plan;
        FString Why;
        if (!PwMakePlan(Plan, Why)) { return FStepOutcome::Fail(Why); }
        if (!Rt.bHaveTarget) { Rt.Target = FVector(0.0, 1.0e9, 0.0); Rt.bHaveTarget = true; }
        const APWNpcCharacter* Npc = PwFind(Sub, PwDebashis);
        if (Npc == nullptr)
        {
            if (Rt.Target.X < 150.0)
            {
                return Rt.Elapsed < 3.0 ? FStepOutcome::Wait()
                                        : FStepOutcome::Fail(FString::Printf(TEXT("he vanished after moving only %.0f cm from the counter: removed, not walked; %s"), Rt.Target.X, *PwClock(Sub)));
            }
            if (Rt.Target.Y > 250.0) { return FStepOutcome::Fail(FString::Printf(TEXT("he left but was never within 250 cm of the threshold (closest %.0f)"), Rt.Target.Y)); }
            return FStepOutcome::Pass(FString::Printf(TEXT("walked %.0f cm from the counter, came within %.0f cm of the threshold and left the lane; %s"), Rt.Target.X, Rt.Target.Y, *PwClock(Sub)));
        }
        const FVector At = Npc->GetActorLocation();
        Rt.Target.X = FMath::Max(Rt.Target.X, FVector::Dist2D(At, Plan.Counter));
        Rt.Target.Y = FMath::Min(Rt.Target.Y, FVector::Dist2D(At, Plan.Door));
        Rt.Note = FString::Printf(TEXT("at (%.0f, %.0f), farthest from the counter %.0f, closest to the threshold %.0f; %s"), At.X, At.Y, Rt.Target.X, Rt.Target.Y, *PwClock(Sub));
        return FStepOutcome::Wait();
    }, 150.0);
}

bool PwMahuaPoint(const TCHAR* AnchorId, FVector& Out)
{
    PWLane::FLaneBuilding Building;
    if (!PWLane::MakeProprietorBuilding(PWLane::MahuaBuildingId, Building)) { return false; }
    const FNpBindingProvider Provider = PWLane::NpMahuaBindings(Building);
    const FNpAnchorBinding* Anchor = Provider.Anchors.Find(FName(AnchorId));
    if (Anchor == nullptr) { return false; }
    Out = Anchor->Feet.GetLocation();
    return true;
}
} // namespace

namespace PWAcceptance
{
void AppendProprietorsWorkdaySteps(TArray<FStepDef>& Out)
{
    const auto State = MakeShared<FPwState>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(PwAction(TEXT("restart_phase_and_slot"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("proprietors_workday requires AW-02 --restart-pair (explicit new then continue)"));
        State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        FPwPlan Plan;
        FString Why;
        if (!PwMakePlan(Plan, Why)) { return FStepOutcome::Fail(Why); }
        FVector Unused;
        if (!PwMahuaPoint(PWLane::MahuaAnchorCounter, Unused)) { return FStepOutcome::Fail(TEXT("the hardware store provider does not resolve")); }
        if (State->bContinue && !PwReadSlot(Sub, State->Saved)) { return FStepOutcome::Fail(TEXT("the continue phase cannot read the first phase's slot")); }
        return FStepOutcome::Pass(State->bContinue ? TEXT("fresh-process Continue phase; the original slot was captured") : TEXT("new-life phase"));
    }));
    Out.Add(MakeEnterWorldStep());

    // ---- fresh-process Continue phase
    Out.Add(PwPhase(PwAction(TEXT("continued_agreement_history"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&) { return PwHistory(Sub, *State); }), State, true));
    Out.Add(PwPhase(PwObserve(TEXT("continued_debashis_at_counter"), PwDebashis,
        [](FVector& P) { FPwPlan Plan; FString Why; if (!PwMakePlan(Plan, Why)) { return false; } P = Plan.Counter; return true; }, -1, 60.0), State, true));
    Out.Add(PwPhase(PwAction(TEXT("remembered_agreement_and_one_charge"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Talk == nullptr || !Talk->BeginConversationWith(PwDebashis, FText::FromString(TEXT("Debashis")))) { return FStepOutcome::Fail(TEXT("Debashis's conversation was refused")); }
        const auto* Open = Life->FindConversation(PwDebashis);
        if (Open == nullptr || !Open->Open || FString(UTF8_TO_TCHAR(Open->NodeId.c_str())).Contains(TEXT("remembered")) == false)
        {
            Talk->LeaveConversation();
            return FStepOutcome::Fail(TEXT("the opening line does not remember the agreement"));
        }
        const uint32 Session = Open->SessionCount;
        const auto Before = Life->GetNow();
        Talk->ChooseOption(FName(PwPaid));
        const auto* After = Life->FindConversation(PwDebashis);
        const bool bCharged = After != nullptr && After->Receipts.size() == 1 && Life->GetNow() == Before + 10;
        const auto Clock = Life->GetNow();
        const bool bDuplicate = Life->ChooseConversation(PwDebashis, FName(PwPaid), Session).bAccepted || Life->GetNow() != Clock;
        Talk->LeaveConversation();
        if (!bCharged) { return FStepOutcome::Fail(TEXT("the paid choice did not charge ten minutes exactly once")); }
        if (bDuplicate) { return FStepOutcome::Fail(TEXT("a duplicate of the paid choice was accepted or charged")); }
        return FStepOutcome::Pass(TEXT("the line remembered the agreement; ten minutes were charged once and the duplicate was refused for nothing"));
    }), State, true));
    Out.Add(PwPhase(MakeShotStep(TEXT("shot_continued_workshop")), State, true));

    // ---- new-life phase: the workday
    Out.Add(PwPhase(PwAction(TEXT("declared_start_outside_workshop"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        FPwPlan Plan;
        FString Why;
        if (Pawn == nullptr || !PwMakePlan(Plan, Why)) { return FStepOutcome::Fail(Pawn == nullptr ? FString(TEXT("no pawn")) : Why); }
        if (Rt.Phase == 0)
        {
            const FVector Start = Plan.Door + Plan.Out * 150.0 + FVector(0.0, 0.0, PWPlayer::CapsuleHalfHeight + 5.0);
            Pawn->TeleportTo(Start, FRotator::ZeroRotator); // the scenario's one teleport
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Sub.FaceWorldPoint(Plan.Door);
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        return Rt.InPhase() < 0.8 || !Pawn->GetCharacterMovement()->IsMovingOnGround() ? FStepOutcome::Wait() : FStepOutcome::Pass(TEXT("declared start 150 cm in front of the workshop threshold"));
    }), State, false));
    Out.Add(PwPhase(PwJumpClock(TEXT("clock_to_monday_0750"), 7, 50), State, false));
    Out.Add(PwPhase(PwObserve(TEXT("debashis_prepares_at_workbench"), PwDebashis,
        [](FVector& P) { FPwPlan Plan; FString Why; if (!PwMakePlan(Plan, Why)) { return false; } P = Plan.Workbench; return true; }, -1, 60.0), State, false));
    Out.Add(PwPhase(PwObserve(TEXT("debashis_at_counter_when_open"), PwDebashis,
        [](FVector& P) { FPwPlan Plan; FString Why; if (!PwMakePlan(Plan, Why)) { return false; } P = Plan.Counter; return true; }, 8 * 60, 120.0), State, false));
    Out.Add(PwPhase(PwWalkTo(TEXT("walk_to_threshold"), [](const FPwPlan& Plan) { return Plan.Door; }, 60.0, 25.0), State, false));
    Out.Add(PwPhase(PWAcceptance::MakeInteractStep(TEXT("open_workshop_door"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return PwDoor(Sub.GetPlayWorld()); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const APWDoorActor* Door = PwDoor(Sub.GetPlayWorld());
            if (Door == nullptr || !Door->IsOpen()) { return false; }
            Detail = TEXT("door.nabapara.repair_workshop_front is open");
            return true;
        },
        [](UPWAcceptanceSubsystem& Sub) { const APWDoorActor* Door = PwDoor(Sub.GetPlayWorld()); return Door != nullptr && Door->IsOpen(); }, 15.0), State, false));
    Out.Add(PwPhase(PwWalkTo(TEXT("walk_to_customer_spot"), [](const FPwPlan& Plan) { return Plan.Customer; }, 60.0, 30.0), State, false));
    Out.Add(PwPhase(PwAction(TEXT("face_counter"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        FPwPlan Plan;
        FString Why;
        if (!PwMakePlan(Plan, Why)) { return FStepOutcome::Fail(Why); }
        Sub.FaceWorldPoint(Plan.Counter);
        return FStepOutcome::Pass(TEXT("the view is turned to the staff counter"));
    }), State, false));
    Out.Add(PwPhase(MakeShotStep(TEXT("shot_debashis_at_counter")), State, false));
    Out.Add(PwPhase(PWAcceptance::MakeInteractStep(TEXT("talk_to_debashis"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return PwFind(Sub, PwDebashis); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            if (!PwConversationIs(Sub, PwDebashis)) { return false; }
            Detail = TEXT("the conversation panel is open and the core's open conversation is Debashis");
            return true;
        }, nullptr, 20.0), State, false));
    Out.Add(PwPhase(PwAction(TEXT("agree_to_bring_something_in"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Talk == nullptr || !PwConversationIs(Sub, PwDebashis)) { return FStepOutcome::Fail(TEXT("no open conversation with Debashis")); }
        const auto Before = Life->GetNow();
        Talk->ChooseOption(FName(PwPromise));
        const auto* Memory = Life->FindConversation(PwDebashis);
        if (Memory == nullptr || !PerfectWorld::HasConversationFact(*Memory, TCHAR_TO_UTF8(PwFact)) || !Memory->Receipts.empty() || Life->GetNow() != Before)
            return FStepOutcome::Fail(TEXT("agreeing did not set exactly the fact, free of charge"));
        return FStepOutcome::Pass(TEXT("the agreement is remembered; no receipt and no time passed"));
    }), State, false));
    Out.Add(PwPhase(MakeShotStep(TEXT("shot_debashis_conversation")), State, false));
    Out.Add(PwPhase(PwAction(TEXT("leave_debashis"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        UPWConversationPresenter* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Talk == nullptr || Life == nullptr) { return FStepOutcome::Fail(TEXT("no conversation presenter")); }
        if (Rt.Phase == 0)
        {
            if (!PwConversationIs(Sub, PwDebashis)) { return FStepOutcome::Fail(TEXT("no open conversation to leave")); }
            Talk->LeaveConversation();
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        return Talk->IsOpen() || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty() ? FStepOutcome::Wait() : FStepOutcome::Pass(TEXT("Leave used; the core has no open conversation"));
    }), State, false));
    Out.Add(PwPhase(PwAction(TEXT("save_agreement"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        bool bSaved = false;
        const FText Why = Sub.GetHud()->SaveActiveGame(bSaved);
        if (!bSaved || !PwReadSlot(Sub, State->Saved)) { return FStepOutcome::Fail(Why.ToString()); }
        return PwHistory(Sub, *State);
    }), State, false));
    Out.Add(PwPhase(PwResume(TEXT("debashis_resumes_counter"), [](const FPwPlan& Plan) { return Plan.Counter; }), State, false));
    // The staff gap: walk the route the front door line takes (up the gap column) and stand where his route to the workbench turns.
    Out.Add(PwPhase(PwWalkTo(TEXT("walk_to_staff_gap"), [](const FPwPlan& Plan) { return Plan.Beyond; }, 40.0, 30.0), State, false));
    Out.Add(PwPhase(PwWalkTo(TEXT("stand_in_staff_gap"), [](const FPwPlan& Plan) { return Plan.Junction; }, 30.0, 20.0), State, false));
    Out.Add(PwPhase(PwYieldStep(), State, false));
    Out.Add(PwPhase(MakeShotStep(TEXT("shot_gap_yield")), State, false));
    Out.Add(PwPhase(PwWalkTo(TEXT("step_aside_up_the_gap"), [](const FPwPlan& Plan) { return Plan.Beyond; }, 40.0, 20.0), State, false));
    Out.Add(PwPhase(PwResume(TEXT("debashis_recovers_to_workbench"), [](const FPwPlan& Plan) { return Plan.Workbench; }), State, false));
    Out.Add(PwPhase(MakeShotStep(TEXT("shot_debashis_at_workbench")), State, false));
    Out.Add(PwPhase(PwWalkTo(TEXT("walk_out_of_the_way"), [](const FPwPlan& Plan) { return Plan.Door + Plan.Out * 100.0 + Plan.Side * 250.0; }, 60.0, 40.0), State, false));
    Out.Add(PwPhase(PwJumpClock(TEXT("clock_to_monday_1155"), 11, 55), State, false));
    Out.Add(PwPhase(PwObserve(TEXT("debashis_at_counter_before_closing"), PwDebashis,
        [](FVector& P) { FPwPlan Plan; FString Why; if (!PwMakePlan(Plan, Why)) { return false; } P = Plan.Counter; return true; }, -1, 60.0), State, false));
    Out.Add(PwPhase(PwDeparture(), State, false));
    Out.Add(PwPhase(PwJumpClock(TEXT("clock_to_monday_1230"), 12, 30), State, false));
    Out.Add(PwPhase(PwObserve(TEXT("mahua_at_counter_in_open_hours"), PwMahua,
        [](FVector& P) { return PwMahuaPoint(PWLane::MahuaAnchorCounter, P); }, -1, 60.0), State, false));
    Out.Add(PwPhase(PwJumpClock(TEXT("clock_to_monday_1315"), 13, 15), State, false));
    Out.Add(PwPhase(PwObserve(TEXT("mahua_in_stock_room_at_meal"), PwMahua,
        [](FVector& P) { return PwMahuaPoint(PWLane::MahuaAnchorStockRoom, P); }, -1, 60.0), State, false));
    Out.Add(PwPhase(PwAbsent(TEXT("debashis_off_lane_during_closure"), PwDebashis), State, false));
}
} // namespace PWAcceptance
