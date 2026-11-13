#include "PerfectWorldAcceptanceSubsystem.h"

#include "LaneJournal.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "WorldHash.h"
#include "WorldSave.h"

// journal_state (R4-04), AW-02 --restart-pair: explicit new, then continue in a fresh process.
// This is a CORE STATE scenario: authored conversation commands run in a detached copy of the live lane world,
// then AdoptSavedWorld installs the result and the production HUD saves it. It makes no walking, NPC availability,
// conversation-panel or journal-UI claim; those belong to proprietors_workday and journal_today.
// No direct edits to facts, journal or clock. Every exchange and midnight uses the normal reducer commands.
// Continue captures the original slot before entering, then compares the loaded core against that slot AND the
// independent expected two-day roster/agreements below. Reading the slot alone cannot satisfy the scenario.
//
// Integrator hooks (do not add them in this worker):
// PerfectWorldAcceptanceSubsystem.h, namespace PWAcceptance:
//     void AppendJournalStateSteps(TArray<FStepDef>& OutSteps);
// BuildScenario in PerfectWorldAcceptanceLaneScenario.cpp:
//     if (Name == TEXT("journal_state")) { AppendJournalStateSteps(OutSteps); return true; }
namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

constexpr const char* R4JDebashis = "person.debashis_pal";
constexpr const char* R4JMahua = "person.mahua_saha";
constexpr PerfectWorld::GameInstant R4JMorning = 180; // Campaign day, 09:00, absolute game minutes since 06:00.

struct R4JAcceptanceState
{
    bool bContinue = false;
    PerfectWorld::WorldSaveData Saved;
    TUniquePtr<PerfectWorld::WorldSim> Prepared;
};

FStepDef R4JAction(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepDef R4JPhase(FStepDef Def, TSharedRef<R4JAcceptanceState> State, bool bContinue)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("belongs to the other fresh-process phase"));
    };
    return Def;
}

bool R4JReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out).HashVerified;
}

bool R4JAdvance(PerfectWorld::WorldSim& Sim, PerfectWorld::GameInstant Target)
{
    for (int32 Round = 0; Round < 64; ++Round)
    {
        if (!PerfectWorld::IsWorldQuiescent(Sim.State()))
        {
            if (!Sim.Submit({PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}}).Accepted) return false;
        }
        else if (Sim.State().Clock.Now() < Target)
        {
            if (!Sim.Submit({PerfectWorld::CommandSource::System, PerfectWorld::AdvanceTime{Target - Sim.State().Clock.Now()}}).Accepted) return false;
        }
        else return Sim.State().Clock.Now() == Target;
    }
    return false;
}

bool R4JTalk(PerfectWorld::WorldSim& Sim, const char* Person, const char* Choice)
{
    if (!Sim.Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationOpen{Person, PerfectWorld::PersonAvailability::Awake}}).Accepted) return false;
    const auto Session = Sim.State().Conversations.find(Person)->second.SessionCount;
    return Sim.Submit({PerfectWorld::CommandSource::Player, PerfectWorld::ConversationChoose{Person, Choice, Session}}).Accepted &&
        Sim.Submit({PerfectWorld::CommandSource::Player, PerfectWorld::ConversationLeave{Person}}).Accepted;
}

bool R4JExpected(const PerfectWorld::WorldState& World)
{
    using namespace PerfectWorld;
    const auto Day = GameClock::DayNumber(R4JMorning);
    const std::vector<LaneJournalEncounter> Rows = {
        {Day, R4JDebashis, R4JMorning}, {Day, R4JMahua, R4JMorning},
        {Day + 1, R4JDebashis, R4JMorning + MinutesPerDay}, {Day + 1, R4JMahua, R4JMorning + MinutesPerDay}};
    const std::vector<LaneJournalAgreement> Agreements = {
        {R4JDebashis, "fact.debashis_pal.bring_promised", "journal.agreement.debashis_pal.bring_promised"},
        {R4JMahua, "fact.mahua_saha.sort_promised", "journal.agreement.mahua_saha.sort_promised"}};
    if (!IsLaneJournalValid(World) || World.Journal.Encounters != Rows || !OpenConversationPersonId(World).empty()) return false;
    for (const auto Date : {Day, Day + 1})
    {
        const auto View = LaneJournalToday(World, Date);
        if (View.PeopleMetToday.size() != 2 || View.AgreementsRemembered != Agreements ||
            View.PeopleMetToday[0].DisplayName != "Debashis Pal" || View.PeopleMetToday[1].DisplayName != "Mahua Saha") return false;
    }
    return true;
}

