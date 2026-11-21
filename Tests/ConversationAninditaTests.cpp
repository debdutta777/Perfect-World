// Anindita Sen's conversation (order AN; LANE_LIFE_PLAN section B): her catalogue validates, every availability and minute selects
// exactly one node that matches her day, the remembered line plays only after the player has actually helped with the labels,
// helping (labels, counting stock) costs ten minutes charged once per session (and again in a later one), declining costs
// nothing, refusals change nothing, and no line promises a purchase or a result nothing records.
#include "Conversation.h"

#include "WorldState.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int AnConversationChecks = 0;

void AnCheck(bool condition, const char* message)
{
    ++AnConversationChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr std::string_view AnWho = "person.anindita_sen";
constexpr std::string_view AnLabelsHelped = "fact.anindita_sen.labels_helped";
constexpr std::string_view AnHelpLabelId = "choice.anindita_sen.help_label";
constexpr std::string_view AnDeclineLabelId = "choice.anindita_sen.decline_label";
constexpr std::string_view AnCountStockId = "choice.anindita_sen.count_stock";

constexpr std::int32_t AnHour(std::int32_t hour, std::int32_t minute = 0) { return hour * 60 + minute; }

const ConversationCatalogue AnCatalogue = AninditaConversationCatalogue();

std::string_view AnNodeAt(const ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(AnCatalogue, AnWho, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

std::string_view AnLineOf(std::string_view nodeId)
{
    const ConversationNodeDef* node = FindConversationNode(AnCatalogue, nodeId);
    AnCheck(node != nullptr, "the node exists");
    return node != nullptr ? node->Line : std::string_view{};
}

bool AnOffers(const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(AnCatalogue, state))
    {
        if (view.Id == choiceId)
            return true;
    }
    return false;
}

std::size_t AnAuthoredOffers(const ConversationState& state)
{
    std::size_t count = 0;
    for (const ConversationChoiceView& view : VisibleConversationChoices(AnCatalogue, state))
        count += view.bIsLeave ? 0 : 1;
    return count;
}

void AnOpen(ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    AnCheck(OpenConversation(AnCatalogue, state, AnWho, minute, availability) == ConversationError::None, "Anindita opens a conversation");
}

std::string AnLower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

ConversationState AnStateWithHelped()
{
    ConversationState state;
    AnOpen(state, AnHour(9));
    AnCheck(ChooseConversationChoice(AnCatalogue, state, AnHelpLabelId).Ok(), "helping with the labels is accepted");
    AnCheck(LeaveConversation(state) == ConversationError::None, "leave");
    AnCheck(HasConversationFact(state, AnLabelsHelped), "helping set the fact");
    return state;
}

void AnTestValidAndRegistered()
{
    AnCheck(ValidateConversationCatalogue(AnCatalogue), "Anindita's catalogue validates");
    for (const ConversationNodeDef& node : AnCatalogue.Nodes)
        AnCheck(node.PersonId == AnWho, "every node is Anindita's");
    for (const ConversationCatalogue& other :
         {HouseholdConversationCatalogue(), CareConversationCatalogue(), FaridaConversationCatalogue()})
    {
        AnCheck(ValidateConversationCatalogue(other), "the other catalogues still validate");
        for (const ConversationNodeDef& node : other.Nodes)
            AnCheck(FindConversationNode(AnCatalogue, node.Id) == nullptr, "no node id is shared with another catalogue");
    }
    const auto registered = ConversationCatalogueForPerson(AnWho);
    AnCheck(registered.has_value() && registered->Nodes.data() == AnCatalogue.Nodes.data(), "the world finds her catalogue by person");
}

void AnTestEveryMinuteSelectsExactlyOneNode()
{
    const ConversationState none;
    const ConversationState helped = AnStateWithHelped();
    for (const ConversationState* state : {&none, &helped})
    {
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        {
            const PersonAvailability availability = static_cast<PersonAvailability>(a);
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int serving = 0;
                for (const ConversationNodeDef& node : AnCatalogue.Nodes)
                    serving += ConversationNodeServes(node, minute, *state, availability) ? 1 : 0;
                AnCheck(serving == 1, "exactly one node serves every availability and minute");
                const ConversationNodeDef* selected = SelectConversationNode(AnCatalogue, AnWho, minute, *state, availability);
                AnCheck(selected != nullptr && selected->bSelectable && selected->Availability == availability, "a matching selectable node is selected");
            }
        }
    }
    AnCheck(SelectConversationNode(AnCatalogue, AnWho, ConversationMinutesPerDay, none, PersonAvailability::Awake) == nullptr, "minute 1440 selects nothing");
}

void AnTestDayShape()
{
    const ConversationState none;
    AnCheck(AnNodeAt(none, 0) == "node.anindita_sen.late", "midnight is late");
    AnCheck(AnNodeAt(none, AnHour(7, 29)) == "node.anindita_sen.late", "07:29 is still late");
    AnCheck(AnNodeAt(none, AnHour(7, 30)) == "node.anindita_sen.preparing", "07:30 is the stock check");
    AnCheck(AnNodeAt(none, AnHour(8)) == "node.anindita_sen.counter_morning", "08:00 is the counter");
    AnCheck(AnNodeAt(none, AnHour(12, 59)) == "node.anindita_sen.counter_morning", "12:59 is the counter");
    AnCheck(AnNodeAt(none, AnHour(13)) == "node.anindita_sen.closed_midday", "13:00 is the meal closure");
    AnCheck(AnNodeAt(none, AnHour(14, 59)) == "node.anindita_sen.closed_midday", "14:59 is the meal closure");
    AnCheck(AnNodeAt(none, AnHour(15)) == "node.anindita_sen.counter_afternoon", "15:00 is the counter");
    AnCheck(AnNodeAt(none, AnHour(17, 29)) == "node.anindita_sen.counter_afternoon", "17:29 is the counter");
    AnCheck(AnNodeAt(none, AnHour(17, 30)) == "node.anindita_sen.counter_evening", "17:30 is the stock check before closing");
    AnCheck(AnNodeAt(none, AnHour(19, 29)) == "node.anindita_sen.counter_evening", "19:29 is before closing");
    AnCheck(AnNodeAt(none, AnHour(19, 30)) == "node.anindita_sen.closing", "19:30 is closed");
    AnCheck(AnNodeAt(none, AnHour(20, 59)) == "node.anindita_sen.closing", "20:59 is closed");
    AnCheck(AnNodeAt(none, AnHour(21)) == "node.anindita_sen.late", "21:00 is late");

    // The plan's sample lines. The first two are as written; the third is reworded to the recorded fact.
    AnCheck(AnLineOf("node.anindita_sen.counter_morning") == "The plain exercise books are on the lower shelf.", "the earlier-encounter sample line");
    AnCheck(AnLineOf("node.anindita_sen.counter_evening") == "I'm checking the stock before I close.", "the time-dependent sample line");
    AnCheck(AnLineOf("node.anindita_sen.closed_midday").find("three") != std::string_view::npos, "the midday line says when the shop opens again");

    // Busy and asleep decline politely at every hour and offer Leave only.
    for (const PersonAvailability availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 30)
        {
            const std::string_view id = AnNodeAt(none, minute, availability);
            AnCheck(id == (availability == PersonAvailability::Busy ? "node.anindita_sen.busy" : "node.anindita_sen.asleep"), "the decline node serves");
            const ConversationNodeDef* node = FindConversationNode(AnCatalogue, id);
            AnCheck(node != nullptr && node->bDeclines && node->ChoiceCount == 0, "a decline offers nothing but Leave");
        }
    }
    ConversationState busy;
    AnOpen(busy, AnHour(7, 40), PersonAvailability::Busy);
    AnCheck(AnAuthoredOffers(busy) == 0 && VisibleConversationChoices(AnCatalogue, busy).size() == 1, "a busy session shows Leave only");
    AnCheck(LeaveConversation(busy) == ConversationError::None, "leave");
}

