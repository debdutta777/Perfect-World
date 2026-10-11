#include "TimeContract.h"

#include <limits>

// The time contract in content (ROADMAP M3-01: "content without declarations fails
// validation"). Today only activity records declare a clock; each states
// clock{mode, interrupt, resolve} and its cost as durationMinutes. The activity
// schema has no mission terms or beat counts yet, so a content mission declaration
// is refused (MissionTermsMissing) until M4-04 adds them.
namespace PerfectWorld
{
namespace
{
const std::string* TimeContractText(const JsonValue* object, std::string_view key)
{
    const JsonValue* value = object != nullptr ? object->Find(key) : nullptr;
    return value != nullptr && value->IsString() ? &value->String : nullptr;
}
} // namespace

DeclarationParse DeclarationFromActivityRecord(const ContentRecord& record)
{
    DeclarationParse result;
    const JsonValue* clock = record.Data.Find("clock");
    if (clock == nullptr || !clock->IsObject())
    {
        result.Error = TimeContractError::MissingDeclaration;
        return result;
    }
    InteractionDeclaration& declaration = result.Declaration;
    declaration.Id = record.Id;
    const std::string* mode = TimeContractText(clock, "mode");
    const std::string* interrupt = TimeContractText(clock, "interrupt");
    const std::string* resolve = TimeContractText(clock, "resolve");
    if (mode == nullptr || !ClockModeFromName(*mode, declaration.Mode))
        result.Error = TimeContractError::UnknownMode;
    else if (interrupt == nullptr || !InterruptPolicyFromName(*interrupt, declaration.Interrupt))
        result.Error = TimeContractError::UnknownInterrupt;
    else if (resolve == nullptr || !ResolutionBoundaryFromName(*resolve, declaration.Resolve))
        result.Error = TimeContractError::UnknownResolve;
    if (result.Error != TimeContractError::None)
        return result;

    const JsonValue* cost = record.Data.Find("durationMinutes");
    if (cost == nullptr || !cost->Integer || *cost->Integer < 0 || *cost->Integer > std::numeric_limits<std::int32_t>::max())
    {
        result.Error = TimeContractError::CostOutOfRange;
        return result;
    }
    declaration.CostMinutes = static_cast<std::int32_t>(*cost->Integer);
    result.Error = ValidateDeclaration(declaration);
    return result;
}

std::vector<std::string> ValidateActivityDeclarations(const ContentDb& content)
{
    std::vector<std::string> failures;
    for (const ContentRecord* record : content.OfType("activity"))
    {
        const DeclarationParse parsed = DeclarationFromActivityRecord(*record);
        if (!parsed.Ok())
            failures.push_back(record->Id + ": " + std::string(TimeContractErrorCode(parsed.Error)));
    }
    return failures;
}
} // namespace PerfectWorld
