#include "P406BExaminationSupport.h"
#include "P405ExamTestSupport.h"

#include <iostream>

using namespace PerfectWorld;

namespace
{
void P406BContentFailures(const P402World& P406BWorld, const ContentDb& P406BDb)
{
    const ContentDb P406BEmpty;
    ContentDb P406BNoFollowups = P406BDb;
    for (auto P406BIt = P406BNoFollowups.Records.begin(); P406BIt != P406BNoFollowups.Records.end();)
        if (P406BIt->second.Type == "school_followup") P406BIt = P406BNoFollowups.Records.erase(P406BIt);
        else ++P406BIt;
    ContentDb P406BBroken = P406BDb;
    for (auto& [P406BId, P406BRecord] : P406BBroken.Records)
        if (P406BRecord.Type == "tower_scenario") { P406BRecord.Data = {}; break; }
    for (const auto* P406BInput : {static_cast<const ContentDb*>(nullptr), &P406BEmpty,
                                  static_cast<const ContentDb*>(&P406BNoFollowups), static_cast<const ContentDb*>(&P406BBroken)})
    {
        const P406Exam::P406BExaminationContent P406BContent(P406BInput);
        const auto P406BBinding = P406BContent.P406BBinding();
        P2CExpect(!P406BContent.Ok() && !P406BContent.Issues.empty() && P406BBinding.Runners == nullptr &&
            P406BBinding.Followups == nullptr, "failed production load publishes diagnostics and no borrowed content");
        auto P406BSim = P406BWorld.Sim(P406BWorld.Morning, P406BBinding.Runners);
        S04Refused(P406BSim, P402Cmd(TowerSessionAction::Enter), "exam.missing_content", "missing content refuses Sit honestly");
        S04Accept(P406BSim, P402Cmd(TowerSessionAction::Decline), "a failed loader leaves the participation exit usable");
    }
}

void P406BRealContent(const P402World& P406BWorld, const ContentDb& P406BDb)
{
    const P406Exam::P406BExaminationContent P406BContent(&P406BDb);
    const auto P406BBinding = P406BContent.P406BBinding();
    P2CExpect(P406BContent.Ok() && P406BContent.Runners.Scenarios.size() == 15 &&
        P406BContent.Followups.Actions.size() == 19 && P406BBinding.Runners == &P406BContent.Runners &&
        P406BBinding.Followups == &P406BContent.Followups, "production owner retains every real variant and follow-up");
    const auto P406BBuild = [&](WorldState P406BState)
    {
        return WorldSim(std::move(P406BState), MaxEventsPerAdvance, P406BWorld.W.School.Cats.Day, {},
            P406BWorld.W.School.School(), P406BWorld.W.Bind(), P406BBinding);
    };
    auto P406BNew = P406BBuild(P406BWorld.Before);
    S04Accept(P406BNew, AdvanceTime{60}, "a bound life reaches examination morning");
    P2CExpect(P406BNew.ExaminationWorldView().CanSit, "the real production binding enables Sit");
    auto P406BLoaded = P406BBuild(P402Reload(P406BWorld, P406BNew.State(), "pending decision saves and loads"));
    for (auto* P406BSim : {&P406BNew, &P406BLoaded})
    {
        P2CExpect(P406BSim->ExaminationWorldView().CanSit, "new life and Continue both enable Sit");
        S04Accept(*P406BSim, P402Cmd(TowerSessionAction::Enter), "real Sit accepted");
        S04Accept(*P406BSim, P402Cmd(TowerSessionAction::OpenRoute, "tower.route.pace", TowerMethod::Endurance),
            "real route opens");
        P405Beat(*P406BSim, P406BContent.Runners);
    }
    P2CExpect(HashWorldState(P406BNew.State()) == HashWorldState(P406BLoaded.State()), "Continue repeats the same first beat");
    auto P406BResume = P406BBuild(P402Reload(P406BWorld, P406BNew.State(), "active passage saves and loads"));
    P2CExpect(P406BResume.ExaminationView().Runner == P406BNew.ExaminationView().Runner &&
        P406BResume.ExaminationView().Beat == P406BNew.ExaminationView().Beat, "resume retains runner and beat");
    P405Beat(P406BResume, P406BContent.Runners);
    P405Beat(P406BNew, P406BContent.Runners);
    P2CExpect(HashWorldState(P406BNew.State()) == HashWorldState(P406BResume.State()), "same next choice after Continue");
    S04Accept(P406BResume, P402Cmd(TowerSessionAction::Withdraw), "real passage can withdraw");
    P405Next(P406BResume.ExaminationResult(), "withdrawn");
}

void P406BPauseLifecycle(const P402World& P406BWorld)
{
    auto P406BSim = P406BWorld.Sim(P406BWorld.Morning);
    const auto P406BBefore = HashWorldState(P406BSim.State());
    bool P406BPanelOpen = true;
    bool P406BMenuOpen = false;
    int P406BPresentations = 0;
    const auto P406BTick = [&]()
    {
        return P406Exam::P406BPresentDecision(P406BSim.ExaminationWorldView().DecisionRequired,
            P406BMenuOpen, P406BPanelOpen, [&]() { P406BPanelOpen = true; ++P406BPresentations; });
    };
    for (int P406BVisit = 0; P406BVisit < 3; ++P406BVisit)
    {
        P406Exam::P406BLeaveExamination(P406BSim.ExaminationWorldView().DecisionRequired,
            [&]() { P406BPanelOpen = false; }, [&]()
            {
                P2CExpect(!P406BPanelOpen, "panel releases input before pause opens");
                P406BMenuOpen = true;
            });
        for (int P406BFrame = 0; P406BFrame < 120; ++P406BFrame)
            P2CExpect(P406BTick() && !P406BPanelOpen, "pending decision holds time without replacing pause or its subpanels");
        P2CExpect(P406BPresentations == 0, "automatic presentation never fights pause");
        const auto P406BSaved = P402Reload(P406BWorld, P406BSim.State(), "saving from pause retains the required decision");
        P2CExpect(HashWorldState(P406BSaved) == P406BBefore, "saving and reading never mutate participation or time");
        S04Refused(P406BSim, AdvanceTime{1}, "exam.decision_required", "time remains blocked while pause owns input");
        // The Examination entry owns a panel within the existing pause host.
        P406BPanelOpen = true;
        P2CExpect(P406BTick() && P406BPresentations == 0, "reopened examination is not duplicated");
    }
    P406BPanelOpen = false;
    P406BMenuOpen = false; // Resume retries the automatic decision once.
    P2CExpect(P406BTick() && P406BPanelOpen && P406BPresentations == 1, "Resume presents the still-required decision");
    S04Accept(P406BSim, P402Cmd(TowerSessionAction::Defer), "a required choice resolves normally");
    P406BPanelOpen = false;
    bool P406BPauseRequested = false;
    P406Exam::P406BLeaveExamination(false, []() {}, [&]() { P406BPauseRequested = true; });
    P2CExpect(!P406BPauseRequested && !P406BTick(), "terminal Close returns to ordinary play");
    S04Accept(P406BSim, AdvanceTime{1}, "time resumes after the decision");
}
}

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture P406BFixture(argv[1]);
    const P402World P406BWorld(P406BFixture);
    P406BContentFailures(P406BWorld, P406BFixture.Content);
    P406BRealContent(P406BWorld, P406BFixture.Content);
    P406BPauseLifecycle(P406BWorld);
    std::cout << "P406B examination integration passed\n";
}
