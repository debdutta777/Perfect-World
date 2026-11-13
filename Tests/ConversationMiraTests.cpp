// Mira Sen's conversation (LANE_LIFE_PLAN section B): her catalogue validates, every availability and minute selects exactly one
// node that matches her day, the remembered line plays only after the player has actually given her notes back, the two
// ten-minute choices are charged once per session (and again in a later one), declining and returning the notes cost nothing,
// refusals change nothing, an asleep or busy Mira declines, and no line promises money, a grade or a result nothing records.
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
int MrConversationChecks = 0;

void MrCheck(bool condition, const char* message)
{
    ++MrConversationChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr std::string_view MrWho = "person.mira_sen";
constexpr std::string_view MrNotesReturned = "fact.mira_sen.notes_returned";
constexpr std::string_view MrReturnNotesId = "choice.mira_sen.return_notes";
constexpr std::string_view MrCompareId = "choice.mira_sen.compare_question";
constexpr std::string_view MrWalkId = "choice.mira_sen.walk_to_corner";
constexpr std::string_view MrDeclineWalkId = "choice.mira_sen.decline_walk";

constexpr std::int32_t MrHour(std::int32_t hour, std::int32_t minute = 0) { return hour * 60 + minute; }

const ConversationCatalogue MrCatalogue = MiraConversationCatalogue();

std::string_view MrNodeAt(const ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(MrCatalogue, MrWho, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

std::string_view MrLineOf(std::string_view nodeId)
{
    const ConversationNodeDef* node = FindConversationNode(MrCatalogue, nodeId);
    MrCheck(node != nullptr, "the node exists");
    return node != nullptr ? node->Line : std::string_view{};
}

bool MrOffers(const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(MrCatalogue, state))
    {
        if (view.Id == choiceId)
            return true;
    }
    return false;
}

std::size_t MrAuthoredOffers(const ConversationState& state)
{
    std::size_t count = 0;
    for (const ConversationChoiceView& view : VisibleConversationChoices(MrCatalogue, state))
        count += view.bIsLeave ? 0 : 1;
    return count;
}

void MrOpen(ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    MrCheck(OpenConversation(MrCatalogue, state, MrWho, minute, availability) == ConversationError::None, "Mira opens a conversation");
}

std::string MrLower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/** The state after the player gave her notes back at the desk (Tuesday or Thursday 16:30 to 18:00). */
ConversationState MrStateWithNotes()
{
    ConversationState state;
    MrOpen(state, MrHour(17));
    MrCheck(ChooseConversationChoice(MrCatalogue, state, MrReturnNotesId).Ok(), "giving the notes back is accepted");
    MrCheck(LeaveConversation(state) == ConversationError::None, "leave");
    MrCheck(HasConversationFact(state, MrNotesReturned), "giving the notes back set the fact");
    return state;
}

void MrTestValidAndRegistered()
{
    MrCheck(ValidateConversationCatalogue(MrCatalogue), "Mira's catalogue validates");
    for (const ConversationNodeDef& node : MrCatalogue.Nodes)
        MrCheck(node.PersonId == MrWho, "every node is Mira's");
    for (const ConversationCatalogue& other : {HouseholdConversationCatalogue(), CareConversationCatalogue(), FaridaConversationCatalogue(),
                                               AninditaConversationCatalogue()})
    {
        MrCheck(ValidateConversationCatalogue(other), "the other catalogues still validate");
        for (const ConversationNodeDef& node : other.Nodes)
            MrCheck(FindConversationNode(MrCatalogue, node.Id) == nullptr, "no node id is shared with another catalogue");
        for (const ConversationNodeDef& node : other.Nodes)
            for (std::size_t i = 0; i < node.ChoiceCount; ++i)
                MrCheck(FindConversationChoice(MrCatalogue, node.Choices[i].Id) == nullptr, "no choice id is shared with another catalogue");
    }
    const auto registered = ConversationCatalogueForPerson(MrWho);
    MrCheck(registered.has_value() && registered->Nodes.data() == MrCatalogue.Nodes.data(), "the world finds her catalogue by person");
    for (const std::string_view other : {"person.anindita_sen", "person.farida_qadri", "person.care_worker"})
    {
        const auto found = ConversationCatalogueForPerson(other);
        MrCheck(found.has_value() && found->Nodes.data() != MrCatalogue.Nodes.data(), "the others still resolve to their own catalogues");
    }
}

void MrTestEveryMinuteSelectsExactlyOneNode()
{
    const ConversationState none;
    const ConversationState notes = MrStateWithNotes();
    for (const ConversationState* state : {&none, &notes})
    {
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        {
            const PersonAvailability availability = static_cast<PersonAvailability>(a);
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int serving = 0;
                for (const ConversationNodeDef& node : MrCatalogue.Nodes)
                    serving += ConversationNodeServes(node, minute, *state, availability) ? 1 : 0;
                MrCheck(serving == 1, "exactly one node serves every availability and minute");
                const ConversationNodeDef* selected = SelectConversationNode(MrCatalogue, MrWho, minute, *state, availability);
                MrCheck(selected != nullptr && selected->bSelectable && selected->Availability == availability, "a matching selectable node is selected");
            }
        }
    }
    MrCheck(SelectConversationNode(MrCatalogue, MrWho, ConversationMinutesPerDay, none, PersonAvailability::Awake) == nullptr, "minute 1440 selects nothing");
}

void MrTestDayShape()
{
    const ConversationState none;
    MrCheck(MrNodeAt(none, 0) == "node.mira_sen.late", "midnight is late");
    MrCheck(MrNodeAt(none, MrHour(6, 14)) == "node.mira_sen.late", "06:14 is still late");
    MrCheck(MrNodeAt(none, MrHour(6, 15)) == "node.mira_sen.morning", "06:15 is the morning greeting");
    MrCheck(MrNodeAt(none, MrHour(9, 29)) == "node.mira_sen.morning", "09:29 is the morning greeting");
    MrCheck(MrNodeAt(none, MrHour(9, 30)) == "node.mira_sen.away", "09:30 she cannot stop");
    MrCheck(MrNodeAt(none, MrHour(16, 29)) == "node.mira_sen.away", "16:29 she cannot stop");
    MrCheck(MrNodeAt(none, MrHour(16, 30)) == "node.mira_sen.counter", "16:30 is the packing desk");
    MrCheck(MrNodeAt(none, MrHour(17, 59)) == "node.mira_sen.counter", "17:59 is the packing desk");
    MrCheck(MrNodeAt(none, MrHour(18)) == "node.mira_sen.evening", "18:00 is the evening at home");
    MrCheck(MrNodeAt(none, MrHour(21, 29)) == "node.mira_sen.evening", "21:29 is the evening at home");
    MrCheck(MrNodeAt(none, MrHour(21, 30)) == "node.mira_sen.late", "21:30 is late");

    // The plan's sample lines, word for word.
    MrCheck(MrLineOf("node.mira_sen.morning") == "I've got time to walk with you as far as the corner.", "the school-day greeting");
    MrCheck(MrLineOf("node.mira_sen.counter") == "I'm at the counter today. We can talk while I finish this list.", "the at-the-counter line");
    MrCheck(MrLineOf("node.mira_sen.counter_remembered") == "You brought the notes back when you said you would. Want to compare the last question?",
            "the remembered line");
    MrCheck(MrLineOf("node.mira_sen.away").find("Tuesday or Thursday") != std::string_view::npos, "the away line says when to find her at the shop");

    // Busy and asleep decline politely at every hour and offer Leave only.
    for (const PersonAvailability availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 30)
        {
            const std::string_view id = MrNodeAt(none, minute, availability);
            MrCheck(id == (availability == PersonAvailability::Busy ? "node.mira_sen.busy" : "node.mira_sen.asleep"), "the decline node serves");
            const ConversationNodeDef* node = FindConversationNode(MrCatalogue, id);
            MrCheck(node != nullptr && node->bDeclines && node->ChoiceCount == 0, "a decline offers nothing but Leave");
        }
    }
    for (const PersonAvailability availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        ConversationState declined;
        MrOpen(declined, MrHour(17), availability);
        MrCheck(MrAuthoredOffers(declined) == 0 && VisibleConversationChoices(MrCatalogue, declined).size() == 1, "a declined session shows Leave only");
        MrCheck(!MrOffers(declined, MrReturnNotesId) && !MrOffers(declined, MrWalkId), "no authored choice is reachable while she is busy or asleep");
        MrCheck(LeaveConversation(declined) == ConversationError::None, "leave");
    }
}

