#include "Conversation.h"

// Sahana Mitra, who keeps a home tailoring trade in tall-flats unit B and goes out on one errand a day (NR-03). Her week follows the
// authored schedule (npc_schedule.sahana_mitra.json): late to bed and late to rise, sewing at home in the daytime, an errand away
// from the lane on most afternoons, and home and Awake through every evening, when her door is visiting (17:00-20:30). Plain ASCII,
// no markup. A choice's cost is shown beside its text by the presenter, so the text never says it.
//
// What is promised and what is not.
//   - Time is the only price. Talking about her sewing a while is ten minutes, charged once per session by receipt. Agreeing to
//     talk again, declining and leaving are free. There is no score, no vote and nothing changes hands.
//   - The one remembered fact is that the player AGREED, in conversation, TO TALK ABOUT HER SEWING ANOTHER EVENING. It is a free choice
//     made here. After it she says only that the player agreed and invites them to talk now. She never says that anything was made,
//     finished, fitted or collected: nothing in the world records that, so no line claims it.
//   - She never speaks for the other households of the lane and never asks the player to do her work.
//
// A node is chosen by minute of day and availability only, never by weekday, so every line is true on any day she can be spoken to.
// She declines while Busy (cooking, sewing, her errand) and while Asleep.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view SmPerson = "person.sahana_mitra";
constexpr std::string_view SmAgreement = "fact.sahana_mitra.sewing_evening_agreed";
constexpr ConversationChoiceDef SmAgree{"choice.sahana_mitra.agree", "Agree to talk about her sewing another evening", 0, false,
    SmAgreement, "", "node.sahana_mitra.agreed", "", ""};
constexpr ConversationChoiceDef SmSewing{"choice.sahana_mitra.sewing", "Spend a little time talking about her sewing", 10, false,
    "", "", "node.sahana_mitra.sewing", "", ""};
constexpr std::array<ConversationNodeDef, 6> SmNodes{{
    {"node.sahana_mitra.greeting", SmPerson, "Hello. I sew here at home. Would you like to talk about it another evening?",
     PersonAvailability::Awake, 0, 1440, {}, {SmAgreement, ""}, true, false, {SmAgree, SmSewing}, 2},
    {"node.sahana_mitra.remembered", SmPerson, "You agreed to talk about my sewing another evening. We can talk for a little now.",
     PersonAvailability::Awake, 0, 1440, {SmAgreement, ""}, {}, true, false, {SmSewing}, 1},
    {"node.sahana_mitra.agreed", SmPerson, "Another evening, then. For now, shall we talk for a little?", PersonAvailability::Awake,
     0, 1440, {}, {}, false, false, {SmSewing}, 1},
    {"node.sahana_mitra.sewing", SmPerson, "Most of it is small work, a stitch at a time. We can leave the rest for another time.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {SmSewing}, 1},
    {"node.sahana_mitra.busy", SmPerson, "My hands are full just now. Could we talk when I am free?", PersonAvailability::Busy,
     0, 1440, {}, {}, true, true, {}, 0},
    {"node.sahana_mitra.asleep", SmPerson, "I am asleep. Please come back later.", PersonAvailability::Asleep,
     0, 1440, {}, {}, true, true, {}, 0},
}};
constexpr std::array<ConversationFactDef, 1> SmFacts{{{SmAgreement, "", ""}}};
}

ConversationCatalogue SahanaConversationCatalogue() noexcept
{
    return {SmNodes, SmFacts};
}
} // namespace PerfectWorld
