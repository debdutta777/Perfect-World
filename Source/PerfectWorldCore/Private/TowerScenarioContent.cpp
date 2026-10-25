#include "TowerScenarioContent.h"

#include "ContentSchemaBuilders.h"
#include "StructureModelCatalogue.h"

#include <algorithm>
#include <array>
#include <set>

namespace PerfectWorld
{
namespace
{
constexpr bool HasPrefix(std::string_view v, std::string_view p) { return v.size() > p.size() && v.rfind(p, 0) == 0; }
const JsonValue* FindMember(const JsonValue& o, std::string_view k) { return o.Kind == JsonKind::Object ? o.Find(k) : nullptr; }

bool HasOnlyAllowedKeys(const JsonValue& obj, std::span<const std::string_view> allowed, std::string& err)
{
    for (const auto& [k, _] : obj.Object)
        if (std::find(allowed.begin(), allowed.end(), k) == allowed.end()) { err = "unknown field \"" + k + "\""; return false; }
    return true;
}

bool ReadString(const JsonValue& obj, std::string_view key, std::string& out, std::string& err, bool allowEmpty = false)
{
    const JsonValue* val = FindMember(obj, key);
    if (val == nullptr || val->Kind != JsonKind::String || (!allowEmpty && val->String.empty()))
    {
        err = "missing or invalid string field \"" + std::string(key) + "\"";
        return false;
    }
    out = val->String;
    return true;
}
bool ReadInt(const JsonValue& obj, std::string_view key, std::int64_t lo, std::int64_t hi, std::int32_t& out, std::string& err)
{
    const JsonValue* val = FindMember(obj, key);
    if (val == nullptr || val->Kind != JsonKind::Integer || val->Integer < lo || val->Integer > hi)
    {
        err = "invalid integer field \"" + std::string(key) + "\"";
        return false;
    }
    out = static_cast<std::int32_t>(val->Integer);
    return true;
}
bool ReadBool(const JsonValue& obj, std::string_view key, bool& out, std::string& err)
{
    const JsonValue* val = FindMember(obj, key);
    if (val == nullptr || val->Kind != JsonKind::Bool) { err = "invalid bool field \"" + std::string(key) + "\""; return false; }
    out = val->Bool;
    return true;
}
const TowerScenarioBeat* FindBeat(const TowerScenario& s, std::string_view id)
{
    for (const auto& b : s.Beats) if (b.Id == id) return &b;
    return nullptr;
}

bool ParseBeat(const JsonValue& node, const TowerScenario& s, std::set<std::string>& beatIds, TowerScenarioBeat& beat, std::string& err)
{
    static constexpr std::array<std::string_view, 6> beatKeys = {"id", "promptKey", "metric", "clues", "decisions", "recovery"};
    std::string metricStr;
    if (node.Kind != JsonKind::Object || !HasOnlyAllowedKeys(node, beatKeys, err) ||
        !ReadString(node, "id", beat.Id, err) || !ReadString(node, "promptKey", beat.PromptKey, err) ||
        !ReadString(node, "metric", metricStr, err) || !HasPrefix(beat.Id, "beat.") ||
        !beatIds.insert(beat.Id).second || !HasPrefix(beat.PromptKey, "p2.a7.") ||
        !TowerScenarioMetricFromId(metricStr, beat.Metric))
    {
        if (err.empty()) err = "invalid beat header in \"" + beat.Id + "\"";
        return false;
    }

    const JsonValue* cluesVal = FindMember(node, "clues");
    if (cluesVal == nullptr || cluesVal->Kind != JsonKind::Array || cluesVal->Array.size() < 2 || cluesVal->Array.size() > 4)
    {
        err = "each beat must have 2..4 clues";
        return false;
    }
    std::set<std::string> clueIds;
    for (const auto& cNode : cluesVal->Array)
    {
        static constexpr std::array<std::string_view, 3> clueKeys = {"id", "method", "textKey"};
        TowerScenarioClue clue;
        std::string mStr;
        if (cNode.Kind != JsonKind::Object || !HasOnlyAllowedKeys(cNode, clueKeys, err) ||
            !ReadString(cNode, "id", clue.Id, err) || !ReadString(cNode, "method", mStr, err) ||
            !ReadString(cNode, "textKey", clue.TextKey, err) || !HasPrefix(clue.Id, "clue.") ||
            !clueIds.insert(clue.Id).second || !TowerMethodFromId(mStr, clue.Method) || !HasPrefix(clue.TextKey, "p2.a7."))
        {
            if (err.empty()) err = "invalid clue in beat \"" + beat.Id + "\"";
            return false;
        }
        beat.Clues.push_back(std::move(clue));
    }

    const JsonValue* recVal = FindMember(node, "recovery");
    static constexpr std::array<std::string_view, 8> recKeys = {
        "id", "promptKey", "actionKey", "outcomeKey", "method", "restoredScoreMilli", "minutes", "nextBeat"};
    std::string recMStr;
    if (recVal == nullptr || recVal->Kind != JsonKind::Object || !HasOnlyAllowedKeys(*recVal, recKeys, err) ||
        !ReadString(*recVal, "id", beat.Recovery.Id, err) || !ReadString(*recVal, "promptKey", beat.Recovery.PromptKey, err) ||
        !ReadString(*recVal, "actionKey", beat.Recovery.ActionKey, err) || !ReadString(*recVal, "outcomeKey", beat.Recovery.OutcomeKey, err) ||
        !ReadString(*recVal, "method", recMStr, err) ||
        !ReadInt(*recVal, "restoredScoreMilli", 1, TowerScoreMaxMilli, beat.Recovery.RestoredScoreMilli, err) ||
        !ReadInt(*recVal, "minutes", 1, 60, beat.Recovery.Minutes, err) || !ReadString(*recVal, "nextBeat", beat.Recovery.NextBeat, err) ||
        !HasPrefix(beat.Recovery.Id, "rec.") || !HasPrefix(beat.Recovery.PromptKey, "p2.a7.") ||
        !HasPrefix(beat.Recovery.ActionKey, "p2.a7.") || !HasPrefix(beat.Recovery.OutcomeKey, "p2.a7.") ||
        !TowerMethodFromId(recMStr, beat.Recovery.Method) || beat.Recovery.Method == TowerMethod::Combat)
    {
        if (err.empty()) err = "invalid recovery in beat \"" + beat.Id + "\"";
        return false;
    }

    const JsonValue* decsVal = FindMember(node, "decisions");
    if (decsVal == nullptr || decsVal->Kind != JsonKind::Array || decsVal->Array.size() < 2 || decsVal->Array.size() > 4)
    {
        err = "each beat must have 2..4 decisions";
        return false;
    }
    std::set<std::string> decIds;
    bool hasNonCombatGood = false, hasMistake = false;
    for (const auto& dNode : decsVal->Array)
    {
        static constexpr std::array<std::string_view, 9> decKeys = {
            "id", "labelKey", "outcomeKey", "method", "isCombat", "isMistake", "scoreMilli", "minutes", "nextBeat"};
        TowerScenarioDecision dec;
        std::string dMStr;
        if (dNode.Kind != JsonKind::Object || !HasOnlyAllowedKeys(dNode, decKeys, err) ||
            !ReadString(dNode, "id", dec.Id, err) || !ReadString(dNode, "labelKey", dec.LabelKey, err) ||
            !ReadString(dNode, "outcomeKey", dec.OutcomeKey, err) || !ReadString(dNode, "method", dMStr, err) ||
            !ReadBool(dNode, "isCombat", dec.IsCombat, err) || !ReadBool(dNode, "isMistake", dec.IsMistake, err) ||
            !ReadInt(dNode, "scoreMilli", 0, TowerScoreMaxMilli, dec.ScoreMilli, err) ||
            !ReadInt(dNode, "minutes", 1, 60, dec.Minutes, err) || !ReadString(dNode, "nextBeat", dec.NextBeat, err) ||
            !HasPrefix(dec.Id, "opt.") || !decIds.insert(dec.Id).second ||
            !HasPrefix(dec.LabelKey, "p2.a7.") || !HasPrefix(dec.OutcomeKey, "p2.a7.") || !TowerMethodFromId(dMStr, dec.Method))
        {
            if (err.empty()) err = "invalid decision in beat \"" + beat.Id + "\"";
            return false;
        }
        if ((dec.IsCombat && (!s.AllowsCombat || dec.Method != TowerMethod::Combat || beat.Metric == TowerScenarioMetric::CommonJudgment)) ||
            (!dec.IsCombat && dec.Method == TowerMethod::Combat))
        {
            err = "combat decision violates route or common_judgment rules in beat \"" + beat.Id + "\"";
            return false;
        }
        if (dec.IsMistake)
        {
            hasMistake = true;
            if (dec.NextBeat != "recovery" || beat.Recovery.RestoredScoreMilli <= dec.ScoreMilli)
            {
                err = "mistake decision must route to recovery with higher restoredScoreMilli";
                return false;
            }
        }
        else if (dec.NextBeat != beat.Recovery.NextBeat)
        {
            err = "non-mistake decision nextBeat must match beat recovery nextBeat";
            return false;
        }
        else hasNonCombatGood = hasNonCombatGood || !dec.IsCombat;
        beat.Decisions.push_back(std::move(dec));
    }
    if (!hasNonCombatGood || !hasMistake) { err = "beat requires non-combat valid decision and mistake decision"; return false; }
    return true;
}
} // namespace

std::string_view TowerScenarioMetricId(TowerScenarioMetric metric)
{
    return metric == TowerScenarioMetric::CommonJudgment ? "common_judgment" : "discipline";
}
bool TowerScenarioMetricFromId(std::string_view id, TowerScenarioMetric& outMetric)
{
    if (id == "discipline") { outMetric = TowerScenarioMetric::Discipline; return true; }
    if (id == "common_judgment") { outMetric = TowerScenarioMetric::CommonJudgment; return true; }
    return false;
}
const TowerScenario* TowerScenarioCatalogue::FindById(std::string_view id) const
{
    for (const auto& s : Scenarios) if (s.Id == id) return &s;
    return nullptr;
}
const TowerScenario* TowerScenarioCatalogue::FindByRoute(std::string_view routeId) const
{
    for (const auto& s : Scenarios) if (s.RouteId == routeId) return &s;
    return nullptr;
}

TowerScenarioResult LoadTowerScenario(const ContentRecord& record)
{
    TowerScenarioResult result;
    const JsonValue& root = record.Data;
    static constexpr std::array<std::string_view, 14> topKeys = {
        "type", "id", "note", "route", "floor", "runner", "primaryMethod",
        "allowsCombat", "classroomModel", "changedRuleKey", "titleKey", "setupKey", "entryBeat", "beats"};
    if (root.Kind != JsonKind::Object || !HasOnlyAllowedKeys(root, topKeys, result.Error))
    {
        if (result.Error.empty()) result.Error = "root must be a JSON object";
        return result;
    }
    std::string typeStr, primaryMethodStr;
    std::int32_t floorInt = 0;
    TowerScenario s;
    if (!ReadString(root, "type", typeStr, result.Error) || typeStr != "tower_scenario" ||
        !ReadString(root, "id", s.Id, result.Error) || !HasPrefix(s.Id, "p2.") || (!record.Id.empty() && record.Id != s.Id) ||
        !ReadString(root, "note", s.Note, result.Error) || !ReadString(root, "route", s.RouteId, result.Error) ||
        !ReadInt(root, "floor", 1, static_cast<std::int64_t>(TowerFloorCount), floorInt, result.Error) ||
        !ReadString(root, "runner", s.RunnerId, result.Error) || !ReadString(root, "primaryMethod", primaryMethodStr, result.Error) ||
        !ReadBool(root, "allowsCombat", s.AllowsCombat, result.Error) || !ReadString(root, "classroomModel", s.ClassroomModelId, result.Error) ||
        !ReadString(root, "changedRuleKey", s.ChangedRuleKey, result.Error, true) ||
        !ReadString(root, "titleKey", s.TitleKey, result.Error) || !ReadString(root, "setupKey", s.SetupKey, result.Error) ||
        !ReadString(root, "entryBeat", s.EntryBeat, result.Error))
    {
        if (result.Error.empty()) result.Error = "invalid scenario header fields";
        return result;
    }
    s.Floor = static_cast<std::uint8_t>(floorInt);
    const TowerRouteSpec* routeSpec = FindTowerRoute(s.RouteId);
    if (routeSpec == nullptr || !TowerMethodFromId(primaryMethodStr, s.PrimaryMethod) ||
        routeSpec->Floor != s.Floor || routeSpec->AllowsCombat != s.AllowsCombat ||
        routeSpec->Method != s.PrimaryMethod || !HasPrefix(s.RunnerId, "tower.runner.") ||
        !HasPrefix(s.TitleKey, "p2.a7.") || !HasPrefix(s.SetupKey, "p2.a7."))
    {
        result.Error = "scenario route, floor, method, runner, or text key contract mismatch";
        return result;
    }
    const bool validModel = (s.Floor == 2)
        ? (FindStructureModel(s.ClassroomModelId) != nullptr && HasPrefix(s.ChangedRuleKey, "p2.a7."))
        : ((s.ClassroomModelId == "none" || FindStructureModel(s.ClassroomModelId) != nullptr) &&
           (s.ChangedRuleKey.empty() || HasPrefix(s.ChangedRuleKey, "p2.a7.")));
    if (!validModel) { result.Error = "invalid classroomModel or changedRuleKey for floor"; return result; }
    const JsonValue* beatsVal = FindMember(root, "beats");
    if (beatsVal == nullptr || beatsVal->Kind != JsonKind::Array || beatsVal->Array.size() < 3 || beatsVal->Array.size() > 6)
    {
        result.Error = "beats must be an array of 3..6 beat objects";
        return result;
    }
    std::set<std::string> beatIds;
    for (const auto& bNode : beatsVal->Array)
    {
        TowerScenarioBeat beat;
        if (!ParseBeat(bNode, s, beatIds, beat, result.Error)) return result;
        s.Beats.push_back(std::move(beat));
    }

    std::set<std::string> visited;
    std::string cursor = s.EntryBeat;
    bool hasDisc = false, hasJudg = false;
    while (cursor != "complete")
    {
        const TowerScenarioBeat* beat = FindBeat(s, cursor);
        if (beat == nullptr || !visited.insert(cursor).second) { result.Error = "unreachable or cyclic beat graph"; return result; }
        hasDisc = hasDisc || (beat->Metric == TowerScenarioMetric::Discipline);
        hasJudg = hasJudg || (beat->Metric == TowerScenarioMetric::CommonJudgment);
        cursor = beat->Recovery.NextBeat;
    }
    if (visited.size() != s.Beats.size() || !hasDisc || !hasJudg)
    {
        result.Error = "scenario must reach all beats and feed both discipline and common_judgment";
        return result;
    }

    const TowerScenarioRunResult nonCombat = BuildOptimalNonCombatAttempt(s);
    if (!nonCombat.Valid || nonCombat.Attempt.UsedCombat ||
        nonCombat.Attempt.DisciplineMilli != TowerScoreMaxMilli || nonCombat.Attempt.CommonJudgmentMilli != TowerScoreMaxMilli)
    {
        result.Error = "scenario non-combat path must reach 1000/1000 ceiling";
        return result;
    }
    if (s.AllowsCombat)
    {
        const TowerScenarioRunResult combatRun = BuildOptimalCombatAttempt(s);
        if (!combatRun.Valid || !combatRun.Attempt.UsedCombat || combatRun.Attempt.CombatFedCommonJudgment ||
            combatRun.Attempt.DisciplineMilli != TowerScoreMaxMilli || combatRun.Attempt.CommonJudgmentMilli != TowerScoreMaxMilli)
        {
            result.Error = "combat-allowing scenario must reach 1000/1000 without feeding common_judgment";
            return result;
        }
    }
    result.Value = std::move(s);
    return result;
}

TowerScenarioCatalogueResult LoadTowerScenarioCatalogue(const ContentDb& db, bool requireFullRouteCoverage)
{
    TowerScenarioCatalogueResult out;
    std::set<std::string> seenRoutes;
    std::array<bool, TowerMethodCount> seenMethods{};
    for (const auto& [id, record] : db.Records)
    {
        if (record.Type != "tower_scenario") continue;
        TowerScenarioResult parsed = LoadTowerScenario(record);
        if (!parsed.Ok()) { out.Issues.push_back({record.File, id, parsed.Error}); continue; }
        if (!seenRoutes.insert(parsed.Value->RouteId).second)
        {
            out.Issues.push_back({record.File, id, "duplicate scenario for route \"" + parsed.Value->RouteId + "\""});
            continue;
        }
        seenMethods[static_cast<std::size_t>(parsed.Value->PrimaryMethod)] = true;
        for (const auto& beat : parsed.Value->Beats)
        {
            seenMethods[static_cast<std::size_t>(beat.Recovery.Method)] = true;
            for (const auto& clue : beat.Clues) seenMethods[static_cast<std::size_t>(clue.Method)] = true;
            for (const auto& dec : beat.Decisions) seenMethods[static_cast<std::size_t>(dec.Method)] = true;
        }
        out.Catalogue.Scenarios.push_back(std::move(*parsed.Value));
    }
    if (requireFullRouteCoverage)
    {
        for (const auto& route : TowerRoutes())
            if (out.Catalogue.FindByRoute(route.Id) == nullptr)
                out.Issues.push_back({"<catalogue>", std::string(route.Id), "missing tower_scenario for route"});
        for (std::size_t idx = 0; idx < TowerMethodCount; ++idx)
            if (!seenMethods[idx])
                out.Issues.push_back({"<catalogue>", std::string(TowerMethodId(static_cast<TowerMethod>(idx))), "missing TowerMethod coverage"});
    }
    return out;
}

TowerScenarioCatalogueResult LoadTowerScenarioFiles(std::span<const ContentFile> files, bool requireFullRouteCoverage)
{
    TowerScenarioCatalogueResult out;
    ContentDb db;
    for (const auto& file : files)
    {
        JsonParseResult parsed = ParseJson(file.JsonText);
        if (!parsed.Value.has_value()) { out.Issues.push_back({file.Path, "", "JSON parse error: " + parsed.Error}); continue; }
        CheckFairness(file.Path, "", *parsed.Value, out.Issues);
        const JsonValue* idVal = FindMember(*parsed.Value, "id");
        const std::string idStr = (idVal != nullptr && idVal->Kind == JsonKind::String) ? idVal->String : "";
        std::string norm = file.Path;
        std::replace(norm.begin(), norm.end(), '\\', '/');
        const std::string suffix = "/tower_scenario/" + idStr + ".json";
        if (idStr.empty() || norm.size() < suffix.size() || norm.compare(norm.size() - suffix.size(), suffix.size(), suffix) != 0)
        {
            out.Issues.push_back({file.Path, idStr, "file path must end with " + suffix});
            continue;
        }
        if (!db.Records.emplace(idStr, ContentRecord{"tower_scenario", idStr, file.Path, std::move(*parsed.Value), {}}).second)
            out.Issues.push_back({file.Path, idStr, "duplicate content id \"" + idStr + "\""});
    }
    TowerScenarioCatalogueResult loaded = LoadTowerScenarioCatalogue(db, requireFullRouteCoverage);
    out.Catalogue = std::move(loaded.Catalogue);
    out.Issues.insert(out.Issues.end(), loaded.Issues.begin(), loaded.Issues.end());
    return out;
}

std::vector<std::string> CollectTowerScenarioTextKeys(const TowerScenario& scenario)
{
    std::vector<std::string> keys = {scenario.TitleKey, scenario.SetupKey};
    if (!scenario.ChangedRuleKey.empty()) keys.push_back(scenario.ChangedRuleKey);
    for (const auto& beat : scenario.Beats)
    {
        keys.push_back(beat.PromptKey);
        for (const auto& clue : beat.Clues) keys.push_back(clue.TextKey);
        for (const auto& dec : beat.Decisions) { keys.push_back(dec.LabelKey); keys.push_back(dec.OutcomeKey); }
        keys.push_back(beat.Recovery.PromptKey);
        keys.push_back(beat.Recovery.ActionKey);
        keys.push_back(beat.Recovery.OutcomeKey);
    }
    return keys;
}

bool LoadTowerScenarioTextTable(std::string_view jsonText, std::map<std::string, std::string>& outTable, std::string& outError)
{
    outTable.clear();
    JsonParseResult parsed = ParseJson(jsonText);
    if (!parsed.Value.has_value() || parsed.Value->Kind != JsonKind::Object)
    {
        outError = parsed.Error.empty() ? "root must be a JSON object" : parsed.Error;
        return false;
    }
    const JsonValue* stringsObj = FindMember(*parsed.Value, "strings");
    const JsonValue& source = (stringsObj != nullptr && stringsObj->Kind == JsonKind::Object) ? *stringsObj : *parsed.Value;
    for (const auto& [key, val] : source.Object)
    {
        if (val.Kind != JsonKind::String || val.String.empty() || !outTable.emplace(key, val.String).second)
        {
            outError = "invalid or duplicate text key \"" + key + "\"";
            return false;
        }
    }
    return !outTable.empty();
}

void ValidateTowerScenarioTextKeys(
    const TowerScenarioCatalogue& catalogue,
    const std::map<std::string, std::string>& textTable,
    std::vector<ContentIssue>& issues)
{
    for (const auto& scenario : catalogue.Scenarios)
        for (const auto& key : CollectTowerScenarioTextKeys(scenario))
            if (const auto it = textTable.find(key); it == textTable.end() || it->second.empty())
                issues.push_back({"Data/Text/en/phase2_a7.json", scenario.Id, "missing text key \"" + key + "\""});
}

TowerScenarioRunResult EvaluateTowerScenarioRun(const TowerScenario& scenario, std::span<const std::string_view> decisionIds)
{
    TowerScenarioRunResult run;
    run.Attempt.Floor = scenario.Floor;
    run.Attempt.RouteId = scenario.RouteId;
    run.Attempt.PrimaryMethod = scenario.PrimaryMethod;
    std::string cursor = scenario.EntryBeat;
    std::size_t stepIdx = 0;
    while (cursor != "complete")
    {
        const TowerScenarioBeat* beat = FindBeat(scenario, cursor);
        if (beat == nullptr || stepIdx >= decisionIds.size()) { run.Error = "incomplete decision sequence"; return run; }
        run.VisitedBeats.push_back(beat->Id);
        const std::string_view wantedId = decisionIds[stepIdx++];
        const TowerScenarioDecision* chosen = nullptr;
        for (const auto& dec : beat->Decisions) if (dec.Id == wantedId) { chosen = &dec; break; }
        if (chosen == nullptr) { run.Error = "decision not found in beat \"" + beat->Id + "\""; return run; }
        run.Attempt.MissionMinutes += chosen->Minutes;
        if (chosen->IsCombat)
        {
            run.Attempt.UsedCombat = true;
            run.Attempt.PrimaryMethod = TowerMethod::Combat;
            if (beat->Metric == TowerScenarioMetric::CommonJudgment) run.Attempt.CombatFedCommonJudgment = true;
        }
        std::int32_t earned = chosen->ScoreMilli;
        if (chosen->IsMistake)
        {
            ++run.RecoveriesUsed;
            run.Attempt.MissionMinutes += beat->Recovery.Minutes;
            earned = beat->Recovery.RestoredScoreMilli;
            cursor = beat->Recovery.NextBeat;
        }
        else cursor = chosen->NextBeat;
        if (beat->Metric == TowerScenarioMetric::Discipline)
            run.Attempt.DisciplineMilli = std::min(TowerScoreMaxMilli, run.Attempt.DisciplineMilli + earned);
        else
            run.Attempt.CommonJudgmentMilli = std::min(TowerScoreMaxMilli, run.Attempt.CommonJudgmentMilli + earned);
    }
    if (stepIdx != decisionIds.size()) { run.Error = "extra decisions supplied after completion"; return run; }
    run.Valid = true;
    return run;
}

TowerScenarioRunResult SelectAndEvaluate(const TowerScenario& scenario, bool preferCombat, std::size_t mistakeIdx)
{
    std::vector<std::string_view> choices;
    for (std::size_t idx = 0; idx < scenario.Beats.size(); ++idx)
    {
        const TowerScenarioDecision* best = nullptr;
        for (const auto& dec : scenario.Beats[idx].Decisions)
        {
            if (idx == mistakeIdx) { if (dec.IsMistake) { best = &dec; break; } continue; }
            if (dec.IsMistake || (!preferCombat && dec.IsCombat)) continue;
            if (best == nullptr || (preferCombat && dec.IsCombat && !best->IsCombat && dec.ScoreMilli >= best->ScoreMilli) ||
                ((!preferCombat || dec.IsCombat == best->IsCombat) && dec.ScoreMilli > best->ScoreMilli))
                best = &dec;
        }
        if (best != nullptr) choices.push_back(best->Id);
    }
    return EvaluateTowerScenarioRun(scenario, choices);
}
TowerScenarioRunResult BuildOptimalNonCombatAttempt(const TowerScenario& s) { return SelectAndEvaluate(s, false, s.Beats.size()); }
TowerScenarioRunResult BuildOptimalCombatAttempt(const TowerScenario& s) { return SelectAndEvaluate(s, true, s.Beats.size()); }
TowerScenarioRunResult BuildRecoveredMistakeAttempt(const TowerScenario& s, std::size_t idx) { return SelectAndEvaluate(s, false, idx); }
namespace ContentSchema
{
const TypeSchema& TowerScenarioSchema()
{
    static const TypeSchema schema = [] {
        const std::vector<std::string> methods = {"combat", "analysis", "endurance", "construction", "negotiation", "logistics"};
        const std::vector<std::string> metrics = {"discipline", "common_judgment"};
        TypeSchema clue = Obj("tower_clue", {Id("id", "clue."), E("method", methods), Id("textKey", "p2.a7.")});
        TypeSchema decision = Obj("tower_decision", {
            Id("id", "opt."), Id("labelKey", "p2.a7."), Id("outcomeKey", "p2.a7."), E("method", methods),
            B("isCombat"), B("isMistake"), I("scoreMilli", 0, TowerScoreMaxMilli), I("minutes", 1, 60), S("nextBeat")});
        TypeSchema recovery = Obj("tower_recovery", {
            Id("id", "rec."), Id("promptKey", "p2.a7."), Id("actionKey", "p2.a7."), Id("outcomeKey", "p2.a7."),
            E("method", methods), I("restoredScoreMilli", 1, TowerScoreMaxMilli), I("minutes", 1, 60), S("nextBeat")});
        TypeSchema beat = Obj("tower_beat", {
            Id("id", "beat."), Id("promptKey", "p2.a7."), E("metric", metrics),
            Arr("clues", FieldType::Object, 2, 4, false, {}, "", clue),
            Arr("decisions", FieldType::Object, 2, 4, false, {}, "", decision),
            Obj("recovery", recovery.Fields)});
        return TypeSchema{"tower_scenario", "p2.", {
            S("type"), Id("id", "p2."), S("note"), Id("route", "tower.route."),
            I("floor", 1, static_cast<std::int64_t>(TowerFloorCount)), Id("runner", "tower.runner."),
            E("primaryMethod", methods), B("allowsCombat"), S("classroomModel"), S("changedRuleKey", true),
            Id("titleKey", "p2.a7."), Id("setupKey", "p2.a7."), Id("entryBeat", "beat."),
            Arr("beats", FieldType::Object, 3, 6, false, {}, "", beat)}};
    }();
    return schema;
}
void ValidateTowerScenarioSemantics(const ContentDb& db, std::vector<ContentIssue>& issues)
{
    std::size_t count = 0;
    for (const auto& [_, rec] : db.Records) if (rec.Type == "tower_scenario") ++count;
    TowerScenarioCatalogueResult res = LoadTowerScenarioCatalogue(db, count >= TowerRoutes().size());
    issues.insert(issues.end(), res.Issues.begin(), res.Issues.end());
}
} // namespace ContentSchema
} // namespace PerfectWorld