void MrTestRememberedLineOnlyAfterTheFact()
{
    const ConversationState none;
    const ConversationState notes = MrStateWithNotes();

    for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            MrCheck(MrLineOf(MrNodeAt(none, minute, static_cast<PersonAvailability>(a))).find("brought the notes back") == std::string_view::npos,
                    "no remembered line before the fact");
    }
    // Declining the walk, walking, and leaving without choosing never set it.
    ConversationState state;
    MrOpen(state, MrHour(8));
    MrCheck(ChooseConversationChoice(MrCatalogue, state, MrDeclineWalkId).Ok(), "decline the walk");
    MrCheck(LeaveConversation(state) == ConversationError::None, "leave");
    MrOpen(state, MrHour(8));
    MrCheck(ChooseConversationChoice(MrCatalogue, state, MrWalkId).Ok(), "walk to the corner");
    MrCheck(LeaveConversation(state) == ConversationError::None, "leave");
    MrOpen(state, MrHour(17));
    MrCheck(LeaveConversation(state) == ConversationError::None, "leave without choosing");
    MrCheck(!HasConversationFact(state, MrNotesReturned), "walking, declining and leaving never set the notes fact");
    MrCheck(MrNodeAt(state, MrHour(17)) == "node.mira_sen.counter", "the plain counter line still plays");

    // Giving the notes back sets exactly the fact and costs nothing.
    ConversationState giving;
    MrOpen(giving, MrHour(17));
    MrCheck(MrOffers(giving, MrReturnNotesId) && !MrOffers(giving, MrCompareId), "the notes are offered at the counter, the comparison is not yet");
    const ConversationChooseResult result = ChooseConversationChoice(MrCatalogue, giving, MrReturnNotesId);
    MrCheck(result.Ok() && result.CostMinutes == 0 && result.FactSet == MrNotesReturned && giving.Receipts.empty(),
            "giving the notes back is free and sets exactly the notes fact");
    MrCheck(result.NextNodeId == "node.mira_sen.reply.notes_returned", "she thanks the player for bringing them back");
    MrCheck(LeaveConversation(giving) == ConversationError::None, "leave");

    MrCheck(MrNodeAt(notes, MrHour(17)) == "node.mira_sen.counter_remembered", "the counter line now remembers");
    MrCheck(MrNodeAt(notes, MrHour(8)) == "node.mira_sen.morning", "the morning is unchanged");
    MrCheck(MrNodeAt(notes, MrHour(12)) == "node.mira_sen.away", "the away line is unchanged");
    MrCheck(MrNodeAt(notes, MrHour(19)) == "node.mira_sen.evening", "the evening is unchanged");
    MrCheck(MrNodeAt(notes, MrHour(17), PersonAvailability::Busy) == "node.mira_sen.busy", "busy still declines whatever she remembers");
    MrCheck(MrNodeAt(notes, MrHour(17), PersonAvailability::Asleep) == "node.mira_sen.asleep", "asleep still declines whatever she remembers");

    // After the fact, the notes cannot be given back again, and the comparison is what is offered.
    ConversationState later = notes;
    MrOpen(later, MrHour(17));
    MrCheck(!MrOffers(later, MrReturnNotesId) && MrOffers(later, MrCompareId), "the comparison replaces the return of the notes");
    MrCheck(MrAuthoredOffers(later) == 1, "exactly the comparison");
    const ConversationState before = later;
    MrCheck(!ChooseConversationChoice(MrCatalogue, later, MrReturnNotesId).Ok() && later == before, "the notes cannot be given back twice");
    MrCheck(LeaveConversation(later) == ConversationError::None, "leave");

    // The remembered line states no outcome of the comparison.
    const std::string line = MrLower(MrLineOf("node.mira_sen.counter_remembered"));
    MrCheck(line.find("score") == std::string::npos && line.find("mark") == std::string::npos && line.find("right") == std::string::npos &&
                line.find("wrong") == std::string::npos,
            "no claimed result in the remembered line");
}

