#include "TimeContract.h"

#include <charconv>

// The mode controller (which mode is live, free-roam real time) and the text forms
// of the two states this module introduces: TimeModeState and InteractionRun.
// Integers and stable names only; the parsers are strict, so a truncated or edited
// text is refused rather than half read.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view TimeContractModesHeader = "pw.time_modes/1";
constexpr std::string_view TimeContractRunHeader = "pw.time_run/1";
constexpr std::string_view TimeContractEnd = "end";

bool TimeContractNextLine(std::string_view text, std::size_t& position, std::string_view& line)
{
    const std::size_t newline = text.find('\n', position);
    if (newline == std::string_view::npos)
        return false; // no newline left: what a truncated text looks like
    line = text.substr(position, newline - position);
    position = newline + 1;
    return true;
}

bool TimeContractKeyValue(std::string_view line, std::string_view key, std::string_view& value)
{
    if (line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

/** Canonical decimal only: no '+', no leading zeros, no "-0", nothing trailing. */
template <typename Int>
bool TimeContractParseInt(std::string_view text, Int& out)
{
    if (text.empty())
        return false;
    const std::string_view digits = text.front() == '-' ? text.substr(1) : text;
    if (digits.empty() || (digits.size() > 1 && digits.front() == '0') || (text.front() == '-' && digits == "0"))
        return false;
    for (const char c : digits)
    {
        if (c < '0' || c > '9')
            return false;
    }
    Int value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size())
        return false;
    out = value;
    return true;
}

bool TimeContractParseBool(std::string_view text, bool& out)
{
    if (text != "0" && text != "1")
        return false;
    out = text == "1";
    return true;
}

/** Reads the next "key=value" line into `value`; false when absent or the key differs. */
bool TimeContractField(std::string_view text, std::size_t& position, std::string_view key, std::string_view& value)
{
    std::string_view line;
    return TimeContractNextLine(text, position, line) && TimeContractKeyValue(line, key, value);
}

template <typename Int>
bool TimeContractIntField(std::string_view text, std::size_t& position, std::string_view key, Int& out)
{
    std::string_view value;
    return TimeContractField(text, position, key, value) && TimeContractParseInt(value, out);
}

constexpr std::array<std::string_view, 5> TimeContractOutcomeNames{"running", "completed", "interrupted_at_beat",
                                                                   "interrupted_immediately", "cancelled"};

/** True when a run (as parsed from text) could have arisen from Begin and Step. */
bool TimeContractRunConsistent(const InteractionRun& run)
{
    if (ValidateDeclaration(run.Decl) != TimeContractError::None || run.InterruptsHeld < 0 || run.EventsConsumed < 0 ||
        run.UnusedProgress < 0 || run.Progress < 0 || run.Progress > MaxSingleAdvanceMinutes)
        return false;
    const bool bounded = run.Decl.Mode != ClockMode::Mission && run.Decl.CostMinutes > 0;
    if (InteractionAbsorbsProgress(run.Decl) && run.Progress != 0)
        return false;
    if (bounded && run.Progress > run.Decl.CostMinutes)
        return false;
    if (run.Outcome == TimeOutcome::Running &&
        (run.Decl.Resolve == ResolutionBoundary::OnCommit || (bounded && run.Progress >= run.Decl.CostMinutes) ||
         (run.StopPoint >= 0 && run.StopPoint < run.Progress)))
        return false;
    // A pending stop exists only for an at_beat interaction with a length, and stays inside it (any outcome may follow it).
    if (run.StopPoint < -1 || (run.StopPoint >= 0 && (!bounded || run.StopPoint > run.Decl.CostMinutes ||
                                                      run.Decl.Interrupt != InterruptPolicy::AtBeat)))
        return false;
    if (run.Outcome == TimeOutcome::InterruptedAtBeat &&
        (run.Decl.Interrupt != InterruptPolicy::AtBeat || run.Progress >= run.Decl.CostMinutes ||
         run.Progress != BeatBoundary(run.Decl, BeatsCoveredBy(run.Decl, run.Progress))))
        return false;
    if (run.Outcome == TimeOutcome::Completed && !TimeModeRule(run.Decl.Mode).EarlyFinishAllowed && run.Progress != run.Decl.CostMinutes)
        return false;
    if (run.Decl.Mode == ClockMode::Compressed && run.Outcome != TimeOutcome::Running &&
        run.Progress != BeatBoundary(run.Decl, BeatsCoveredBy(run.Decl, run.Progress)))
        return false;
    if (run.Outcome == TimeOutcome::InterruptedImmediately && run.Decl.Interrupt != InterruptPolicy::Immediate)
        return false;
    if (run.StopPoint >= 0 && run.StopPoint != BeatBoundary(run.Decl, BeatsCoveredBy(run.Decl, run.StopPoint)) &&
        run.StopPoint != run.Decl.CostMinutes)
        return false;
    return true;
}
} // namespace

