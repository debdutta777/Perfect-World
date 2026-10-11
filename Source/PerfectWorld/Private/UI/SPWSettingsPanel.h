#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Settings: fullscreen or windowed, which is applied and saved through the
 * engine's game user settings, and a mouse sensitivity value that is saved but
 * not yet read by the in-world camera (labelled as such on screen).
 */
class SPWSettingsPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWSettingsPanel) {}
        SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
    static bool IsFullscreen();
    void ToggleFullscreen();
    void SetSensitivity(float Value, bool bSave);

    FSimpleDelegate OnBack;
    float MouseSensitivity = 1.0f;
};
