#include "PerfectWorldExaminationBridge.h"

#include "Engine/World.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldSchoolPresenter.h"
#include "TowerRunnerContent.h"

namespace P406Exam
{
const PerfectWorld::TowerRunnerCatalogue& P406RunnerCatalogueSeam(const PerfectWorld::ContentDb& Db)
{
    static const auto P407ULoaded = PerfectWorld::LoadTowerRunners(Db);
    return P407ULoaded.Catalogue;
}

PerfectWorld::CommandResult FFakeWorld::Submit(const FAction& Action)
{
    if (Action.bP407UFollowup)
    {
        P407USubmitted.Add(Action.P407UCommand);
        if (Reply.Accepted) Model = AfterAccepted;
        return Reply;
    }
    PerfectWorld::TowerSessionCommand P406Command;
    P406Command.Kind = Action.Kind;
    P406Command.RouteId = TCHAR_TO_UTF8(*Action.RouteId);
    P406Command.Method = Action.Method;
    P406Command.ChoiceId = TCHAR_TO_UTF8(*Action.ChoiceId);
    P406Command.Runner = Model.Session.Runner;
    P406Command.Beat = Model.Session.Beat;
    Submitted.Add(P406Command);
    if (Reply.Accepted) Model = AfterAccepted;
    return Reply;
}

bool P406PauseEntryVisible(const UWorld* World)
{
    const auto* P406Life = UPWLaneLifeSubsystem::Get(World);
    if (P406Life == nullptr) return false;
    const auto P406View = P406Life->GetSim().ExaminationWorldView();
    return P406View.DecisionRequired || P406View.CanWithdraw || P406View.Finished;
}

bool P406PresentRequired(UWorld* World)
{
    const auto* P406Life = UPWLaneLifeSubsystem::Get(World);
    if (P406Life == nullptr || !P406Life->GetSim().ExaminationWorldView().DecisionRequired) return false;
    auto* P406Presenter = UPWSchoolPresenter::Get(World);
    if (P406Presenter != nullptr && !P406Presenter->IsOpen()) P406Presenter->P406OpenExamination();
    // A blocked host must still hold free-roam time at the decision boundary.
    return true;
}
}
