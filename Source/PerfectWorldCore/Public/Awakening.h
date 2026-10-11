#pragma once

#include "AptitudeProjection.h"
#include "AwakenedProgression.h"
#include "ConductLedger.h"
#include "ContentRegistry.h"
#include "LatentStats.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The Unified Awakening: a Tattva distribution, a capability profile and one
 * derived ability, each with the evidence behind it (ROADMAP M4-02), and the
 * reader that turns the result into the AwakenedProgression starting profile
 * (ROADMAP M1-34). Sources: AWAKENING_AND_EXAM.md "The tutorial dungeon" ("a
 * profile: a distribution across the five Tattvas, an initial profession
 * projection, and the capacity to develop"; "the projection reads the sixteen
 * years; the dungeon reads the person"), AWAKENED_GRADES.md §1 and §4 (the six
 * dimensions, "the awakening cannot promote beyond F"), PATHS_AND_PROGRESSION.md
 * ("Tattva affinity changes the method, never the ceiling"), SYSTEMS_CATALOGUE.md
 * "Awakening profile and abilities" ("every Tattva is reachable by several
 * different lives; affinity never changes a ceiling; every output names its top
 * three causes").
 *
 * A pure, deterministic, integer-only function from four inputs (the conduct
 * summary, the latent stats, the aptitude projection and the world seed) and the
 * authored ability recipes (Data/Content/ability_recipe). It is standalone: not a
 * member of WorldState, the reducer, the hash or the save (a later integration).
 *
 * THE DOCUMENTS LEAVE THESE UNSPECIFIED. The choices below are this module's, all
 * named constants or tables, all tuning data:
 *
 * 1. Signals. Everything the awakening reads is first reduced to thirteen
 *    readings of 0..1000 (AwakeningSignal): eight LATENT signals from the
 *    sixteen years (study, craft, bodywork, people, order, making, resolve,
 *    senses), each the mean of its inputs (attributes as growth above the
 *    starting value, skills as accumulated XP, aptitude facets as Shown 500 and
 *    Established 1000, an Unobserved or merely Observed facet 0, so missing
 *    evidence is never weakness), and five CONDUCT signals, the dimensions of the
 *    conduct ledger (ConductLedger.h) as its summary reports them.
 * 2. Tattva distribution (milli, five values summing to 100,000). Each Tattva
 *    has a weight per signal (AwakeningResolve.cpp). A latent share and a
 *    conduct share are each the signal-weighted score plus a prior, apportioned
 *    to 1000, then mixed AwakeningLatentSharePercent : AwakeningConductSharePercent
 *    (60:40). Every Tattva keeps AwakeningTattvaFloorMilli, so none is ever shut
 *    out and the lives that reach each one are many.
 * 3. Capability (six dimensions, points on the AwakenedProgression scale). Each
 *    rating is a latent term of 0..AwakeningLatentCapabilityMaxPoints (the
 *    signal-weighted mean of the latent signals) plus a conduct term. The
 *    conduct terms are a redistribution of one fixed budget: always exactly
 *    AwakeningConductBudgetPoints across the six dimensions, each between
 *    AwakeningConductFloorPoints and floor + the remainder, evenly split when
 *    there is no conduct. So conduct decides where the dungeon's contribution
 *    goes, never how much of it there is; a student who withdrew, a student who
 *    rescued and a student absent on the day all receive the same total.
 *    Together a rating is at most AwakeningMaxRatingPoints (130), below F's
 *    ceiling of 149, and no Tattva changes that ceiling.
 * 4. Absence. A student with no conduct recorded (absent, awaiting the
 *    supervised make-up session) is not penalised: the conduct share is dropped
 *    and the sixteen years speak for the whole distribution, and the conduct
 *    terms are the even split. Absence changes where nothing is added or removed.
 * 5. The ability. Each recipe is a Tattva, either a fallback (asks for nothing)
 *    or an evidence recipe (1..3 signals, each at or above a minimum). A recipe
 *    qualifies when every requirement is met and its Tattva is among the top
 *    AwakeningAbilityTattvaRanks of the distribution; the one with the highest
 *    score (its Tattva's share in permille of 100% plus the mean reading of what
 *    it requires) is granted. When none qualifies, the fallback of the leading
 *    Tattva is granted, so every awakening grants exactly one ability.
 * 6. Tie-breaks. Ties (equal remainders, equal Tattva shares, equal recipe
 *    scores) are broken by a keyed draw from the world seed, never by list order:
 *    a pure function of (seed, a text key naming the tie), so asking in any order
 *    gives the same answer. The seed is used for nothing else.
 *
 * FAIRNESS. The inputs are the conduct summary, the latent stats, the aptitude
 * projection and the seed. Stream, household, appearance, pronouns, languages,
 * religion and names are not inputs, and the latent stats themselves are a pure
 * function of trait ranks (LatentStats.h), so none of them can reach a result.
 * Nothing here is a penalty: every term is added or redistributed, never taken.
 *
 * Evidence. Every output names its causes, at most AwakeningCauseCount (3),
 * strongest first: the signal, its reading, how much it supplied, the strongest
 * input behind it (an attribute, skill or facet, or a conduct dimension) and, for
 * conduct, the test and choice that added most. With little evidence there are
 * fewer than three; a cause is never invented.
 */

/** The thirteen readings. Names are stable text; they are in content. Ordered as the weight tables are. */
enum class AwakeningSignal : std::uint8_t
{
    // Latent: the sixteen years.
    Study,    // intelligence, academics, computing, analysis, interpretation
    Craft,    // practical repair, household tasks, hands-on work, fault finding
    Bodywork, // body, agility, fitness, physical performance
    People,   // charisma, communication, teamwork
    Order,    // logistics, commerce, self-management, accounts, scheduling
    Making,   // the creative skill, creative work
    Resolve,  // willpower, self-management, steadiness
    Senses,   // perception, fault finding, analysis
    // Conduct: the tutorial dungeon (ConductDimension, in its order).
    Care,
    Steadfastness,
    Restraint,
    Resourcefulness,
    Attentiveness,
};
constexpr std::size_t AwakeningSignalCount = 13;
constexpr std::size_t AwakeningLatentSignalCount = 8;
constexpr std::size_t AwakeningCapabilityCount = 6; // PowerDimensionCount

/** "study" ... "attentiveness". Empty for a bad value. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AwakeningSignalName(AwakeningSignal signal);
[[nodiscard]] PERFECTWORLDCORE_API std::optional<AwakeningSignal> ParseAwakeningSignal(std::string_view name);
[[nodiscard]] PERFECTWORLDCORE_API bool AwakeningSignalIsConduct(AwakeningSignal signal);
/** "prithvi", "jala", "agni", "vayu", "akasha". Empty for a bad value. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AwakeningTattvaId(Tattva tattva);
[[nodiscard]] PERFECTWORLDCORE_API std::optional<Tattva> ParseAwakeningTattva(std::string_view id);
/** "output", "precision", "range", "duration", "perception", "coordination". Empty for a bad value. */
[[nodiscard]] PERFECTWORLDCORE_API std::string_view AwakeningCapabilityName(PowerDimension dimension);

// Readings from the inputs (documented tuning).
constexpr std::int32_t AwakeningReadingMax = 1000;
/** Every character's attributes start here; growth above it is the reading (LatentStats: startMilli). */
constexpr std::int32_t AwakeningAttributeBaselineMilli = 10'000;
/** Growth of this much, or more, above the baseline reads as 1000. */
constexpr std::int32_t AwakeningAttributeGrowthCapMilli = 30'000;
/** A skill at this many milli-XP or more reads as 1000. */
constexpr std::int32_t AwakeningSkillCapMilliXp = 100'000;
constexpr std::int32_t AwakeningFacetShownReading = 500;
constexpr std::int32_t AwakeningFacetEstablishedReading = 1000;

// The Tattva distribution.
constexpr std::int32_t AwakeningTattvaTotalMilli = 100'000;
constexpr std::int32_t AwakeningTattvaFloorMilli = 8'000;
constexpr std::int32_t AwakeningLatentSharePercent = 60;
constexpr std::int32_t AwakeningConductSharePercent = 40;
/** Added to every Tattva's score before it becomes a share: thin evidence gives a gentle slope, not a sharp one. */
constexpr std::int32_t AwakeningLatentPrior = 1'000;
constexpr std::int32_t AwakeningConductPrior = 1'000;

// The capability profile.
constexpr std::int32_t AwakeningLatentCapabilityMaxPoints = 100;
constexpr std::int32_t AwakeningConductBudgetPoints = 60;
constexpr std::int32_t AwakeningConductFloorPoints = 6;
constexpr std::int32_t AwakeningCapabilityPrior = 1'000;
constexpr std::int32_t AwakeningConductRemainderPoints =
    AwakeningConductBudgetPoints - static_cast<std::int32_t>(AwakeningCapabilityCount) * AwakeningConductFloorPoints;
/** The most one conduct term can be: the floor plus the whole remainder. */
constexpr std::int32_t AwakeningConductMaxPoints = AwakeningConductFloorPoints + AwakeningConductRemainderPoints;
/** The most a starting rating can be: below F's ceiling (149 points) whatever the Tattva. */
constexpr std::int32_t AwakeningMaxRatingPoints = AwakeningLatentCapabilityMaxPoints + AwakeningConductMaxPoints;
static_assert(AwakeningMaxRatingPoints < 150, "awakening never exceeds F (AWAKENED_GRADES.md section 4)");
static_assert(AwakeningConductBudgetPoints % static_cast<std::int32_t>(AwakeningCapabilityCount) == 0,
              "the even split must be whole points");

// The ability.
constexpr std::int32_t AwakeningAbilityTattvaRanks = 2;
constexpr std::size_t AwakeningCauseCount = 3;
constexpr std::size_t AwakeningRecipeMaxRequirements = 3;
constexpr std::int32_t AwakeningAbilityRank = 1;
constexpr std::string_view AwakeningOriginTag = "awakening";

/** One thing a recipe needs: a signal at or above a minimum reading. */
struct AbilityRequirement
{
    AwakeningSignal Signal = AwakeningSignal::Study;
    std::int32_t MinReading = 1;

    friend bool operator==(const AbilityRequirement&, const AbilityRequirement&) = default;
};

/** An authored recipe (Data/Content/ability_recipe). */
struct AbilityRecipe
{
    std::string Id;
    std::string Name;
    PerfectWorld::Tattva Affinity = PerfectWorld::Tattva::Prithvi;
    /** A fallback asks for nothing; every Tattva has exactly one. */
    bool Fallback = false;
    std::vector<AbilityRequirement> Requires;
    std::string Effect;

    friend bool operator==(const AbilityRecipe&, const AbilityRecipe&) = default;
};

struct AbilityRecipeSet
{
    /** Ascending by Id. */
    std::vector<AbilityRecipe> Recipes;

    [[nodiscard]] PERFECTWORLDCORE_API const AbilityRecipe* Find(std::string_view id) const;
    /** The Tattva's fallback recipe, or nullptr. */
    [[nodiscard]] PERFECTWORLDCORE_API const AbilityRecipe* FallbackFor(Tattva tattva) const;

    friend bool operator==(const AbilityRecipeSet&, const AbilityRecipeSet&) = default;
};

struct AbilityRecipeLoad
{
    /** Empty on success; otherwise why no recipes could be read. */
    std::string Error;
    AbilityRecipeSet Set;

    [[nodiscard]] bool Ok() const { return Error.empty(); }
};

/** Reads the ability_recipe records of validated content. A refusal returns no recipes. */
[[nodiscard]] PERFECTWORLDCORE_API AbilityRecipeLoad LoadAbilityRecipes(const ContentDb& content);

/** Everything the awakening reads. */
struct AwakeningInput
{
    /** SummariseConduct(ledger); a ledger with no tests (the student was absent) is the unpenalised case. */
    ConductSummary Conduct;
    LatentStats Stats;
    AptitudeProjection Projection;
    /** Used only for the documented keyed tie-breaks. */
    std::uint64_t WorldSeed = 0;
};

/** A reason behind an output. */
struct AwakeningCause
{
    AwakeningSignal Signal = AwakeningSignal::Study;
    /** The signal's reading, 0..1000. */
    std::int32_t Reading = 0;
    /** What it supplied to this output: its weight times its reading. */
    std::int32_t Contribution = 0;
    /** The strongest input behind the signal: "attribute.intelligence", "skill.academics", "facet.analysis", "conduct.care". */
    std::string Source;
    /** Conduct signals only: the test and choice that added most to the dimension. */
    std::string TestId;
    std::string ChoiceId;

    friend bool operator==(const AwakeningCause&, const AwakeningCause&) = default;
};

/** The one ability the awakening grants (SYSTEMS_CATALOGUE: Abilities[{Id, OriginTag, Rank}]). */
struct AwakenedAbilityGrant
{
    /** Empty only when AwakeningResult::Error is set. */
    std::string RecipeId;
    std::string OriginTag;
    std::int32_t Rank = 0;
    /** True when no evidence recipe qualified and the leading Tattva's fallback was granted. */
    bool Fallback = false;
    std::int32_t Score = 0;
    /** The requirements' signals strongest first; for a fallback, the leading Tattva's causes. */
    std::vector<AwakeningCause> Causes;

    friend bool operator==(const AwakenedAbilityGrant&, const AwakenedAbilityGrant&) = default;
};

struct AwakeningResult
{
    /** Empty on success. A set of recipes without a fallback for the leading Tattva is the only refusal. */
    std::string Error;
    /** False for a student with no conduct recorded. */
    bool ConductObserved = false;
    /** The thirteen readings, in AwakeningSignal order. */
    std::array<std::int32_t, AwakeningSignalCount> Signals{};
    /** The distribution in Tattva order: sums to AwakeningTattvaTotalMilli, each at least the floor. */
    std::array<std::int32_t, TattvaCount> TattvaMilli{};
    /** The same as whole percent summing to exactly 100 (what AwakenedProfile stores). */
    std::array<std::int32_t, TattvaCount> TattvaPercent{};
    std::array<std::vector<AwakeningCause>, TattvaCount> TattvaCauses;
    /** The capability profile in PowerDimension order: Rating = LatentRating + ConductRating. */
    std::array<std::int32_t, PowerDimensionCount> Rating{};
    std::array<std::int32_t, PowerDimensionCount> LatentRating{};
    std::array<std::int32_t, PowerDimensionCount> ConductRating{};
    std::array<std::vector<AwakeningCause>, PowerDimensionCount> RatingCauses;
    AwakenedAbilityGrant Ability;

    [[nodiscard]] bool Ok() const { return Error.empty(); }

    friend bool operator==(const AwakeningResult&, const AwakeningResult&) = default;
};

/** The awakening. Pure and deterministic; see the header comment. */
[[nodiscard]] PERFECTWORLDCORE_API AwakeningResult ResolveAwakening(const AwakeningInput& input, const AbilityRecipeSet& recipes);

/**
 * The reader (ROADMAP M1-34): the AwakenedProgression starting profile for a
 * result, awakened on `day`: the affinity is the result's whole percent and the
 * six ratings its capability, both inside the F band whatever the inputs. Refuses
 * a result whose Error is set or a negative day.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::optional<AwakenedProfile> AwakenedStartingProfile(const AwakeningResult& result,
                                                                                         std::int64_t day);
} // namespace PerfectWorld