void MrTestChoicesChargeOncePerSession()
{
    // The morning walk.
    ConversationState state;
    MrOpen(state, MrHour(8));
    MrCheck(state.SessionCount == 1, "the first session");
    MrCheck(MrOffers(state, MrWalkId) && MrOffers(state, MrDeclineWalkId), "she offers the walk and the player may decline");
    for (const ConversationChoiceView& view : VisibleConversationChoices(MrCatalogue, state))
    {
        if (view.Id == MrWalkId)
            MrCheck(view.CostMinutes == 10, "the walk declares ten minutes");
        if (view.Id == MrDeclineWalkId)
            MrCheck(view.CostMinutes == 0, "declining declares nothing");
    }
    ConversationChooseResult result = ChooseConversationChoice(MrCatalogue, state, MrWalkId);
    MrCheck(result.Ok() && result.CostMinutes == 10 && result.Session == 1 && result.FactSet.empty(), "walking is charged ten minutes, once, and sets no fact");
    MrCheck(WasChoiceChargedThisSession(state, MrWalkId), "a receipt records the charge");
    MrCheck(result.NextNodeId == "node.mira_sen.reply.walked" && !result.bEnded, "she answers");
    MrCheck(!MrOffers(state, MrWalkId), "the paid choice is not offered again on the reply");
    const ConversationState charged = state;
    const ConversationChooseResult repeat = ChooseConversationChoice(MrCatalogue, state, MrWalkId);
    MrCheck(!repeat.Ok() && repeat.CostMinutes == 0 && state == charged, "a repeat in the same session is refused and changes nothing");
    MrCheck(LeaveConversation(state) == ConversationError::None, "leave");
    MrCheck(state.Receipts.size() == 1, "one receipt for the session");

    // A later session: offered afresh, charged once more.
    MrOpen(state, MrHour(8, 30));
    MrCheck(state.SessionCount == 2 && state.Receipts.empty(), "a new session drops the old receipt");
    MrCheck(MrOffers(state, MrWalkId), "the walk is offered afresh");
    result = ChooseConversationChoice(MrCatalogue, state, MrDeclineWalkId);
    MrCheck(result.Ok() && result.CostMinutes == 0 && state.Receipts.empty(), "declining is free and leaves no receipt");
    MrCheck(LeaveConversation(state) == ConversationError::None, "leave");

    // The comparison at the desk, after the notes.
    ConversationState notes = MrStateWithNotes();
    MrOpen(notes, MrHour(16, 40));
    result = ChooseConversationChoice(MrCatalogue, notes, MrCompareId);
    MrCheck(result.Ok() && result.CostMinutes == 10 && result.FactSet.empty() && result.Session == notes.SessionCount, "comparing costs ten minutes and sets no fact");
    MrCheck(result.NextNodeId == "node.mira_sen.reply.compared", "she answers the comparison");
    MrCheck(!ChooseConversationChoice(MrCatalogue, notes, MrCompareId).Ok(), "the comparison is not charged twice in a session");
    MrCheck(LeaveConversation(notes) == ConversationError::None, "leave");
    MrOpen(notes, MrHour(17, 10));
    MrCheck(MrOffers(notes, MrCompareId), "the comparison is offered again in a later session");
    MrCheck(LeaveConversation(notes) == ConversationError::None, "leave");

    // The state survives a save.
    const std::string text = SerialiseConversationState(MrCatalogue, notes);
    MrCheck(!text.empty() && text.rfind("pw.conversation/2", 0) == 0, "the state serialises");
    const ConversationStateParse parsed = DeserialiseConversationState(MrCatalogue, text);
    MrCheck(parsed.Ok() && parsed.State == notes, "the state reads back identically");
}

