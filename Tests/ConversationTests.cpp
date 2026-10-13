// LANE_LIFE_IMPLEMENTATION rows 5.1 and 5.2 (minimal): the household adult's conversation catalogue, selection, costs and text form.
#include "Conversation.h"

#include <cstdlib>
#include <iostream>
#include <string>

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

constexpr std::int32_t Morning = 7 * 60;
constexpr std::int32_t Afternoon = 15 * 60;
constexpr std::int32_t Evening = 20 * 60;
constexpr std::int32_t Night = 23 * 60;

constexpr std::string_view Breakfast = "node.household_adult.breakfast";
constexpr std::string_view RotaOpen = "node.household_adult.rota_open";
constexpr std::string_view RotaSettled = "node.household_adult.rota_settled";
constexpr std::string_view Remembered = "node.household_adult.remembered";
constexpr std::string_view KeptMorning = "node.household_adult.kept_morning";
constexpr std::string_view Late = "node.household_adult.late";
constexpr std::string_view TalkRota = "choice.household_adult.talk_rota";
constexpr std::string_view KeepRota = "choice.household_adult.keep_rota";
constexpr std::string_view RevisitRota = "choice.household_adult.revisit_rota";
constexpr std::string_view BeforeDark = "choice.household_adult.back_before_dark";

