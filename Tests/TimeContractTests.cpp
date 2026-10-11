// ROADMAP M3-01: the six clock modes' rules, declaration validation, the mode
// controller and the text forms. Resolution, content and equivalence are in the
// sibling TimeContract*Tests.cpp files.
#include "TimeContract.h"
#include "TimeContractTestSupport.h"

using namespace PerfectWorld;

void TestModeRules()
{
    Expect(ClockModeCount == 7, "six documented modes plus compressed");
    for (std::size_t i = 0; i < ClockModeCount; ++i)
    {
        const ClockModeRule& rule = TimeModeRule(static_cast<ClockMode>(i));
        Expect(static_cast<std::size_t>(rule.Mode) == i, "the rule table is in enum order");
        ClockMode back = ClockMode::Scene;
        Expect(ClockModeFromName(rule.Name, back) && back == rule.Mode, "a mode name round-trips");
        Expect(ClockModeName(rule.Mode) == rule.Name, "the mode name is the rule name");
    }
    ClockMode unused = ClockMode::Scene;
    Expect(!ClockModeFromName("slow", unused) && !ClockModeFromName("", unused) && !ClockModeFromName("Scene", unused),
           "an unknown mode name is refused");

    const ClockModeRule& roam = TimeModeRule(ClockMode::FreeRoam);
    Expect(roam.World == WorldClockBehaviour::Runs && roam.Charge == ChargeRule::PerRealSecond, "free roam runs the world clock");
    Expect(roam.WorldMilliMinutesPerRealSecond == 1000 && RealMillisecondsPerGameMinute == 1000, "one game minute per real second");
    Expect(roam.PausedByOverlays, "menus pause free roam");

    const ClockModeRule& talk = TimeModeRule(ClockMode::Conversation);
    Expect(talk.World == WorldClockBehaviour::Paused && talk.Charge == ChargeRule::DeclaredAtClose, "a conversation pauses the world");
    Expect(talk.MinCostMinutes == 0 && talk.MaxCostMinutes == TimeConversationMaxCostMinutes, "a conversation costs nothing, or up to the cap");

    const ClockModeRule& scene = TimeModeRule(ClockMode::Scene);
    Expect(scene.World == WorldClockBehaviour::Paused && scene.Charge == ChargeRule::DeclaredAtResolution, "a scene pauses the world");
    Expect(scene.MinCostMinutes == 1 && scene.MaxCostMinutes == 1440, "a scene costs one minute to one day");
    Expect(scene.AllowedResolutions != 0 && !scene.EarlyFinishAllowed, "a scene is not ended early, only cancelled");

    for (const ClockMode paused : {ClockMode::UiPaused, ClockMode::AccessibilityPause})
    {
        const ClockModeRule& rule = TimeModeRule(paused);
        Expect(rule.World == WorldClockBehaviour::Paused && rule.Charge == ChargeRule::Never, "phone, menus and accessibility pause the world");
        Expect(!rule.ChargesOpportunityTime && rule.MaxCostMinutes == 0, "a pause never consumes opportunity time");
    }
    const ClockModeRule& mission = TimeModeRule(ClockMode::Mission);
    Expect(mission.World == WorldClockBehaviour::MissionClockRuns && mission.Charge == ChargeRule::MissionFormula, "the mission clock runs");
    Expect(!mission.ManualSaveAllowed, "no manual save during a mission");
    for (std::size_t i = 0; i < ClockModeCount; ++i)
    {
        if (static_cast<ClockMode>(i) != ClockMode::Mission)
            Expect(TimeModeRule(static_cast<ClockMode>(i)).ManualSaveAllowed, "manual saves are allowed outside a mission");
    }
    const ClockModeRule& compressed = TimeModeRule(ClockMode::Compressed);
    Expect(compressed.World == WorldClockBehaviour::Jumps && compressed.Charge == ChargeRule::SpanResolved, "compression jumps the clock");

    InterruptPolicy policy = InterruptPolicy::None;
    ResolutionBoundary boundary = ResolutionBoundary::OnCommit;
    Expect(InterruptPolicyFromName("at_beat", policy) && policy == InterruptPolicy::AtBeat, "at_beat parses");
    Expect(!InterruptPolicyFromName("soon", policy), "an unknown interrupt policy is refused");
    Expect(ResolutionBoundaryFromName("per_beat", boundary) && boundary == ResolutionBoundary::PerBeat, "per_beat parses");
    Expect(!ResolutionBoundaryFromName("on_exit", boundary), "an unknown resolution boundary is refused");
}

