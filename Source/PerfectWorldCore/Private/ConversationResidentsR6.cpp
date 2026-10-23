#include "Conversation.h"

namespace PerfectWorld
{
namespace
{
// R6-04: Madhabi Das (alley house) and Arup Basu (stair flats). Each catalogue remembers exactly one agreement. No line claims
// a favour, a visit or a task was completed. Busy also covers travel and off-lane work: there is no separate Absent availability.
// Physical absence and sleep are refused by the schedule-derived host predicate and the door rules, not by these lines.
constexpr std::string_view R64Madhabi = "person.madhabi_das";
constexpr std::string_view R64MadhabiFact = "fact.madhabi_das.ward_records_agreed";
constexpr ConversationChoiceDef R64MadhabiAgree{"choice.madhabi_das.agree", "Agree to talk about the old ward records another evening", 0, false,
    R64MadhabiFact, "", "node.madhabi_das.agreed", "", R64MadhabiFact};
constexpr ConversationChoiceDef R64MadhabiTalk{"choice.madhabi_das.talk", "Talk about the files she keeps in order", 15, false,
    "", "", "node.madhabi_das.talked", "", ""};
constexpr std::array<ConversationNodeDef, 10> R64MadhabiNodes{{
    {"node.madhabi_das.morning", R64Madhabi,
     "Good morning. The records office keeps me busy on weekdays, but the mornings at home are mine. Shall we talk about the old ward records another evening?",
     PersonAvailability::Awake, 0, 720, {}, {R64MadhabiFact}, true, false, {R64MadhabiAgree, R64MadhabiTalk}, 2},
    {"node.madhabi_das.morning_remembered", R64Madhabi,
     "You agreed to talk about the old ward records another evening. We can talk a little this morning, if the kettle waits.",
     PersonAvailability::Awake, 0, 720, {R64MadhabiFact}, {}, true, false, {R64MadhabiTalk}, 1},
    {"node.madhabi_das.afternoon", R64Madhabi,
     "The afternoon is for resting my eyes. Years of small handwriting do that. Shall we talk about the old ward records another evening?",
     PersonAvailability::Awake, 720, 1080, {}, {R64MadhabiFact}, true, false, {R64MadhabiAgree, R64MadhabiTalk}, 2},
    {"node.madhabi_das.afternoon_remembered", R64Madhabi,
     "You agreed to an evening of the old ward records. There is a little time this afternoon too, if you would like to sit.",
     PersonAvailability::Awake, 720, 1080, {R64MadhabiFact}, {}, true, false, {R64MadhabiTalk}, 1},
    {"node.madhabi_das.evening", R64Madhabi,
     "Good evening. I count the days to my retirement the way I count files, in neat rows. Shall we talk about the old ward records another evening?",
     PersonAvailability::Awake, 1080, 1440, {}, {R64MadhabiFact}, true, false, {R64MadhabiAgree, R64MadhabiTalk}, 2},
    {"node.madhabi_das.evening_remembered", R64Madhabi,
     "You agreed to an evening of the old ward records. This evening we can talk a little, if you have time.",
     PersonAvailability::Awake, 1080, 1440, {R64MadhabiFact}, {}, true, false, {R64MadhabiTalk}, 1},
    {"node.madhabi_das.agreed", R64Madhabi,
     "Another evening, then. Bring yourself; the files stay at the office.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {R64MadhabiTalk}, 1},
    {"node.madhabi_das.talked", R64Madhabi,
     "Every house on the lane has a file with its owner's name and the year it was last corrected. People are more than their papers, but papers remember dates.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.madhabi_das.busy", R64Madhabi,
     "I cannot stop just now; I am at my desk or on the way there. Try me at home in the late afternoon, once I am back.",
     PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.madhabi_das.asleep", R64Madhabi, "Madhabi is asleep. There is no reply; come back when she is awake.",
     PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
}};
constexpr std::array<ConversationFactDef, 1> R64MadhabiFacts{{{R64MadhabiFact, "", ""}}};

constexpr std::string_view R64Arup = "person.arup_basu";
constexpr std::string_view R64ArupFact = "fact.arup_basu.parcel_round_agreed";
constexpr ConversationChoiceDef R64ArupAgree{"choice.arup_basu.agree", "Agree to hear about his parcel round another evening", 0, false,
    R64ArupFact, "", "node.arup_basu.agreed", "", R64ArupFact};
constexpr ConversationChoiceDef R64ArupTalk{"choice.arup_basu.talk", "Talk about the parcels that go astray", 10, false,
    "", "", "node.arup_basu.talked", "", ""};
constexpr std::array<ConversationNodeDef, 10> R64ArupNodes{{
    {"node.arup_basu.morning", R64Arup,
     "Morning. I keep an early shift, so for me this is already half the day. Shall we talk about the parcel round another evening?",
     PersonAvailability::Awake, 0, 720, {}, {R64ArupFact}, true, false, {R64ArupAgree, R64ArupTalk}, 2},
    {"node.arup_basu.morning_remembered", R64Arup,
     "You agreed to hear about the parcel round another evening. I have a few minutes this morning, if you would rather start now.",
     PersonAvailability::Awake, 0, 720, {R64ArupFact}, {}, true, false, {R64ArupTalk}, 1},
    {"node.arup_basu.afternoon", R64Arup,
     "Afternoons are for putting my feet up after the round. Shall we talk about the parcel round another evening?",
     PersonAvailability::Awake, 720, 1080, {}, {R64ArupFact}, true, false, {R64ArupAgree, R64ArupTalk}, 2},
    {"node.arup_basu.afternoon_remembered", R64Arup,
     "You agreed to an evening of the parcel round. I have time this afternoon too, if you want to sit a while.",
     PersonAvailability::Awake, 720, 1080, {R64ArupFact}, {}, true, false, {R64ArupTalk}, 1},
    {"node.arup_basu.evening", R64Arup,
     "Good evening. I go to bed early, so this is my last quiet stretch. Shall we talk about the parcel round another evening?",
     PersonAvailability::Awake, 1080, 1440, {}, {R64ArupFact}, true, false, {R64ArupAgree, R64ArupTalk}, 2},
    {"node.arup_basu.evening_remembered", R64Arup,
     "You agreed to an evening of the parcel round. We can talk a little now, before my early night.",
     PersonAvailability::Awake, 1080, 1440, {R64ArupFact}, {}, true, false, {R64ArupTalk}, 1},
    {"node.arup_basu.agreed", R64Arup,
     "Another evening, then. No need to arrange anything; the round will still be there.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {R64ArupTalk}, 1},
    {"node.arup_basu.talked", R64Arup,
     "Most parcels go astray over a missing flat number or a smudged phone number. A careful label saves more trouble than a fast van.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.arup_basu.busy", R64Arup,
     "I cannot talk while I am at the depot or on my round. Try me at home in the late afternoon, when I am back.",
     PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.arup_basu.asleep", R64Arup, "Arup is asleep. There is no reply; come back when he is awake.",
     PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
}};
constexpr std::array<ConversationFactDef, 1> R64ArupFacts{{{R64ArupFact, "", ""}}};
} // namespace

ConversationCatalogue MadhabiConversationCatalogue() noexcept
{
    return {R64MadhabiNodes, R64MadhabiFacts};
}

ConversationCatalogue ArupConversationCatalogue() noexcept
{
    return {R64ArupNodes, R64ArupFacts};
}
} // namespace PerfectWorld
