#include "PerfectWorldSchoolPresenter.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "PerfectWorldAwakeningResultBridge.h"
#include "UI/SPWAwakeningResultPanel.h"
#include "UI/SPWCountdownPanel.h"

// P38: reuse the existing paused school host; panels only read/submit through the live bridges.
bool UPWSchoolPresenter::OpenAwakeningResult()
{
    UWorld* World = GetWorld();
    const auto Bridge = PWAwakening::ApBridge::Resolve(World);
    if (!Bridge.IsValid() || !Bridge->ReadResultModel().bGranted || !BeginHosting()) return false;
    const TSharedRef<SPWAwakeningResultPanel> Result = SNew(SPWAwakeningResultPanel)
        .World(World).BridgeOverride(Bridge).ViewportSize(LogicalSpace())
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::Close));
    ShowPanel(Result, EPWSchoolPanel::AwakeningResult);
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Panel, EFocusCause::SetDirectly);
    return true;
}

bool UPWSchoolPresenter::OpenAwakeningCountdown()
{
    UWorld* World = GetWorld();
    if (World == nullptr || !BeginHosting()) return false;
    const TSharedRef<SPWCountdownPanel> Countdown = SNew(SPWCountdownPanel)
        .World(World).BridgeOverride(PWAwakening::ApBridge::Resolve(World)).ViewportSize(LogicalSpace())
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWSchoolPresenter::Close));
    ShowPanel(Countdown, EPWSchoolPanel::AwakeningCountdown);
    if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(Panel, EFocusCause::SetDirectly);
    return true;
}
