// The household adult's conversation (LANE_LIFE_PLAN section B; gpt-6-astra critique of 2026-10-14): the evening-jobs talk offers
// concrete proposals, the agreed arrangement is remembered by WHAT it is and never as having worked, "talk it over again" leads
// to real alternatives, a paid talk may recur but is never charged twice in a session, and an old "pw.conversation/1" text is
// migrated honestly. The general runtime has its own file (ConversationTests.cpp).
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

constexpr std::int32_t Morning = 7 * 60;
constexpr std::int32_t Afternoon = 15 * 60;
constexpr std::int32_t Evening = 20 * 60;
constexpr std::int32_t Night = 23 * 60;

constexpr std::string_view TalkRota = "choice.household_adult.talk_rota";
constexpr std::string_view RevisitRota = "choice.household_adult.revisit_rota";
constexpr std::string_view KeepRota = "choice.household_adult.keep_rota";
constexpr std::string_view BeforeDark = "choice.household_adult.back_before_dark";
constexpr std::string_view ProposeDishes = "choice.household_adult.propose_dishes";
constexpr std::string_view ProposeAlternate = "choice.household_adult.propose_alternate";
constexpr std::string_view ProposeSweepCook = "choice.household_adult.propose_sweep_cook";

struct Arrangement
{
    std::string_view Proposal;
    std::string_view ProposalText;
    std::string_view Fact;
    std::string_view Reply;
    std::string_view Remembered;
    std::string_view Kept;
    std::string_view Settled;
};

constexpr Arrangement Arrangements[3] = {
    {ProposeDishes, "I take the dishes on weekdays.", FactRotaDishesWeekdays, "node.household_adult.reply.dishes_weekdays",
     "node.household_adult.remembered_dishes_weekdays", "node.household_adult.kept_dishes_weekdays", "node.household_adult.rota_settled_dishes_weekdays"},
    {ProposeAlternate, "We alternate days.", FactRotaAlternateDays, "node.household_adult.reply.alternate_days",
     "node.household_adult.remembered_alternate_days", "node.household_adult.kept_alternate_days", "node.household_adult.rota_settled_alternate_days"},
    {ProposeSweepCook, "I sweep and you cook.", FactRotaSweepAndCook, "node.household_adult.reply.sweep_and_cook",
     "node.household_adult.remembered_sweep_and_cook", "node.household_adult.kept_sweep_and_cook", "node.household_adult.rota_settled_sweep_and_cook"},
};

void Open(ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    Check(OpenConversation(state, HouseholdAdultId, minute, availability) == ConversationError::None, "the household adult opens a conversation");
}

