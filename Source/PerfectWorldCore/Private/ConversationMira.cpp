#include "Conversation.h"

// Mira Sen, Class XI Commerce student and Anindita's daughter (LANE_LIFE_PLAN section B cast table and sample lines).
// Her week follows the authored schedule (npc_schedule.mira_sen.json): morning study at home, school 10:00-15:50, on Tuesday
// and Thursday 16:30-18:00 help at the stationery packing desk, other afternoons her own plans, evening study, asleep from
// 22:30. Plain ASCII, no markup. A choice's cost is shown beside its text by the presenter, so the text never says it.
//
// A node is chosen by minute of day and availability only, never by weekday, so every line is true on any day she can be
// spoken to. She can be spoken to only while she stands somewhere (a walk is Busy and declines), and the only place the lane
// can show her standing is the packing desk, 16:30-18:00 on a Tuesday or Thursday. The lines for the other windows are
// therefore short and make no claim about where she is beyond "not here now"; the desk line is the one that is acted on.
//
// What is promised and what is not.
//   - Time is the only price. Walking with her to the corner and comparing the last question are ten minutes each, charged once
//     per session by receipt, and may recur in a later session. There is no money, no score and no grade.
//   - The one remembered fact is that the player GAVE BACK HER NOTES (a choice made here, free). The plan's third sample line
//     is used as written and plays only after it. She never says how the comparison went or what anyone learned.
//   - She never asks the player to take her place at the shop, and never speaks for her mother.
//
// Day shape (minutes since midnight), Awake:
//   06:15-09:29  at home, getting ready for school: the walk-to-the-corner greeting
//   09:30-16:29  at school, on her way, or on her own plans: she cannot stop
//   16:30-17:59  at the packing desk: the counter line, and the remembered line after the notes
//   18:00-21:29  at home for the evening
//   21:30-06:14  late, about to sleep (Awake only if the schedule says so)
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view MiPerson = "person.mira_sen";
/** Set only by the player giving back her notes, in conversation. */
constexpr std::string_view MiFactNotesReturned = "fact.mira_sen.notes_returned";

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef MiWalkToCorner{"choice.mira_sen.walk_to_corner", "Walk with her as far as the corner", 10, false, "", "",
                                               "node.mira_sen.reply.walked", "", ""};
constexpr ConversationChoiceDef MiDeclineWalk{"choice.mira_sen.decline_walk", "Not this morning, thank you.", 0, false, "", "",
                                              "node.mira_sen.reply.walk_declined", "", ""};
constexpr ConversationChoiceDef MiReturnNotes{"choice.mira_sen.return_notes", "Give back the notes she lent you", 0, false, MiFactNotesReturned, "",
                                              "node.mira_sen.reply.notes_returned", "", ""};
constexpr ConversationChoiceDef MiCompareQuestion{"choice.mira_sen.compare_question", "Compare the last question", 10, false, "", "",
                                                  "node.mira_sen.reply.compared", "", ""};

constexpr std::array<ConversationNodeDef, 12> MiNodes{{
    // --- late (21:30-06:14), if she is awake at all ---
    {"node.mira_sen.late", MiPerson, "It's late and I should be asleep soon. Let's talk tomorrow.", PersonAvailability::Awake, 1290, 375,
     {}, {}, true, false, {}, 0},
    // --- at home in the morning (06:15-09:29): the plan's school-day greeting, as written ---
    {"node.mira_sen.morning", MiPerson, "I've got time to walk with you as far as the corner.", PersonAvailability::Awake, 375, 570,
     {}, {}, true, false, {MiWalkToCorner, MiDeclineWalk}, 2},
    // --- school, her own plans, or on her way (09:30-16:29) ---
    {"node.mira_sen.away", MiPerson, "I can't stop just now. Come and find me at the shop on a Tuesday or Thursday afternoon.",
     PersonAvailability::Awake, 570, 990, {}, {}, true, false, {}, 0},
    // --- at the packing desk (16:30-17:59): the plan's at-the-counter line, as written ---
    {"node.mira_sen.counter", MiPerson, "I'm at the counter today. We can talk while I finish this list.", PersonAvailability::Awake, 990, 1080,
     {}, {MiFactNotesReturned, ""}, true, false, {MiReturnNotes}, 1},
    // --- the plan's remembered line, as written, only after the notes were given back ---
    {"node.mira_sen.counter_remembered", MiPerson, "You brought the notes back when you said you would. Want to compare the last question?",
     PersonAvailability::Awake, 990, 1080, {MiFactNotesReturned, ""}, {}, true, false, {MiCompareQuestion}, 1},
    // --- at home for the evening (18:00-21:29) ---
    {"node.mira_sen.evening", MiPerson, "I'm home for the evening and I have studying to finish. See you tomorrow.", PersonAvailability::Awake,
     1080, 1290, {}, {}, true, false, {}, 0},
    // --- polite declines ---
    {"node.mira_sen.busy", MiPerson, "Sorry, I'm in the middle of something. Could you give me a few minutes?", PersonAvailability::Busy, 0, 1440,
     {}, {}, true, true, {}, 0},
    {"node.mira_sen.asleep", MiPerson, "Not now, please. I'm asleep. Let's talk tomorrow.", PersonAvailability::Asleep, 0, 1440,
     {}, {}, true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.mira_sen.reply.walked", MiPerson, "Thank you for the company. Mind the road at the corner.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.mira_sen.reply.walk_declined", MiPerson, "That's all right. Another day.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.mira_sen.reply.notes_returned", MiPerson, "Thank you for bringing them back. I'll go through them this evening.", PersonAvailability::Awake,
     0, 1440, {}, {}, false, false, {}, 0},
    {"node.mira_sen.reply.compared", MiPerson, "Good. I'll read mine out first, and you can tell me where yours is different.", PersonAvailability::Awake,
     0, 1440, {}, {}, false, false, {}, 0},
}};

constexpr std::array<ConversationFactDef, 1> MiFacts{{
    {MiFactNotesReturned, "", ""},
}};
} // namespace

ConversationCatalogue MiraConversationCatalogue() noexcept
{
    return ConversationCatalogue{MiNodes, MiFacts};
}
} // namespace PerfectWorld
