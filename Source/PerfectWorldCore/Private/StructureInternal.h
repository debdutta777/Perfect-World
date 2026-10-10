#pragma once

#include "Json.h"
#include "StructureCommon.h"

#include <charconv>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

// Helpers shared by the explain, investigate and coordinate structures' sources
// (ROADMAP M2-09): reading content, keeping the per-family records, and the strict
// text form every structure's save uses. Private to the core library.
namespace PerfectWorld::StructureInternal
{
// ---- Reading content --------------------------------------------------------------

inline std::string JsonString(const JsonValue* object, std::string_view name)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->IsString() ? value->String : std::string();
}

inline bool JsonInt32(const JsonValue* object, std::string_view name, std::int32_t& out)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    if (value == nullptr || !value->Integer || *value->Integer < std::numeric_limits<std::int32_t>::min() ||
        *value->Integer > std::numeric_limits<std::int32_t>::max())
        return false;
    out = static_cast<std::int32_t>(*value->Integer);
    return true;
}

/** True when the field is absent or false is not meant: reads a boolean that defaults to `fallback`. */
inline bool JsonFlag(const JsonValue* object, std::string_view name, bool fallback)
{
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->Type == JsonValue::Kind::Bool ? value->Bool : fallback;
}

inline std::vector<std::string> JsonStrings(const JsonValue* object, std::string_view name)
{
    std::vector<std::string> out;
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    if (value != nullptr && value->IsArray())
    {
        for (const JsonValue& item : value->Array)
        {
            if (item.IsString())
                out.push_back(item.String);
        }
    }
    return out;
}

/** The array field's items, or an empty list when it is absent or not an array. */
inline const std::vector<JsonValue>& JsonItems(const JsonValue* object, std::string_view name)
{
    static const std::vector<JsonValue> none;
    const JsonValue* value = object != nullptr ? object->Find(name) : nullptr;
    return value != nullptr && value->IsArray() ? value->Array : none;
}

/** Reads the family's name, skill, minCases and methods. Returns why not, or empty. */
std::string LoadFamilyHeader(const ContentRecord& record, StructureFamilyHeader& header);
/** Reads the case's name, subject, brief and method against its family. Returns why not, or empty. */
std::string LoadCaseBase(const ContentRecord& record, const StructureFamilyHeader& family, StructureCaseBase& base);

// ---- The family records -----------------------------------------------------------

StructureFamilyRecord* FindFamilyMutable(std::vector<StructureFamilyRecord>& families, std::string_view familyId);
/** Records `caseId` as seen and the most recent; creates the family's record. Refuses at the bounds. */
StructureError NoteCaseSeen(std::vector<StructureFamilyRecord>& families, std::string_view familyId, std::string_view caseId);
/** Counts a finished exposure and awards mastery once it is earned. */
void NoteFinish(StructureFamilyRecord& record, bool foreground, StructureGrade grade, StructureOutcome& outcome);
/** Routine abbreviation on the records: refuses an unmastered family; otherwise a Sound exposure at normal value. */
StructureError ResolveRoutineOnRecords(std::vector<StructureFamilyRecord>& families, std::string_view familyId, StructureOutcome& outcome);
/** The shared grading rule: Unsound when any `unsoundMask` bit is set, Sound when none is, else Partial. */
StructureGrade GradeFor(std::uint8_t failures, std::uint8_t unsoundMask);

// ---- Text -------------------------------------------------------------------------

inline constexpr std::string_view TextEnd = "end";
inline constexpr std::string_view TextNone = "-";

std::string Join(const std::vector<std::string>& ids, char separator = ',');
std::vector<std::string_view> Split(std::string_view text, char separator = ',');

/** Reads whole lines. A line without its newline is what a truncated text looks like, so it is refused. */
class LineReader
{
public:
    explicit LineReader(std::string_view text) : Text(text) {}
    bool Next(std::string_view& line);
    /** Reads the next line as `key=value`. */
    bool Pair(std::string_view key, std::string_view& value);
    [[nodiscard]] bool AtEnd() const noexcept { return Position == Text.size(); }

private:
    std::string_view Text;
    std::size_t Position = 0;
};

/** Canonical non-negative decimal: no sign, no leading zeros, nothing trailing. */
template <typename Int>
bool ParseInt(std::string_view text, Int& out)
{
    if (text.empty() || (text.size() > 1 && text.front() == '0'))
        return false;
    for (const char c : text)
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

bool ParseFlag(std::string_view text, bool& out);
/** `-` is an empty list; otherwise comma-separated well-formed IDs. */
bool ParseIds(std::string_view text, std::vector<std::string>& out, char separator = ',');

void AppendFamilies(std::string& text, const std::vector<StructureFamilyRecord>& families);
/** Reads `families=N` and its records. False when anything is malformed. */
bool ReadFamilies(LineReader& reader, std::vector<StructureFamilyRecord>& families);
/** Whether the exposure's case is the family's most recent one (what a running exposure must satisfy). */
bool ExposureMatchesRecent(const std::vector<StructureFamilyRecord>& families, const StructureExposureBase& exposure);
/** Checks the fields every exposure shares: well-formed IDs, a known family, units within bounds. */
bool BaseExposureValid(const std::vector<StructureFamilyRecord>& families, const StructureExposureBase& exposure);
} // namespace PerfectWorld::StructureInternal