std::string_view NodeAt(const ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(HouseholdAdultId, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

std::string_view LineOf(std::string_view nodeId)
{
    const ConversationNodeDef* node = FindConversationNode(nodeId);
    Check(node != nullptr, "the node exists");
    return node != nullptr ? node->Line : std::string_view{};
}

bool Offers(const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(state))
    {
        if (view.Id == choiceId)
            return true;
    }
    return false;
}

std::vector<std::string_view> OfferedIds(const ConversationState& state)
{
    std::vector<std::string_view> ids;
    for (const ConversationChoiceView& view : VisibleConversationChoices(state))
    {
        if (!view.bIsLeave)
            ids.push_back(view.Id);
    }
    return ids;
}

bool Contains(std::string_view text, std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

/** Walks the evening talk to the end: open at 20:00, talk (3 minutes), propose; returns the state, closed. */
ConversationState AgreeInTheEvening(const Arrangement& arrangement, std::int32_t* charged = nullptr)
{
    ConversationState state;
    Open(state, Evening);
    ConversationChooseResult result = ChooseConversationChoice(state, TalkRota);
    Check(result.Ok(), "the evening talk is accepted");
    if (charged != nullptr)
        *charged += result.CostMinutes;
    result = ChooseConversationChoice(state, arrangement.Proposal);
    Check(result.Ok(), "the proposal is accepted");
    if (charged != nullptr)
        *charged += result.CostMinutes;
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    return state;
}

void TestCatalogue()
{
    Check(ValidateConversationCatalogue(), "the household catalogue is well formed and every situation selects exactly one node");
    Check(ValidateConversationCatalogue(HouseholdConversationCatalogue()) && DefaultConversationCatalogue().Nodes.data() == HouseholdConversationCatalogue().Nodes.data(),
          "the default catalogue is the household's");
    Check(HouseholdAdultId == "person.household_adult" && HouseholdAdultLabel == "Your parent", "the unnamed household adult keeps its role label");
    Check(ConversationNodes().size() == 23, "twenty-three authored nodes");
    for (const ConversationNodeDef& node : ConversationNodes())
    {
        Check(node.PersonId == HouseholdAdultId, "every node is the household adult's");
        Check(node.ChoiceCount <= 3 && !node.Line.empty(), "at most three authored choices, and every node speaks");
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
        {
            Check(!node.Choices[i].bOnceEver, "no household choice is one-time: every talk may be had again");
            Check(!Contains(node.Choices[i].Text, "minute"), "a choice never states its own cost; the presenter does");
        }
        Check(std::all_of(node.Line.begin(), node.Line.end(), [](char c) { return c >= 32 && c < 127; }), "lines are plain ASCII");
        Check(!Contains(node.Line, "worked") && !Contains(node.Line, "working well") && !Contains(node.Line, "yesterday"),
              "no line claims a result or a day that nothing records");
    }
    Check(FindConversationNode("node.household_adult.breakfast")->Line == "Breakfast is ready. Tell me what time you expect to be back.", "the first sample line is verbatim");
    Check(LineOf("node.household_adult.rota_open") == "We still need to agree who does the evening jobs.", "the second sample line is verbatim");
    Check(FindConversationChoice(TalkRota)->CostMinutes == 3 && FindConversationChoice(RevisitRota)->CostMinutes == 3, "the two talks cost three minutes");
    Check(FindConversationChoice(KeepRota)->CostMinutes == 0 && FindConversationChoice(ProposeDishes)->CostMinutes == 0, "keeping and proposing are free");
    Check(FindConversationChoice(TalkRota)->Text == "Talk about the evening jobs", "the talk keeps its text");
    for (const Arrangement& arrangement : Arrangements)
        Check(FindConversationChoice(arrangement.Proposal)->Text == arrangement.ProposalText, "the proposals are the player's own words");
}

// An independent sweep of the coverage rule over the states the facts allow, using only the public node test.
void TestEverySituationSelectsExactlyOneNode()
{
    std::vector<std::vector<std::string_view>> factSets{{}, {FactEveningRotaAgreed}, {FactEveningRotaAgreed, FactEveningRotaKept}};
    for (const Arrangement& arrangement : Arrangements)
    {
        factSets.push_back({FactEveningRotaAgreed, arrangement.Fact});
        factSets.push_back({FactEveningRotaAgreed, FactEveningRotaKept, arrangement.Fact});
    }
    for (const std::vector<std::string_view>& facts : factSets)
    {
        ConversationState state;
        for (const std::string_view fact : facts)
            state.Facts.emplace_back(fact);
        std::sort(state.Facts.begin(), state.Facts.end());
        Check(IsConversationStateValid(state), "each probe state is valid");
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        {
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int matches = 0;
                for (const ConversationNodeDef& node : ConversationNodes())
                    matches += ConversationNodeServes(node, minute, state, static_cast<PersonAvailability>(a)) ? 1 : 0;
                Check(matches == 1, "exactly one node serves every minute, availability and set of facts");
            }
        }
    }
}

void TestSelectionByTime()
{
    const ConversationState none;
    Check(NodeAt(none, 5 * 60) == "node.household_adult.breakfast" && NodeAt(none, 10 * 60 + 59) == "node.household_adult.breakfast", "breakfast runs 05:00 to 10:59");
    Check(NodeAt(none, 11 * 60) == "node.household_adult.rota_open" && NodeAt(none, 21 * 60 + 59) == "node.household_adult.rota_open", "the open jobs run 11:00 to 21:59");
    Check(NodeAt(none, 22 * 60) == "node.household_adult.late" && NodeAt(none, 0) == "node.household_adult.late" && NodeAt(none, 4 * 60 + 59) == "node.household_adult.late",
          "the late line wraps midnight, 22:00 to 04:59");
    Check(SelectConversationNode(HouseholdAdultId, -1, none, PersonAvailability::Awake) == nullptr, "a negative minute selects nothing");
    Check(SelectConversationNode(HouseholdAdultId, 1440, none, PersonAvailability::Awake) == nullptr, "minute 1440 selects nothing");
    Check(SelectConversationNode("person.nobody", Morning, none, PersonAvailability::Awake) == nullptr, "an unknown person selects nothing");
}

void TestDeclines()
{
    for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 13)
    {
        Check(NodeAt(ConversationState{}, minute, PersonAvailability::Asleep) == "node.household_adult.asleep", "asleep declines at every hour");
        Check(NodeAt(ConversationState{}, minute, PersonAvailability::Busy) == "node.household_adult.busy", "busy declines at every hour");
    }
    ConversationState state;
    Open(state, Morning, PersonAvailability::Asleep);
    const auto choices = VisibleConversationChoices(state);
    Check(choices.size() == 1 && choices[0].bIsLeave, "a decline offers only Leave");
    Check(ChooseConversationChoice(state, ConversationLeaveChoiceId).bEnded, "which is free");
}

