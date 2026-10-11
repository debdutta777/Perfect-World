#include "ContentSchemaBuilders.h"

#include "ThreadsInternal.h"

// The thread template record (ROADMAP M3-06; ALTERNATE_WORLD.md section 8, SYSTEMS_CATALOGUE.md "Threads").
// One file per template under Data/Content/thread_template. This file is the shape only, with
// deliberately wide numeric ranges so that a number outside the design limits reaches the rules in
// ValidateThreadTemplate (Threads.h) and gets a message that says which limit it broke.
namespace PerfectWorld::ContentSchema
{
using namespace Build;
using namespace ThreadInternal;

namespace
{
const TypeSchema& ThreadSchemaExpression()
{
    static const TypeSchema s{"thread_expression", "", {
        OneOf("channel", true, NameList(ChannelNames)), Str("by", true), Str("scene", false), Str("observe", true)}};
    return s;
}

const TypeSchema& ThreadSchemaRole()
{
    static const TypeSchema s{"thread_role", "", {Str("id", true), Str("name", true)}};
    return s;
}

const TypeSchema& ThreadSchemaStage()
{
    static const TypeSchema s{"thread_stage", "", {
        Str("id", true), Str("name", true), Int("intensity", true, 0, 100000), Int("dwellDays", true, 0, 100000),
        Obj("expression", true, &ThreadSchemaExpression(), false)}};
    return s;
}

const TypeSchema& ThreadSchemaResolution()
{
    static const TypeSchema s{"thread_resolution", "", {
        Str("id", true), Str("name", true), OneOf("outcome", true, NameList(OutcomeNames)),
        Obj("expression", true, &ThreadSchemaExpression(), false)}};
    return s;
}

// One condition: "kind" says which of the other fields it takes (ThreadTemplates.cpp refuses the rest).
const TypeSchema& ThreadSchemaCondition()
{
    static const TypeSchema s{"thread_condition", "", {
        OneOf("kind", true, NameList(ConditionNames)), Int("days", false, -100000, 100000), Str("span", false),
        OneOf("phase", false, NameList(WhenNames)), Str("a", false), Str("b", false),
        OneOf("dimension", false, NameList(DimensionNames)), OneOf("op", false, NameList(CompareNames)),
        Int("value", false, -10000000, 10000000), Str("event", false), Strs("parties", false), Int("count", false, -100000, 100000),
        OneOf("window", false, NameList(WindowNames))}};
    return s;
}

const TypeSchema& ThreadSchemaTrigger()
{
    static const TypeSchema s{"thread_trigger", "", {
        Str("id", true), Str("armedFrom", true), Str("armedUntil", false), Obj("when", true, &ThreadSchemaCondition(), true),
        OneOf("effect", true, NameList(EffectNames)), Str("resolution", false),
        Obj("expression", false, &ThreadSchemaExpression(), false)}};
    return s;
}
} // namespace

const TypeSchema& ThreadTemplateSchema()
{
    static const TypeSchema s{"thread_template", "thread.", Base({
        Str("name", true), OneOf("situation", true, NameList(SituationNames)), OneOf("kind", true, NameList(KindNames)),
        Str("opensWhen", true), Ref("calendar", false, "calendar"), Obj("roles", true, &ThreadSchemaRole(), true),
        Obj("stages", true, &ThreadSchemaStage(), true), Obj("resolutions", true, &ThreadSchemaResolution(), true),
        Str("defaultResolution", true), Obj("triggers", true, &ThreadSchemaTrigger(), true)})};
    return s;
}

void CheckThreadSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    for (const ContentRecord* record : db.OfType("thread_template"))
    {
        ThreadTemplate templ;
        for (std::string& problem : ReadAndValidateThread(db, *record, templ))
            issues.push_back({record->File, record->Id, std::move(problem)});
    }
}
} // namespace PerfectWorld::ContentSchema
