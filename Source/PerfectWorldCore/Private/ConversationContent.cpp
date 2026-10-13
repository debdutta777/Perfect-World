#include "Conversation.h"

// The authored conversation catalogue: one person, the household adult of the opening home.
// The three time-dependent lines are the "Household adult / care worker" samples in LANE_LIFE_PLAN section B;
// the rest are a short greeting set and replies. Plain ASCII, no markup. A choice's cost is shown beside its text
// by the presenter ("Talk about the evening jobs - 3 minutes"), so the text itself never says it.
//
// Day shape (minutes since midnight):
//   05:00-10:59  breakfast (before the rota is agreed), or the remembered arrangement (after it)
//   11:00-21:59  the rota is still open, or settled
//   22:00-04:59  late: rest
// An Asleep or Busy person declines, whatever the hour.
namespace PerfectWorld
{
namespace
{
constexpr std::string_view Person = HouseholdAdultId;

constexpr std::array<ConversationNodeDef, 12> Nodes{{
    // --- the greeting and time-dependent lines (Awake) ---
    {"node.household_adult.breakfast", Person,
     "Breakfast is ready. Tell me what time you expect to be back.",
     PersonAvailability::Awake, 300, 660, "", FactEveningRotaAgreed, true, false,
     {{{"choice.household_adult.back_before_dark", "Before dark.", 0, false, "", "node.household_adult.reply.back_noted"},
       {"choice.household_adult.back_around_seven", "Around seven.", 0, false, "", "node.household_adult.reply.back_noted"},
       {"choice.household_adult.back_unsure", "I am not sure yet.", 0, false, "", "node.household_adult.reply.back_noted"}}},
     3},
    {"node.household_adult.rota_open", Person,
     "We still need to agree who does the evening jobs.",
     PersonAvailability::Awake, 660, 1320, "", FactEveningRotaAgreed, true, false,
     {{{"choice.household_adult.talk_rota", "Talk about the evening jobs", 3, true, FactEveningRotaAgreed,
        "node.household_adult.reply.rota_agreed"}}},
     1},
    {"node.household_adult.rota_settled", Person,
     "That is settled, then. We will try it this evening.",
     PersonAvailability::Awake, 660, 1320, FactEveningRotaAgreed, "", true, false, {}, 0},
    {"node.household_adult.remembered", Person,
     "The arrangement you suggested worked yesterday. Shall we keep it?",
     PersonAvailability::Awake, 300, 660, FactEveningRotaAgreed, FactEveningRotaKept, true, false,
     {{{"choice.household_adult.keep_rota", "Yes, let us keep it.", 0, true, FactEveningRotaKept,
        "node.household_adult.reply.kept"},
       {"choice.household_adult.revisit_rota", "Talk it over again", 3, true, "", "node.household_adult.reply.revisit"}}},
     2},
    {"node.household_adult.kept_morning", Person,
     "Breakfast is ready. The arrangement is working well.",
     PersonAvailability::Awake, 300, 660, FactEveningRotaKept, "", true, false, {}, 0},
    {"node.household_adult.late", Person,
     "It is late. You should get some rest.",
     PersonAvailability::Awake, 1320, 300, "", "", true, false, {}, 0},
    // --- polite declines ---
    {"node.household_adult.busy", Person,
     "I am in the middle of something. Can we talk in a little while?",
     PersonAvailability::Busy, 0, 1440, "", "", true, true, {}, 0},
    {"node.household_adult.asleep", Person,
     "Mmm... not now. Talk to me in the morning.",
     PersonAvailability::Asleep, 0, 1440, "", "", true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.household_adult.reply.back_noted", Person,
     "All right. Let me know if it changes.",
     PersonAvailability::Awake, 0, 1440, "", "", false, false, {}, 0},
    {"node.household_adult.reply.rota_agreed", Person,
     "Good. We will try that this evening.",
     PersonAvailability::Awake, 0, 1440, "", "", false, false, {}, 0},
    {"node.household_adult.reply.kept", Person,
     "Good. Then that is one less thing to worry about.",
     PersonAvailability::Awake, 0, 1440, "", "", false, false, {}, 0},
    {"node.household_adult.reply.revisit", Person,
     "All right. Tell me what you would change.",
     PersonAvailability::Awake, 0, 1440, "", "", false, false, {}, 0},
}};

constexpr std::array<std::string_view, 2> Facts{FactEveningRotaAgreed, FactEveningRotaKept};
} // namespace

std::span<const ConversationNodeDef> ConversationNodes() noexcept
{
    return Nodes;
}

std::span<const std::string_view> ConversationFactIds() noexcept
{
    return Facts;
}
} // namespace PerfectWorld
