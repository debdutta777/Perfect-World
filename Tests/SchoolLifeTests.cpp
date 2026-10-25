#include "SchoolLife.h"

#include "P2CTestSupport.h"

using namespace PerfectWorld;
int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root"); const P2CFixture f(argv[1]);
    const auto loaded = LoadThreadTemplates(f.Content); P2CExpect(loaded.Ok(), "real thread templates");
    const auto& templ = loaded.Set.Templates.front();
    SchoolSceneDefinition scene;
    scene.Id = "school.scene.friendship"; scene.TitleTextKey = "phase2.test.title"; scene.BodyTextKey = "phase2.test.body";
    IdentityRegistry people;
    std::vector<ThreadRoleBinding> bindings;
    for (std::size_t i = 0; i < templ.Roles.size(); ++i)
    {
        scene.Roles.push_back(templ.Roles[i].Id);
        const auto id = "npc.p2c_" + std::to_string(i);
        const auto registered = RegisterAuthoredIdentity(people, id, {IdentityReferenceKind::Record, 0, "school.scene.friendship"});
        P2CExpect(registered.Ok(), "stable authored identities");
        bindings.push_back({templ.Roles[i].Id, registered.Id});
    }
    SchoolSceneChoice accept;
    accept.Id = "choice.share"; accept.TextKey = "phase2.test.share"; accept.EventType = "school.shared_time";
    accept.Relationships.push_back({scene.Roles[0], scene.Roles[1], RelationshipActKind::ShareTime});
    accept.OpenThreadTemplate = templ.Id;
    SchoolSceneChoice refuse;
    refuse.Id = "choice.refuse"; refuse.TextKey = "phase2.test.refuse"; refuse.EventType = "school.refused"; refuse.IsRefusal = true;
    scene.Choices = {accept, refuse};
    P2CExpect(ValidateSchoolScene(scene).Ok(), "scene has respectful refusal");
    SchoolLifeState state; EventLog log; RelationshipBook relationships; ThreadSet threads;
    const auto result = ChooseSchoolScene(state, log, relationships, threads, loaded.Set, people, f.Cats().Day.Calendar, scene, bindings, accept.Id, 0);
    P2CExpect(result.Ok() && result.EventSeq == 1 && relationships.Size() == 1 && threads.Active.size() == 1, "atomic real event/relationship/thread effects");
    const auto oldLog = log; const auto oldRelationships = relationships; const auto oldThreads = threads;
    P2CExpect(!ChooseSchoolScene(state, log, relationships, threads, loaded.Set, people, f.Cats().Day.Calendar, scene, bindings, accept.Id, 0).Ok() &&
        log == oldLog && relationships == oldRelationships && threads == oldThreads, "scene choice exactly once");
    P2CExpect(QuerySchoolScene(state, scene).Resolved && QuerySchoolScene(state, scene).Choices.empty(), "remembered choice view");
    scene.Id = "school.scene.later"; scene.Choices[0].OpenThreadTemplate.clear();
    const auto repeat = ChooseSchoolScene(state, log, relationships, threads, loaded.Set, people, f.Cats().Day.Calendar, scene, bindings, accept.Id, 1);
    P2CExpect(repeat.Ok() && repeat.Relationships.front().Status == RelationshipStatus::Repeat, "existing anti-farming rule retained");
    scene.Id = "school.scene.resolution";
    scene.Choices[0].CloseThreadTemplate = templ.Id; scene.Choices[0].ResolutionId = templ.Resolutions.front().Id;
    P2CExpect(ChooseSchoolScene(state, log, relationships, threads, loaded.Set, people, f.Cats().Day.Calendar, scene, bindings, accept.Id, 2).Ok() &&
        threads.Active.empty() && threads.Resolved.size() == 1, "authored thread resolution reachable");
    scene.Id = "school.scene.refusal";
    const auto refused = ChooseSchoolScene(state, log, relationships, threads, loaded.Set, people, f.Cats().Day.Calendar, scene, bindings, refuse.Id, 3);
    P2CExpect(refused.Ok() && refused.Relationships.empty(), "refusal without humiliation");
    scene.Id = "school.scene.bad"; scene.Choices[0].CloseThreadTemplate = "thread.missing";
    const auto beforeLog = log; const auto beforeRelationships = relationships; const auto beforeThreads = threads; const auto beforeSize = state.Scenes.size();
    P2CExpect(!ChooseSchoolScene(state, log, relationships, threads, loaded.Set, people, f.Cats().Day.Calendar, scene, bindings, accept.Id, 4).Ok() &&
        log == beforeLog && relationships == beforeRelationships && threads == beforeThreads && state.Scenes.size() == beforeSize, "late effect refusal rolls back every owner");
    std::cout << "P2C SchoolLife passed\n";
}
