// Subrata Dey's conversation and world save (UP-03a): his catalogue validates and selects exactly one node for every availability
// and minute; the remembered agreement plays only after the choice that sets its fact; paid choices are charged once per session;
// no line claims a repair or a result nothing records; and a world saved in the middle of his session (world save v8) round trips
// with no second charge. Standalone: it reuses the shared world-save builders (WorldSaveTestSupport.h). His schedule is covered by
// SubrataLifeTests.cpp.
#include "Conversation.h"

#include "WorldHash.h"
#include "WorldReducer.h"
#include "WorldSave.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int SbChecks = 0;
}

void Check(bool condition, const char* message)
{
    ++SbChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

#include "WorldSaveTestSupport.h"

namespace
{
constexpr std::string_view SbPerson = "person.subrata_dey";

// ---------------------------------------------------------------- conversation

constexpr std::string_view SbFact = "fact.subrata_dey.step_promised";
constexpr std::string_view SbPromiseId = "choice.subrata_dey.promise_step";
constexpr std::string_view SbDeclineId = "choice.subrata_dey.decline_promise";
constexpr std::string_view SbReportId = "choice.subrata_dey.report_step";
constexpr std::string_view SbNotKeepId = "choice.subrata_dey.not_keep_you";
constexpr std::int32_t SbHour(std::int32_t hour, std::int32_t minute = 0) { return hour * 60 + minute; }
const ConversationCatalogue SbCatalogue = SubrataConversationCatalogue();

std::string_view SbNodeAt(const ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(SbCatalogue, SbPerson, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

bool SbOffers(const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(SbCatalogue, state))
        if (view.Id == choiceId)
            return true;
    return false;
}

std::size_t SbAuthoredOffers(const ConversationState& state)
{
    std::size_t count = 0;
    for (const ConversationChoiceView& view : VisibleConversationChoices(SbCatalogue, state))
        count += view.bIsLeave ? 0 : 1;
    return count;
}

void SbOpen(ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    Check(OpenConversation(SbCatalogue, state, SbPerson, minute, availability) == ConversationError::None, "Subrata opens a conversation");
}

std::string SbLower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

ConversationState SbStateWithPromise()
{
    ConversationState state;
    SbOpen(state, SbHour(8, 30));
    Check(ChooseConversationChoice(SbCatalogue, state, SbPromiseId).Ok(), "agreeing to tell him is accepted");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(HasConversationFact(state, SbFact), "agreeing set the fact");
    return state;
}

void SbConversationShape()
{
    Check(ValidateConversationCatalogue(SbCatalogue), "Subrata's catalogue validates");
    for (const ConversationNodeDef& node : SbCatalogue.Nodes)
        Check(node.PersonId == SbPerson, "every node is his");
    for (const ConversationCatalogue& other : {HouseholdConversationCatalogue(), CareConversationCatalogue(), FaridaConversationCatalogue(),
                                               AninditaConversationCatalogue(), MiraConversationCatalogue()})
    {
        Check(ValidateConversationCatalogue(other), "the other catalogues still validate");
        for (const ConversationNodeDef& node : other.Nodes)
        {
            Check(FindConversationNode(SbCatalogue, node.Id) == nullptr, "no node id is shared with another catalogue");
            for (std::size_t i = 0; i < node.ChoiceCount; ++i)
                Check(FindConversationChoice(SbCatalogue, node.Choices[i].Id) == nullptr, "no choice id is shared with another catalogue");
        }
    }
    const auto registered = ConversationCatalogueForPerson(SbPerson);
    Check(registered.has_value() && registered->Nodes.data() == SbCatalogue.Nodes.data(), "the world finds his catalogue by person");
    const ConversationState none;
    const ConversationState promised = SbStateWithPromise();
    for (const ConversationState* state : {&none, &promised})
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int serving = 0;
                for (const ConversationNodeDef& node : SbCatalogue.Nodes)
                    serving += ConversationNodeServes(node, minute, *state, static_cast<PersonAvailability>(a)) ? 1 : 0;
                Check(serving == 1, "exactly one node serves every availability and minute");
            }
    // The day shape; a line is chosen by minute alone, so it is true on any day he can be spoken to.
    Check(SbNodeAt(none, 0) == "node.subrata_dey.late" && SbNodeAt(none, SbHour(5, 14)) == "node.subrata_dey.late", "late until 05:14");
    Check(SbNodeAt(none, SbHour(5, 15)) == "node.subrata_dey.early" && SbNodeAt(none, SbHour(7, 59)) == "node.subrata_dey.early", "05:15 to 07:59 starting the day");
    Check(SbNodeAt(none, SbHour(8)) == "node.subrata_dey.board_morning" && SbNodeAt(none, SbHour(9, 49)) == "node.subrata_dey.board_morning", "08:00 to 09:49 the repair list");
    Check(SbNodeAt(none, SbHour(9, 50)) == "node.subrata_dey.midday_home" && SbNodeAt(none, SbHour(13, 59)) == "node.subrata_dey.midday_home", "09:50 to 13:59 at home");
    Check(SbNodeAt(none, SbHour(14)) == "node.subrata_dey.board_afternoon" && SbNodeAt(none, SbHour(17, 29)) == "node.subrata_dey.board_afternoon", "14:00 to 17:29 the repair list");
    Check(SbNodeAt(none, SbHour(17, 30)) == "node.subrata_dey.evening_home" && SbNodeAt(none, SbHour(21, 29)) == "node.subrata_dey.evening_home", "17:30 to 21:29 at home");
    Check(SbNodeAt(none, SbHour(21, 30)) == "node.subrata_dey.late", "21:30 is late");
    const ConversationNodeDef* greeting = FindConversationNode(SbCatalogue, "node.subrata_dey.board_morning");
    Check(greeting != nullptr && greeting->Line == "The repair list is up. Add the location if I missed something.", "the plan's sample line, word for word");
    // Greeting, busy and asleep: busy and asleep decline at every hour and offer Leave only.
    for (const PersonAvailability availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 30)
        {
            const std::string_view id = SbNodeAt(promised, minute, availability);
            Check(id == (availability == PersonAvailability::Busy ? "node.subrata_dey.busy" : "node.subrata_dey.asleep"), "the decline node serves");
            const ConversationNodeDef* node = FindConversationNode(SbCatalogue, id);
            Check(node != nullptr && node->bDeclines && node->ChoiceCount == 0, "a decline offers nothing but Leave");
        }
        ConversationState declined;
        SbOpen(declined, SbHour(9), availability);
        Check(SbAuthoredOffers(declined) == 0 && VisibleConversationChoices(SbCatalogue, declined).size() == 1, "a declined session shows Leave only");
        Check(!SbOffers(declined, SbPromiseId) && !SbOffers(declined, SbReportId), "no authored choice is reachable while he is busy or asleep");
        Check(LeaveConversation(declined) == ConversationError::None, "Leave ends a declined session");
    }
    // Every awake window can be left, and Leave is always offered.
    for (const std::int32_t minute : {SbHour(2), SbHour(6), SbHour(9), SbHour(12), SbHour(15), SbHour(19)})
    {
        ConversationState talk;
        SbOpen(talk, minute);
        bool leave = false;
        for (const ConversationChoiceView& view : VisibleConversationChoices(SbCatalogue, talk))
            leave = leave || view.bIsLeave;
        Check(leave && LeaveConversation(talk) == ConversationError::None && !talk.Open, "Leave is offered and ends the session");
    }
}

void SbConversationAgreement()
{
    const ConversationState none;
    const ConversationState promised = SbStateWithPromise();
    // No remembered line before the fact, at any hour or availability.
    for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            Check(SbLower(FindConversationNode(SbCatalogue, SbNodeAt(none, minute, static_cast<PersonAvailability>(a)))->Line).find("you said") == std::string::npos,
                  "no remembered line before the fact");
    // Declining, reporting-less leaving and walking away never set it.
    ConversationState state;
    SbOpen(state, SbHour(15));
    Check(SbOffers(state, SbPromiseId) && SbOffers(state, SbDeclineId) && !SbOffers(state, SbReportId), "he asks for the promise; reporting needs one first");
    Check(!ChooseConversationChoice(SbCatalogue, state, SbReportId).Ok() && !HasConversationFact(state, SbFact), "reporting a step before agreeing is refused");
    Check(ChooseConversationChoice(SbCatalogue, state, SbDeclineId).Ok() && SbOffers(state, SbPromiseId), "declining is free and the offer stands");
    Check(LeaveConversation(state) == ConversationError::None && !HasConversationFact(state, SbFact), "declining and leaving set no fact");
    // Agreeing sets exactly the fact and costs nothing.
    ConversationState agree;
    SbOpen(agree, SbHour(15));
    const ConversationChooseResult result = ChooseConversationChoice(SbCatalogue, agree, SbPromiseId);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == SbFact && agree.Receipts.empty(), "agreeing is free and sets exactly the fact");
    Check(result.NextNodeId == "node.subrata_dey.reply.promised", "he answers the agreement");
    Check(LeaveConversation(agree) == ConversationError::None, "leave");
    Check(SbNodeAt(promised, SbHour(8, 30)) == "node.subrata_dey.board_morning_remembered" &&
              SbNodeAt(promised, SbHour(15)) == "node.subrata_dey.board_afternoon_remembered", "both repair-list windows now remember");
    Check(SbNodeAt(promised, SbHour(7)) == "node.subrata_dey.early" && SbNodeAt(promised, SbHour(12)) == "node.subrata_dey.midday_home" &&
              SbNodeAt(promised, SbHour(19)) == "node.subrata_dey.evening_home", "the other windows are unchanged");
    Check(SbNodeAt(promised, SbHour(15), PersonAvailability::Busy) == "node.subrata_dey.busy", "busy still declines whatever he remembers");
    // Afterwards the promise is not offered again; reporting is, at ten minutes, once per session, again in the next.
    ConversationState later = promised;
    SbOpen(later, SbHour(15));
    Check(!SbOffers(later, SbPromiseId) && SbOffers(later, SbReportId) && SbOffers(later, SbNotKeepId) && SbAuthoredOffers(later) == 2, "report or move on");
    for (const ConversationChoiceView& view : VisibleConversationChoices(SbCatalogue, later))
        if (view.Id == SbReportId)
            Check(view.CostMinutes == 10, "reporting declares ten minutes");
    const ConversationChooseResult report = ChooseConversationChoice(SbCatalogue, later, SbReportId);
    Check(report.Ok() && report.CostMinutes == 10 && report.Session == later.SessionCount && report.FactSet.empty(), "reporting is charged ten minutes, once, and sets no fact");
    Check(WasChoiceChargedThisSession(later, SbReportId) && report.NextNodeId == "node.subrata_dey.reply.reported", "a receipt records the charge and he answers");
    const ConversationState charged = later;
    const ConversationChooseResult repeat = ChooseConversationChoice(SbCatalogue, later, SbReportId);
    Check(!repeat.Ok() && repeat.CostMinutes == 0 && later == charged, "a repeat in the same session is refused and changes nothing");
    Check(LeaveConversation(later) == ConversationError::None, "leave");
    SbOpen(later, SbHour(8, 45));
    Check(later.Receipts.empty() && SbOffers(later, SbReportId), "the report is offered afresh in a later session");
    Check(ChooseConversationChoice(SbCatalogue, later, SbNotKeepId).Ok() && ChooseConversationChoice(SbCatalogue, later, SbReportId).CostMinutes == 0, "moving on is free and ends the choices");
    Check(LeaveConversation(later) == ConversationError::None, "leave");
    // Refusals change nothing.
    ConversationState refused;
    SbOpen(refused, SbHour(12));
    const ConversationState before = refused;
    for (const std::string_view bogus : {SbPromiseId, SbReportId, std::string_view{"choice.subrata_dey.nonsense"}, std::string_view{"choice.mira_sen.walk_to_corner"}})
        Check(!ChooseConversationChoice(SbCatalogue, refused, bogus).Ok() && refused == before, "a choice not on this node is refused");
    Check(LeaveConversation(refused) == ConversationError::None && LeaveConversation(refused) != ConversationError::None, "leaving twice is refused");
    ConversationState stranger;
    Check(OpenConversation(SbCatalogue, stranger, "person.nobody", SbHour(9), PersonAvailability::Awake) != ConversationError::None, "an unknown person is refused");
    // The state survives a save.
    const std::string text = SerialiseConversationState(SbCatalogue, later);
    const ConversationStateParse parsed = DeserialiseConversationState(SbCatalogue, text);
    Check(!text.empty() && text.rfind("pw.conversation/2", 0) == 0 && parsed.Ok() && parsed.State == later, "the state serialises and reads back identically");
}

