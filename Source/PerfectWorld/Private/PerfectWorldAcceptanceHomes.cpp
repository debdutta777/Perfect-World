#include "PerfectWorldAcceptanceHomes.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameClock.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneInteriorKit.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSubrataBindings.h"
#include "Misc/FileHelper.h"
#include "WorldSave.h"



// resident_homes (NR-03): the four residents who keep a home behind a private door of the lane (Subrata Dey, Rupa Ghosh, Nirmal Bose of
// tall-flats unit A, Sahana Mitra of unit B), proved from the visitor's side. Everything a resident is supposed to do comes from the
// authored schedules the core resolves; nothing here restates a time except the probe instants of the script.
//
//   1. registered_and_authored: each of the four doors has a resident with an authored schedule whose home is the door's location and
//      the production lane builder registered that resident with the presenter.
//   2. seven_day_coverage: for every resident, every minute of all seven days resolves (no gap, nothing squeezed), and the resident is
//      home on every day inside the door's visiting window (Nirmal and Sahana: home and Awake for all of it).
//   3. The one declared start (outside the tall flats' public front), then walked: open the front, into the passage, to unit B's door.
//   4. Unit B and unit A host truth is independent (the clock is moved by the core's own commands): Tuesday 11:00 Sahana is sewing at home,
//      Nirmal is at work; 14:30 she is out on her errand and he is still at work; 18:30 both are home. Each host answer comes from
//      UPWLaneLifeSubsystem::IsHostHome and must equal the answer derived from the schedules.
//   5. Real knocks (E on the door) at unit B: 11:00 not receiving (home, outside visiting hours), 14:30 no answer (away, the explained
//      unavailability), 18:30 admitted, 20:45 not receiving (late sewing, home but busy and outside visiting hours).
//   6. At 18:30 Talk and Leave through the conversation panel; the agreement is a core fact and survives SaveActiveGame (slot read back).
//   7. At 20:45 and 23:30 the polite decline (busy, asleep) with Leave only; the core intent keeps her work across the conversation.
//   8. Wednesday 13:44: she walks out of the passage (appears, then is gone off-lane), never two bodies for one person.
//
// Integrator hook: declare `void AppendResidentHomesSteps(TArray<FStepDef>& OutSteps);` beside AppendDoorsKnockSteps in
// PerfectWorldAcceptanceSubsystem.h and dispatch `if (Name == TEXT("resident_homes")) { AppendResidentHomesSteps(OutSteps); return true; }`
// in BuildScenario (PerfectWorldAcceptanceLaneScenario.cpp). The clock runs one game minute per real second unless the window is unfocused.

namespace
{
using namespace PWAcceptance::Homes;

/** Host truth of both flats at the current instant: from IsHostHome (the presenter's intent) and from the schedules; they must agree and match the stated expectation. */
FStepDef HmHostTruthStep(const FString& Name, const bool bExpectA, const bool bExpectB)
{
    return HmAction(Name, [bExpectA, bExpectB](UPWAcceptanceSubsystem& Sub) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        UPWNpcPresenceSubsystem* Presence = World->GetSubsystem<UPWNpcPresenceSubsystem>();
        const PerfectWorld::DoorDef* A = PerfectWorld::FindDoor(std::string(TCHAR_TO_UTF8(HmUnitA)));
        const PerfectWorld::DoorDef* B = PerfectWorld::FindDoor(std::string(TCHAR_TO_UTF8(HmUnitB)));
        if (Life == nullptr || Presence == nullptr || A == nullptr || B == nullptr) { return FStepOutcome::Fail(TEXT("no lane-life, presence or door definition")); }
        Presence->Reconcile(Life->GetNow());
        const bool bSchedA = HmAnyHomeBySchedule(World, *A, Life->GetNow());
        const bool bSchedB = HmAnyHomeBySchedule(World, *B, Life->GetNow());
        const bool bLiveA = Life->IsHostHome(*A);
        const bool bLiveB = Life->IsHostHome(*B);
        const FString Text = FString::Printf(TEXT("%s: unit A host home %d (schedule %d), unit B host home %d (schedule %d)"), *Life->GetClockText().ToString(), bLiveA, bSchedA, bLiveB, bSchedB);
        if (bSchedA != bExpectA || bSchedB != bExpectB || bLiveA != bSchedA || bLiveB != bSchedB) { return FStepOutcome::Fail(Text + TEXT("; expected A ") + FString::FromInt(bExpectA) + TEXT(", B ") + FString::FromInt(bExpectB)); }
        return FStepOutcome::Pass(Text);
    });
}

