#pragma once

#include "Conversation.h"

namespace PerfectWorld
{
// R66: also add this declaration to Conversation.h when integrating the shared catalogue.
[[nodiscard]] PERFECTWORLDCORE_API ConversationCatalogue TailorConversationCatalogue() noexcept;
}
