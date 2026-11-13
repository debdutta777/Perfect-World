#include "KnownCommitments.h"

#include "Conversation.h"
#include "Threads.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string_view>
#include <utility>

using namespace PerfectWorld;

namespace
{
void P311Expect(bool condition, const char* message)
{
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

std::vector<ContentFile> P311ReadContent(const std::filesystem::path& root)
{
    std::vector<ContentFile> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;
        std::ifstream stream(entry.path(), std::ios::binary);
        P311Expect(stream.good(), "content file opens");
        files.push_back({entry.path().generic_string(), std::string(std::istreambuf_iterator<char>(stream), {})});
    }
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.Path < b.Path; });
    return files;
}

LaneJournalView P311Journal(std::int32_t juneDay, std::string_view fact)
{
    LaneJournalView journal;
    journal.Day = GameClock::DayNumber(GameClock::FromCalendar(2042, 6, juneDay, 12, 0));
    if (!fact.empty()) journal.AgreementsRemembered.push_back({std::string(HouseholdAdultId), std::string(fact), ""});
    return journal;
}

void P311RotaQueries(const KnownCommitmentContent& content)
{
    const GameInstant monday = GameClock::FromCalendar(2042, 6, 2, 18, 0);
    LaneJournalView journal = P311Journal(2, FactRotaDishesWeekdays);
    const LaneJournalView before = journal;
    const KnownCommitmentContent contentBefore = content;
    const auto known = KnownEveningCommitment(journal, content);
    P311Expect(known && known->Start == monday && known->Minutes == 60 && known->ActivityId == "activity.household_chores",
               "Monday dishes returns the authored instant, duration and activity");
    const auto promises = KnownTimedPromises(journal, content);
    P311Expect(promises.size() == 1 && promises[0].Commitment == *known && promises[0].PersonId == HouseholdAdultId &&
               promises[0].AgreementId == FactRotaDishesWeekdays, "timed promise retains its agreement identity");
    P311Expect(journal == before && content == contentBefore, "queries leave both inputs untouched");
    P311Expect(KnownTimedPromises(journal, content) == promises, "repeated reads are identical");
    P311Expect(KnownEveningCommitment(P311Journal(6, FactRotaDishesWeekdays), content).has_value(), "Friday dishes is due");
    P311Expect(!KnownEveningCommitment(P311Journal(7, FactRotaDishesWeekdays), content), "Saturday dishes is omitted");
    P311Expect(!KnownEveningCommitment(P311Journal(8, FactRotaDishesWeekdays), content), "Sunday dishes is omitted");
    const auto next = KnownEveningCommitment(P311Journal(9, FactRotaDishesWeekdays), content);
    P311Expect(next && next->Start == monday + 7 * MinutesPerDay, "next Monday uses the requested day");
    P311Expect(KnownEveningCommitment(P311Journal(8, FactRotaSweepAndCook), content).has_value(), "sweep/cook includes Sunday");
    for (const std::string_view fact : {std::string_view{}, FactEveningRotaAgreed, FactEveningRotaKept, FactRotaAlternateDays})
    {
        P311Expect(!KnownEveningCommitment(P311Journal(2, fact), content), "no invented legacy or alternating turn");
        P311Expect(KnownTimedPromises(P311Journal(2, fact), content).empty(), "untimed agreements are omitted");
    }
    journal.AgreementsRemembered.push_back(journal.AgreementsRemembered.front());
    P311Expect(KnownTimedPromises(journal, content).size() == 1, "duplicate inputs do not duplicate a promise");
    journal.AgreementsRemembered.push_back({std::string(HouseholdAdultId), std::string(FactEveningRotaAgreed), ""});
    P311Expect(KnownTimedPromises(journal, content).size() == 1, "implied generic agreement does not duplicate a promise");
    journal.AgreementsRemembered.push_back({std::string(HouseholdAdultId), std::string(FactRotaSweepAndCook), ""});
    P311Expect(KnownTimedPromises(journal, content).empty(), "contradictory specific agreements are omitted");
    journal = P311Journal(2, FactRotaDishesWeekdays);
    journal.AgreementsRemembered[0].PersonId = "person.care_worker";
    P311Expect(KnownTimedPromises(journal, content).empty(), "a rota fact for another person is not the household agreement");
    journal = P311Journal(2, FactRotaDishesWeekdays);
    journal.Day = std::numeric_limits<std::int64_t>::max();
    P311Expect(!KnownEveningCommitment(journal, content), "day overflow is refused");
    journal.Day = std::numeric_limits<std::int64_t>::min();
    P311Expect(!KnownEveningCommitment(journal, content), "pre-campaign day is refused without overflow");
    P311Expect(!KnownEveningCommitment(before, {}), "missing timing content gives no commitment");
    KnownCommitmentContent malformed = content;
    malformed.EveningRota->Minutes = 0;
    P311Expect(!KnownEveningCommitment(before, malformed), "zero duration cannot produce a commitment");
}

