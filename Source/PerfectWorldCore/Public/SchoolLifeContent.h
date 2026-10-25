#pragma once

#include "Attendance.h"
#include "ContentRegistry.h"
#include "Json.h"
#include "Relationships.h"
#include "SchoolCalendar.h"
#include "Threads.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * Phase 2 Order A4: Teachers, classmates and home obligations (`school_scene`).
 *
 * Defines the portable schema, loader, semantic validator, text-key table and
 * minimal A3 `SchoolLife` trigger/outcome contract for the three bounded Phase 2
 * school-and-home arcs:
 *   1. `p2.changing_friendship`      (classmate friendship under afternoon pressure)
 *   2. `p2.group_project`            (shared practical/chart group project)
 *   3. `p2.household_responsibility` (renegotiated evening household/care-home rota)
 *
 * Every arc binds to real Nabapara Vidyalaya timetable periods, the 2042-43 school
 * calendar, an existing thread template, both `family` and `care_home` origin
 * variants, remembered relationship/thread/conversation consequences, and explicit
 * refusal choices without humiliation.
 */

inline constexpr std::string_view SchoolSceneTypeName = "school_scene";
inline constexpr std::string_view SchoolSceneIdPrefix = "p2.";

inline constexpr std::string_view SchoolArcChangingFriendshipId = "p2.changing_friendship";
inline constexpr std::string_view SchoolArcGroupProjectId = "p2.group_project";
inline constexpr std::string_view SchoolArcHouseholdResponsibilityId = "p2.household_responsibility";

/** Evening home obligation window in minutes from midnight (18:00..22:00). */
constexpr std::int32_t SchoolHomeEveningStartMinute = 18 * 60;
constexpr std::int32_t SchoolHomeEveningEndMinute = 22 * 60;

/** Care worker on-duty window at the care home (07:00..15:00). */
constexpr std::int32_t SchoolCareWorkerDutyStartMinute = 7 * 60;
constexpr std::int32_t SchoolCareWorkerDutyEndMinute = 15 * 60;

enum class SchoolArcKind : std::uint8_t
{
    ChangingFriendship,
    GroupProject,
    HouseholdResponsibility,
};

enum class SchoolOriginVariant : std::uint8_t
{
    Both,
    Family,
    CareHome,
};

enum class SchoolSceneSlotKind : std::uint8_t
{
    Assembly,
    TimetablePeriod,
    SchoolBreak,
    AfterSchool,
    CareHandover,
    HomeEvening,
};

/** Known school and home cast entry used to validate person references in `school_scene`. */
struct SchoolCastMember
{
    std::string_view PersonId;
    std::string_view DisplayName;
    /** "teacher", "classmate", "family_adult", or "care_adult". */
    std::string_view Role;
    /** True when proposed as a new Phase 2 classmate identity rather than pre-existing core cast. */
    bool bProposedNewCast = false;
};

/** Returns the canonical cast roster (existing core cast plus clearly marked proposed new classmates). */
[[nodiscard]] PERFECTWORLDCORE_API const std::vector<SchoolCastMember>& KnownSchoolLifeCast();
[[nodiscard]] PERFECTWORLDCORE_API const SchoolCastMember* FindSchoolLifeCast(std::string_view personId) noexcept;

/** Cast declaration inside one `school_scene` record. */
struct SchoolSceneCastEntry
{
    std::string PersonId;
    std::string Role;
    SchoolOriginVariant Origin = SchoolOriginVariant::Both;
    bool bProposedNewCast = false;

    friend bool operator==(const SchoolSceneCastEntry&, const SchoolSceneCastEntry&) = default;
};

/** Timetable or home-window binding for one scene node. */
struct SchoolSceneScheduleBinding
{
    SchoolSceneSlotKind SlotKind = SchoolSceneSlotKind::TimetablePeriod;
    /** `bell_schedule.*` ID when SlotKind is Assembly, TimetablePeriod, SchoolBreak or AfterSchool. */
    std::string BellScheduleId;
    /** Zero-based period index (0..5) when SlotKind is TimetablePeriod; -1 otherwise. */
    std::int32_t PeriodIndex = -1;
    /** Subject token when SlotKind is TimetablePeriod; empty otherwise. */
    std::string Subject;
    /** Room or neighbourhood location token (e.g. "nhss.xi_a", "nhss.court", "location.nabapara.care_home"). */
    std::string Zone;
    /** Start and end minutes from midnight (`StartMinute < EndMinute`). */
    std::int32_t StartMinute = 0;
    std::int32_t EndMinute = 0;

