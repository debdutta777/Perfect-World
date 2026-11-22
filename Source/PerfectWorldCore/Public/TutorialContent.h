#pragma once

#include "ConductLedger.h"
#include "ContentRegistry.h"

#include <map>

namespace PerfectWorld
{
// Content aliases are not persistent person IDs. Bind roles to the current cast;
// schedule/classroom sources retain established names without inventing identities.
struct TutorialActor
{
    std::string Id, Role, NameKey, ScheduleId, ClassroomId, MemberId;
    friend bool operator==(const TutorialActor&, const TutorialActor&) = default;
};
struct TutorialLine
{
    std::string ActorId, TextKey;
    friend bool operator==(const TutorialLine&, const TutorialLine&) = default;
};
struct TutorialScene
{
    std::string Id;
    std::vector<TutorialLine> Lines;
    friend bool operator==(const TutorialScene&, const TutorialScene&) = default;
};
struct TutorialChoice
{
    std::string Id, LabelKey, ActionKey, ConsequenceKey;
    ConductAction Action = ConductAction::Observe;
    friend bool operator==(const TutorialChoice&, const TutorialChoice&) = default;
};
struct TutorialSituation
{
    std::string Id, PromptKey, RecoveryKey;
    std::int32_t Stakes = ConductMinStakes; // shared by EVERY choice, including withdrawal
    std::vector<std::string> ActorIds;
    std::vector<TutorialChoice> Choices; // all offered unconditionally; no timing/trait gates
    friend bool operator==(const TutorialSituation&, const TutorialSituation&) = default;
};
struct TutorialContent
{
    std::string Id, SchoolId, AnnouncementId, FamilyEveningId, CareEveningId;
    std::vector<TutorialActor> Actors;
    std::vector<TutorialScene> Scenes;
    std::vector<TutorialSituation> Situations; // authored encounter order
    std::map<std::string, std::string, std::less<>> Text;
    friend bool operator==(const TutorialContent&, const TutorialContent&) = default;
};
struct TutorialContentResult
{
    TutorialContent Content; // empty on any error, including a missing text key
    std::vector<ContentIssue> Issues;
    [[nodiscard]] bool Ok() const { return Issues.empty(); }
};

// textFile is a JSON object mapping text.awakening.* keys to nonempty strings.
// No filesystem access or fallback strings. Both Db and text are validated here.
[[nodiscard]] PERFECTWORLDCORE_API TutorialContentResult LoadTutorialContent(
    const ContentDb& db, const ContentFile& textFile);
[[nodiscard]] PERFECTWORLDCORE_API const TutorialSituation* FindTutorialSituation(
    const TutorialContent& content, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const TutorialChoice* FindTutorialChoice(
    const TutorialSituation& situation, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const TutorialScene* FindTutorialScene(
    const TutorialContent& content, std::string_view id);
[[nodiscard]] PERFECTWORLDCORE_API const std::string* FindTutorialText(
    const TutorialContent& content, std::string_view key);

namespace ContentSchema
{
struct TypeSchema;
const TypeSchema& R93TutorialSchema();
void ValidateTutorialContentSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
}
} // namespace PerfectWorld
