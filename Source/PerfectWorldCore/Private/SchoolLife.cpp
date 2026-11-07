#include "SchoolLife.h"

#include <algorithm>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace
{
SchoolIssue P2CLifeIssue(std::string_view reason) { return {"school.life." + std::string(reason), {}}; }
std::string P2CScenePerson(const std::vector<ThreadRoleBinding>& bindings, std::string_view role)
{
    for (const auto& b : bindings) if (b.Role == role) return b.Person;
    return {};
}
} // namespace

SchoolIssue ValidateSchoolScene(const SchoolSceneDefinition& s)
{
    if (!IsReceiptIdWellFormed(s.Id) || !IsReceiptIdWellFormed(s.TitleTextKey) || !IsReceiptIdWellFormed(s.BodyTextKey) ||
        s.Roles.size() < ThreadMinRoles || s.Roles.size() > ThreadMaxRoles || s.Choices.empty() || s.Choices.size() > 8)
        return P2CLifeIssue("bad_scene");
    std::set<std::string> roles;
    for (const auto& role : s.Roles)
        if (!IsReceiptIdWellFormed(role) || !roles.insert(role).second) return P2CLifeIssue("bad_roles");
    std::set<std::string> choices;
    bool refusal = false;
    for (const auto& choice : s.Choices)
    {
        if (!IsReceiptIdWellFormed(choice.Id) || !choices.insert(choice.Id).second ||
            !IsReceiptIdWellFormed(choice.TextKey) || !IsRelationshipEventTypeFair(choice.EventType) ||
            choice.Relationships.size() > 4 || choice.CloseThreadTemplate.empty() != choice.ResolutionId.empty())
            return P2CLifeIssue("bad_choice");
        refusal = refusal || choice.IsRefusal;
        std::set<std::pair<std::string, std::string>> pairs;
        for (const auto& effect : choice.Relationships)
        {
            if (!roles.contains(effect.ActorRole) || !roles.contains(effect.OtherRole) || effect.ActorRole == effect.OtherRole ||
                static_cast<std::size_t>(effect.Kind) >= RelationshipActKindCount ||
                (choice.IsRefusal && (effect.Kind == RelationshipActKind::Insult || effect.Kind == RelationshipActKind::Betray ||
                    effect.Kind == RelationshipActKind::Bereave))) return P2CLifeIssue("bad_relationship");
            const auto key = std::minmax(effect.ActorRole, effect.OtherRole);
            if (!pairs.emplace(key.first, key.second).second) return P2CLifeIssue("duplicate_pair");
        }
    }
    return refusal ? SchoolIssue{} : P2CLifeIssue("refusal_required");
}

SchoolSceneView QuerySchoolScene(const SchoolLifeState& state, const SchoolSceneDefinition& scene)
{
    SchoolSceneView view{scene.Id, scene.TitleTextKey, scene.BodyTextKey, false, {}, scene.Choices};
    for (const auto& record : state.Scenes)
        if (record.SceneId == scene.Id) { view.Resolved = true; view.RememberedChoiceId = record.ChoiceId; view.Choices.clear(); }
    return view;
}

