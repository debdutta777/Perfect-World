#include "Awakening.h"

#include "AwakeningInternal.h"
#include "RandomStreams.h"

#include <algorithm>
#include <numeric>
#include <span>

namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, AwakeningSignalCount> AwSignalNames = {
    "study", "craft", "bodywork", "people", "order", "making", "resolve", "senses",
    "care",  "steadfastness", "restraint", "resourcefulness", "attentiveness"};

constexpr std::array<std::string_view, TattvaCount> AwTattvaIds = {"prithvi", "jala", "agni", "vayu", "akasha"};

constexpr std::array<std::string_view, PowerDimensionCount> AwCapabilityNames = {"output", "precision", "range",
                                                                                  "duration", "perception", "coordination"};

enum class AwInputKind : std::uint8_t
{
    Attribute,
    Skill,
    Facet
};

/** One input to a latent signal: an attribute, a skill (by content ID) or an aptitude facet. */
struct AwInput
{
    AwInputKind Kind;
    std::size_t Index; // Attribute or AptitudeFacet value
    std::string_view SkillId;
};

constexpr AwInput AwAttr(Attribute attribute)
{
    return {AwInputKind::Attribute, static_cast<std::size_t>(attribute), {}};
}
constexpr AwInput AwSkill(std::string_view id)
{
    return {AwInputKind::Skill, 0, id};
}
constexpr AwInput AwFacet(AptitudeFacet facet)
{
    return {AwInputKind::Facet, static_cast<std::size_t>(facet), {}};
}

constexpr std::size_t AwMaxInputs = 5;
struct AwInputList
{
    std::array<AwInput, AwMaxInputs> Inputs;
    std::size_t Count;
};

// The latent signals' inputs, in AwakeningSignal order (the documented mapping; Awakening.h point 1).
constexpr std::array<AwInputList, AwakeningLatentSignalCount> AwLatentInputs = {{
    // Study
    {{AwAttr(Attribute::Intelligence), AwSkill("skill.academics"), AwSkill("skill.computing"), AwFacet(AptitudeFacet::Analysis),
      AwFacet(AptitudeFacet::Interpretation)}, 5},
    // Craft
    {{AwSkill("skill.practical_repair"), AwSkill("skill.household_tasks"), AwFacet(AptitudeFacet::HandsOn),
      AwFacet(AptitudeFacet::FaultFinding), {}}, 4},
    // Bodywork
    {{AwAttr(Attribute::Body), AwAttr(Attribute::Agility), AwSkill("skill.fitness"), AwFacet(AptitudeFacet::Physical), {}}, 4},
    // People
    {{AwAttr(Attribute::Charisma), AwSkill("skill.communication"), AwFacet(AptitudeFacet::Teamwork), {}, {}}, 3},
    // Order
    {{AwSkill("skill.logistics"), AwSkill("skill.commerce"), AwSkill("skill.self_management"), AwFacet(AptitudeFacet::Accounts),
      AwFacet(AptitudeFacet::Scheduling)}, 5},
    // Making
    {{AwSkill("skill.creative"), AwFacet(AptitudeFacet::Creative), {}, {}, {}}, 2},
    // Resolve
    {{AwAttr(Attribute::Willpower), AwSkill("skill.self_management"), AwFacet(AptitudeFacet::Steadiness), {}, {}}, 3},
    // Senses
    {{AwAttr(Attribute::Perception), AwFacet(AptitudeFacet::FaultFinding), AwFacet(AptitudeFacet::Analysis), {}, {}}, 3},
}};

std::int32_t AwClampReading(std::int64_t value)
{
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(value, 0, AwakeningReadingMax));
}