bool HmReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out).Succeeded();
}

/** Talk through the real panel: the greeting, the free agreement, Leave. */
FStepDef HmTalkAgreeStep()
{
    return HmAction(TEXT("unit_b_talk_agree_leave"), [](UPWAcceptanceSubsystem& Sub) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        UPWConversationPresenter* Talk = UPWConversationPresenter::Get(World);
        if (Life == nullptr || Talk == nullptr) { return FStepOutcome::Fail(TEXT("no lane-life subsystem or conversation presenter")); }
        // Her home is off-lane, so the production panel seam stands in for walking up to a person who is not rendered.
        if (!Talk->BeginConversationWith(HmSahana, FText::FromString(TEXT("Sahana")))) { return FStepOutcome::Fail(TEXT("the conversation with Sahana was refused at 18:30")); }
        const PerfectWorld::ConversationState* Memory = Life->FindConversation(HmSahana);
        const bool bGreeting = Memory != nullptr && Memory->NodeId == "node.sahana_mitra.greeting";
        Talk->ChooseOption(FName(TEXT("choice.sahana_mitra.agree")));
        Talk->LeaveConversation();
        Memory = Life->FindConversation(HmSahana);
        const bool bClosed = !Talk->IsOpen() && PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty();
        if (!bGreeting || Memory == nullptr || !PerfectWorld::HasConversationFact(*Memory, TCHAR_TO_UTF8(HmFact)) || !bClosed)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("greeting %d, agreement kept %d, closed %d"), bGreeting,
                Memory != nullptr && PerfectWorld::HasConversationFact(*Memory, TCHAR_TO_UTF8(HmFact)), bClosed));
        }
        return FStepOutcome::Pass(TEXT("Talk opened her greeting, the agreement was recorded, Leave closed the panel and the core session"));
    });
}

FStepDef HmSaveMemoryStep()
{
    return HmAction(TEXT("save_and_read_back_agreement"), [](UPWAcceptanceSubsystem& Sub) -> FStepOutcome
    {
        bool bSaved = false;
        const FText Why = Sub.GetHud()->SaveActiveGame(bSaved);
        PerfectWorld::WorldSaveData Data;
        if (!bSaved || !HmReadSlot(Sub, Data)) { return FStepOutcome::Fail(Why.ToString()); }
        const auto Found = Data.World.Conversations.find("person.sahana_mitra");
        if (Found == Data.World.Conversations.end() || Found->second.Open || !PerfectWorld::HasConversationFact(Found->second, TCHAR_TO_UTF8(HmFact)))
        {
            return FStepOutcome::Fail(TEXT("the saved slot does not hold Sahana's agreement in a closed session"));
        }
        return FStepOutcome::Pass(TEXT("the slot read back holds her agreement; the session is closed"));
    });
}

