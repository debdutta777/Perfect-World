#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldExaminationBridge.h"
#include "Misc/Optional.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;
class SVerticalBox;

/** P406: four examination screens share the lesson panel's paused, screen-aligned host. */
class SPWExaminationPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWExaminationPanel) : _World(nullptr), _ViewportSize(1920.0, 1080.0) {}
        SLATE_ARGUMENT(UWorld*, World)
        SLATE_ARGUMENT(TSharedPtr<P406Exam::IWorld>, BridgeOverride)
        SLATE_ARGUMENT(FVector2D, ViewportSize)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void P406Refresh();
    bool P406TriggerAction(int32 Index);
    bool P406TriggerWithdraw();
    bool P407UFinishSession();
    const P406Exam::FModel& P406GetModel() const { return P406Model; }
    const FText& P406GetNotice() const { return P406Notice; }
    int32 P406GetFocusCount() const { return P406Controls.Num(); }
    int32 P406GetFocusIndex() const { return P406Selected; }
    void P406SelectFocus(int32 Index);
    TSharedPtr<SWidget> P406InitialFocus() const;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
    void P406Rebuild();
    bool P406Send(const P406Exam::FAction& Action);
    void P406Close();
    void P406AddLine(const FText& Text);
    void P406AddControl(const FText& Text, bool bEnabled, TFunction<bool()> Activate, bool bFooter);

    /** Unpaid confirmation for attendance-only meetings; task progress always comes from the core. */
    TOptional<P406Exam::FAction> P506Meeting;
    TSharedPtr<P406Exam::IWorld> P406Bridge;
    P406Exam::FModel P406Model;
    FText P406Notice;
    FSimpleDelegate P406OnClose;
    FVector2D P406Space;
    TSharedPtr<SVerticalBox> P406Body;
    TSharedPtr<SVerticalBox> P406Footer;
    TSharedPtr<SScrollBox> P406Scroll;
    TArray<TSharedPtr<SWidget>> P406Controls;
    TArray<TFunction<bool()>> P406Activations;
    int32 P406Selected = 0;
};
