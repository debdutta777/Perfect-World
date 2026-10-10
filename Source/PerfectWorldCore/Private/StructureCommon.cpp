#include "StructureInternal.h"

#include "RandomStreams.h"

#include <algorithm>
#include <array>

// What the explain, investigate and coordinate structures share (ROADMAP M2-09): the
// error codes, case selection, the per-family records with mastery and routine, the
// family header of a content record, and the strict text helpers. The conventions are
// documented in Public/StructureCommon.h.
namespace PerfectWorld
{
std::string_view StructureErrorCode(StructureError error) noexcept
{
    switch (error)
    {
    case StructureError::None:
        return {};
    case StructureError::UnknownFamily:
        return "structure.unknown_family";
    case StructureError::UnknownCase:
        return "structure.unknown_case";
    case StructureError::Busy:
        return "structure.busy";
    case StructureError::NotActive:
        return "structure.not_active";
    case StructureError::WrongBeat:
        return "structure.wrong_beat";
    case StructureError::BadChoice:
        return "structure.bad_choice";
    case StructureError::UnknownItem:
        return "structure.unknown_item";
    case StructureError::AlreadyDone:
        return "structure.already_done";
    case StructureError::NotNeeded:
        return "structure.not_needed";
    case StructureError::UnitsShort:
        return "structure.units_short";
    case StructureError::NotSecure:
        return "structure.not_secure";
    case StructureError::InvalidSkill:
        return "structure.invalid_skill";
    case StructureError::InvalidState:
        return "structure.invalid_state";
    }
    return "structure.invalid_state";
}

StructureSelection StructureSelectCase(const std::vector<std::string>& caseIds, const StructureFamilyRecord* record,
                                       std::string_view familyId, std::uint64_t worldSeed, GameInstant at)
{
    StructureSelection selection;
    if (caseIds.empty())
    {
        selection.Error = StructureError::UnknownCase;
        return selection;
    }
    const auto contains = [](const std::vector<std::string>& ids, const std::string& id) {
        return std::find(ids.begin(), ids.end(), id) != ids.end();
    };

    // The last cases shown are not offered again: up to StructureRecentKept, but always leaving one choice.
    std::vector<std::string> excluded;
    if (record != nullptr)
    {
        const std::size_t window = std::min(StructureRecentKept, caseIds.size() - 1);
        const std::size_t keep = std::min(window, record->Recent.size());
        excluded.assign(record->Recent.end() - static_cast<std::ptrdiff_t>(keep), record->Recent.end());
    }
    std::vector<const std::string*> eligible;
    for (const std::string& id : caseIds)
    {
        if (!contains(excluded, id))
            eligible.push_back(&id);
    }
    // A case the player has not met comes before one they have.
    std::vector<const std::string*> unseen;
    if (record != nullptr)
    {
        for (const std::string* id : eligible)
        {
            if (!std::binary_search(record->Seen.begin(), record->Seen.end(), *id))
                unseen.push_back(id);
        }
    }
    else
        unseen = eligible;
    selection.FromUnseen = !unseen.empty();
    const std::vector<const std::string*>& pool = selection.FromUnseen ? unseen : eligible;
    selection.Candidates = pool.size();
    const std::array<std::int64_t, 2> key{at, record != nullptr ? static_cast<std::int64_t>(record->Exposures) : 0};
    selection.CaseId = *pool[KeyedRandomBelow(worldSeed, RandomStreamId::Opportunity, pool.size(), key, familyId)];
    return selection;
}

namespace
{
bool StructureIdsSortedUnique(const std::vector<std::string>& ids, std::size_t limit)
{
    if (ids.size() > limit)
        return false;
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        if (!ModelIdWellFormed(ids[i]) || (i > 0 && !(ids[i - 1] < ids[i])))
            return false;
    }
    return true;
}

bool StructureFamilyRecordValid(const StructureFamilyRecord& record)
{
    if (!ModelIdWellFormed(record.FamilyId) || !StructureIdsSortedUnique(record.Seen, StructureMaxSeen) ||
        record.Recent.size() > StructureRecentKept || record.SoundExposures > record.Exposures)
        return false;
    for (const std::string& id : record.Recent)
    {
        if (!std::binary_search(record.Seen.begin(), record.Seen.end(), id))
            return false;
    }
    // Mastery is awarded when an exposure finishes, so the counts may be ahead of the flag but never behind it.
    return !record.Secure || (record.SoundExposures >= StructureSecureSoundExposures && record.Seen.size() >= StructureSecureDistinctCases);
}
} // namespace

