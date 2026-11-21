// Farida Qadri's conversation (order CF; LANE_LIFE_PLAN section B): her catalogue validates, every availability and minute selects
// exactly one node that matches her day, the remembered-relationship line plays only after the player has actually asked about the
// bench in conversation, sitting down with a cup of tea costs ten minutes charged once per session (and again in a later one),
// declining costs nothing, and no line promises a purchase or a result nothing records.
#include "Conversation.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int Checks = 0;

void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr std::string_view Farida = "person.farida_qadri";
constexpr std::string_view FactBenchAsked = "fact.farida_qadri.bench_asked";
constexpr std::string_view SitWithTea = "choice.farida_qadri.sit_with_tea";
constexpr std::string_view DeclineTea = "choice.farida_qadri.decline_tea";
constexpr std::string_view AskBench = "choice.farida_qadri.ask_bench";

constexpr std::int32_t Hour(std::int32_t hour, std::int32_t minute = 0) { return hour * 60 + minute; }

const ConversationCatalogue Catalogue = FaridaConversationCatalogue();

std::string_view NodeAt(const ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(Catalogue, Farida, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

std::string_view LineOf(std::string_view nodeId)
{
    const ConversationNodeDef* node = FindConversationNode(Catalogue, nodeId);
    Check(node != nullptr, "the node exists");
    return node != nullptr ? node->Line : std::string_view{};
}

bool Offers(const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(Catalogue, state))
    {
        if (view.Id == choiceId)
            return true;
    }
    return false;
}

std::size_t AuthoredOffers(const ConversationState& state)
{
    std::size_t count = 0;
    for (const ConversationChoiceView& view : VisibleConversationChoices(Catalogue, state))
        count += view.bIsLeave ? 0 : 1;
    return count;
}

void Open(ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    Check(OpenConversation(Catalogue, state, Farida, minute, availability) == ConversationError::None, "Farida opens a conversation");
}

bool Contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

std::string Lower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

ConversationState StateWithBenchAsked()
{
    ConversationState state;
    Open(state, Hour(7));
    Check(ChooseConversationChoice(Catalogue, state, AskBench).Ok(), "asking about the bench is accepted");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(HasConversationFact(state, FactBenchAsked), "asking set the fact");
    return state;
}

void TestValidAndTrue()
{
    Check(ValidateConversationCatalogue(Catalogue), "Farida's catalogue validates");
    for (const ConversationNodeDef& node : Catalogue.Nodes)
        Check(node.PersonId == Farida, "every node is Farida's");
    Check(ValidateConversationCatalogue(HouseholdConversationCatalogue()), "the household catalogue still validates");
    // Her catalogue stands beside the household's without sharing a node or a choice id.
    for (const ConversationNodeDef& node : HouseholdConversationCatalogue().Nodes)
        Check(FindConversationNode(Catalogue, node.Id) == nullptr, "no node id is shared with the household");
}

void TestEveryMinuteSelectsExactlyOneNode()
{
    const ConversationState none;
    const ConversationState asked = StateWithBenchAsked();
    for (const ConversationState* state : {&none, &asked})
    {
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        {
            const PersonAvailability availability = static_cast<PersonAvailability>(a);
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int serving = 0;
                for (const ConversationNodeDef& node : Catalogue.Nodes)
                    serving += ConversationNodeServes(node, minute, *state, availability) ? 1 : 0;
                Check(serving == 1, "exactly one node serves every availability and minute");
                const ConversationNodeDef* selected = SelectConversationNode(Catalogue, Farida, minute, *state, availability);
                Check(selected != nullptr && selected->bSelectable, "a selectable node is selected");
                Check(selected->Availability == availability, "the node serves the availability asked for");
            }
        }
    }
    Check(SelectConversationNode(Catalogue, Farida, ConversationMinutesPerDay, none, PersonAvailability::Awake) == nullptr, "minute 1440 selects nothing");
    Check(SelectConversationNode(Catalogue, "person.nobody", Hour(7), none, PersonAvailability::Awake) == nullptr, "an unknown person selects nothing");
}

