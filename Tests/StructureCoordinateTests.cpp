// ROADMAP M2-09: the "coordinate a group" runner (assignment, conversation, settlement,
// hand-over, costs, failure kinds, refusals). The catalogue is the real Data/Content. The
// properties shared by every structure (completion, selection, mastery, routine, save
// text, random play) run through StructureOthersHarness.h.
#include "StructureOthersHarness.h"

using namespace PerfectWorld;

namespace
{
struct CoordinateT
{
    using Catalogue = CoordinateCatalogue;
    using State = CoordinateState;
    using Action = CoordinateAction;
    using Outcome = CoordinateOutcome;
    using CaseDef = CoordinateCaseDef;
    static constexpr int Goals = 4;
    static constexpr std::size_t UnitsField = 4;
    static constexpr const char* Header = "pw.structure_coordinate/1";
    static constexpr const char* IdleTail = "assign=-\nheard=-\nresolution=none\n";

    static const Catalogue& Cat() { return TestCatalogues->Coordinate; }
    static StructureBeginResult Begin(State& s, const Catalogue& c, std::string_view id, int goal, std::int32_t skill)
    {
        return BeginCoordinateExposure(s, c, id, static_cast<CoordinateGoal>(goal), skill);
    }
    static StructureError Cancel(State& s) { return CancelCoordinateExposure(s); }
    static StructureQuote Quote(const State& s, const Catalogue& c, const Action& a) { return QuoteCoordinateAction(s, c, a); }
    static CoordinateStepResult Apply(State& s, const Catalogue& c, const Action& a) { return ApplyCoordinateAction(s, c, a); }
    static Action Standard(const CaseDef& d, const CoordinateExposure& e) { return CoordinateStandardAction(d, e); }
    static StructureRoutineResult Routine(State& s, const Catalogue& c, std::string_view f) { return ResolveCoordinateRoutine(s, c, f); }
    static StructureSelection Select(const Catalogue& c, const State& s, std::string_view f, std::uint64_t seed, GameInstant at)
    {
        return SelectCoordinateCase(c, s, f, seed, at);
    }
    static std::string Serialise(const State& s) { return SerialiseStructureCoordinate(s); }
    static CoordinateParse Parse(std::string_view t) { return DeserialiseStructureCoordinate(t); }
    static bool Valid(const State& s) { return IsCoordinateStateValid(s); }
    static std::int32_t Budget(const CaseDef& d) { return CoordinateBudgetUnits(d.Scarce); }
    /** An action that quotes its price with nothing chosen but is refused when applied. */
    static bool PriceOnly(const Action& a) { return a.Kind == CoordinateActionKind::AssignWork && a.Assignment.empty(); }

    static Action Random(OthersRng& rng, const CaseDef& def, const State& state)
    {
        Action action;
        const CoordinateBeat beat = state.Exposure.Beat;
        action.Kind = rng.Below(100) < 75 ? (beat == CoordinateBeat::Assign ? CoordinateActionKind::AssignWork
                                             : beat == CoordinateBeat::Exchange ? static_cast<CoordinateActionKind>(1 + rng.Below(2))
                                             : beat == CoordinateBeat::Settle ? CoordinateActionKind::Settle
                                                                              : CoordinateActionKind::Submit)
                                          : static_cast<CoordinateActionKind>(rng.Below(5));
        if (rng.Below(100) < 45)
            action.Assignment = CoordinateSoundAssignment(def);
        else
        {
            for (const CoordinateTask& task : def.Tasks)
                action.Assignment.push_back({task.Id, def.Members[rng.Below(def.Members.size())].Id});
            if (rng.Below(10) == 0 && !action.Assignment.empty())
                action.Assignment.pop_back();
            if (rng.Below(10) == 0 && action.Assignment.size() > 1)
                action.Assignment.back().Task = action.Assignment.front().Task;
            if (rng.Below(20) == 0 && !action.Assignment.empty())
                action.Assignment.front().Member = "m_nope";
        }
        const std::size_t pick = rng.Below(20);
        action.Member = pick == 0 ? "m_nope" : pick == 1 ? std::string() : def.Members[rng.Below(def.Members.size())].Id;
        action.Resolution = rng.Below(100) < 45 ? CoordinateMatchingResolution(def.Constraint) : static_cast<CoordinateResolution>(rng.Below(7));
        action.Check = rng.Below(2) == 0;
        return action;
    }

