#include "Conversation.h"

// Anindita Sen, proprietor of the stationery shop and Mira's parent (LANE_LIFE_PLAN section B cast table and sample lines).
// Her day follows the authored schedule (npc_schedule.anindita_sen.json): stock check, counter, a midday closure at home,
// counter again, reconciliation, home. Plain ASCII, no markup. A choice's cost is shown beside its text by the presenter, so
// the text itself never says it.
//
// What is promised and what is not.
//   - There is no economy yet, so there is no price, no "buy" and no payment. Her offers are small jobs in the shop. Helping
//     label the shelves, or counting stock with her, is the declared cost: ten minutes, paid once per session (a receipt), and
//     it may recur in later sessions. Declining costs nothing.
//   - Her lines claim only what the catalogue itself records. The one remembered fact is that the player HELPED with the
//     labels (a choice made here). She thanks the player for it; she never says how many items were counted, that a count
//     was saved, or that anything was achieved. The plan's third sample line ("Your labels saved me a second count. I've
//     kept them.") is therefore reworded to the fact. Her first two sample lines are used as written.
//   - She never asks Mira's help and never treats the player as staff: the plan's concern is a shop made manageable without
//     automatically relying on Mira.
//
// Day shape (minutes since midnight), Awake:
//   07:30-07:59  stock check before opening
//   08:00-12:59  at the counter
//   13:00-14:59  closed for the midday meal
//   15:00-17:29  at the counter
//   17:30-19:29  at the counter, checking stock before closing
//   19:30-20:59  closed, doing the day's books, going home
//   21:00-07:29  late (she is home; Awake only if the schedule says so)
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view AnPerson = "person.anindita_sen";
/** Set only by the player helping with the labels, in conversation. */
constexpr std::string_view AnFactLabelsHelped = "fact.anindita_sen.labels_helped";

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef AnHelpLabel{"choice.anindita_sen.help_label", "Help label the shelves for a while", 10, false, AnFactLabelsHelped, "",
                                            "node.anindita_sen.reply.label_helped", "", ""};
constexpr ConversationChoiceDef AnDeclineLabel{"choice.anindita_sen.decline_label", "Not just now, thank you.", 0, false, "", "",
                                               "node.anindita_sen.reply.label_declined", "", ""};
constexpr ConversationChoiceDef AnCountStock{"choice.anindita_sen.count_stock", "Help count the stock for a while", 10, false, "", "",
                                             "node.anindita_sen.reply.stock_counted", "", ""};
constexpr ConversationChoiceDef AnComeBack{"choice.anindita_sen.come_back", "I'll come back then.", 0, false, "", "",
                                           "node.anindita_sen.reply.come_back", "", ""};
constexpr ConversationChoiceDef AnNotKeepYou{"choice.anindita_sen.not_keep_you", "I won't keep you.", 0, false, "", "",
                                             "node.anindita_sen.reply.not_kept", "", ""};

constexpr std::array<ConversationNodeDef, 16> AnNodes{{
    // --- late (21:00-07:29), if she is awake at all ---
    {"node.anindita_sen.late", AnPerson, "The shop is closed for the night. Please come back in the morning; we open at eight.",
     PersonAvailability::Awake, 1260, 450, {}, {}, true, false, {}, 0},
    // --- stock check before opening (07:30-07:59) ---
    {"node.anindita_sen.preparing", AnPerson, "I'm counting the stock before we open. Please give me until eight.", PersonAvailability::Awake, 450, 480,
     {}, {}, true, false, {}, 0},
    // --- counter, morning (08:00-12:59): the plan's earlier-encounter line ---
    {"node.anindita_sen.counter_morning", AnPerson, "The plain exercise books are on the lower shelf.", PersonAvailability::Awake, 480, 780,
     {}, {AnFactLabelsHelped, ""}, true, false, {AnHelpLabel, AnDeclineLabel}, 2},
    {"node.anindita_sen.counter_morning_remembered", AnPerson,
     "You helped with the labels before. Thank you. The plain exercise books are on the lower shelf.", PersonAvailability::Awake, 480, 780,
     {AnFactLabelsHelped, ""}, {}, true, false, {AnHelpLabel, AnDeclineLabel}, 2},
    // --- closed for the midday meal (13:00-14:59) ---
    {"node.anindita_sen.closed_midday", AnPerson, "The shop is closed for the midday meal. We open again at three.", PersonAvailability::Awake, 780, 900,
     {}, {}, true, false, {AnComeBack}, 1},
    // --- counter, afternoon (15:00-17:29) ---
    {"node.anindita_sen.counter_afternoon", AnPerson, "The plain exercise books are on the lower shelf.", PersonAvailability::Awake, 900, 1050,
     {}, {AnFactLabelsHelped, ""}, true, false, {AnHelpLabel, AnDeclineLabel}, 2},
    {"node.anindita_sen.counter_afternoon_remembered", AnPerson,
     "You helped with the labels before. Thank you. The plain exercise books are on the lower shelf.", PersonAvailability::Awake, 900, 1050,
     {AnFactLabelsHelped, ""}, {}, true, false, {AnHelpLabel, AnDeclineLabel}, 2},
    // --- counter, evening (17:30-19:29): the plan's time-dependent line ---
    {"node.anindita_sen.counter_evening", AnPerson, "I'm checking the stock before I close.", PersonAvailability::Awake, 1050, 1170,
     {}, {}, true, false, {AnCountStock, AnNotKeepYou}, 2},
    // --- closed, books, going home (19:30-20:59) ---
    {"node.anindita_sen.closing", AnPerson, "We're closed for today. I'm doing the day's books before I go home. Please come back tomorrow.",
     PersonAvailability::Awake, 1170, 1260, {}, {}, true, false, {AnNotKeepYou}, 1},
    // --- polite declines ---
    {"node.anindita_sen.busy", AnPerson, "I'm in the middle of something. Could you give me a few minutes?", PersonAvailability::Busy, 0, 1440,
     {}, {}, true, true, {}, 0},
    {"node.anindita_sen.asleep", AnPerson, "Not now, please. Come when the shop is open.", PersonAvailability::Asleep, 0, 1440,
     {}, {}, true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.anindita_sen.reply.label_helped", AnPerson, "Thank you. Start with the lower shelf, and I'll show you where the labels go.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.anindita_sen.reply.label_declined", AnPerson, "That's all right. The offer stands, if you change your mind.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {AnHelpLabel}, 1},
    {"node.anindita_sen.reply.stock_counted", AnPerson, "Thank you. We'll do the pens first, then the pencils. There's no hurry.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.anindita_sen.reply.come_back", AnPerson, "Good. Come when you like.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.anindita_sen.reply.not_kept", AnPerson, "Thank you. Mind the road.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
}};

constexpr std::array<ConversationFactDef, 1> AnFacts{{
    {AnFactLabelsHelped, "", ""},
}};
} // namespace

ConversationCatalogue AninditaConversationCatalogue() noexcept
{
    return ConversationCatalogue{AnNodes, AnFacts};
}
} // namespace PerfectWorld