/** The strongest state each facet shows in any path of the projection. */
std::array<AptitudeFacetState, AptitudeFacetCount> AwFacetStates(const AptitudeProjection& projection)
{
    std::array<AptitudeFacetState, AptitudeFacetCount> states{};
    for (const AptitudePathProjection& path : projection.Paths)
    {
        for (const AptitudeFacetStatement& statement : path.Facets)
        {
            const std::size_t index = static_cast<std::size_t>(statement.Facet);
            if (index < AptitudeFacetCount && statement.State > states[index])
                states[index] = statement.State;
        }
    }
    return states;
}

std::int32_t AwFacetReading(AptitudeFacetState state)
{
    // Unobserved and Observed read zero: not yet shown is unknown, never weak (PATHS_AND_PROGRESSION "Legibility").
    switch (state)
    {
    case AptitudeFacetState::Shown:
        return AwakeningFacetShownReading;
    case AptitudeFacetState::Established:
        return AwakeningFacetEstablishedReading;
    default:
        return 0;
    }
}

std::int32_t AwAttributeReading(const LatentStats& stats, std::size_t attribute)
{
    const std::int64_t growth = static_cast<std::int64_t>(stats.Attributes[attribute]) - AwakeningAttributeBaselineMilli;
    return AwClampReading(growth * AwakeningReadingMax / AwakeningAttributeGrowthCapMilli);
}

std::int32_t AwSkillReading(const LatentStats& stats, std::string_view skillId)
{
    const SkillLevel* skill = stats.FindSkill(skillId);
    return skill == nullptr ? 0 : AwClampReading(static_cast<std::int64_t>(skill->LevelMilli) * AwakeningReadingMax / AwakeningSkillCapMilliXp);
}
} // namespace

std::string_view AwakeningSignalName(AwakeningSignal signal)
{
    const std::size_t index = static_cast<std::size_t>(signal);
    return index < AwakeningSignalCount ? AwSignalNames[index] : std::string_view();
}

std::optional<AwakeningSignal> ParseAwakeningSignal(std::string_view name)
{
    for (std::size_t i = 0; i < AwakeningSignalCount; ++i)
    {
        if (AwSignalNames[i] == name)
            return static_cast<AwakeningSignal>(i);
    }
    return std::nullopt;
}

bool AwakeningSignalIsConduct(AwakeningSignal signal)
{
    const std::size_t index = static_cast<std::size_t>(signal);
    return index >= AwakeningLatentSignalCount && index < AwakeningSignalCount;
}

std::string_view AwakeningTattvaId(Tattva tattva)
{
    const std::size_t index = static_cast<std::size_t>(tattva);
    return index < TattvaCount ? AwTattvaIds[index] : std::string_view();
}

std::optional<Tattva> ParseAwakeningTattva(std::string_view id)
{
    for (std::size_t i = 0; i < TattvaCount; ++i)
    {
        if (AwTattvaIds[i] == id)
            return static_cast<Tattva>(i);
    }
    return std::nullopt;
}

std::string_view AwakeningCapabilityName(PowerDimension dimension)
{
    const std::size_t index = static_cast<std::size_t>(dimension);
    return index < PowerDimensionCount ? AwCapabilityNames[index] : std::string_view();
}

