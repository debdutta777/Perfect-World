// ROADMAP M3-01: every activity in the shipped content declares a time contract
// that validates, and content without a sound declaration is refused.
#include "TimeContract.h"
#include "TimeContractTestSupport.h"

#include <initializer_list>
#include <utility>

using namespace PerfectWorld;

namespace
{
JsonValue* Member(JsonValue& object, const char* key)
{
    for (auto& [name, value] : object.Object)
    {
        if (name == key)
            return &value;
    }
    return nullptr;
}

JsonValue MakeString(const char* text)
{
    JsonValue value;
    value.Type = JsonValue::Kind::String;
    value.String = text;
    return value;
}

JsonValue MakeInteger(std::int64_t number)
{
    JsonValue value;
    value.Type = JsonValue::Kind::Number;
    value.Number = static_cast<double>(number);
    value.Integer = number;
    return value;
}

/** The first activity record, copied, with clock-block members replaced and optionally a new cost. */
DeclarationParse Mutated(const ContentDb& db, std::initializer_list<std::pair<const char*, const char*>> clockEdits, std::int64_t duration = -1)
{
    ContentRecord copy = *db.OfType("activity").front();
    JsonValue* clock = Member(copy.Data, "clock");
    Expect(clock != nullptr, "the activity has a clock block");
    for (const auto& [key, value] : clockEdits)
        *Member(*clock, key) = MakeString(value);
    if (duration >= 0)
        *Member(copy.Data, "durationMinutes") = MakeInteger(duration);
    return DeclarationFromActivityRecord(copy);
}
} // namespace

void TestContent(const std::vector<ContentFile>& files)
{
    const ContentLoadResult content = LoadContent(files);
    Expect(content.Ok(), "the real content validates");
    const std::vector<const ContentRecord*> activities = content.Db.OfType("activity");
    Expect(activities.size() >= 7, "the shipped activities are present");

    // Every declaration in Data/Content/activity validates against the mode rules.
    Expect(ValidateActivityDeclarations(content.Db).empty(), "every activity declaration validates");
    for (const ContentRecord* record : activities)
    {
        const DeclarationParse parsed = DeclarationFromActivityRecord(*record);
        Expect(parsed.Ok(), "an activity declaration parses and validates");
        const JsonValue* clock = record->Data.Find("clock");
        ClockMode mode = ClockMode::Compressed;
        Expect(ClockModeFromName(clock->Find("mode")->String, mode) && parsed.Declaration.Mode == mode, "the declared mode is the content mode");
        Expect(parsed.Declaration.CostMinutes == *record->Data.Find("durationMinutes")->Integer, "the declared cost is the activity duration");
        Expect(parsed.Declaration.Id == record->Id, "the declaration carries the activity id");
        const TimeResolution full = ResolveInteraction(parsed.Declaration, {Progress(parsed.Declaration.CostMinutes)});
        Expect(full.Outcome == TimeOutcome::Completed && full.ChargedMinutes == parsed.Declaration.CostMinutes,
               "an activity run to its end is charged exactly its declared duration");
    }
    const ContentRecord* sleep = content.Db.Find("activity.sleep_night");
    Expect(sleep != nullptr, "the sleep activity exists");
    const DeclarationParse sleeping = DeclarationFromActivityRecord(*sleep);
    const TimeResolution breach = ResolveInteraction(sleeping.Declaration, {Progress(100), InterruptEvent()});
    Expect(breach.Outcome == TimeOutcome::InterruptedImmediately && breach.ChargedMinutes == 100, "a breach ends the night's sleep and charges the elapsed minutes");

    // Content without a declaration, or with an unsound one, fails.
    ContentDb stripped = content.Db;
    ContentRecord& victim = stripped.Records.at("activity.eat_meal");
    std::erase_if(victim.Data.Object, [](const auto& member) { return member.first == "clock"; });
    const std::vector<std::string> failures = ValidateActivityDeclarations(stripped);
    Expect(failures.size() == 1 && failures[0] == "activity.eat_meal: time.missing_declaration", "an activity with no clock block fails validation");
    Expect(DeclarationFromActivityRecord(victim).Error == TimeContractError::MissingDeclaration, "the missing declaration is named");

    // The content schema itself also refuses a record with no clock.
    std::vector<ContentFile> edited = files;
    bool found = false;
    for (ContentFile& file : edited)
    {
        if (file.Path.find("/activity/activity.rest.json") == std::string::npos)
            continue;
        const std::size_t from = file.Text.find("\"clock\"");
        Expect(from != std::string::npos, "the rest activity has a clock block");
        file.Text.replace(from, 7, "\"nope\"");
        found = true;
    }
    Expect(found && !LoadContent(edited).Ok(), "the content validator rejects an activity without its clock");

    // The first activity is attend_class: scene, at_beat, on_complete, 45 minutes.
    Expect(Mutated(content.Db, {}).Ok(), "the unmutated first activity is sound");
    Expect(Mutated(content.Db, {{"mode", "slow"}}).Error == TimeContractError::UnknownMode, "an unknown mode is named");
    Expect(Mutated(content.Db, {{"interrupt", "eventually"}}).Error == TimeContractError::UnknownInterrupt, "an unknown interrupt policy is named");
    Expect(Mutated(content.Db, {{"resolve", "someday"}}).Error == TimeContractError::UnknownResolve, "an unknown resolution boundary is named");
    Expect(Mutated(content.Db, {{"mode", "mission"}, {"interrupt", "none"}}).Error == TimeContractError::MissionTermsMissing,
           "content cannot declare a mission without its terms yet");
    Expect(Mutated(content.Db, {{"mode", "mission"}}).Error == TimeContractError::InterruptNotAllowed, "a mission has no beats to interrupt at");
    Expect(Mutated(content.Db, {{"mode", "ui_paused"}}).Error == TimeContractError::CostOutOfRange, "a phone interaction cannot carry a cost");
    Expect(Mutated(content.Db, {{"mode", "conversation"}}, 120).Error == TimeContractError::CostOutOfRange, "a conversation cannot declare two hours");
    Expect(Mutated(content.Db, {{"mode", "conversation"}}, 20).Ok(), "a short conversation declares");
    Expect(Mutated(content.Db, {}, 1441).Error == TimeContractError::CostOutOfRange, "a scene cannot declare more than a day");
    Expect(Mutated(content.Db, {}, 0).Error == TimeContractError::CostOutOfRange, "a scene cannot be free");
    Expect(Mutated(content.Db, {{"resolve", "on_commit"}}).Error == TimeContractError::CommitCannotBeInterrupted, "on_commit with an at_beat policy is contradictory");
    Expect(Mutated(content.Db, {{"resolve", "on_commit"}, {"interrupt", "none"}}).Ok(), "on_commit with no interruption is sound");
    Expect(Mutated(content.Db, {{"mode", "compressed"}}, 10080).Ok(), "a compressed declaration is judged by the compressed rules");
    Expect(Mutated(content.Db, {{"mode", "compressed"}, {"interrupt", "immediate"}}, 10080).Error == TimeContractError::InterruptNotAllowed,
           "compression cannot be interrupted immediately");
}