/** Talk at a moment she declines (busy or asleep): the decline node, Leave only, and her core intent keeps its work (or sleep) across the talk. */
FStepDef HmDeclineStep(const FString& Name, const char* NodeId, const PerfectWorld::NpcState Resumes)
{
    const std::string Expected(NodeId);
    return HmAction(Name, [Expected, Resumes](UPWAcceptanceSubsystem& Sub) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        UPWNpcPresenceSubsystem* Presence = World->GetSubsystem<UPWNpcPresenceSubsystem>();
        UPWConversationPresenter* Talk = UPWConversationPresenter::Get(World);
        const PerfectWorld::ContentDb* Db = HmDb(World);
        if (Life == nullptr || Presence == nullptr || Talk == nullptr || Db == nullptr) { return FStepOutcome::Fail(TEXT("missing subsystem")); }
        const PerfectWorld::NpcScheduleResult Schedule = PerfectWorld::LoadNpcSchedule(*Db, "npc_schedule.sahana_mitra");
        if (!Schedule.Ok()) { return FStepOutcome::Fail(TEXT("her schedule does not load")); }
        const auto During = PerfectWorld::ResolveNpcIntentAt(Schedule.Value->Person, Schedule.Value->Travel, Life->GetNow(), true, false);
        const PerfectWorld::NpcIntent* Before = Presence->FindIntent(HmSahana);
        if (Before == nullptr || Before->State != Resumes) { return FStepOutcome::Fail(TEXT("her presented intent is not the expected work or sleep")); }
        const PerfectWorld::NpcIntent Kept = *Before;
        if (!Talk->BeginConversationWith(HmSahana, FText::FromString(TEXT("Sahana")))) { return FStepOutcome::Fail(TEXT("the conversation was refused")); }
        const PerfectWorld::ConversationState* Memory = Life->FindConversation(HmSahana);
        const bool bNode = Memory != nullptr && Memory->NodeId == Expected;
        Talk->LeaveConversation();
        Presence->Reconcile(Life->GetNow());
        const PerfectWorld::NpcIntent* After = Presence->FindIntent(HmSahana);
        const bool bResumed = After != nullptr && After->State == Kept.State && After->WorkActionId == Kept.WorkActionId && After->AnchorId == Kept.AnchorId;
        const bool bChatKeeps = Resumes != PerfectWorld::NpcState::Work || (During.Ok() && During.Intent.State == PerfectWorld::NpcState::Chat &&
                                During.Intent.ResumeState == PerfectWorld::NpcState::Work && During.Intent.ResumeWorkActionId == Kept.WorkActionId);
        const bool bClosed = !Talk->IsOpen() && PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty();
        const FString Text = FString::Printf(TEXT("%s: decline node %d, work or sleep resumed %d, chat keeps work %d, closed %d"), *Life->GetClockText().ToString(), bNode, bResumed, bChatKeeps, bClosed);
        return (bNode && bResumed && bChatKeeps && bClosed) ? FStepOutcome::Pass(Text) : FStepOutcome::Fail(Text);
    });
}

/** Counts presented bodies: never more actors than registered people, and Sahana at most once. */
FString HmPresenceNote(UWorld* World, bool& bOk)
{
    UPWNpcPresenceSubsystem* Presence = World->GetSubsystem<UPWNpcPresenceSubsystem>();
    int32 Bodies = 0;
    for (TActorIterator<APWNpcCharacter> It(World); It; ++It) { ++Bodies; }
    bOk = Presence != nullptr && Bodies <= Presence->RegisteredPersonCount();
    return FString::Printf(TEXT("%d presented bodies for %d registered people"), Bodies, Presence != nullptr ? Presence->RegisteredPersonCount() : 0);
}

/** Waits until Sahana is presented (bWantPresent) or gone; passes with the body count. A second body for anyone fails. */
FStepDef HmPresenceStep(const FString& Name, const bool bWantPresent)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = 25.0;
    Def.Run = [bWantPresent](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        const UPWNpcPresenceSubsystem* Presence = World->GetSubsystem<UPWNpcPresenceSubsystem>();
        if (Presence == nullptr) { return FStepOutcome::Fail(TEXT("no presence subsystem")); }
        bool bOk = false;
        const FString Note = HmPresenceNote(World, bOk);
        if (!bOk) { return FStepOutcome::Fail(Note + TEXT(": a person is presented twice")); }
        const bool bPresent = Presence->FindPerson(HmSahana) != nullptr;
        Rt.Note = FString::Printf(TEXT("Sahana presented %d, %s"), bPresent, *Note);
        return bPresent == bWantPresent ? FStepOutcome::Pass(Rt.Note) : FStepOutcome::Wait();
    };
    return Def;
}

