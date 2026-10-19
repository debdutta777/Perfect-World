#include "LaneJournal.h"

#include "WorldState.h"

#include <algorithm>
#include <charconv>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::pair<std::string_view, std::string_view> R4JPeople[] = {
    {"person.household_adult", "Your parent"}, {"person.care_worker", "Care worker"},
    {"person.farida_qadri", "Farida Qadri"}, {"person.anindita_sen", "Anindita Sen"},
    {"person.mira_sen", "Mira Sen"}, {"person.subrata_dey", "Subrata Dey"},
    {"person.nirmal_bose", "Nirmal Bose"}, {"person.rupa_ghosh", "Rupa Ghosh"},
    {"person.sahana_mitra", "Sahana Mitra"}, {"person.purnima_das", "Purnima Das"},
    {"person.arun_paul", "Arun Paul"}, {"person.rekha_bose", "Rekha Bose"},
    {"person.debashis_pal", "Debashis Pal"}, {"person.mahua_saha", "Mahua Saha"}};

// Explicit content mapping: arbitrary remembered facts must never become promises in the journal.
constexpr std::pair<std::string_view, std::string_view> R4JAgreements[] = {
    {"person.household_adult", FactEveningRotaAgreed},
    {"person.household_adult", FactRotaDishesWeekdays},
    {"person.household_adult", FactRotaAlternateDays},
    {"person.household_adult", FactRotaSweepAndCook},
    {"person.care_worker", "fact.care_jobs_breakfast_weekdays"},
    {"person.care_worker", "fact.care_jobs_turns"},
    {"person.care_worker", "fact.care_jobs_study_room"},
    {"person.nirmal_bose", "fact.nirmal_bose.book_evening_agreed"},
    {"person.rupa_ghosh", "fact.rupa_ghosh.visit_promised"},
    {"person.sahana_mitra", "fact.sahana_mitra.sewing_evening_agreed"},
    {"person.subrata_dey", "fact.subrata_dey.step_promised"},
    {"person.debashis_pal", "fact.debashis_pal.bring_promised"},
    {"person.mahua_saha", "fact.mahua_saha.sort_promised"}};

bool R4JBefore(const LaneJournalEncounter& a, const LaneJournalEncounter& b)
{
    return a.Day < b.Day || (a.Day == b.Day && a.PersonId < b.PersonId);
}

bool R4JNumber(std::string_view text, std::int64_t& value)
{
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && std::to_string(value) == text;
}

bool R4JLine(std::string_view& text, std::string_view& line)
{
    const auto end = text.find('\n');
    if (end == std::string_view::npos) return false;
    line = text.substr(0, end);
    text.remove_prefix(end + 1);
    return true;
}
} // namespace

std::string_view LaneJournalDisplayName(std::string_view personId) noexcept
{
    for (const auto& [id, name] : R4JPeople)
        if (id == personId) return name;
    return {};
}

LaneJournalView LaneJournalToday(const WorldState& world, std::int64_t day)
{
    LaneJournalView view;
    view.Day = day;
    for (const auto& row : world.Journal.Encounters)
        if (row.Day == day)
            view.PeopleMetToday.push_back({row.PersonId, std::string(LaneJournalDisplayName(row.PersonId)), row.FirstMetMinute});
    for (const auto& [person, conversation] : world.Conversations)
    {
        for (const auto& [owner, fact] : R4JAgreements)
        {
            if (owner != person || !HasConversationFact(conversation, fact)) continue;
            if (fact == FactEveningRotaAgreed &&
                (HasConversationFact(conversation, FactRotaDishesWeekdays) || HasConversationFact(conversation, FactRotaAlternateDays) ||
                 HasConversationFact(conversation, FactRotaSweepAndCook))) continue;
            view.AgreementsRemembered.push_back({person, std::string(fact), "journal.agreement." + std::string(fact.substr(5))});
        }
    }
    std::sort(view.AgreementsRemembered.begin(), view.AgreementsRemembered.end(), [](const auto& a, const auto& b)
        { return std::pair{a.PersonId, a.AgreementId} < std::pair{b.PersonId, b.AgreementId}; });
    return view;
}

void RecordLaneJournalEncounter(WorldState& world, std::string_view personId)
{
    LaneJournalEncounter row{GameClock::DayNumber(world.Clock.Now()), std::string(personId), world.Clock.Now()};
    auto& rows = world.Journal.Encounters;
    const auto at = std::lower_bound(rows.begin(), rows.end(), row, R4JBefore);
    if (at == rows.end() || at->Day != row.Day || at->PersonId != row.PersonId)
        rows.insert(at, std::move(row));
}

bool IsLaneJournalValid(const WorldState& world) noexcept
{
    const LaneJournalEncounter* previous = nullptr;
    for (const auto& row : world.Journal.Encounters)
    {
        const auto conversation = world.Conversations.find(row.PersonId);
        if (row.FirstMetMinute < 0 || row.FirstMetMinute > DayLoopMaxInstant || row.FirstMetMinute > world.Clock.Now() ||
            GameClock::DayNumber(row.FirstMetMinute) != row.Day || LaneJournalDisplayName(row.PersonId).empty() ||
            conversation == world.Conversations.end() || conversation->second.SessionCount == 0 ||
            (previous != nullptr && !R4JBefore(*previous, row))) return false;
        previous = &row;
    }
    return true;
}

std::string SerialiseLaneJournal(const WorldState& world)
{
    if (!IsLaneJournalValid(world)) return {};
    std::string text = "pw.lane_journal/1\ncount=" + std::to_string(world.Journal.Encounters.size()) + "\n";
    for (const auto& row : world.Journal.Encounters)
        text += "encounter=" + std::to_string(row.Day) + "," + std::to_string(row.FirstMetMinute) + "," + row.PersonId + "\n";
    return text + "end\n";
}

bool DeserialiseLaneJournal(std::string_view text, WorldState& world)
{
    const std::string_view original = text;
    std::string_view line;
    std::int64_t count = 0;
    if (!R4JLine(text, line) || line != "pw.lane_journal/1" || !R4JLine(text, line) || !line.starts_with("count=") ||
        !R4JNumber(line.substr(6), count) || count < 0 || static_cast<std::uint64_t>(count) > text.size() / 16) return false;
    LaneJournalState parsed;
    for (std::int64_t i = 0; i < count; ++i)
    {
        if (!R4JLine(text, line) || !line.starts_with("encounter=")) return false;
        line.remove_prefix(10);
        const auto first = line.find(',');
        const auto second = first == std::string_view::npos ? first : line.find(',', first + 1);
        LaneJournalEncounter row;
        if (first == std::string_view::npos || second == std::string_view::npos ||
            !R4JNumber(line.substr(0, first), row.Day) || !R4JNumber(line.substr(first + 1, second - first - 1), row.FirstMetMinute)) return false;
        row.PersonId = line.substr(second + 1);
        parsed.Encounters.push_back(std::move(row));
    }
    if (text != "end\n") return false;
    LaneJournalState old = std::move(world.Journal);
    world.Journal = std::move(parsed);
    if (SerialiseLaneJournal(world) == original) return true;
    world.Journal = std::move(old);
    return false;
}
} // namespace PerfectWorld
