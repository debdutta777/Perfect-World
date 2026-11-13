// The care worker's conversation (LANE_LIFE_PLAN section B; gpt-6-astra critique of 2026-10-14): bounded duty hours with a
// handover to "the evening staff", polite off-duty declines that point to who is on duty, a concrete shared-jobs talk with
// remembered agreement facts and no claim of results, a paid talk charged once per session by receipt, and never a parental
// label. The general runtime has its own file (ConversationTests.cpp); the family home's is ConversationHouseholdTests.cpp.
#include "Conversation.h"
#include "LaneOrigin.h"

#include <algorithm>
#include <cctype>
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

constexpr std::string_view CareId = "person.care_worker";
constexpr std::string_view CareLabel = "Care worker";

constexpr std::int32_t Morning = 7 * 60 + 30;
constexpr std::int32_t Midday = 12 * 60;
constexpr std::int32_t Evening = 18 * 60;
constexpr std::int32_t Night = 23 * 60;

constexpr std::string_view FactKept = "fact.care_jobs_kept";
constexpr std::string_view TalkJobs = "choice.care_worker.talk_jobs";
constexpr std::string_view RevisitJobs = "choice.care_worker.revisit_jobs";
constexpr std::string_view KeepJobs = "choice.care_worker.keep_jobs";
constexpr std::string_view BeforeDark = "choice.care_worker.back_before_dark";

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
    {"choice.care_worker.propose_breakfast", "I will set out breakfast on weekdays.", "fact.care_jobs_breakfast_weekdays", "node.care_worker.reply.breakfast_weekdays",
     "node.care_worker.remembered_breakfast_weekdays", "node.care_worker.kept_breakfast_weekdays", "node.care_worker.jobs_settled_breakfast_weekdays"},
    {"choice.care_worker.propose_turns", "I will take turns with the others, day by day.", "fact.care_jobs_turns", "node.care_worker.reply.turns",
     "node.care_worker.remembered_turns", "node.care_worker.kept_turns", "node.care_worker.jobs_settled_turns"},
    {"choice.care_worker.propose_study_room", "I will tidy the study room after homework hour.", "fact.care_jobs_study_room", "node.care_worker.reply.study_room",
     "node.care_worker.remembered_study_room", "node.care_worker.kept_study_room", "node.care_worker.jobs_settled_study_room"},
};

const ConversationCatalogue Care = CareConversationCatalogue();

void Open(ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    Check(OpenConversation(Care, state, CareId, minute, availability) == ConversationError::None, "the care worker opens a conversation");
}

std::string_view NodeAt(const ConversationState& state, std::int32_t minute, PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(Care, CareId, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}

std::string_view LineOf(std::string_view nodeId)
{
    const ConversationNodeDef* node = FindConversationNode(Care, nodeId);
    Check(node != nullptr, "the node exists");
    return node != nullptr ? node->Line : std::string_view{};
}

bool Offers(const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(Care, state))
    {
        if (view.Id == choiceId)
            return true;
    }
    return false;
}