std::string_view NodeAt(const ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(HouseholdAdultId, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

ConversationState WithFacts(bool agreed, bool kept)
{
    ConversationState state;
    if (agreed)
        state.Facts.emplace_back(FactEveningRotaAgreed);
    if (kept)
        state.Facts.emplace_back(FactEveningRotaKept);
    return state; // not a full valid state (no taken choices); selection only reads the facts
}

void TestCatalogue()
{
    Check(ValidateConversationCatalogue(), "the catalogue is well formed and every situation selects exactly one node");
    Check(HouseholdAdultId == "person.household_adult" && HouseholdAdultLabel == "Your parent", "the unnamed household adult keeps its role label");
    Check(ConversationNodes().size() == 12, "twelve authored nodes");
    for (const ConversationNodeDef& node : ConversationNodes())
    {
        Check(node.ChoiceCount <= 3, "at most three authored choices per node");
        Check(!node.Line.empty(), "every node speaks");
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            Check(node.Choices[i].CostMinutes == 0 || node.Choices[i].bOnce, "a costed choice is once-only");
    }
    Check(FindConversationNode(Breakfast) != nullptr && FindConversationNode("node.nope") == nullptr, "nodes are found by id");
    Check(FindConversationChoice(TalkRota) != nullptr && FindConversationChoice(TalkRota)->CostMinutes == 3, "the rota talk costs three minutes");
    Check(FindConversationChoice(ConversationLeaveChoiceId) == nullptr, "Leave is not an authored choice");
    Check(FindConversationNode(Breakfast)->Line == "Breakfast is ready. Tell me what time you expect to be back.", "the first sample line is authored verbatim");
    Check(FindConversationNode(RotaOpen)->Line == "We still need to agree who does the evening jobs.", "the second sample line is authored verbatim");
    Check(FindConversationNode(Remembered)->Line == "The arrangement you suggested worked yesterday. Shall we keep it?",
          "the third sample line is authored verbatim");
    Check(PersonAvailabilityId(PersonAvailability::Asleep) == "asleep" && PersonAvailabilityId(PersonAvailability::Busy) == "busy", "availability ids");
    Check(ConversationErrorCode(ConversationError::AlreadyTaken) == "already_taken" && ConversationErrorCode(ConversationError::StaleChoice) == "stale_choice",
          "error codes are stable");
}

void TestSelectionByTime()
{
    const ConversationState none;
    Check(NodeAt(none, 5 * 60) == Breakfast && NodeAt(none, 10 * 60 + 59) == Breakfast, "breakfast runs 05:00 to 10:59");
    Check(NodeAt(none, 11 * 60) == RotaOpen && NodeAt(none, 21 * 60 + 59) == RotaOpen, "the open rota runs 11:00 to 21:59");
    Check(NodeAt(none, 22 * 60) == Late && NodeAt(none, 0) == Late && NodeAt(none, 4 * 60 + 59) == Late, "the late line wraps midnight, 22:00 to 04:59");
    Check(NodeAt(none, Morning) == Breakfast && NodeAt(none, Afternoon) == RotaOpen && NodeAt(none, Evening) == RotaOpen && NodeAt(none, Night) == Late,
          "typical hours select as authored");
    Check(SelectConversationNode(HouseholdAdultId, -1, none, PersonAvailability::Awake) == nullptr, "a negative minute selects nothing");
    Check(SelectConversationNode(HouseholdAdultId, 1440, none, PersonAvailability::Awake) == nullptr, "minute 1440 selects nothing");
    Check(SelectConversationNode("person.nobody", Morning, none, PersonAvailability::Awake) == nullptr, "an unknown person selects nothing");
}

void TestSelectionByFact()
{
    Check(NodeAt(WithFacts(true, false), Morning) == Remembered, "the agreed arrangement is remembered the next morning");
    Check(NodeAt(WithFacts(true, false), Afternoon) == RotaSettled && NodeAt(WithFacts(true, false), Evening) == RotaSettled, "and settled by day");
    Check(NodeAt(WithFacts(true, true), Morning) == KeptMorning, "a kept arrangement changes the morning line");
    Check(NodeAt(WithFacts(true, true), Afternoon) == RotaSettled, "a kept arrangement is still settled by day");
    Check(NodeAt(WithFacts(true, false), Night) == Late && NodeAt(WithFacts(true, true), Night) == Late, "night is the same whatever is remembered");
}

void TestSelectionByAvailability()
{
    for (const bool agreed : {false, true})
    {
        const ConversationState facts = WithFacts(agreed, false);
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
        {
            Check(NodeAt(facts, minute, PersonAvailability::Asleep) == "node.household_adult.asleep", "asleep declines at every minute");
            Check(NodeAt(facts, minute, PersonAvailability::Busy) == "node.household_adult.busy", "busy declines at every minute");
            Check(!NodeAt(facts, minute, PersonAvailability::Awake).empty(), "awake always speaks");
        }
    }
    Check(FindConversationNode("node.household_adult.asleep")->bDeclines && FindConversationNode("node.household_adult.busy")->bDeclines, "decline nodes are flagged");
    Check(FindConversationNode("node.household_adult.asleep")->ChoiceCount == 0, "a decline offers no authored choices");
    Check(!FindConversationNode("node.household_adult.reply.kept")->bSelectable, "replies are never selected, only reached");

    ConversationState state;
    Check(OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Asleep) == ConversationError::None, "an asleep person still opens a session");
    Check(state.NodeId == "node.household_adult.asleep", "which shows the polite decline");
    const auto choices = VisibleConversationChoices(state);
    Check(choices.size() == 1 && choices[0].bIsLeave, "the decline offers only Leave");
}

void TestOpenAndRefusals()
{
    ConversationState state;
    Check(ChooseConversationChoice(state, TalkRota).Error == ConversationError::NotOpen, "choosing with nothing open is refused");
    Check(LeaveConversation(state) == ConversationError::NotOpen, "leaving with nothing open is refused");
    Check(VisibleConversationChoices(state).empty(), "nothing to show with nothing open");
    Check(OpenConversation(state, "person.nobody", Morning, PersonAvailability::Awake) == ConversationError::UnknownPerson, "an unknown person is refused");
    Check(OpenConversation(state, HouseholdAdultId, 1440, PersonAvailability::Awake) == ConversationError::InvalidTime, "a bad minute is refused");
    Check(state == ConversationState{}, "refusals left the state untouched");
    Check(OpenConversation(state, HouseholdAdultId, Afternoon, PersonAvailability::Awake) == ConversationError::None, "a valid open succeeds");
    Check(state.Open && state.PersonId == HouseholdAdultId && state.NodeId == RotaOpen, "and records the session");
    const ConversationState open = state;
    Check(OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Awake) == ConversationError::AlreadyOpen, "opening twice is refused");
    Check(state == open, "and changes nothing");
    Check(ChooseConversationChoice(state, "choice.nope").Error == ConversationError::StaleChoice, "an unknown choice is stale");
    Check(ChooseConversationChoice(state, KeepRota).Error == ConversationError::StaleChoice, "a choice of another node is stale");
    Check(state == open, "a stale choice changes nothing");
}