void P311UntimedVisits(const KnownCommitmentContent& content)
{
    LaneJournalView journal = P311Journal(2, "");
    // This encounter has an exact time; it is history, never an appointment for another visit.
    journal.PeopleMetToday.push_back({"person.rupa_ghosh", "Rupa Ghosh", GameClock::FromCalendar(2042, 6, 2, 17, 0)});
    journal.AgreementsRemembered = {
        {"person.rupa_ghosh", "fact.rupa_ghosh.visit_promised", ""},
        {"person.nirmal_bose", "fact.nirmal_bose.book_evening_agreed", ""},
        {"person.sahana_mitra", "fact.sahana_mitra.sewing_evening_agreed", ""},
        {"person.subrata_dey", "fact.subrata_dey.step_promised", ""},
        {"person.care_worker", "fact.care_jobs_breakfast_weekdays", ""},
        {"person.satyajit_dutta", "fact.satyajit_dutta.bring_garment_agreed", ""}};
    const auto before = journal;
    P311Expect(KnownTimedPromises(journal, content).empty(), "visiting hours and a due time without a duration invent no window");
    journal.AgreementsRemembered.push_back({std::string(HouseholdAdultId), std::string(FactRotaSweepAndCook), ""});
    const auto mixed = journal;
    P311Expect(KnownTimedPromises(journal, content).size() == 1, "mixed journal yields only the timed promise");
    P311Expect(journal == mixed && before.PeopleMetToday == journal.PeopleMetToday, "promise queries preserve journal history");
}

void P311Replace(std::vector<ContentFile>& files, std::string_view oldText, std::string_view newText)
{
    for (auto& file : files)
    {
        if (file.Path.find("thread.evening_rota.json") == std::string::npos) continue;
        const auto at = file.Text.find(oldText);
        P311Expect(at != std::string::npos, "mutation target exists");
        file.Text.replace(at, oldText.size(), newText);
        return;
    }
    P311Expect(false, "rota content exists");
}

void P311LoaderFailures(const std::vector<ContentFile>& original)
{
    const std::pair<std::string_view, std::string_view> mutations[] = {
        {"\"startMinute\": 1080", "\"startMinute\": -1"},
        {"\"startMinute\": 1080", "\"startMinute\": 1440"},
        {"\"startMinute\": 1080", "\"startMinute\": 1400"},
        {"\"startMinute\": 1080", "\"startMinute\": 1080.5"},
        {"\"durationMinutes\": 60", "\"durationMinutes\": 0"},
        {"\"durationMinutes\": 60", "\"durationMinutes\": 30"},
        {"\"durationMinutes\": 60", "\"durationMinutes\": 9223372036854775807"},
        {"\"activity\": \"activity.household_chores\"", "\"activity\": \"activity.missing\""},
        {"\"activity\": \"activity.household_chores\"", "\"activity\": \"thread.evening_rota\""},
        {"\"startMinute\": 1080, ", ""},
        {"\"durationMinutes\": 60, ", ""},
        {"\"eveningCommitment\": { \"startMinute\": 1080, \"durationMinutes\": 60, \"activity\": \"activity.household_chores\" },", ""}};
    for (const auto& [from, to] : mutations)
    {
        auto files = original;
        P311Replace(files, from, to);
        const auto loaded = LoadContent(files);
        P311Expect(!loaded.Ok(), "registry rejects invalid rota metadata");
        P311Expect(!LoadKnownCommitmentContent(loaded.Db).Ok(), "dedicated loader rejects invalid metadata");
        P311Expect(!LoadThreadTemplates(loaded.Db).Ok(), "thread loader also rejects invalid metadata");
    }
    P311Expect(LoadKnownCommitmentContent({}).Ok(), "partial content without a rota is allowed");
    P311Expect(!LoadKnownCommitmentContent({}).Content.EveningRota, "partial content supplies no default timing");
    auto files = original;
    P311Replace(files, "\"startMinute\": 1080", "\"startMinute\": 1140");
    const auto shifted = LoadKnownCommitmentContent(LoadContent(files).Db);
    const auto promise = KnownEveningCommitment(P311Journal(2, FactRotaSweepAndCook), shifted.Content);
    P311Expect(shifted.Ok() && promise && promise->Start == GameClock::FromCalendar(2042, 6, 2, 19, 0),
               "query uses content timing rather than a hardcoded due time");
}
} // namespace

int main(int argc, char** argv)
{
    P311Expect(argc == 2, "usage: PerfectWorldKnownCommitmentsTests <Data/Content>");
    const auto files = P311ReadContent(argv[1]);
    const auto loaded = LoadContent(files);
    for (const auto& issue : loaded.Issues) std::cerr << issue.Id << ": " << issue.Message << '\n';
    P311Expect(loaded.Ok(), "shipped content validates");
    P311Expect(LoadThreadTemplates(loaded.Db).Ok(), "shipped thread templates load");
    const auto definitions = LoadKnownCommitmentContent(loaded.Db);
    P311Expect(definitions.Ok() && definitions.Content.EveningRota.has_value(), "authored timing loads");
    P311RotaQueries(definitions.Content);
    P311UntimedVisits(definitions.Content);
    P311LoaderFailures(files);
    std::cout << "Known commitments tests passed\n";
}