bool IsStructureFamilyListValid(const std::vector<StructureFamilyRecord>& families)
{
    if (families.size() > StructureMaxFamilies)
        return false;
    for (std::size_t i = 0; i < families.size(); ++i)
    {
        if (!StructureFamilyRecordValid(families[i]) || (i > 0 && !(families[i - 1].FamilyId < families[i].FamilyId)))
            return false;
    }
    return true;
}
} // namespace PerfectWorld

namespace PerfectWorld::StructureInternal
{
std::string LoadFamilyHeader(const ContentRecord& record, StructureFamilyHeader& header)
{
    header.Id = record.Id;
    header.Name = JsonString(&record.Data, "name");
    header.SkillId = JsonString(&record.Data, "skill");
    if (!JsonInt32(&record.Data, "minCases", header.MinCases) || header.MinCases < 1)
        return "minCases must be a positive whole number";
    const std::vector<JsonValue>& methods = JsonItems(&record.Data, "methods");
    if (methods.empty())
        return "a family needs at least one method";
    for (const JsonValue& entry : methods)
    {
        StructureMethod method;
        method.Id = JsonString(&entry, "id");
        method.Name = JsonString(&entry, "name");
        if (!ModelIdWellFormed(method.Id) || !JsonInt32(&entry, "knownAtMilli", method.KnownAtMilli) || method.KnownAtMilli < 0)
            return "a method needs a well-formed id and a non-negative knownAtMilli";
        if (std::any_of(header.Methods.begin(), header.Methods.end(), [&](const StructureMethod& m) { return m.Id == method.Id; }))
            return "the family names the method " + method.Id + " twice";
        header.Methods.push_back(std::move(method));
    }
    return {};
}

std::string LoadCaseBase(const ContentRecord& record, const StructureFamilyHeader& family, StructureCaseBase& base)
{
    base.Id = record.Id;
    base.FamilyId = family.Id;
    base.Name = JsonString(&record.Data, "name");
    base.Subject = JsonString(&record.Data, "subject");
    base.Brief = JsonString(&record.Data, "brief");
    base.MethodId = JsonString(&record.Data, "method");
    const auto method = std::find_if(family.Methods.begin(), family.Methods.end(), [&](const StructureMethod& m) { return m.Id == base.MethodId; });
    if (method == family.Methods.end())
        return "the method " + base.MethodId + " is not one of the family's methods";
    base.MethodKnownAtMilli = method->KnownAtMilli;
    return {};
}

// ---- The family records -----------------------------------------------------------

StructureFamilyRecord* FindFamilyMutable(std::vector<StructureFamilyRecord>& families, std::string_view familyId)
{
    const auto it = std::find_if(families.begin(), families.end(), [&](const StructureFamilyRecord& r) { return r.FamilyId == familyId; });
    return it == families.end() ? nullptr : &*it;
}

StructureError NoteCaseSeen(std::vector<StructureFamilyRecord>& families, std::string_view familyId, std::string_view caseId)
{
    auto it = std::lower_bound(families.begin(), families.end(), familyId,
                               [](const StructureFamilyRecord& record, std::string_view key) { return record.FamilyId < key; });
    if (it == families.end() || it->FamilyId != familyId)
    {
        if (families.size() >= StructureMaxFamilies)
            return StructureError::InvalidState;
        StructureFamilyRecord fresh;
        fresh.FamilyId = std::string(familyId);
        it = families.insert(it, std::move(fresh));
    }
    StructureFamilyRecord& record = *it;
    const auto seen = std::lower_bound(record.Seen.begin(), record.Seen.end(), caseId);
    if (seen == record.Seen.end() || *seen != caseId)
    {
        if (record.Seen.size() >= StructureMaxSeen)
            return StructureError::InvalidState;
        record.Seen.insert(seen, std::string(caseId));
    }
    record.Recent.emplace_back(caseId);
    if (record.Recent.size() > StructureRecentKept)
        record.Recent.erase(record.Recent.begin());
    return StructureError::None;
}

void NoteFinish(StructureFamilyRecord& record, bool foreground, StructureGrade grade, StructureOutcome& outcome)
{
    ++record.Exposures;
    if (foreground && grade == StructureGrade::Sound)
        ++record.SoundExposures;
    const bool secure = record.SoundExposures >= StructureSecureSoundExposures && record.Seen.size() >= StructureSecureDistinctCases;
    outcome.BecameSecure = secure && !record.Secure;
    record.Secure = record.Secure || secure;
}

StructureError ResolveRoutineOnRecords(std::vector<StructureFamilyRecord>& families, std::string_view familyId, StructureOutcome& outcome)
{
    StructureFamilyRecord* record = FindFamilyMutable(families, familyId);
    if (record == nullptr || !record->Secure)
        return StructureError::NotSecure;
    if (!IsStructureFamilyListValid(families) || record->Exposures == std::numeric_limits<std::uint32_t>::max())
        return StructureError::InvalidState;
    outcome = StructureOutcome{};
    outcome.FamilyId = std::string(familyId);
    outcome.Foreground = false;
    NoteFinish(*record, false, StructureGrade::Sound, outcome);
    return StructureError::None;
}

StructureGrade GradeFor(std::uint8_t failures, std::uint8_t unsoundMask)
{
    if ((failures & unsoundMask) != 0)
        return StructureGrade::Unsound;
    return failures == 0 ? StructureGrade::Sound : StructureGrade::Partial;
}

// ---- Text -------------------------------------------------------------------------

std::string Join(const std::vector<std::string>& ids, char separator)
{
    if (ids.empty())
        return std::string(TextNone);
    std::string text;
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        if (i > 0)
            text += separator;
        text += ids[i];
    }
    return text;
}

