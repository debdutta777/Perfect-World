#include "ConversationTailor.h"

namespace PerfectWorld
{
namespace
{
constexpr std::string_view R66Person = "person.satyajit_dutta";
constexpr std::string_view R66Agreement = "fact.satyajit_dutta.bring_garment_agreed";
constexpr ConversationChoiceDef R66Agree{"choice.satyajit_dutta.agree", "Say you will bring a garment to discuss alterations", 0,
    false, R66Agreement, "", "node.satyajit_dutta.agreed", "", ""};
constexpr ConversationChoiceDef R66Talk{"choice.satyajit_dutta.talk", "Talk about measuring and alterations", 0,
    false, "", "", "node.satyajit_dutta.service", "", ""};
constexpr std::array<ConversationFactDef, 1> R66Facts{{{R66Agreement, "", ""}}};
constexpr std::array<ConversationNodeDef, 10> R66Nodes{{
    {"node.satyajit_dutta.morning", R66Person,
     "I'm Satyajit Dutta. I measure, cut, stitch and press here. Purchasing is unavailable; we can discuss alterations.",
     PersonAvailability::Awake, 600, 780, {}, {R66Agreement}, true, false, {R66Talk, R66Agree}, 2},
    {"node.satyajit_dutta.morning_remembered", R66Person,
     "You agreed to bring a garment to discuss alterations. Whenever you have it, we can talk. Purchasing is unavailable.",
     PersonAvailability::Awake, 600, 780, {R66Agreement}, {}, true, false, {R66Talk}, 1},
    {"node.satyajit_dutta.afternoon", R66Person,
     "Good afternoon. We can discuss fit at the counter. Purchasing is unavailable; no order or payment can be taken.",
     PersonAvailability::Awake, 900, 1170, {}, {R66Agreement}, true, false, {R66Talk, R66Agree}, 2},
    {"node.satyajit_dutta.afternoon_remembered", R66Person,
     "You agreed to bring a garment to discuss alterations. That is a conversation agreement; no order was placed. Purchasing is unavailable.",
     PersonAvailability::Awake, 900, 1170, {R66Agreement}, {}, true, false, {R66Talk}, 1},
    {"node.satyajit_dutta.break", R66Person,
     "The shop is closed for my meal and rest. Browsing resumes at three. Purchasing is unavailable.",
     PersonAvailability::Awake, 780, 900, {}, {}, true, false, {}, 0},
    {"node.satyajit_dutta.closed", R66Person,
     "The shop is closed. Browsing hours are ten to one and three to half past seven, daily. Purchasing is unavailable.",
     PersonAvailability::Awake, 1170, 600, {}, {}, true, false, {}, 0},
    {"node.satyajit_dutta.busy", R66Person,
     "Let me reach a safe pause with the cloth and tools, please. If I'm walking, meet me at the counter during browsing hours.",
     PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.satyajit_dutta.asleep", R66Person, "I'm resting. Please come during the posted browsing hours.",
     PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.satyajit_dutta.service", R66Person,
     "First measure the fit, then mark and cut, stitch, and press. Purchasing is unavailable: this discussion takes no garment, money or order.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {R66Agree}, 1},
    {"node.satyajit_dutta.agreed", R66Person,
     "I'll remember that you said you would bring one to discuss. Nothing has been handed over, ordered, paid for or altered.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {R66Talk}, 1},
}};
}

ConversationCatalogue TailorConversationCatalogue() noexcept { return {R66Nodes, R66Facts}; }
}