void SbConversationText()
{
    constexpr std::string_view Forbidden[] = {"buy", "bought", "price", "pay", "paid", "rupee", "rs.", "cost", "sold", "order", "fee", "money", "vote", "score"};
    constexpr std::string_view Claims[] = {"repaired", "fixed", "mended", "finished", "done", "swept", "replaced", "forgot", "i have written"};
    for (const ConversationNodeDef& node : SbCatalogue.Nodes)
    {
        std::vector<std::string_view> texts{node.Line};
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            texts.push_back(node.Choices[i].Text);
        for (const std::string_view text : texts)
        {
            const std::string lower = SbLower(text);
            for (const std::string_view word : Forbidden)
                Check(lower.find(word) == std::string::npos, "no money or scoring words");
            for (const std::string_view claim : Claims)
                Check(lower.find(claim) == std::string::npos, "no claim that a repair or other change happened");
            for (const char c : text)
                Check(static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127, "plain ASCII text");
        }
        Check(node.Line.size() <= 110, "a line stays short");
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            Check(node.Choices[i].CostMinutes == 0 || node.Choices[i].Id == SbReportId, "no paid choice but the report");
    }
    Check(FindConversationChoice(SbCatalogue, SbReportId)->CostMinutes == 10, "the report costs ten minutes");
}