std::size_t OfferedCount(const ConversationState& state)
{
    std::size_t count = 0;
    for (const ConversationChoiceView& view : VisibleConversationChoices(Care, state))
        count += view.bIsLeave ? 0 : 1;
    return count;
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

/** Whole-word search on lower-cased text, so "mum" is not found inside "summer". */
bool HasWord(std::string_view text, std::string_view word)
{
    const std::string lower = Lower(text);
    std::size_t at = 0;
    while ((at = lower.find(word, at)) != std::string::npos)
    {
        const bool startOk = at == 0 || !std::isalpha(static_cast<unsigned char>(lower[at - 1]));
        const std::size_t end = at + word.size();
        const bool endOk = end >= lower.size() || !std::isalpha(static_cast<unsigned char>(lower[end]));
        if (startOk && endOk)
            return true;
        ++at;
    }
    return false;
}

/** Walks the midday talk to the end: open at 12:00, talk (3 minutes), propose; returns the state, closed. */
ConversationState AgreeAtMidday(const Arrangement& arrangement, std::int32_t* charged = nullptr)
{
    ConversationState state;
    Open(state, Midday);
    ConversationChooseResult result = ChooseConversationChoice(Care, state, TalkJobs);
    Check(result.Ok(), "the talk is accepted");
    if (charged != nullptr)
        *charged += result.CostMinutes;
    result = ChooseConversationChoice(Care, state, arrangement.Proposal);
    Check(result.Ok(), "the proposal is accepted");
    if (charged != nullptr)
        *charged += result.CostMinutes;
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    return state;
}

void TestCatalogue()
{
    Check(ValidateConversationCatalogue(Care), "the care catalogue is well formed and every situation selects exactly one node");
    Check(DefaultConversationCatalogue().Nodes.data() != Care.Nodes.data(), "the care catalogue is its own; the default stays the household's");
    Check(Care.Nodes.size() == 24, "twenty-four authored nodes");
    Check(Care.Facts.size() == 4, "four facts");

    // The person matches the care binding of the lane origin, and carries the role label, not a parental one.
    bool found = false;
    for (const LaneOriginBinding& binding : AuthoredLaneOrigins())
    {
        if (binding.GrewUpInCare)
        {
            found = true;
            Check(binding.CounterpartPersonId == CareId && binding.RelationshipLabel == CareLabel, "the care origin's counterpart is this person with the label Care worker");
        }
    }
    Check(found, "the lane origin has a care binding");

    for (const ConversationNodeDef& node : Care.Nodes)
    {
        Check(node.PersonId == CareId, "every node is the care worker's");
        Check(node.ChoiceCount <= 3 && !node.Line.empty(), "at most three authored choices, and every node speaks");
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
        {
            Check(!node.Choices[i].bOnceEver, "no care choice is one-time: every talk may be had again");
            Check(!Contains(node.Choices[i].Text, "minute"), "a choice never states its own cost; the presenter does");
            Check(Contains(node.Choices[i].Id, "choice.care_worker."), "choice ids are the care worker's own, so catalogues compose without clashes");
        }
        Check(Contains(node.Id, "node.care_worker."), "node ids are the care worker's own");
        Check(std::all_of(node.Line.begin(), node.Line.end(), [](char c) { return c >= 32 && c < 127; }), "lines are plain ASCII");
        Check(!Contains(node.Line, "worked") && !Contains(node.Line, "working well") && !Contains(node.Line, "yesterday") && !Contains(node.Line, "succeeded"),
              "no line claims a result or a day that nothing records");
        for (const std::string_view word : {"parent", "mother", "father", "mum", "mom", "dad", "mummy", "daddy", "son", "daughter", "family", "child", "children", "kid", "orphan",
                                            "poor", "pity", "dear", "sweetie", "honey", "institution", "ward", "inmate", "resident", "supervisor", "behave"})
            Check(!HasWord(node.Line, word), "no parental label, pity or institutional coldness in a line");
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
        {
            for (const std::string_view word : {"parent", "mother", "father", "mum", "mom", "dad", "child", "kid", "orphan", "poor"})
                Check(!HasWord(node.Choices[i].Text, word), "nor in the player's own words");
        }
    }
    Check(!Contains(CareLabel, "parent") && !Contains(CareLabel, "Your"), "the label is a role, not a relationship to the player");
    for (const ConversationFactDef& fact : Care.Facts)
        Check(Contains(fact.Id, "fact.care_jobs_"), "fact ids are the care worker's own");

    Check(FindConversationChoice(Care, TalkJobs)->CostMinutes == 3 && FindConversationChoice(Care, RevisitJobs)->CostMinutes == 3, "the two talks cost three minutes");
    Check(FindConversationChoice(Care, KeepJobs)->CostMinutes == 0 && FindConversationChoice(Care, Arrangements[0].Proposal)->CostMinutes == 0, "keeping and proposing are free");
    for (const Arrangement& arrangement : Arrangements)
        Check(FindConversationChoice(Care, arrangement.Proposal)->Text == arrangement.ProposalText, "the proposals are the player's own words");
}

// An independent sweep of the coverage rule over the states the facts allow, using only the public node test.
void TestEverySituationSelectsExactlyOneNode()
{
    std::vector<std::vector<std::string_view>> factSets{{}, {FactKept}};
    for (const Arrangement& arrangement : Arrangements)
    {
        factSets.push_back({arrangement.Fact});
        factSets.push_back({arrangement.Fact, FactKept});
    }
    for (const std::vector<std::string_view>& facts : factSets)
    {
        ConversationState state;
        for (const std::string_view fact : facts)
            state.Facts.emplace_back(fact);
        std::sort(state.Facts.begin(), state.Facts.end());
        Check(IsConversationStateValid(Care, state), "each probe state is valid");
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        {
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int matches = 0;
                for (const ConversationNodeDef& node : Care.Nodes)
                    matches += ConversationNodeServes(node, minute, state, static_cast<PersonAvailability>(a)) ? 1 : 0;
                Check(matches == 1, "exactly one node serves every minute, availability and set of facts");
            }
        }
    }
}

