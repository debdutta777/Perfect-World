#include "PracticeWindow.h"

#include <algorithm>
#include <charconv>

// Practice limits (ROADMAP M2-04). The rules and their sources are in the header;
// this file holds the arithmetic and the strict text form.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view PracticeTextHeader = "pw.practice/1";
constexpr std::string_view PracticeTextEnd = "end";

bool PracticeIdWellFormed(std::string_view id)
{
    if (id.empty() || id.size() > PracticeMaxIdLength)
        return false;
    for (const char c : id)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
        if (!ok)
            return false;
    }
    return true;
}

const PracticeFamilyRecord* PracticeFindFamily(const PracticeState& state, std::string_view id)
{
    const auto it = std::lower_bound(state.Families.begin(), state.Families.end(), id,
                                     [](const PracticeFamilyRecord& r, std::string_view key) { return r.FamilyId < key; });
    return it != state.Families.end() && it->FamilyId == id ? &*it : nullptr;
}

const PracticeSkillRecord* PracticeFindSkill(const PracticeState& state, std::string_view id)
{
    const auto it = std::lower_bound(state.Skills.begin(), state.Skills.end(), id,
                                     [](const PracticeSkillRecord& r, std::string_view key) { return r.SkillId < key; });
    return it != state.Skills.end() && it->SkillId == id ? &*it : nullptr;
}

/** Sessions among the kept ones that lie in the 24 hours before `now`. */
std::size_t PracticeCountInWindow(const std::vector<GameInstant>& recent, GameInstant now)
{
    std::size_t count = 0;
    for (const GameInstant at : recent)
    {
        if (now - at < PracticeWindowMinutes)
            ++count;
    }
    return count;
}

std::int32_t PracticeBudgetLeft(const PracticeSkillRecord* record, std::int64_t day)
{
    const std::int32_t used = record != nullptr && record->BudgetDay == day ? record->BudgetUsedMilli : 0;
    return PracticeSkillDailyBudgetMilli - used;
}

/** True when `days` (ascending) holds enough distinct days in the seven ending with `day`. */
bool PracticeDaysConsolidate(const std::vector<std::int64_t>& days, std::int64_t day)
{
    std::size_t inSpan = 0;
    for (const std::int64_t d : days)
    {
        if (d <= day && day - d < PracticeConsolidationSpanDays)
            ++inSpan;
    }
    return inSpan >= PracticeConsolidationDays;
}

bool PracticeSessionValid(const PracticeSession& session)
{
    if (!PracticeIdWellFormed(session.FamilyId) || session.Base.empty() || session.Base.size() > PracticeMaxSkills)
        return false;
    for (std::size_t i = 0; i < session.Base.size(); ++i)
    {
        const SkillCredit& credit = session.Base[i];
        if (!PracticeIdWellFormed(credit.SkillId) || credit.MilliXp <= 0 || credit.MilliXp > PracticeMaxBaseMilli)
            return false;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (session.Base[j].SkillId == credit.SkillId)
                return false;
        }
    }
    return true;
}

PracticeForecast PracticeRefuse(PracticeError error)
{
    PracticeForecast forecast;
    forecast.Error = error;
    return forecast;
}

PracticeForecast PracticeCompute(const PracticeState& state, const PracticeSession& session)
{
    if (!IsPracticeStateValid(state))
        return PracticeRefuse(PracticeError::InvalidState);
    if (!PracticeSessionValid(session))
        return PracticeRefuse(PracticeError::InvalidSession);
    if (session.At < state.LastAt)
        return PracticeRefuse(PracticeError::InvalidTime);

    const PracticeFamilyRecord* family = PracticeFindFamily(state, session.FamilyId);
    std::size_t newSkills = 0;
    for (const SkillCredit& base : session.Base)
    {
        if (PracticeFindSkill(state, base.SkillId) == nullptr)
            ++newSkills;
    }
    if ((family == nullptr && state.Families.size() >= PracticeMaxFamilies) ||
        state.Skills.size() + newSkills > PracticeMaxSkills)
        return PracticeRefuse(PracticeError::TooMany);

    PracticeForecast forecast;
    forecast.Tier = PracticeTierForCount(family != nullptr ? PracticeCountInWindow(family->Recent, session.At) : 0);
    const std::int32_t permille = PracticeTierPermille(forecast.Tier);
    const bool usefulTier = forecast.Tier == PracticeTier::Full || forecast.Tier == PracticeTier::Half;
    const std::int64_t day = GameClock::DayNumber(session.At);

    bool anyCredit = false;
    for (const SkillCredit& base : session.Base)
    {
        const PracticeSkillRecord* skill = PracticeFindSkill(state, base.SkillId);
        const std::int32_t byTier = static_cast<std::int32_t>(static_cast<std::int64_t>(base.MilliXp) * permille / 1000);
        const std::int32_t credit = std::min(byTier, PracticeBudgetLeft(skill, day));
        forecast.Credit.push_back({base.SkillId, credit});
        forecast.BudgetLimited = forecast.BudgetLimited || credit < byTier;
        anyCredit = anyCredit || credit > 0;

        if (usefulTier && credit > 0)
        {
            std::vector<std::int64_t> days = skill != nullptr ? skill->UsefulDays : std::vector<std::int64_t>{};
            const bool before = PracticeDaysConsolidate(days, day);
            if (days.empty() || days.back() != day)
                days.push_back(day);
            if (!before && PracticeDaysConsolidate(days, day))
                forecast.Consolidates.push_back(base.SkillId);
        }
    }

    if (!anyCredit || forecast.Tier == PracticeTier::Tenth || forecast.Tier == PracticeTier::None)
        forecast.Label = PracticeLabel::LittleNewLearning;
    else
        forecast.Label = forecast.Tier == PracticeTier::Full ? PracticeLabel::UsefulPractice : PracticeLabel::Consolidation;
    return forecast;
}