void TestGreetingIsFreeAndRepeatable()
{
    ConversationState state;
    Open(state, Morning);
    Check(OfferedIds(state).size() == 3, "breakfast offers its three answers");
    ConversationChooseResult result = ChooseConversationChoice(state, BeforeDark);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet.empty() && state.Receipts.empty() && state.Facts.empty(), "an answer is free and records nothing");
    Check(state.NodeId == "node.household_adult.reply.back_noted", "and moves to the reply");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Open(state, Morning);
    Check(ChooseConversationChoice(state, BeforeDark).Ok(), "it may be answered again");
}

void TestEveningTalkOffersConcreteProposals()
{
    ConversationState state;
    Open(state, Evening);
    Check(state.NodeId == "node.household_adult.rota_open" && state.SessionCount == 1, "in the evening the jobs are open");
    auto choices = VisibleConversationChoices(state);
    Check(choices.size() == 2 && choices[0].Id == TalkRota && choices[0].CostMinutes == 3 && choices[1].bIsLeave, "the talk shows its cost before commitment");

    ConversationChooseResult result = ChooseConversationChoice(state, TalkRota);
    Check(result.Ok() && result.CostMinutes == 3 && result.ChoiceId == TalkRota && result.Session == 1 && result.FactSet.empty(), "talking costs three minutes once and decides nothing yet");
    Check(state.NodeId == "node.household_adult.reply.proposals" && LineOf(state.NodeId) == "All right. What would you like to suggest?", "the parent asks for a suggestion");
    Check(state.Facts.empty() && state.Receipts.size() == 1, "nothing is agreed, and the charge has its receipt");
    choices = VisibleConversationChoices(state);
    Check(choices.size() == 4 && choices[0].Id == ProposeDishes && choices[1].Id == ProposeAlternate && choices[2].Id == ProposeSweepCook && choices[3].bIsLeave,
          "three concrete proposals, then Leave");
    for (std::size_t i = 0; i < 3; ++i)
        Check(choices[i].Text == Arrangements[i].ProposalText && choices[i].CostMinutes == 0, "each proposal is the player's own sentence, and free");

    // Leaving without proposing agrees nothing, and a talk later this evening is a new, paid session.
    Check(LeaveConversation(state) == ConversationError::None, "leave without proposing");
    Open(state, Evening);
    Check(state.NodeId == "node.household_adult.rota_open" && state.Facts.empty(), "the jobs are still open");
    Check(Offers(state, TalkRota) && state.Receipts.empty(), "and the talk is offered again with no old receipt");
}

