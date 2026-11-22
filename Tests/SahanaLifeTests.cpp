#include "NpcScheduleContent.h"

#include "ContentSchemas.h"
#include "Conversation.h"
#include "Doors.h"
#include "NpcBehaviour.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>

using namespace PerfectWorld;
namespace
{
int SmAssertions = 0;
constexpr std::string_view SmPerson = "person.sahana_mitra";
constexpr std::string_view SmHome = "location.nabapara.tall_flats_unit_b";
constexpr std::string_view SmErrand = "location.nabapara.sahana_errand";
constexpr std::string_view SmFact = "fact.sahana_mitra.sewing_evening_agreed";
void SmRequire(bool condition, const char* message)
{
    ++SmAssertions;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
// Day 0 is a Monday; minute is the minute of that day. Instant 0 is Monday 06:00.
GameInstant SmAt(int day, int minute) { return (day + 7) * MinutesPerDay + minute - 360; }
bool SmErrandMinute(int day, int minute)
{
    if (day % 7 <= 4) return minute >= 840 && minute < 930;
    if (day % 7 == 5) return minute >= 630 && minute < 720;
    return false;
}
bool SmHomeAt(const NpcSchedule& schedule, std::string_view home, GameInstant at)
{
    const auto resolved = ResolveSchedule(schedule.Person, schedule.Travel, at);
    return resolved.Status == ScheduleStatus::Stay && resolved.Zone == home;
}

void SmWeek(const NpcSchedule& schedule)
{
    const DoorDef* door = FindDoor("door.nabapara.tall_flats_unit_b");
    SmRequire(door && door->Access == DoorAccess::Private && door->LocationId == SmHome, "unit B owns her private door");
    SmRequire(door->VisitingWindows.size() == 1 && door->VisitingWindows[0].OpenMinute == 1020 &&
        door->VisitingWindows[0].CloseMinute == 1230, "17:00-20:30 visiting window");
    int errandMinutes = 0;
    for (int day = 0; day < 15; ++day)
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto at = SmAt(day, minute);
            const auto resolved = ResolveSchedule(schedule.Person, schedule.Travel, at);
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
            SmRequire(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle && intent.Ok(),
                "complete unsqueezed fortnight and rollovers");
            SmRequire(resolved.Zone != "location.nabapara.tall_flats_unit_a", "never in unit A");
            if (minute < 390 || minute >= 1380)
                SmRequire(resolved.Zone == SmHome && intent.Intent.Availability == PersonAvailability::Asleep, "23:00-06:30 sleep at home");
            const bool errand = resolved.Status == ScheduleStatus::Stay && resolved.Zone == SmErrand;
            SmRequire(errand == SmErrandMinute(day, minute), "errand on weekday afternoons and Saturday morning only");
            errandMinutes += errand;
            if (day % 7 <= 4 && minute >= 480 && minute < 750)
                SmRequire(resolved.Zone == SmHome && intent.Intent.State == NpcState::Work &&
                    intent.Intent.Availability == PersonAvailability::Busy, "weekday morning sewing at home");
            if (day % 7 == 6 && minute >= 480 && minute < 1020)
                SmRequire(resolved.Zone == SmHome && resolved.Status == ScheduleStatus::Stay, "Sunday entirely at home");
            if (minute >= 1020 && minute < 1230)
            {
                SmRequire(resolved.Zone == SmHome && resolved.Status == ScheduleStatus::Stay &&
                    intent.Intent.Availability == PersonAvailability::Awake && intent.Intent.bTalkable,
                    "genuinely home and awake throughout visiting hours, all seven days");
                DoorUseContext context;
                context.IsHostHome = true;
                SmRequire(EvaluateDoorKnock(*door, minute, context).Answer == DoorKnockAnswer::Admitted, "home knock admitted");
            }
            if (minute >= 1230 && minute < 1290)
                SmRequire(intent.Intent.Availability == PersonAvailability::Busy, "late sewing after visiting hours");
        }
    SmRequire(errandMinutes == (11 + 2) * 90, "eleven weekday errands and two Saturday errands in fifteen days");
    for (int minute : {450, 760, 1100})
        SmRequire(ResolveSchedule(schedule.Person, schedule.Travel, SmAt(0, minute)).Doing == "break", "breaks at breakfast, midday and supper");
    DoorUseContext absent;
    SmRequire(EvaluateDoorKnock(*door, 1100, absent).Answer == DoorKnockAnswer::NoAnswer, "no absent admission");
    absent.IsHostHome = true;
    SmRequire(EvaluateDoorKnock(*door, 1230, absent).Answer == DoorKnockAnswer::NotReceiving, "end boundary excludes visitors");
    // Her trips are exactly 16 minutes each way; a stay never begins before she could arrive.
    const auto leaving = ResolveSchedule(schedule.Person, schedule.Travel, SmAt(0, 830));
    SmRequire(leaving.Status == ScheduleStatus::Travel && leaving.Zone == SmErrand && leaving.FromZone == SmHome, "walking out to the errand");
    const auto coming = ResolveSchedule(schedule.Person, schedule.Travel, SmAt(5, 725));
    SmRequire(coming.Status == ScheduleStatus::Travel && coming.Zone == SmHome && coming.FromZone == SmErrand, "walking home on Saturday");
    for (const auto at : {SmAt(0, 660), SmAt(0, 1080), SmAt(1, 1250)})
    {
        const auto before = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
        const auto chat = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, true, false);
        const auto after = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
        SmRequire(chat.Ok() && chat.Intent.State == NpcState::Chat && chat.Intent.ResumeState == before.Intent.State &&
            chat.Intent.ResumeWorkActionId == before.Intent.WorkActionId && before.Intent == after.Intent,
            "interruption preserves work or rest and Leave resumes");
    }
    auto whole = StartSchedulePerson(SmPerson, SmAt(0, 0));
    auto split = whole;
    const auto end = SmAt(8, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        SmRequire(part.Ok, "split advance");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    SmRequire(full.Ok && whole == split && full.Events == events, "partition independent schedule events");
}

// Unit A and unit B are independent households: each host's truth is its own schedule, and the two never share the passage at once.
void SmIndependence(const NpcSchedule& sahana, const NpcSchedule& nirmal)
{
    int onlyNirmalHome = 0, onlySahanaHome = 0, bothHome = 0;
    for (int day = 0; day < 7; ++day)
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto at = SmAt(day, minute);
            const bool a = SmHomeAt(nirmal, "location.nabapara.tall_flats_unit_a", at);
            const bool b = SmHomeAt(sahana, SmHome, at);
            onlyNirmalHome += a && !b;
            onlySahanaHome += !a && b;
            bothHome += a && b;
            const auto ra = ResolveSchedule(nirmal.Person, nirmal.Travel, at);
            const auto rb = ResolveSchedule(sahana.Person, sahana.Travel, at);
            SmRequire(!(ra.Status == ScheduleStatus::Travel && rb.Status == ScheduleStatus::Travel), "never both walking the passage");
            if (minute >= 1020 && minute < 1230) SmRequire(a && b, "both hosts home through the shared visiting window");
        }
    SmRequire(onlySahanaHome > 0 && bothHome > 0, "unit B truth differs from unit A's");
    SmRequire(nirmal.Person.PersonId != sahana.Person.PersonId && nirmal.Person.HomeZone != sahana.Person.HomeZone, "distinct people and homes");
    for (const auto& entry : sahana.Person.Routine)
        SmRequire(entry.Zone != nirmal.Person.HomeZone, "no routine entry in unit A");
    // A distinct rhythm: she wakes later and sleeps later than the stocking shift of unit A.
    const auto nirmalWake = ResolveSchedule(nirmal.Person, nirmal.Travel, SmAt(0, 370));
    const auto sahanaWake = ResolveSchedule(sahana.Person, sahana.Travel, SmAt(0, 370));
    SmRequire(nirmalWake.Doing != "sleeping" && sahanaWake.Doing == "sleeping", "she is still asleep when unit A is up");
    const auto nirmalLate = ResolveSchedule(nirmal.Person, nirmal.Travel, SmAt(0, 1340));
    const auto sahanaLate = ResolveSchedule(sahana.Person, sahana.Travel, SmAt(0, 1340));
    SmRequire(nirmalLate.Doing == "sleeping" && sahanaLate.Doing != "sleeping", "she is still up when unit A sleeps");
}