namespace AwakeningDetail
{
SignalSet ComputeSignals(const AwakeningInput& input)
{
    SignalSet set;
    const auto facetStates = AwFacetStates(input.Projection);
    for (std::size_t s = 0; s < AwakeningLatentSignalCount; ++s)
    {
        const AwInputList& list = AwLatentInputs[s];
        std::int64_t sum = 0;
        std::int32_t best = 0;
        for (std::size_t i = 0; i < list.Count; ++i)
        {
            const AwInput& source = list.Inputs[i];
            std::int32_t reading = 0;
            std::string name;
            switch (source.Kind)
            {
            case AwInputKind::Attribute:
                reading = AwAttributeReading(input.Stats, source.Index);
                name = std::string(AttributeContentId(static_cast<Attribute>(source.Index)));
                break;
            case AwInputKind::Skill:
                reading = AwSkillReading(input.Stats, source.SkillId);
                name = std::string(source.SkillId);
                break;
            case AwInputKind::Facet:
                reading = AwFacetReading(facetStates[source.Index]);
                name = "facet." + std::string(AptitudeFacetId(static_cast<AptitudeFacet>(source.Index)));
                break;
            }
            sum += reading;
            if (reading > best)
            {
                best = reading;
                set.Source[s] = name;
            }
        }
        set.Reading[s] = AwClampReading(sum / static_cast<std::int64_t>(list.Count));
    }
    // A summary resting on no test is the unpenalised case whatever it holds: no conduct is read.
    for (std::size_t c = 0; c < ConductDimensionCount && input.Conduct.TestsRecorded > 0; ++c)
    {
        const std::size_t s = AwakeningLatentSignalCount + c;
        const ConductDimensionSummary& dimension = input.Conduct.Dimensions[c];
        set.Reading[s] = AwClampReading(dimension.Milli);
        if (set.Reading[s] > 0)
        {
            set.Source[s] = "conduct." + std::string(ConductDimensionName(static_cast<ConductDimension>(c)));
            if (!dimension.Evidence.empty())
            {
                set.TestId[s] = dimension.Evidence.front().TestId;
                set.ChoiceId[s] = dimension.Evidence.front().ChoiceId;
            }
        }
    }
    return set;
}

std::uint64_t TieDraw(std::uint64_t seed, std::string_view text)
{
    return KeyedRandomU64(seed, RandomStreamId::Opportunity, std::span<const std::int64_t>{}, text);
}

std::vector<std::int32_t> Apportion(const std::vector<std::int64_t>& weights, std::int64_t total, std::uint64_t seed,
                                    std::string_view what)
{
    const std::size_t n = weights.size();
    std::vector<std::int32_t> out(n, 0);
    if (n == 0)
        return out;
    std::int64_t sum = std::accumulate(weights.begin(), weights.end(), std::int64_t{0});
    std::vector<std::int64_t> effective = weights;
    if (sum <= 0)
    {
        effective.assign(n, 1);
        sum = static_cast<std::int64_t>(n);
    }
    std::vector<std::int64_t> remainder(n, 0);
    std::int64_t given = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        const std::int64_t scaled = total * effective[i];
        out[i] = static_cast<std::int32_t>(scaled / sum);
        remainder[i] = scaled % sum;
        given += out[i];
    }
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::vector<std::uint64_t> draw(n, 0);
    for (std::size_t i = 0; i < n; ++i)
        draw[i] = TieDraw(seed, std::string(what) + ":" + std::to_string(i));
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (remainder[a] != remainder[b])
            return remainder[a] > remainder[b];
        if (draw[a] != draw[b])
            return draw[a] > draw[b];
        return a < b;
    });
    for (std::int64_t left = total - given, k = 0; left > 0 && k < static_cast<std::int64_t>(n); --left, ++k)
        ++out[order[static_cast<std::size_t>(k)]];
    return out;
}

std::vector<AwakeningCause> TopCauses(const SignalSet& signals, const SignalWeights& weights)
{
    std::vector<AwakeningCause> causes;
    for (std::size_t s = 0; s < AwakeningSignalCount; ++s)
    {
        const std::int32_t contribution = weights[s] * signals.Reading[s];
        if (contribution <= 0)
            continue;
        AwakeningCause cause;
        cause.Signal = static_cast<AwakeningSignal>(s);
        cause.Reading = signals.Reading[s];
        cause.Contribution = contribution;
        cause.Source = signals.Source[s];
        cause.TestId = signals.TestId[s];
        cause.ChoiceId = signals.ChoiceId[s];
        causes.push_back(std::move(cause));
    }
    std::stable_sort(causes.begin(), causes.end(),
                     [](const AwakeningCause& a, const AwakeningCause& b) { return a.Contribution > b.Contribution; });
    if (causes.size() > AwakeningCauseCount)
        causes.resize(AwakeningCauseCount);
    return causes;
}
} // namespace AwakeningDetail
} // namespace PerfectWorld
