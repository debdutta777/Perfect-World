#include "DayLoopTestSupport.h"

using namespace PerfectWorld;

namespace
{
/** The command is refused for exactly this reason and the state is exactly as it was. */
void ExpectRefusal(const DayLoopState& state, const DayLoopCatalogues& cats, const DayCommand& command, DayLoopError error,
                   const char* message)
{
    DayLoopState copy = state;
    const std::string textBefore = SerialiseDayLoop(state);
    const DayLoopResult result = ApplyDayCommand(copy, cats, command);
    if (result.Error != error)
        std::cerr << "got " << DayLoopErrorCode(result.Error) << " " << result.Detail << " for: " << message << '\n';
    Check(!result.Ok() && result.Error == error, message);
    Check(result.Events.empty() && !result.CheckpointDue, "a refusal reports no events and no checkpoint");
    Check(copy == state && SerialiseDayLoop(copy) == textBefore && HashDayLoop(copy) == HashDayLoop(state),
          "a refusal changes nothing");
    Check(!DayLoopErrorCode(result.Error).empty(), "every refusal has a stable code");
}

DayLoopState Planned(GameInstant start = 0)
{
    DayLoopState state = NewDayLoopState(start);
    MustApply(state, PlanCmd({}), "plan");
    return state;
}

void TestBeforeThePlan()
{
    const DayLoopState fresh = MorningState();
    const DayLoopCatalogues& cats = DayLoopCats();
    ExpectRefusal(fresh, cats, CommitCmd("activity.rest", "home"), DayLoopError::PlanFirst, "committing needs the morning plan");
    ExpectRefusal(fresh, cats, SimpleCmd(DayCommandKind::Start), DayLoopError::PlanFirst, "starting needs the morning plan");
    ExpectRefusal(fresh, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::PlanFirst, "time does not pass before the plan");
    ExpectRefusal(fresh, cats, SimpleCmd(DayCommandKind::ArriveSchool), DayLoopError::PlanFirst, "arriving needs the plan");
    ExpectRefusal(fresh, cats, SimpleCmd(DayCommandKind::Wake, 60), DayLoopError::NotAsleep, "a waking player cannot wake");
    ExpectRefusal(fresh, cats, SimpleCmd(DayCommandKind::Complete), DayLoopError::NotRunning, "nothing runs to complete");
    ExpectRefusal(fresh, cats, SimpleCmd(DayCommandKind::Cancel), DayLoopError::NotCommitted, "nothing to cancel");

    ExpectRefusal(fresh, cats, PlanCmd({{360, "activity.nonesuch"}}), DayLoopError::UnknownActivity, "a plan names real activities");
    ExpectRefusal(fresh, cats, PlanCmd({{360, "activity.study_at_home"}, {400, "activity.rest"}}), DayLoopError::InvalidCommand,
                  "plan items may not overlap");
    ExpectRefusal(fresh, cats, PlanCmd({{400, "activity.rest"}, {360, "activity.rest"}}), DayLoopError::InvalidCommand,
                  "plan items are in order");
    ExpectRefusal(fresh, cats, PlanCmd({{1440, "activity.rest"}}), DayLoopError::InvalidCommand, "a plan item starts inside the day");
    ExpectRefusal(fresh, cats, PlanCmd({{-1, "activity.rest"}}), DayLoopError::InvalidCommand, "a plan item cannot start before midnight");
    std::vector<DayPlanItem> many;
    for (int i = 0; i < 25; ++i)
        many.push_back({i * 50, "activity.rest"});
    ExpectRefusal(fresh, cats, PlanCmd(many), DayLoopError::InvalidCommand, "a plan has at most 24 items");

    DayLoopCatalogues none;
    ExpectRefusal(fresh, none, PlanCmd({}), DayLoopError::MissingCatalogue, "planning needs the activity catalogue");
    const DayLoopState planned = Planned();
    ExpectRefusal(planned, none, CommitCmd("activity.rest", "home"), DayLoopError::MissingCatalogue, "committing needs the catalogue");
}

void TestCommitments()
{
    const DayLoopState planned = Planned();
    const DayLoopCatalogues& cats = DayLoopCats();
    ExpectRefusal(planned, cats, CommitCmd("activity.nonesuch", "home"), DayLoopError::UnknownActivity, "an unknown activity");
    ExpectRefusal(planned, cats, CommitCmd("activity.study_at_home", "classroom"), DayLoopError::WrongPlace,
                  "studying at home needs a home");
    ExpectRefusal(planned, cats, CommitCmd("activity.rest", "Not A Token"), DayLoopError::InvalidCommand, "a place kind is a token");
    ExpectRefusal(planned, cats, CommitCmd("activity.rest", ""), DayLoopError::InvalidCommand, "a place kind is not empty");
    ExpectRefusal(planned, cats, CommitCmd("activity.rest", "home", {Candidate("Bad Id", 0, 0, 30, true, 0)}),
                  DayLoopError::InvalidCommand, "an alternative must be well formed");
    ExpectRefusal(planned, cats, CommitCmd("activity.rest", "home", {Candidate("alt.x", 0, 0, 0, true, 0)}),
                  DayLoopError::InvalidCommand, "an alternative needs a duration");
    std::vector<ReceiptCandidate> crowd;
    for (int i = 0; i <= static_cast<int>(DayLoopMaxAlternatives); ++i)
        crowd.push_back(Candidate("alt.n" + std::to_string(i), 0, 0, 30, true, 0));
    ExpectRefusal(planned, cats, CommitCmd("activity.rest", "home", crowd), DayLoopError::InvalidCommand, "too many alternatives");

    DayLoopState committed = planned;
    MustApply(committed, CommitCmd("activity.study_at_home", "home"), "commit");
    ExpectRefusal(committed, cats, CommitCmd("activity.rest", "home"), DayLoopError::AlreadyCommitted, "one commitment at a time");
    ExpectRefusal(committed, cats, SimpleCmd(DayCommandKind::GoToSleep), DayLoopError::Busy, "sleep after the commitment is settled");
    ExpectRefusal(committed, cats, SimpleCmd(DayCommandKind::Complete), DayLoopError::NotRunning, "a commitment that has not started");
    ExpectRefusal(committed, cats, SimpleCmd(DayCommandKind::Cancel, 5), DayLoopError::InvalidCommand,
                  "a commitment that has not started has no elapsed minutes");

    DayLoopState running = committed;
    MustApply(running, SimpleCmd(DayCommandKind::Start), "start");
    ExpectRefusal(running, cats, SimpleCmd(DayCommandKind::Start), DayLoopError::Busy, "start twice");
    ExpectRefusal(running, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::Busy, "time passes only through the activity");
    ExpectRefusal(running, cats, SimpleCmd(DayCommandKind::ArriveSchool), DayLoopError::Busy, "no arriving mid-activity");
    ExpectRefusal(running, cats, SimpleCmd(DayCommandKind::GoToSleep), DayLoopError::Busy, "no sleeping mid-activity");
    ExpectRefusal(running, cats, CommitCmd("activity.rest", "home"), DayLoopError::AlreadyCommitted, "no second commitment");
    ExpectRefusal(running, cats, SimpleCmd(DayCommandKind::Cancel, 120), DayLoopError::InvalidCommand,
                  "cancelling at the end is completing");
    ExpectRefusal(running, cats, SimpleCmd(DayCommandKind::Cancel, -1), DayLoopError::InvalidCommand, "negative elapsed minutes");
    DayLoopCatalogues noActivities;
    ExpectRefusal(running, noActivities, SimpleCmd(DayCommandKind::Complete), DayLoopError::MissingCatalogue, "completing needs the catalogue");

    // A commitment that is not startable (the place changed under it) is refused at the start, state untouched.
    DayLoopState stale = committed;
    stale.Commitment.PlaceKind = "classroom";
    ExpectRefusal(stale, cats, SimpleCmd(DayCommandKind::Start), DayLoopError::ActivityRefused, "the activity refuses a wrong place");
}

void TestTimeAndSleep()
{
    const DayLoopState planned = Planned();
    const DayLoopCatalogues& cats = DayLoopCats();
    ExpectRefusal(planned, cats, PassCmd(0, DayPassKind::Wait), DayLoopError::InvalidCommand, "pass at least a minute");
    ExpectRefusal(planned, cats, PassCmd(1441, DayPassKind::Wait), DayLoopError::InvalidCommand, "pass at most a day");
    ExpectRefusal(planned, cats, PassCmd(-5, DayPassKind::Wait), DayLoopError::InvalidCommand, "time does not run backwards");
    DayCommand odd = PassCmd(10, DayPassKind::Wait);
    odd.Pass = static_cast<DayPassKind>(9);
    ExpectRefusal(planned, cats, odd, DayLoopError::InvalidCommand, "an unknown kind of pass");
    DayCommand unknown = SimpleCmd(DayCommandKind::Wake);
    unknown.Kind = static_cast<DayCommandKind>(99);
    ExpectRefusal(planned, cats, unknown, DayLoopError::InvalidCommand, "an unknown command");
    ExpectRefusal(planned, cats, SimpleCmd(DayCommandKind::Start), DayLoopError::NotCommitted, "start needs a commitment");

    DayLoopState asleep = planned;
    MustApply(asleep, SimpleCmd(DayCommandKind::GoToSleep), "sleep");
    Check(asleep.Phase == DayPhase::Sleep && asleep.Asleep && asleep.SleptAt == 0, "the sleeper is in the sleep phase");
    ExpectRefusal(asleep, cats, SimpleCmd(DayCommandKind::GoToSleep), DayLoopError::Asleep, "already asleep");
    ExpectRefusal(asleep, cats, PlanCmd({}), DayLoopError::Asleep, "no planning in one's sleep");
    ExpectRefusal(asleep, cats, CommitCmd("activity.rest", "home"), DayLoopError::Asleep, "no committing in one's sleep");
    ExpectRefusal(asleep, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::Asleep, "no passing while asleep");
    ExpectRefusal(asleep, cats, SimpleCmd(DayCommandKind::ArriveSchool), DayLoopError::Asleep, "no arriving while asleep");
    ExpectRefusal(asleep, cats, SimpleCmd(DayCommandKind::Wake, 0), DayLoopError::InvalidCommand, "sleep at least a minute");
    ExpectRefusal(asleep, cats, SimpleCmd(DayCommandKind::Wake, 1441), DayLoopError::InvalidCommand, "sleep at most a day");
    ExpectRefusal(asleep, cats, SimpleCmd(DayCommandKind::Start), DayLoopError::Asleep, "no starting while asleep");

    // The end of the clock is a refusal, not a wrap.
    DayLoopState late = NewDayLoopState(DayLoopMaxInstant - 5);
    MustApply(late, PlanCmd({}), "plan at the edge of time");
    ExpectRefusal(late, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::InvalidTime, "the clock stops at a hundred years");
    MustApply(late, SimpleCmd(DayCommandKind::GoToSleep), "sleep at the edge of time");
    ExpectRefusal(late, cats, SimpleCmd(DayCommandKind::Wake, 10), DayLoopError::InvalidTime, "waking past the end of the clock");
}

void TestSchoolRefusals()
{
    const DayLoopCatalogues& cats = DayLoopCats();
    DayLoopCatalogues noSchool;
    noSchool.Activities = cats.Activities;
    const DayLoopState planned = Planned();
    ExpectRefusal(planned, noSchool, SimpleCmd(DayCommandKind::ArriveSchool), DayLoopError::NoSchool, "a life without a school");

    DayCommand badDeparture = SimpleCmd(DayCommandKind::ArriveSchool);
    badDeparture.DepartureMinute = -3;
    ExpectRefusal(planned, cats, badDeparture, DayLoopError::AttendanceRefused, "the departure is a minute of the day");

    // Saturday: day 5 of the campaign.
    DayLoopState saturday = Planned(5 * MinutesPerDay);
    ExpectRefusal(saturday, cats, SimpleCmd(DayCommandKind::ArriveSchool), DayLoopError::AttendanceRefused, "no school on Saturday");

    DayLoopState arrived = planned;
    MustApply(arrived, PassCmd(200, DayPassKind::Wait), "wait");
    MustApply(arrived, SimpleCmd(DayCommandKind::ArriveSchool), "arrive");
    ExpectRefusal(arrived, cats, SimpleCmd(DayCommandKind::ArriveSchool), DayLoopError::AttendanceRefused, "one arrival a day");
}

void TestInvalidStates()
{
    const DayLoopCatalogues& cats = DayLoopCats();
    DayLoopState wrongPhase = Planned();
    wrongPhase.Phase = DayPhase::Evening;
    ExpectRefusal(wrongPhase, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::InvalidState, "a phase that disagrees with the state");
    Check(SerialiseDayLoop(wrongPhase).empty() && HashDayLoop(wrongPhase) == 0, "an invalid state has no text and no hash");

    DayLoopState badMeter = Planned();
    badMeter.Condition.Energy = MeterMax + 1;
    ExpectRefusal(badMeter, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::InvalidState, "a condition meter off its scale");

    DayLoopState stuck = Planned();
    stuck.Runner.ActivityId = "activity.rest";
    stuck.Runner.Instance = 1;
    ExpectRefusal(stuck, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::InvalidState, "a running activity without a commitment");

    DayLoopState overdue = Planned();
    overdue.Practice.LastAt = 100;
    ExpectRefusal(overdue, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::InvalidState, "a practice record from the future");

    DayLoopState counted = Planned();
    counted.Seq = 0;
    counted.HasLastAction = true;
    ExpectRefusal(counted, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::InvalidState, "a last action without a command count");

    DayLoopState maxed = Planned();
    maxed.Seq = UINT64_MAX;
    ExpectRefusal(maxed, cats, PassCmd(10, DayPassKind::Wait), DayLoopError::InvalidState, "the sequence number cannot wrap");
}
} // namespace

void TestDayLoopRefusals()
{
    TestBeforeThePlan();
    TestCommitments();
    TestTimeAndSleep();
    TestSchoolRefusals();
    TestInvalidStates();
}
