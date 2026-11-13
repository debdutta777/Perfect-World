#include "LaneAmbient.h"

#include "ContentRegistry.h"
#include "Conversation.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "WorldReducer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace
{
using namespace PerfectWorld;
int AmAmbientAssertions = 0;
void AmAmbientRequire(bool Value, const char* Message)
{
    ++AmAmbientAssertions;
    if (!Value) { std::cerr << "FAIL: " << Message << '\n'; std::exit(1); }
}
GameInstant AmAmbientAt(int Day, int Minute) { return (Day + 7) * MinutesPerDay + Minute - 360; }
void AmAmbientWeek(const NpcSchedule& Schedule)
{
    int Travels = 0;
    for (int Day = 0; Day < 8; ++Day)
    {
        for (int Minute = 0; Minute < 1440; ++Minute)
        {
            const auto At = AmAmbientAt(Day, Minute);
            const auto Resolved = ResolveSchedule(Schedule.Person, Schedule.Travel, At);
            AmAmbientRequire(Resolved.Valid && !Resolved.Squeezed && Resolved.Status != ScheduleStatus::Idle, "complete unsqueezed week");
            const auto Normal = ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, At, false, false);
            AmAmbientRequire(Normal.Ok(), "known role at every minute");
            if (Minute < 360 || Minute >= 1320)
                AmAmbientRequire(Normal.Intent.State == NpcState::Sleep && Normal.Intent.AnchorId == Schedule.Person.HomeZone, "off-lane night");
            if (Normal.Intent.State != NpcState::Travel) { continue; }
            ++Travels;
            const auto Chat = ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, At, true, false);
            const auto Blocked = ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, At, false, true);
            AmAmbientRequire(Chat.Ok() && Chat.Intent.State == NpcState::Chat && Chat.Intent.ResumeState == NpcState::Travel &&
                Chat.Intent.RouteId == Normal.Intent.RouteId, "Talk suspends route context");
            AmAmbientRequire(Blocked.Ok() && Blocked.Intent.State == NpcState::Yield, "obstruction holds route");
            AmAmbientRequire(ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, At, false, false).Intent == Normal.Intent, "Leave/unblock resumes without simulation drift");
        }
    }
    AmAmbientRequire(Travels > 0, "visible walks exist");
    auto Whole = StartSchedulePerson(Schedule.Person.PersonId, AmAmbientAt(0, 0));
    auto Parts = Whole;
    const auto Full = AdvanceSchedulePerson(Schedule.Person, Schedule.Travel, Whole, AmAmbientAt(7, 0));
    std::vector<ScheduleEvent> Events;
    while (Parts.At < Whole.At)
    {
        const auto Part = AdvanceSchedulePerson(Schedule.Person, Schedule.Travel, Parts, std::min(Whole.At, Parts.At + 47));
        AmAmbientRequire(Part.Ok, "bounded schedule advance");
        Events.insert(Events.end(), Part.Events.begin(), Part.Events.end());
    }
    AmAmbientRequire(Full.Ok && Whole == Parts && Full.Events == Events, "boundary updates equal whole-week advance");
}
void AmAmbientTalk(std::string_view Person)
{
    const auto Catalogue = ConversationCatalogueForPerson(Person);
    AmAmbientRequire(Catalogue.has_value(), "world routes person to catalogue");
    for (auto Availability : {PersonAvailability::Awake, PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        for (int Minute = 0; Minute < 1440; ++Minute)
            AmAmbientRequire(SelectConversationNode(*Catalogue, Person, Minute, {}, Availability) != nullptr, "every availability and minute has a line");
        ConversationState State;
        AmAmbientRequire(OpenConversation(*Catalogue, State, Person, 540, Availability) == ConversationError::None, "session opens");
        const auto Choices = VisibleConversationChoices(*Catalogue, State);
        AmAmbientRequire(!Choices.empty() && Choices.back().bIsLeave, "Leave offered");
        if (Availability == PersonAvailability::Awake)
        {
            const auto Choice = Choices.front().Id;
            const auto Paid = ChooseConversationChoice(*Catalogue, State, Choice);
            AmAmbientRequire(Paid.Ok() && Paid.CostMinutes == 2, "short authored chat");
            const auto Saved = SerialiseConversationState(*Catalogue, State);
            const auto Loaded = DeserialiseConversationState(*Catalogue, Saved);
            AmAmbientRequire(Loaded.Ok() && Loaded.State == State, "receipt survives text round trip");
            const auto Repeat = ChooseConversationChoice(*Catalogue, State, Choice);
            AmAmbientRequire(!Repeat.Ok() && Repeat.CostMinutes == 0, "no duplicate charge");
        }
        AmAmbientRequire(LeaveConversation(State) == ConversationError::None && !State.Open, "Leave ends session");
    }
    WorldSim Sim(403);
    Command Open;
    Open.Payload = ConversationOpen{std::string(Person), PersonAvailability::Awake};
    AmAmbientRequire(Sim.Submit(Open).Accepted, "world command opens ambient conversation");
    const auto Session = Sim.State().Conversations.at(std::string(Person)).SessionCount;
    const auto Choice = VisibleConversationChoices(*Catalogue, Sim.State().Conversations.at(std::string(Person))).front().Id;
    const auto Before = Sim.State().Clock.Now();
    Command Pick;
    Pick.Payload = ConversationChoose{std::string(Person), std::string(Choice), Session};
    AmAmbientRequire(Sim.Submit(Pick).Accepted && Sim.State().Clock.Now() == Before + 2, "world charges exactly two minutes");
    AmAmbientRequire(!Sim.Submit(Pick).Accepted && Sim.State().Clock.Now() == Before + 2, "world retry cannot charge twice");
    Command Leave;
    Leave.Payload = ConversationLeave{std::string(Person)};
    AmAmbientRequire(Sim.Submit(Leave).Accepted && OpenConversationPersonId(Sim.State()).empty(), "world Leave closes session");
}
}
int main(int Argc, char** Argv)
{
    using namespace PerfectWorld;
    AmAmbientRequire(Argc == 2, "content directory argument");
    std::vector<ContentFile> Files;
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Argv[1]))
    {
        if (!Entry.is_regular_file() || Entry.path().extension() != ".json") { continue; }
        std::ifstream Input(Entry.path());
        Files.push_back({Entry.path().generic_string(), {std::istreambuf_iterator<char>(Input), {}}});
    }
    const auto Loaded = LoadContent(Files);
    for (const auto& Issue : Loaded.Issues) { std::cerr << Issue.File << ": " << Issue.Message << '\n'; }
    AmAmbientRequire(Loaded.Ok(), "entire production content database loads");
    std::cout << "pwcontent: " << Files.size() << " files, " << Loaded.Db.Count() << " records, "
              << Loaded.Db.CountOfType("npc_schedule") << " npc_schedule, " << Loaded.Issues.size() << " issues\n";
    AmAmbientRequire(ValidateConversationCatalogue(AmbientConversationCatalogue()), "ambient catalogue valid");
    for (const auto Person : AmbientPeople)
    {
        const std::string ScheduleId = "npc_schedule." + std::string(Person.substr(7));
        const auto LoadedSchedule = LoadNpcSchedule(Loaded.Db, ScheduleId);
        if (!LoadedSchedule.Ok()) { std::cerr << LoadedSchedule.Error << '\n'; }
        AmAmbientRequire(LoadedSchedule.Ok(), "typed ambient schedule");
        const auto& Schedule = *LoadedSchedule.Value;
        for (std::size_t I = 1; I < Schedule.Travel.Legs.size(); ++I)
        {
            const auto& A = Schedule.Travel.Legs[I-1];
            const auto& B = Schedule.Travel.Legs[I];
            AmAmbientRequire(std::pair(A.From, A.To) < std::pair(B.From, B.To), "strict ascending travel pairs");
        }
        auto Bad = Schedule;
        Bad.Routes.front().LengthCm = 1;
        AmAmbientRequire(!ValidateNpcScheduleContent(Bad).empty(), "undersized route refused");
        Bad = Schedule;
        Bad.Travel.Legs.pop_back();
        AmAmbientRequire(!ValidateNpcScheduleContent(Bad).empty(), "missing all-pairs leg refused");
        AmAmbientWeek(Schedule);
        AmAmbientTalk(Person);
    }
    for (const auto& Route : LaneAmbientRoutes())
    {
        double Length = 0;
        for (std::size_t I = 1; I < Route.Count; ++I)
        {
            const auto A = Route.Points[I-1], B = Route.Points[I];
            Length += std::hypot(double(B.X-A.X), double(B.Y-A.Y), double(B.Height-A.Height));
        }
        AmAmbientRequire(Length <= Route.LengthCm && Route.LengthCm - Length < 100, "geometry fits authored conservative budget");
        const auto Schedule = LoadNpcSchedule(Loaded.Db, "npc_schedule." + std::string(Route.Person.substr(7)));
        const auto Found = std::find_if(Schedule.Value->Routes.begin(), Schedule.Value->Routes.end(),
            [&](const auto& R) { return R.Id == Route.Id; });
        AmAmbientRequire(Found != Schedule.Value->Routes.end() && Found->LengthCm == Route.LengthCm, "bindings and JSON lengths agree");
    }
    std::cout << "PASS: " << AmAmbientAssertions << " assertions\n";
}