FStepDef HmRegisteredStep()
{
    return HmAction(TEXT("registered_and_authored"), [](UPWAcceptanceSubsystem& Sub) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        const UPWNpcPresenceSubsystem* Presence = World->GetSubsystem<UPWNpcPresenceSubsystem>();
        if (Presence == nullptr) { return FStepOutcome::Fail(TEXT("no presence subsystem")); }
        FString Who;
        for (const TCHAR* DoorId : HmHomeDoors)
        {
            const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(std::string(TCHAR_TO_UTF8(DoorId)));
            const TArray<PerfectWorld::NpcSchedule> Residents = Def != nullptr ? HmResidentsOf(World, *Def) : TArray<PerfectWorld::NpcSchedule>();
            if (Residents.IsEmpty()) { return FStepOutcome::Fail(FString::Printf(TEXT("%s has no resident with an authored schedule"), DoorId)); }
            for (const PerfectWorld::NpcSchedule& Schedule : Residents)
            {
                if (!Presence->IsRegistered(HmName(Schedule.Person.PersonId))) { return FStepOutcome::Fail(FString::Printf(TEXT("%hs is not registered with the presenter"), Schedule.Person.PersonId.c_str())); }
                Who += FString::Printf(TEXT("%hs, "), Schedule.Person.PersonId.c_str());
            }
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("registered with the presenter and authored at their own doors: %s"), *Who));
    });
}

FStepDef HmCoverageStep()
{
    return HmAction(TEXT("seven_day_coverage"), [](UPWAcceptanceSubsystem& Sub) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        int32 Minutes = 0;
        for (const TCHAR* DoorId : HmHomeDoors)
        {
            const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(std::string(TCHAR_TO_UTF8(DoorId)));
            if (Def == nullptr) { return FStepOutcome::Fail(TEXT("a home door is not in the roster")); }
            for (const PerfectWorld::NpcSchedule& Schedule : HmResidentsOf(World, *Def))
            {
                const bool bAlwaysHome = Schedule.Person.PersonId == "person.nirmal_bose" || Schedule.Person.PersonId == "person.sahana_mitra";
                for (int32 Day = 0; Day < 7; ++Day)
                {
                    int32 HomeInWindow = 0;
                    for (int32 Minute = 0; Minute < 1440; ++Minute, ++Minutes)
                    {
                        const PerfectWorld::GameInstant At = HmInstant(Day, 0, 0) + Minute;
                        const auto Resolved = PerfectWorld::ResolveSchedule(Schedule.Person, Schedule.Travel, At);
                        const auto Intent = PerfectWorld::ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, At, false, false);
                        const FString Where = FString::Printf(TEXT("%hs day %d %02d:%02d"), Schedule.Person.PersonId.c_str(), Day, Minute / 60, Minute % 60);
                        if (!Resolved.Valid || Resolved.Squeezed || Resolved.Status == PerfectWorld::ScheduleStatus::Idle || !Intent.Ok()) { return FStepOutcome::Fail(Where + TEXT(": a gap in the week")); }
                        if (PerfectWorld::DoorMinutesUntilVisiting(*Def, Minute) != 0) { continue; }
                        const bool bHome = Resolved.Status == PerfectWorld::ScheduleStatus::Stay && Resolved.Zone == Def->LocationId;
                        HomeInWindow += bHome;
                        if (bAlwaysHome && (!bHome || Intent.Intent.Availability != PerfectWorld::PersonAvailability::Awake)) { return FStepOutcome::Fail(Where + TEXT(": not home and Awake in visiting hours")); }
                    }
                    if (HomeInWindow == 0) { return FStepOutcome::Fail(FString::Printf(TEXT("%hs is never home in visiting hours on day %d"), Schedule.Person.PersonId.c_str(), Day)); }
                }
            }
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("%d resident-minutes resolved without a gap; every resident is home in visiting hours on all seven days"), Minutes));
    });
}