void TestDutyHoursAreBounded()
{
    const ConversationState none;
    Check(NodeAt(none, 6 * 60 + 59) == "node.care_worker.off_duty_late", "before seven the care worker is off duty");
    Check(NodeAt(none, 7 * 60) == "node.care_worker.breakfast" && NodeAt(none, 10 * 60 + 59) == "node.care_worker.breakfast", "on duty from seven, breakfast to 10:59");
    Check(NodeAt(none, 11 * 60) == "node.care_worker.jobs_open" && NodeAt(none, 14 * 60 + 59) == "node.care_worker.jobs_open", "the jobs are open to 14:59");
    Check(NodeAt(none, 15 * 60) == "node.care_worker.off_duty_evening" && NodeAt(none, 21 * 60 + 59) == "node.care_worker.off_duty_evening", "the shift ends at fifteen hundred");
    Check(NodeAt(none, 22 * 60) == "node.care_worker.off_duty_late" && NodeAt(none, 0) == "node.care_worker.off_duty_late", "the late line wraps midnight");
    Check(SelectConversationNode(Care, CareId, -1, none, PersonAvailability::Awake) == nullptr, "a negative minute selects nothing");
    Check(SelectConversationNode(Care, CareId, 1440, none, PersonAvailability::Awake) == nullptr, "minute 1440 selects nothing");
    Check(SelectConversationNode(Care, "person.nobody", Morning, none, PersonAvailability::Awake) == nullptr, "an unknown person selects nothing");
    Check(SelectConversationNode(Care, HouseholdAdultId, Morning, none, PersonAvailability::Awake) == nullptr, "the household adult is not in the care catalogue");

    // The shift is one of several: it ends, the evening staff take over, and nobody is on duty around the clock.
    for (const std::string_view node : {"node.care_worker.off_duty_evening", "node.care_worker.busy_evening"})
        Check(Contains(LineOf(node), "evening staff") && Contains(LineOf(node), "handover note") && Contains(LineOf(node), "three") && Contains(LineOf(node), "seven"),
              "the evening lines name the shift end, the evening staff, the handover note and the return");
    for (const std::string_view node : {"node.care_worker.off_duty_late", "node.care_worker.busy_late", "node.care_worker.asleep"})
        Check(Contains(LineOf(node), "off duty until seven") && Contains(LineOf(node), "staff on duty"), "the late lines say the care worker is off duty and point to the staff on duty");
    for (const ConversationNodeDef& node : Care.Nodes)
    {
        Check(!Contains(node.Line, "always here") && !Contains(node.Line, "all day and night") && !Contains(node.Line, "any time") && !Contains(node.Line, "twenty-four"),
              "no line implies one employee is always on duty");
        Check(!Contains(node.Line, "Anita") && !Contains(node.Line, "Mr ") && !Contains(node.Line, "Mrs ") && !Contains(node.Line, "Ms "),
              "no second named person is invented");
    }
}

void TestDeclinesPointToWhoIsOnDuty()
{
    for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 13)
    {
        Check(NodeAt(ConversationState{}, minute, PersonAvailability::Asleep) == "node.care_worker.asleep", "asleep declines at every hour");
        const std::string_view busy = NodeAt(ConversationState{}, minute, PersonAvailability::Busy);
        if (minute >= 7 * 60 && minute < 15 * 60)
            Check(busy == "node.care_worker.busy_on_duty", "busy on duty is in the middle of something");
        else if (minute >= 15 * 60 && minute < 22 * 60)
            Check(busy == "node.care_worker.busy_evening", "busy in the evening means the shift is over");
        else
            Check(busy == "node.care_worker.busy_late", "busy late means off duty until seven");
    }
    Check(Contains(LineOf("node.care_worker.busy_on_duty"), "a little while"), "an on-duty decline offers a later time");
    // An off-duty schedule reads Busy, and must never claim "in the middle of something".
    Check(!Contains(LineOf("node.care_worker.busy_evening"), "middle of something") && !Contains(LineOf("node.care_worker.busy_late"), "middle of something"),
          "an off-duty decline does not pretend to be busy");
    for (const PersonAvailability availability : {PersonAvailability::Asleep, PersonAvailability::Busy})
    {
        ConversationState state;
        Open(state, Morning, availability);
        const auto choices = VisibleConversationChoices(Care, state);
        Check(choices.size() == 1 && choices[0].bIsLeave, "a decline offers only Leave");
        Check(ChooseConversationChoice(Care, state, ConversationLeaveChoiceId).bEnded, "which is free");
    }
}

