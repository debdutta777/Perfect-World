// Rupa Ghosh's conversation and world save (NR-01): her catalogue validates and selects exactly one node for every availability and
// minute; the remembered agreement plays only after the choice that sets its fact; the one paid choice is charged once per session;
// no line claims an outcome nothing records; and a world saved in the middle of her session (world save v8) round trips with no
// second charge. Standalone: it reuses the shared world-save builders (WorldSaveTestSupport.h). Her schedule is covered by
// RupaLifeTests.cpp.
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
int RgChecks = 0;
}

void Check(bool condition, const char* message)
{
    ++RgChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

#include "WorldSaveTestSupport.h"

namespace
{
constexpr std::string_view RgPerson = "person.rupa_ghosh";
constexpr std::string_view RgFact = "fact.rupa_ghosh.visit_promised";
constexpr std::string_view RgPromiseId = "choice.rupa_ghosh.promise_visit";
constexpr std::string_view RgDeclineId = "choice.rupa_ghosh.decline_visit";
constexpr std::string_view RgSitId = "choice.rupa_ghosh.sit_a_while";
constexpr std::string_view RgNotKeepId = "choice.rupa_ghosh.not_keep_you";
constexpr std::int32_t RgHour(std::int32_t hour, std::int32_t minute = 0) { return hour * 60 + minute; }
const ConversationCatalogue RgCatalogue = RupaConversationCatalogue();

std::string_view RgNodeAt(const ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(RgCatalogue, RgPerson, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

bool RgOffers(const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(RgCatalogue, state))
        if (view.Id == choiceId)
            return true;
    return false;
}

std::size_t RgAuthoredOffers(const ConversationState& state)
{
    std::size_t count = 0;
    for (const ConversationChoiceView& view : VisibleConversationChoices(RgCatalogue, state))
        count += view.bIsLeave ? 0 : 1;
    return count;
}

void RgOpen(ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    Check(OpenConversation(RgCatalogue, state, RgPerson, minute, availability) == ConversationError::None, "Rupa opens a conversation");
}

std::string RgLower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

ConversationState RgStateWithPromise()
{
    ConversationState state;
    RgOpen(state, RgHour(16, 30));
    Check(ChooseConversationChoice(RgCatalogue, state, RgPromiseId).Ok(), "agreeing to come by is accepted");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(HasConversationFact(state, RgFact), "agreeing set the fact");
    return state;
}

// ---------------------------------------------------------------- conversation

void RgConversationShape()
{
    Check(ValidateConversationCatalogue(RgCatalogue), "Rupa's catalogue validates");
    for (const ConversationNodeDef& node : RgCatalogue.Nodes)
        Check(node.PersonId == RgPerson, "every node is hers");
    for (const ConversationCatalogue& other : {HouseholdConversationCatalogue(), CareConversationCatalogue(), FaridaConversationCatalogue(),
                                               AninditaConversationCatalogue(), MiraConversationCatalogue(), SubrataConversationCatalogue()})
    {
        Check(ValidateConversationCatalogue(other), "the other catalogues still validate");
        for (const ConversationNodeDef& node : other.Nodes)
        {
            Check(FindConversationNode(RgCatalogue, node.Id) == nullptr, "no node id is shared with another catalogue");
            for (std::size_t i = 0; i < node.ChoiceCount; ++i)
                Check(FindConversationChoice(RgCatalogue, node.Choices[i].Id) == nullptr, "no choice id is shared with another catalogue");
        }
    }
    const auto registered = ConversationCatalogueForPerson(RgPerson);
    Check(registered.has_value() && registered->Nodes.data() == RgCatalogue.Nodes.data(), "the world finds her catalogue by person");
    const ConversationState none;
    const ConversationState promised = RgStateWithPromise();
    for (const ConversationState* state : {&none, &promised})
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int serving = 0;
                for (const ConversationNodeDef& node : RgCatalogue.Nodes)
                    serving += ConversationNodeServes(node, minute, *state, static_cast<PersonAvailability>(a)) ? 1 : 0;
                Check(serving == 1, "exactly one node serves every availability and minute");
            }
    // The day shape; a line is chosen by minute alone, so it is true on any day she can be spoken to.
    Check(RgNodeAt(none, 0) == "node.rupa_ghosh.late" && RgNodeAt(none, RgHour(5, 29)) == "node.rupa_ghosh.late", "late until 05:29");
    Check(RgNodeAt(none, RgHour(5, 30)) == "node.rupa_ghosh.morning" && RgNodeAt(none, RgHour(7, 59)) == "node.rupa_ghosh.morning", "05:30 to 07:59 the early morning");
    Check(RgNodeAt(none, RgHour(8)) == "node.rupa_ghosh.day" && RgNodeAt(none, RgHour(15, 59)) == "node.rupa_ghosh.day", "08:00 to 15:59 at home in the quiet");
    Check(RgNodeAt(none, RgHour(16)) == "node.rupa_ghosh.afternoon" && RgNodeAt(none, RgHour(17, 59)) == "node.rupa_ghosh.afternoon", "16:00 to 17:59 the quiet hour");
    Check(RgNodeAt(none, RgHour(18)) == "node.rupa_ghosh.supper" && RgNodeAt(none, RgHour(21, 59)) == "node.rupa_ghosh.supper", "18:00 to 21:59 supper and the evening");
    Check(RgNodeAt(none, RgHour(22)) == "node.rupa_ghosh.late", "22:00 is late");
    // Greeting, busy and asleep: busy and asleep decline at every hour and offer Leave only.
    for (const PersonAvailability availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 30)
        {
            const std::string_view id = RgNodeAt(promised, minute, availability);
            Check(id == (availability == PersonAvailability::Busy ? "node.rupa_ghosh.busy" : "node.rupa_ghosh.asleep"), "the decline node serves");
            const ConversationNodeDef* node = FindConversationNode(RgCatalogue, id);
            Check(node != nullptr && node->bDeclines && node->ChoiceCount == 0, "a decline offers nothing but Leave");
        }
        ConversationState declined;
        RgOpen(declined, RgHour(9), availability);
        Check(RgAuthoredOffers(declined) == 0 && VisibleConversationChoices(RgCatalogue, declined).size() == 1, "a declined session shows Leave only");
        Check(!RgOffers(declined, RgPromiseId) && !RgOffers(declined, RgSitId), "no authored choice is reachable while she is busy or asleep");
        Check(LeaveConversation(declined) == ConversationError::None, "Leave ends a declined session");
    }
    // Every awake window can be left, and Leave is always offered.
    for (const std::int32_t minute : {RgHour(2), RgHour(6), RgHour(9), RgHour(12), RgHour(16, 30), RgHour(19)})
    {
        ConversationState talk;
        RgOpen(talk, minute);
        bool leave = false;
        for (const ConversationChoiceView& view : VisibleConversationChoices(RgCatalogue, talk))
            leave = leave || view.bIsLeave;
        Check(leave && LeaveConversation(talk) == ConversationError::None && !talk.Open, "Leave is offered and ends the session");
    }
}

void RgConversationAgreement()
{
    const ConversationState none;
    const ConversationState promised = RgStateWithPromise();
    // No remembered line before the fact, at any hour or availability.
    for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            Check(RgLower(FindConversationNode(RgCatalogue, RgNodeAt(none, minute, static_cast<PersonAvailability>(a)))->Line).find("you said") == std::string::npos,
                  "no remembered line before the fact");
    // Declining and leaving never set it; reporting needs the agreement first.
    ConversationState state;
    RgOpen(state, RgHour(17));
    Check(RgOffers(state, RgPromiseId) && RgOffers(state, RgDeclineId) && !RgOffers(state, RgSitId), "she asks for the visit; sitting needs one first");
    Check(!ChooseConversationChoice(RgCatalogue, state, RgSitId).Ok() && !HasConversationFact(state, RgFact), "sitting before agreeing is refused");
    Check(ChooseConversationChoice(RgCatalogue, state, RgDeclineId).Ok() && RgOffers(state, RgPromiseId), "declining is free and the offer stands");
    Check(LeaveConversation(state) == ConversationError::None && !HasConversationFact(state, RgFact), "declining and leaving set no fact");
    // Agreeing sets exactly the fact and costs nothing.
    ConversationState agree;
    RgOpen(agree, RgHour(19));
    const ConversationChooseResult result = ChooseConversationChoice(RgCatalogue, agree, RgPromiseId);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == RgFact && agree.Receipts.empty(), "agreeing is free and sets exactly the fact");
    Check(result.NextNodeId == "node.rupa_ghosh.reply.promised", "she answers the agreement");
    Check(LeaveConversation(agree) == ConversationError::None, "leave");
    Check(RgNodeAt(promised, RgHour(16, 30)) == "node.rupa_ghosh.afternoon_remembered" && RgNodeAt(promised, RgHour(19)) == "node.rupa_ghosh.supper_remembered",
          "both evening windows now remember");
    Check(RgNodeAt(promised, RgHour(7)) == "node.rupa_ghosh.morning" && RgNodeAt(promised, RgHour(12)) == "node.rupa_ghosh.day" &&
              RgNodeAt(promised, RgHour(23)) == "node.rupa_ghosh.late", "the other windows are unchanged");
    Check(RgNodeAt(promised, RgHour(17), PersonAvailability::Busy) == "node.rupa_ghosh.busy", "busy still declines whatever she remembers");
    // Afterwards the agreement is not offered again; sitting is, at fifteen minutes, once per session, again in the next.
    ConversationState later = promised;
    RgOpen(later, RgHour(17));
    Check(!RgOffers(later, RgPromiseId) && RgOffers(later, RgSitId) && RgOffers(later, RgNotKeepId) && RgAuthoredOffers(later) == 2, "sit or move on");
    for (const ConversationChoiceView& view : VisibleConversationChoices(RgCatalogue, later))
        if (view.Id == RgSitId)
            Check(view.CostMinutes == 15, "sitting declares fifteen minutes");
    const ConversationChooseResult sat = ChooseConversationChoice(RgCatalogue, later, RgSitId);
    Check(sat.Ok() && sat.CostMinutes == 15 && sat.Session == later.SessionCount && sat.FactSet.empty(), "sitting is charged fifteen minutes, once, and sets no fact");
    Check(WasChoiceChargedThisSession(later, RgSitId) && sat.NextNodeId == "node.rupa_ghosh.reply.sat", "a receipt records the charge and she answers");
    const ConversationState charged = later;
    const ConversationChooseResult repeat = ChooseConversationChoice(RgCatalogue, later, RgSitId);
    Check(!repeat.Ok() && repeat.CostMinutes == 0 && later == charged, "a repeat in the same session is refused and changes nothing");
    Check(LeaveConversation(later) == ConversationError::None, "leave");
    RgOpen(later, RgHour(18, 45));
    Check(later.Receipts.empty() && RgOffers(later, RgSitId), "sitting is offered afresh in a later session");
    Check(ChooseConversationChoice(RgCatalogue, later, RgNotKeepId).Ok() && ChooseConversationChoice(RgCatalogue, later, RgSitId).CostMinutes == 0, "moving on is free and ends the choices");
    Check(LeaveConversation(later) == ConversationError::None, "leave");
    // Refusals change nothing.
    ConversationState refused;
    RgOpen(refused, RgHour(12));
    const ConversationState before = refused;
    for (const std::string_view bogus : {RgPromiseId, RgSitId, std::string_view{"choice.rupa_ghosh.nonsense"}, std::string_view{"choice.subrata_dey.report_step"}})
        Check(!ChooseConversationChoice(RgCatalogue, refused, bogus).Ok() && refused == before, "a choice not on this node is refused");
    Check(LeaveConversation(refused) == ConversationError::None && LeaveConversation(refused) != ConversationError::None, "leaving twice is refused");
    ConversationState stranger;
    Check(OpenConversation(RgCatalogue, stranger, "person.nobody", RgHour(9), PersonAvailability::Awake) != ConversationError::None, "an unknown person is refused");
    // The state survives a save.
    const std::string text = SerialiseConversationState(RgCatalogue, later);
    const ConversationStateParse parsed = DeserialiseConversationState(RgCatalogue, text);
    Check(!text.empty() && text.rfind("pw.conversation/2", 0) == 0 && parsed.Ok() && parsed.State == later, "the state serialises and reads back identically");
}

