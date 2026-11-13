#include "Conversation.h"

// Debashis Pal, who keeps the repair workshop (R4-02). His week follows the authored schedule (npc_schedule.debashis_pal.json): he
// opens at eight, works the staff counter, the workbench, the tool rack and the finishing table through the day, goes home for the
// midday meal between 12:00 and 14:00, and closes at 18:30 (door.nabapara.repair_workshop_front: 08:00-12:00 and 14:00-18:30).
// Plain ASCII, no markup. A choice's cost is shown beside its text by the presenter, so the text itself never says it.
//
// What is promised and what is not.
//   - There is no economy yet, so there is no price, no payment and no purchase. He looks things over on the bench; he does not
//     sell anything across the counter. Holding the work lamp while he checks something is the declared cost: ten minutes, charged
//     once per session by receipt, and it may recur in a later session. Agreeing, declining and leaving are free.
//   - The one remembered fact is that the player AGREED, in conversation, TO BRING SOMETHING IN FOR HIM TO LOOK AT. It is a free
//     choice made here. After it he says only that the player said so and where to put it. He never says that the player brought
//     anything, nor that anything was looked at, mended or returned: nothing in the world records that, so no line claims it.
//   - He never speaks for the other businesses of the lane and never asks the player to do his work.
//
// A node is chosen by minute of day and availability only, never by weekday, so every line is true on any day he can be spoken to.
// He can be spoken to only while he is Awake (at the counter, or at home and not busy); the bench, the finishing table and the tool
// rack are Busy work and decline, as does sleep.
//
// Day shape (minutes since midnight), Awake:
//   19:30-07:29  the workshop is shut for the night
//   07:30-07:59  setting up the bench before opening
//   08:00-11:59  open: the counter
//   12:00-13:59  shut for the midday meal
//   14:00-18:29  open: the counter
//   18:30-19:29  shut, putting the tools back
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view DpPerson = "person.debashis_pal";
/** Set only by the player agreeing, in conversation, to bring something in for him to look at. */
constexpr std::string_view DpFactBringPromised = "fact.debashis_pal.bring_promised";

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef DpPromiseBring{"choice.debashis_pal.promise_bring", "Say you will bring something in for him to look at", 0,
                                               false, DpFactBringPromised, "", "node.debashis_pal.reply.promised", "", ""};
constexpr ConversationChoiceDef DpDeclineBring{"choice.debashis_pal.decline_bring", "Nothing today, thank you.", 0, false, "", "",
                                               "node.debashis_pal.reply.declined", "", ""};
constexpr ConversationChoiceDef DpHoldLamp{"choice.debashis_pal.hold_lamp", "Hold the work lamp while he looks something over", 10, false, "", "",
                                           "node.debashis_pal.reply.lamp_held", "", ""};
constexpr ConversationChoiceDef DpComeBack{"choice.debashis_pal.come_back", "I'll be back at two.", 0, false, "", "",
                                           "node.debashis_pal.reply.come_back", "", ""};
constexpr ConversationChoiceDef DpNotKeepYou{"choice.debashis_pal.not_keep_you", "I won't keep you.", 0, false, "", "",
                                             "node.debashis_pal.reply.not_kept", "", ""};

constexpr std::array<ConversationNodeDef, 15> DpNodes{{
    // --- shut for the night (19:30-07:29), if he is awake at all ---
    {"node.debashis_pal.late", DpPerson, "The workshop is shut for the night. Come by after eight and bring whatever needs looking at.",
     PersonAvailability::Awake, 1170, 450, {}, {}, true, false, {}, 0},
    // --- setting up the bench before opening (07:30-07:59) ---
    {"node.debashis_pal.setting_up", DpPerson, "I'm laying out the bench. We open at eight. Give me a few minutes.", PersonAvailability::Awake,
     450, 480, {}, {}, true, false, {}, 0},
    // --- open, morning (08:00-11:59) ---
    {"node.debashis_pal.counter_morning", DpPerson, "Put it on the counter and tell me what it did before it stopped.", PersonAvailability::Awake,
     480, 720, {}, {DpFactBringPromised, ""}, true, false, {DpPromiseBring, DpDeclineBring}, 2},
    {"node.debashis_pal.counter_morning_remembered", DpPerson, "You said you would bring something in. Put it on the counter whenever you have it.",
     PersonAvailability::Awake, 480, 720, {DpFactBringPromised, ""}, {}, true, false, {DpHoldLamp, DpNotKeepYou}, 2},
    // --- shut for the midday meal (12:00-13:59) ---
    {"node.debashis_pal.closed_midday", DpPerson, "We're shut for the midday meal. The bench opens again at two.", PersonAvailability::Awake, 720,
     840, {}, {}, true, false, {DpComeBack}, 1},
    // --- open, afternoon (14:00-18:29) ---
    {"node.debashis_pal.counter_afternoon", DpPerson, "Good afternoon. Leave it with me and I'll look it over on the bench.",
     PersonAvailability::Awake, 840, 1110, {}, {DpFactBringPromised, ""}, true, false, {DpPromiseBring, DpDeclineBring}, 2},
    {"node.debashis_pal.counter_afternoon_remembered", DpPerson, "You said you would bring something in. Put it on the counter whenever you have it.",
     PersonAvailability::Awake, 840, 1110, {DpFactBringPromised, ""}, {}, true, false, {DpHoldLamp, DpNotKeepYou}, 2},
    // --- shut, putting the tools back (18:30-19:29) ---
    {"node.debashis_pal.closing", DpPerson, "We're shut for today. I'm putting the tools back. Come by tomorrow.", PersonAvailability::Awake, 1110,
     1170, {}, {}, true, false, {DpNotKeepYou}, 1},
    // --- polite declines ---
    {"node.debashis_pal.busy", DpPerson, "There is something on the bench that needs me. Give me a few minutes, please.", PersonAvailability::Busy, 0, 1440,
     {}, {}, true, true, {}, 0},
    {"node.debashis_pal.asleep", DpPerson, "Not now, please. Come when the workshop is open.", PersonAvailability::Asleep, 0, 1440,
     {}, {}, true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.debashis_pal.reply.promised", DpPerson, "Good. Small things and big things both fit on the bench.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.debashis_pal.reply.declined", DpPerson, "That's all right. The bench is here if something stops working.", PersonAvailability::Awake, 0,
     1440, {}, {}, false, false, {DpPromiseBring}, 1},
    {"node.debashis_pal.reply.lamp_held", DpPerson, "A little to the left. There. Thank you, that's a help.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.debashis_pal.reply.come_back", DpPerson, "Good. The bench is open again at two.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.debashis_pal.reply.not_kept", DpPerson, "Thank you. Mind the step on your way out.", PersonAvailability::Awake, 0, 1440, {}, {}, false,
     false, {}, 0},
}};

constexpr std::array<ConversationFactDef, 1> DpFacts{{
    {DpFactBringPromised, "", ""},
}};
} // namespace

ConversationCatalogue DebashisConversationCatalogue() noexcept
{
    return ConversationCatalogue{DpNodes, DpFacts};
}
} // namespace PerfectWorld
