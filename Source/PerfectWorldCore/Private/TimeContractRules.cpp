#include "TimeContract.h"

#include <algorithm>

// The six modes' rules and the declaration validator (ROADMAP M3-01; SYSTEMS §2.5).
namespace PerfectWorld
{
namespace
{
constexpr std::uint32_t TimeContractBit(InterruptPolicy policy)
{
    return 1u << static_cast<std::uint32_t>(policy);
}

constexpr std::uint32_t TimeContractBit(ResolutionBoundary boundary)
{
    return 1u << static_cast<std::uint32_t>(boundary);
}

constexpr std::uint32_t TimeContractAnyInterrupt =
    TimeContractBit(InterruptPolicy::None) | TimeContractBit(InterruptPolicy::AtBeat) | TimeContractBit(InterruptPolicy::Immediate);
constexpr std::uint32_t TimeContractAnyResolution = TimeContractBit(ResolutionBoundary::OnCommit) |
                                                    TimeContractBit(ResolutionBoundary::OnComplete) |
                                                    TimeContractBit(ResolutionBoundary::PerBeat);
constexpr std::uint32_t TimeContractComplete = TimeContractBit(ResolutionBoundary::OnComplete);

// Order matches ClockMode. Fields: mode, name, world, charge, milli-minutes/real second, paused-by-overlays,
// opportunity time, manual save, early finish, cost min, cost max, interrupts, resolutions, max beats.
constexpr std::array<ClockModeRule, ClockModeCount> TimeContractRules{{
    {ClockMode::FreeRoam, "free_roam", WorldClockBehaviour::Runs, ChargeRule::PerRealSecond,
     FreeRoamMilliMinutesPerRealSecond, true, true, true, true, 0, TimeSceneMaxCostMinutes, TimeContractAnyInterrupt,
     TimeContractComplete | TimeContractBit(ResolutionBoundary::PerBeat), TimeMaxBeats},
    {ClockMode::Conversation, "conversation", WorldClockBehaviour::Paused, ChargeRule::DeclaredAtClose, 0, true, true, true,
     true, 0, TimeConversationMaxCostMinutes, TimeContractAnyInterrupt, TimeContractComplete, TimeMaxBeats},
    {ClockMode::Scene, "scene", WorldClockBehaviour::Paused, ChargeRule::DeclaredAtResolution, 0, true, true, true, false, 1,
     TimeSceneMaxCostMinutes, TimeContractAnyInterrupt, TimeContractAnyResolution, TimeMaxBeats},
    {ClockMode::UiPaused, "ui_paused", WorldClockBehaviour::Paused, ChargeRule::Never, 0, true, false, true, true, 0, 0,
     TimeContractBit(InterruptPolicy::None), TimeContractBit(ResolutionBoundary::OnCommit) | TimeContractComplete, 1},
    {ClockMode::AccessibilityPause, "accessibility_pause", WorldClockBehaviour::Paused, ChargeRule::Never, 0, true, false,
     true, true, 0, 0, TimeContractBit(InterruptPolicy::None),
     TimeContractBit(ResolutionBoundary::OnCommit) | TimeContractComplete, 1},
    {ClockMode::Mission, "mission", WorldClockBehaviour::MissionClockRuns, ChargeRule::MissionFormula, 0, true, true, false,
     true, 1, TimeMissionMaxCapMinutes, TimeContractBit(InterruptPolicy::None) | TimeContractBit(InterruptPolicy::Immediate),
     TimeContractComplete, 1},
    {ClockMode::Compressed, "compressed", WorldClockBehaviour::Jumps, ChargeRule::SpanResolved, 0, false, true, true, false,
     1, TimeCompressedMaxCostMinutes, TimeContractBit(InterruptPolicy::None) | TimeContractBit(InterruptPolicy::AtBeat),
     TimeContractComplete | TimeContractBit(ResolutionBoundary::PerBeat), TimeMaxBeats},
}};

constexpr std::size_t TimeContractMaxIdLength = 128;

bool TimeContractIdWellFormed(std::string_view id)
{
    if (id.empty() || id.size() > TimeContractMaxIdLength)
        return false;
    return std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
    });
}

bool TimeContractMissionTermsEmpty(const MissionTerms& terms)
{
    return terms.FixedMinutes == 0 && terms.RatePermille == 0 && terms.CapMinutes == 0;
}