void TestEachProposalIsRecordedAsTheAgreedArrangement()
{
    for (const Arrangement& arrangement : Arrangements)
    {
        std::int32_t charged = 0;
        ConversationState state = AgreeInTheEvening(arrangement, &charged);
        Check(charged == 3, "the whole agreement cost three minutes");
        Check(HasConversationFact(state, arrangement.Fact) && HasConversationFact(state, FactEveningRotaAgreed) && !HasConversationFact(state, FactEveningRotaKept),
              "the proposal is recorded as the agreed arrangement, not yet kept");
        Check(IsConversationStateValid(state), "the state is valid");
        int arrangementFacts = 0;
        for (const Arrangement& other : Arrangements)
            arrangementFacts += HasConversationFact(state, other.Fact) ? 1 : 0;
        Check(arrangementFacts == 1, "exactly one arrangement is held");

        // The reply and the settled line say what was agreed.
        ConversationState replyCheck;
        Open(replyCheck, Evening);
        (void)ChooseConversationChoice(replyCheck, TalkRota);
        const ConversationChooseResult result = ChooseConversationChoice(replyCheck, arrangement.Proposal);
        Check(result.FactSet == arrangement.Fact && result.NextNodeId == arrangement.Reply && !result.bEnded, "the proposal sets its fact and moves to its reply");
        Check(Offers(replyCheck, ConversationLeaveChoiceId) && OfferedIds(replyCheck).empty(), "the reply offers only Leave");

        Open(state, Afternoon);
        Check(state.NodeId == arrangement.Settled, "the same day it is settled in its own words");
        Check(Offers(state, RevisitRota) && !Offers(state, TalkRota), "with a way to talk it over again");
        Check(LeaveConversation(state) == ConversationError::None, "leave");

        Open(state, Morning);
        Check(state.NodeId == arrangement.Remembered, "the next morning the parent remembers what was agreed");
        Check(!Contains(LineOf(state.NodeId), "worked") && Contains(LineOf(state.NodeId), "suggested") && Contains(LineOf(state.NodeId), "Shall we keep it"),
              "as a suggestion to keep, with no claim that it worked");
        Check(OfferedIds(state).size() == 2 && Offers(state, KeepRota) && Offers(state, RevisitRota), "keep it, or talk it over again");
        Check(LeaveConversation(state) == ConversationError::None, "leave");
    }
    Check(LineOf("node.household_adult.remembered_dishes_weekdays") == "Good morning. You suggested taking the dishes on weekdays. Shall we keep it that way?", "the dishes line");
    Check(LineOf("node.household_adult.remembered_alternate_days") == "Good morning. You suggested we alternate days. Shall we keep it that way?", "the alternate-days line");
    Check(LineOf("node.household_adult.remembered_sweep_and_cook") == "Good morning. You suggested that you sweep and I cook. Shall we keep it that way?", "the sweep-and-cook line");
}

void TestKeepingItIsRememberedWithoutClaimingSuccess()
{
    ConversationState state = AgreeInTheEvening(Arrangements[1]);
    Open(state, Morning);
    const ConversationChooseResult result = ChooseConversationChoice(state, KeepRota);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == FactEveningRotaKept && state.Receipts.empty(), "keeping it is free and remembered");
    Check(state.NodeId == "node.household_adult.reply.kept" && LineOf(state.NodeId) == "Good. Then we both know where we stand.", "and answered plainly");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Open(state, Morning);
    Check(state.NodeId == "node.household_adult.kept_alternate_days", "the next breakfast keeps to what was agreed");
    Check(LineOf(state.NodeId) == "Breakfast is ready. We are keeping to what we agreed: we alternate days.", "naming it");
    Check(OfferedIds(state).size() == 1 && Offers(state, RevisitRota) && !Offers(state, KeepRota), "and still allows talking it over");
    Check(IsConversationStateValid(state), "the state is valid");
}