void SmTalkAndSave()
{
    const auto catalogue = SahanaConversationCatalogue();
    SmRequire(ValidateConversationCatalogue(catalogue), "catalogue valid");
    SmRequire(ConversationCatalogueForPerson(SmPerson).has_value(), "world routes Sahana");
    for (const auto& node : catalogue.Nodes)
    {
        for (const char c : node.Line) SmRequire(c >= 32 && c < 127, "plain ASCII");
        for (const auto& word : {"pay", "paid", "money", "rupee", "price", "cost", "fee", "buy", "sell"})
            SmRequire(node.Line.find(word) == std::string_view::npos, "no money words");
    }
    ConversationState memory;
    for (int remembered = 0; remembered < 2; ++remembered)
    {
        for (int a = 0; a < static_cast<int>(PersonAvailabilityCount); ++a)
            for (int minute = 0; minute < 1440; ++minute)
            {
                int count = 0;
                for (const auto& node : catalogue.Nodes)
                    count += ConversationNodeServes(node, minute, memory, static_cast<PersonAvailability>(a));
                SmRequire(count == 1, "exactly one greeting or decline at every minute");
            }
        SmRequire(OpenConversation(catalogue, memory, SmPerson, 1080, PersonAvailability::Awake) == ConversationError::None, "open");
        if (!remembered) SmRequire(ChooseConversationChoice(catalogue, memory, "choice.sahana_mitra.agree").Ok(), "free agreement");
        SmRequire(LeaveConversation(memory) == ConversationError::None && HasConversationFact(memory, SmFact), "Leave retains agreement");
    }
    for (const auto availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        ConversationState state;
        SmRequire(OpenConversation(catalogue, state, SmPerson, 1080, availability) == ConversationError::None, "decline opens");
        SmRequire(VisibleConversationChoices(catalogue, state).size() == 1 &&
            !ChooseConversationChoice(catalogue, state, "choice.sahana_mitra.agree").Ok(), "decline offers Leave only");
        SmRequire(LeaveConversation(state) == ConversationError::None && state.Facts.empty(), "decline creates no memory");
    }
    CharacterCreator creator;
    SmRequire(creator.ApplyPreset("balanced_scholar").Succeeded() && creator.StartLife().Succeeded(), "started creation");
    WorldSaveData data;
    SmRequire(BeginWorldFromCreation(creator.CaptureSave(), 9503, data).Succeeded(), "new world");
    WorldSim live(data.World);
    const std::string person(SmPerson);
    SmRequire(live.Submit({CommandSource::Player, AdvanceTime{720}}).Accepted, "18:00");
    SmRequire(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open reducer session");
    SmRequire(live.Submit({CommandSource::Player, ConversationChoose{person, "choice.sahana_mitra.agree", 1}}).Accepted, "agreement through reducer");
    const auto before = live.State().Clock.Now();
    SmRequire(live.Submit({CommandSource::Player, ConversationChoose{person, "choice.sahana_mitra.sewing", 1}}).Accepted, "timed choice");
    SmRequire(live.State().Clock.Now() == before + 10, "ten minute charge");
    data.World = live.State();
    data.Player = {"zone.street.nabapara_lane", {400, 300, 85}, 12345, -1200, CameraMode::ThirdPerson};
    const std::string text = SerializeWorldSave(data);
    WorldSaveData loaded;
    SmRequire(!text.empty() && DeserializeWorldSave(text, loaded).HashVerified, "certified mid-session world round trip");
    SmRequire(loaded.Player == data.Player && loaded.World.Conversations == data.World.Conversations, "placement memory and receipt retained");
    WorldSim resumed(loaded.World);
    SmRequire(!resumed.Submit({CommandSource::Player, ConversationChoose{person, "choice.sahana_mitra.sewing", 1}}).Accepted &&
        resumed.State().Clock.Now() == before + 10, "no duplicate charge after restore");
    SmRequire(resumed.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "restore closes session");
    SmRequire(HasConversationFact(resumed.State().Conversations.at(person), SmFact) &&
        resumed.State().Conversations.at(person).Receipts.size() == 1, "closing session preserves history");
    SmRequire(resumed.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted &&
        resumed.State().Conversations.at(person).NodeId == "node.sahana_mitra.remembered", "new session remembers agreement");
    auto bad = loaded;
    bad.World.Conversations.at(person).Facts.push_back("fact.sahana_mitra.invented");
    SmRequire(SerializeWorldSave(bad).empty(), "invented memory refused");
}

std::string SmRead(const char* path)
{
    std::ifstream input(path);
    SmRequire(input.good(), "content exists");
    return std::string{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
}

int main(int argc, char** argv)
{
    SmRequire(argc == 3, "content paths supplied (Sahana, Nirmal)");
    const auto result = LoadContent({{"/npc_schedule/npc_schedule.sahana_mitra.json", SmRead(argv[1])},
                                     {"/npc_schedule/npc_schedule.nirmal_bose.json", SmRead(argv[2])}});
    for (const auto& issue : result.Issues) std::cerr << issue.Message << '\n';
    SmRequire(result.Ok(), "registered content loads");
    const auto loaded = LoadNpcSchedule(result.Db, "npc_schedule.sahana_mitra");
    if (!loaded.Ok()) std::cerr << loaded.Error << '\n';
    SmRequire(loaded.Ok() && ValidateNpcScheduleContent(*loaded.Value).empty(), "typed schedule loads");
    const auto nirmal = LoadNpcSchedule(result.Db, "npc_schedule.nirmal_bose");
    SmRequire(nirmal.Ok(), "unit A schedule loads");
    const auto& legs = loaded.Value->Travel.Legs;
    for (std::size_t i = 1; i < legs.size(); ++i)
        SmRequire(legs[i - 1].From < legs[i].From || (legs[i - 1].From == legs[i].From && legs[i - 1].To < legs[i].To),
            "travel legs strictly ascending");
    auto bad = *loaded.Value;
    bad.Person.HomeZone = "location.nabapara.tall_flats_unit_a";
    SmRequire(!ValidateNpcScheduleContent(bad).empty(), "unit A rejected");
    bad = *loaded.Value;
    bad.Routes[0].LengthCm = 1;
    SmRequire(!ValidateNpcScheduleContent(bad).empty(), "undersized route refused");
    bad = *loaded.Value;
    bad.Paths.pop_back();
    SmRequire(!ValidateNpcScheduleContent(bad).empty(), "incomplete travel refused");
    bad = *loaded.Value;
    std::swap(bad.Travel.Legs[0], bad.Travel.Legs[1]);
    SmRequire(!ValidateNpcScheduleContent(bad).empty(), "unsorted travel legs refused");
    bad = *loaded.Value;
    for (auto& entry : bad.Person.Routine)
        if (entry.Zone == SmErrand && entry.StartMinute == 840) entry.StartMinute = 480;
    SmRequire(!ValidateNpcScheduleContent(bad).empty(), "errand outside the duty window refused");
    SmWeek(*loaded.Value);
    SmIndependence(*loaded.Value, *nirmal.Value);
    SmTalkAndSave();
    std::cout << "PerfectWorldSahanaLifeTests passed (" << SmAssertions << " assertions)\n";
}
