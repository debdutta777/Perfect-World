#pragma once

#include "ContentRegistry.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld::ContentSchema
{
enum class FieldKind
{
    String,
    Bool,
    Int,
    Enum,
    Ref,
    RefArray,
    StringArray,
    /** An array of whole numbers, each within Min..Max. */
    IntArray,
    Object,
    ObjectArray
};

struct TypeSchema;

struct FieldSpec
{
    std::string Name;
    FieldKind Kind = FieldKind::String;
    bool Required = false;
    std::int64_t Min = 0;
    std::int64_t Max = 0;
    bool Nullable = false;
    std::vector<std::string> Values;
    std::string RefType;
    const TypeSchema* Nested = nullptr;
};

/**
 * A record type, or a nested record type. An empty IdPrefix means the nested
 * object has no identity of its own (a plain sub-object).
 */
struct TypeSchema
{
    std::string Type;
    std::string IdPrefix;
    std::vector<FieldSpec> Fields;
};

const std::vector<const TypeSchema*>& TopLevelSchemas();
const TypeSchema* FindTopLevel(std::string_view type);

/**
 * The fairness rule (SCHOOL_AND_PROGRESSION.md: religion, language, family
 * background and stream never carry innate stat multipliers). Walks one parsed
 * record and reports any key that attaches a multiplier, bonus, modifier,
 * XP grant or similar to one of those dimensions. It runs on every record of
 * every type, whether or not the type's schema would also refuse the key.
 */
void CheckFairness(const JsonValue& root, const std::string& file, std::vector<ContentIssue>& issues);

/** The activity record type (ContentActivitySchemas.cpp, ROADMAP M2-03). */
const TypeSchema& ActivitySchema();

/** The activity cross-field rules: the learning split, the evidence cap, the meal and outcome pairing. */
void CheckActivitySemantics(const ContentDb& db, std::vector<ContentIssue>& issues);

/** The structure, problem family and problem case record types (ContentProblemSchemas.cpp, ROADMAP M2-08). */
const TypeSchema& StructureSchema();
const TypeSchema& ProblemFamilySchema();
const TypeSchema& ProblemCaseSchema();

/** The problem rules: each case fits its family's template, the family really varies, the structure matches the runner. */
void CheckProblemSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);

/**
 * The explain, investigate and coordinate families and cases (ContentStructureSchemas.cpp, ROADMAP M2-09), and the
 * rules that tie them to their structure records, to their templates and to the runners' constants.
 */
const TypeSchema& ExplainFamilySchema();
const TypeSchema& ExplainCaseSchema();
const TypeSchema& InvestigateFamilySchema();
const TypeSchema& InvestigateCaseSchema();
const TypeSchema& CoordinateFamilySchema();
const TypeSchema& CoordinateCaseSchema();
void CheckStructureSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);

/** The bell schedule record type (ContentScheduleSchemas.cpp, ROADMAP M2-07) and its per-record and per-school rules. */
const TypeSchema& BellScheduleSchema();
void CheckBellScheduleSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);

/** The school calendar record type (ContentCalendarSchemas.cpp, ROADMAP M3-09) and its rules (SchoolCalendar.h). */
const TypeSchema& CalendarSchema();
void CheckCalendarSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);

/** The assessment paper record type (ContentAssessmentSchemas.cpp, ROADMAP M3-07) and its rules (Assessment.h). */
const TypeSchema& AssessmentPaperSchema();
void CheckAssessmentSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);

/** The ability recipe record type (ContentAbilitySchemas.cpp, ROADMAP M4-02) and its rules (Awakening.h). */
const TypeSchema& AbilityRecipeSchema();
void CheckAbilitySemantics(const ContentDb& db, std::vector<ContentIssue>& issues);

/** The thread template record type (ContentThreadSchemas.cpp, ROADMAP M3-06) and its rules (Threads.h). */
const TypeSchema& ThreadTemplateSchema();
void CheckThreadSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);

/** Cross-record rules, run after every reference has resolved. */
void CheckSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
} // namespace PerfectWorld::ContentSchema