void TestGreetingIsFreeAndRepeatable()
{
    ConversationState state;
    Open(state, Morning);
    Check(state.NodeId == "node.care_worker.breakfast" && OfferedCount(state) == 3 && Offers(state, TalkJobs), "breakfast offers two answers and the talk");
    ConversationChooseResult result = ChooseConversationChoice(Care, state, BeforeDark);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet.empty() && state.Receipts.empty() && state.Facts.empty(), "an answer is free and records nothing");
    Check(state.NodeId == "node.care_worker.reply.back_noted", "and moves to the reply");
    Check(Contains(LineOf(state.NodeId), "evening staff") && !Contains(LineOf(state.NodeId), "will pass"), "the reply names the evening staff and promises only the note");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Open(state, Morning);
    Check(ChooseConversationChoice(Care, state, BeforeDark).Ok(), "it may be answered again");
}

void TestTalkOffersConcreteProposals()
{
    ConversationState state;
    Open(state, Midday);
    Check(state.NodeId == "node.care_worker.jobs_open" && state.SessionCount == 1, "at midday the jobs are open");
    auto choices = VisibleConversationChoices(Care, state);
    Check(choices.size() == 2 && choices[0].Id == TalkJobs && choices[0].CostMinutes == 3 && choices[1].bIsLeave, "the talk shows its cost before commitment");

    ConversationChooseResult result = ChooseConversationChoice(Care, state, TalkJobs);
    Check(result.Ok() && result.CostMinutes == 3 && result.ChoiceId == TalkJobs && result.Session == 1 && result.FactSet.empty(), "talking costs three minutes once and decides nothing yet");
    Check(state.NodeId == "node.care_worker.reply.proposals" && LineOf(state.NodeId) == "All right. What would you like to take on?", "the care worker asks what the player would take on");
    Check(state.Facts.empty() && state.Receipts.size() == 1, "nothing is agreed, and the charge has its receipt");
    choices = VisibleConversationChoices(Care, state);
    Check(choices.size() == 4 && choices[0].Id == Arrangements[0].Proposal && choices[1].Id == Arrangements[1].Proposal && choices[2].Id == Arrangements[2].Proposal &&
              choices[3].bIsLeave,
          "three concrete proposals, then Leave");
    for (std::size_t i = 0; i < 3; ++i)
        Check(choices[i].Text == Arrangements[i].ProposalText && choices[i].CostMinutes == 0, "each proposal is the player's own sentence, and free");

    Check(LeaveConversation(state) == ConversationError::None, "leave without proposing");
    Open(state, Midday);
    Check(state.NodeId == "node.care_worker.jobs_open" && state.Facts.empty(), "the jobs are still open");
    Check(Offers(state, TalkJobs) && state.Receipts.empty(), "and the talk is offered again with no old receipt");
}