    /** A foreground exposure that ends Unsound: a sound assignment, nobody asked, the constraint left. */
    static Outcome FailOnce(State& state, const CaseDef& def)
    {
        Check(Begin(state, Cat(), def.Id, 0, FullSkill).Ok(), "begins");
        Harness::Do<CoordinateT>(state, AssignAction(CoordinateSoundAssignment(def)));
        Harness::Do<CoordinateT>(state, EndAction());
        Harness::Do<CoordinateT>(state, SettleAction(CoordinateResolution::Leave));
        return Harness::Do<CoordinateT>(state, SubmitAction(false)).Outcome;
    }

    static CoordinateAction AssignAction(std::vector<CoordinateAssignment> assignment)
    {
        CoordinateAction action;
        action.Kind = CoordinateActionKind::AssignWork;
        action.Assignment = std::move(assignment);
        return action;
    }
    static CoordinateAction ExchangeAction(std::string member)
    {
        CoordinateAction action;
        action.Kind = CoordinateActionKind::Exchange;
        action.Member = std::move(member);
        return action;
    }
    static CoordinateAction EndAction()
    {
        CoordinateAction action;
        action.Kind = CoordinateActionKind::EndExchange;
        return action;
    }
    static CoordinateAction SettleAction(CoordinateResolution resolution)
    {
        CoordinateAction action;
        action.Kind = CoordinateActionKind::Settle;
        action.Resolution = resolution;
        return action;
    }
    static CoordinateAction SubmitAction(bool checked)
    {
        CoordinateAction action;
        action.Kind = CoordinateActionKind::Submit;
        action.Check = checked;
        return action;
    }
};

using CT = CoordinateT;

const CoordinateCaseDef& CaseOf(const std::string& shortName)
{
    const CoordinateCaseDef* def = CT::Cat().FindCase("coordinate_case.xi_" + shortName);
    Check(def != nullptr, "the named case exists");
    return *def;
}

CoordinateOutcome PlayAll(const CoordinateCaseDef& def, CoordinateGoal goal, const std::vector<CoordinateAssignment>& assignment,
                          const std::vector<std::string>& talk, CoordinateResolution resolution, bool checked)
{
    CoordinateState state = Harness::Started<CT>(def, static_cast<int>(goal), FullSkill);
    Harness::Do<CT>(state, CT::AssignAction(assignment));
    for (const std::string& member : talk)
        Harness::Do<CT>(state, CT::ExchangeAction(member));
    Harness::Do<CT>(state, CT::EndAction());
    Harness::Do<CT>(state, CT::SettleAction(resolution));
    return Harness::Do<CT>(state, CT::SubmitAction(checked)).Outcome;
}

/** How many of the member^task assignments are Sound: every task with a member who suits it, nobody over capacity. */
int CountSoundAssignments(const CoordinateCaseDef& def)
{
    int count = 0;
    std::vector<std::size_t> pick(def.Tasks.size(), 0);
    while (true)
    {
        std::vector<std::int32_t> load(def.Members.size(), 0);
        bool fits = true;
        for (std::size_t t = 0; t < def.Tasks.size(); ++t)
        {
            load[pick[t]] += def.Tasks[t].Load;
            const std::vector<std::string>& suits = def.Tasks[t].Fits;
            fits = fits && std::find(suits.begin(), suits.end(), def.Members[pick[t]].Id) != suits.end() && load[pick[t]] <= def.Members[pick[t]].Capacity;
        }
        count += fits ? 1 : 0;
        std::size_t at = pick.size();
        while (at > 0 && ++pick[at - 1] == def.Members.size())
            pick[--at] = 0;
        if (at == 0)
            return count;
    }
}

void TestCatalogueShape()
{
    std::set<std::string> triples, constraints, roles, scarces, subjects;
    int multiple = 0;
    for (const CoordinateCaseDef& def : Harness::Cases<CT>())
    {
        const std::string constraint(ModelNameOf(CoordinateConstraintNames, def.Constraint));
        const std::string role(ModelNameOf(CoordinateRoleNames, def.Role));
        const std::string scarce(ModelNameOf(CoordinateScarceNames, def.Scarce));
        triples.insert(constraint + "/" + role + "/" + scarce);
        constraints.insert(constraint);
        roles.insert(role);
        scarces.insert(scarce);
        subjects.insert(def.Subject);
        Check(def.Members.size() >= 3 && def.Tasks.size() >= 3, "three or more members and tasks");
        Check(std::count_if(def.Members.begin(), def.Members.end(), [](const CoordinateMember& m) { return m.Self; }) == 1, "exactly one member is the player");
        const int sound = CountSoundAssignments(def);
        Check(sound >= 1 && !CoordinateSoundAssignment(def).empty(), "some assignment is Sound");
        multiple += sound >= 2 ? 1 : 0;
        const std::vector<std::string> holders = CoordinateCriticalHolders(def);
        Check(!holders.empty() && holders.size() <= 2, "one or two classmates hold the critical facts");
        Check(CoordinateKnownFacts(def, {}).empty(), "the player begins knowing no critical fact: only their own facts are known");
        Check(std::any_of(def.Members.begin(), def.Members.end(), [&](const CoordinateMember& m) {
                  return !m.Self && std::find(holders.begin(), holders.end(), m.Id) == holders.end();
              }),
              "some classmate holds nothing critical, so a conversation can be idle");
        Check(CoordinateMatchingResolution(def.Constraint) != CoordinateResolution::Leave && CoordinateMatchingResolution(def.Constraint) != CoordinateResolution::None,
              "the constraint has a resolution that fits");
    }
    Check(multiple >= 1, "some case has more than one Sound assignment: there is no single hidden right answer");
    Check(triples.size() == Harness::Cases<CT>().size(), "no two cases share their constraint, role and scarce resource");
    Check(constraints.size() == 4 && roles.size() == 4 && scarces.size() == 4 && subjects.size() == 4, "the cases cover every constraint, role, scarce resource and subject");
    Check(CoordinateMatchingResolution(CoordinateConstraint::Capacity) == CoordinateResolution::Rebalance &&
              CoordinateMatchingResolution(CoordinateConstraint::Clash) == CoordinateResolution::Reschedule &&
              CoordinateMatchingResolution(CoordinateConstraint::Budget) == CoordinateResolution::TrimScope &&
              CoordinateMatchingResolution(CoordinateConstraint::Deadline) == CoordinateResolution::AskExtension,
          "each constraint has its own resolution");
}

void TestCostsShowBeforeCommit()
{
    // The stall: chair (assigning is a unit cheaper), time scarce (five units), no seam.
    const CoordinateCaseDef& stall = CaseOf("science_stall");
    Check(CoordinateBudgetUnits(stall.Scarce) == StructureScarceTimeUnits && CoordinateBudgetUnits(stall.Scarce) == 5, "scarce time leaves five units");
    Check(CoordinateAssignCost(stall, CoordinateGoal::HearEveryone, true) == 1 && CoordinateAssignCost(stall, CoordinateGoal::HearEveryone, false) == 2,
          "the chair assigns for 1 practised, 3 less 1 unfamiliar");
    Check(CoordinateExchangeCost(stall, CoordinateGoal::SettleConstraint) == 1 && CoordinateExchangeCost(stall, CoordinateGoal::HearEveryone) == 0,
          "a conversation costs a unit, free for the goal hear_everyone");
    Check(CoordinateSettleCost(stall, CoordinateGoal::SettleConstraint, CoordinateResolution::Rebalance) == 1 &&
              CoordinateSettleCost(stall, CoordinateGoal::OrganiseWork, CoordinateResolution::Leave) == 0,
          "a resolution costs a unit and never less, leaving it is free");
    Check(CoordinateSubmitCost(stall, CoordinateGoal::OrganiseWork, true) == 1 && CoordinateSubmitCost(stall, CoordinateGoal::SubmitCleanly, true) == 0 &&
              CoordinateSubmitCost(stall, CoordinateGoal::OrganiseWork, false) == 0,
          "a check costs a unit, free for the goal submit_cleanly; submitting as it stands is free");

    // The debate: mediator (listening is free), space scarce (settling a unit dearer).
    const CoordinateCaseDef& debate = CaseOf("debate_hall");
    Check(CoordinateExchangeCost(debate, CoordinateGoal::OrganiseWork) == 0 && CoordinateAssignCost(debate, CoordinateGoal::OrganiseWork, false) == 2 &&
              CoordinateAssignCost(debate, CoordinateGoal::HearEveryone, false) == 3,
          "the mediator's conversation costs 1 less 1; assigning is 3 unless the goal organise_work");
    Check(CoordinateSettleCost(debate, CoordinateGoal::HearEveryone, CoordinateResolution::Reschedule) == 2 &&
              CoordinateSettleCost(debate, CoordinateGoal::SettleConstraint, CoordinateResolution::Reschedule) == 1,
          "space scarce: settling costs 1+1, a unit less for the goal settle_constraint");

    // The sports stall: treasurer, materials scarce (the check a unit dearer). The titration: scribe, attention scarce.
    const CoordinateCaseDef& sports = CaseOf("sports_day_stall");
    Check(CoordinateSettleCost(sports, CoordinateGoal::HearEveryone, CoordinateResolution::TrimScope) == 1 &&
              CoordinateSubmitCost(sports, CoordinateGoal::OrganiseWork, true) == 2 && CoordinateSubmitCost(sports, CoordinateGoal::SubmitCleanly, true) == 1,
          "materials scarce: the check costs 1+1, a unit less for the goal submit_cleanly");
    const CoordinateCaseDef& titration = CaseOf("titration_report");
    Check(CoordinateExchangeCost(titration, CoordinateGoal::OrganiseWork) == 2 && CoordinateExchangeCost(titration, CoordinateGoal::HearEveryone) == 1 &&
              CoordinateSubmitCost(titration, CoordinateGoal::OrganiseWork, true) == 0,
          "attention scarce: a conversation costs 1+1; the scribe's check costs 1 less 1");

    // Every price of the current beat can be read at the start, and quoting changes nothing.
    CoordinateState state = Harness::Started<CT>(debate, 1, NoSkill);
    const CoordinateState before = state;
    const StructureQuote quote = QuoteCoordinateAction(state, CT::Cat(), CT::AssignAction({}));
    Check(quote.Ok() && quote.Units == 3 && quote.UnitsAfter == 4, "assigning is quoted with nothing assigned: 3 of 7");
    Check(QuoteCoordinateAction(state, CT::Cat(), CT::SettleAction(CoordinateResolution::Leave)).Error == StructureError::WrongBeat, "a later beat is not yet legal");
    Check(state == before, "reading prices changes nothing");
    const CoordinateState known = Harness::Started<CT>(debate, 1, FullSkill);
    Check(QuoteCoordinateAction(known, CT::Cat(), CT::AssignAction({})).Units == 1, "a practised character assigns for one unit");
}

void TestUnitsRunOut()
{
    // The titration, unfamiliar, goal settle_constraint: assigning costs 3, and two conversations at 2 spend the rest.
    const CoordinateCaseDef& titration = CaseOf("titration_report");
    CoordinateState state = Harness::Started<CT>(titration, 2, NoSkill);
    Harness::Do<CT>(state, CT::AssignAction(CoordinateSoundAssignment(titration)));
    Check(state.Exposure.UnitsLeft == 4, "assigning leaves four units");
    Harness::Do<CT>(state, CT::ExchangeAction("m_ishan"));
    Harness::Do<CT>(state, CT::ExchangeAction("m_mira"));
    Check(state.Exposure.UnitsLeft == 0, "two conversations spend the rest");
    Harness::Do<CT>(state, CT::EndAction());
    const CoordinateState spent = state;
    const StructureQuote dear = QuoteCoordinateAction(state, CT::Cat(), CT::SettleAction(CoordinateResolution::AskExtension));
    Check(dear.Error == StructureError::UnitsShort && dear.Units == 1 && dear.UnitsAfter == -1, "the quote shows 1 against 0 left");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::SettleAction(CoordinateResolution::AskExtension)).Error == StructureError::UnitsShort && state == spent,
          "a refusal for spent time changes nothing");
    // Every beat has a free way forward: leave the constraint, then submit as it stands.
    Harness::Do<CT>(state, CT::SettleAction(CoordinateResolution::Leave));
    const CoordinateOutcome outcome = Harness::Do<CT>(state, CT::SubmitAction(false)).Outcome;
    Check((outcome.Failures & CoordinateFailUnsettled) != 0 && (outcome.Failures & CoordinateFailSeam) != 0 && (outcome.Failures & CoordinateFailWasted) != 0 &&
              outcome.Grade == StructureGrade::Unsound && outcome.IdleConversations == 1,
          "left unsettled, unchecked and with an idle conversation");

    // Assigning is always affordable at the start, and the first beat is the only one that cannot be skipped.
    for (const CoordinateCaseDef& def : Harness::Cases<CT>())
    {
        for (int goal = 0; goal < CT::Goals; ++goal)
            Check(QuoteCoordinateAction(Harness::Started<CT>(def, goal, NoSkill), CT::Cat(), CT::AssignAction({})).Ok(), "assigning is always affordable");
    }
}