/** The next line without its newline. False when no newline remains, which is what a truncated text looks like. */
bool PracticeNextLine(std::string_view text, std::size_t& position, std::string_view& line)
{
    const std::size_t newline = text.find('\n', position);
    if (newline == std::string_view::npos)
        return false;
    line = text.substr(position, newline - position);
    position = newline + 1;
    return true;
}

bool PracticeKeyValue(std::string_view line, std::string_view key, std::string_view& value)
{
    if (line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

/** Canonical decimal only: no '+', no leading zeros, no "-0", nothing trailing. */
template <typename Int>
bool PracticeParseInt(std::string_view text, Int& out)
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

/** Splits on ',' keeping empty fields, so "a,,b" has three fields. */
std::vector<std::string_view> PracticeSplit(std::string_view text)
{
    std::vector<std::string_view> fields;
    std::size_t start = 0;
    for (;;)
    {
        const std::size_t comma = text.find(',', start);
        if (comma == std::string_view::npos)
        {
            fields.push_back(text.substr(start));
            return fields;
        }
        fields.push_back(text.substr(start, comma - start));
        start = comma + 1;
    }
}

PracticeParse PracticeRefuseText(std::string message)
{
    PracticeParse result;
    result.Error = std::move(message);
    return result;
}
} // namespace

PracticeTier PracticeTierForCount(std::size_t sessionsInWindow) noexcept
{
    switch (sessionsInWindow)
    {
    case 0:
        return PracticeTier::Full;
    case 1:
        return PracticeTier::Half;
    case 2:
        return PracticeTier::Tenth;
    default:
        return PracticeTier::None;
    }
}

std::int32_t PracticeTierPermille(PracticeTier tier) noexcept
{
    switch (tier)
    {
    case PracticeTier::Full:
        return PracticeFullPermille;
    case PracticeTier::Half:
        return PracticeHalfPermille;
    case PracticeTier::Tenth:
        return PracticeTenthPermille;
    case PracticeTier::None:
        break;
    }
    return 0;
}

std::string_view PracticeLabelText(PracticeLabel label) noexcept
{
    switch (label)
    {
    case PracticeLabel::UsefulPractice:
        return "Useful practice";
    case PracticeLabel::Consolidation:
        return "Consolidation";
    case PracticeLabel::LittleNewLearning:
        break;
    }
    return "Little new learning";
}

std::string_view PracticeTierName(PracticeTier tier) noexcept
{
    switch (tier)
    {
    case PracticeTier::Full:
        return "full";
    case PracticeTier::Half:
        return "half";
    case PracticeTier::Tenth:
        return "tenth";
    case PracticeTier::None:
        break;
    }
    return "none";
}

std::string_view PracticeErrorCode(PracticeError error) noexcept
{
    switch (error)
    {
    case PracticeError::None:
        return {};
    case PracticeError::InvalidState:
        return "practice.invalid_state";
    case PracticeError::InvalidSession:
        return "practice.invalid_session";
    case PracticeError::InvalidTime:
        return "practice.invalid_time";
    case PracticeError::TooMany:
        break;
    }
    return "practice.too_many";
}

bool IsPracticeStateValid(const PracticeState& state)
{
    if (state.LastAt < 0 || state.Families.size() > PracticeMaxFamilies || state.Skills.size() > PracticeMaxSkills)
        return false;
    const std::int64_t lastDay = GameClock::DayNumber(state.LastAt);
    for (std::size_t i = 0; i < state.Families.size(); ++i)
    {
        const PracticeFamilyRecord& family = state.Families[i];
        if (!PracticeIdWellFormed(family.FamilyId) || (i > 0 && !(state.Families[i - 1].FamilyId < family.FamilyId)))
            return false;
        if (family.Recent.empty() || family.Recent.size() > PracticeKeptSessions)
            return false;
        for (std::size_t j = 0; j < family.Recent.size(); ++j)
        {
            if (family.Recent[j] < 0 || family.Recent[j] > state.LastAt || (j > 0 && family.Recent[j] < family.Recent[j - 1]))
                return false;
        }
    }
    for (std::size_t i = 0; i < state.Skills.size(); ++i)
    {
        const PracticeSkillRecord& skill = state.Skills[i];
        if (!PracticeIdWellFormed(skill.SkillId) || (i > 0 && !(state.Skills[i - 1].SkillId < skill.SkillId)))
            return false;
        if (skill.BudgetDay < 0 || skill.BudgetDay > lastDay || skill.BudgetUsedMilli < 0 ||
            skill.BudgetUsedMilli > PracticeSkillDailyBudgetMilli || skill.UsefulDays.size() > PracticeKeptSessions)
            return false;
        for (std::size_t j = 0; j < skill.UsefulDays.size(); ++j)
        {
            if (skill.UsefulDays[j] < 0 || skill.UsefulDays[j] > lastDay || (j > 0 && skill.UsefulDays[j] <= skill.UsefulDays[j - 1]))
                return false;
        }
    }
    return true;
}

std::size_t PracticeSessionsInWindow(const PracticeState& state, std::string_view familyId, GameInstant now)
{
    const PracticeFamilyRecord* family = PracticeFindFamily(state, familyId);
    return family != nullptr ? PracticeCountInWindow(family->Recent, now) : 0;
}

std::int32_t PracticeBudgetRemaining(const PracticeState& state, std::string_view skillId, GameInstant now)
{
    return PracticeBudgetLeft(PracticeFindSkill(state, skillId), GameClock::DayNumber(now));
}

bool PracticeIsConsolidated(const PracticeState& state, std::string_view skillId, GameInstant now)
{
    const PracticeSkillRecord* skill = PracticeFindSkill(state, skillId);
    return skill != nullptr && PracticeDaysConsolidate(skill->UsefulDays, GameClock::DayNumber(now));
}

PracticeForecast ForecastPractice(const PracticeState& state, const PracticeSession& session)
{
    return PracticeCompute(state, session);
}

PracticeForecast RecordPractice(PracticeState& state, const PracticeSession& session)
{
    PracticeForecast forecast = PracticeCompute(state, session);
    if (!forecast.Ok())
        return forecast;

    auto family = std::lower_bound(state.Families.begin(), state.Families.end(), std::string_view(session.FamilyId),
                                   [](const PracticeFamilyRecord& r, std::string_view key) { return r.FamilyId < key; });
    if (family == state.Families.end() || family->FamilyId != session.FamilyId)
        family = state.Families.insert(family, PracticeFamilyRecord{session.FamilyId, {}});
    family->Recent.push_back(session.At);
    if (family->Recent.size() > PracticeKeptSessions)
        family->Recent.erase(family->Recent.begin());

    const std::int64_t day = GameClock::DayNumber(session.At);
    const bool usefulTier = forecast.Tier == PracticeTier::Full || forecast.Tier == PracticeTier::Half;
    for (const SkillCredit& credit : forecast.Credit)
    {
        auto skill = std::lower_bound(state.Skills.begin(), state.Skills.end(), std::string_view(credit.SkillId),
                                      [](const PracticeSkillRecord& r, std::string_view key) { return r.SkillId < key; });
        if (skill == state.Skills.end() || skill->SkillId != credit.SkillId)
            skill = state.Skills.insert(skill, PracticeSkillRecord{credit.SkillId, day, 0, {}});
        if (skill->BudgetDay != day)
        {
            skill->BudgetDay = day;
            skill->BudgetUsedMilli = 0;
        }
        skill->BudgetUsedMilli += credit.MilliXp;
        if (usefulTier && credit.MilliXp > 0 && (skill->UsefulDays.empty() || skill->UsefulDays.back() != day))
        {
            skill->UsefulDays.push_back(day);
            if (skill->UsefulDays.size() > PracticeKeptSessions)
                skill->UsefulDays.erase(skill->UsefulDays.begin());
        }
    }
    state.LastAt = session.At;
    return forecast;
}

std::string SerialisePractice(const PracticeState& state)
{
    if (!IsPracticeStateValid(state))
        return {};
    std::string text(PracticeTextHeader);
    text += "\nlast_at=" + std::to_string(state.LastAt);
    text += "\nfamilies=" + std::to_string(state.Families.size());
    for (const PracticeFamilyRecord& family : state.Families)
    {
        text += "\nfamily=" + family.FamilyId;
        for (const GameInstant at : family.Recent)
            text += "," + std::to_string(at);
    }
    text += "\nskills=" + std::to_string(state.Skills.size());
    for (const PracticeSkillRecord& skill : state.Skills)
    {
        text += "\nskill=" + skill.SkillId + "," + std::to_string(skill.BudgetDay) + "," + std::to_string(skill.BudgetUsedMilli);
        for (const std::int64_t day : skill.UsefulDays)
            text += "," + std::to_string(day);
    }
    text += "\n";
    text += PracticeTextEnd;
    text += "\n";
    return text;
}

PracticeParse DeserialisePractice(std::string_view text)
{
    std::size_t position = 0;
    std::string_view line;
    std::string_view value;
    if (!PracticeNextLine(text, position, line) || line != PracticeTextHeader)
        return PracticeRefuseText("The practice text has no recognised header.");

    PracticeState state;
    if (!PracticeNextLine(text, position, line) || !PracticeKeyValue(line, "last_at", value) ||
        !PracticeParseInt(value, state.LastAt))
        return PracticeRefuseText("The practice text has no valid last_at line.");

    std::size_t familyCount = 0;
    if (!PracticeNextLine(text, position, line) || !PracticeKeyValue(line, "families", value) ||
        !PracticeParseInt(value, familyCount) || familyCount > PracticeMaxFamilies)
        return PracticeRefuseText("The practice text has no valid families line.");
    for (std::size_t i = 0; i < familyCount; ++i)
    {
        if (!PracticeNextLine(text, position, line) || !PracticeKeyValue(line, "family", value))
            return PracticeRefuseText("The practice text is truncated or has a bad family line.");
        const std::vector<std::string_view> fields = PracticeSplit(value);
        if (fields.size() < 2 || fields.size() > 1 + PracticeKeptSessions)
            return PracticeRefuseText("A practice family line has the wrong number of fields.");
        PracticeFamilyRecord family;
        family.FamilyId = std::string(fields[0]);
        for (std::size_t j = 1; j < fields.size(); ++j)
        {
            GameInstant at = 0;
            if (!PracticeParseInt(fields[j], at))
                return PracticeRefuseText("A practice family line has a malformed instant.");
            family.Recent.push_back(at);
        }
        state.Families.push_back(std::move(family));
    }

    std::size_t skillCount = 0;
    if (!PracticeNextLine(text, position, line) || !PracticeKeyValue(line, "skills", value) ||
        !PracticeParseInt(value, skillCount) || skillCount > PracticeMaxSkills)
        return PracticeRefuseText("The practice text has no valid skills line.");
    for (std::size_t i = 0; i < skillCount; ++i)
    {
        if (!PracticeNextLine(text, position, line) || !PracticeKeyValue(line, "skill", value))
            return PracticeRefuseText("The practice text is truncated or has a bad skill line.");
        const std::vector<std::string_view> fields = PracticeSplit(value);
        if (fields.size() < 3 || fields.size() > 3 + PracticeKeptSessions)
            return PracticeRefuseText("A practice skill line has the wrong number of fields.");
        PracticeSkillRecord skill;
        skill.SkillId = std::string(fields[0]);
        if (!PracticeParseInt(fields[1], skill.BudgetDay) || !PracticeParseInt(fields[2], skill.BudgetUsedMilli))
            return PracticeRefuseText("A practice skill line has a malformed budget.");
        for (std::size_t j = 3; j < fields.size(); ++j)
        {
            std::int64_t day = 0;
            if (!PracticeParseInt(fields[j], day))
                return PracticeRefuseText("A practice skill line has a malformed day.");
            skill.UsefulDays.push_back(day);
        }
        state.Skills.push_back(std::move(skill));
    }

    if (!PracticeNextLine(text, position, line) || line != PracticeTextEnd)
        return PracticeRefuseText("The practice text is truncated or has no end line.");
    if (position != text.size())
        return PracticeRefuseText("The practice text has data after the end line.");
    if (!IsPracticeStateValid(state))
        return PracticeRefuseText("The practice text describes an inconsistent state.");

    PracticeParse result;
    result.State = std::move(state);
    return result;
}
} // namespace PerfectWorld