    friend bool operator==(const SchoolSceneScheduleBinding&, const SchoolSceneScheduleBinding&) = default;
};

/** Remembered consequence applied when a player selects a choice in a `school_scene`. */
struct SchoolSceneConsequence
{
    /** Target person ID for the relationship act (must belong to the arc's cast). */
    std::string TargetPersonId;
    RelationshipActKind ActKind = RelationshipActKind::ShareTime;
    /** Fair EventLog event token (validated with IsRelationshipEventTypeFair). */
    std::string RelationshipEventType;
    /** Optional event token forwarded to the arc's bound `thread_template`. */
    std::string ThreadEventType;
    /** Conversation memory fact set by this choice (e.g. "school.friendship.covering_pickup"). */
    std::string ConversationFactSet;
    /** Conversation memory fact cleared by this choice, if any. */
    std::string ConversationFactCleared;
    /** Optional journal agreement fact (`journal.agreement.<fact>` in `phase2_a4.json`). */
    std::string JournalAgreementFact;

    friend bool operator==(const SchoolSceneConsequence&, const SchoolSceneConsequence&) = default;
};

/** One player choice inside a `school_scene` node. */
struct SchoolSceneChoice
{
    std::string Id;
    std::string LabelKey;
    std::string OutcomeKey;
    std::int32_t CostMinutes = 0;
    /** True when this option declines, pauses or steps back from an ask. */
    bool bRefusal = false;
    /** Must always be false in valid content: refusals and compliances never humiliate the player. */
    bool bHumiliating = false;
    /** Next scene node ID in this arc, or empty when this choice resolves the arc. */
    std::string NextSceneId;
    /** Arc resolution ID (must match a resolution of the bound `thread_template`), or empty when continuing. */
    std::string ResolutionId;
    SchoolSceneConsequence Consequence;

    friend bool operator==(const SchoolSceneChoice&, const SchoolSceneChoice&) = default;
};

/** One scene node in a bounded `school_scene` arc. */
struct SchoolSceneNode
{
    std::string Id;
    std::string TitleKey;
    std::string PromptKey;
    std::string SpeakerPersonId;
    SchoolOriginVariant Origin = SchoolOriginVariant::Both;
    SchoolSceneScheduleBinding Schedule;
    /** Optional conversation topic ID that surfaces this scene in `Conversation.h`. */
    std::string ConversationTopicId;
    std::vector<SchoolSceneChoice> Choices;

    friend bool operator==(const SchoolSceneNode&, const SchoolSceneNode&) = default;
};

/** One complete `school_scene` arc record (`Data/Content/school_scene/p2.*.json`). */
struct SchoolSceneArc
{
    std::string Id;
    SchoolArcKind Kind = SchoolArcKind::ChangingFriendship;
    std::string TitleKey;
    std::string SummaryKey;
    std::string InstitutionId;
    std::string CalendarId;
    std::string TermId;
    std::string CalendarSpanId;
    std::string ThreadTemplateId;
    std::string EntrySceneId;
    std::vector<SchoolSceneCastEntry> Cast;
    std::vector<SchoolSceneNode> Scenes;
    std::vector<std::string> Resolutions;

    [[nodiscard]] PERFECTWORLDCORE_API const SchoolSceneNode* FindScene(std::string_view sceneId) const noexcept;
    friend bool operator==(const SchoolSceneArc&, const SchoolSceneArc&) = default;
};

struct SchoolSceneCatalogue
{
    /** Sorted by Id. */
    std::vector<SchoolSceneArc> Arcs;

    [[nodiscard]] PERFECTWORLDCORE_API const SchoolSceneArc* FindArc(std::string_view arcId) const noexcept;
    friend bool operator==(const SchoolSceneCatalogue&, const SchoolSceneCatalogue&) = default;
};

struct SchoolSceneLoadResult
{
    std::string Error;
    SchoolSceneCatalogue Catalogue;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

/** English text table loaded from `Data/Text/en/phase2_a4.json`. */
struct SchoolTextTable
{
    std::map<std::string, std::string, std::less<>> Entries;

    [[nodiscard]] PERFECTWORLDCORE_API const std::string* Find(std::string_view key) const noexcept;
    friend bool operator==(const SchoolTextTable&, const SchoolTextTable&) = default;
};

struct SchoolTextLoadResult
{
    std::string Error;
    SchoolTextTable Table;