FStepDef HmVerdictStep()
{
    FStepDef Def;
    Def.Name = TEXT("verdict");
    Def.TimeoutSeconds = 5.0;
    Def.Run = [](UPWAcceptanceSubsystem&, FStepRuntime&) -> FStepOutcome
    {
        const FHmLedger& Ledger = HmLedger();
        if (!Ledger.Mismatches.IsEmpty()) { return FStepOutcome::Fail(FString::Printf(TEXT("%d knocks, %d mismatches: %s"), Ledger.Probes, Ledger.Mismatches.Num(), *FString::Join(Ledger.Mismatches, TEXT(" | ")))); }
        return FStepOutcome::Pass(FString::Printf(TEXT("%d knocks at unit B, every answer matched the schedules and the visiting hours"), Ledger.Probes));
    };
    return Def;
}
} // namespace

namespace PWAcceptance
{
using namespace Homes;

void AppendResidentHomesSteps(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(HmRegisteredStep());
    Out.Add(HmCoverageStep());
    Out.Add(HmStartStep());
    Out.Add(HmDoorStep(TEXT("open_tall_flats_front"), HmFront, true, TEXT("the public front door is open")));
    Out.Add(HmWalkToDoor(TEXT("walk_into_passage"), HmFront, true));
    Out.Add(HmWalkToDoor(TEXT("walk_to_unit_b"), HmUnitB, false));
    // Tuesday (day 1). 11:00: Sahana sews at home, Nirmal is at work.
    Out.Add(HmClockStep(TEXT("tuesday_1100_clock"), 1, 11, 0));
    Out.Add(HmHostTruthStep(TEXT("host_truth_1100_a_away_b_home"), false, true));
    HmAddKnock(Out, TEXT("unit_b_home_1100"));
    // 14:30: she is out on her errand (off-lane); Nirmal is still at work.
    Out.Add(HmClockStep(TEXT("tuesday_1430_clock"), 1, 14, 30));
    Out.Add(HmHostTruthStep(TEXT("host_truth_1430_both_away"), false, false));
    HmAddKnock(Out, TEXT("unit_b_away_1430"));
    // 18:30: both are home inside the shared visiting window.
    Out.Add(HmClockStep(TEXT("tuesday_1830_clock"), 1, 18, 30));
    Out.Add(HmHostTruthStep(TEXT("host_truth_1830_both_home"), true, true));
    HmAddKnock(Out, TEXT("unit_b_visiting_1830"));
    Out.Add(HmDoorStep(TEXT("unit_b_shut_again"), HmUnitB, false, TEXT("unit B's door is shut again")));
    Out.Add(HmTalkAgreeStep());
    Out.Add(HmSaveMemoryStep());
    Out.Add(PWAcceptance::MakeShotStep(TEXT("shot_unit_b_passage")));
    // 20:45: late sewing, home but busy and outside visiting hours.
    Out.Add(HmClockStep(TEXT("tuesday_2045_clock"), 1, 20, 45));
    HmAddKnock(Out, TEXT("unit_b_late_2045"));
    Out.Add(HmDoorStep(TEXT("unit_b_shut_late"), HmUnitB, false, TEXT("unit B's door is shut")));
    Out.Add(HmDeclineStep(TEXT("busy_decline_resumes_work"), "node.sahana_mitra.busy", PerfectWorld::NpcState::Work));
    Out.Add(HmClockStep(TEXT("tuesday_2330_clock"), 1, 23, 30));
    Out.Add(HmDeclineStep(TEXT("asleep_decline"), "node.sahana_mitra.asleep", PerfectWorld::NpcState::Sleep));
    // Wednesday 13:44: she walks out of the passage to the errand; one body, then gone off-lane.
    Out.Add(HmClockStep(TEXT("wednesday_1344_clock"), 2, 13, 44));
    Out.Add(HmPresenceStep(TEXT("she_appears_in_the_passage"), true));
    Out.Add(PWAcceptance::MakeShotStep(TEXT("shot_she_walks_out")));
    Out.Add(HmPresenceStep(TEXT("she_leaves_the_lane_once"), false));
    Out.Add(HmVerdictStep());
}
} // namespace PWAcceptance