void TestFailureKinds()
{
    const CoordinateCaseDef& stall = CaseOf("science_stall");
    const CoordinateGoal organise = CoordinateGoal::OrganiseWork;
    const std::vector<CoordinateAssignment> sound{{"t_model", "m_ishan"}, {"t_poster", "m_you"}, {"t_script", "m_tara"}};
    const std::vector<CoordinateAssignment> other{{"t_model", "m_you"}, {"t_poster", "m_tara"}, {"t_script", "m_ishan"}};
    const CoordinateResolution rebalance = CoordinateResolution::Rebalance;

    CoordinateOutcome o = PlayAll(stall, organise, sound, {"m_tara"}, rebalance, false);
    Check(o.Failures == 0 && o.Grade == StructureGrade::Sound && o.MissedFacts == 0 && o.IdleConversations == 0, "an assignment, the right conversation, the fitting resolution: Sound");
    o = PlayAll(stall, organise, other, {"m_tara"}, rebalance, false);
    Check(o.Failures == 0 && o.Grade == StructureGrade::Sound, "a different Sound assignment is as sound as the first");

    o = PlayAll(stall, organise, {{"t_model", "m_you"}, {"t_poster", "m_you"}, {"t_script", "m_tara"}}, {"m_tara"}, rebalance, false);
    Check(o.Failures == CoordinateFailOverloaded && o.OverloadedMembers == 1 && o.Grade == StructureGrade::Unsound, "three units of work on a member of capacity two is overload");
    o = PlayAll(stall, organise, {{"t_model", "m_you"}, {"t_poster", "m_ishan"}, {"t_script", "m_tara"}}, {"m_tara"}, rebalance, false);
    Check(o.Failures == CoordinateFailMismatched && o.MismatchedTasks == 1 && o.Grade == StructureGrade::Partial, "a task given to a member it does not suit is weaker work, not a failure to deliver");
    o = PlayAll(stall, organise, sound, {}, rebalance, false);
    Check(o.Failures == CoordinateFailUninformed && o.MissedFacts == 1 && o.Grade == StructureGrade::Unsound,
          "the right resolution chosen without hearing the critical fact is uninformed, not lucky");
    o = PlayAll(stall, organise, sound, {"m_ishan", "m_tara"}, rebalance, false);
    Check(o.Failures == CoordinateFailWasted && o.IdleConversations == 1 && o.Grade == StructureGrade::Partial, "a conversation with a member who held nothing critical is wasted effort");
    o = PlayAll(stall, organise, sound, {"m_tara"}, CoordinateResolution::Reschedule, false);
    Check(o.Failures == CoordinateFailUnsettled && o.Grade == StructureGrade::Unsound, "a resolution that does not fit leaves the constraint unsettled");
    o = PlayAll(stall, organise, sound, {"m_tara"}, CoordinateResolution::Leave, false);
    Check(o.Failures == CoordinateFailUnsettled, "so does leaving it");
    o = PlayAll(stall, organise, sound, {"m_tara"}, rebalance, true);
    Check(o.Failures == CoordinateFailWasted && o.Grade == StructureGrade::Partial, "checking a hand-over with no seam wastes the effort");
    o = PlayAll(stall, organise, {{"t_model", "m_you"}, {"t_poster", "m_you"}, {"t_script", "m_tara"}}, {"m_ishan"}, CoordinateResolution::Leave, false);
    Check(o.Failures == (CoordinateFailOverloaded | CoordinateFailUninformed | CoordinateFailUnsettled | CoordinateFailWasted), "failure kinds combine and stay separate");

    // A seam is only found by checking; a Sound case with a seam is checked.
    const CoordinateCaseDef& debate = CaseOf("debate_hall");
    const std::vector<CoordinateAssignment> debateSound{{"t_opening", "m_you"}, {"t_rebuttal", "m_arup"}, {"t_timekeeping", "m_nasrin"}};
    o = PlayAll(debate, organise, debateSound, {"m_nasrin"}, CoordinateResolution::Reschedule, false);
    Check(o.Failures == CoordinateFailSeam && o.Grade == StructureGrade::Unsound, "an unchecked hand-over with a seam fails to join");
    o = PlayAll(debate, organise, debateSound, {"m_nasrin"}, CoordinateResolution::Reschedule, true);
    Check(o.Failures == 0 && o.Grade == StructureGrade::Sound, "the check finds it");

    // The role and the goal do not change what is Sound, only the price.
    for (int goal = 0; goal < CT::Goals; ++goal)
        Check(PlayAll(debate, static_cast<CoordinateGoal>(goal), debateSound, {"m_nasrin"}, CoordinateResolution::Reschedule, true).Failures == 0, "the goal never changes what is Sound");
}