TimeContractError TimeContractValidateMission(const InteractionDeclaration& declaration)
{
    const MissionTerms& terms = declaration.Mission;
    if (TimeContractMissionTermsEmpty(terms))
        return TimeContractError::MissionTermsMissing;
    const bool fixed = terms.FixedMinutes > 0 && terms.RatePermille == 0 && terms.CapMinutes == 0;
    const bool rated = terms.FixedMinutes == 0 && terms.RatePermille >= 1 && terms.RatePermille <= TimeMissionMaxRatePermille &&
                       terms.CapMinutes >= 1;
    if (!fixed && !rated)
        return TimeContractError::MissionTermsInvalid;
    const std::int32_t most = fixed ? terms.FixedMinutes : terms.CapMinutes;
    if (most > TimeMissionMaxCapMinutes || declaration.CostMinutes != most)
        return TimeContractError::MissionTermsInvalid;
    return TimeContractError::None;
}
} // namespace

const ClockModeRule& TimeModeRule(ClockMode mode) noexcept
{
    return TimeContractRules[std::min<std::size_t>(static_cast<std::size_t>(mode), ClockModeCount - 1)];
}

std::string_view ClockModeName(ClockMode mode) noexcept
{
    return TimeModeRule(mode).Name;
}

bool ClockModeFromName(std::string_view name, ClockMode& out) noexcept
{
    for (const ClockModeRule& rule : TimeContractRules)
    {
        if (rule.Name == name)
        {
            out = rule.Mode;
            return true;
        }
    }
    return false;
}

std::string_view InterruptPolicyName(InterruptPolicy policy) noexcept
{
    switch (policy)
    {
    case InterruptPolicy::None: return "none";
    case InterruptPolicy::AtBeat: return "at_beat";
    case InterruptPolicy::Immediate: return "immediate";
    }
    return "none";
}

std::string_view ResolutionBoundaryName(ResolutionBoundary boundary) noexcept
{
    switch (boundary)
    {
    case ResolutionBoundary::OnCommit: return "on_commit";
    case ResolutionBoundary::OnComplete: return "on_complete";
    case ResolutionBoundary::PerBeat: return "per_beat";
    }
    return "on_complete";
}

bool InterruptPolicyFromName(std::string_view name, InterruptPolicy& out) noexcept
{
    for (const InterruptPolicy policy : {InterruptPolicy::None, InterruptPolicy::AtBeat, InterruptPolicy::Immediate})
    {
        if (InterruptPolicyName(policy) == name)
        {
            out = policy;
            return true;
        }
    }
    return false;
}

bool ResolutionBoundaryFromName(std::string_view name, ResolutionBoundary& out) noexcept
{
    for (const ResolutionBoundary boundary : {ResolutionBoundary::OnCommit, ResolutionBoundary::OnComplete, ResolutionBoundary::PerBeat})
    {
        if (ResolutionBoundaryName(boundary) == name)
        {
            out = boundary;
            return true;
        }
    }
    return false;
}

std::int64_t MissionOutsideMinutes(const MissionTerms& terms, std::int64_t missionMinutes) noexcept
{
    if (terms.FixedMinutes > 0)
        return terms.FixedMinutes;
    if (missionMinutes <= 0 || terms.RatePermille <= 0)
        return 0;
    const std::int64_t capped = std::min<std::int64_t>(missionMinutes, static_cast<std::int64_t>(TimeMissionMaxCapMinutes) * 1000);
    return std::min<std::int64_t>(terms.CapMinutes, capped * terms.RatePermille / 1000);
}

std::string_view TimeContractErrorCode(TimeContractError error) noexcept
{
    switch (error)
    {
    case TimeContractError::None: return {};
    case TimeContractError::MissingDeclaration: return "time.missing_declaration";
    case TimeContractError::UnknownMode: return "time.unknown_mode";
    case TimeContractError::UnknownInterrupt: return "time.unknown_interrupt";
    case TimeContractError::UnknownResolve: return "time.unknown_resolve";
    case TimeContractError::BadId: return "time.bad_id";
    case TimeContractError::CostOutOfRange: return "time.cost_out_of_range";
    case TimeContractError::InterruptNotAllowed: return "time.interrupt_not_allowed";
    case TimeContractError::ResolveNotAllowed: return "time.resolve_not_allowed";
    case TimeContractError::BeatsOutOfRange: return "time.beats_out_of_range";
    case TimeContractError::CommitCannotBeInterrupted: return "time.commit_cannot_be_interrupted";
    case TimeContractError::MissionTermsMissing: return "time.mission_terms_missing";
    case TimeContractError::MissionTermsInvalid: return "time.mission_terms_invalid";
    case TimeContractError::MissionTermsOnOtherMode: return "time.mission_terms_on_other_mode";
    case TimeContractError::InvalidEvent: return "time.invalid_event";
    case TimeContractError::InvalidState: return "time.invalid_state";
    case TimeContractError::PlanNotRoutine: return "time.plan_not_routine";
    case TimeContractError::PlanOutOfOrder: return "time.plan_out_of_order";
    case TimeContractError::PlanOverlap: return "time.plan_overlap";
    case TimeContractError::PlanOutOfSpan: return "time.plan_out_of_span";
    case TimeContractError::PlanSpanInvalid: return "time.plan_span_invalid";
    case TimeContractError::PlanNeedsPlay: return "time.plan_needs_play";
    }
    return "time.invalid_state";
}