void TestTalkingItOverAgainLeadsToRealAlternatives()
{
    for (std::size_t current = 0; current < 3; ++current)
    {
        ConversationState state = AgreeInTheEvening(Arrangements[current]);
        Open(state, Morning);
        ConversationChooseResult result = ChooseConversationChoice(state, KeepRota);
        Check(result.Ok(), "keep it first");
        Check(LeaveConversation(state) == ConversationError::None, "leave");

        Open(state, Morning);
        result = ChooseConversationChoice(state, RevisitRota);
        Check(result.Ok() && result.CostMinutes == 3 && result.NextNodeId == "node.household_adult.reply.revisit", "talking it over again costs three minutes");
        Check(LineOf(state.NodeId) == "All right. Tell me what you would change.", "the parent asks what would change");
        const std::vector<std::string_view> alternatives = OfferedIds(state);
        Check(alternatives.size() == 2, "two alternatives are offered");
        for (std::size_t other = 0; other < 3; ++other)
        {
            const bool offered = std::find(alternatives.begin(), alternatives.end(), Arrangements[other].Proposal) != alternatives.end();
            Check(offered == (other != current), "every proposal but the present arrangement is offered");
        }

        const std::size_t next = (current + 1) % 3;
        result = ChooseConversationChoice(state, Arrangements[next].Proposal);
        Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == Arrangements[next].Fact && result.FactCleared == FactEveningRotaKept,
              "choosing an alternative replaces the arrangement");
        Check(HasConversationFact(state, Arrangements[next].Fact) && !HasConversationFact(state, Arrangements[current].Fact) &&
                  HasConversationFact(state, FactEveningRotaAgreed),
              "the new one is held, the old is not");
        Check(!HasConversationFact(state, FactEveningRotaKept), "a change withdraws the keeping that belonged to the old arrangement");
        Check(IsConversationStateValid(state), "the state is valid");
        Check(LeaveConversation(state) == ConversationError::None, "leave");
        Open(state, Morning);
        Check(state.NodeId == Arrangements[next].Remembered && Offers(state, KeepRota), "the next morning asks about the new one");
    }
}

void TestPaidTalkRecursButIsNotChargedTwiceInOneSession()
{
    ConversationState state = AgreeInTheEvening(Arrangements[0]);
    std::int32_t charged = 0;
    for (int day = 0; day < 3; ++day)
    {
        Open(state, Morning);
        const ConversationChooseResult result = ChooseConversationChoice(state, RevisitRota);
        Check(result.Ok() && result.Session == state.SessionCount, "each day the talk is available and keyed to its own session");
        charged += result.CostMinutes;
        // Back at the same line in the same session: the charge is not repeated.
        state.NodeId = std::string(Arrangements[0].Remembered);
        const ConversationState before = state;
        const ConversationChooseResult again = ChooseConversationChoice(state, RevisitRota);
        Check(again.Error == ConversationError::AlreadyCharged && again.CostMinutes == 0 && state == before, "choosing it again in the same session is refused with no charge");
        Check(!Offers(state, RevisitRota) && Offers(state, KeepRota), "and it is not offered for the rest of the session");
        Check(LeaveConversation(state) == ConversationError::None, "leave");
    }
    Check(charged == 9, "three separate talks were charged three minutes each");
}

