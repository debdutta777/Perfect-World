#include "Conversation.h"

// Mahua Saha, who keeps the hardware store (R4-02). Her week follows the authored schedule (npc_schedule.mahua_saha.json): she opens
// the stock room before nine, serves at the staff counter, weighs and packs loose goods at the weigh station, restocks, takes her
// midday meal in the stock room between 13:00 and 15:00, and closes at eight in the evening (door.nabapara.hardware_store_front:
// 09:00-13:00 and 15:00-20:00). Plain ASCII, no markup. A choice's cost is shown beside its text by the presenter, so the text
// itself never says it.
//
// What is promised and what is not.
//   - There is no economy yet, so there is no price, no payment and no purchase: nothing is sold across her counter, and no line
//     talks about weighing out or handing over goods for anything. Sorting the loose screws with her is the declared cost: ten
//     minutes, charged once per session by receipt, and it may recur in a later session. Agreeing, declining and leaving are free.
//   - The one remembered fact is that the player AGREED, in conversation, TO HELP SORT THE LOOSE SCREWS ONE QUIET AFTERNOON. It is a
//     free choice made here. After it she says only that the player said so and that the tray is ready to be used. She never says
//     that the player helped, nor that anything was sorted, counted or put away: nothing in the world records that, so no line
//     claims it.
//   - She never speaks for the other businesses of the lane and never asks the player to do her work.
//
// A node is chosen by minute of day and availability only, never by weekday, so every line is true on any day she can be spoken to.
// She can be spoken to only while she is Awake (at the counter, in the stock room at her meal, or at home and not busy); weighing,
// restocking and the stock check are Busy work and decline, as does sleep.
//
// Day shape (minutes since midnight), Awake:
//   20:30-08:19  the store is shut for the night
//   08:20-08:59  counting the stock before opening
//   09:00-12:59  open: the counter
//   13:00-14:59  shut for the midday meal
//   15:00-19:59  open: the counter
//   20:00-20:29  shut, weighing up for tomorrow
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view MhPerson = "person.mahua_saha";
/** Set only by the player agreeing, in conversation, to help sort the loose screws one quiet afternoon. */
constexpr std::string_view MhFactSortPromised = "fact.mahua_saha.sort_promised";

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef MhPromiseSort{"choice.mahua_saha.promise_sort", "Say you will help sort the loose screws one quiet afternoon", 0,
                                              false, MhFactSortPromised, "", "node.mahua_saha.reply.promised", "", ""};
constexpr ConversationChoiceDef MhDeclineSort{"choice.mahua_saha.decline_sort", "Not this week, thank you.", 0, false, "", "",
                                              "node.mahua_saha.reply.declined", "", ""};
constexpr ConversationChoiceDef MhSortScrews{"choice.mahua_saha.sort_screws", "Help sort the loose screws for a while", 10, false, "", "",
                                             "node.mahua_saha.reply.sorted", "", ""};
constexpr ConversationChoiceDef MhComeBack{"choice.mahua_saha.come_back", "I'll be back at three.", 0, false, "", "",
                                           "node.mahua_saha.reply.come_back", "", ""};
constexpr ConversationChoiceDef MhNotKeepYou{"choice.mahua_saha.not_keep_you", "I won't keep you.", 0, false, "", "",
                                             "node.mahua_saha.reply.not_kept", "", ""};

constexpr std::array<ConversationNodeDef, 15> MhNodes{{
    // --- shut for the night (20:30-08:19), if she is awake at all ---
    {"node.mahua_saha.late", MhPerson, "The store is shut for the night. We open at nine. Please come then.", PersonAvailability::Awake, 1230, 500,
     {}, {}, true, false, {}, 0},
    // --- counting the stock before opening (08:20-08:59) ---
    {"node.mahua_saha.preparing", MhPerson, "I'm counting the stock before we open. Nine o'clock, please.", PersonAvailability::Awake, 500, 540, {},
     {}, true, false, {}, 0},
    // --- open, morning (09:00-12:59) ---
    {"node.mahua_saha.counter_morning", MhPerson, "Good morning. Nails and screws are in the drawers behind me; rope and tape are by the door.",
     PersonAvailability::Awake, 540, 780, {}, {MhFactSortPromised, ""}, true, false, {MhPromiseSort, MhDeclineSort}, 2},
    {"node.mahua_saha.counter_morning_remembered", MhPerson,
     "You said you would help sort the loose screws one quiet afternoon. The tray is ready whenever you are.", PersonAvailability::Awake, 540,
     780, {MhFactSortPromised, ""}, {}, true, false, {MhSortScrews, MhNotKeepYou}, 2},
    // --- shut for the midday meal (13:00-14:59) ---
    {"node.mahua_saha.closed_midday", MhPerson, "The store is shut for the midday meal. We open again at three.", PersonAvailability::Awake, 780,
     900, {}, {}, true, false, {MhComeBack}, 1},
    // --- open, afternoon (15:00-19:59) ---
    {"node.mahua_saha.counter_afternoon", MhPerson, "Good afternoon. Have a look through the drawers if you need something small.",
     PersonAvailability::Awake, 900, 1200, {}, {MhFactSortPromised, ""}, true, false, {MhPromiseSort, MhDeclineSort}, 2},
    {"node.mahua_saha.counter_afternoon_remembered", MhPerson,
     "You said you would help sort the loose screws one quiet afternoon. The tray is ready whenever you are.", PersonAvailability::Awake, 900,
     1200, {MhFactSortPromised, ""}, {}, true, false, {MhSortScrews, MhNotKeepYou}, 2},
    // --- shut, weighing up for tomorrow (20:00-20:29) ---
    {"node.mahua_saha.closing", MhPerson, "We're shut for today. I'm weighing up for tomorrow. Please come back in the morning.",
     PersonAvailability::Awake, 1200, 1230, {}, {}, true, false, {MhNotKeepYou}, 1},
    // --- polite declines ---
    {"node.mahua_saha.busy", MhPerson, "I have my hands in something just now. A few minutes, please.", PersonAvailability::Busy, 0, 1440, {}, {},
     true, true, {}, 0},
    {"node.mahua_saha.asleep", MhPerson, "Not now, please. Come when the store is open.", PersonAvailability::Asleep, 0, 1440, {}, {}, true,
     true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.mahua_saha.reply.promised", MhPerson, "Good. Come in a quiet hour and I'll find you a chair by the tray.", PersonAvailability::Awake,
     0, 1440, {}, {}, false, false, {}, 0},
    {"node.mahua_saha.reply.declined", MhPerson, "That's all right. The tray will keep until you have a free afternoon.", PersonAvailability::Awake,
     0, 1440, {}, {}, false, false, {MhPromiseSort}, 1},
    {"node.mahua_saha.reply.sorted", MhPerson, "Short ones to the left, long ones to the right. There is no hurry.", PersonAvailability::Awake, 0,
     1440, {}, {}, false, false, {}, 0},
    {"node.mahua_saha.reply.come_back", MhPerson, "Good. The shutters go up again at three.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.mahua_saha.reply.not_kept", MhPerson, "Thank you. Careful of the rope by the door.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
}};

constexpr std::array<ConversationFactDef, 1> MhFacts{{
    {MhFactSortPromised, "", ""},
}};
} // namespace

ConversationCatalogue MahuaConversationCatalogue() noexcept
{
    return ConversationCatalogue{MhNodes, MhFacts};
}
} // namespace PerfectWorld
