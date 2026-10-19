// World save v8 with an Anindita Sen conversation (order AN): a world saved in the middle of her session (after the paid
// labels job, on its reply node) round-trips field by field and by hash, resumes identically to the uninterrupted run, replays
// from the command log, and a save carrying her closed session and facts also round-trips. Standalone: it reuses the shared
// world-save builders (WorldSaveTestSupport.h) and the lane save tests' pattern (WorldSaveLaneTests.cpp).
#include "WorldSave.h"

#include "Conversation.h"
#include "WorldHash.h"
#include "WorldReducer.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace PerfectWorld;

namespace
{
int AnSaveChecks = 0;
}

void Check(bool condition, const char* message)
{
    ++AnSaveChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

#include "WorldSaveTestSupport.h"

namespace
{
constexpr std::string_view AnSavePerson = "person.anindita_sen";
constexpr std::string_view AnSaveHelp = "choice.anindita_sen.help_label";
constexpr std::string_view AnSaveFact = "fact.anindita_sen.labels_helped";

void AnSaveMidSession()
{
    WorldSim live(91);
    const std::string person(AnSavePerson);
    Check(live.Submit({CommandSource::Player, AdvanceTime{180}}).Accepted, "advance to the counter hours (09:00)");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open Anindita's session through the reducer");
    const auto before = live.State().Clock.Now();
    const auto chosen = live.Submit({CommandSource::Player, ConversationChoose{person, std::string(AnSaveHelp), 1}});
    Check(chosen.Accepted, "the labels job is accepted through the reducer");
    Check(live.State().Clock.Now() == before + 10, "ten minutes were charged through the world clock, once");
    const ConversationState& session = live.State().Conversations.at(person);
    Check(session.Open && HasConversationFact(session, AnSaveFact) && session.Receipts.size() == 1, "the session is live with the fact and one receipt");
    // A second charge in the same session is refused by the reducer, and the clock does not move.
    const auto clockAfter = live.State().Clock.Now();
    Check(!live.Submit({CommandSource::Player, ConversationChoose{person, std::string(AnSaveHelp), 1}}).Accepted, "a duplicate charge is refused");
    Check(live.State().Clock.Now() == clockAfter, "a refused duplicate costs nothing");

    WorldSaveData data = SaveOf(live, MakeCreation("parent_household"));
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(SerializeWorldSave(data), loaded);
    Check(result.Succeeded() && result.SourceVersion == WorldSaveSchemaVersion && result.HashVerified, "current-schema mid-session round trip with Anindita is certified");
    CheckSame(data, loaded, "all persisted fields survive, including her session");
    Check(loaded.World.Conversations.at(person).Open && WasChoiceChargedThisSession(loaded.World.Conversations.at(person), AnSaveHelp),
          "the persisted receipt still guards the charge");

    WorldSim resumed(loaded.World);
    Check(!resumed.Submit({CommandSource::Player, ConversationChoose{person, std::string(AnSaveHelp), 1}}).Accepted,
          "after reload the same choice is still refused (no second charge)");
    for (WorldSim* sim : {&live, &resumed})
    {
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave the resumed session");
        Check(sim->Submit({CommandSource::Player, AdvanceTime{25}}).Accepted, "advance the continued world");
        Check(sim->Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open her next session");
        Check(sim->Submit({CommandSource::Player, ConversationChoose{person, std::string(AnSaveHelp), 2}}).Accepted,
              "the job is charged again in the next session");
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave again");
    }
    Check(HashWorldState(live.State()) == HashWorldState(resumed.State()), "reload continuation equals the uninterrupted run");
    const auto replay = ReplayCommandLog(91, live.Log());
    Check(replay.Complete && replay.Hash == HashWorldState(live.State()), "command replay is equivalent");
}

void AnSaveClosedSessionAndRefusals()
{
    WorldSaveData data = RichSave(92, 0);
    const auto catalogue = AninditaConversationCatalogue();
    const std::string person(AnSavePerson);
    auto& state = data.World.Conversations[person];
    Check(OpenConversation(catalogue, state, AnSavePerson, 1100, PersonAvailability::Awake) == ConversationError::None, "open the evening session");
    Check(ChooseConversationChoice(catalogue, state, "choice.anindita_sen.count_stock").Ok(), "count the stock");
    Check(LeaveConversation(state) == ConversationError::None, "close the session");
    const std::string text = SerializeWorldSave(data);
    Check(!text.empty(), "a world with her closed session serialises");
    WorldSaveData loaded;
    Check(DeserializeWorldSave(text, loaded).HashVerified, "her closed session round trips certified");
    CheckSame(data, loaded, "her session history survives");

    // The envelope is refused when it is stored under another person, or is cut short inside valid JSON.
    const std::string envelope = SerialiseConversationState(catalogue, state);
    Check(ErrorOf(ReplaceOnce(text, "\"person.anindita_sen\":", "\"person.farida_qadri\":")) == WorldSaveError::InvalidWorld,
          "her session cannot be filed under another person");
    Check(ErrorOf(ReplaceOnce(text, "\"person.anindita_sen\":", "\"person.nobody\":")) == WorldSaveError::InvalidWorld, "an unknown person is refused");
    WorldSaveData bad = data;
    bad.World.Conversations.at(person).Facts = {"fact.anindita_sen.unknown"};
    Check(SerializeWorldSave(bad).empty(), "the writer refuses a fact her catalogue does not define");
    bad = data;
    bad.World.Conversations.at(person).Facts = {std::string(AnSaveFact)};
    const std::string withFact = SerialiseConversationState(catalogue, bad.World.Conversations.at(person));
    Check(!withFact.empty() && withFact != envelope, "a valid fact mutation yields a different envelope");
    for (std::size_t cut = 0; cut < text.size(); cut += 97)
        Check(ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "truncated saves are refused atomically");
}
} // namespace

int main()
{
    AnSaveMidSession();
    AnSaveClosedSessionAndRefusals();
    std::cout << "PerfectWorldWorldSaveAninditaTests passed (" << AnSaveChecks << " checks)\n";
    return 0;
}