bool IsTimeModeStateValid(const TimeModeState& state) noexcept
{
    const bool overlayBase = state.Base == ClockMode::UiPaused || state.Base == ClockMode::AccessibilityPause;
    if (static_cast<std::size_t>(state.Base) >= ClockModeCount || overlayBase)
        return false;
    if (state.CarryMilliseconds < 0 || state.CarryMilliseconds >= RealMillisecondsPerGameMinute)
        return false;
    return state.CarryMilliseconds == 0 || (state.Base == ClockMode::FreeRoam && !state.UiOpen && !state.AccessibilityOpen);
}

ClockMode EffectiveClockMode(const TimeModeState& state) noexcept
{
    if (state.AccessibilityOpen)
        return ClockMode::AccessibilityPause;
    return state.UiOpen ? ClockMode::UiPaused : state.Base;
}

bool WorldClockRuns(const TimeModeState& state) noexcept
{
    return TimeModeRule(EffectiveClockMode(state)).World == WorldClockBehaviour::Runs;
}

bool ManualSaveAllowed(const TimeModeState& state) noexcept
{
    return TimeModeRule(state.Base).ManualSaveAllowed;
}

bool EnterBaseMode(TimeModeState& state, ClockMode mode) noexcept
{
    if (static_cast<std::size_t>(mode) >= ClockModeCount || mode == ClockMode::UiPaused || mode == ClockMode::AccessibilityPause ||
        state.UiOpen || state.AccessibilityOpen)
        return false;
    if (mode != state.Base)
    {
        state.Base = mode;
        state.CarryMilliseconds = 0;
    }
    return true;
}

void SetUiPause(TimeModeState& state, bool open) noexcept
{
    state.UiOpen = open;
    state.CarryMilliseconds = 0;
}

void SetAccessibilityPause(TimeModeState& state, bool open) noexcept
{
    state.AccessibilityOpen = open;
    state.CarryMilliseconds = 0;
}

std::int64_t AdvanceRealTime(TimeModeState& state, std::int64_t realMilliseconds) noexcept
{
    if (realMilliseconds <= 0 || !WorldClockRuns(state))
        return 0;
    const std::int64_t whole = realMilliseconds / RealMillisecondsPerGameMinute;
    const std::int64_t rest = state.CarryMilliseconds + realMilliseconds % RealMillisecondsPerGameMinute;
    state.CarryMilliseconds = static_cast<std::int32_t>(rest % RealMillisecondsPerGameMinute);
    return whole + rest / RealMillisecondsPerGameMinute;
}

std::string SerialiseTimeModeState(const TimeModeState& state)
{
    if (!IsTimeModeStateValid(state))
        return {};
    std::string text(TimeContractModesHeader);
    text += "\nbase=" + std::string(ClockModeName(state.Base));
    text += "\nui=" + std::string(state.UiOpen ? "1" : "0");
    text += "\naccessibility=" + std::string(state.AccessibilityOpen ? "1" : "0");
    text += "\ncarry_ms=" + std::to_string(state.CarryMilliseconds);
    text += "\n";
    text += TimeContractEnd;
    text += "\n";
    return text;
}

TimeModeStateParse DeserialiseTimeModeState(std::string_view text)
{
    TimeModeStateParse result;
    const auto refuse = [&](const char* message) {
        result.Error = message;
        result.State = TimeModeState{};
        return result;
    };
    std::size_t position = 0;
    std::string_view line;
    std::string_view value;
    if (!TimeContractNextLine(text, position, line) || line != TimeContractModesHeader)
        return refuse("The time mode text has no recognised header.");
    TimeModeState state;
    if (!TimeContractField(text, position, "base", value) || !ClockModeFromName(value, state.Base))
        return refuse("The time mode text has no valid base line.");
    if (!TimeContractField(text, position, "ui", value) || !TimeContractParseBool(value, state.UiOpen))
        return refuse("The time mode text has no valid ui line.");
    if (!TimeContractField(text, position, "accessibility", value) || !TimeContractParseBool(value, state.AccessibilityOpen))
        return refuse("The time mode text has no valid accessibility line.");
    if (!TimeContractIntField(text, position, "carry_ms", state.CarryMilliseconds))
        return refuse("The time mode text has no valid carry_ms line.");
    if (!TimeContractNextLine(text, position, line) || line != TimeContractEnd)
        return refuse("The time mode text is truncated or has no end line.");
    if (position != text.size())
        return refuse("The time mode text has data after the end line.");
    if (!IsTimeModeStateValid(state))
        return refuse("The time mode text describes an inconsistent state.");
    result.State = state;
    return result;
}

