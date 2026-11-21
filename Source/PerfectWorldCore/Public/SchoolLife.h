#pragma once

#include "SchoolSessions.h"
#include "Threads.h"

namespace PerfectWorld
{
inline constexpr std::uint32_t SchoolLifeContractVersion = 1;
inline constexpr std::size_t SchoolLifeSceneLimit = 1024;
struct SchoolSceneRelationship
{
    std::string ActorRole;
    std::string OtherRole;
    RelationshipActKind Kind = RelationshipActKind::ShareTime;
};
struct SchoolSceneChoice
{
    std::string Id;
    std::string TextKey;
    /** Event type consumed by the existing thread conditions. No identity traits. */
    std::string EventType;
    bool IsRefusal = false;
    std::vector<SchoolSceneRelationship> Relationships;
    /** Optional existing template to open using scene role bindings, then optional
     * existing instance/template resolution to close. No arbitrary thread deltas. */
    std::string OpenThreadTemplate;
    std::string CloseThreadTemplate;
    std::string ResolutionId;
};
struct SchoolSceneDefinition
{
    std::string Id;
    std::string TitleTextKey;
    std::string BodyTextKey;
    std::vector<std::string> Roles;
    std::vector<SchoolSceneChoice> Choices;
};
struct SchoolSceneRecord
{
    std::string SceneId;
    std::string ChoiceId;
    GameInstant At = 0;
    std::uint64_t EventSeq = 0;
    std::vector<ThreadRoleBinding> Bindings;
};
struct SchoolLifeState { std::vector<SchoolSceneRecord> Scenes; };
struct SchoolSceneView
{
    std::string SceneId;
    std::string TitleTextKey;
    std::string BodyTextKey;
    bool Resolved = false;
    std::string RememberedChoiceId;
    std::vector<SchoolSceneChoice> Choices;
};
struct SchoolLifeResult
{
    SchoolIssue Issue;
    std::uint64_t EventSeq = 0;
    std::vector<RelationshipApplyResult> Relationships;
    std::vector<ThreadEvent> Threads;
    [[nodiscard]] bool Ok() const noexcept { return Issue.Ok(); }
};
/** A4 content must offer a refusal and use existing cast IDs via role bindings.
 * A scene ID resolves once ever; author a distinct ID for a later consequential scene.
 * Caller controls reachability; this layer authenticates roles/effects/history. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolIssue ValidateSchoolScene(const SchoolSceneDefinition& scene);
[[nodiscard]] PERFECTWORLDCORE_API SchoolSceneView QuerySchoolScene(const SchoolLifeState& state,
    const SchoolSceneDefinition& scene);
/** school.life.choose: all five mutable owners commit together or remain unchanged.
 * Advances threads with old facts, appends the real event, advances with that event,
 * applies Relationships, then advances with the new relationship facts. No XP. */
PERFECTWORLDCORE_API SchoolLifeResult ChooseSchoolScene(SchoolLifeState& state, EventLog& log,
    RelationshipBook& relationships, ThreadSet& threads, const ThreadTemplateSet& templates,
    const IdentityRegistry& identities, const SchoolCalendar* calendar, const SchoolSceneDefinition& scene,
    const std::vector<ThreadRoleBinding>& bindings, std::string_view choiceId, GameInstant now);
} // namespace PerfectWorld
