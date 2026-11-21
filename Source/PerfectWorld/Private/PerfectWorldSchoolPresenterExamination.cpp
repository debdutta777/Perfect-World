#include "PerfectWorldSchoolPresenter.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "P406BExaminationSupport.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UI/PWWidgets.h"
#include "UI/SPWExaminationPanel.h"

bool UPWSchoolPresenter::P406OpenExamination()
{
    UWorld* P406World = GetWorld();
    if (P406World == nullptr || !BeginHosting()) return false;
    const auto P406Panel = SNew(SPWExaminationPanel).World(P406World).ViewportSize(LogicalSpace())
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::P406BCloseExamination));
    ShowPanel(P406Panel, EPWSchoolPanel::P406Examination);
    PWUI::FocusWhenReady(P406Panel, P406Panel->P406InitialFocus());
    return true;
}

void UPWSchoolPresenter::P406BCloseExamination()
{
    const auto* P406BLife = UPWLaneLifeSubsystem::Get(this);
    const bool P406BPending = P406BLife != nullptr && P406BLife->GetSim().ExaminationWorldView().DecisionRequired;
    const auto* P406BPlayer = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
    auto* P406BHud = P406BPlayer != nullptr ? Cast<APerfectWorldFrontEndHUD>(P406BPlayer->GetHUD()) : nullptr;
    P406Exam::P406BLeaveExamination(P406BPending, [this]() { Close(); }, [P406BHud]()
    {
        if (P406BHud != nullptr && !P406BHud->IsMenuOpen()) P406BHud->ToggleInWorldMenu();
    });
}