void MrTestRefusals()
{
    // A choice that is not on the current node is refused and changes nothing.
    ConversationState state;
    MrOpen(state, MrHour(8));
    ConversationState before = state;
    ConversationChooseResult result = ChooseConversationChoice(MrCatalogue, state, MrCompareId);
    MrCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "the comparison is not offered in the morning");
    result = ChooseConversationChoice(MrCatalogue, state, MrReturnNotesId);
    MrCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "the notes are not given back in the morning");
    result = ChooseConversationChoice(MrCatalogue, state, "choice.mira_sen.nonsense");
    MrCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "an unknown choice is refused");
    result = ChooseConversationChoice(MrCatalogue, state, "choice.anindita_sen.help_label");
    MrCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "another person's choice is refused");
    MrCheck(LeaveConversation(state) == ConversationError::None, "leave");
    // Not open: nothing to choose.
    before = state;
    result = ChooseConversationChoice(MrCatalogue, state, MrWalkId);
    MrCheck(!result.Ok() && result.CostMinutes == 0 && state == before, "a choice with no open session is refused");
    MrCheck(LeaveConversation(state) != ConversationError::None, "leaving twice is refused");
    // Opening twice and opening a stranger.
    MrOpen(state, MrHour(8));
    MrCheck(OpenConversation(MrCatalogue, state, MrWho, MrHour(8), PersonAvailability::Awake) != ConversationError::None, "a second open is refused");
    MrCheck(LeaveConversation(state) == ConversationError::None, "leave");
    ConversationState stranger;
    MrCheck(OpenConversation(MrCatalogue, stranger, "person.nobody", MrHour(8), PersonAvailability::Awake) != ConversationError::None, "an unknown person is refused");
    // At school time nothing is offered but Leave.
    ConversationState away;
    MrOpen(away, MrHour(12));
    MrCheck(MrAuthoredOffers(away) == 0 && !ChooseConversationChoice(MrCatalogue, away, MrWalkId).Ok(), "nothing is offered while she cannot stop");
    MrCheck(LeaveConversation(away) == ConversationError::None, "leave");
}

