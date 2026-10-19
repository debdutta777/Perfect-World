#include "Conversation.h"

namespace PerfectWorld
{
namespace
{
constexpr std::string_view NbPerson = "person.nirmal_bose";
constexpr std::string_view NbAgreement = "fact.nirmal_bose.book_evening_agreed";
// An agreement to talk again, never a claim that a book was lent, read or returned.
constexpr ConversationChoiceDef NbAgree{"choice.nirmal_bose.agree", "Agree to another evening talking about books", 0, false,
    NbAgreement, "", "node.nirmal_bose.agreed", "", ""};
constexpr ConversationChoiceDef NbBooks{"choice.nirmal_bose.books", "Spend a little time talking about books", 10, false,
    "", "", "node.nirmal_bose.books", "", ""};
constexpr std::array<ConversationNodeDef, 6> NbNodes{{
    {"node.nirmal_bose.greeting", NbPerson, "Hello. Would you like to talk about books another evening?", PersonAvailability::Awake,
     0, 1440, {}, {NbAgreement, ""}, true, false, {NbAgree, NbBooks}, 2},
    {"node.nirmal_bose.remembered", NbPerson, "You agreed to another evening talking about books. We can talk for a little now.",
     PersonAvailability::Awake, 0, 1440, {NbAgreement, ""}, {}, true, false, {NbBooks}, 1},
    {"node.nirmal_bose.agreed", NbPerson, "Another evening, then. For now, shall we talk for a little?", PersonAvailability::Awake,
     0, 1440, {}, {}, false, false, {NbBooks}, 1},
    {"node.nirmal_bose.books", NbPerson, "I like a story that leaves room to think. We can leave the rest for another time.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {NbBooks}, 1},
    {"node.nirmal_bose.busy", NbPerson, "I have something in hand. Could we talk when I am free?", PersonAvailability::Busy,
     0, 1440, {}, {}, true, true, {}, 0},
    {"node.nirmal_bose.asleep", NbPerson, "I am asleep. Please come back later.", PersonAvailability::Asleep,
     0, 1440, {}, {}, true, true, {}, 0},
}};
constexpr std::array<ConversationFactDef, 1> NbFacts{{{NbAgreement, "", ""}}};
}

ConversationCatalogue NirmalConversationCatalogue() noexcept
{
    return {NbNodes, NbFacts};
}
} // namespace PerfectWorld
