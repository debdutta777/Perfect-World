#include "Conversation.h"

// The authored household catalogue: the adult of the opening home. The first two sample lines are the "Household adult /
// care worker" lines in LANE_LIFE_PLAN section B, verbatim. The third ("worked yesterday") claimed a result nothing yet
// records, so it is replaced: the morning line remembers WHAT was agreed ("You suggested ...") and never that it worked,
// until completed chores exist to say so. Plain ASCII, no markup. A choice's cost is shown beside its text by the
// presenter ("Talk about the evening jobs - 3 minutes"), so the text itself never says it.
//
// The evening jobs. Talking about them costs three minutes (paid, and it may recur: a household can talk again). The talk
// offers three concrete proposals; choosing one records it as the agreed arrangement (a fact), replaces any earlier
// arrangement, and withdraws a "kept" that belonged to the old one. Each proposal is hidden while it is the arrangement.
//
// Day shape (minutes since midnight):
//   05:00-10:59  breakfast (before anything is agreed), or the remembered arrangement (asked, then kept)
//   11:00-21:59  the jobs are still open, or settled
//   22:00-04:59  late: rest
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view Person = HouseholdAdultId;

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef BackBeforeDark{"choice.household_adult.back_before_dark", "Before dark.", 0, false, "", "", "node.household_adult.reply.back_noted", "", ""};
constexpr ConversationChoiceDef BackAroundSeven{"choice.household_adult.back_around_seven", "Around seven.", 0, false, "", "", "node.household_adult.reply.back_noted", "", ""};
constexpr ConversationChoiceDef BackUnsure{"choice.household_adult.back_unsure", "I am not sure yet.", 0, false, "", "", "node.household_adult.reply.back_noted", "", ""};
constexpr ConversationChoiceDef TalkRota{"choice.household_adult.talk_rota", "Talk about the evening jobs", 3, false, "", "", "node.household_adult.reply.proposals", "", ""};
constexpr ConversationChoiceDef RevisitRota{"choice.household_adult.revisit_rota", "Talk it over again", 3, false, "", "", "node.household_adult.reply.revisit", "", ""};
constexpr ConversationChoiceDef KeepRota{"choice.household_adult.keep_rota", "Yes, let us keep it.", 0, false, FactEveningRotaKept, "", "node.household_adult.reply.kept", "", ""};
constexpr ConversationChoiceDef ProposeDishes{"choice.household_adult.propose_dishes", "I take the dishes on weekdays.", 0, false, FactRotaDishesWeekdays, FactEveningRotaKept,
                                              "node.household_adult.reply.dishes_weekdays", "", FactRotaDishesWeekdays};
constexpr ConversationChoiceDef ProposeAlternate{"choice.household_adult.propose_alternate", "We alternate days.", 0, false, FactRotaAlternateDays, FactEveningRotaKept,
                                                 "node.household_adult.reply.alternate_days", "", FactRotaAlternateDays};
constexpr ConversationChoiceDef ProposeSweepCook{"choice.household_adult.propose_sweep_cook", "I sweep and you cook.", 0, false, FactRotaSweepAndCook, FactEveningRotaKept,
                                                 "node.household_adult.reply.sweep_and_cook", "", FactRotaSweepAndCook};