FStepOutcome R4JHistory(UPWAcceptanceSubsystem& Sub, const R4JAcceptanceState& State)
{
    const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    if (Life == nullptr || !R4JExpected(State.Saved.World) || !R4JExpected(Life->GetWorldState()))
        return FStepOutcome::Fail(TEXT("two dated encounters per person or the two current agreements differ from expected core truth"));
    const auto& Live = Life->GetWorldState();
    const auto Day = PerfectWorld::GameClock::DayNumber(R4JMorning);
    if (Live.Journal != State.Saved.World.Journal || Live.Conversations != State.Saved.World.Conversations ||
        PerfectWorld::LaneJournalToday(Live, Day) != PerfectWorld::LaneJournalToday(State.Saved.World, Day) ||
        PerfectWorld::LaneJournalToday(Live, Day + 1) != PerfectWorld::LaneJournalToday(State.Saved.World, Day + 1))
        return FStepOutcome::Fail(TEXT("loaded core dates, names, first-met minutes, agreements or conversation facts differ from the original slot"));
    return FStepOutcome::Pass(FString::Printf(TEXT("%d dated rows and two agreements match the authenticated slot and expected two-day core projection"),
        static_cast<int32>(Live.Journal.Encounters.size())));
}
} // namespace

namespace PWAcceptance
{
void AppendJournalStateSteps(TArray<FStepDef>& Out)
{
    const auto State = MakeShared<R4JAcceptanceState>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(R4JAction(TEXT("journal_restart_phase_and_slot"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("journal_state requires --restart-pair: explicit new, then fresh-process continue"));
        State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (State->bContinue && !R4JReadSlot(Sub, State->Saved))
            return FStepOutcome::Fail(TEXT("the original slot is absent, damaged or lacks current hash certification"));
        return FStepOutcome::Pass(State->bContinue ? TEXT("fresh-process Continue; original slot captured") : TEXT("new-life phase"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(R4JPhase(R4JAction(TEXT("journal_continued_core_history"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
        { return R4JHistory(Sub, *State); }), State, true));
    Out.Add(R4JPhase(R4JAction(TEXT("journal_two_people_core_commands"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || !Life->GetWorldState().Journal.Encounters.empty() || !Life->GetWorldState().Conversations.empty())
            return FStepOutcome::Fail(TEXT("a fresh lane world is required"));
        State->Prepared = MakeUnique<PerfectWorld::WorldSim>(Life->GetWorldState());
        auto& Sim = *State->Prepared;
        if (!R4JAdvance(Sim, R4JMorning) || !R4JTalk(Sim, R4JDebashis, "choice.debashis_pal.promise_bring") ||
            !R4JTalk(Sim, R4JMahua, "choice.mahua_saha.promise_sort") ||
            !R4JTalk(Sim, R4JDebashis, "choice.debashis_pal.not_keep_you") || Sim.State().Journal.Encounters.size() != 2)
            return FStepOutcome::Fail(TEXT("the two promises or same-day encounter deduplication failed"));
        return FStepOutcome::Pass(TEXT("two people met at 09:00; both agreements remembered; repeated greeting kept two dated rows"));
    }), State, false));
    Out.Add(R4JPhase(R4JAction(TEXT("journal_midnight_through_game_clock"), [State](UPWAcceptanceSubsystem&, FStepRuntime&)
    {
        if (!State->Prepared) return FStepOutcome::Fail(TEXT("no prepared core world"));
        auto& Sim = *State->Prepared;
        const auto Midnight = PerfectWorld::GameClock::StartOfDay(R4JMorning) + PerfectWorld::MinutesPerDay;
        if (!R4JAdvance(Sim, Midnight)) return FStepOutcome::Fail(TEXT("time.advance did not reach midnight"));
        const auto View = PerfectWorld::LaneJournalToday(Sim.State(), PerfectWorld::GameClock::DayNumber(Midnight));
        return View.PeopleMetToday.empty() && View.AgreementsRemembered.size() == 2
            ? FStepOutcome::Pass(TEXT("midnight begins an empty encounter day while both agreements remain"))
            : FStepOutcome::Fail(TEXT("midnight invented dates or lost agreements"));
    }), State, false));
    Out.Add(R4JPhase(R4JAction(TEXT("journal_next_day_core_commands"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || !State->Prepared) return FStepOutcome::Fail(TEXT("no prepared or live lane world"));
        auto& Sim = *State->Prepared;
        if (!R4JAdvance(Sim, R4JMorning + PerfectWorld::MinutesPerDay) ||
            !R4JTalk(Sim, R4JDebashis, "choice.debashis_pal.not_keep_you") ||
            !R4JTalk(Sim, R4JMahua, "choice.mahua_saha.not_keep_you") || !R4JExpected(Sim.State()))
            return FStepOutcome::Fail(TEXT("next-day core history differs from expected four dated rows and two agreements"));
        Life->AdoptSavedWorld(MoveTemp(State->Prepared));
        return FStepOutcome::Pass(TEXT("both people met again at 09:00 the next day; four dated rows adopted by the live lane"));
    }), State, false));
    Out.Add(R4JPhase(R4JAction(TEXT("journal_save_active_slot"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetHud() == nullptr) return FStepOutcome::Fail(TEXT("no HUD save service"));
        bool bSaved = false;
        const FText Why = Sub.GetHud()->SaveActiveGame(bSaved);
        if (!bSaved || !R4JReadSlot(Sub, State->Saved)) return FStepOutcome::Fail(Why.ToString());
        return R4JHistory(Sub, *State);
    }), State, false));
}
} // namespace PWAcceptance