void TestInformation()
{
    const CoordinateCaseDef& sports = CaseOf("sports_day_stall");
    Check(CoordinateCriticalHolders(sports) == std::vector<std::string>({"m_mira"}), "Mira holds the critical fact");
    Check(CoordinateKnownFacts(sports, {}).empty() && CoordinateKnownFacts(sports, {"m_mira"}) == std::vector<std::string>({"f_float"}) &&
              CoordinateKnownFacts(sports, {"m_tara", "m_mira"}).size() == 2,
          "a conversation teaches the facts its member holds");
    // The player's own facts are known from the start.
    CoordinateCaseDef own = sports;
    own.Facts.push_back({"f_mine", "m_you", "something the player knows", false});
    Check(CoordinateKnownFacts(own, {}) == std::vector<std::string>({"f_mine"}), "the player's own facts are known without a conversation");
}

void TestCorrectAnswerIsNeverRefusedForAStat()
{
    for (const CoordinateCaseDef& def : Harness::Cases<CT>())
    {
        CoordinateOutcome results[2];
        std::int32_t spent[2];
        for (int who = 0; who < 2; ++who)
        {
            CoordinateState state = Harness::Started<CT>(def, 1, who == 0 ? NoSkill : FullSkill);
            Check(state.Exposure.MethodKnown == (who == 1), "the least able character lacks the method, the master has it");
            Harness::Do<CT>(state, CT::AssignAction(CoordinateSoundAssignment(def)));
            Check(state.Exposure.UnitsLeft >= 0 && state.Exposure.Beat == CoordinateBeat::Exchange, "the sound assignment is accepted whatever the skill");
            for (const std::string& holder : CoordinateCriticalHolders(def))
                Harness::Do<CT>(state, CT::ExchangeAction(holder));
            Harness::Do<CT>(state, CT::EndAction());
            Harness::Do<CT>(state, CT::SettleAction(CoordinateMatchingResolution(def.Constraint)));
            const CoordinateStepResult last = Harness::Do<CT>(state, CT::SubmitAction(def.Seam));
            results[who] = last.Outcome;
            spent[who] = last.Outcome.UnitsSpent;
        }
        Check(results[0].Grade == StructureGrade::Sound && results[1].Grade == StructureGrade::Sound && results[0].Failures == results[1].Failures &&
                  results[0].LearningPermille == results[1].LearningPermille,
              "skill changes the price, never the result");
        Check(spent[0] > spent[1], "the unfamiliar method costs more to assign");
    }
}

