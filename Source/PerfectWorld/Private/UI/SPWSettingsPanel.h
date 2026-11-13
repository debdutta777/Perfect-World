#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FKbBindingStore;
class SPWControlsSection;

namespace PWSettingsUi
{
/** Tags the tests look for. */
inline const FName GeneralTabTag(TEXT("PWSettings.TabGeneral"));
inline const FName ControlsTabTag(TEXT("PWSettings.TabControls"));
inline const FName GeneralPageTag(TEXT("PWSettings.PageGeneral"));
inline const FName ControlsPageTag(TEXT("PWSettings.PageControls"));
} // namespace PWSettingsUi

/**
 * Settings, in two tabs. General: fullscreen or windowed, which is applied and saved through the engine's game user settings, reduced
 * motion, and a mouse sensitivity value that is saved but not yet read by the in-world camera (labelled as such on screen).
 * Controls: key and gamepad remapping (SPWControlsSection). The controller's shoulder buttons step between the tabs.
 */
class SPWSettingsPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWSettingsPanel) : _InitialTab(0), _BindingStore(nullptr) {}
        SLATE_EVENT(FSimpleDelegate, OnBack)
        /** 0 General, 1 Controls. */
        SLATE_ARGUMENT(int32, InitialTab)
        /** The key binding store the Controls tab edits; the process-wide one if null. Tests pass their own. */
        SLATE_ARGUMENT(FKbBindingStore*, BindingStore)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    int32 GetTab() const { return ActiveTab; }
    void SetTab(int32 Tab);

private:
    TSharedRef<SWidget> MakeGeneralTab();
    static bool IsFullscreen();
    void ToggleFullscreen();
    void SetSensitivity(float Value, bool bSave);

    FSimpleDelegate OnBack;
    float MouseSensitivity = 1.0f;
    int32 ActiveTab = 0;
    TWeakPtr<SPWControlsSection> ControlsSection;
};