// ---------------------------------------------------------------- world save v8

void SbSaveMidSession()
{
    WorldSim live(95);
    const std::string person(SbPerson);
    // Monday 06:00 plus 640 minutes is 16:40, inside the afternoon repair-list window.
    Check(live.Submit({CommandSource::Player, AdvanceTime{640}}).Accepted, "advance to the afternoon (16:40)");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open his first session");
    const auto beforeAgreement = live.State().Clock.Now();
    Check(live.Submit({CommandSource::Player, ConversationChoose{person, std::string(SbPromiseId), 1}}).Accepted, "agreeing is accepted");
    Check(live.State().Clock.Now() == beforeAgreement, "agreeing costs no time");
    Check(HasConversationFact(live.State().Conversations.at(person), SbFact), "the fact is set");
    Check(live.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "close the first session");
    Check(live.Submit({CommandSource::Player, AdvanceTime{10}}).Accepted, "ten minutes pass");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open his second session through the reducer");
    const auto before = live.State().Clock.Now();
    Check(live.Submit({CommandSource::Player, ConversationChoose{person, std::string(SbReportId), 2}}).Accepted, "the report is accepted through the reducer");
    Check(live.State().Clock.Now() == before + 10, "ten minutes were charged through the world clock, once");
    const ConversationState& session = live.State().Conversations.at(person);
    Check(session.Open && session.NodeId == "node.subrata_dey.reply.reported" && session.Receipts.size() == 1, "the session is live on the reply with one receipt");
    const auto clockAfter = live.State().Clock.Now();
    Check(!live.Submit({CommandSource::Player, ConversationChoose{person, std::string(SbReportId), 2}}).Accepted, "a duplicate charge is refused");
    Check(live.State().Clock.Now() == clockAfter, "a refused duplicate costs nothing");

    WorldSaveData data = SaveOf(live, MakeCreation("parent_household"));
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(SerializeWorldSave(data), loaded);
    Check(result.Succeeded() && result.SourceVersion == 8 && result.HashVerified, "v8 mid-session round trip with Subrata is certified");
    CheckSame(data, loaded, "all persisted fields survive, including his session");
    const ConversationState& restored = loaded.World.Conversations.at(person);
    Check(restored.Open && WasChoiceChargedThisSession(restored, SbReportId) && HasConversationFact(restored, SbFact), "the receipt still guards the charge and the agreement survived");
    WorldSim resumed(loaded.World);
    Check(!resumed.Submit({CommandSource::Player, ConversationChoose{person, std::string(SbReportId), 2}}).Accepted, "after reload the same choice is still refused (no second charge)");
    for (WorldSim* sim : {&live, &resumed})
    {
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave the resumed session");
        Check(sim->Submit({CommandSource::Player, AdvanceTime{25}}).Accepted, "advance the continued world");
        Check(sim->Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open his next session");
        Check(sim->Submit({CommandSource::Player, ConversationChoose{person, std::string(SbReportId), 3}}).Accepted, "the report is charged again in the next session");
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave again");
    }
    Check(HashWorldState(live.State()) == HashWorldState(resumed.State()), "reload continuation equals the uninterrupted run");
    const auto replay = ReplayCommandLog(95, live.Log());
    Check(replay.Complete && replay.Hash == HashWorldState(live.State()), "command replay is equivalent");
}

void SbSaveClosedSessionAndRefusals()
{
    WorldSaveData data = RichSave(96, 0);
    const std::string person(SbPerson);
    auto& state = data.World.Conversations[person];
    Check(OpenConversation(SbCatalogue, state, SbPerson, 8 * 60, PersonAvailability::Awake) == ConversationError::None, "open the morning session");
    Check(ChooseConversationChoice(SbCatalogue, state, SbPromiseId).Ok(), "agree to tell him");
    Check(LeaveConversation(state) == ConversationError::None, "close the session");
    const std::string text = SerializeWorldSave(data);
    Check(!text.empty(), "a world with his closed session serialises");
    WorldSaveData loaded;
    Check(DeserializeWorldSave(text, loaded).HashVerified, "his closed session round trips certified");
    CheckSame(data, loaded, "his session history and agreement survive");
    Check(HasConversationFact(loaded.World.Conversations.at(person), SbFact), "the agreement is still remembered after loading");
    Check(ErrorOf(ReplaceOnce(text, "\"person.subrata_dey\":", "\"person.mira_sen\":")) == WorldSaveError::InvalidWorld, "his session cannot be filed under another person");
    Check(ErrorOf(ReplaceOnce(text, "\"person.subrata_dey\":", "\"person.nobody\":")) == WorldSaveError::InvalidWorld, "an unknown person is refused");
    WorldSaveData bad = data;
    bad.World.Conversations.at(person).Facts = {"fact.subrata_dey.unknown"};
    Check(SerializeWorldSave(bad).empty(), "the writer refuses a fact his catalogue does not define");
    for (std::size_t cut = 0; cut < text.size(); cut += 97)
        Check(ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "truncated saves are refused atomically");
}
} // namespace

int main()
{
    SbConversationShape();
    SbConversationAgreement();
    SbConversationText();
    SbSaveMidSession();
    SbSaveClosedSessionAndRefusals();
    std::cout << "PerfectWorldSubrataConversationTests passed (" << SbChecks << " checks)\n";
    return 0;
}