void TestStateTextRoundTrips()
{
    ConversationState state = AgreeInTheEvening(Arrangements[2]);
    Open(state, Morning);
    (void)ChooseConversationChoice(state, RevisitRota);
    const std::string text = SerialiseConversationState(state);
    Check(text.rfind("pw.conversation/2\n", 0) == 0 && text.find("receipt=choice.household_adult.revisit_rota@2\n") != std::string::npos, "the text is the current version with the receipt");
    const ConversationStateParse parsed = DeserialiseConversationState(text);
    Check(parsed.Ok() && !parsed.bMigrated && parsed.State == state && SerialiseConversationState(parsed.State) == text, "and round trips exactly");
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialiseConversationState(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
}

// --- migration of the first version's text ---
constexpr const char* LegacyFresh = "pw.conversation/1\nopen=0\nperson=\nnode=\ntaken=0\nfacts=0\nend\n";
constexpr const char* LegacyAgreed =
    "pw.conversation/1\nopen=0\nperson=\nnode=\ntaken=1\nchoice=choice.household_adult.talk_rota\nfacts=1\nfact=fact.evening_rota_agreed\nend\n";
constexpr const char* LegacyKeptAndRevisited =
    "pw.conversation/1\nopen=0\nperson=\nnode=\ntaken=3\nchoice=choice.household_adult.keep_rota\nchoice=choice.household_adult.revisit_rota\n"
    "choice=choice.household_adult.talk_rota\nfacts=2\nfact=fact.evening_rota_agreed\nfact=fact.evening_rota_kept\nend\n";
constexpr const char* LegacyOpenAtRemovedNode =
    "pw.conversation/1\nopen=1\nperson=person.household_adult\nnode=node.household_adult.reply.rota_agreed\ntaken=1\nchoice=choice.household_adult.talk_rota\n"
    "facts=1\nfact=fact.evening_rota_agreed\nend\n";
constexpr const char* LegacyOpenAtRotaOpen =
    "pw.conversation/1\nopen=1\nperson=person.household_adult\nnode=node.household_adult.rota_open\ntaken=0\nfacts=0\nend\n";

void TestOldTextIsMigratedHonestly()
{
    ConversationStateParse parsed = DeserialiseConversationState(LegacyFresh);
    Check(parsed.Ok() && parsed.bMigrated && parsed.State == ConversationState{}, "an empty old state migrates to the empty state");

    parsed = DeserialiseConversationState(LegacyAgreed);
    Check(parsed.Ok() && parsed.bMigrated, "an old agreement migrates");
    Check(parsed.State.Facts.size() == 1 && HasConversationFact(parsed.State, FactEveningRotaAgreed), "the agreement is kept: it is never erased");
    Check(parsed.State.TakenChoices.empty() && parsed.State.Receipts.empty() && parsed.State.SessionCount == 0 && !parsed.State.Open,
          "the once-only record is dropped and no receipt is invented");
    Check(NodeAt(parsed.State, Morning) == "node.household_adult.remembered_unnamed" && NodeAt(parsed.State, Afternoon) == "node.household_adult.rota_unnamed",
          "an old agreement never said how the jobs are shared, so the parent says so and offers to put it plainly");
    Check(!Contains(LineOf("node.household_adult.remembered_unnamed"), "worked") && Offers(parsed.State, TalkRota) == false, "(not open yet, so nothing is offered)");
    const std::string rewritten = SerialiseConversationState(parsed.State);
    Check(rewritten.rfind("pw.conversation/2\n", 0) == 0, "serialising writes the current version");
    Check(DeserialiseConversationState(rewritten).Ok() && !DeserialiseConversationState(rewritten).bMigrated, "which reads back as current");

    // The unnamed agreement can be put into words, which replaces the vagueness.
    ConversationState state = parsed.State;
    Open(state, Morning);
    Check(state.NodeId == "node.household_adult.remembered_unnamed" && Offers(state, TalkRota), "the morning offers the talk");
    Check(ChooseConversationChoice(state, TalkRota).CostMinutes == 3, "which costs three minutes");
    Check(ChooseConversationChoice(state, ProposeSweepCook).FactSet == FactRotaSweepAndCook, "and a proposal names the arrangement");
    Check(IsConversationStateValid(state), "the state is valid");

    parsed = DeserialiseConversationState(LegacyKeptAndRevisited);
    Check(parsed.Ok() && parsed.bMigrated && parsed.State.Facts.size() == 2 && HasConversationFact(parsed.State, FactEveningRotaKept), "an old kept agreement stays kept");
    Check(parsed.State.TakenChoices.empty(), "with its taken choices dropped");
    Check(NodeAt(parsed.State, Morning) == "node.household_adult.remembered_unnamed", "and still not claimed to be anything specific");

    parsed = DeserialiseConversationState(LegacyOpenAtRemovedNode);
    Check(parsed.Ok() && parsed.bMigrated && !parsed.State.Open && parsed.State.PersonId.empty() && parsed.State.NodeId.empty() && parsed.State.SessionCount == 1,
          "a session left open at a line that no longer exists is closed (leaving is free), the agreement stays");
    Check(HasConversationFact(parsed.State, FactEveningRotaAgreed), "and its agreement is kept");

    parsed = DeserialiseConversationState(LegacyOpenAtRotaOpen);
    Check(parsed.Ok() && parsed.State.Open && parsed.State.NodeId == "node.household_adult.rota_open" && parsed.State.SessionCount == 1,
          "a session open at a line that still exists stays open, as session one");
    Check(ChooseConversationChoice(parsed.State, TalkRota).Ok(), "and can go on");
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "test text contains the part to replace");
    text.replace(at, from.size(), to);
    return text;
}