void RgConversationText()
{
    constexpr std::string_view Forbidden[] = {"buy", "bought", "price", "pay", "paid", "rupee", "rs.", "cost", "sold", "order", "fee", "money", "vote", "score"};
    constexpr std::string_view Claims[] = {"repaired", "fixed", "mended", "finished", "done", "swept", "replaced", "forgot", "i have written",
                                           "you came", "you helped", "you kept", "you promised me", "thank you for coming"};
    for (const ConversationNodeDef& node : RgCatalogue.Nodes)
    {
        std::vector<std::string_view> texts{node.Line};
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            texts.push_back(node.Choices[i].Text);
        for (const std::string_view text : texts)
        {
            const std::string lower = RgLower(text);
            for (const std::string_view word : Forbidden)
                Check(lower.find(word) == std::string::npos, "no money or scoring words");
            for (const std::string_view claim : Claims)
                Check(lower.find(claim) == std::string::npos, "no claim that the agreement was kept or any other change happened");
            for (const char c : text)
                Check(static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127, "plain ASCII text");
        }
        Check(node.Line.size() <= 120, "a line stays short");
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            Check(node.Choices[i].CostMinutes == 0 || node.Choices[i].Id == RgSitId, "no paid choice but sitting a while");
    }
    Check(FindConversationChoice(RgCatalogue, RgSitId)->CostMinutes == 15, "sitting costs fifteen minutes");
}

