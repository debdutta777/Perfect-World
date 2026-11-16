#include "PerfectWorldSchoolPresenter.h"

#include "Engine/World.h"
#include "UI/PWWidgets.h"
#include "UI/SPWExaminationPanel.h"

bool UPWSchoolPresenter::P406OpenExamination()
{
    UWorld* P406World = GetWorld();
    if (P406World == nullptr || !BeginHosting()) return false;
    const auto P406Panel = SNew(SPWExaminationPanel).World(P406World).ViewportSize(LogicalSpace())
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::Close));
    ShowPanel(P406Panel, EPWSchoolPanel::P406Examination);
    PWUI::FocusWhenReady(P406Panel, P406Panel->P406InitialFocus());
    return true;
}