void TestDayShape()
{
    const ConversationState none;
    // The edges of every window of the day.
    Check(NodeAt(none, 0) == "node.farida_qadri.late", "midnight is late");
    Check(NodeAt(none, Hour(5, 29)) == "node.farida_qadri.late", "05:29 is still late");
    Check(NodeAt(none, Hour(5, 30)) == "node.farida_qadri.preparing", "05:30 is preparation");
    Check(NodeAt(none, Hour(5, 59)) == "node.farida_qadri.preparing", "05:59 is preparation");
    Check(NodeAt(none, Hour(6)) == "node.farida_qadri.serving_morning", "06:00 is serving");
    Check(NodeAt(none, Hour(11, 29)) == "node.farida_qadri.serving_morning", "11:29 is serving");
    Check(NodeAt(none, Hour(11, 30)) == "node.farida_qadri.closed_midday", "11:30 is closed");
    Check(NodeAt(none, Hour(15, 29)) == "node.farida_qadri.closed_midday", "15:29 is closed");
    Check(NodeAt(none, Hour(15, 30)) == "node.farida_qadri.serving_afternoon", "15:30 is serving");
    Check(NodeAt(none, Hour(18, 59)) == "node.farida_qadri.serving_afternoon", "18:59 is serving");
    Check(NodeAt(none, Hour(19)) == "node.farida_qadri.closing", "19:00 is washing up and closing");
    Check(NodeAt(none, Hour(21, 59)) == "node.farida_qadri.closing", "21:59 is closing");
    Check(NodeAt(none, Hour(22)) == "node.farida_qadri.late", "22:00 is late");

    // The plan's sample lines, in their truthful form.
    Check(LineOf("node.farida_qadri.preparing") == "The water is nearly ready. There's room beside the bench.", "the preparation sample line");
    Check(LineOf("node.farida_qadri.closing") == "I'm washing the last cups. I'll open again tomorrow morning.", "the closing sample line");
    Check(Contains(LineOf("node.farida_qadri.closed_midday"), "half past three"), "the midday line says when she opens again");

    // Busy and asleep decline politely at every hour and offer Leave only.
    for (const PersonAvailability availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 30)
        {
            const std::string_view id = NodeAt(none, minute, availability);
            Check(id == (availability == PersonAvailability::Busy ? "node.farida_qadri.busy" : "node.farida_qadri.asleep"), "the decline node serves");
            const ConversationNodeDef* node = FindConversationNode(Catalogue, id);
            Check(node != nullptr && node->bDeclines && node->ChoiceCount == 0, "a decline offers nothing but Leave");
        }
    }
    ConversationState asleep;
    Open(asleep, Hour(23), PersonAvailability::Asleep);
    Check(AuthoredOffers(asleep) == 0 && VisibleConversationChoices(Catalogue, asleep).size() == 1, "an asleep session shows Leave only");
    Check(LeaveConversation(asleep) == ConversationError::None, "leave");
}