void TestEachProposalIsRecordedAsTheAgreedArrangement()
{
    for (const Arrangement& arrangement : Arrangements)
    {
        std::int32_t charged = 0;
        ConversationState state = AgreeAtMidday(arrangement, &charged);
        Check(charged == 3, "the whole agreement cost three minutes");
        Check(HasConversationFact(state, arrangement.Fact) && !HasConversationFact(state, FactKept), "the proposal is recorded as the agreed arrangement, not yet kept");
        Check(IsConversationStateValid(Care, state), "the state is valid");
        int held = 0;
        for (const Arrangement& other : Arrangements)
            held += HasConversationFact(state, other.Fact) ? 1 : 0;
        Check(held == 1, "exactly one arrangement is held");

        ConversationState replyCheck;
        Open(replyCheck, Midday);
        (void)ChooseConversationChoice(Care, replyCheck, TalkJobs);
        const ConversationChooseResult result = ChooseConversationChoice(Care, replyCheck, arrangement.Proposal);
        Check(result.FactSet == arrangement.Fact && result.NextNodeId == arrangement.Reply && !result.bEnded, "the proposal sets its fact and moves to its reply");
        Check(Offers(replyCheck, ConversationLeaveChoiceId) && OfferedCount(replyCheck) == 0, "the reply offers only Leave");
        Check(!Contains(LineOf(arrangement.Reply), "worked") && !Contains(LineOf(arrangement.Reply), "done"), "the reply claims no result");

        Open(state, Midday);
        Check(state.NodeId == arrangement.Settled, "the same day it is settled in its own words");
        Check(Offers(state, RevisitJobs) && !Offers(state, TalkJobs), "with a way to talk it over again");
        Check(LeaveConversation(state) == ConversationError::None, "leave");

        Open(state, Morning);
        Check(state.NodeId == arrangement.Remembered, "the next morning the care worker remembers what was agreed");
        Check(Contains(LineOf(state.NodeId), "You suggested") && Contains(LineOf(state.NodeId), "Shall we keep it"), "as a suggestion to keep, with no claim that it worked");
        Check(OfferedCount(state) == 2 && Offers(state, KeepJobs) && Offers(state, RevisitJobs), "keep it, or talk it over again");
        Check(LeaveConversation(state) == ConversationError::None, "leave");

        // In the evening the care worker is off duty and the agreement stays put, untouched.
        Open(state, Evening);
        Check(state.NodeId == "node.care_worker.off_duty_evening" && OfferedCount(state) == 0 && HasConversationFact(state, arrangement.Fact),
              "off duty there is nothing to take, and the agreement is remembered");
    }
    Check(LineOf("node.care_worker.remembered_breakfast_weekdays") == "Good morning. You suggested setting out breakfast on weekdays. Shall we keep it that way?",
          "the breakfast line");
    Check(LineOf("node.care_worker.remembered_turns") == "Good morning. You suggested taking turns with the others, day by day. Shall we keep it that way?", "the turns line");
    Check(LineOf("node.care_worker.remembered_study_room") == "Good morning. You suggested tidying the study room after homework hour. Shall we keep it that way?",
          "the study-room line");
}

void TestKeepingItIsRememberedWithoutClaimingSuccess()
{
    ConversationState state = AgreeAtMidday(Arrangements[1]);
    Open(state, Morning);
    const ConversationChooseResult result = ChooseConversationChoice(Care, state, KeepJobs);
    Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == FactKept && state.Receipts.empty(), "keeping it is free and remembered");
    Check(state.NodeId == "node.care_worker.reply.kept" && LineOf(state.NodeId) == "Good. Then it is clear what we agreed.", "and answered plainly");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Open(state, Morning);
    Check(state.NodeId == "node.care_worker.kept_turns", "the next breakfast keeps to what was agreed");
    Check(LineOf(state.NodeId) == "Good morning. We are keeping to what we agreed: you take turns with the others, day by day.", "naming it");
    Check(OfferedCount(state) == 1 && Offers(state, RevisitJobs) && !Offers(state, KeepJobs), "and still allows talking it over");
    Check(IsConversationStateValid(Care, state), "the state is valid");
}