void AnTestRememberedLineOnlyAfterTheFact()
{
    const ConversationState none;
    const ConversationState helped = AnStateWithHelped();

    for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            AnCheck(AnLower(AnLineOf(AnNodeAt(none, minute, static_cast<PersonAvailability>(a)))).find("you helped") == std::string::npos,
                    "no remembered line before the fact");
    }
    // Declining, leaving, and counting stock do not set it.
    ConversationState state;
    AnOpen(state, AnHour(9));
    AnCheck(ChooseConversationChoice(AnCatalogue, state, AnDeclineLabelId).Ok(), "decline the labels");
    AnCheck(LeaveConversation(state) == ConversationError::None, "leave");
    AnOpen(state, AnHour(18));
    AnCheck(ChooseConversationChoice(AnCatalogue, state, AnCountStockId).Ok(), "count the stock");
    AnCheck(LeaveConversation(state) == ConversationError::None, "leave");
    AnOpen(state, AnHour(16));
    AnCheck(LeaveConversation(state) == ConversationError::None, "leave without choosing");
    AnCheck(!HasConversationFact(state, AnLabelsHelped), "declining, counting and leaving never set the labels fact");
    AnCheck(AnNodeAt(state, AnHour(9)) == "node.anindita_sen.counter_morning", "the plain greeting still plays");

    // Helping with the labels sets exactly the fact.
    ConversationState helping;
    AnOpen(helping, AnHour(9));
    AnCheck(AnOffers(helping, AnHelpLabelId), "the labels job is offered");
    const ConversationChooseResult result = ChooseConversationChoice(AnCatalogue, helping, AnHelpLabelId);
    AnCheck(result.Ok() && result.CostMinutes == 10 && result.FactSet == AnLabelsHelped, "helping costs ten minutes and sets exactly the labels fact");
    AnCheck(LeaveConversation(helping) == ConversationError::None, "leave");

    AnCheck(AnNodeAt(helped, AnHour(9)) == "node.anindita_sen.counter_morning_remembered", "morning remembers");
    AnCheck(AnNodeAt(helped, AnHour(16)) == "node.anindita_sen.counter_afternoon_remembered", "afternoon remembers");
    AnCheck(AnLineOf("node.anindita_sen.counter_morning_remembered").find("You helped with the labels") != std::string_view::npos, "the remembered line names what happened");
    AnCheck(AnNodeAt(helped, AnHour(7, 40)) == "node.anindita_sen.preparing", "the stock check is unchanged");
    AnCheck(AnNodeAt(helped, AnHour(14)) == "node.anindita_sen.closed_midday", "the closure is unchanged");
    AnCheck(AnNodeAt(helped, AnHour(18)) == "node.anindita_sen.counter_evening", "the evening is unchanged");
    AnCheck(AnNodeAt(helped, AnHour(9), PersonAvailability::Busy) == "node.anindita_sen.busy", "busy still declines whatever she remembers");

    // The remembered line states no outcome beyond thanks.
    for (const std::string_view id : {"node.anindita_sen.counter_morning_remembered", "node.anindita_sen.counter_afternoon_remembered"})
    {
        const std::string line = AnLower(AnLineOf(id));
        AnCheck(line.find("saved") == std::string::npos && line.find("second count") == std::string::npos && line.find("kept") == std::string::npos,
                "no claimed result in the remembered lines");
    }
    // The job can be offered again after it was done: it recurs, like the tea.
    ConversationState again = helped;
    AnOpen(again, AnHour(9));
    AnCheck(AnOffers(again, AnHelpLabelId), "the labels job is offered again in a later session");
}