constexpr std::array<ConversationNodeDef, 23> Nodes{{
    // --- breakfast, before anything is agreed (Awake, 05:00-10:59) ---
    {"node.household_adult.breakfast", Person, "Breakfast is ready. Tell me what time you expect to be back.", PersonAvailability::Awake, 300, 660,
     {}, {FactEveningRotaAgreed, ""}, true, false, {BackBeforeDark, BackAroundSeven, BackUnsure}, 3},
    // An agreement from the first version of this conversation that never said how the jobs are shared.
    {"node.household_adult.remembered_unnamed", Person, "Breakfast is ready. We agreed to share the evening jobs, but we never said how. Shall we put it plainly?",
     PersonAvailability::Awake, 300, 660, {FactEveningRotaAgreed, ""}, {FactRotaDishesWeekdays, FactRotaAlternateDays, FactRotaSweepAndCook}, true, false, {TalkRota}, 1},
    // --- the next morning: what was agreed, asked once whether to keep it ---
    {"node.household_adult.remembered_dishes_weekdays", Person, "Good morning. You suggested taking the dishes on weekdays. Shall we keep it that way?",
     PersonAvailability::Awake, 300, 660, {FactRotaDishesWeekdays, ""}, {FactEveningRotaKept, ""}, true, false, {KeepRota, RevisitRota}, 2},
    {"node.household_adult.remembered_alternate_days", Person, "Good morning. You suggested we alternate days. Shall we keep it that way?",
     PersonAvailability::Awake, 300, 660, {FactRotaAlternateDays, ""}, {FactEveningRotaKept, ""}, true, false, {KeepRota, RevisitRota}, 2},
    {"node.household_adult.remembered_sweep_and_cook", Person, "Good morning. You suggested that you sweep and I cook. Shall we keep it that way?",
     PersonAvailability::Awake, 300, 660, {FactRotaSweepAndCook, ""}, {FactEveningRotaKept, ""}, true, false, {KeepRota, RevisitRota}, 2},
    // --- the mornings after it was kept: it is still what was agreed, and no more is claimed ---
    {"node.household_adult.kept_dishes_weekdays", Person, "Breakfast is ready. We are keeping to what we agreed: you take the dishes on weekdays.",
     PersonAvailability::Awake, 300, 660, {FactRotaDishesWeekdays, FactEveningRotaKept}, {}, true, false, {RevisitRota}, 1},
    {"node.household_adult.kept_alternate_days", Person, "Breakfast is ready. We are keeping to what we agreed: we alternate days.",
     PersonAvailability::Awake, 300, 660, {FactRotaAlternateDays, FactEveningRotaKept}, {}, true, false, {RevisitRota}, 1},
    {"node.household_adult.kept_sweep_and_cook", Person, "Breakfast is ready. We are keeping to what we agreed: you sweep and I cook.",
     PersonAvailability::Awake, 300, 660, {FactRotaSweepAndCook, FactEveningRotaKept}, {}, true, false, {RevisitRota}, 1},
    // --- the rest of the day: open, or settled (Awake, 11:00-21:59) ---
    {"node.household_adult.rota_open", Person, "We still need to agree who does the evening jobs.", PersonAvailability::Awake, 660, 1320,
     {}, {FactEveningRotaAgreed, ""}, true, false, {TalkRota}, 1},
    {"node.household_adult.rota_unnamed", Person, "We agreed to share the evening jobs, but we never said how.", PersonAvailability::Awake, 660, 1320,
     {FactEveningRotaAgreed, ""}, {FactRotaDishesWeekdays, FactRotaAlternateDays, FactRotaSweepAndCook}, true, false, {TalkRota}, 1},
    {"node.household_adult.rota_settled_dishes_weekdays", Person, "That is settled, then: you take the dishes on weekdays.",
     PersonAvailability::Awake, 660, 1320, {FactRotaDishesWeekdays, ""}, {}, true, false, {RevisitRota}, 1},
    {"node.household_adult.rota_settled_alternate_days", Person, "That is settled, then: we alternate days.",
     PersonAvailability::Awake, 660, 1320, {FactRotaAlternateDays, ""}, {}, true, false, {RevisitRota}, 1},
    {"node.household_adult.rota_settled_sweep_and_cook", Person, "That is settled, then: you sweep and I cook.",
     PersonAvailability::Awake, 660, 1320, {FactRotaSweepAndCook, ""}, {}, true, false, {RevisitRota}, 1},
    {"node.household_adult.late", Person, "It is late. You should get some rest.", PersonAvailability::Awake, 1320, 300, {}, {}, true, false, {}, 0},
    // --- polite declines ---
    {"node.household_adult.busy", Person, "I am in the middle of something. Can we talk in a little while?", PersonAvailability::Busy, 0, 1440,
     {}, {}, true, true, {}, 0},
    {"node.household_adult.asleep", Person, "Mmm... not now. Talk to me in the morning.", PersonAvailability::Asleep, 0, 1440,
     {}, {}, true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.household_adult.reply.back_noted", Person, "All right. Let me know if it changes.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.household_adult.reply.proposals", Person, "All right. What would you like to suggest?", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {ProposeDishes, ProposeAlternate, ProposeSweepCook}, 3},
    {"node.household_adult.reply.revisit", Person, "All right. Tell me what you would change.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {ProposeDishes, ProposeAlternate, ProposeSweepCook}, 3},
    {"node.household_adult.reply.dishes_weekdays", Person, "That is fair. The dishes are yours on weekdays. We will see how it goes.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.household_adult.reply.alternate_days", Person, "Alternate days, then. Tonight we can decide who starts.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.household_adult.reply.sweep_and_cook", Person, "You sweep and I cook. That suits me. Let us try it.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.household_adult.reply.kept", Person, "Good. Then we both know where we stand.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
}};

constexpr std::array<ConversationFactDef, 5> Facts{{
    {FactEveningRotaAgreed, "", ""},
    {FactEveningRotaKept, "", FactEveningRotaAgreed},
    {FactRotaDishesWeekdays, "evening_rota_arrangement", FactEveningRotaAgreed},
    {FactRotaAlternateDays, "evening_rota_arrangement", FactEveningRotaAgreed},
    {FactRotaSweepAndCook, "evening_rota_arrangement", FactEveningRotaAgreed},
}};
} // namespace

ConversationCatalogue HouseholdConversationCatalogue() noexcept
{
    return ConversationCatalogue{Nodes, Facts};
}

ConversationCatalogue DefaultConversationCatalogue() noexcept
{
    return HouseholdConversationCatalogue();
}
} // namespace PerfectWorld