void TestRememberedLineOnlyAfterTheFact()
{
    const ConversationState none;
    const ConversationState asked = StateWithBenchAsked();
    constexpr std::string_view Remembered = "You asked me about the bench first.";

    // Without the fact, no line at any minute or availability remembers the bench.
    for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
        {
            const std::string_view id = NodeAt(none, minute, static_cast<PersonAvailability>(a));
            Check(!Contains(LineOf(id), "You asked"), "no remembered line before the fact");
        }
    }
    // Opening the day, declining tea and leaving does not set it; neither does sitting down.
    ConversationState state;
    Open(state, Hour(7));
    Check(ChooseConversationChoice(Catalogue, state, SitWithTea).Ok(), "sit with tea");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Open(state, Hour(7));
    Check(ChooseConversationChoice(Catalogue, state, DeclineTea).Ok(), "decline tea");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Open(state, Hour(16));
    Check(LeaveConversation(state) == ConversationError::None, "leave without choosing");
    Check(!HasConversationFact(state, FactBenchAsked), "tea, declining and leaving never set the bench fact");
    Check(NodeAt(state, Hour(7)) == "node.farida_qadri.serving_morning", "the plain morning line still plays");

    // The fact is set by the player's own choice, and only that choice.
    ConversationState asking;
    Open(asking, Hour(7));
    Check(Offers(asking, AskBench), "the bench question is offered while it has not been asked");
    const ConversationChooseResult result = ChooseConversationChoice(Catalogue, asking, AskBench);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == FactBenchAsked, "asking is free and sets exactly the bench fact");
    Check(Contains(LineOf(result.NextNodeId), "leave the doorway clear"), "she answers what was asked and claims no result");
    Check(LeaveConversation(asking) == ConversationError::None, "leave");

    // After the fact: the remembered line in both serving windows, the other windows unchanged.
    Check(NodeAt(asked, Hour(7)) == "node.farida_qadri.serving_morning_remembered", "morning remembers");
    Check(NodeAt(asked, Hour(17)) == "node.farida_qadri.serving_afternoon_remembered", "afternoon remembers");
    Check(Contains(LineOf("node.farida_qadri.serving_morning_remembered"), Remembered), "the remembered line names what happened");
    Check(NodeAt(asked, Hour(5, 45)) == "node.farida_qadri.preparing", "preparation is unchanged");
    Check(NodeAt(asked, Hour(13)) == "node.farida_qadri.closed_midday", "the closed window is unchanged");
    Check(NodeAt(asked, Hour(20)) == "node.farida_qadri.closing", "closing is unchanged");
    Check(NodeAt(asked, Hour(7), PersonAvailability::Asleep) == "node.farida_qadri.asleep", "asleep still declines whatever she remembers");

    // The remembered line states no outcome: it never says the bench was moved, that it worked or that anything was achieved.
    for (const std::string_view id : {"node.farida_qadri.serving_morning_remembered", "node.farida_qadri.serving_afternoon_remembered",
                                      "node.farida_qadri.reply.bench_asked"})
    {
        const std::string line = Lower(LineOf(id));
        Check(line.find("moved") == std::string::npos && line.find("worked") == std::string::npos && line.find("kept it") == std::string::npos,
              "no claimed result in the bench lines");
    }

    // Once asked the question is not offered again, and a second ask is refused untouched.
    ConversationState again = asked;
    Open(again, Hour(7));
    Check(!Offers(again, AskBench), "the bench question is not offered again");
    const ConversationState before = again;
    const ConversationChooseResult second = ChooseConversationChoice(Catalogue, again, AskBench);
    Check(!second.Ok() && second.CostMinutes == 0 && again == before, "asking twice is refused and changes nothing");
}