void AnTestHelpChargesOncePerSession()
{
    ConversationState state;
    AnOpen(state, AnHour(9));
    AnCheck(state.SessionCount == 1, "the first session");
    AnCheck(AnOffers(state, AnHelpLabelId) && AnOffers(state, AnDeclineLabelId), "she offers the job and the player may decline");
    for (const ConversationChoiceView& view : VisibleConversationChoices(AnCatalogue, state))
    {
        if (view.Id == AnHelpLabelId)
            AnCheck(view.CostMinutes == 10, "helping declares ten minutes");
        if (view.Id == AnDeclineLabelId)
            AnCheck(view.CostMinutes == 0, "declining declares nothing");
    }
    ConversationChooseResult result = ChooseConversationChoice(AnCatalogue, state, AnDeclineLabelId);
    AnCheck(result.Ok() && result.CostMinutes == 0 && state.Receipts.empty(), "declining is free and leaves no receipt");
    AnCheck(AnOffers(state, AnHelpLabelId), "the offer stands after declining");
    result = ChooseConversationChoice(AnCatalogue, state, AnHelpLabelId);
    AnCheck(result.Ok() && result.CostMinutes == 10 && result.Session == 1, "helping is charged ten minutes, once");
    AnCheck(WasChoiceChargedThisSession(state, AnHelpLabelId), "a receipt records the charge");
    AnCheck(!AnOffers(state, AnHelpLabelId), "the paid choice is hidden for the rest of the session");

    // Refused in the same session, costs nothing, changes nothing.
    const ConversationState before = state;
    const ConversationChooseResult repeat = ChooseConversationChoice(AnCatalogue, state, AnHelpLabelId);
    AnCheck(!repeat.Ok() && repeat.CostMinutes == 0 && state == before, "a repeat in the same session is refused and changes nothing");
    AnCheck(LeaveConversation(state) == ConversationError::None, "leave");
    AnCheck(state.Receipts.size() == 1, "one receipt for the session");

    // Next session: offered afresh, charged once more.
    AnOpen(state, AnHour(18));
    AnCheck(state.SessionCount == 2 && state.Receipts.empty(), "a new session drops the old receipt");
    AnCheck(AnOffers(state, AnCountStockId), "counting stock is offered in the evening");
    result = ChooseConversationChoice(AnCatalogue, state, AnCountStockId);
    AnCheck(result.Ok() && result.CostMinutes == 10 && result.Session == 2 && result.FactSet.empty(), "counting stock costs ten minutes and sets no fact");
    AnCheck(!ChooseConversationChoice(AnCatalogue, state, AnCountStockId).Ok(), "counting is not charged twice in a session");
    AnCheck(LeaveConversation(state) == ConversationError::None, "leave");

    // The state survives a save.
    const std::string text = SerialiseConversationState(AnCatalogue, state);
    AnCheck(!text.empty() && text.rfind("pw.conversation/2", 0) == 0, "the state serialises");
    const ConversationStateParse parsed = DeserialiseConversationState(AnCatalogue, text);
    AnCheck(parsed.Ok() && parsed.State == state, "the state reads back identically");
}