std::vector<std::string_view> Split(std::string_view text, char separator)
{
    std::vector<std::string_view> parts;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t at = text.find(separator, start);
        if (at == std::string_view::npos)
        {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, at - start));
        start = at + 1;
    }
}

bool LineReader::Next(std::string_view& line)
{
    const std::size_t newline = Text.find('\n', Position);
    if (newline == std::string_view::npos)
        return false;
    line = Text.substr(Position, newline - Position);
    Position = newline + 1;
    return true;
}

bool LineReader::Pair(std::string_view key, std::string_view& value)
{
    std::string_view line;
    if (!Next(line) || line.size() <= key.size() || line.substr(0, key.size()) != key || line[key.size()] != '=')
        return false;
    value = line.substr(key.size() + 1);
    return true;
}

bool ParseFlag(std::string_view text, bool& out)
{
    if (text != "0" && text != "1")
        return false;
    out = text == "1";
    return true;
}

bool ParseIds(std::string_view text, std::vector<std::string>& out, char separator)
{
    out.clear();
    if (text == TextNone)
        return true;
    for (const std::string_view part : Split(text, separator))
    {
        if (!ModelIdWellFormed(part))
            return false;
        out.emplace_back(part);
    }
    return true;
}

void AppendFamilies(std::string& text, const std::vector<StructureFamilyRecord>& families)
{
    text += "families=" + std::to_string(families.size()) + "\n";
    for (const StructureFamilyRecord& record : families)
    {
        text += "family=" + record.FamilyId + "," + std::to_string(record.Exposures) + "," + std::to_string(record.SoundExposures) + "," +
                (record.Secure ? "1" : "0") + "\n";
        text += "seen=" + Join(record.Seen) + "\n";
        text += "recent=" + Join(record.Recent) + "\n";
    }
}

bool ReadFamilies(LineReader& reader, std::vector<StructureFamilyRecord>& families)
{
    std::string_view value;
    std::size_t count = 0;
    if (!reader.Pair("families", value) || !ParseInt(value, count) || count > StructureMaxFamilies)
        return false;
    families.clear();
    for (std::size_t i = 0; i < count; ++i)
    {
        StructureFamilyRecord record;
        if (!reader.Pair("family", value))
            return false;
        const std::vector<std::string_view> parts = Split(value);
        if (parts.size() != 4 || !ModelIdWellFormed(parts[0]))
            return false;
        record.FamilyId = std::string(parts[0]);
        if (!ParseInt(parts[1], record.Exposures) || !ParseInt(parts[2], record.SoundExposures) || !ParseFlag(parts[3], record.Secure))
            return false;
        if (!reader.Pair("seen", value) || !ParseIds(value, record.Seen) || !reader.Pair("recent", value) || !ParseIds(value, record.Recent))
            return false;
        families.push_back(std::move(record));
    }
    return true;
}

bool ExposureMatchesRecent(const std::vector<StructureFamilyRecord>& families, const StructureExposureBase& exposure)
{
    const auto it = std::lower_bound(families.begin(), families.end(), exposure.FamilyId,
                                     [](const StructureFamilyRecord& record, std::string_view key) { return record.FamilyId < key; });
    return it != families.end() && it->FamilyId == exposure.FamilyId && !it->Recent.empty() && it->Recent.back() == exposure.CaseId;
}

bool BaseExposureValid(const std::vector<StructureFamilyRecord>& families, const StructureExposureBase& exposure)
{
    return ModelIdWellFormed(exposure.FamilyId) && ModelIdWellFormed(exposure.CaseId) && ExposureMatchesRecent(families, exposure) &&
           exposure.UnitsTotal >= 1 && exposure.UnitsTotal <= StructureWorkUnits && exposure.UnitsLeft >= 0 &&
           exposure.UnitsLeft <= exposure.UnitsTotal;
}
} // namespace PerfectWorld::StructureInternal
