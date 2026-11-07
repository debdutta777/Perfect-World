#include "TowerScenarioContent.h"

#include "ContentSchemaBuilders.h"

// The tower_scenario record shape (TowerScenarioContent.h holds the rules).
namespace PerfectWorld
{
namespace ContentSchema
{
namespace
{
using namespace Build;
const std::vector<std::string>& ScenarioMethods()
{
    static const std::vector<std::string> methods = {"combat", "analysis", "endurance", "construction", "negotiation", "logistics"};
    return methods;
}
const TypeSchema& ScenarioClueSchema()
{
    static const TypeSchema s{"tower_clue", "", {Str("id", true), OneOf("method", true, ScenarioMethods()), Str("textKey", true)}};
    return s;
}
const TypeSchema& ScenarioDecisionSchema()
{
    static const TypeSchema s{"tower_decision", "", {Str("id", true), Str("labelKey", true), Str("outcomeKey", true),
        OneOf("method", true, ScenarioMethods()), Flag("isCombat", true), Flag("isMistake", true),
        Int("scoreMilli", true, 0, TowerScoreMaxMilli), Int("minutes", true, 1, 60), Str("nextBeat", true)}};
    return s;
}
const TypeSchema& ScenarioRecoverySchema()
{
    static const TypeSchema s{"tower_recovery", "", {Str("id", true), Str("promptKey", true), Str("actionKey", true),
        Str("outcomeKey", true), OneOf("method", true, ScenarioMethods()), Int("restoredScoreMilli", true, 1, TowerScoreMaxMilli),
        Int("minutes", true, 1, 60), Str("nextBeat", true)}};
    return s;
}
const TypeSchema& ScenarioBeatSchema()
{
    static const TypeSchema s{"tower_beat", "", {Str("id", true), Str("promptKey", true),
        OneOf("metric", true, {"discipline", "common_judgment"}), Obj("clues", true, &ScenarioClueSchema(), true),
        Obj("decisions", true, &ScenarioDecisionSchema(), true), Obj("recovery", true, &ScenarioRecoverySchema(), false)}};
    return s;
}
} // namespace

// Shape only; the route, floor, beat-graph and score rules are LoadTowerScenario's.
const TypeSchema& TowerScenarioSchema()
{
    static const TypeSchema s{"tower_scenario", "p2.", Base({
        Str("route", true), Int("floor", true, 1, TowerFloorCount), Str("runner", true),
        OneOf("primaryMethod", true, ScenarioMethods()), Flag("allowsCombat", true), Str("classroomModel", true),
        Str("changedRuleKey", true), Str("titleKey", true), Str("setupKey", true), Str("entryBeat", true),
        Obj("beats", true, &ScenarioBeatSchema(), true)})};
    return s;
}
} // namespace ContentSchema
} // namespace PerfectWorld