void TestDeclarations()
{
    using M = ClockMode;
    using I = InterruptPolicy;
    using R = ResolutionBoundary;
    const auto sound = [](const InteractionDeclaration& d, const char* what) {
        Expect(ValidateDeclaration(d) == TimeContractError::None, what);
    };
    const auto refused = [](const InteractionDeclaration& d, TimeContractError error, const char* what) {
        Expect(ValidateDeclaration(d) == error, what);
    };
    sound(MakeDecl("a.scene", M::Scene, 45, I::AtBeat, R::OnComplete), "a scene declares");
    sound(MakeDecl("a.scene3", M::Scene, 45, I::AtBeat, R::PerBeat, 3), "a per-beat scene declares");
    sound(MakeDecl("a.sleep", M::Scene, 480, I::Immediate, R::OnComplete), "sleep declares");
    sound(MakeDecl("a.instant", M::Scene, 5, I::None, R::OnCommit), "an on-commit scene declares");
    sound(MakeDecl("a.run", M::FreeRoam, 30, I::Immediate, R::OnComplete), "free roam declares");
    sound(MakeDecl("a.walk", M::FreeRoam, 0, I::None, R::OnComplete), "open-ended free roam declares");
    sound(MakeDecl("a.talk", M::Conversation, 10, I::AtBeat, R::OnComplete, 2), "a conversation declares");
    sound(MakeDecl("a.free_talk", M::Conversation, 0, I::None, R::OnComplete), "a free conversation declares");
    sound(MakeDecl("a.phone", M::UiPaused, 0, I::None, R::OnCommit), "the phone declares");
    sound(MakeDecl("a.text_speed", M::AccessibilityPause, 0, I::None, R::OnComplete), "an accessibility adjustment declares");
    sound(MakeDecl("a.week", M::Compressed, 7 * 1440, I::AtBeat, R::PerBeat, 7), "a compressed week declares");
    InteractionDeclaration fixed = MakeDecl("a.tower", M::Mission, 600, I::Immediate, R::OnComplete);
    fixed.Mission.FixedMinutes = 600;
    sound(fixed, "a fixed-cost mission declares");
    InteractionDeclaration rated = MakeDecl("a.gate", M::Mission, 2880, I::None, R::OnComplete);
    rated.Mission.RatePermille = 250;
    rated.Mission.CapMinutes = 2880;
    sound(rated, "a rated mission declares");

    refused(MakeDecl("a.s", M::Scene, 0, I::None, R::OnComplete), TimeContractError::CostOutOfRange, "a scene costs at least a minute");
    refused(MakeDecl("a.s", M::Scene, 1441, I::None, R::OnComplete), TimeContractError::CostOutOfRange, "a scene costs at most a day");
    refused(MakeDecl("a.s", M::Scene, -5, I::None, R::OnComplete), TimeContractError::CostOutOfRange, "a negative cost is refused");
    refused(MakeDecl("a.p", M::UiPaused, 5, I::None, R::OnComplete), TimeContractError::CostOutOfRange, "the phone is never charged");
    refused(MakeDecl("a.p", M::AccessibilityPause, 1, I::None, R::OnCommit), TimeContractError::CostOutOfRange, "an accessibility pause is never charged");
    refused(MakeDecl("a.t", M::Conversation, 61, I::None, R::OnComplete), TimeContractError::CostOutOfRange, "a talk never silently costs hours");
    refused(MakeDecl("a.c", M::Compressed, 366 * 1440 + 1, I::None, R::OnComplete), TimeContractError::CostOutOfRange, "a compressed span is at most a year");
    refused(MakeDecl("a.p", M::UiPaused, 0, I::Immediate, R::OnCommit), TimeContractError::InterruptNotAllowed, "a paused menu cannot be interrupted");
    refused(MakeDecl("a.w", M::FreeRoam, 0, I::AtBeat, R::OnComplete), TimeContractError::InterruptNotAllowed, "a zero cost has no beats to wait for");
    refused(MakeDecl("a.m", M::Mission, 60, I::AtBeat, R::OnComplete), TimeContractError::InterruptNotAllowed, "a mission has no beats");
    refused(MakeDecl("a.c", M::Compressed, 1440, I::Immediate, R::OnComplete), TimeContractError::InterruptNotAllowed, "compression stops only at a beat");
    refused(MakeDecl("a.w", M::FreeRoam, 30, I::None, R::OnCommit), TimeContractError::ResolveNotAllowed, "free roam cannot resolve on commit");
    refused(MakeDecl("a.t", M::Conversation, 10, I::None, R::PerBeat, 2), TimeContractError::ResolveNotAllowed, "a talk resolves at its close");
    refused(MakeDecl("a.s", M::Scene, 30, I::AtBeat, R::OnCommit), TimeContractError::CommitCannotBeInterrupted, "a commit cannot be interrupted");
    refused(MakeDecl("a.s", M::Scene, 30, I::None, R::OnCommit, 2), TimeContractError::CommitCannotBeInterrupted, "a commit has one beat");
    refused(MakeDecl("a.s", M::Scene, 30, I::AtBeat, R::OnComplete, 0), TimeContractError::BeatsOutOfRange, "no beats is refused");
    refused(MakeDecl("a.s", M::Scene, 3, I::AtBeat, R::OnComplete, 4), TimeContractError::BeatsOutOfRange, "more beats than minutes is refused");
    refused(MakeDecl("a.s", M::Scene, 1440, I::AtBeat, R::OnComplete, 401), TimeContractError::BeatsOutOfRange, "too many beats is refused");
    refused(MakeDecl("a.s", M::Scene, 30, I::AtBeat, R::PerBeat, 1), TimeContractError::BeatsOutOfRange, "per-beat needs two beats");
    refused(MakeDecl("A.s", M::Scene, 30, I::None, R::OnComplete), TimeContractError::BadId, "an upper-case id is refused");
    refused(MakeDecl("", M::Scene, 30, I::None, R::OnComplete), TimeContractError::BadId, "an empty id is refused");
    refused(MakeDecl("a s", M::Scene, 30, I::None, R::OnComplete), TimeContractError::BadId, "a spaced id is refused");
    refused(MakeDecl("a.m", M::Mission, 60, I::None, R::OnComplete), TimeContractError::MissionTermsMissing, "a mission needs terms");

    InteractionDeclaration both = fixed;
    both.Mission.RatePermille = 500;
    refused(both, TimeContractError::MissionTermsInvalid, "a mission is fixed or rated, not both");
    InteractionDeclaration fast = rated;
    fast.Mission.RatePermille = 1001;
    refused(fast, TimeContractError::MissionTermsInvalid, "the outside world may not run faster than the mission clock");
    InteractionDeclaration mismatch = rated;
    mismatch.CostMinutes = 100;
    refused(mismatch, TimeContractError::MissionTermsInvalid, "a mission's declared cost is its cap");
    InteractionDeclaration capless = rated;
    capless.Mission.CapMinutes = 0;
    refused(capless, TimeContractError::MissionTermsInvalid, "a rated mission needs a cap");
    InteractionDeclaration years = fixed;
    years.Mission.FixedMinutes = years.CostMinutes = TimeMissionMaxCapMinutes + 1;
    refused(years, TimeContractError::CostOutOfRange, "a mission costs weeks of a life, not years");
    InteractionDeclaration stray = MakeDecl("a.s", M::Scene, 30, I::None, R::OnComplete);
    stray.Mission.FixedMinutes = 30;
    refused(stray, TimeContractError::MissionTermsOnOtherMode, "mission terms on another mode are refused");

    InteractionDeclaration odd = MakeDecl("a.s", M::Scene, 30, I::None, R::OnComplete);
    odd.Mode = static_cast<ClockMode>(99);
    refused(odd, TimeContractError::UnknownMode, "an unknown mode value is refused");
    odd = MakeDecl("a.s", M::Scene, 30, I::None, R::OnComplete);
    odd.Interrupt = static_cast<InterruptPolicy>(9);
    refused(odd, TimeContractError::UnknownInterrupt, "an unknown interrupt value is refused");
    odd = MakeDecl("a.s", M::Scene, 30, I::None, R::OnComplete);
    odd.Resolve = static_cast<ResolutionBoundary>(9);
    refused(odd, TimeContractError::UnknownResolve, "an unknown resolve value is refused");
    Expect(TimeContractErrorCode(TimeContractError::None).empty() && TimeContractErrorCode(TimeContractError::MissingDeclaration) == "time.missing_declaration",
           "error codes are stable");

    const InteractionDeclaration beats = MakeDecl("a.s", M::Scene, 45, I::AtBeat, R::PerBeat, 3);
    Expect(BeatBoundary(beats, 0) == 0 && BeatBoundary(beats, 1) == 15 && BeatBoundary(beats, 3) == 45 && BeatBoundary(beats, 9) == 45, "beat boundaries divide the cost");
    const InteractionDeclaration uneven = MakeDecl("a.s", M::Scene, 10, I::AtBeat, R::PerBeat, 3);
    Expect(BeatBoundary(uneven, 1) == 3 && BeatBoundary(uneven, 2) == 6 && BeatBoundary(uneven, 3) == 10, "uneven beats floor, the last takes the remainder");
    for (std::int64_t p = 0; p <= 12; ++p)
    {
        std::int32_t k = 0;
        while (k < 3 && BeatBoundary(uneven, k + 1) <= p)
            ++k;
        Expect(BeatsCoveredBy(uneven, p) == k, "beats covered agrees with the boundaries");
    }
}

