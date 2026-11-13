#include "Conversation.h"

// Farida Qadri, proprietor of Shanti Chai (INDIA_WORLD_BIBLE cast table; LANE_LIFE_PLAN section B). Her day follows the
// authored schedule: prepare, serve, wash, break, close. Plain ASCII, no markup. A choice's cost is shown beside its text by
// the presenter, so the text itself never says it.
//
// What is promised and what is not.
//   - There is no economy yet, so there is no price, no "buy" and no payment. She offers a cup; the player accepts or declines
//     socially. Sitting down with a cup is the one declared cost: ten minutes, paid once per session (a receipt), and it may
//     recur in later sessions. Declining costs nothing.
//   - Her lines claim only what the catalogue itself records. The only remembered fact is that the player ASKED before moving
//     the bench (a choice made here). She never says the bench was moved, that anything worked, or that anything was achieved;
//     she says what she likes and thanks the player for asking.
//   - The first and second sample lines of the plan are used as written; the third is reworded to the fact (see above).
//
// Day shape (minutes since midnight), Awake:
//   05:30-05:59  preparation: busy but greetable
//   06:00-11:29  serving
//   11:30-15:29  closed: supplies and rest
//   15:30-18:59  serving
//   19:00-21:59  washing the last cups, closing, going home
//   22:00-05:29  late (she is up early; Awake only if the schedule says so)
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view Person = "person.farida_qadri";
/** Set only by the player asking, in conversation, whether the bench may be moved. */
constexpr std::string_view FactBenchAsked = "fact.farida_qadri.bench_asked";

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef SitWithTea{"choice.farida_qadri.sit_with_tea", "Sit down for a cup of tea", 10, false, "", "", "node.farida_qadri.reply.tea_served", "", ""};
constexpr ConversationChoiceDef DeclineTea{"choice.farida_qadri.decline_tea", "Not today, thank you.", 0, false, "", "", "node.farida_qadri.reply.tea_declined", "", ""};
constexpr ConversationChoiceDef AskBench{"choice.farida_qadri.ask_bench", "May I move the bench a little?", 0, false, FactBenchAsked, "", "node.farida_qadri.reply.bench_asked",
                                         "", FactBenchAsked};
constexpr ConversationChoiceDef ComeBack{"choice.farida_qadri.come_back", "I'll come back then.", 0, false, "", "", "node.farida_qadri.reply.come_back", "", ""};
constexpr ConversationChoiceDef NotKeepYou{"choice.farida_qadri.not_keep_you", "I won't keep you.", 0, false, "", "", "node.farida_qadri.reply.not_kept", "", ""};

constexpr std::array<ConversationNodeDef, 15> Nodes{{
    // --- late (22:00-05:29), if she is awake at all ---
    {"node.farida_qadri.late", Person, "It's late, and I start early. Please let me finish here and rest.", PersonAvailability::Awake, 1320, 330,
     {}, {}, true, false, {}, 0},
    // --- preparation (05:30-05:59): busy, but she will greet ---
    {"node.farida_qadri.preparing", Person, "The water is nearly ready. There's room beside the bench.", PersonAvailability::Awake, 330, 360,
     {}, {}, true, false, {AskBench}, 1},
    // --- serving, morning (06:00-11:29) ---
    {"node.farida_qadri.serving_morning", Person, "Good morning. The first pot is ready, if you'd like a cup.", PersonAvailability::Awake, 360, 690,
     {}, {FactBenchAsked, ""}, true, false, {SitWithTea, DeclineTea, AskBench}, 3},
    {"node.farida_qadri.serving_morning_remembered", Person,
     "Good morning. You asked me about the bench first. Thank you; I like the doorway kept clear. The first pot is ready, if you'd like a cup.",
     PersonAvailability::Awake, 360, 690, {FactBenchAsked, ""}, {}, true, false, {SitWithTea, DeclineTea}, 2},
    // --- closed for the middle of the day (11:30-15:29): supplies and rest ---
    {"node.farida_qadri.closed_midday", Person, "The stall is closed for now. I'm sorting supplies and taking a rest. I open again at half past three.",
     PersonAvailability::Awake, 690, 930, {}, {}, true, false, {ComeBack}, 1},
    // --- serving, afternoon (15:30-18:59) ---
    {"node.farida_qadri.serving_afternoon", Person, "Good afternoon. The kettle is on, if you'd like a cup.", PersonAvailability::Awake, 930, 1140,
     {}, {FactBenchAsked, ""}, true, false, {SitWithTea, DeclineTea, AskBench}, 3},
    {"node.farida_qadri.serving_afternoon_remembered", Person,
     "Good afternoon. You asked me about the bench first. Thank you; I like the doorway kept clear. The kettle is on, if you'd like a cup.",
     PersonAvailability::Awake, 930, 1140, {FactBenchAsked, ""}, {}, true, false, {SitWithTea, DeclineTea}, 2},
    // --- washing up and closing (19:00-21:59) ---
    {"node.farida_qadri.closing", Person, "I'm washing the last cups. I'll open again tomorrow morning.", PersonAvailability::Awake, 1140, 1320,
     {}, {}, true, false, {NotKeepYou}, 1},
    // --- polite declines ---
    {"node.farida_qadri.busy", Person, "My hands are full just now. Could you give me a few minutes?", PersonAvailability::Busy, 0, 1440,
     {}, {}, true, true, {}, 0},
    {"node.farida_qadri.asleep", Person, "Not now, please. Come when the stall is open.", PersonAvailability::Asleep, 0, 1440,
     {}, {}, true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.farida_qadri.reply.tea_served", Person, "Here you are. Mind the cup, it's hot. There's no hurry.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {AskBench}, 1},
    {"node.farida_qadri.reply.tea_declined", Person, "That's all right. The offer stands, if you change your mind.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {SitWithTea}, 1},
    {"node.farida_qadri.reply.bench_asked", Person, "Of course, and thank you for asking. Just leave the doorway clear.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.farida_qadri.reply.come_back", Person, "Good. Come when you like.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.farida_qadri.reply.not_kept", Person, "Thank you. Mind the road.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
}};

constexpr std::array<ConversationFactDef, 1> Facts{{
    {FactBenchAsked, "", ""},
}};
} // namespace

ConversationCatalogue FaridaConversationCatalogue() noexcept
{
    return ConversationCatalogue{Nodes, Facts};
}
} // namespace PerfectWorld
