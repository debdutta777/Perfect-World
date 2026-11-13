// LANE_LIFE_IMPLEMENTATION rows 5.1 and 5.2: the general conversation runtime. These tests use their OWN catalogue (two people,
// five facts, a group, an implication, a one-time choice, paid choices that recur) so they prove the runtime is not tied to the
// household adult. The household content has its own file (ConversationHouseholdTests.cpp).
#include "Conversation.h"

#include <cstdlib>
#include <iostream>
#include <limits>
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

constexpr std::string_view PersonA = "person.t.a";
constexpr std::string_view PersonB = "person.t.b";
constexpr std::string_view FactAsked = "fact.t.asked";
constexpr std::string_view FactGift = "fact.t.gift";
constexpr std::string_view FactNote = "fact.t.note";
constexpr std::string_view FactModeX = "fact.t.mode_x";
constexpr std::string_view FactModeY = "fact.t.mode_y";

// Id, Text, Cost, bOnceEver, SetsFact, ClearsFact, NextNode, RequiresFact, ForbidsFact
const ConversationChoiceDef Ask{"choice.t.ask", "Ask", 4, false, FactAsked, "", "node.t.a.reply.asked", "", ""};
const ConversationChoiceDef Gift{"choice.t.gift", "Give a gift", 0, true, FactGift, "", "", "", ""};
const ConversationChoiceDef PickX{"choice.t.pick_x", "Pick X", 0, false, FactModeX, "", "node.t.a.reply.picked", "", FactModeX};
const ConversationChoiceDef PickY{"choice.t.pick_y", "Pick Y", 0, false, FactModeY, "", "node.t.a.reply.picked", "", FactModeY};
const ConversationChoiceDef Note{"choice.t.note", "Note", 0, false, FactNote, "", "node.t.a.reply.noted", "", FactNote};
const ConversationChoiceDef Tear{"choice.t.tear", "Tear", 0, false, "", FactNote, "node.t.a.reply.noted", FactNote, ""};
const ConversationChoiceDef Pay{"choice.t.pay", "Pay", 6, false, "", "", "", "", ""};

