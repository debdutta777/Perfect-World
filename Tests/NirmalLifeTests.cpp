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
int NbAssertions = 0;
constexpr std::string_view NbPerson = "person.nirmal_bose";
constexpr std::string_view NbHome = "location.nabapara.tall_flats_unit_a";
constexpr std::string_view NbFact = "fact.nirmal_bose.book_evening_agreed";
void NbRequire(bool condition, const char* message)
{
    ++NbAssertions;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
GameInstant NbAt(int day, int minute) { return (day + 7) * MinutesPerDay + minute - 360; }

void NbWeek(const NpcSchedule& schedule)
{
    const DoorDef* door = FindDoor("door.nabapara.tall_flats_unit_a");
    NbRequire(door && door->Access == DoorAccess::Private && door->LocationId == NbHome, "unit A owns his private door");
    NbRequire(door->VisitingWindows.size() == 1 && door->VisitingWindows[0].OpenMinute == 1020 &&
        door->VisitingWindows[0].CloseMinute == 1230, "17:00-20:30 visiting window");
    for (int day = 0; day < 9; ++day)
        for (int minute = 0; minute < 1440; ++minute)
        {
            const auto at = NbAt(day, minute);
            const auto resolved = ResolveSchedule(schedule.Person, schedule.Travel, at);
            const auto intent = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
            NbRequire(resolved.Valid && !resolved.Squeezed && resolved.Status != ScheduleStatus::Idle && intent.Ok(), "complete unsqueezed week and rollover");
            if (minute < 360 || minute >= 1320)
                NbRequire(resolved.Zone == NbHome && intent.Intent.Availability == PersonAvailability::Asleep, "22:00-06:00 sleep at home");
            if ((minute >= 480 && minute < 720) || (minute >= 750 && minute < 960))
                NbRequire(resolved.Zone == "location.nabapara.nirmal_work" && intent.Intent.State == NpcState::Work &&
                    intent.Intent.Availability == PersonAvailability::Busy, "off-lane stocking shift each day");
            if (minute >= 1020 && minute < 1230)
            {
                NbRequire(resolved.Zone == NbHome && intent.Intent.Availability == PersonAvailability::Awake && intent.Intent.bTalkable,
                    "genuinely home and awake throughout visiting hours");
                DoorUseContext context;
                context.IsHostHome = true;
                NbRequire(EvaluateDoorKnock(*door, minute, context).Answer == DoorKnockAnswer::Admitted, "home knock admitted");
            }
        }
    for (int minute : {390, 730, 1030})
        NbRequire(ResolveSchedule(schedule.Person, schedule.Travel, NbAt(0, minute)).Doing == "break", "break spans author breakfast lunch and supper");
    DoorUseContext absent;
    NbRequire(EvaluateDoorKnock(*door, 1100, absent).Answer == DoorKnockAnswer::NoAnswer, "no absent admission");
    absent.IsHostHome = true;
    NbRequire(EvaluateDoorKnock(*door, 1230, absent).Answer == DoorKnockAnswer::NotReceiving, "end boundary excludes visitors");
    for (const auto at : {NbAt(0, 660), NbAt(0, 1080)})
    {
        const auto before = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
        const auto chat = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, true, false);
        const auto after = ResolveNpcIntentAt(schedule.Person, schedule.Travel, at, false, false);
        NbRequire(chat.Ok() && chat.Intent.State == NpcState::Chat && chat.Intent.ResumeState == before.Intent.State &&
            chat.Intent.ResumeWorkActionId == before.Intent.WorkActionId && before.Intent == after.Intent, "interruption preserves work or rest and Leave resumes");
    }
    auto whole = StartSchedulePerson(NbPerson, NbAt(0, 0));
    auto split = whole;
    const auto end = NbAt(8, 0);
    const auto full = AdvanceSchedulePerson(schedule.Person, schedule.Travel, whole, end);
    std::vector<ScheduleEvent> events;
    while (split.At < end)
    {
        const auto part = AdvanceSchedulePerson(schedule.Person, schedule.Travel, split, std::min(end, split.At + 37));
        NbRequire(part.Ok, "split advance");
        events.insert(events.end(), part.Events.begin(), part.Events.end());
    }
    NbRequire(full.Ok && whole == split && full.Events == events, "partition independent schedule events");
}

