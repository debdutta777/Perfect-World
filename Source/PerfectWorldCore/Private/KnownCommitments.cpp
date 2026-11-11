#include "KnownCommitments.h"

#include "Conversation.h"

#include <limits>
#include <string_view>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view P311RotaId = "thread.evening_rota";

bool P311DefinitionValid(const EveningRotaDefinition& definition)
{
    return definition.StartMinute >= 0 && definition.StartMinute < MinutesPerDay && definition.Minutes > 0 &&
           definition.Minutes <= MinutesPerDay - definition.StartMinute && !definition.ActivityId.empty();
}

const LaneJournalAgreement* P311PlayerRota(const LaneJournalView& journal)
{
    const LaneJournalAgreement* selected = nullptr;
    for (const LaneJournalAgreement& agreement : journal.AgreementsRemembered)
    {
        if (agreement.PersonId != HouseholdAdultId) continue;
        if (agreement.AgreementId != FactRotaDishesWeekdays && agreement.AgreementId != FactRotaSweepAndCook &&
            agreement.AgreementId != FactRotaAlternateDays) continue;
        if (selected != nullptr && selected->AgreementId != agreement.AgreementId) return nullptr;
        selected = &agreement;
    }
    // Alternate days never records who starts. Do not infer a turn from the campaign date.
    return selected != nullptr && selected->AgreementId != FactRotaAlternateDays ? selected : nullptr;
}
} // namespace

KnownCommitmentContentResult LoadKnownCommitmentContent(const ContentDb& content)
{
    KnownCommitmentContentResult result;
    const ContentRecord* record = content.Find(P311RotaId);
    if (record == nullptr) return result;
    const JsonValue* metadata = record->Data.Find("eveningCommitment");
    const JsonValue* start = metadata != nullptr ? metadata->Find("startMinute") : nullptr;
    const JsonValue* minutes = metadata != nullptr ? metadata->Find("durationMinutes") : nullptr;
    const JsonValue* activity = metadata != nullptr ? metadata->Find("activity") : nullptr;
    if (record->Type != "thread_template" || metadata == nullptr || !metadata->IsObject() ||
        start == nullptr || !start->Integer || *start->Integer < 0 || *start->Integer >= MinutesPerDay ||
        minutes == nullptr || !minutes->Integer || *minutes->Integer <= 0 ||
        *minutes->Integer > MinutesPerDay - *start->Integer || activity == nullptr || !activity->IsString())
    {
        result.Error = "thread.evening_rota needs a valid eveningCommitment start, duration and activity";
        return result;
    }
    const ContentRecord* linked = content.Find(activity->String);
    const JsonValue* duration = linked != nullptr ? linked->Data.Find("durationMinutes") : nullptr;
    if (linked == nullptr || linked->Type != "activity" || duration == nullptr || !duration->Integer ||
        *duration->Integer != *minutes->Integer)
    {
        result.Error = "thread.evening_rota eveningCommitment must name an activity with the same duration";
        return result;
    }
    result.Content.EveningRota = EveningRotaDefinition{static_cast<std::int32_t>(*start->Integer),
        static_cast<std::int32_t>(*minutes->Integer), activity->String};
    return result;
}

std::optional<KnownCommitment> KnownEveningCommitment(const LaneJournalView& journal, const KnownCommitmentContent& content)
{
    if (!content.EveningRota || !P311DefinitionValid(*content.EveningRota)) return std::nullopt;
    const LaneJournalAgreement* agreement = P311PlayerRota(journal);
    if (agreement == nullptr) return std::nullopt;
    const std::int64_t firstDay = GameClock::DayNumber(0);
    if (journal.Day < firstDay) return std::nullopt;
    const std::int64_t day = journal.Day - firstDay;
    // Leave room for the entire window; refuse unrepresentable input before multiplying.
    if (day > (std::numeric_limits<GameInstant>::max() - MinutesPerDay) / MinutesPerDay) return std::nullopt;
    if (agreement->AgreementId == FactRotaDishesWeekdays && day % 7 >= 5) return std::nullopt;
    const EveningRotaDefinition& definition = *content.EveningRota;
    const GameInstant start = GameClock::StartOfDay(0) + day * MinutesPerDay + definition.StartMinute;
    if (start < 0) return std::nullopt;
    return KnownCommitment{start, definition.Minutes, definition.ActivityId};
}

std::vector<KnownPromiseWindow> KnownTimedPromises(const LaneJournalView& journal, const KnownCommitmentContent& content)
{
    const std::optional<KnownCommitment> evening = KnownEveningCommitment(journal, content);
    if (!evening) return {};
    const LaneJournalAgreement* agreement = P311PlayerRota(journal);
    return {{agreement->PersonId, agreement->AgreementId, *evening}};
}
} // namespace PerfectWorld