// ---------------------------------------------------------------- world save v8

void RgSaveMidSession()
{
    WorldSim live(97);
    const std::string person(RgPerson);
    // Monday 06:00 plus 640 minutes is 16:40, inside the quiet hour before supper.
    Check(live.Submit({CommandSource::Player, AdvanceTime{640}}).Accepted, "advance to the afternoon (16:40)");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open her first session");
    const auto beforeAgreement = live.State().Clock.Now();
    Check(live.Submit({CommandSource::Player, ConversationChoose{person, std::string(RgPromiseId), 1}}).Accepted, "agreeing is accepted");
    Check(live.State().Clock.Now() == beforeAgreement, "agreeing costs no time");
    Check(HasConversationFact(live.State().Conversations.at(person), RgFact), "the fact is set");
    Check(live.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "close the first session");
    Check(live.Submit({CommandSource::Player, AdvanceTime{10}}).Accepted, "ten minutes pass");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open her second session through the reducer");
    const auto before = live.State().Clock.Now();
    Check(live.Submit({CommandSource::Player, ConversationChoose{person, std::string(RgSitId), 2}}).Accepted, "sitting is accepted through the reducer");
    Check(live.State().Clock.Now() == before + 15, "fifteen minutes were charged through the world clock, once");
    const ConversationState& session = live.State().Conversations.at(person);
    Check(session.Open && session.NodeId == "node.rupa_ghosh.reply.sat" && session.Receipts.size() == 1, "the session is live on the reply with one receipt");
    const auto clockAfter = live.State().Clock.Now();
    Check(!live.Submit({CommandSource::Player, ConversationChoose{person, std::string(RgSitId), 2}}).Accepted, "a duplicate charge is refused");
    Check(live.State().Clock.Now() == clockAfter, "a refused duplicate costs nothing");

    WorldSaveData data = SaveOf(live, MakeCreation("parent_household"));
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(SerializeWorldSave(data), loaded);
    Check(result.Succeeded() && result.SourceVersion == 8 && result.HashVerified, "v8 mid-session round trip with Rupa is certified");
    CheckSame(data, loaded, "all persisted fields survive, including her session");
    const ConversationState& restored = loaded.World.Conversations.at(person);
    Check(restored.Open && WasChoiceChargedThisSession(restored, RgSitId) && HasConversationFact(restored, RgFact), "the receipt still guards the charge and the agreement survived");
    WorldSim resumed(loaded.World);
    Check(!resumed.Submit({CommandSource::Player, ConversationChoose{person, std::string(RgSitId), 2}}).Accepted, "after reload the same choice is still refused (no second charge)");
    for (WorldSim* sim : {&live, &resumed})
    {
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave the resumed session");
        Check(sim->Submit({CommandSource::Player, AdvanceTime{25}}).Accepted, "advance the continued world");
        Check(sim->Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open her next session");
        Check(sim->Submit({CommandSource::Player, ConversationChoose{person, std::string(RgSitId), 3}}).Accepted, "sitting is charged again in the next session");
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave again");
    }
    Check(HashWorldState(live.State()) == HashWorldState(resumed.State()), "reload continuation equals the uninterrupted run");
    const auto replay = ReplayCommandLog(97, live.Log());
    Check(replay.Complete && replay.Hash == HashWorldState(live.State()), "command replay is equivalent");
}

void RgSaveClosedSessionAndRefusals()
{
    WorldSaveData data = RichSave(98, 0);
    const std::string person(RgPerson);
    auto& state = data.World.Conversations[person];
    Check(OpenConversation(RgCatalogue, state, RgPerson, 17 * 60, PersonAvailability::Awake) == ConversationError::None, "open the afternoon session");
    Check(ChooseConversationChoice(RgCatalogue, state, RgPromiseId).Ok(), "agree to come by");
    Check(LeaveConversation(state) == ConversationError::None, "close the session");
    const std::string text = SerializeWorldSave(data);
    Check(!text.empty(), "a world with her closed session serialises");
    WorldSaveData loaded;
    Check(DeserializeWorldSave(text, loaded).HashVerified, "her closed session round trips certified");
    CheckSame(data, loaded, "her session history and agreement survive");
    Check(HasConversationFact(loaded.World.Conversations.at(person), RgFact), "the agreement is still remembered after loading");
    Check(ErrorOf(ReplaceOnce(text, "\"person.rupa_ghosh\":", "\"person.mira_sen\":")) == WorldSaveError::InvalidWorld, "her session cannot be filed under another person");
    Check(ErrorOf(ReplaceOnce(text, "\"person.rupa_ghosh\":", "\"person.nobody\":")) == WorldSaveError::InvalidWorld, "an unknown person is refused");
    WorldSaveData bad = data;
    bad.World.Conversations.at(person).Facts = {"fact.rupa_ghosh.unknown"};
    Check(SerializeWorldSave(bad).empty(), "the writer refuses a fact her catalogue does not define");
    for (std::size_t cut = 0; cut < text.size(); cut += 97)
        Check(ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "truncated saves are refused atomically");
}
} // namespace

int main()
{
    RgConversationShape();
    RgConversationAgreement();
    RgConversationText();
    RgSaveMidSession();
    RgSaveClosedSessionAndRefusals();
    std::cout << "PerfectWorldRupaConversationTests passed (" << RgChecks << " checks)\n";
    return 0;
}