SchoolLifeResult ChooseSchoolScene(SchoolLifeState& state, EventLog& log, RelationshipBook& relationships, ThreadSet& threads,
    const ThreadTemplateSet& templates, const IdentityRegistry& identities, const SchoolCalendar* calendar,
    const SchoolSceneDefinition& scene, const std::vector<ThreadRoleBinding>& bindings, std::string_view choiceId, GameInstant now)
{
    SchoolLifeResult result;
    result.Issue = ValidateSchoolScene(scene);
    if (!result.Ok()) return result;
    if (QuerySchoolScene(state, scene).Resolved) { result.Issue = P2CLifeIssue("already_resolved"); return result; }
    if (state.Scenes.size() >= SchoolLifeSceneLimit) { result.Issue = P2CLifeIssue("full"); return result; }
    if (now < 0 || now > DayLoopMaxInstant) { result.Issue = P2CLifeIssue("bad_time"); return result; }
    if (bindings.size() != scene.Roles.size()) { result.Issue = P2CLifeIssue("bad_roles"); return result; }
    std::set<std::string> roleIds, people;
    for (const auto& b : bindings)
        if (std::find(scene.Roles.begin(), scene.Roles.end(), b.Role) == scene.Roles.end() || !roleIds.insert(b.Role).second ||
            !people.insert(b.Person).second || !identities.People.contains(b.Person))
            { result.Issue = P2CLifeIssue("bad_roles"); return result; }
    const auto choice = std::find_if(scene.Choices.begin(), scene.Choices.end(), [&](const auto& c) { return c.Id == choiceId; });
    if (choice == scene.Choices.end()) { result.Issue = P2CLifeIssue("unknown_choice"); return result; }
    auto nextState = state;
    auto nextLog = log;
    auto nextRelationships = relationships;
    auto nextThreads = threads;
    const auto advance = [&]()
    {
        const auto evolved = ThreadAdvance(nextThreads, templates, {&nextLog, &nextRelationships, calendar}, now);
        if (!evolved.Ok()) { result.Issue = P2CLifeIssue("thread_refused"); return false; }
        result.Threads.insert(result.Threads.end(), evolved.Events.begin(), evolved.Events.end());
        return true;
    };
    if (!advance()) return result;
    std::vector<std::string> parties;
    for (const auto& role : scene.Roles) parties.push_back(P2CScenePerson(bindings, role));
    const auto appended = nextLog.Append({now, choice->EventType, 0, parties});
    if (!appended.Ok()) { result.Issue = P2CLifeIssue("event_refused"); return result; }
    if (!advance()) return result;
    for (const auto& effect : choice->Relationships)
    {
        const auto applied = nextRelationships.Apply(nextLog, {appended.Seq, P2CScenePerson(bindings, effect.ActorRole),
            P2CScenePerson(bindings, effect.OtherRole), effect.Kind});
        if (applied.Status != RelationshipStatus::Applied && applied.Status != RelationshipStatus::Repeat &&
            applied.Status != RelationshipStatus::Frozen)
            { result.Issue = P2CLifeIssue("relationship_refused"); return result; }
        result.Relationships.push_back(applied);
    }
    if (!advance()) return result;
    if (!choice->OpenThreadTemplate.empty())
    {
        const auto* templ = templates.Find(choice->OpenThreadTemplate);
        if (!templ) { result.Issue = P2CLifeIssue("unknown_template"); return result; }
        std::vector<ThreadRoleBinding> threadBindings;
        for (const auto& role : templ->Roles) threadBindings.push_back({role.Id, P2CScenePerson(bindings, role.Id)});
        const auto opened = ThreadOpen(nextThreads, templates, templ->Id, threadBindings, identities,
            {&nextLog, &nextRelationships, calendar}, now);
        if (!opened.Ok()) { result.Issue = P2CLifeIssue("thread_refused"); return result; }
        result.Threads.insert(result.Threads.end(), opened.Events.begin(), opened.Events.end());
    }
    if (!choice->CloseThreadTemplate.empty())
    {
        const auto* templ = templates.Find(choice->CloseThreadTemplate);
        if (!templ) { result.Issue = P2CLifeIssue("unknown_template"); return result; }
        std::vector<std::string> orderedPeople;
        for (const auto& role : templ->Roles) orderedPeople.push_back(P2CScenePerson(bindings, role.Id));
        const auto instance = std::find_if(nextThreads.Active.begin(), nextThreads.Active.end(),
            [&](const auto& t) { return t.Template == templ->Id && t.People == orderedPeople; });
        if (instance == nextThreads.Active.end()) { result.Issue = P2CLifeIssue("thread_not_found"); return result; }
        const auto closed = ThreadClose(nextThreads, templates, instance->Id, choice->ResolutionId, now);
        if (!closed.Ok()) { result.Issue = P2CLifeIssue("thread_refused"); return result; }
        result.Threads.insert(result.Threads.end(), closed.Events.begin(), closed.Events.end());
    }
    nextState.Scenes.push_back({scene.Id, choice->Id, now, appended.Seq, bindings});
    state = std::move(nextState);
    log = std::move(nextLog);
    relationships = std::move(nextRelationships);
    threads = std::move(nextThreads);
    result.EventSeq = appended.Seq;
    return result;
}
} // namespace PerfectWorld
