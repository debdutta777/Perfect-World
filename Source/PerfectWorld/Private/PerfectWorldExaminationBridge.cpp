#include "PerfectWorldExaminationBridge.h"

#include "Engine/World.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldSchoolPresenter.h"

namespace P406Exam
{
const PerfectWorld::TowerRunnerCatalogue& P406RunnerCatalogueSeam()
{
    // P4-05c LoadTowerRunners is absent in this base. Do not manufacture playable content here.
    static const PerfectWorld::TowerRunnerCatalogue P406Empty;
    return P406Empty;
}

PerfectWorld::CommandResult FFakeWorld::Submit(const FAction& Action)
{
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
    return P406View.DecisionRequired || P406View.CanWithdraw;
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