void TestLeaveAlwaysPresent()
{
    for (const ConversationNodeDef& node : ConversationNodes())
    {
        ConversationState state;
        state.Open = true;
        state.PersonId = std::string(HouseholdAdultId);
        state.NodeId = std::string(node.Id);
        const auto choices = VisibleConversationChoices(state);
        Check(!choices.empty() && choices.back().bIsLeave && choices.back().Id == ConversationLeaveChoiceId && choices.back().CostMinutes == 0,
              "every node, replies and declines included, ends its list with a free Leave");
        Check(choices.size() == node.ChoiceCount + 1, "and offers its authored choices before it");
        Check(ChooseConversationChoice(state, ConversationLeaveChoiceId).bEnded && !state.Open, "Leave ends the session from any node");
    }
}

void TestFreeGreetingIsRepeatable()
{
    ConversationState state;
    Check(OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Awake) == ConversationError::None, "open at breakfast");
    ConversationChooseResult result = ChooseConversationChoice(state, BeforeDark);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet.empty() && !result.bEnded, "a greeting answer is free and sets nothing");
    Check(result.NextNodeId == "node.household_adult.reply.back_noted" && state.NodeId == result.NextNodeId, "and moves to the reply");
    Check(state.TakenChoices.empty(), "a free greeting is not recorded as taken");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Awake) == ConversationError::None, "reopen");
    result = ChooseConversationChoice(state, BeforeDark);
    Check(result.Ok() && result.CostMinutes == 0, "the free greeting may be answered again at no cost");
    Check(state.Facts.empty(), "and still awards nothing");
}

void TestCostChargedOnceAndNoDoubleReward()
{
    ConversationState state;
    std::int32_t charged = 0;
    Check(OpenConversation(state, HouseholdAdultId, Afternoon, PersonAvailability::Awake) == ConversationError::None, "open at the open rota");
    auto choices = VisibleConversationChoices(state);
    Check(choices.size() == 2 && choices[0].Id == TalkRota && choices[0].CostMinutes == 3 && choices[1].bIsLeave, "the rota talk shows its cost before commitment");
    Check(choices[0].Text == "Talk about the evening jobs", "with its authored text");
    ConversationChooseResult result = ChooseConversationChoice(state, TalkRota);
    charged += result.CostMinutes;
    Check(result.Ok() && result.CostMinutes == 3 && result.FactSet == FactEveningRotaAgreed, "choosing returns the cost and sets the fact");
    Check(HasConversationFact(state, FactEveningRotaAgreed) && WasChoiceTaken(state, TalkRota), "the fact and the choice are remembered");
    Check(state.NodeId == "node.household_adult.reply.rota_agreed", "the session moves to the reply");
    Check(IsConversationStateValid(state), "the state is valid mid-session");

    Check(LeaveConversation(state) == ConversationError::None, "leave after the exchange");
    Check(OpenConversation(state, HouseholdAdultId, Afternoon, PersonAvailability::Awake) == ConversationError::None, "reopen the same afternoon");
    Check(state.NodeId == RotaSettled, "the settled line shows now");
    result = ChooseConversationChoice(state, TalkRota);
    Check(result.Error == ConversationError::StaleChoice && result.CostMinutes == 0, "the rota talk cannot be chosen again from here");
    Check(VisibleConversationChoices(state).size() == 1, "only Leave remains");
    Check(charged == 3, "three minutes were charged in all");

    // Force the node back to the one holding a taken choice: the choice is refused as taken, not re-charged.
    ConversationState forced = state;
    forced.NodeId = std::string(RotaOpen);
    const ConversationState forcedBefore = forced;
    result = ChooseConversationChoice(forced, TalkRota);
    Check(result.Error == ConversationError::AlreadyTaken && result.CostMinutes == 0 && result.FactSet.empty(), "a taken choice is refused without a charge or a reward");
    Check(forced == forcedBefore, "and changes nothing");
    Check(VisibleConversationChoices(forced).size() == 1, "a taken choice is no longer offered");
}