void MrTestNoMoneyAndPlainText()
{
    constexpr std::string_view Forbidden[] = {"buy", "bought", "price", "pay", "paid", "rupee", "rs.", "cost", "sold", "order", "grade", "marks", "anindita"};
    for (const ConversationNodeDef& node : MrCatalogue.Nodes)
    {
        std::vector<std::string_view> texts{node.Line};
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            texts.push_back(node.Choices[i].Text);
        for (const std::string_view text : texts)
        {
            const std::string lower = MrLower(text);
            for (const std::string_view word : Forbidden)
                MrCheck(lower.find(word) == std::string::npos, "no money, grades or reliance on her mother");
            for (const char c : text)
                MrCheck(static_cast<unsigned char>(c) >= 32 && static_cast<unsigned char>(c) < 127, "plain ASCII text");
        }
        MrCheck(node.Line.size() <= 110, "a line stays short");
    }
    for (const ConversationNodeDef& node : MrCatalogue.Nodes)
    {
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            MrCheck(node.Choices[i].CostMinutes == 0 || node.Choices[i].Id == MrWalkId || node.Choices[i].Id == MrCompareId,
                    "no paid choice but the walk and the comparison");
    }
    MrCheck(FindConversationChoice(MrCatalogue, MrWalkId)->CostMinutes == 10 && FindConversationChoice(MrCatalogue, MrCompareId)->CostMinutes == 10,
            "both cost ten minutes");
    MrCheck(FindConversationChoice(MrCatalogue, MrCompareId)->Text == "Compare the last question", "the comparison is worded as in the plan");
}
} // namespace

int main()
{
    MrTestValidAndRegistered();
    MrTestEveryMinuteSelectsExactlyOneNode();
    MrTestDayShape();
    MrTestRememberedLineOnlyAfterTheFact();
    MrTestChoicesChargeOncePerSession();
    MrTestRefusals();
    MrTestNoMoneyAndPlainText();
    std::cout << "PerfectWorldConversationMiraTests passed (" << MrConversationChecks << " checks)\n";
    return 0;
}