void TestRefusals()
{
    const CoordinateCaseDef& stall = CaseOf("science_stall");
    const std::vector<CoordinateAssignment> sound = CoordinateSoundAssignment(stall);
    CoordinateState state;
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::AssignAction(sound)).Error == StructureError::NotActive, "no exposure, no action");
    Check(QuoteCoordinateAction(state, CT::Cat(), CT::EndAction()).Error == StructureError::NotActive, "no exposure, no quote");
    Check(CancelCoordinateExposure(state) == StructureError::NotActive, "nothing to cancel");
    Check(BeginCoordinateExposure(state, CT::Cat(), "coordinate_case.xi_nope", CoordinateGoal::OrganiseWork, 0).Error == StructureError::UnknownCase, "unknown case");
    Check(BeginCoordinateExposure(state, CT::Cat(), stall.Id, CoordinateGoal::OrganiseWork, -1).Error == StructureError::InvalidSkill, "negative skill");
    Check(BeginCoordinateExposure(state, CT::Cat(), stall.Id, static_cast<CoordinateGoal>(9), 0).Error == StructureError::InvalidState, "goal out of range");
    Check(state == CoordinateState{}, "refused begins change nothing");

    state = Harness::Started<CT>(stall, 0, FullSkill);
    const CoordinateState begun = state;
    Check(BeginCoordinateExposure(state, CT::Cat(), stall.Id, CoordinateGoal::OrganiseWork, 0).Error == StructureError::Busy && state == begun, "one exposure at a time");
    for (const CoordinateAction& wrong : {CT::ExchangeAction("m_tara"), CT::EndAction(), CT::SettleAction(CoordinateResolution::Leave), CT::SubmitAction(false)})
        Check(ApplyCoordinateAction(state, CT::Cat(), wrong).Error == StructureError::WrongBeat && state == begun, "an action of another beat is refused");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::AssignAction({})).Error == StructureError::BadChoice && state == begun, "an empty assignment is refused");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::AssignAction({sound[0], sound[1]})).Error == StructureError::BadChoice && state == begun, "an assignment that leaves a task out is refused");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::AssignAction({sound[0], sound[1], sound[1]})).Error == StructureError::BadChoice && state == begun, "a task assigned twice is refused");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::AssignAction({sound[0], sound[1], {"t_nope", "m_you"}})).Error == StructureError::UnknownItem && state == begun, "an unknown task is refused");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::AssignAction({sound[0], sound[1], {"t_script", "m_nope"}})).Error == StructureError::UnknownItem && state == begun, "an unknown member is refused");
    Harness::Do<CT>(state, CT::AssignAction({sound[2], sound[0], sound[1]}));
    Check(state.Exposure.Assignment.front().Task == "t_model" && state.Exposure.Assignment.back().Task == "t_script", "the assignment is kept sorted by task");
    const CoordinateState assigned = state;
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::AssignAction(sound)).Error == StructureError::WrongBeat && state == assigned, "the work is assigned once");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::ExchangeAction("")).Error == StructureError::BadChoice && state == assigned, "no member named is refused");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::ExchangeAction("m_you")).Error == StructureError::BadChoice && state == assigned, "talking to yourself is refused");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::ExchangeAction("m_nope")).Error == StructureError::UnknownItem && state == assigned, "an unknown member is refused");
    Harness::Do<CT>(state, CT::ExchangeAction("m_tara"));
    const CoordinateState heard = state;
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::ExchangeAction("m_tara")).Error == StructureError::AlreadyDone && state == heard, "a member is spoken to once");
    Check(state.Exposure.Heard == std::vector<std::string>({"m_tara"}), "the members spoken to are kept");
    Harness::Do<CT>(state, CT::EndAction());
    const CoordinateState ended = state;
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::ExchangeAction("m_ishan")).Error == StructureError::WrongBeat && state == ended, "no conversation after the exchange has ended");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::SettleAction(CoordinateResolution::None)).Error == StructureError::BadChoice && state == ended, "no resolution is not a choice");
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::SettleAction(static_cast<CoordinateResolution>(9))).Error == StructureError::BadChoice && state == ended, "an unknown resolution is refused");
    Harness::Do<CT>(state, CT::SettleAction(CoordinateResolution::Rebalance));
    const CoordinateState settled = state;
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::SettleAction(CoordinateResolution::Leave)).Error == StructureError::WrongBeat && state == settled, "the constraint is settled once");

    // Submitting ends the exposure once; a second finds nothing to end.
    Check(Harness::Do<CT>(state, CT::SubmitAction(false)).Finished, "submitting finishes the exposure");
    const CoordinateState finished = state;
    Check(ApplyCoordinateAction(state, CT::Cat(), CT::SubmitAction(false)).Error == StructureError::NotActive && state == finished, "an outcome is applied exactly once");
    Check(state.FindFamily(stall.FamilyId)->Exposures == 1, "one exposure was recorded");

    // Cancel: no outcome, no mastery change, but the case stays seen.
    state = Harness::Started<CT>(stall, 0, FullSkill);
    Check(CancelCoordinateExposure(state) == StructureError::None && !state.Exposure.Active(), "an exposure can be abandoned");
    const StructureFamilyRecord* record = state.FindFamily(stall.FamilyId);
    Check(record != nullptr && record->Exposures == 0 && record->SoundExposures == 0 && record->Seen.size() == 1, "abandoning records no outcome but the case was seen");
}
} // namespace

void TestCoordinate()
{
    Harness::AllGeneric<CT>();
    TestCatalogueShape();
    TestCostsShowBeforeCommit();
    TestUnitsRunOut();
    TestFailureKinds();
    TestInformation();
    TestCorrectAnswerIsNeverRefusedForAStat();
    TestRefusals();
}