std::string SerialiseInteractionRun(const InteractionRun& run)
{
    if (!TimeContractRunConsistent(run))
        return {};
    const InteractionDeclaration& d = run.Decl;
    std::string text(TimeContractRunHeader);
    text += "\nid=" + d.Id;
    text += "\nmode=" + std::string(ClockModeName(d.Mode));
    text += "\ncost=" + std::to_string(d.CostMinutes);
    text += "\ninterrupt=" + std::string(InterruptPolicyName(d.Interrupt));
    text += "\nresolve=" + std::string(ResolutionBoundaryName(d.Resolve));
    text += "\nbeats=" + std::to_string(d.Beats);
    text += "\nfixed=" + std::to_string(d.Mission.FixedMinutes);
    text += "\nrate=" + std::to_string(d.Mission.RatePermille);
    text += "\ncap=" + std::to_string(d.Mission.CapMinutes);
    text += "\nprogress=" + std::to_string(run.Progress);
    text += "\nstop=" + std::to_string(run.StopPoint);
    text += "\nheld=" + std::to_string(run.InterruptsHeld);
    text += "\nevents=" + std::to_string(run.EventsConsumed);
    text += "\nunused=" + std::to_string(run.UnusedProgress);
    text += "\noutcome=" + std::string(TimeContractOutcomeNames[static_cast<std::size_t>(run.Outcome)]);
    text += "\n";
    text += TimeContractEnd;
    text += "\n";
    return text;
}

InteractionRunParse DeserialiseInteractionRun(std::string_view text)
{
    InteractionRunParse result;
    const auto refuse = [&](const char* message) {
        result.Error = message;
        result.Run = InteractionRun{};
        return result;
    };
    std::size_t position = 0;
    std::string_view line;
    std::string_view value;
    if (!TimeContractNextLine(text, position, line) || line != TimeContractRunHeader)
        return refuse("The time run text has no recognised header.");
    InteractionRun run;
    InteractionDeclaration& d = run.Decl;
    if (!TimeContractField(text, position, "id", value))
        return refuse("The time run text has no id line.");
    d.Id = std::string(value);
    if (!TimeContractField(text, position, "mode", value) || !ClockModeFromName(value, d.Mode))
        return refuse("The time run text has no valid mode line.");
    if (!TimeContractIntField(text, position, "cost", d.CostMinutes))
        return refuse("The time run text has no valid cost line.");
    if (!TimeContractField(text, position, "interrupt", value) || !InterruptPolicyFromName(value, d.Interrupt))
        return refuse("The time run text has no valid interrupt line.");
    if (!TimeContractField(text, position, "resolve", value) || !ResolutionBoundaryFromName(value, d.Resolve))
        return refuse("The time run text has no valid resolve line.");
    if (!TimeContractIntField(text, position, "beats", d.Beats) || !TimeContractIntField(text, position, "fixed", d.Mission.FixedMinutes) ||
        !TimeContractIntField(text, position, "rate", d.Mission.RatePermille) ||
        !TimeContractIntField(text, position, "cap", d.Mission.CapMinutes))
        return refuse("The time run text has an invalid beats or mission line.");
    if (!TimeContractIntField(text, position, "progress", run.Progress) || !TimeContractIntField(text, position, "stop", run.StopPoint) ||
        !TimeContractIntField(text, position, "held", run.InterruptsHeld) ||
        !TimeContractIntField(text, position, "events", run.EventsConsumed) ||
        !TimeContractIntField(text, position, "unused", run.UnusedProgress))
        return refuse("The time run text has an invalid progress line.");
    if (!TimeContractField(text, position, "outcome", value))
        return refuse("The time run text has no outcome line.");
    bool known = false;
    for (std::size_t i = 0; i < TimeContractOutcomeNames.size(); ++i)
    {
        if (TimeContractOutcomeNames[i] == value)
        {
            run.Outcome = static_cast<TimeOutcome>(i);
            known = true;
        }
    }
    if (!known)
        return refuse("The time run text has an unknown outcome.");
    if (!TimeContractNextLine(text, position, line) || line != TimeContractEnd)
        return refuse("The time run text is truncated or has no end line.");
    if (position != text.size())
        return refuse("The time run text has data after the end line.");
    if (!TimeContractRunConsistent(run))
        return refuse("The time run text describes an inconsistent state.");
    result.Run = std::move(run);
    return result;
}
} // namespace PerfectWorld