    [[nodiscard]] bool Ok() const noexcept { return Error.empty(); }
};

// ---- Minimal A3 `SchoolLife` trigger and outcome contract ----------------------

/** Query context from the A3 `SchoolLife` loop to ask which `school_scene` nodes are active now. */
struct SchoolLifeTrigger
{
    bool bGrewUpInCare = false;
    std::string Stream = "science";
    std::string CurrentTermId = "term_1";
    std::string CurrentZone;
    std::int32_t MinuteOfDay = 0;
    /** Map from `arcId` to current `sceneId` (empty string if the arc has not started yet, or `"resolved:<id>"`). */
    std::map<std::string, std::string, std::less<>> ArcProgress;
};

/** Deterministic outcome returned to the A3 `SchoolLife` loop when a player picks a scene choice. */
struct SchoolLifeOutcome
{
    bool bOk = false;
    std::string Error;
    std::string ArcId;
    std::string SceneId;
    std::string ChoiceId;
    std::string NextSceneId;
    std::string ResolutionId;
    std::string OutcomeTextKey;
    std::int32_t ElapsedMinutes = 0;
    SchoolSceneConsequence Consequence;
};

// ---- Functions -----------------------------------------------------------------

[[nodiscard]] PERFECTWORLDCORE_API std::optional<SchoolArcKind> ParseSchoolArcKind(std::string_view token) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view SchoolArcKindName(SchoolArcKind kind) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API std::optional<SchoolOriginVariant> ParseSchoolOriginVariant(std::string_view token) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view SchoolOriginVariantName(SchoolOriginVariant variant) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool SchoolOriginMatches(SchoolOriginVariant sceneOrigin, bool bGrewUpInCare) noexcept;

[[nodiscard]] PERFECTWORLDCORE_API std::optional<SchoolSceneSlotKind> ParseSchoolSceneSlotKind(std::string_view token) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API std::string_view SchoolSceneSlotKindName(SchoolSceneSlotKind kind) noexcept;

/** Parses one `school_scene` record and appends any structural or semantic problems. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolSceneArc ParseSchoolSceneRecord(const ContentRecord& record,
                                                                         std::vector<std::string>& problems);

/** Validates one parsed arc against `ContentDb` (schedules, calendar, thread templates, reachability, variants, refusals). */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> ValidateSchoolSceneArc(const SchoolSceneArc& arc,
                                                                                   const ContentDb& db);

/** Loads and validates all `school_scene` records from `db`. Refuses atomically if any record is invalid. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolSceneLoadResult LoadSchoolSceneCatalogue(const ContentDb& db);

/**
 * Loads a directory of `ContentFile` records including `school_scene` files even before `ContentSchemas.cpp`
 * registers `SchoolSceneSchema()`, or delegates directly to `LoadContent` once registered.
 */
[[nodiscard]] PERFECTWORLDCORE_API ContentLoadResult LoadContentWithSchoolScenes(const std::vector<ContentFile>& files);

/** Loads the strict JSON string table in `Data/Text/en/phase2_a4.json`. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolTextLoadResult LoadSchoolTextTable(std::string_view jsonText);

/** Validates that every text key and journal agreement key in `catalogue` exists and is non-empty in `textTable`. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<std::string> ValidateSchoolSceneTextCoverage(
    const SchoolSceneCatalogue& catalogue, const SchoolTextTable& textTable);

/** Returns every scene node in `catalogue` that can trigger at `trigger` for the player's origin and minute. */
[[nodiscard]] PERFECTWORLDCORE_API std::vector<const SchoolSceneNode*> QueryAvailableSchoolScenes(
    const SchoolSceneCatalogue& catalogue, const SchoolLifeTrigger& trigger);

/** Resolves `choiceId` on `(arcId, sceneId)` for the given origin (`bGrewUpInCare`). */
[[nodiscard]] PERFECTWORLDCORE_API SchoolLifeOutcome ResolveSchoolSceneChoice(
    const SchoolSceneCatalogue& catalogue, std::string_view arcId, std::string_view sceneId,
    std::string_view choiceId, bool bGrewUpInCare);

namespace ContentSchema
{
struct TypeSchema;
/** Integrator: include `"SchoolLifeContent.h"` in `ContentSchemas.cpp` and add `&SchoolSceneSchema()` to `TopLevelSchemas()`. */
[[nodiscard]] PERFECTWORLDCORE_API const TypeSchema& SchoolSceneSchema();
[[nodiscard]] PERFECTWORLDCORE_API std::string ValidateSchoolSceneShape(const JsonValue& data);
/** Integrator: call `ValidateSchoolSceneSemantics(db, issues);` from `CheckSemantics` in `ContentSchemas.cpp`. */
PERFECTWORLDCORE_API void ValidateSchoolSceneSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
} // namespace ContentSchema
} // namespace PerfectWorld