// Id, Person, Line, Availability, From, To, Requires{}, Forbids{}, selectable, declines, Choices, count
const std::array<ConversationNodeDef, 12> MainNodes{{
    {"node.t.a.greet", PersonA, "Hello.", PersonAvailability::Awake, 0, 1440, {}, {FactAsked, ""}, true, false, {Ask, Gift, PickX}, 3},
    {"node.t.a.followup", PersonA, "Again?", PersonAvailability::Awake, 0, 1440, {FactAsked, ""}, {}, true, false, {Ask, PickX, PickY}, 3},
    {"node.t.a.busy", PersonA, "Later.", PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.t.a.asleep", PersonA, "Zzz.", PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.t.a.reply.asked", PersonA, "Noted.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {Ask, Note, Tear}, 3},
    {"node.t.a.reply.noted", PersonA, "Written.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.t.a.reply.picked", PersonA, "Picked.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.t.b.day", PersonB, "Day.", PersonAvailability::Awake, 360, 1380, {}, {FactGift, ""}, true, false, {Pay}, 1},
    {"node.t.b.day_gift", PersonB, "Day, thanks.", PersonAvailability::Awake, 360, 1380, {FactGift, ""}, {}, true, false, {}, 0},
    {"node.t.b.night", PersonB, "Night.", PersonAvailability::Awake, 1380, 360, {}, {}, true, false, {}, 0},
    {"node.t.b.busy", PersonB, "Busy.", PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.t.b.asleep", PersonB, "Asleep.", PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
}};

// Id, Group, Implies
const std::array<ConversationFactDef, 5> MainFacts{{
    {FactAsked, "", ""},
    {FactGift, "", ""},
    {FactNote, "", ""},
    {FactModeX, "mode", FactAsked},
    {FactModeY, "mode", FactAsked},
}};

const ConversationCatalogue Main{MainNodes, MainFacts};

std::string_view NodeAt(const ConversationState& state, std::string_view person, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(Main, person, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

bool Offers(const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(Main, state))
    {
        if (view.Id == choiceId)
            return true;
    }
    return false;
}

void Open(ConversationState& state, std::string_view person, std::int32_t minute)
{
    Check(OpenConversation(Main, state, person, minute, PersonAvailability::Awake) == ConversationError::None, "the test conversation opens");
}

void TestCatalogueIsValidAndGeneral()
{
    Check(ValidateConversationCatalogue(Main), "a catalogue of other people and other facts validates by its own data");
    Check(ValidateConversationCatalogue(), "the default catalogue validates");
    Check(FindConversationNode(Main, "node.t.a.greet") != nullptr && FindConversationNode(Main, "node.household_adult.late") == nullptr,
          "nodes are found in the catalogue given, not in the default one");
    Check(FindConversationChoice(Main, "choice.t.ask") == &MainNodes[0].Choices[0], "a choice offered by several nodes resolves to its first offer");
    Check(FindConversationChoice(Main, ConversationLeaveChoiceId) == nullptr, "Leave is not an authored choice");
    Check(FindConversationFact(Main, FactModeX) != nullptr && FindConversationFact(Main, "fact.nope") == nullptr, "facts are found by id");
    Check(PersonAvailabilityId(PersonAvailability::Asleep) == "asleep" && PersonAvailabilityId(PersonAvailability::Busy) == "busy" &&
              PersonAvailabilityId(PersonAvailability::Awake) == "awake",
          "availability ids");
    Check(ConversationErrorCode(ConversationError::AlreadyTaken) == "already_taken" && ConversationErrorCode(ConversationError::StaleChoice) == "stale_choice" &&
              ConversationErrorCode(ConversationError::AlreadyCharged) == "already_charged" && ConversationErrorCode(ConversationError::SessionLimit) == "session_limit",
          "error codes are stable");
}

void TestSelection()
{
    ConversationState none;
    Check(NodeAt(none, PersonA, 0) == "node.t.a.greet" && NodeAt(none, PersonA, 1439) == "node.t.a.greet", "a person with no time windows speaks all day");
    Check(NodeAt(none, PersonB, 360) == "node.t.b.day" && NodeAt(none, PersonB, 1379) == "node.t.b.day", "a window starts and ends on its half-open edges");
    Check(NodeAt(none, PersonB, 1380) == "node.t.b.night" && NodeAt(none, PersonB, 0) == "node.t.b.night" && NodeAt(none, PersonB, 359) == "node.t.b.night",
          "a window with an earlier end wraps past midnight");
    ConversationState asked;
    asked.Facts = {std::string(FactAsked)};
    Check(NodeAt(asked, PersonA, 100) == "node.t.a.followup", "a required fact selects another node");
    ConversationState gifted;
    gifted.Facts = {std::string(FactGift)};
    Check(NodeAt(gifted, PersonB, 600) == "node.t.b.day_gift" && NodeAt(gifted, PersonB, 100) == "node.t.b.night", "facts of one person's node come from the shared state");
    for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 17)
    {
        Check(NodeAt(none, PersonA, minute, PersonAvailability::Asleep) == "node.t.a.asleep" && NodeAt(none, PersonB, minute, PersonAvailability::Busy) == "node.t.b.busy",
              "asleep and busy decline at every hour");
    }
    Check(SelectConversationNode(Main, PersonA, -1, none, PersonAvailability::Awake) == nullptr && SelectConversationNode(Main, PersonA, 1440, none, PersonAvailability::Awake) == nullptr,
          "a minute outside the day selects nothing");
    Check(SelectConversationNode(Main, "person.t.nobody", 100, none, PersonAvailability::Awake) == nullptr, "an unknown person selects nothing");
    Check(!ConversationNodeServes(MainNodes[4], 100, none, PersonAvailability::Awake), "a reply node is never served by selection");
}

void TestOpenAndRefusals()
{
    ConversationState state;
    Check(ChooseConversationChoice(Main, state, Ask.Id).Error == ConversationError::NotOpen, "choosing with nothing open is refused");
    Check(LeaveConversation(state) == ConversationError::NotOpen, "leaving with nothing open is refused");
    Check(VisibleConversationChoices(Main, state).empty(), "nothing to show with nothing open");
    Check(OpenConversation(Main, state, "person.t.nobody", 100, PersonAvailability::Awake) == ConversationError::UnknownPerson, "an unknown person is refused");
    Check(OpenConversation(Main, state, PersonA, 1440, PersonAvailability::Awake) == ConversationError::InvalidTime, "a bad minute is refused");
    Check(state == ConversationState{}, "refusals left the state untouched");
    Open(state, PersonA, 100);
    Check(state.Open && state.PersonId == PersonA && state.NodeId == "node.t.a.greet" && state.SessionCount == 1, "a valid open records the session and counts it");
    const ConversationState open = state;
    Check(OpenConversation(Main, state, PersonA, 100, PersonAvailability::Awake) == ConversationError::AlreadyOpen && state == open, "opening twice is refused and changes nothing");
    Check(ChooseConversationChoice(Main, state, "choice.t.nope").Error == ConversationError::StaleChoice, "an unknown choice is stale");
    Check(ChooseConversationChoice(Main, state, Pay.Id).Error == ConversationError::StaleChoice, "a choice of another node is stale");
    Check(ChooseConversationChoice(Main, state, Tear.Id).Error == ConversationError::StaleChoice && state == open, "a stale choice changes nothing");

    ConversationState maxed;
    maxed.SessionCount = std::numeric_limits<std::uint32_t>::max();
    Check(IsConversationStateValid(Main, maxed), "a state at the largest session count is valid");
    Check(OpenConversation(Main, maxed, PersonA, 100, PersonAvailability::Awake) == ConversationError::SessionLimit && !maxed.Open, "the session counter never wraps");
}

void TestPaidChoiceReceiptsAndRecurrence()
{
    ConversationState state;
    Open(state, PersonA, 100);
    Check(Offers(state, Ask.Id), "the paid choice is offered");
    ConversationChooseResult result = ChooseConversationChoice(Main, state, Ask.Id);
    Check(result.Ok() && result.CostMinutes == 4 && result.ChoiceId == Ask.Id && result.Session == 1, "choosing returns the cost once, with the key (choice, session)");
    Check(result.FactSet == FactAsked && result.NextNodeId == "node.t.a.reply.asked" && !result.bEnded, "and the fact and the next node");
    Check(state.Receipts.size() == 1 && state.Receipts[0].ChoiceId == Ask.Id && state.Receipts[0].Session == 1 && WasChoiceChargedThisSession(state, Ask.Id),
          "a receipt records the charge for this session");
    Check(!Offers(state, Ask.Id), "a charged choice is no longer offered in this session, even where the graph offers it again");
    const ConversationState before = state;
    result = ChooseConversationChoice(Main, state, Ask.Id);
    Check(result.Error == ConversationError::AlreadyCharged && result.CostMinutes == 0 && result.FactSet.empty() && state == before,
          "choosing it again in the same session is refused without a charge or a change");
    Check(IsConversationStateValid(Main, state), "the state with a receipt is valid");

    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(state.Receipts.size() == 1, "leaving keeps the receipt (the integrator may still be settling the charge)");
    Check(WasChoiceChargedThisSession(state, Ask.Id), "and it still reads as charged");
    Open(state, PersonA, 100);
    Check(state.SessionCount == 2 && state.Receipts.empty() && !WasChoiceChargedThisSession(state, Ask.Id), "a new session starts with no receipts");
    Check(state.NodeId == "node.t.a.followup" && Offers(state, Ask.Id), "the topic is not exhausted: the same paid choice is offered again");
    result = ChooseConversationChoice(Main, state, Ask.Id);
    Check(result.Ok() && result.CostMinutes == 4 && result.Session == 2 && result.FactSet.empty(), "and charged again in the new session, awarding nothing twice");
    Check(state.Receipts.size() == 1 && state.Receipts[0].Session == 2, "with a fresh receipt");

    // A paid choice that ends the session.
    ConversationState b;
    Open(b, PersonB, 600);
    result = ChooseConversationChoice(Main, b, Pay.Id);
    Check(result.Ok() && result.CostMinutes == 6 && result.bEnded && !b.Open && b.Receipts.size() == 1, "a paid choice with no next node ends the session and leaves its receipt");
}

void TestFreeChoiceIsRepeatable()
{
    ConversationState state;
    Open(state, PersonA, 100);
    ConversationChooseResult result = ChooseConversationChoice(Main, state, PickX.Id);
    Check(result.Ok() && result.CostMinutes == 0 && state.Receipts.empty(), "a free choice leaves no receipt");
    Check(Offers(state, PickX.Id) == false, "the node moved on; PickX is not on the reply node");
    // Force the node back: the free choice is idempotent but hidden while its own fact holds.
    state.NodeId = "node.t.a.followup";
    Check(!Offers(state, PickX.Id) && Offers(state, PickY.Id), "a choice is hidden while its forbidden fact is set");
    Check(ChooseConversationChoice(Main, state, PickX.Id).Error == ConversationError::StaleChoice, "and refused as stale");
}

void TestOnceEver()
{
    ConversationState state;
    Open(state, PersonA, 100);
    ConversationChooseResult result = ChooseConversationChoice(Main, state, Gift.Id);
    Check(result.Ok() && result.FactSet == FactGift && result.bEnded && WasChoiceTaken(state, Gift.Id), "a one-time choice is taken once and recorded");
    Open(state, PersonA, 100);
    Check(!Offers(state, Gift.Id), "and never offered again");
    state.NodeId = "node.t.a.greet";
    state.Facts.clear(); // pretend the fact were gone: the choice stays taken
    const ConversationState forced = state;
    result = ChooseConversationChoice(Main, state, Gift.Id);
    Check(result.Error == ConversationError::AlreadyTaken && result.CostMinutes == 0 && state == forced, "choosing it again is refused as taken, with no change");
}

void TestFactsGroupImplicationAndClearing()
{
    ConversationState state;
    Open(state, PersonA, 100);
    ConversationChooseResult result = ChooseConversationChoice(Main, state, PickX.Id);
    Check(result.Ok() && result.FactSet == FactModeX, "a choice sets its fact");
    Check(HasConversationFact(state, FactModeX) && HasConversationFact(state, FactAsked), "and what the fact implies");
    Check(IsConversationStateValid(Main, state), "the state is valid");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Open(state, PersonA, 100);
    Check(state.NodeId == "node.t.a.followup", "the implied fact selects the follow-up");
    result = ChooseConversationChoice(Main, state, PickY.Id);
    Check(result.Ok() && result.FactSet == FactModeY && result.FactCleared == FactModeX, "setting another fact of the group replaces the first");
    Check(!HasConversationFact(state, FactModeX) && HasConversationFact(state, FactModeY) && HasConversationFact(state, FactAsked), "the group holds one fact, the implication stays");
    Check(IsConversationStateValid(Main, state), "still valid");

    // Clearing a fact, and hiding a choice while its condition fails.
    ConversationState noting;
    Open(noting, PersonA, 100);
    Check(ChooseConversationChoice(Main, noting, Ask.Id).Ok(), "ask");
    Check(Offers(noting, Note.Id) && !Offers(noting, Tear.Id), "Tear is hidden until there is a note");
    Check(ChooseConversationChoice(Main, noting, Tear.Id).Error == ConversationError::StaleChoice, "and refused as stale");
    Check(ChooseConversationChoice(Main, noting, Note.Id).FactSet == FactNote, "write the note");
    noting.NodeId = "node.t.a.reply.asked";
    Check(!Offers(noting, Note.Id) && Offers(noting, Tear.Id), "Note is hidden once written, Tear appears");
    result = ChooseConversationChoice(Main, noting, Tear.Id);
    Check(result.Ok() && result.FactCleared == FactNote && !HasConversationFact(noting, FactNote), "Tear clears the note");
}

void TestLeaveAlwaysPresent()
{
    for (const ConversationNodeDef& node : MainNodes)
    {
        ConversationState state;
        state.Open = true;
        state.SessionCount = 1;
        state.PersonId = std::string(node.PersonId);
        state.NodeId = std::string(node.Id);
        const auto choices = VisibleConversationChoices(Main, state);
        Check(!choices.empty() && choices.back().bIsLeave && choices.back().Id == ConversationLeaveChoiceId && choices.back().CostMinutes == 0,
              "every node, replies and declines included, ends its list with a free Leave");
        Check(ChooseConversationChoice(Main, state, ConversationLeaveChoiceId).bEnded && !state.Open, "Leave ends the session from any node");
    }
    ConversationState asleep;
    Check(OpenConversation(Main, asleep, PersonA, 100, PersonAvailability::Asleep) == ConversationError::None && asleep.NodeId == "node.t.a.asleep", "an asleep person opens a polite decline");
    const auto choices = VisibleConversationChoices(Main, asleep);
    Check(choices.size() == 1 && choices[0].bIsLeave, "which offers only Leave");
}

void TestDeterminism()
{
    const auto play = [] {
        ConversationState state;
        (void)OpenConversation(Main, state, PersonA, 100, PersonAvailability::Awake);
        (void)ChooseConversationChoice(Main, state, Ask.Id);
        (void)LeaveConversation(state);
        (void)OpenConversation(Main, state, PersonA, 700, PersonAvailability::Awake);
        (void)ChooseConversationChoice(Main, state, PickY.Id);
        return state;
    };
    Check(play() == play(), "the same inputs give the same state");
    Check(SerialiseConversationState(Main, play()) == SerialiseConversationState(Main, play()), "and the same text");
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "test text contains the part to replace");
    text.replace(at, from.size(), to);
    return text;
}

bool Parses(std::string_view text)
{
    return DeserialiseConversationState(Main, text).Ok();
}

void TestRoundTripAndRefusedText()
{
    const std::string freshText = SerialiseConversationState(Main, ConversationState{});
    Check(freshText.rfind("pw.conversation/2\n", 0) == 0, "the text opens with the current header");
    ConversationStateParse parsed = DeserialiseConversationState(Main, freshText);
    Check(parsed.Ok() && parsed.State == ConversationState{} && !parsed.bMigrated, "a fresh state round trips and is not a migration");

    ConversationState state;
    Open(state, PersonA, 100);
    (void)ChooseConversationChoice(Main, state, Ask.Id);
    const std::string mid = SerialiseConversationState(Main, state);
    parsed = DeserialiseConversationState(Main, mid);
    Check(parsed.Ok() && parsed.State == state && parsed.State.Open && SerialiseConversationState(Main, parsed.State) == mid, "a mid-session state with a receipt round trips exactly");
    Check(mid.find("receipt=choice.t.ask@1\n") != std::string::npos && mid.find("sessions=1\n") != std::string::npos, "the receipt and session counter are written");

    state = ConversationState{};
    Open(state, PersonA, 100);
    (void)ChooseConversationChoice(Main, state, Gift.Id);
    Check(ChooseConversationChoice(Main, state, Ask.Id).Error == ConversationError::NotOpen, "the session ended with the gift");
    const std::string closed = SerialiseConversationState(Main, state);
    parsed = DeserialiseConversationState(Main, closed);
    Check(parsed.Ok() && parsed.State == state && !parsed.State.Open && parsed.State.TakenChoices.size() == 1, "a closed state with a taken choice round trips");

    ConversationState invalid;
    invalid.Facts.emplace_back("fact.invented");
    Check(SerialiseConversationState(Main, invalid).empty(), "an inconsistent state serialises to nothing");

    for (std::size_t length = 0; length < mid.size(); ++length)
        Check(!Parses(std::string_view(mid).substr(0, length)), "every truncation is refused");
    Check(!Parses(mid + "x"), "trailing data is refused");
    Check(!Parses(Replace(mid, "pw.conversation/2", "pw.conversation/3")), "an unknown newer header is refused");
    Check(!Parses(Replace(mid, "open=1", "open=2")), "a bad open flag is refused");
    Check(!Parses(Replace(mid, "sessions=1", "sessions=0")), "an open session with a zero counter is refused");
    Check(!Parses(Replace(mid, "sessions=1", "sessions=01")), "a non-canonical counter is refused");
    Check(!Parses(Replace(mid, "sessions=1", "sessions=4294967296")), "a counter beyond 32 bits is refused");
    Check(!Parses(Replace(mid, "person=person.t.a", "person=person.t.b")), "a node of another person is refused");
    Check(!Parses(Replace(mid, "node=node.t.a.reply.asked", "node=node.t.nope")), "an unknown node is refused");
    Check(!Parses(Replace(mid, "receipt=choice.t.ask@1", "receipt=choice.t.ask@2")), "a receipt of another session is refused");
    Check(!Parses(Replace(mid, "receipt=choice.t.ask@1", "receipt=choice.t.ask")), "a receipt with no session is refused");
    Check(!Parses(Replace(mid, "receipt=choice.t.ask@1", "receipt=choice.t.nope@1")), "a receipt for an unknown choice is refused");
    Check(!Parses(Replace(mid, "receipts=1", "receipts=2")), "a wrong receipt count is refused");
    Check(!Parses(Replace(mid, "fact=fact.t.asked", "fact=fact.invented")), "an unknown fact is refused");
    Check(!Parses(Replace(closed, "taken=1\nchoice=choice.t.gift", "taken=1\nchoice=choice.t.ask")), "a taken choice that is not one-time is refused");
    Check(!Parses(Replace(closed, "taken=1\nchoice=choice.t.gift", "taken=1\nchoice=choice.t.nope")), "an unknown taken choice is refused");
    const ConversationStateParse bad = DeserialiseConversationState(Main, "garbage\n");
    Check(!bad.Ok() && bad.State == ConversationState{}, "a refusal returns a default state");

    // Facts that break the catalogue's rules.
    ConversationState conflicting;
    conflicting.Facts = {std::string(FactAsked), std::string(FactModeX), std::string(FactModeY)};
    Check(!IsConversationStateValid(Main, conflicting), "two facts of one group are not a valid state");
    ConversationState lacking;
    lacking.Facts = {std::string(FactModeX)};
    Check(!IsConversationStateValid(Main, lacking), "a fact without what it implies is not a valid state");
    ConversationState unsorted;
    unsorted.Facts = {std::string(FactNote), std::string(FactAsked)};
    Check(!IsConversationStateValid(Main, unsorted), "facts out of order are not a valid state");
    ConversationState freeReceipt;
    freeReceipt.SessionCount = 1;
    freeReceipt.Receipts.push_back(ConversationReceipt{std::string(PickX.Id), 1});
    Check(!IsConversationStateValid(Main, freeReceipt), "a receipt for a free choice is not a valid state");
    ConversationState staleReceipt;
    staleReceipt.SessionCount = 2;
    staleReceipt.Receipts.push_back(ConversationReceipt{std::string(Ask.Id), 1});
    Check(!IsConversationStateValid(Main, staleReceipt), "a receipt of an earlier session is not a valid state");
}

bool ValidWith(const std::vector<ConversationNodeDef>& nodes, const std::vector<ConversationFactDef>& facts)
{
    return ValidateConversationCatalogue(ConversationCatalogue{nodes, facts});
}

void TestCatalogueValidationRefusals()
{
    const std::vector<ConversationNodeDef> baseNodes(MainNodes.begin(), MainNodes.end());
    const std::vector<ConversationFactDef> baseFacts(MainFacts.begin(), MainFacts.end());
    Check(ValidWith(baseNodes, baseFacts), "the unmodified copy validates");
    Check(!ValidateConversationCatalogue(ConversationCatalogue{}), "an empty catalogue is invalid");

    auto nodes = baseNodes;
    nodes[1].Id = nodes[0].Id;
    Check(!ValidWith(nodes, baseFacts), "a duplicate node id is invalid");
    nodes = baseNodes;
    nodes[0].Choices[1].NextNode = "node.t.nope";
    Check(!ValidWith(nodes, baseFacts), "a link to no node is invalid");
    nodes = baseNodes;
    nodes[0].Choices[1].NextNode = "node.t.b.day";
    Check(!ValidWith(nodes, baseFacts), "a link to another person's node is invalid");
    nodes = baseNodes;
    nodes.erase(nodes.begin() + 9);
    Check(!ValidWith(nodes, baseFacts), "a gap in a person's day is invalid");
    nodes = baseNodes;
    nodes[9].FromMinute = 1300;
    Check(!ValidWith(nodes, baseFacts), "two nodes serving one situation is invalid");
    nodes = baseNodes;
    nodes[8].RequiresFacts = {};
    Check(!ValidWith(nodes, baseFacts), "two nodes with no telling condition is invalid");
    nodes = baseNodes;
    nodes.push_back(ConversationNodeDef{"node.t.a.orphan", PersonA, "Alone.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0});
    Check(!ValidWith(nodes, baseFacts), "a reply nothing leads to is invalid");
    nodes = baseNodes;
    nodes[1].Choices[0].CostMinutes = 5;
    Check(!ValidWith(nodes, baseFacts), "one choice id offered differently by two nodes is invalid");
    nodes = baseNodes;
    nodes[0].Choices[1].SetsFact = "fact.t.nope";
    Check(!ValidWith(nodes, baseFacts), "a choice setting an unknown fact is invalid");
    nodes = baseNodes;
    nodes[4].Choices[2].ClearsFact = FactAsked;
    Check(!ValidWith(nodes, baseFacts), "clearing a fact another fact implies is invalid");
    nodes = baseNodes;
    nodes[7].Choices[0].CostMinutes = 61;
    Check(!ValidWith(nodes, baseFacts), "a cost beyond an hour is invalid");
    nodes = baseNodes;
    nodes[2].Choices[0] = Pay;
    nodes[2].ChoiceCount = 1;
    Check(!ValidWith(nodes, baseFacts), "a decline offering choices is invalid");
    nodes = baseNodes;
    nodes[3].bDeclines = false;
    Check(!ValidWith(nodes, baseFacts), "an asleep person who is not declining is invalid");
    nodes = baseNodes;
    nodes[0].PersonId = "a";
    Check(!ValidWith(nodes, baseFacts), "a person id without the person prefix is invalid");
    nodes = baseNodes;
    nodes[7].RequiresFacts = {"fact.t.nope", ""};
    Check(!ValidWith(nodes, baseFacts), "a node conditioned on an unknown fact is invalid");

    auto facts = baseFacts;
    facts[0].Implies = FactModeX; // asked <-> mode_x
    Check(!ValidWith(baseNodes, facts), "an implication loop is invalid");
    facts = baseFacts;
    facts.push_back(ConversationFactDef{"fact.t.orphan", "", ""});
    Check(!ValidWith(baseNodes, facts), "a fact nothing can set is invalid");
    facts = baseFacts;
    facts[1].Implies = FactModeY; // an exclusive-group fact that something else implies
    Check(!ValidWith(baseNodes, facts), "a group fact that another fact implies is invalid");
    facts = baseFacts;
    facts[2].Id = "note";
    Check(!ValidWith(baseNodes, facts), "a fact id without the fact prefix is invalid");
    facts = baseFacts;
    facts.push_back(facts[0]);
    Check(!ValidWith(baseNodes, facts), "a duplicate fact is invalid");
}
} // namespace

int main()
{
    TestCatalogueIsValidAndGeneral();
    TestSelection();
    TestOpenAndRefusals();
    TestPaidChoiceReceiptsAndRecurrence();
    TestFreeChoiceIsRepeatable();
    TestOnceEver();
    TestFactsGroupImplicationAndClearing();
    TestLeaveAlwaysPresent();
    TestDeterminism();
    TestRoundTripAndRefusedText();
    TestCatalogueValidationRefusals();
    std::cout << "PerfectWorld conversation: " << Checks << " checks passed\n";
    return 0;
}