void TestOldTextThatCouldNotHaveBeenWrittenIsRefused()
{
    const std::string agreed = LegacyAgreed;
    for (std::size_t length = 0; length < agreed.size(); ++length)
        Check(!DeserialiseConversationState(std::string_view(agreed).substr(0, length)).Ok(), "every truncation of an old text is refused");
    Check(!DeserialiseConversationState(agreed + "x").Ok(), "trailing data is refused");
    Check(!DeserialiseConversationState(Replace(agreed, "pw.conversation/1", "pw.conversation/0")).Ok(), "an unknown older header is refused");
    Check(!DeserialiseConversationState(Replace(agreed, "choice=choice.household_adult.talk_rota", "choice=choice.household_adult.back_before_dark")).Ok(),
          "a free choice recorded as taken is refused");
    Check(!DeserialiseConversationState(Replace(agreed, "choice=choice.household_adult.talk_rota", "choice=choice.household_adult.nope")).Ok(), "an unknown taken choice is refused");
    Check(!DeserialiseConversationState(Replace(agreed, "fact=fact.evening_rota_agreed", "fact=fact.invented")).Ok(), "an unknown fact is refused");
    Check(!DeserialiseConversationState(Replace(agreed, "fact=fact.evening_rota_agreed", "fact=fact.evening_rota_dishes_weekdays")).Ok(),
          "a fact the old version could not set is refused");
    Check(!DeserialiseConversationState(Replace(agreed, "taken=1\nchoice=choice.household_adult.talk_rota\n", "taken=0\n")).Ok(), "a fact whose choice was not taken is refused");
    Check(!DeserialiseConversationState(Replace(agreed, "facts=1\nfact=fact.evening_rota_agreed\n", "facts=0\n")).Ok(), "a taken choice whose fact is missing is refused");
    Check(!DeserialiseConversationState(Replace(LegacyKeptAndRevisited, "fact=fact.evening_rota_agreed\nfact=fact.evening_rota_kept", "fact=fact.evening_rota_kept\nfact=fact.evening_rota_agreed")).Ok(),
          "facts out of order are refused");
    Check(!DeserialiseConversationState(Replace(LegacyKeptAndRevisited, "taken=3\nchoice=choice.household_adult.keep_rota\nchoice=choice.household_adult.revisit_rota\n",
                                                "taken=2\nchoice=choice.household_adult.revisit_rota\n"))
               .Ok(),
          "a kept fact whose choice was not taken is refused");
    Check(!DeserialiseConversationState(Replace(LegacyOpenAtRotaOpen, "person=person.household_adult", "person=person.other")).Ok(), "an open session of the wrong person is refused");
    Check(!DeserialiseConversationState(Replace(LegacyOpenAtRotaOpen, "open=1", "open=2")).Ok(), "a bad open flag is refused");
}
} // namespace

int main()
{
    TestCatalogue();
    TestEverySituationSelectsExactlyOneNode();
    TestSelectionByTime();
    TestDeclines();
    TestGreetingIsFreeAndRepeatable();
    TestEveningTalkOffersConcreteProposals();
    TestEachProposalIsRecordedAsTheAgreedArrangement();
    TestKeepingItIsRememberedWithoutClaimingSuccess();
    TestTalkingItOverAgainLeadsToRealAlternatives();
    TestPaidTalkRecursButIsNotChargedTwiceInOneSession();
    TestStateTextRoundTrips();
    TestOldTextIsMigratedHonestly();
    TestOldTextThatCouldNotHaveBeenWrittenIsRefused();
    std::cout << "PerfectWorld conversation household: " << Checks << " checks passed\n";
    return 0;
}
