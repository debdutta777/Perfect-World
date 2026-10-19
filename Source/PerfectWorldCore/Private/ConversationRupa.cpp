#include "Conversation.h"

// Rupa Ghosh, who keeps the tea row house and works away from the lane (NR-01; LANE_LIFE_PLAN section B conversation rules). Her week
// follows the authored schedule (npc_schedule.rupa_ghosh.json): she sleeps from 22:00, cooks and eats in the early morning, works
// away from the lane on Monday to Saturday (08:00-16:00; Saturday to 13:00), and is at home, awake, through the evening hours in
// which her front door is visiting: 16:11-17:00 on Monday to Friday and 18:00-19:30 every day, with the cooking hour from 17:00 to
// 18:00 between them. Plain ASCII, no markup. A choice's cost is shown beside its text by the presenter, so the text never says it.
//
// A node is chosen by minute of day and availability only, never by weekday, so every line is true on any day she can be spoken
// to. She can be spoken to only while she is at home and not busy (washing and dressing, a meal, a rest, reading); cooking, sweeping
// and work are Busy and decline. The lines claim nothing about where she has been or what she did before the visit.
//
// What is promised and what is not.
//   - Time is the only price. Sitting and talking a while is fifteen minutes, charged once per session by receipt, and may recur in
//     a later session. Agreeing to come by, declining, and leaving are free. There is no money, no score and no vote.
//   - The one remembered fact is that the player AGREED, in conversation, TO COME BY ONE EVENING. It is a free choice made here. After
//     it she says only that the player said so and invites them to sit. She never says the player came, helped or kept anything:
//     nothing in the world records that, so no line claims it. What is recorded is the agreement.
//   - She never speaks for the other households of the lane and never asks the player to do her work.
//
// Day shape (minutes since midnight), Awake:
//   22:00-05:29  late, about to sleep (Awake only if the schedule says so)
//   05:30-07:59  the early morning: the house comes first
//   08:00-15:59  at home in the quiet of the day
//   16:00-17:59  the quiet hour before supper: the agreement is offered, or remembered
//   18:00-21:59  supper and the evening: the same agreement, offered or remembered
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view RgPerson = "person.rupa_ghosh";
/** Set only by the player agreeing, in conversation, to come by one evening. */
constexpr std::string_view RgFactVisitPromised = "fact.rupa_ghosh.visit_promised";

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef RgPromiseVisit{"choice.rupa_ghosh.promise_visit", "Say you will come by one evening", 0, false,
                                               RgFactVisitPromised, "", "node.rupa_ghosh.reply.promised", "", ""};
constexpr ConversationChoiceDef RgDeclineVisit{"choice.rupa_ghosh.decline_visit", "Not just now, thank you.", 0, false, "", "",
                                               "node.rupa_ghosh.reply.declined", "", ""};
constexpr ConversationChoiceDef RgSitAWhile{"choice.rupa_ghosh.sit_a_while", "Sit and talk a while", 15, false, "", "",
                                            "node.rupa_ghosh.reply.sat", "", ""};
constexpr ConversationChoiceDef RgNotKeepYou{"choice.rupa_ghosh.not_keep_you", "I won't keep you.", 0, false, "", "",
                                             "node.rupa_ghosh.reply.not_kept", "", ""};

constexpr std::array<ConversationNodeDef, 13> RgNodes{{
    // --- late (22:00-05:29), if she is awake at all ---
    {"node.rupa_ghosh.late", RgPerson, "It's late, and I should be asleep soon. Come another evening, when the house is awake.",
     PersonAvailability::Awake, 1320, 330, {}, {}, true, false, {}, 0},
    // --- the early morning (05:30-07:59) ---
    {"node.rupa_ghosh.morning", RgPerson, "Good morning. The house comes first at this hour. Come back when the day is under way.",
     PersonAvailability::Awake, 330, 480, {}, {}, true, false, {}, 0},
    // --- at home in the quiet of the day (08:00-15:59) ---
    {"node.rupa_ghosh.day", RgPerson, "Come in. The house is quiet at this hour. Sit down, if you like.", PersonAvailability::Awake, 480, 960,
     {}, {}, true, false, {}, 0},
    // --- the quiet hour before supper (16:00-17:59) ---
    {"node.rupa_ghosh.afternoon", RgPerson, "Come in, come in. This is the quiet hour before supper. Would you come by one evening and sit with us?",
     PersonAvailability::Awake, 960, 1080, {}, {RgFactVisitPromised, ""}, true, false, {RgPromiseVisit, RgDeclineVisit}, 2},
    {"node.rupa_ghosh.afternoon_remembered", RgPerson, "You said you would come by one evening. Sit down for a while, if you can.",
     PersonAvailability::Awake, 960, 1080, {RgFactVisitPromised, ""}, {}, true, false, {RgSitAWhile, RgNotKeepYou}, 2},
    // --- supper and the evening (18:00-21:59) ---
    {"node.rupa_ghosh.supper", RgPerson, "We are about to sit down to eat. Come in. Would you come by again one evening?",
     PersonAvailability::Awake, 1080, 1320, {}, {RgFactVisitPromised, ""}, true, false, {RgPromiseVisit, RgDeclineVisit}, 2},
    {"node.rupa_ghosh.supper_remembered", RgPerson, "You said you would come by one evening. Come in and sit. We are about to eat.",
     PersonAvailability::Awake, 1080, 1320, {RgFactVisitPromised, ""}, {}, true, false, {RgSitAWhile, RgNotKeepYou}, 2},
    // --- polite declines ---
    {"node.rupa_ghosh.busy", RgPerson, "I'm in the middle of something. Could you give me a few minutes?", PersonAvailability::Busy, 0, 1440,
     {}, {}, true, true, {}, 0},
    {"node.rupa_ghosh.asleep", RgPerson, "Not now, please. I'm asleep. Come back in the evening.", PersonAvailability::Asleep, 0, 1440,
     {}, {}, true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.rupa_ghosh.reply.promised", RgPerson, "Thank you. Come when the day's work is put away. We eat early.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.rupa_ghosh.reply.declined", RgPerson, "That's all right. You are welcome here whenever the door is answered.", PersonAvailability::Awake, 0,
     1440, {}, {}, false, false, {RgPromiseVisit}, 1},
    {"node.rupa_ghosh.reply.sat", RgPerson, "It is good to have someone to talk to in the house. Thank you for sitting a while.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.rupa_ghosh.reply.not_kept", RgPerson, "Of course. Thank you for stopping by.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false,
     {}, 0},
}};

constexpr std::array<ConversationFactDef, 1> RgFacts{{
    {RgFactVisitPromised, "", ""},
}};
} // namespace

ConversationCatalogue RupaConversationCatalogue() noexcept
{
    return ConversationCatalogue{RgNodes, RgFacts};
}
} // namespace PerfectWorld
