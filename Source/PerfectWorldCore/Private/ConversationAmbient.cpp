#include "Conversation.h"

namespace PerfectWorld
{
namespace
{
// Busy includes walking: these three greet, then offer Leave, as the shared catalogue
// contract requires. Their authored pauses offer a longer chat. No purchase or reward.
constexpr ConversationChoiceDef AmAmbientPurnimaChoice{"choice.purnima_das.round", "Ask about her round", 2, false,
    "", "", "node.purnima_das.reply", "", ""};
constexpr ConversationChoiceDef AmAmbientArunChoice{"choice.arun_paul.round", "Ask about his walk", 2, false,
    "", "", "node.arun_paul.reply", "", ""};
constexpr ConversationChoiceDef AmAmbientRekhaChoice{"choice.rekha_bose.round", "Ask about her errands", 2, false,
    "", "", "node.rekha_bose.reply", "", ""};
constexpr std::array<ConversationNodeDef, 12> AmAmbientNodes{{
    {"node.purnima_das.greet", "person.purnima_das", "The cart can wait here. I am Purnima. I take this round with fruit, then put the cart away between rounds.", PersonAvailability::Awake, 0, 1440, {}, {}, true, false, {AmAmbientPurnimaChoice}, 1},
    {"node.purnima_das.walking", "person.purnima_das", "I can stop a moment. Come to one of my stopping places if you want a longer chat.", PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.purnima_das.asleep", "person.purnima_das", "I am resting now. Another time, please.", PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.purnima_das.reply", "person.purnima_das", "I use three stopping places so people have space to pass. I go off the lane for my meal and rest.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.arun_paul.greet", "person.arun_paul", "Hello. Arun Paul. I am taking the lane on my way to the bus; a short walk clears my head.", PersonAvailability::Awake, 0, 1440, {}, {}, true, false, {AmAmbientArunChoice}, 1},
    {"node.arun_paul.walking", "person.arun_paul", "Hello. I can stop for a moment, then I must be on my way.", PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.arun_paul.asleep", "person.arun_paul", "I am resting now. Another time, please.", PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.arun_paul.reply", "person.arun_paul", "I like a little time on foot between the house and the bus. There is no need to hurry every step.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.rekha_bose.greet", "person.rekha_bose", "Hello, I am Rekha Bose. I take this way for my errands. There is usually someone to greet.", PersonAvailability::Awake, 0, 1440, {}, {}, true, false, {AmAmbientRekhaChoice}, 1},
    {"node.rekha_bose.walking", "person.rekha_bose", "Hello. Let us leave room for people to pass. I must get on with my errands.", PersonAvailability::Busy, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.rekha_bose.asleep", "person.rekha_bose", "I am resting now. Another time, please.", PersonAvailability::Asleep, 0, 1440, {}, {}, true, true, {}, 0},
    {"node.rekha_bose.reply", "person.rekha_bose", "I write down what I need before leaving. Otherwise I get home with the one thing I needed still missing.", PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
}};
}
ConversationCatalogue AmbientConversationCatalogue() noexcept { return {AmAmbientNodes, {}}; }
} // namespace PerfectWorld
