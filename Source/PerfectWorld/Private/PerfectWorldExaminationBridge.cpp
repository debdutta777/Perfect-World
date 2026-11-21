#include "PerfectWorldExaminationBridge.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "P406BExaminationSupport.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldSchoolPresenter.h"

namespace P406Exam
{
PerfectWorld::P402WorldExamCatalogues P406BExaminationCatalogues(const UWorld* World)
{
    static TUniquePtr<const P406BExaminationContent> P406BContent;
    if (!P406BContent.IsValid())
    {
        const auto* P406BInstance = World != nullptr ? World->GetGameInstance() : nullptr;
        const auto* P406BDb = P406BInstance != nullptr ? P406BInstance->GetSubsystem<UPWContentSubsystem>() : nullptr;
        auto P406BLoaded = MakeUnique<P406BExaminationContent>(P406BDb != nullptr ? P406BDb->GetDb() : nullptr);
        if (!P406BLoaded->Ok())
        {
            for (const auto& P406BIssue : P406BLoaded->Issues)
                UE_LOG(LogPWLaneLife, Error, TEXT("P406B examination content unavailable: %hs %hs: %hs"),
                    P406BIssue.File.c_str(), P406BIssue.Id.c_str(), P406BIssue.Message.c_str());
            return {}; // No cached failure: a later new-life/load binding may retry.
        }
        P406BContent = MoveTemp(P406BLoaded);
    }
    return P406BContent->P406BBinding();
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
    if (P406Life == nullptr) return false;
    const auto* P406BPlayer = World != nullptr ? World->GetFirstPlayerController() : nullptr;
    const auto* P406BHud = P406BPlayer != nullptr ? Cast<APerfectWorldFrontEndHUD>(P406BPlayer->GetHUD()) : nullptr;
    auto* P406Presenter = UPWSchoolPresenter::Get(World);
    return P406BPresentDecision(P406Life->GetSim().ExaminationWorldView().DecisionRequired,
        P406BHud != nullptr && P406BHud->IsMenuOpen(), P406Presenter != nullptr && P406Presenter->IsOpen(),
        [P406Presenter]() { if (P406Presenter != nullptr) P406Presenter->P406OpenExamination(); });
}
}
