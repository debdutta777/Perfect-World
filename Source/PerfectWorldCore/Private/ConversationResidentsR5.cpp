#include "Conversation.h"

namespace PerfectWorld
{
namespace
{
constexpr std::string_view R5RPrabir = "person.prabir_chatterjee";
constexpr std::string_view R5RPrabirFact = "fact.prabir_chatterjee.lane_stories_agreed";
constexpr ConversationChoiceDef R5RPrabirAgree{"choice.prabir_chatterjee.agree", "Agree to talk about the lane another afternoon", 0, false,
    R5RPrabirFact, "", "node.prabir_chatterjee.agreed", "", R5RPrabirFact};
constexpr ConversationChoiceDef R5RPrabirTalk{"choice.prabir_chatterjee.talk", "Talk about how the lane has changed", 10, false,
    "", "", "node.prabir_chatterjee.talked", "", ""};

// These lines remember only an agreement. They never claim a visit, favour or task was completed.
// Busy also covers travel and off-lane errands: there is no separate Absent availability enum.
// Physical absence/asleep refusal belongs to the schedule-derived host predicate and door rules.
constexpr std::array<ConversationNodeDef, 10> R5RPrabirNodes{{
    {"node.prabir_chatterjee.morning", R5RPrabir,
     "Good morning. Retirement has left me time for my own newspaper. Shall we talk about the lane another afternoon?",
     PersonAvailability::Awake, 0, 720, {}, {R5RPrabirFact}, true, false, {R5RPrabirAgree, R5RPrabirTalk}, 2},
    {"node.prabir_chatterjee.morning_remembered", R5RPrabir,
     "You agreed to talk about the lane another afternoon. There is time for a little conversation this morning, too.",
     PersonAvailability::Awake, 0, 720, {R5RPrabirFact}, {}, true, false, {R5RPrabirTalk}, 1},
    {"node.prabir_chatterjee.afternoon", R5RPrabir,
     "The afternoon is my quiet time. I live here on my own, but I like company. Shall we talk about the lane another afternoon?",
     PersonAvailability::Awake, 720, 1080, {}, {R5RPrabirFact}, true, false, {R5RPrabirAgree, R5RPrabirTalk}, 2},
    {"node.prabir_chatterjee.afternoon_remembered", R5RPrabir,
     "You agreed to another afternoon talking about the lane. Shall we sit for a little? I have my own stories, and you will have yours.",
     PersonAvailability::Awake, 720, 1080, {R5RPrabirFact}, {}, true, false, {R5RPrabirTalk}, 1},
    {"node.prabir_chatterjee.evening", R5RPrabir,
     "Good evening. I used to sort letters; now I read without watching the clock. Shall we talk about the lane another afternoon?",
     PersonAvailability::Awake, 1080, 1440, {}, {R5RPrabirFact}, true, false, {R5RPrabirAgree, R5RPrabirTalk}, 2},
    {"node.prabir_chatterjee.evening_remembered", R5RPrabir,
     "You agreed to another afternoon talking about the lane. This evening we can talk a little, if you have time.",
     PersonAvailability::Awake, 1080, 1440, {R5RPrabirFact}, {}, true, false, {R5RPrabirTalk}, 1},
    {"node.prabir_chatterjee.agreed", R5RPrabir,
     "Another afternoon, then. No need to bring anything; a conversation is enough.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {R5RPrabirTalk}, 1},
    {"node.prabir_chatterjee.talked", R5RPrabir,
     "When I sorted post, I knew addresses before I knew faces. A new flat number tells you very little about the person inside.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.prabir_chatterjee.busy", R5RPrabir,
     "I cannot stop just now. Try me at home in the late afternoon; I keep the mornings for my errands and the house.",
     PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.prabir_chatterjee.asleep", R5RPrabir, "Prabir is asleep. There is no reply; come back when he is awake.",
     PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
}};
constexpr std::array<ConversationFactDef, 1> R5RPrabirFacts{{{R5RPrabirFact, "", ""}}};

constexpr std::string_view R5RIshita = "person.ishita_banerjee";
constexpr std::string_view R5RIshitaFact = "fact.ishita_banerjee.quiet_evening_agreed";
constexpr ConversationChoiceDef R5RIshitaAgree{"choice.ishita_banerjee.agree", "Agree to another quiet evening talking", 0, false,
    R5RIshitaFact, "", "node.ishita_banerjee.agreed", "", R5RIshitaFact};
constexpr ConversationChoiceDef R5RIshitaTalk{"choice.ishita_banerjee.talk", "Talk about making time outside work", 15, false,
    "", "", "node.ishita_banerjee.talked", "", ""};
constexpr std::array<ConversationNodeDef, 10> R5RIshitaNodes{{
    {"node.ishita_banerjee.morning", R5RIshita,
     "Good morning. An early clinic shift suits me; I want some of the day for myself. Shall we talk another quiet evening?",
     PersonAvailability::Awake, 0, 720, {}, {R5RIshitaFact}, true, false, {R5RIshitaAgree, R5RIshitaTalk}, 2},
    {"node.ishita_banerjee.morning_remembered", R5RIshita,
     "You agreed to another quiet evening talking. We can talk a little this morning, if we both have time.",
     PersonAvailability::Awake, 0, 720, {R5RIshitaFact}, {}, true, false, {R5RIshitaTalk}, 1},
    {"node.ishita_banerjee.afternoon", R5RIshita,
     "The afternoon is a good time to put work aside. Shall we talk another quiet evening?",
     PersonAvailability::Awake, 720, 1080, {}, {R5RIshitaFact}, true, false, {R5RIshitaAgree, R5RIshitaTalk}, 2},
    {"node.ishita_banerjee.afternoon_remembered", R5RIshita,
     "You agreed to another quiet evening talking. There is time this afternoon for a little conversation, too.",
     PersonAvailability::Awake, 720, 1080, {R5RIshitaFact}, {}, true, false, {R5RIshitaTalk}, 1},
    {"node.ishita_banerjee.evening", R5RIshita,
     "Good evening. I like having an evening with no forms to sort. Shall we talk another quiet evening?",
     PersonAvailability::Awake, 1080, 1440, {}, {R5RIshitaFact}, true, false, {R5RIshitaAgree, R5RIshitaTalk}, 2},
    {"node.ishita_banerjee.evening_remembered", R5RIshita,
     "You agreed to another quiet evening talking. What do you like to do when your time is your own?",
     PersonAvailability::Awake, 1080, 1440, {R5RIshitaFact}, {}, true, false, {R5RIshitaTalk}, 1},
    {"node.ishita_banerjee.agreed", R5RIshita, "Another evening, then. We need not turn it into an appointment.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {R5RIshitaTalk}, 1},
    {"node.ishita_banerjee.talked", R5RIshita,
     "At the clinic I sort records, and those stay private. Here I like recipes and radio plays. Work need not fill every conversation.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.ishita_banerjee.busy", R5RIshita,
     "I cannot talk while I am occupied or on my way. Try me at home in the late afternoon, when I can give you my time.",
     PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.ishita_banerjee.asleep", R5RIshita, "Ishita is asleep. There is no reply; let her rest and return later.",
     PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
}};
constexpr std::array<ConversationFactDef, 1> R5RIshitaFacts{{{R5RIshitaFact, "", ""}}};
} // namespace

ConversationCatalogue PrabirConversationCatalogue() noexcept
{
    return {R5RPrabirNodes, R5RPrabirFacts};
}

ConversationCatalogue IshitaConversationCatalogue() noexcept
{
    return {R5RIshitaNodes, R5RIshitaFacts};
}
} // namespace PerfectWorld