void TestController()
{
    TimeModeState state;
    Expect(IsTimeModeStateValid(state) && state.Base == ClockMode::FreeRoam && WorldClockRuns(state), "free roam is the opening mode");
    Expect(AdvanceRealTime(state, 60'000) == 60, "60 real seconds add 60 game minutes");
    Expect(AdvanceRealTime(state, 0) == 0 && AdvanceRealTime(state, -5000) == 0, "no or negative time adds nothing");

    state = TimeModeState{};
    Expect(AdvanceRealTime(state, 1500) == 1 && state.CarryMilliseconds == 500, "a partial second is carried");
    Expect(AdvanceRealTime(state, 500) == 1 && state.CarryMilliseconds == 0, "two halves make a minute");
    Expect(AdvanceRealTime(state, 999) == 0 && AdvanceRealTime(state, 1) == 1, "the carry is exact");

    // Splitting real time anywhere gives the same total (additivity, SYSTEMS §2.4).
    TimeTestRng rng(7);
    for (int trial = 0; trial < 200; ++trial)
    {
        TimeModeState whole;
        TimeModeState parts;
        std::int64_t total = 0;
        std::int64_t sum = 0;
        for (int i = 0; i < 20; ++i)
        {
            const std::int64_t ms = rng.Range(0, 5000);
            total += ms;
            sum += AdvanceRealTime(parts, ms);
        }
        Expect(AdvanceRealTime(whole, total) == sum && whole == parts, "real time is additive");
    }

    SetUiPause(state, true);
    Expect(!WorldClockRuns(state) && EffectiveClockMode(state) == ClockMode::UiPaused, "the phone pauses free roam");
    Expect(AdvanceRealTime(state, 30'000) == 0, "30 seconds paused add nothing");
    SetAccessibilityPause(state, true);
    Expect(EffectiveClockMode(state) == ClockMode::AccessibilityPause && AdvanceRealTime(state, 30'000) == 0, "accessibility pause wins and adds nothing");
    SetAccessibilityPause(state, false);
    Expect(EffectiveClockMode(state) == ClockMode::UiPaused && !WorldClockRuns(state), "the menu is still open under the pause");
    SetUiPause(state, false);
    Expect(WorldClockRuns(state) && AdvanceRealTime(state, 1000) == 1, "closing the menu resumes the clock");

    state = TimeModeState{};
    Expect(AdvanceRealTime(state, 700) == 0 && state.CarryMilliseconds == 700, "a fraction is held");
    SetUiPause(state, true);
    SetUiPause(state, false);
    Expect(state.CarryMilliseconds == 0, "a pause drops the fraction");
    Expect(AdvanceRealTime(state, 700) == 0, "so a pause cannot bank time");

    Expect(!EnterBaseMode(state, ClockMode::UiPaused) && !EnterBaseMode(state, ClockMode::AccessibilityPause), "overlays are not base modes");
    Expect(!EnterBaseMode(state, static_cast<ClockMode>(50)), "an unknown mode is refused");
    SetUiPause(state, true);
    Expect(!EnterBaseMode(state, ClockMode::Scene), "a scene cannot begin under an open menu");
    SetUiPause(state, false);
    Expect(AdvanceRealTime(state, 400) == 0, "re-arm the carry");
    Expect(EnterBaseMode(state, ClockMode::Scene) && state.CarryMilliseconds == 0, "entering a scene drops the fraction");
    Expect(!WorldClockRuns(state) && AdvanceRealTime(state, 60'000) == 0, "a scene does not tick with real time");
    Expect(ManualSaveAllowed(state) && EnterBaseMode(state, ClockMode::Mission) && !ManualSaveAllowed(state), "no manual save in a mission");
    SetUiPause(state, true);
    Expect(!ManualSaveAllowed(state), "an open menu does not make a mission saveable");
    SetUiPause(state, false);
    Expect(EnterBaseMode(state, ClockMode::Mission), "entering the same mode is accepted");
    Expect(EnterBaseMode(state, ClockMode::FreeRoam) && WorldClockRuns(state), "leaving the mission returns to free roam");
    for (const ClockMode mode : {ClockMode::Conversation, ClockMode::Compressed})
    {
        Expect(EnterBaseMode(state, mode) && !WorldClockRuns(state), "conversation and compression do not run on real time");
    }
}

void TestText()
{
    TimeModeState state;
    state.Base = ClockMode::FreeRoam;
    state.CarryMilliseconds = 250;
    const std::string text = SerialiseTimeModeState(state);
    Expect(!text.empty(), "a valid mode state serialises");
    const TimeModeStateParse back = DeserialiseTimeModeState(text);
    Expect(back.Ok() && back.State == state, "a mode state round-trips");
    for (std::size_t length = 0; length < text.size(); ++length)
        Expect(!DeserialiseTimeModeState(std::string_view(text).substr(0, length)).Ok(), "a truncated mode text is refused at every length");
    Expect(!DeserialiseTimeModeState(text + "x").Ok() && !DeserialiseTimeModeState(text + "\n").Ok(), "data after the end is refused");
    Expect(!DeserialiseTimeModeState("pw.time_modes/2\nbase=free_roam\nui=0\naccessibility=0\ncarry_ms=0\nend\n").Ok(), "an unknown version is refused");
    Expect(!DeserialiseTimeModeState("pw.time_modes/1\nbase=ui_paused\nui=0\naccessibility=0\ncarry_ms=0\nend\n").Ok(), "an overlay as the base is refused");
    Expect(!DeserialiseTimeModeState("pw.time_modes/1\nbase=scene\nui=0\naccessibility=0\ncarry_ms=5\nend\n").Ok(), "a carry outside free roam is refused");
    Expect(!DeserialiseTimeModeState("pw.time_modes/1\nbase=free_roam\nui=0\naccessibility=0\ncarry_ms=1000\nend\n").Ok(), "a whole-second carry is refused");
    Expect(!DeserialiseTimeModeState("pw.time_modes/1\nbase=free_roam\nui=2\naccessibility=0\ncarry_ms=0\nend\n").Ok(), "a bad flag is refused");
    Expect(!DeserialiseTimeModeState("pw.time_modes/1\nbase=free_roam\nui=0\naccessibility=0\ncarry_ms=007\nend\n").Ok(), "a non-canonical integer is refused");
    TimeModeState bad;
    bad.Base = ClockMode::AccessibilityPause;
    Expect(SerialiseTimeModeState(bad).empty(), "an invalid state does not serialise");
    TimeModeState paused;
    paused.UiOpen = true;
    paused.AccessibilityOpen = true;
    paused.Base = ClockMode::Mission;
    Expect(DeserialiseTimeModeState(SerialiseTimeModeState(paused)).State == paused, "overlay flags round-trip");

    InteractionDeclaration d = MakeDecl("activity.attend_class", ClockMode::Scene, 45, InterruptPolicy::AtBeat, ResolutionBoundary::PerBeat, 3);
    TimeContractError error = TimeContractError::None;
    InteractionRun run = BeginInteraction(d, error);
    Expect(error == TimeContractError::None, "the run begins");
    Expect(StepInteraction(run, Progress(10)) == StepResult::Consumed && StepInteraction(run, InterruptEvent()) == StepResult::Consumed, "step into a pending stop");
    const std::string runText = SerialiseInteractionRun(run);
    Expect(!runText.empty(), "a run in flight serialises");
    const InteractionRunParse parsed = DeserialiseInteractionRun(runText);
    Expect(parsed.Ok() && parsed.Run == run && run.StopPoint == 15, "a run round-trips with its pending stop");
    for (std::size_t length = 0; length < runText.size(); ++length)
        Expect(!DeserialiseInteractionRun(std::string_view(runText).substr(0, length)).Ok(), "a truncated run text is refused at every length");
    Expect(!DeserialiseInteractionRun(runText + "x").Ok(), "data after the run end is refused");

    // Save mid-way, load, continue: the same as an uninterrupted run.
    InteractionRun resumed = parsed.Run;
    Expect(StepInteraction(run, Progress(30)) == StepResult::Consumed && StepInteraction(resumed, Progress(30)) == StepResult::Consumed, "both continue");
    Expect(run == resumed && run.Outcome == TimeOutcome::InterruptedAtBeat && run.Progress == 15, "a reloaded run stops where the live run stops");

    const auto tampered = [&](const std::string& from, const std::string& to, const char* what) {
        std::string copy = runText;
        const std::size_t at = copy.find(from);
        Expect(at != std::string::npos, "the tamper target exists");
        copy.replace(at, from.size(), to);
        Expect(!DeserialiseInteractionRun(copy).Ok(), what);
    };
    tampered("progress=10", "progress=99", "progress past the cost is refused");
    tampered("stop=15", "stop=14", "a stop that is not a beat boundary is refused");
    tampered("stop=15", "stop=5", "a stop behind the progress is refused");
    tampered("outcome=running", "outcome=completed", "completion before the cost is refused");
    tampered("outcome=running", "outcome=sulking", "an unknown outcome is refused");
    tampered("mode=scene", "mode=slow", "an unknown mode is refused");
    tampered("cost=45", "cost=0", "an unsound declaration is refused");
    tampered("id=activity.attend_class", "id=Activity Attend", "a bad id is refused");
    tampered("interrupt=at_beat", "interrupt=none", "a stop without an at_beat policy is refused");
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldTimeContractTests <path to Data/Content>\n";
        return 2;
    }
    const std::vector<ContentFile> files = TimeTestReadContent(argv[1]);
    TestModeRules();
    TestDeclarations();
    TestController();
    TestText();
    TestResolveBasics();
    TestInterruptionPolicies();
    TestMissionClock();
    TestRandomResolution();
    TestContent(files);
    TestEquivalence();
    TestRandomEquivalence();
    std::cout << "PerfectWorld time contract: " << TimeTestChecks << " checks passed\n";
    return 0;
}
