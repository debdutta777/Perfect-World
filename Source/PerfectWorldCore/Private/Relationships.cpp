#include "Relationships.h"

#include "IdentityRegistry.h"

#include <algorithm>
#include <array>

namespace PerfectWorld
{
namespace
{
constexpr std::array<std::string_view, RelationshipStageCount> RelationshipStageNames = {
    "acquaintance", "friend", "close", "strained", "estranged", "reconciled", "family", "partner", "memorial"};

constexpr std::array<std::string_view, RelationshipActKindCount> RelationshipKindNames = {
    "help", "gift", "confide", "defend", "apologise", "share_time", "insult", "betray", "bereave"};

// Trust, Warmth, Respect, Owed (to the actor), Salience. Fixed and keyed on the kind alone.
constexpr std::array<RelationshipEffect, RelationshipActKindCount> RelationshipEffects = {{
    {3000, 4000, 2000, 3000, 600},     // help
    {1000, 4000, 0, 2000, 400},        // gift
    {6000, 3000, 1000, 0, 800},        // confide
    {5000, 3000, 4000, 4000, 800},     // defend
    {2000, 1000, 2000, 0, 600},        // apologise
    {1000, 3000, 0, 0, 300},           // share_time
    {-4000, -6000, -5000, 0, 600},     // insult
    {-15000, -8000, -10000, 0, 1000},  // betray
    {0, 3000, 0, 0, 1000},             // bereave
}};

// Words that may not appear in a remembered event type (fairness: nothing is keyed on them).
constexpr std::array<std::string_view, 10> RelationshipForbiddenWords = {
    "religion", "religions", "religious", "faith", "caste", "language", "languages", "tongue", "stream", "background"};

std::int32_t RelationshipClamp(std::int64_t value)
{
    return static_cast<std::int32_t>(std::clamp<std::int64_t>(value, RelationshipMilliMin, RelationshipMilliMax));
}

bool RelationshipKindInRange(RelationshipActKind kind)
{
    return static_cast<std::size_t>(kind) < RelationshipActKindCount;
}

// Keeps `Memories` in ascending EventSeq and at most RelationshipMaxMemories. Returns whether the candidate was
// kept and which older memory (if any) it displaced.
void RelationshipRemember(Relationship& relationship, const RelationshipMemory& candidate, RelationshipApplyResult& result)
{
    std::vector<RelationshipMemory>& memories = relationship.Memories;
    if (memories.size() < RelationshipMaxMemories)
    {
        memories.push_back(candidate);
        result.MemoryKept = true;
        return;
    }
    // The lowest effective salience at the new event's time loses; the lower Seq loses a tie. The candidate has the
    // greatest Seq of all, so it only loses when strictly lower than every held memory.
    std::size_t weakest = 0;
    std::int32_t weakestValue = RelationshipEffectiveSalience(memories[0], candidate.At);
    for (std::size_t i = 1; i < memories.size(); ++i)
    {
        const std::int32_t value = RelationshipEffectiveSalience(memories[i], candidate.At);
        if (value < weakestValue)
        {
            weakest = i;
            weakestValue = value;
        }
    }
    if (RelationshipEffectiveSalience(candidate, candidate.At) < weakestValue)
    {
        result.MemoryKept = false;
        return;
    }
    result.EvictedSeq = memories[weakest].EventSeq;
    memories.erase(memories.begin() + static_cast<std::ptrdiff_t>(weakest));
    memories.push_back(candidate);
    result.MemoryKept = true;
}
} // namespace

std::string_view RelationshipStageName(RelationshipStage stage)
{
    const std::size_t index = static_cast<std::size_t>(stage);
    return index < RelationshipStageCount ? RelationshipStageNames[index] : std::string_view();
}

std::optional<RelationshipStage> ParseRelationshipStage(std::string_view name)
{
    for (std::size_t i = 0; i < RelationshipStageCount; ++i)
    {
        if (RelationshipStageNames[i] == name)
            return static_cast<RelationshipStage>(i);
    }
    return std::nullopt;
}

std::string_view RelationshipActKindName(RelationshipActKind kind)
{
    return RelationshipKindInRange(kind) ? RelationshipKindNames[static_cast<std::size_t>(kind)] : std::string_view();
}

std::optional<RelationshipActKind> ParseRelationshipActKind(std::string_view name)
{
    for (std::size_t i = 0; i < RelationshipActKindCount; ++i)
    {
        if (RelationshipKindNames[i] == name)
            return static_cast<RelationshipActKind>(i);
    }
    return std::nullopt;
}

RelationshipEffect RelationshipActEffect(RelationshipActKind kind)
{
    return RelationshipKindInRange(kind) ? RelationshipEffects[static_cast<std::size_t>(kind)] : RelationshipEffect{};
}

bool IsRelationshipEventTypeFair(std::string_view type)
{
    if (!IsStableIdToken(type))
        return false;
    std::size_t start = 0;
    while (start <= type.size())
    {
        std::size_t end = start;
        while (end < type.size() && type[end] != '_' && type[end] != '.' && type[end] != ':' && type[end] != '-')
            ++end;
        const std::string_view word = type.substr(start, end - start);
        if (std::find(RelationshipForbiddenWords.begin(), RelationshipForbiddenWords.end(), word) != RelationshipForbiddenWords.end())
            return false;
        start = end + 1;
    }
    return true;
}

std::int32_t RelationshipEffectiveSalience(const RelationshipMemory& memory, GameInstant now)
{
    const std::int64_t base = std::clamp<std::int64_t>(memory.Salience, 0, RelationshipSalienceMax);
    const std::int64_t age = now > memory.At ? now - memory.At : 0;
    const std::int64_t halvings = age / RelationshipSalienceHalfLifeMinutes;
    if (halvings >= 11) // 1000 >> 10 is already 0
        return 0;
    const std::int64_t remainder = age % RelationshipSalienceHalfLifeMinutes;
    const std::int64_t high = base >> halvings;
    const std::int64_t low = base >> (halvings + 1);
    return static_cast<std::int32_t>(high - (high - low) * remainder / RelationshipSalienceHalfLifeMinutes);
}

std::int32_t RelationshipOwedBy(const Relationship& relationship, std::string_view person)
{
    if (person == relationship.High)
        return relationship.Owed;
    if (person == relationship.Low)
        return -relationship.Owed;
    return 0;
}

std::vector<RelationshipMemory> RelationshipMemoriesBySalience(const Relationship& relationship, GameInstant now)
{
    std::vector<RelationshipMemory> ordered = relationship.Memories;
    std::stable_sort(ordered.begin(), ordered.end(), [now](const RelationshipMemory& a, const RelationshipMemory& b) {
        const std::int32_t sa = RelationshipEffectiveSalience(a, now);
        const std::int32_t sb = RelationshipEffectiveSalience(b, now);
        return sa != sb ? sa > sb : a.EventSeq > b.EventSeq;
    });
    return ordered;
}

const Relationship* RelationshipBook::Find(std::string_view a, std::string_view b) const
{
    if (a == b)
        return nullptr;
    const bool ordered = a < b;
    const auto it = Pairs_.find(Key(std::string(ordered ? a : b), std::string(ordered ? b : a)));
    return it == Pairs_.end() ? nullptr : &it->second;
}

bool RelationshipBook::RestorePair(Relationship relationship)
{
    if (Pairs_.size() >= RelationshipMaxPairs)
        return false;
    if (!Pairs_.empty() && !(Pairs_.rbegin()->first < Key(relationship.Low, relationship.High)))
        return false;
    Key key(relationship.Low, relationship.High);
    Pairs_.emplace_hint(Pairs_.end(), std::move(key), std::move(relationship));
    return true;
}

RelationshipApplyResult RelationshipBook::Apply(const EventLog& log, const RelationshipAct& act)
{
    const EventLogEntry* entry = log.Find(act.EventSeq);
    if (entry == nullptr)
        return {RelationshipStatus::BadEvent, false, 0};
    const auto hasParty = [entry](const std::string& id) {
        return std::find(entry->Parties.begin(), entry->Parties.end(), id) != entry->Parties.end();
    };
    if (act.Actor == act.Other || !IsValidIdentityId(act.Actor) || !IsValidIdentityId(act.Other) || !hasParty(act.Actor) || !hasParty(act.Other))
        return {RelationshipStatus::BadParties, false, 0};
    if (!RelationshipKindInRange(act.Kind))
        return {RelationshipStatus::BadKind, false, 0};
    if (!IsRelationshipEventTypeFair(entry->Type))
        return {RelationshipStatus::BadEventType, false, 0};

    const bool actorIsLow = act.Actor < act.Other;
    const Key key = actorIsLow ? Key(act.Actor, act.Other) : Key(act.Other, act.Actor);
    const auto existing = Pairs_.find(key);
    const std::uint32_t bit = 1u << static_cast<unsigned>(act.Kind);
    if (existing != Pairs_.end())
    {
        const Relationship& held = existing->second;
        if (act.EventSeq <= held.LastSeq)
            return {RelationshipStatus::OutOfOrder, false, 0};
        if (held.Stage == RelationshipStage::Memorial)
            return {RelationshipStatus::Frozen, false, 0};
    }
    else if (Pairs_.size() >= RelationshipMaxPairs)
    {
        return {RelationshipStatus::Full, false, 0};
    }

    Relationship& relationship = existing != Pairs_.end() ? existing->second : Pairs_[key];
    if (existing == Pairs_.end())
    {
        relationship.Low = key.first;
        relationship.High = key.second;
    }
    const RelationshipEffect effect = RelationshipActEffect(act.Kind);
    const bool repeat = (relationship.FirstActs & bit) != 0;
    if (!repeat)
    {
        relationship.Trust = RelationshipClamp(static_cast<std::int64_t>(relationship.Trust) + effect.Trust);
        relationship.Warmth = RelationshipClamp(static_cast<std::int64_t>(relationship.Warmth) + effect.Warmth);
        relationship.Respect = RelationshipClamp(static_cast<std::int64_t>(relationship.Respect) + effect.Respect);
        // Owed is stored as "High owes Low": the other person owes the actor, so it rises when the actor is Low.
        relationship.Owed = RelationshipClamp(static_cast<std::int64_t>(relationship.Owed) + (actorIsLow ? effect.Owed : -effect.Owed));
        relationship.FirstActs |= bit;
        if (act.Kind == RelationshipActKind::Bereave)
            relationship.Stage = RelationshipStage::Memorial;
    }
    relationship.LastSeq = act.EventSeq;

    RelationshipMemory memory;
    memory.EventSeq = act.EventSeq;
    memory.At = entry->At;
    memory.Type = entry->Type;
    memory.Kind = act.Kind;
    memory.Salience = repeat ? effect.Salience / RelationshipRepeatSalienceDivisor : effect.Salience;
    RelationshipApplyResult result;
    result.Status = repeat ? RelationshipStatus::Repeat : RelationshipStatus::Applied;
    RelationshipRemember(relationship, memory, result);
    return result;
}
} // namespace PerfectWorld
