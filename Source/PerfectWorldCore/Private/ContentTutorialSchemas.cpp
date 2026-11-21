#include "TutorialContent.h"

#include "ContentSchemaBuilders.h"

namespace PerfectWorld::ContentSchema
{
using namespace Build;
namespace
{
const TypeSchema& R93ActorSchema()
{
    static const TypeSchema s{"tutorial_actor", "actor.awakening.", {
        Str("id", true), OneOf("role", true, {"class_teacher", "classmate", "family_member", "evening_care_staff"}),
        Str("nameKey", true), Ref("schedule", false, "npc_schedule"), Ref("classroom", false, "coordinate_case"),
        Str("member", false)}};
    return s;
}
const TypeSchema& R93LineSchema()
{
    static const TypeSchema s{"tutorial_line", "", {Ref("actor", true, "tutorial_actor"), Str("textKey", true)}};
    return s;
}
const TypeSchema& R93SceneSchema()
{
    static const TypeSchema s{"tutorial_scene", "scene.awakening.", {
        Str("id", true), Obj("lines", true, &R93LineSchema(), true)}};
    return s;
}
const TypeSchema& R93ChoiceSchema()
{
    static const TypeSchema s{"tutorial_choice", "choice.awakening.", {
        Str("id", true), OneOf("action", true, {"observe", "assist", "evade", "improvise", "withdraw", "hold"}),
        Str("labelKey", true), Str("actionKey", true), Str("consequenceKey", true)}};
    return s;
}
const TypeSchema& R93SituationSchema()
{
    static const TypeSchema s{"tutorial_situation", "situation.awakening.", {
        Str("id", true), Str("promptKey", true), Str("recoveryKey", true),
        Int("stakes", true, ConductMinStakes, ConductMaxStakes), Refs("actors", true, "tutorial_actor"),
        Obj("choices", true, &R93ChoiceSchema(), true)}};
    return s;
}
}
const TypeSchema& R93TutorialSchema()
{
    static const TypeSchema s{"tutorial_content", "tutorial.", Base({
        Ref("school", true, "institution"), Ref("announcement", true, "tutorial_scene"),
        Ref("familyEvening", true, "tutorial_scene"), Ref("careEvening", true, "tutorial_scene"),
        Obj("actors", true, &R93ActorSchema(), true), Obj("scenes", true, &R93SceneSchema(), true),
        Obj("situations", true, &R93SituationSchema(), true)})};
    return s;
}
} // namespace PerfectWorld::ContentSchema
