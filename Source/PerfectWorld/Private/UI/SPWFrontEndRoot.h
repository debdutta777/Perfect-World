#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldFrontEndHUD.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SBox;

/**
 * The root of the whole front end: a darkened scrim over the 3D scene and one
 * screen at a time (title, load, settings, creation, pause), plus the
 * save-slot chooser and a short notice at the bottom. The HUD owns it and tells
 * it which screen to show; it asks the HUD to do anything that touches the world.
 */
class SPWFrontEndRoot : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWFrontEndRoot) {}
        SLATE_ARGUMENT(TWeakObjectPtr<APerfectWorldFrontEndHUD>, Hud)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    void ShowTitle();
    /** Opens the creation flow on a step, as PWUI::ECreationStep. */
    void ShowCreation(int32 Step);
    void ShowPause();
    /** Removes every screen so the world shows through. A notice, if any, stays up. */
    void Hide();
    /** A short message near the bottom of the screen that clears itself. */
    void ShowToast(const FText& Message);

private:
    enum class EScreen : uint8
    {
        None,
        Title,
        Load,
        Settings,
        Creation,
        Pause
    };

    void SetScreen(EScreen NewScreen, const TSharedRef<SWidget>& Content);
    void ShowLoad();
    void ShowSettings();
    void HandleNewGame();
    FText HandleStartLife();
    void OpenOverwriteChooser();
    void CloseOverwriteChooser();

    TWeakObjectPtr<APerfectWorldFrontEndHUD> Hud;
    TSharedPtr<SBox> ScreenHolder;
    TSharedPtr<SBox> ChooserHolder;
    EScreen Screen = EScreen::None;
    bool bChooserOpen = false;
    FText ChooserStatus;
    FText Toast;
    int32 ToastSerial = 0;
};