void TestRememberedArrangement()
{
    ConversationState state;
    Check(OpenConversation(state, HouseholdAdultId, Evening, PersonAvailability::Awake) == ConversationError::None, "evening of the first day");
    Check(ChooseConversationChoice(state, TalkRota).CostMinutes == 3, "agree the rota");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Awake) == ConversationError::None, "next morning");
    Check(state.NodeId == Remembered, "the arrangement is remembered");
    auto choices = VisibleConversationChoices(state);
    Check(choices.size() == 3 && choices[0].Id == KeepRota && choices[0].CostMinutes == 0 && choices[1].Id == RevisitRota &&
              choices[1].CostMinutes == 3 && choices[2].bIsLeave,
          "keep it is free, talking it over costs three minutes, Leave is last");

    // Talk it over: charged once, and the question is asked again the next morning without that option.
    ConversationChooseResult result = ChooseConversationChoice(state, RevisitRota);
    Check(result.Ok() && result.CostMinutes == 3 && result.FactSet.empty(), "talking it over costs three minutes and awards nothing");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Awake) == ConversationError::None, "reopen the same morning");
    choices = VisibleConversationChoices(state);
    Check(choices.size() == 2 && choices[0].Id == KeepRota && choices[1].bIsLeave, "the used option is withdrawn");
    result = ChooseConversationChoice(state, RevisitRota);
    Check(result.Error == ConversationError::AlreadyTaken && result.CostMinutes == 0, "it cannot be charged again");

    result = ChooseConversationChoice(state, KeepRota);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == FactEveningRotaKept, "keeping it is free and remembered");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Awake) == ConversationError::None, "reopen");
    Check(state.NodeId == KeptMorning, "the morning line now reflects the kept arrangement");
    Check(state.Facts.size() == 2 && state.TakenChoices.size() == 3, "two facts and three once-choices are remembered");
    Check(IsConversationStateValid(state), "the state stays valid");
}

void TestDeterminism()
{
    const auto play = [] {
        ConversationState state;
        (void)OpenConversation(state, HouseholdAdultId, Afternoon, PersonAvailability::Awake);
        (void)ChooseConversationChoice(state, TalkRota);
        (void)LeaveConversation(state);
        (void)OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Awake);
        (void)ChooseConversationChoice(state, KeepRota);
        return state;
    };
    Check(play() == play(), "the same inputs give the same state");
    Check(SerialiseConversationState(play()) == SerialiseConversationState(play()), "and the same text");
}

void TestRoundTrip()
{
    const ConversationState fresh;
    const std::string freshText = SerialiseConversationState(fresh);
    Check(!freshText.empty() && freshText.substr(0, 18) == "pw.conversation/1\n", "the text opens with its header");
    ConversationStateParse parsed = DeserialiseConversationState(freshText);
    Check(parsed.Ok() && parsed.State == fresh, "a fresh state round trips");

    ConversationState state;
    (void)OpenConversation(state, HouseholdAdultId, Afternoon, PersonAvailability::Awake);
    (void)ChooseConversationChoice(state, TalkRota);
    const std::string midText = SerialiseConversationState(state);
    parsed = DeserialiseConversationState(midText);
    Check(parsed.Ok() && parsed.State == state && parsed.State.Open, "a mid-session state round trips");
    Check(SerialiseConversationState(parsed.State) == midText, "and re-serialises identically");

    (void)LeaveConversation(state);
    (void)OpenConversation(state, HouseholdAdultId, Morning, PersonAvailability::Awake);
    (void)ChooseConversationChoice(state, RevisitRota);
    (void)ChooseConversationChoice(state, ConversationLeaveChoiceId);
    const std::string closedText = SerialiseConversationState(state);
    parsed = DeserialiseConversationState(closedText);
    Check(parsed.Ok() && parsed.State == state && !parsed.State.Open, "a closed state with facts and taken choices round trips");
    Check(parsed.State.TakenChoices.size() == 2 && parsed.State.Facts.size() == 1, "and keeps what was taken");

    ConversationState invalid;
    invalid.Facts.emplace_back("fact.invented");
    Check(SerialiseConversationState(invalid).empty(), "an inconsistent state serialises to nothing");
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "test text contains the part to replace");
    text.replace(at, from.size(), to);
    return text;
}