void NbTalkAndSave()
{
    const auto catalogue = NirmalConversationCatalogue();
    NbRequire(ValidateConversationCatalogue(catalogue), "catalogue valid");
    NbRequire(ConversationCatalogueForPerson(NbPerson).has_value(), "world routes Nirmal");
    ConversationState memory;
    for (int remembered = 0; remembered < 2; ++remembered)
    {
        for (int a = 0; a < static_cast<int>(PersonAvailabilityCount); ++a)
            for (int minute = 0; minute < 1440; ++minute)
            {
                int count = 0;
                for (const auto& node : catalogue.Nodes)
                    count += ConversationNodeServes(node, minute, memory, static_cast<PersonAvailability>(a));
                NbRequire(count == 1, "exactly one greeting or decline at every minute");
            }
        NbRequire(OpenConversation(catalogue, memory, NbPerson, 1080, PersonAvailability::Awake) == ConversationError::None, "open");
        if (!remembered) NbRequire(ChooseConversationChoice(catalogue, memory, "choice.nirmal_bose.agree").Ok(), "free agreement");
        NbRequire(LeaveConversation(memory) == ConversationError::None && HasConversationFact(memory, NbFact), "Leave retains agreement");
    }
    for (const auto availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        ConversationState state;
        NbRequire(OpenConversation(catalogue, state, NbPerson, 1080, availability) == ConversationError::None, "decline opens");
        NbRequire(VisibleConversationChoices(catalogue, state).size() == 1 &&
            !ChooseConversationChoice(catalogue, state, "choice.nirmal_bose.agree").Ok(), "decline offers Leave only");
        NbRequire(LeaveConversation(state) == ConversationError::None && state.Facts.empty(), "decline creates no memory");
    }
    CharacterCreator creator;
    NbRequire(creator.ApplyPreset("balanced_scholar").Succeeded() && creator.StartLife().Succeeded(), "started creation");
    WorldSaveData data;
    NbRequire(BeginWorldFromCreation(creator.CaptureSave(), 9502, data).Succeeded(), "new world");
    WorldSim live(data.World);
    const std::string person(NbPerson);
    NbRequire(live.Submit({CommandSource::Player, AdvanceTime{720}}).Accepted, "18:00");
    NbRequire(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open reducer session");
    NbRequire(live.Submit({CommandSource::Player, ConversationChoose{person, "choice.nirmal_bose.agree", 1}}).Accepted, "agreement through reducer");
    const auto before = live.State().Clock.Now();
    NbRequire(live.Submit({CommandSource::Player, ConversationChoose{person, "choice.nirmal_bose.books", 1}}).Accepted, "paid choice");
    NbRequire(live.State().Clock.Now() == before + 10, "ten minute charge");
    data.World = live.State();
    data.Player = {"zone.street.nabapara_lane", {400, 300, 85}, 12345, -1200, CameraMode::ThirdPerson};
    const std::string text = SerializeWorldSave(data);
    WorldSaveData loaded;
    NbRequire(!text.empty() && DeserializeWorldSave(text, loaded).HashVerified, "certified mid-session world round trip");
    NbRequire(loaded.Player == data.Player && loaded.World.Conversations == data.World.Conversations, "placement memory and receipt retained");
    WorldSim resumed(loaded.World);
    NbRequire(!resumed.Submit({CommandSource::Player, ConversationChoose{person, "choice.nirmal_bose.books", 1}}).Accepted &&
        resumed.State().Clock.Now() == before + 10, "no duplicate charge after restore");
    NbRequire(resumed.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "restore closes session");
    NbRequire(HasConversationFact(resumed.State().Conversations.at(person), NbFact) &&
        resumed.State().Conversations.at(person).Receipts.size() == 1, "closing session preserves history");
    NbRequire(resumed.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted &&
        resumed.State().Conversations.at(person).NodeId == "node.nirmal_bose.remembered", "new session remembers agreement");
    auto bad = loaded;
    bad.World.Conversations.at(person).Facts.push_back("fact.nirmal_bose.invented");
    NbRequire(SerializeWorldSave(bad).empty(), "invented memory refused");
}
}

int main(int argc, char** argv)
{
    NbRequire(argc == 2, "content path supplied");
    std::ifstream input(argv[1]);
    NbRequire(input.good(), "content exists");
    const std::string text{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const auto result = LoadContent({{"/npc_schedule/npc_schedule.nirmal_bose.json", text}});
    for (const auto& issue : result.Issues) std::cerr << issue.Message << '\n';
    NbRequire(result.Ok(), "registered content loads");
    const auto loaded = LoadNpcSchedule(result.Db, "npc_schedule.nirmal_bose");
    if (!loaded.Ok()) std::cerr << loaded.Error << '\n';
    NbRequire(loaded.Ok() && ValidateNpcScheduleContent(*loaded.Value).empty(), "typed schedule loads");
    auto bad = *loaded.Value;
    bad.Person.HomeZone = "location.nabapara.tall_flats_unit_b";
    NbRequire(!ValidateNpcScheduleContent(bad).empty(), "unit B rejected");
    bad = *loaded.Value;
    bad.Routes[0].LengthCm = 1;
    NbRequire(!ValidateNpcScheduleContent(bad).empty(), "undersized route refused");
    bad = *loaded.Value;
    bad.Paths.pop_back();
    NbRequire(!ValidateNpcScheduleContent(bad).empty(), "incomplete travel refused");
    NbWeek(*loaded.Value);
    NbTalkAndSave();
    std::cout << "PerfectWorldNirmalLifeTests passed (" << NbAssertions << " assertions)\n";
}