void AnTestRefusals()
{
    // A choice that is not on the current node is refused and changes nothing.
    ConversationState state;
    AnOpen(state, AnHour(9));
    ConversationState before = state;
    ConversationChooseResult result = ChooseConversationChoice(AnCatalogue, state, AnCountStockId);
    AnCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "stock counting is not offered in the morning");
    result = ChooseConversationChoice(AnCatalogue, state, "choice.anindita_sen.nonsense");
    AnCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "an unknown choice is refused");
    result = ChooseConversationChoice(AnCatalogue, state, "choice.farida_qadri.sit_with_tea");
    AnCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "another person's choice is refused");
    AnCheck(LeaveConversation(state) == ConversationError::None, "leave");
    // Not open: nothing to choose.
    before = state;
    result = ChooseConversationChoice(AnCatalogue, state, AnHelpLabelId);
    AnCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "a choice with no open session is refused");
    AnCheck(LeaveConversation(state) != ConversationError::None, "leaving twice is refused");
    // Opening twice and opening a stranger.
    AnOpen(state, AnHour(9));
    AnCheck(OpenConversation(AnCatalogue, state, AnWho, AnHour(9), PersonAvailability::Awake) != ConversationError::None, "a second open is refused");
    AnCheck(LeaveConversation(state) == ConversationError::None, "leave");
    ConversationState stranger;
    AnCheck(OpenConversation(AnCatalogue, stranger, "person.nobody", AnHour(9), PersonAvailability::Awake) != ConversationError::None, "an unknown person is refused");
    // The helped time of day matters: the closure offers only coming back.
    ConversationState closed;
    AnOpen(closed, AnHour(14));
    AnCheck(!ChooseConversationChoice(AnCatalogue, closed, AnHelpLabelId).Ok(), "no job is offered while the shop is closed");
    AnCheck(ChooseConversationChoice(AnCatalogue, closed, "choice.anindita_sen.come_back").Ok(), "coming back later is free");
}

void AnTestNoPurchaseAndPlainText()
{
    constexpr std::string_view Forbidden[] = {"buy", "bought", "price", "pay", "paid", "rupee", "rs.", "cost", "sold", "order", "mira"};
    for (const ConversationNodeDef& node : AnCatalogue.Nodes)
    {
        std::vector<std::string_view> texts{node.Line};
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            texts.push_back(node.Choices[i].Text);
        for (const std::string_view text : texts)
        {
            const std::string lower = AnLower(text);
            for (const std::string_view word : Forbidden)
                AnCheck(lower.find(word) == std::string::npos, "no purchase language and no reliance on Mira");
            for (const char c : text)
                AnCheck(static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127, "plain ASCII text");
        }
    }
    for (const ConversationNodeDef& node : AnCatalogue.Nodes)
    {
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            AnCheck(node.Choices[i].CostMinutes == 0 || node.Choices[i].Id == AnHelpLabelId || node.Choices[i].Id == AnCountStockId,
                    "no paid choice but the two small jobs");
    }
    AnCheck(FindConversationChoice(AnCatalogue, AnHelpLabelId)->CostMinutes == 10 && FindConversationChoice(AnCatalogue, AnCountStockId)->CostMinutes == 10,
            "both jobs cost ten minutes");
}
} // namespace

int main()
{
    AnTestValidAndRegistered();
    AnTestEveryMinuteSelectsExactlyOneNode();
    AnTestDayShape();
    AnTestRememberedLineOnlyAfterTheFact();
    AnTestHelpChargesOncePerSession();
    AnTestRefusals();
    AnTestNoPurchaseAndPlainText();
    std::cout << "PerfectWorldConversationAninditaTests passed (" << AnConversationChecks << " checks)\n";
    return 0;
}