void TestTalkingItOverAgainLeadsToRealAlternatives()
{
    for (std::size_t current = 0; current < 3; ++current)
    {
        ConversationState state = AgreeAtMidday(Arrangements[current]);
        Open(state, Morning);
        Check(ChooseConversationChoice(Care, state, KeepJobs).Ok(), "keep it first");
        Check(LeaveConversation(state) == ConversationError::None, "leave");

        Open(state, Morning);
        ConversationChooseResult result = ChooseConversationChoice(Care, state, RevisitJobs);
        Check(result.Ok() && result.CostMinutes == 3 && result.NextNodeId == "node.care_worker.reply.revisit", "talking it over again costs three minutes");
        Check(LineOf(state.NodeId) == "All right. Tell me what you would change.", "the care worker asks what would change");
        Check(OfferedCount(state) == 2, "two alternatives are offered");
        for (std::size_t other = 0; other < 3; ++other)
            Check(Offers(state, Arrangements[other].Proposal) == (other != current), "every proposal but the present arrangement is offered");

        const std::size_t next = (current + 1) % 3;
        result = ChooseConversationChoice(Care, state, Arrangements[next].Proposal);
        Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == Arrangements[next].Fact && result.FactCleared == FactKept, "choosing an alternative replaces the arrangement");
        Check(HasConversationFact(state, Arrangements[next].Fact) && !HasConversationFact(state, Arrangements[current].Fact), "the new one is held, the old is not");
        Check(!HasConversationFact(state, FactKept), "a change withdraws the keeping that belonged to the old arrangement");
        Check(IsConversationStateValid(Care, state), "the state is valid");
        Check(LeaveConversation(state) == ConversationError::None, "leave");
        Open(state, Morning);
        Check(state.NodeId == Arrangements[next].Remembered && Offers(state, KeepJobs), "the next morning asks about the new one");
    }
}

void TestPaidTalkRecursButIsNotChargedTwiceInOneSession()
{
    ConversationState state = AgreeAtMidday(Arrangements[0]);
    std::int32_t charged = 0;
    for (int day = 0; day < 3; ++day)
    {
        Open(state, Morning);
        const ConversationChooseResult result = ChooseConversationChoice(Care, state, RevisitJobs);
        Check(result.Ok() && result.Session == state.SessionCount, "each day the talk is available and keyed to its own session");
        charged += result.CostMinutes;
        state.NodeId = std::string(Arrangements[0].Remembered);
        const ConversationState before = state;
        const ConversationChooseResult again = ChooseConversationChoice(Care, state, RevisitJobs);
        Check(again.Error == ConversationError::AlreadyCharged && again.CostMinutes == 0 && state == before, "choosing it again in the same session is refused with no charge");
        Check(!Offers(state, RevisitJobs) && Offers(state, KeepJobs), "and it is not offered for the rest of the session");
        Check(LeaveConversation(state) == ConversationError::None, "leave");
    }
    Check(charged == 9, "three separate talks were charged three minutes each");

    // The first talk, too: once per session, then again in the next.
    ConversationState fresh;
    Open(fresh, Midday);
    Check(ChooseConversationChoice(Care, fresh, TalkJobs).CostMinutes == 3, "the first talk costs three minutes");
    fresh.NodeId = "node.care_worker.jobs_open";
    Check(ChooseConversationChoice(Care, fresh, TalkJobs).Error == ConversationError::AlreadyCharged, "and is not charged again in the session");
}

void TestStateTextRoundTrips()
{
    ConversationState state = AgreeAtMidday(Arrangements[2]);
    Open(state, Morning);
    (void)ChooseConversationChoice(Care, state, RevisitJobs);
    const std::string text = SerialiseConversationState(Care, state);
    Check(text.rfind("pw.conversation/2\n", 0) == 0 && text.find("receipt=choice.care_worker.revisit_jobs@2\n") != std::string::npos,
          "the text is the current version with the receipt");
    const ConversationStateParse parsed = DeserialiseConversationState(Care, text);
    Check(parsed.Ok() && !parsed.bMigrated && parsed.State == state && SerialiseConversationState(Care, parsed.State) == text, "and round trips exactly");
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialiseConversationState(Care, std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Check(!DeserialiseConversationState(text).Ok() || DefaultConversationCatalogue().Nodes.data() != Care.Nodes.data(),
          "the household catalogue does not accept the care text");
    Check(!DeserialiseConversationState(text).Ok(), "a care state is not valid under the household catalogue");
}
} // namespace

int main()
{
    TestCatalogue();
    TestEverySituationSelectsExactlyOneNode();
    TestDutyHoursAreBounded();
    TestDeclinesPointToWhoIsOnDuty();
    TestGreetingIsFreeAndRepeatable();
    TestTalkOffersConcreteProposals();
    TestEachProposalIsRecordedAsTheAgreedArrangement();
    TestKeepingItIsRememberedWithoutClaimingSuccess();
    TestTalkingItOverAgainLeadsToRealAlternatives();
    TestPaidTalkRecursButIsNotChargedTwiceInOneSession();
    TestStateTextRoundTrips();
    std::cout << "PerfectWorld conversation care: " << Checks << " checks passed\n";
    return 0;
}
