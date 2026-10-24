// R4-02: the two proprietors' conversations and world save (Debashis Pal, repair workshop; Mahua Saha, hardware store). Each
// catalogue validates and selects exactly one node for every availability, minute and remembered-fact state; the one agreement is
// remembered only after the free choice that sets it; the one paid choice is charged once per session by receipt and never twice,
// not even across a save in the middle of the session (world save v8); no line claims an outcome or mentions money; the two voices
// are distinct from each other and from every other catalogue. Standalone: it reuses the shared world-save builders. Their
// schedules are covered by ProprietorLifeTests.cpp.
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
int PcChecks = 0;
}

void Check(bool condition, const char* message)
{
    ++PcChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

#include "WorldSaveTestSupport.h"

namespace
{
struct PcProbe
{
    int Minute;
    std::string_view Node;       // node suffix without fact
    std::string_view Remembered; // node suffix with the fact; empty when the window never changes
};

struct PcCase
{
    std::string_view Short; // "debashis_pal"
    std::string_view Person;
    std::string_view Fact;
    std::string_view PaidShort; // the paid choice's suffix
    std::string_view PromiseShort;
    std::string_view DeclineShort;
    std::string_view PaidReplyShort;
    int OpenA0, OpenB0;
    std::vector<PcProbe> Probes;
    ConversationCatalogue (*Catalogue)() noexcept;
};

std::string PcId(const PcCase& c, std::string_view kind, std::string_view suffix) { return std::string(kind) + "." + std::string(c.Short) + "." + std::string(suffix); }
std::string PcNode(const PcCase& c, std::string_view suffix) { return PcId(c, "node", suffix); }
std::string PcChoice(const PcCase& c, std::string_view suffix) { return PcId(c, "choice", suffix); }
constexpr std::int32_t PcHour(std::int32_t hour, std::int32_t minute = 0) { return hour * 60 + minute; }

std::string PcLower(std::string_view text)
{
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return out;
}

std::string_view PcNodeAt(const PcCase& c, const ConversationCatalogue& cat, const ConversationState& state, std::int32_t minute,
                          PersonAvailability availability = PersonAvailability::Awake)
{
    const ConversationNodeDef* node = SelectConversationNode(cat, c.Person, minute, state, availability);
    return node != nullptr ? node->Id : std::string_view{};
}
bool PcOffers(const ConversationCatalogue& cat, const ConversationState& state, std::string_view choiceId)
{
    for (const ConversationChoiceView& view : VisibleConversationChoices(cat, state))
        if (view.Id == choiceId)
            return true;
    return false;
}
std::size_t PcAuthoredOffers(const ConversationCatalogue& cat, const ConversationState& state)
{
    std::size_t count = 0;
    for (const ConversationChoiceView& view : VisibleConversationChoices(cat, state))
        count += view.bIsLeave ? 0 : 1;
    return count;
}
void PcOpen(const PcCase& c, const ConversationCatalogue& cat, ConversationState& state, std::int32_t minute,
            PersonAvailability availability = PersonAvailability::Awake)
{
    Check(OpenConversation(cat, state, c.Person, minute, availability) == ConversationError::None, "a conversation opens");
}
ConversationState PcStateWithPromise(const PcCase& c, const ConversationCatalogue& cat)
{
    ConversationState state;
    PcOpen(c, cat, state, c.OpenB0 + 30);
    Check(ChooseConversationChoice(cat, state, PcChoice(c, c.PromiseShort)).Ok(), "agreeing is accepted");
    Check(LeaveConversation(state) == ConversationError::None, "leave");
    Check(HasConversationFact(state, c.Fact), "agreeing set the fact");
    return state;
}

// ---------------------------------------------------------------- shape

void PcShape(const PcCase& c)
{
    const ConversationCatalogue cat = c.Catalogue();
    Check(ValidateConversationCatalogue(cat), "the catalogue validates");
    for (const ConversationNodeDef& node : cat.Nodes)
        Check(node.PersonId == c.Person, "every node is theirs");
    const auto registered = ConversationCatalogueForPerson(c.Person);
    Check(registered.has_value() && registered->Nodes.data() == cat.Nodes.data(), "the world finds the catalogue by person");
    const ConversationState none;
    const ConversationState promised = PcStateWithPromise(c, cat);
    for (const ConversationState* state : {&none, &promised})
        for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
            for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            {
                int serving = 0;
                for (const ConversationNodeDef& node : cat.Nodes)
                    serving += ConversationNodeServes(node, minute, *state, static_cast<PersonAvailability>(a)) ? 1 : 0;
                Check(serving == 1, "exactly one node serves every availability and minute");
            }
    // The day shape, by minute alone.
    for (const PcProbe& probe : c.Probes)
    {
        Check(PcNodeAt(c, cat, none, probe.Minute) == PcNode(c, probe.Node), "the day shape without the agreement");
        const std::string expected = probe.Remembered.empty() ? PcNode(c, probe.Node) : PcNode(c, probe.Remembered);
        Check(PcNodeAt(c, cat, promised, probe.Minute) == expected, "the day shape with the agreement");
    }
    // Busy and asleep decline at every hour and offer Leave only.
    for (const PersonAvailability availability : {PersonAvailability::Busy, PersonAvailability::Asleep})
    {
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; minute += 30)
        {
            const std::string_view id = PcNodeAt(c, cat, promised, minute, availability);
            Check(id == PcNode(c, availability == PersonAvailability::Busy ? "busy" : "asleep"), "the decline node serves");
            const ConversationNodeDef* node = FindConversationNode(cat, id);
            Check(node != nullptr && node->bDeclines && node->ChoiceCount == 0, "a decline offers nothing but Leave");
        }
        ConversationState declined;
        PcOpen(c, cat, declined, c.OpenA0 + 60, availability);
        Check(PcAuthoredOffers(cat, declined) == 0 && VisibleConversationChoices(cat, declined).size() == 1, "a declined session shows Leave only");
        Check(LeaveConversation(declined) == ConversationError::None, "Leave ends a declined session");
    }
    // Every awake window can be left; the closed windows offer only their small replies.
    for (const PcProbe& probe : c.Probes)
    {
        ConversationState talk;
        PcOpen(c, cat, talk, probe.Minute);
        bool leave = false;
        for (const ConversationChoiceView& view : VisibleConversationChoices(cat, talk))
            leave = leave || view.bIsLeave;
        Check(leave && LeaveConversation(talk) == ConversationError::None && !talk.Open, "Leave is offered and ends the session");
    }
}

void PcDistinct(const PcCase& c, const PcCase& other)
{
    const ConversationCatalogue mine = c.Catalogue();
    const ConversationCatalogue theirs = other.Catalogue();
    std::vector<ConversationCatalogue> everyone{HouseholdConversationCatalogue(), CareConversationCatalogue(), FaridaConversationCatalogue(),
                                                AninditaConversationCatalogue(), MiraConversationCatalogue(), SubrataConversationCatalogue(),
                                                NirmalConversationCatalogue(), RupaConversationCatalogue(), SahanaConversationCatalogue(), theirs};
    for (const ConversationCatalogue& cat : everyone)
    {
        Check(ValidateConversationCatalogue(cat), "the other catalogues still validate");
        for (const ConversationNodeDef& node : cat.Nodes)
        {
            Check(FindConversationNode(mine, node.Id) == nullptr, "no node id is shared with another catalogue");
            for (const ConversationNodeDef& own : mine.Nodes)
            {
                if (own.Line == node.Line)
                    std::cerr << "shared line: " << own.Line << " (" << own.Id << " / " << node.Id << ")\n";
                Check(own.Line != node.Line, "no line is shared with another catalogue");
            }
            for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            {
                Check(FindConversationChoice(mine, node.Choices[i].Id) == nullptr, "no choice id is shared with another catalogue");
                for (const ConversationNodeDef& own : mine.Nodes)
                    for (std::size_t j = 0; j < own.ChoiceCount; ++j)
                        Check(own.Choices[j].Text != node.Choices[i].Text || node.Choices[i].Text == "I won't keep you.",
                              "no choice text is shared with another catalogue except the plain leave-taking");
            }
        }
        for (const ConversationFactDef& fact : cat.Facts)
            Check(FindConversationFact(mine, fact.Id) == nullptr, "no fact id is shared with another catalogue");
    }
}

// ---------------------------------------------------------------- agreement and the paid choice

void PcAgreement(const PcCase& c)
{
    const ConversationCatalogue cat = c.Catalogue();
    const std::string promise = PcChoice(c, c.PromiseShort);
    const std::string decline = PcChoice(c, c.DeclineShort);
    const std::string paid = PcChoice(c, c.PaidShort);
    const std::string notKeep = PcChoice(c, "not_keep_you");
    const ConversationState none;
    const ConversationState promised = PcStateWithPromise(c, cat);
    // No remembered line before the fact, at any hour or availability.
    for (std::size_t a = 0; a < PersonAvailabilityCount; ++a)
        for (std::int32_t minute = 0; minute < ConversationMinutesPerDay; ++minute)
            Check(PcLower(FindConversationNode(cat, PcNodeAt(c, cat, none, minute, static_cast<PersonAvailability>(a)))->Line).find("you said") == std::string::npos,
                  "no remembered line before the fact");
    // Declining and leaving never set it; the paid choice needs the agreement first.
    ConversationState state;
    PcOpen(c, cat, state, c.OpenA0 + 90);
    Check(PcOffers(cat, state, promise) && PcOffers(cat, state, decline) && !PcOffers(cat, state, paid), "the agreement is offered; the paid choice needs one first");
    Check(!ChooseConversationChoice(cat, state, paid).Ok() && !HasConversationFact(state, c.Fact), "the paid choice before agreeing is refused");
    Check(ChooseConversationChoice(cat, state, decline).Ok() && PcOffers(cat, state, promise), "declining is free and the offer stands");
    Check(LeaveConversation(state) == ConversationError::None && !HasConversationFact(state, c.Fact), "declining and leaving set no fact");
    // Agreeing sets exactly the fact and costs nothing, in either open window.
    for (const int minute : {c.OpenA0 + 10, c.OpenB0 + 10})
    {
        ConversationState agree;
        PcOpen(c, cat, agree, minute);
        const ConversationChooseResult result = ChooseConversationChoice(cat, agree, promise);
        Check(result.Ok() && result.CostMinutes == 0 && result.FactSet == c.Fact && agree.Receipts.empty(), "agreeing is free and sets exactly the fact");
        Check(result.NextNodeId == PcNode(c, "reply.promised"), "the reply to the agreement");
        Check(LeaveConversation(agree) == ConversationError::None, "leave");
    }
    Check(PcNodeAt(c, cat, promised, c.OpenA0 + 10, PersonAvailability::Busy) == PcNode(c, "busy"), "busy still declines whatever is remembered");
    // Afterwards the agreement is not offered again; the paid choice is, at ten minutes, once per session, again in the next.
    ConversationState later = promised;
    PcOpen(c, cat, later, c.OpenB0 + 40);
    Check(!PcOffers(cat, later, promise) && PcOffers(cat, later, paid) && PcOffers(cat, later, notKeep) && PcAuthoredOffers(cat, later) == 2, "do it or move on");
    for (const ConversationChoiceView& view : VisibleConversationChoices(cat, later))
        if (view.Id == paid)
            Check(view.CostMinutes == 10, "the paid choice declares ten minutes");
    const ConversationChooseResult done = ChooseConversationChoice(cat, later, paid);
    Check(done.Ok() && done.CostMinutes == 10 && done.Session == later.SessionCount && done.FactSet.empty(), "charged ten minutes, once, setting no fact");
    Check(WasChoiceChargedThisSession(later, paid) && done.NextNodeId == PcNode(c, c.PaidReplyShort), "a receipt records the charge and the reply follows");
    const ConversationState charged = later;
    const ConversationChooseResult repeat = ChooseConversationChoice(cat, later, paid);
    Check(!repeat.Ok() && repeat.CostMinutes == 0 && later == charged, "a repeat in the same session is refused and changes nothing");
    Check(LeaveConversation(later) == ConversationError::None, "leave");
    PcOpen(c, cat, later, c.OpenB0 + 100);
    Check(later.Receipts.empty() && PcOffers(cat, later, paid), "the choice is offered afresh in a later session");
    Check(ChooseConversationChoice(cat, later, notKeep).Ok() && ChooseConversationChoice(cat, later, paid).CostMinutes == 0, "moving on is free and ends the choices");
    Check(LeaveConversation(later) == ConversationError::None, "leave");
    // The closed windows offer only a free reply; the closing hour only leave-taking.
    ConversationState closed;
    PcOpen(c, cat, closed, c.OpenA0 + 270);
    Check(PcAuthoredOffers(cat, closed) <= 1 && !PcOffers(cat, closed, paid) && !PcOffers(cat, closed, promise), "while shut nothing paid and no agreement is offered");
    for (const ConversationChoiceView& view : VisibleConversationChoices(cat, closed))
        Check(view.CostMinutes == 0, "while shut every choice is free");
    Check(LeaveConversation(closed) == ConversationError::None, "leave");
    // Refusals change nothing.
    ConversationState refused;
    PcOpen(c, cat, refused, c.OpenA0 + 20);
    const ConversationState before = refused;
    for (const std::string_view bogus : {std::string_view(paid), std::string_view("choice.nonsense"), std::string_view("choice.rupa_ghosh.sit_a_while")})
        Check(!ChooseConversationChoice(cat, refused, bogus).Ok() && refused == before, "a choice not on this node is refused");
    Check(LeaveConversation(refused) == ConversationError::None && LeaveConversation(refused) != ConversationError::None, "leaving twice is refused");
    ConversationState stranger;
    Check(OpenConversation(cat, stranger, "person.nobody", PcHour(9), PersonAvailability::Awake) != ConversationError::None, "an unknown person is refused");
    // The state survives a save.
    const std::string text = SerialiseConversationState(cat, later);
    const ConversationStateParse parsed = DeserialiseConversationState(cat, text);
    Check(!text.empty() && text.rfind("pw.conversation/2", 0) == 0 && parsed.Ok() && parsed.State == later, "the state serialises and reads back identically");
}

void PcText(const PcCase& c)
{
    constexpr std::string_view Forbidden[] = {"buy", "bought", "price", "pay", "paid", "rupee", "rs.", "cost", "sold", "sell", "sale", "order", "fee",
                                              "money", "cash", "bill", "owe", "charge", "vote", "score", "discount", "cheap"};
    constexpr std::string_view Claims[] = {"repaired", "fixed", "mended", "finished", "done", "swept", "replaced", "forgot", "sorted", "counted",
                                           "returned", "you brought", "you came", "you helped", "you kept", "you promised me", "thank you for coming",
                                           "i have written"};
    const ConversationCatalogue cat = c.Catalogue();
    for (const ConversationNodeDef& node : cat.Nodes)
    {
        std::vector<std::string_view> texts{node.Line};
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            texts.push_back(node.Choices[i].Text);
        for (const std::string_view text : texts)
        {
            const std::string lower = PcLower(text);
            for (const std::string_view word : Forbidden)
                Check(lower.find(word) == std::string::npos, "no money or scoring words");
            for (const std::string_view claim : Claims)
                Check(lower.find(claim) == std::string::npos, "no claim that the agreement was kept or any other change happened");
            for (const char ch : text)
                Check(static_cast<unsigned char>(ch) >= 32 && static_cast<unsigned char>(ch) < 127, "plain ASCII text");
        }
        Check(node.Line.size() <= 120, "a line stays short");
        for (std::size_t i = 0; i < node.ChoiceCount; ++i)
            Check(node.Choices[i].CostMinutes == 0 || node.Choices[i].Id == PcChoice(c, c.PaidShort), "no paid choice but the one");
    }
    Check(FindConversationChoice(cat, PcChoice(c, c.PaidShort))->CostMinutes == 10, "the paid choice costs ten minutes");
    Check(cat.Facts.size() == 1 && cat.Facts[0].Id == c.Fact, "exactly one remembered agreement");
}

// ---------------------------------------------------------------- world save v8

void PcSaveMidSession(const PcCase& c, std::uint64_t seed)
{
    const ConversationCatalogue cat = c.Catalogue();
    const std::string person(c.Person);
    const std::string promise = PcChoice(c, c.PromiseShort);
    const std::string paid = PcChoice(c, c.PaidShort);
    WorldSim live(seed);
    // Monday 06:00 plus 640 minutes is 16:40, inside the afternoon business window of both shops.
    Check(live.Submit({CommandSource::Player, AdvanceTime{640}}).Accepted, "advance to the afternoon (16:40)");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open the first session");
    const auto beforeAgreement = live.State().Clock.Now();
    Check(live.Submit({CommandSource::Player, ConversationChoose{person, promise, 1}}).Accepted, "agreeing is accepted");
    Check(live.State().Clock.Now() == beforeAgreement, "agreeing costs no time");
    Check(HasConversationFact(live.State().Conversations.at(person), c.Fact), "the fact is set");
    Check(live.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "close the first session");
    Check(live.Submit({CommandSource::Player, AdvanceTime{10}}).Accepted, "ten minutes pass");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open the second session through the reducer");
    const auto before = live.State().Clock.Now();
    Check(live.Submit({CommandSource::Player, ConversationChoose{person, paid, 2}}).Accepted, "the paid choice is accepted through the reducer");
    Check(live.State().Clock.Now() == before + 10, "ten minutes were charged through the world clock, once");
    const ConversationState& session = live.State().Conversations.at(person);
    Check(session.Open && session.NodeId == PcNode(c, c.PaidReplyShort) && session.Receipts.size() == 1, "the session is live on the reply with one receipt");
    const auto clockAfter = live.State().Clock.Now();
    Check(!live.Submit({CommandSource::Player, ConversationChoose{person, paid, 2}}).Accepted, "a duplicate charge is refused");
    Check(live.State().Clock.Now() == clockAfter, "a refused duplicate costs nothing");

    WorldSaveData data = SaveOf(live, MakeCreation("parent_household"));
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(SerializeWorldSave(data), loaded);
    Check(result.Succeeded() && result.SourceVersion == WorldSaveSchemaVersion && result.HashVerified, "current-schema mid-session round trip is certified");
    CheckSame(data, loaded, "all persisted fields survive, including the session");
    const ConversationState& restored = loaded.World.Conversations.at(person);
    Check(restored.Open && WasChoiceChargedThisSession(restored, paid) && HasConversationFact(restored, c.Fact), "the receipt still guards the charge and the agreement survived");
    WorldSim resumed(loaded.World);
    Check(!resumed.Submit({CommandSource::Player, ConversationChoose{person, paid, 2}}).Accepted, "after reload the same choice is still refused (no second charge)");
    for (WorldSim* sim : {&live, &resumed})
    {
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave the resumed session");
        Check(sim->Submit({CommandSource::Player, AdvanceTime{25}}).Accepted, "advance the continued world");
        Check(sim->Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open the next session");
        Check(sim->Submit({CommandSource::Player, ConversationChoose{person, paid, 3}}).Accepted, "the choice is charged again in the next session");
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave again");
    }
    Check(HashWorldState(live.State()) == HashWorldState(resumed.State()), "reload continuation equals the uninterrupted run");
    const auto replay = ReplayCommandLog(seed, live.Log());
    Check(replay.Complete && replay.Hash == HashWorldState(live.State()), "command replay is equivalent");
}

void PcSaveClosedSession(const PcCase& c, std::uint64_t seed, std::string_view otherPerson)
{
    const ConversationCatalogue cat = c.Catalogue();
    WorldSaveData data = RichSave(seed, 0);
    const std::string person(c.Person);
    auto& state = data.World.Conversations[person];
    Check(OpenConversation(cat, state, c.Person, PcHour(17), PersonAvailability::Awake) == ConversationError::None, "open the afternoon session");
    Check(ChooseConversationChoice(cat, state, PcChoice(c, c.PromiseShort)).Ok(), "agree");
    Check(LeaveConversation(state) == ConversationError::None, "close the session");
    const std::string text = SerializeWorldSave(data);
    Check(!text.empty(), "a world with the closed session serialises");
    WorldSaveData loaded;
    Check(DeserializeWorldSave(text, loaded).HashVerified, "the closed session round trips certified");
    CheckSame(data, loaded, "the session history and agreement survive");
    Check(HasConversationFact(loaded.World.Conversations.at(person), c.Fact), "the agreement is still remembered after loading");
    Check(loaded.World.Conversations.find(std::string(otherPerson)) == loaded.World.Conversations.end(), "the other proprietor holds no agreement of this one");
    const std::string key = "\"" + person + "\":";
    Check(ErrorOf(ReplaceOnce(text, key, "\"" + std::string(otherPerson) + "\":")) == WorldSaveError::InvalidWorld, "a session cannot be filed under the other proprietor");
    Check(ErrorOf(ReplaceOnce(text, key, "\"person.nobody\":")) == WorldSaveError::InvalidWorld, "an unknown person is refused");
    WorldSaveData bad = data;
    bad.World.Conversations.at(person).Facts = {PcId(c, "fact", "unknown")};
    Check(SerializeWorldSave(bad).empty(), "the writer refuses a fact the catalogue does not define");
    for (std::size_t cut = 0; cut < text.size(); cut += 97)
        Check(ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "truncated saves are refused atomically");
}

void PcBothInOneWorld()
{
    WorldSim live(131);
    Check(live.Submit({CommandSource::Player, AdvanceTime{640}}).Accepted, "advance to 16:40");
    for (const std::string_view person : {std::string_view("person.debashis_pal"), std::string_view("person.mahua_saha")})
    {
        Check(live.Submit({CommandSource::Player, ConversationOpen{std::string(person), PersonAvailability::Awake}}).Accepted, "open");
        const std::string promise = person == "person.debashis_pal" ? "choice.debashis_pal.promise_bring" : "choice.mahua_saha.promise_sort";
        Check(live.Submit({CommandSource::Player, ConversationChoose{std::string(person), promise, 1}}).Accepted, "agree");
        Check(live.Submit({CommandSource::Player, ConversationLeave{std::string(person)}}).Accepted, "leave");
    }
    Check(live.Submit({CommandSource::Player, ConversationOpen{"person.debashis_pal", PersonAvailability::Awake}}).Accepted, "reopen the first");
    Check(!live.Submit({CommandSource::Player, ConversationOpen{"person.mahua_saha", PersonAvailability::Awake}}).Accepted, "only one conversation is open at a time");
    Check(live.Submit({CommandSource::Player, ConversationLeave{"person.debashis_pal"}}).Accepted, "leave the first");
    WorldSaveData data = SaveOf(live, MakeCreation("parent_household"));
    WorldSaveData loaded;
    Check(DeserializeWorldSave(SerializeWorldSave(data), loaded).HashVerified, "both agreements round trip certified");
    CheckSame(data, loaded, "both agreements survive together");
    Check(HasConversationFact(loaded.World.Conversations.at("person.debashis_pal"), "fact.debashis_pal.bring_promised") &&
              HasConversationFact(loaded.World.Conversations.at("person.mahua_saha"), "fact.mahua_saha.sort_promised"),
          "each person remembers only their own agreement");
}
} // namespace

int main()
{
    const PcCase debashis{"debashis_pal", "person.debashis_pal", "fact.debashis_pal.bring_promised", "hold_lamp", "promise_bring", "decline_bring",
                          "reply.lamp_held", 480, 840,
                          {{0, "late", ""}, {449, "late", ""}, {450, "setting_up", ""}, {479, "setting_up", ""}, {480, "counter_morning", "counter_morning_remembered"},
                           {719, "counter_morning", "counter_morning_remembered"}, {720, "closed_midday", ""}, {839, "closed_midday", ""},
                           {840, "counter_afternoon", "counter_afternoon_remembered"}, {1109, "counter_afternoon", "counter_afternoon_remembered"},
                           {1110, "closing", ""}, {1169, "closing", ""}, {1170, "late", ""}, {1439, "late", ""}},
                          &DebashisConversationCatalogue};
    const PcCase mahua{"mahua_saha", "person.mahua_saha", "fact.mahua_saha.sort_promised", "sort_screws", "promise_sort", "decline_sort",
                       "reply.sorted", 540, 900,
                       {{0, "late", ""}, {499, "late", ""}, {500, "preparing", ""}, {539, "preparing", ""}, {540, "counter_morning", "counter_morning_remembered"},
                        {779, "counter_morning", "counter_morning_remembered"}, {780, "closed_midday", ""}, {899, "closed_midday", ""},
                        {900, "counter_afternoon", "counter_afternoon_remembered"}, {1199, "counter_afternoon", "counter_afternoon_remembered"},
                        {1200, "closing", ""}, {1229, "closing", ""}, {1230, "late", ""}, {1439, "late", ""}},
                       &MahuaConversationCatalogue};
    PcShape(debashis);
    PcShape(mahua);
    PcDistinct(debashis, mahua);
    PcDistinct(mahua, debashis);
    PcAgreement(debashis);
    PcAgreement(mahua);
    PcText(debashis);
    PcText(mahua);
    PcSaveMidSession(debashis, 141);
    PcSaveMidSession(mahua, 142);
    PcSaveClosedSession(debashis, 143, "person.mahua_saha");
    PcSaveClosedSession(mahua, 144, "person.debashis_pal");
    PcBothInOneWorld();
    std::cout << "PerfectWorldProprietorConversationTests passed (" << PcChecks << " checks)\n";
    return 0;
}