void TestRefusedText()
{
    ConversationState state;
    (void)OpenConversation(state, HouseholdAdultId, Afternoon, PersonAvailability::Awake);
    (void)ChooseConversationChoice(state, TalkRota);
    const std::string text = SerialiseConversationState(state);
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialiseConversationState(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Check(!DeserialiseConversationState(text + "x").Ok(), "trailing data is refused");
    Check(!DeserialiseConversationState(Replace(text, "pw.conversation/1", "pw.conversation/2")).Ok(), "a wrong header is refused");
    Check(!DeserialiseConversationState(Replace(text, "open=1", "open=2")).Ok(), "a bad open flag is refused");
    Check(!DeserialiseConversationState(Replace(text, "person=person.household_adult", "person=person.other")).Ok(), "a person with no such node is refused");
    Check(!DeserialiseConversationState(Replace(text, "node=node.household_adult.reply.rota_agreed", "node=node.household_adult.nope")).Ok(), "an unknown node is refused");
    Check(!DeserialiseConversationState(Replace(text, "taken=1", "taken=2")).Ok(), "a wrong count is refused");
    Check(!DeserialiseConversationState(Replace(text, "taken=1", "taken=01")).Ok(), "a non-canonical count is refused");
    Check(!DeserialiseConversationState(Replace(text, "choice=choice.household_adult.talk_rota", "choice=choice.household_adult.nope")).Ok(), "an unknown taken choice is refused");
    Check(!DeserialiseConversationState(Replace(text, "fact=fact.evening_rota_agreed", "fact=fact.invented")).Ok(), "an unknown fact is refused");
    Check(!DeserialiseConversationState(Replace(text, "facts=1\nfact=fact.evening_rota_agreed\n", "facts=0\n")).Ok(),
          "a taken choice whose fact is missing is refused");
    Check(!DeserialiseConversationState(Replace(text, "taken=1\nchoice=choice.household_adult.talk_rota\n", "taken=0\n")).Ok(),
          "a fact no taken choice set is refused");
    Check(!DeserialiseConversationState(Replace(text, "choice=choice.household_adult.talk_rota", "choice=choice.household_adult.back_before_dark")).Ok(),
          "a repeatable choice recorded as taken is refused");

    // Kept without agreed, and ids out of order.
    ConversationState both;
    (void)OpenConversation(both, HouseholdAdultId, Afternoon, PersonAvailability::Awake);
    (void)ChooseConversationChoice(both, TalkRota);
    (void)LeaveConversation(both);
    (void)OpenConversation(both, HouseholdAdultId, Morning, PersonAvailability::Awake);
    (void)ChooseConversationChoice(both, KeepRota);
    (void)LeaveConversation(both);
    const std::string bothText = SerialiseConversationState(both);
    Check(DeserialiseConversationState(bothText).Ok(), "the two-fact state parses");
    Check(!DeserialiseConversationState(Replace(Replace(Replace(bothText, "fact.evening_rota_agreed", "fact.TMP"), "fact.evening_rota_kept", "fact.evening_rota_agreed"),
                                                "fact.TMP", "fact.evening_rota_kept"))
               .Ok(),
          "facts out of order are refused");
    Check(!DeserialiseConversationState(Replace(bothText, "facts=2\nfact=fact.evening_rota_agreed\n", "facts=1\n")).Ok(), "kept without agreed is refused");
    const ConversationStateParse bad = DeserialiseConversationState("garbage\n");
    Check(!bad.Ok() && bad.State == ConversationState{}, "a refusal returns a default state");
}
} // namespace

int main()
{
    TestCatalogue();
    TestSelectionByTime();
    TestSelectionByFact();
    TestSelectionByAvailability();
    TestOpenAndRefusals();
    TestLeaveAlwaysPresent();
    TestFreeGreetingIsRepeatable();
    TestCostChargedOnceAndNoDoubleReward();
    TestRememberedArrangement();
    TestDeterminism();
    TestRoundTrip();
    TestRefusedText();
    std::cout << "PerfectWorld conversation: " << Checks << " checks passed\n";
    return 0;
}
