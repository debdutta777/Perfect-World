// World save v8 with a Mira Sen conversation: a world saved in the middle of her session (after the paid comparison, on its reply
// node, with the notes fact already remembered) round-trips field by field and by hash, resumes identically to the uninterrupted
// run, replays from the command log, never charges the comparison twice, and a save carrying her closed session and facts also
// round-trips and refuses tampering. Standalone: it reuses the shared world-save builders (WorldSaveTestSupport.h) and the
// Anindita save tests' pattern (WorldSaveAninditaTests.cpp).
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
int MrSaveChecks = 0;
}

void Check(bool condition, const char* message)
{
    ++MrSaveChecks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

#include "WorldSaveTestSupport.h"

namespace
{
constexpr std::string_view MrSavePerson = "person.mira_sen";
constexpr std::string_view MrSaveReturn = "choice.mira_sen.return_notes";
constexpr std::string_view MrSaveCompare = "choice.mira_sen.compare_question";
constexpr std::string_view MrSaveFact = "fact.mira_sen.notes_returned";

void MrSaveMidSession()
{
    const auto creation = MakeCreation("parent_household"); WorldSim live(93, CreatorOf(creation));
    const std::string person(MrSavePerson);
    // Monday 06:00 plus 640 minutes is 16:40; Mira stands at the packing desk on a Tuesday or Thursday, and the core serves the
    // desk line from 16:30 whatever the day.
    Check(live.Submit({CommandSource::Player, AdvanceTime{640}}).Accepted, "advance to the desk hours (16:40)");
    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open Mira's first session");
    const auto beforeNotes = live.State().Clock.Now();
    Check(live.Submit({CommandSource::Player, ConversationChoose{person, std::string(MrSaveReturn), 1}}).Accepted, "giving the notes back is accepted");
    Check(live.State().Clock.Now() == beforeNotes, "giving the notes back costs no time");
    Check(HasConversationFact(live.State().Conversations.at(person), MrSaveFact), "the notes fact is set");
    Check(live.Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "close the first session");
    Check(live.Submit({CommandSource::Player, AdvanceTime{10}}).Accepted, "ten minutes pass");

    Check(live.Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open her second session through the reducer");
    const auto before = live.State().Clock.Now();
    const auto chosen = live.Submit({CommandSource::Player, ConversationChoose{person, std::string(MrSaveCompare), 2}});
    Check(chosen.Accepted, "the comparison is accepted through the reducer");
    Check(live.State().Clock.Now() == before + 10, "ten minutes were charged through the world clock, once");
    const ConversationState& session = live.State().Conversations.at(person);
    Check(session.Open && session.NodeId == "node.mira_sen.reply.compared" && session.Receipts.size() == 1, "the session is live on the reply with one receipt");
    const auto clockAfter = live.State().Clock.Now();
    Check(!live.Submit({CommandSource::Player, ConversationChoose{person, std::string(MrSaveCompare), 2}}).Accepted, "a duplicate charge is refused");
    Check(live.State().Clock.Now() == clockAfter, "a refused duplicate costs nothing");

    WorldSaveData data = SaveOf(live, creation);
    WorldSaveData loaded;
    const auto result = DeserializeWorldSave(SerializeWorldSave(data), loaded);
    Check(result.Succeeded() && result.SourceVersion == WorldSaveSchemaVersion && result.HashVerified, "current-schema mid-session round trip with Mira is certified");
    CheckSame(data, loaded, "all persisted fields survive, including her session");
    Check(loaded.World.Conversations.at(person).Open && WasChoiceChargedThisSession(loaded.World.Conversations.at(person), MrSaveCompare) &&
              HasConversationFact(loaded.World.Conversations.at(person), MrSaveFact),
          "the persisted receipt still guards the charge and the remembered fact survived");

    WorldSim resumed(loaded.World);
    Check(!resumed.Submit({CommandSource::Player, ConversationChoose{person, std::string(MrSaveCompare), 2}}).Accepted,
          "after reload the same choice is still refused (no second charge)");
    for (WorldSim* sim : {&live, &resumed})
    {
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave the resumed session");
        Check(sim->Submit({CommandSource::Player, AdvanceTime{25}}).Accepted, "advance the continued world");
        Check(sim->Submit({CommandSource::Player, ConversationOpen{person, PersonAvailability::Awake}}).Accepted, "open her next session");
        Check(sim->Submit({CommandSource::Player, ConversationChoose{person, std::string(MrSaveCompare), 3}}).Accepted,
              "the comparison is charged again in the next session");
        Check(sim->Submit({CommandSource::Player, ConversationLeave{person}}).Accepted, "leave again");
    }
    Check(HashWorldState(live.State()) == HashWorldState(resumed.State()), "reload continuation equals the uninterrupted run");
    const auto replay = ReplayLife(93, creation, live.Log());
    Check(replay.Complete && replay.Hash == HashWorldState(live.State()), "command replay is equivalent");
}

void MrSaveClosedSessionAndRefusals()
{
    WorldSaveData data = RichSave(94, 0);
    const auto catalogue = MiraConversationCatalogue();
    const std::string person(MrSavePerson);
    auto& state = data.World.Conversations[person];
    Check(OpenConversation(catalogue, state, MrSavePerson, 8 * 60, PersonAvailability::Awake) == ConversationError::None, "open the morning session");
    Check(ChooseConversationChoice(catalogue, state, "choice.mira_sen.walk_to_corner").Ok(), "walk to the corner");
    Check(LeaveConversation(state) == ConversationError::None, "close the session");
    const std::string text = SerializeWorldSave(data);
    Check(!text.empty(), "a world with her closed session serialises");
    WorldSaveData loaded;
    Check(DeserializeWorldSave(text, loaded).HashVerified, "her closed session round trips certified");
    CheckSame(data, loaded, "her session history survives");

    // The envelope is refused when it is stored under another person, or is cut short inside valid JSON.
    const std::string envelope = SerialiseConversationState(catalogue, state);
    Check(ErrorOf(ReplaceOnce(text, "\"person.mira_sen\":", "\"person.anindita_sen\":")) == WorldSaveError::InvalidWorld,
          "her session cannot be filed under her mother");
    Check(ErrorOf(ReplaceOnce(text, "\"person.mira_sen\":", "\"person.nobody\":")) == WorldSaveError::InvalidWorld, "an unknown person is refused");
    WorldSaveData bad = data;
    bad.World.Conversations.at(person).Facts = {"fact.mira_sen.unknown"};
    Check(SerializeWorldSave(bad).empty(), "the writer refuses a fact her catalogue does not define");
    bad = data;
    bad.World.Conversations.at(person).Facts = {std::string(MrSaveFact)};
    const std::string withFact = SerialiseConversationState(catalogue, bad.World.Conversations.at(person));
    Check(!withFact.empty() && withFact != envelope, "a valid fact mutation yields a different envelope");
    for (std::size_t cut = 0; cut < text.size(); cut += 97)
        Check(ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "truncated saves are refused atomically");
}
} // namespace

int main()
{
    MrSaveMidSession();
    MrSaveClosedSessionAndRefusals();
    std::cout << "PerfectWorldWorldSaveMiraTests passed (" << MrSaveChecks << " checks)\n";
    return 0;
}