void TestTeaChargesOncePerSession()
{
    ConversationState state;
    Open(state, Hour(7));
    Check(state.SessionCount == 1, "the first session");
    Check(Offers(state, SitWithTea) && Offers(state, DeclineTea), "she offers tea and the player may decline");
    for (const ConversationChoiceView& view : VisibleConversationChoices(Catalogue, state))
    {
        if (view.Id == SitWithTea)
            Check(view.CostMinutes == 10, "sitting with tea declares ten minutes");
        if (view.Id == DeclineTea)
            Check(view.CostMinutes == 0, "declining declares nothing");
    }

    // Declining costs nothing and leaves no receipt; she repeats the offer and the player may then sit.
    ConversationChooseResult result = ChooseConversationChoice(Catalogue, state, DeclineTea);
    Check(result.Ok() && result.CostMinutes == 0 && !WasChoiceChargedThisSession(state, DeclineTea) && state.Receipts.empty(), "declining is free");
    Check(Offers(state, SitWithTea), "the offer stands after declining");
    result = ChooseConversationChoice(Catalogue, state, SitWithTea);
    Check(result.Ok() && result.CostMinutes == 10 && result.Session == 1 && result.ChoiceId == SitWithTea, "sitting is charged ten minutes, once");
    Check(WasChoiceChargedThisSession(state, SitWithTea), "a receipt records the charge");
    Check(Contains(LineOf(result.NextNodeId), "Mind the cup"), "tea is served");
    Check(!Offers(state, SitWithTea), "the paid choice is hidden for the rest of the session");

    // No second charge in the same session, by any route.
    const ConversationState before = state;
    const ConversationChooseResult repeat = ChooseConversationChoice(Catalogue, state, SitWithTea);
    Check(!repeat.Ok() && repeat.CostMinutes == 0 && state == before, "a repeat in the same session is refused, costs nothing and changes nothing");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(state.Receipts.size() == 1, "one receipt for the session");

    // Recurring across sessions: she offers again next time, and it is charged again, once.
    std::int32_t charged = 10;
    Open(state, Hour(17));
    Check(state.SessionCount == 2 && state.Receipts.empty(), "a new session drops the old receipt");
    Check(Offers(state, SitWithTea), "tea is offered afresh in the next session");
    result = ChooseConversationChoice(Catalogue, state, SitWithTea);
    Check(result.Ok() && result.CostMinutes == 10 && result.Session == 2, "the next session charges ten minutes once more");
    charged += result.CostMinutes;
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(charged == 20, "two sessions, two charges, no more");

    // The state survives a save: header, receipts and facts read back identically.
    const std::string text = SerialiseConversationState(Catalogue, state);
    Check(!text.empty() && text.rfind("pw.conversation/2", 0) == 0, "the state serialises");
    const ConversationStateParse parsed = DeserialiseConversationState(Catalogue, text);
    Check(parsed.Ok() && parsed.State == state, "the state reads back identically");
    ConversationState reloaded = parsed.State;
    Open(reloaded, Hour(7));
    Check(reloaded.SessionCount == 3 && Offers(reloaded, SitWithTea), "tea is offered again after a reload");
}

void TestNoPurchaseAndNoShorthand()
{
    // No economy exists yet: no line or choice mentions money, buying or a price; text is plain ASCII.
    constexpr std::string_view Forbidden[] = {"buy", "bought", "price", "pay", "paid", "rupee", "rs.", "cost", "sold", "order", "menu"};
    for (const ConversationNodeDef& node : Catalogue.Nodes)
    {
        std::vector<std::string_view> texts{node.Line};
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            texts.push_back(node.Choices[i].Text);
        for (const std::string_view text : texts)
        {
            const std::string lower = Lower(text);
            for (const std::string_view word : Forbidden)
                Check(lower.find(word) == std::string::npos, "no purchase language");
            for (const char c : text)
                Check(static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127, "plain ASCII text");
        }
    }
    // Every paid choice is the tea, and only the tea.
    int paid = 0;
    for (const ConversationNodeDef& node : Catalogue.Nodes)
    {
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            paid += node.Choices[i].CostMinutes > 0 ? 1 : 0;
    }
    Check(paid == 5, "the tea is the only paid choice (offered at four serving nodes and after declining)");
    for (const ConversationNodeDef& node : Catalogue.Nodes)
    {
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            Check(node.Choices[i].CostMinutes == 0 || node.Choices[i].Id == SitWithTea, "no paid choice but the tea");
    }
    Check(FindConversationChoice(Catalogue, SitWithTea) != nullptr && FindConversationChoice(Catalogue, SitWithTea)->CostMinutes == 10, "ten minutes");
}
} // namespace

int main()
{
    TestValidAndTrue();
    TestEveryMinuteSelectsExactlyOneNode();
    TestDayShape();
    TestRememberedLineOnlyAfterTheFact();
    TestTeaChargesOncePerSession();
    TestNoPurchaseAndNoShorthand();
    std::cout << "PerfectWorldConversationFaridaTests passed (" << Checks << " checks)\n";
    return 0;
}
