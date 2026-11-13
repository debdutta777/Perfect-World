#include "Conversation.h"

// Subrata Dey, the lane's cooperative caretaker (LANE_LIFE_PLAN section B cast table and sample lines). His week follows the
// authored schedule (npc_schedule.subrata_dey.json): he sleeps from 21:30, walks the lane and the tall flats' shared entry and stair
// in the morning, works at the noticeboard, is at home through the middle of the day and again from 17:30 to 20:30 (his flat's
// visiting hours), and does a short evening round. Plain ASCII, no markup. A choice's cost is shown beside its text by the
// presenter, so the text never says it.
//
// A node is chosen by minute of day and availability only, never by weekday, so every line is true on any day he can be spoken
// to. He can be spoken to only while he stands somewhere awake (his flat, or the noticeboard); a round, a sweep or a walk is Busy
// and declines. The lines therefore claim nothing about where he is beyond what is true wherever he may be in that window.
//
// What is promised and what is not.
//   - Time is the only price. Telling him about a loose step is ten minutes, charged once per session by receipt, and may recur in
//     a later session. Agreeing to tell him, declining, and leaving are free. There is no money, no score and no vote.
//   - The one remembered fact is that the player AGREED TO TELL HIM ABOUT A LOOSE STEP (a choice made here, free). The plan's
//     second sample line is used as written and is the plain greeting of the noticeboard hours. After the agreement he asks, in his
//     own words, whether the player has seen one. He never says a step was found, mended, swept or repaired: nothing in the world
//     records that, so no line claims it. The agreement is what is recorded; what he says after a report is only an intention
//     (he will look at the stair on his next round), never an outcome, and he does not claim to remember a report that no fact holds.
//   - He never speaks for the residents of the flats and never asks the player to do his work.
//
// Day shape (minutes since midnight), Awake:
//   21:30-05:14  late, about to sleep (Awake only if the schedule says so)
//   05:15-07:59  starting the day: the rounds come first
//   08:00-09:49  the notices and the repair list: the plan's repair-list line, with the agreement
//   09:50-13:59  at home for the midday meal and rest
//   14:00-17:29  the afternoon notices and the repair list: the same line, with the agreement
//   17:30-21:29  at home for the evening
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view SdPerson = "person.subrata_dey";
/** Set only by the player agreeing, in conversation, to tell him about a loose step. */
constexpr std::string_view SdFactStepPromised = "fact.subrata_dey.step_promised";

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef SdPromiseStep{"choice.subrata_dey.promise_step", "Say you will tell him about any loose step", 0, false,
                                              SdFactStepPromised, "", "node.subrata_dey.reply.promised", "", ""};
constexpr ConversationChoiceDef SdDeclinePromise{"choice.subrata_dey.decline_promise", "Not just now, thank you.", 0, false, "", "",
                                                 "node.subrata_dey.reply.declined", "", ""};
constexpr ConversationChoiceDef SdReportStep{"choice.subrata_dey.report_step", "Show him the step you mean", 10, false, "", "",
                                             "node.subrata_dey.reply.reported", "", ""};
constexpr ConversationChoiceDef SdNotKeepYou{"choice.subrata_dey.not_keep_you", "I won't keep you.", 0, false, "", "",
                                             "node.subrata_dey.reply.not_kept", "", ""};

constexpr std::array<ConversationNodeDef, 14> SdNodes{{
    // --- late (21:30-05:14), if he is awake at all ---
    {"node.subrata_dey.late", SdPerson, "It's late and I should be asleep soon. Let's talk in the morning.", PersonAvailability::Awake, 1290, 315,
     {}, {}, true, false, {}, 0},
    // --- starting the day (05:15-07:59) ---
    {"node.subrata_dey.early", SdPerson, "Good morning. The lane and the stairs come first. Find me after, if it can wait.", PersonAvailability::Awake,
     315, 480, {}, {}, true, false, {}, 0},
    // --- the notices and the repair list, morning (08:00-09:49): the plan's repair-list line, as written ---
    {"node.subrata_dey.board_morning", SdPerson, "The repair list is up. Add the location if I missed something.", PersonAvailability::Awake, 480, 590,
     {}, {SdFactStepPromised, ""}, true, false, {SdPromiseStep, SdDeclinePromise}, 2},
    {"node.subrata_dey.board_morning_remembered", SdPerson,
     "You said you would tell me about any loose step. Have you seen one? The repair list is up.", PersonAvailability::Awake, 480, 590,
     {SdFactStepPromised, ""}, {}, true, false, {SdReportStep, SdNotKeepYou}, 2},
    // --- at home for the midday meal and rest (09:50-13:59) ---
    {"node.subrata_dey.midday_home", SdPerson, "Come in. I'm having my midday meal, but I can talk for a little.", PersonAvailability::Awake, 590, 840,
     {}, {}, true, false, {}, 0},
    // --- the notices and the repair list, afternoon (14:00-17:29) ---
    {"node.subrata_dey.board_afternoon", SdPerson, "The repair list is up. Add the location if I missed something.", PersonAvailability::Awake, 840, 1050,
     {}, {SdFactStepPromised, ""}, true, false, {SdPromiseStep, SdDeclinePromise}, 2},
    {"node.subrata_dey.board_afternoon_remembered", SdPerson,
     "You said you would tell me about any loose step. Have you seen one? The repair list is up.", PersonAvailability::Awake, 840, 1050,
     {SdFactStepPromised, ""}, {}, true, false, {SdReportStep, SdNotKeepYou}, 2},
    // --- at home for the evening (17:30-21:29) ---
    {"node.subrata_dey.evening_home", SdPerson, "I'm home for the evening. Come in and sit down. If it's about the stairs, the list is on the board.",
     PersonAvailability::Awake, 1050, 1290, {}, {}, true, false, {}, 0},
    // --- polite declines ---
    {"node.subrata_dey.busy", SdPerson, "I'm in the middle of a job. Could you give me a few minutes?", PersonAvailability::Busy, 0, 1440,
     {}, {}, true, true, {}, 0},
    {"node.subrata_dey.asleep", SdPerson, "Not now, please. I'm asleep. Come back in the morning.", PersonAvailability::Asleep, 0, 1440,
     {}, {}, true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.subrata_dey.reply.promised", SdPerson, "Thank you. Tell me which stair and which step, and I will look at it on my round.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.subrata_dey.reply.declined", SdPerson, "That's all right. The list stays on the board for anyone.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {SdPromiseStep}, 1},
    {"node.subrata_dey.reply.reported", SdPerson, "Thank you for showing me. I will look at that stair on my next round.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.subrata_dey.reply.not_kept", SdPerson, "Thank you. Mind the stairs on your way.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
}};

constexpr std::array<ConversationFactDef, 1> SdFacts{{
    {SdFactStepPromised, "", ""},
}};
} // namespace

ConversationCatalogue SubrataConversationCatalogue() noexcept
{
    return ConversationCatalogue{SdNodes, SdFacts};
}
} // namespace PerfectWorld
