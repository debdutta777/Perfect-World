#include "AwakeningInputs.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace PerfectWorld
{
namespace
{
bool R91StableId(std::string_view id)
{
    return !id.empty() && id.size() <= AptitudeMaxIdLength &&
           std::all_of(id.begin(), id.end(), [](char c) {
               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
           });
}

bool R91LatentOwnerValid(const AwakeningState& state)
{
    if (!state.Seeded || !IsAptitudeLedgerValid(state.ProjectionEvidence))
        return false;
    for (std::size_t i = 0; i < AttributeCount; ++i)
    {
        if (state.Stats.Attributes[i] < 0 || state.AttributeEvidenceMilli[i] < 0)
            return false;
    }
    std::string_view previous;
    for (const SkillLevel& skill : state.Stats.Skills)
    {
        if (!R91StableId(skill.SkillId) || skill.SkillId <= previous || skill.LevelMilli < 0)
            return false;
        previous = skill.SkillId;
    }
    std::uint64_t previousInstance = 0;
    for (const std::uint64_t instance : state.AppliedActivityInstances)
    {
        if (instance <= previousInstance)
            return false;
        previousInstance = instance;
    }
    return true;
}

std::string R91ReceiptIssue(const AwakeningState& state, const DayLoopEvent& event)
{
    if (event.Instance == 0 || !R91StableId(event.ActivityId) || event.At < 0 || event.Minutes <= 0 ||
        event.Minutes > event.At)
        return "awakening_inputs.invalid_receipt";
    for (std::size_t i = 0; i < event.Credited.size(); ++i)
    {
        const SkillCredit& credit = event.Credited[i];
        if (credit.MilliXp < 0 || state.Stats.FindSkill(credit.SkillId) == nullptr)
            return "awakening_inputs.invalid_skill_credit";
        for (std::size_t j = 0; j < i; ++j)
        {
            if (event.Credited[j].SkillId == credit.SkillId)
                return "awakening_inputs.repeated_skill";
        }
    }
    for (std::size_t i = 0; i < event.Evidence.size(); ++i)
    {
        const EvidenceCredit& credit = event.Evidence[i];
        if (credit.Milli < 0 || static_cast<std::size_t>(credit.Attr) >= AttributeCount)
            return "awakening_inputs.invalid_attribute_evidence";
        for (std::size_t j = 0; j < i; ++j)
        {
            if (event.Evidence[j].Attr == credit.Attr)
                return "awakening_inputs.repeated_attribute";
        }
    }
    return {};
}

void R91CreditReceipt(AwakeningState& state, const DayLoopEvent& event)
{
    for (const SkillCredit& credit : event.Credited)
    {
        auto skill = std::lower_bound(state.Stats.Skills.begin(), state.Stats.Skills.end(), credit.SkillId,
                                     [](const SkillLevel& level, std::string_view id) { return level.SkillId < id; });
        const std::int64_t sum = static_cast<std::int64_t>(skill->LevelMilli) + credit.MilliXp;
        skill->LevelMilli = static_cast<std::int32_t>(
            std::min<std::int64_t>(sum, std::numeric_limits<std::int32_t>::max()));
    }
    for (const EvidenceCredit& credit : event.Evidence)
    {
        std::int64_t& total = state.AttributeEvidenceMilli[static_cast<std::size_t>(credit.Attr)];
        total += std::min<std::int64_t>(credit.Milli, std::numeric_limits<std::int64_t>::max() - total);
    }
    const auto position = std::lower_bound(state.AppliedActivityInstances.begin(), state.AppliedActivityInstances.end(), event.Instance);
    state.AppliedActivityInstances.insert(position, event.Instance);
}
} // namespace

AwakeningInputsSeedResult SeedAwakeningInputs(const ContentDb& content, const CharacterCreationSave& creation,
                                             std::uint64_t worldSeed)
{
    const LatentStatsResult stats = ComputeStartingStats(content, creation);
    AwakeningInputsSeedResult result;
    if (!stats.Ok())
    {
        result.Error = stats.Error;
        return result;
    }
    result.State.Seeded = true;
    result.State.WorldSeed = worldSeed;
    result.State.Stats = stats.Stats;
    return result;
}

AwakeningCreditsResult ApplyActivityCredits(AwakeningState& state, std::span<const DayLoopEvent> receipts)
{
    if (!R91LatentOwnerValid(state))
        return {"awakening_inputs.invalid_state", 0, 0};
    AwakeningState next = state;
    AwakeningCreditsResult result;
    for (const DayLoopEvent& event : receipts)
    {
        if (event.Kind != DayLoopEventKind::Completed)
            continue;
        if (std::binary_search(next.AppliedActivityInstances.begin(), next.AppliedActivityInstances.end(), event.Instance))
        {
            ++result.Replayed;
            continue;
        }
        const std::string issue = R91ReceiptIssue(next, event);
        if (!issue.empty())
            return {issue, 0, 0};
        R91CreditReceipt(next, event);
        ++result.Applied;
    }
    state = std::move(next);
    return result;
}

AwakeningInput BuildAwakeningInput(const AwakeningState& state)
{
    AwakeningInput input;
    input.Conduct = SummariseConduct(state.Conduct);
    input.Stats = state.Stats;
    input.Projection = AptitudeProject(state.ProjectionEvidence);
    input.WorldSeed = state.WorldSeed;
    return input;
}
} // namespace PerfectWorld