TimeContractError ValidateDeclaration(const InteractionDeclaration& declaration)
{
    const std::size_t modeIndex = static_cast<std::size_t>(declaration.Mode);
    if (modeIndex >= ClockModeCount)
        return TimeContractError::UnknownMode;
    if (static_cast<std::uint8_t>(declaration.Interrupt) > static_cast<std::uint8_t>(InterruptPolicy::Immediate))
        return TimeContractError::UnknownInterrupt;
    if (static_cast<std::uint8_t>(declaration.Resolve) > static_cast<std::uint8_t>(ResolutionBoundary::PerBeat))
        return TimeContractError::UnknownResolve;
    if (!TimeContractIdWellFormed(declaration.Id))
        return TimeContractError::BadId;

    const ClockModeRule& rule = TimeModeRule(declaration.Mode);
    if (declaration.CostMinutes < rule.MinCostMinutes || declaration.CostMinutes > rule.MaxCostMinutes)
        return TimeContractError::CostOutOfRange;
    if ((rule.AllowedInterrupts & TimeContractBit(declaration.Interrupt)) == 0)
        return TimeContractError::InterruptNotAllowed;
    if ((rule.AllowedResolutions & TimeContractBit(declaration.Resolve)) == 0)
        return TimeContractError::ResolveNotAllowed;
    // A beat needs length: with no cost, or fewer minutes than beats, boundaries would coincide.
    if (declaration.Beats < 1 || declaration.Beats > rule.MaxBeats || (declaration.Beats > 1 && declaration.Beats > declaration.CostMinutes))
        return TimeContractError::BeatsOutOfRange;
    if (declaration.Interrupt == InterruptPolicy::AtBeat && declaration.CostMinutes == 0)
        return TimeContractError::InterruptNotAllowed;
    if (declaration.Resolve == ResolutionBoundary::OnCommit &&
        (declaration.Interrupt != InterruptPolicy::None || declaration.Beats != 1))
        return TimeContractError::CommitCannotBeInterrupted;
    if (declaration.Resolve == ResolutionBoundary::PerBeat && declaration.Beats < 2)
        return TimeContractError::BeatsOutOfRange;
    if (declaration.Mode == ClockMode::Mission)
        return TimeContractValidateMission(declaration);
    return TimeContractMissionTermsEmpty(declaration.Mission) ? TimeContractError::None : TimeContractError::MissionTermsOnOtherMode;
}

bool InteractionAbsorbsProgress(const InteractionDeclaration& declaration) noexcept
{
    const ChargeRule charge = TimeModeRule(declaration.Mode).Charge;
    return charge == ChargeRule::Never || (charge == ChargeRule::DeclaredAtClose && declaration.CostMinutes == 0);
}

std::int64_t BeatBoundary(const InteractionDeclaration& declaration, std::int32_t k) noexcept
{
    if (declaration.Beats < 1)
        return 0;
    const std::int32_t clamped = std::clamp(k, 0, declaration.Beats);
    return static_cast<std::int64_t>(declaration.CostMinutes) * clamped / declaration.Beats;
}

std::int32_t BeatsCoveredBy(const InteractionDeclaration& declaration, std::int64_t progress) noexcept
{
    if (declaration.CostMinutes <= 0 || declaration.Beats < 1 || progress < 0)
        return 0;
    if (progress >= declaration.CostMinutes)
        return declaration.Beats;
    // The largest k with floor(cost * k / beats) <= progress, that is ((progress + 1) * beats - 1) / cost.
    const std::int64_t k = ((progress + 1) * declaration.Beats - 1) / declaration.CostMinutes;
    return static_cast<std::int32_t>(std::min<std::int64_t>(k, declaration.Beats));
}
} // namespace PerfectWorld
