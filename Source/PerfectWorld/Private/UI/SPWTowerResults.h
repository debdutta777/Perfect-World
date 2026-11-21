#pragma once

#include "CoreMinimal.h"
#include "SchoolFollowupContent.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;

/** Integrator-owned bridge: query core views; revalidate IDs before opening a planner,
 * study desk or appointment conversation. No widget-supplied dates, scores or money. */
class IA8ResultsWorld
{
public:
    virtual ~IA8ResultsWorld() = default;
    virtual PerfectWorld::TowerSessionView QueryResults() const = 0;
    virtual PerfectWorld::SchoolFollowupContent FollowupContent() const = 0;
    virtual FText ResolveText(const std::string& Key) const = 0;
    virtual bool OpenNextAction(const std::string& ActionId) = 0;
};

/** Read-only results and optional next steps. Single scroll column fits narrow screens;
 * Back remains outside the scroll. No motion, ticking or UObject ownership. */
class SPWTowerResults : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWTowerResults) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(TSharedPtr<IA8ResultsWorld>, World)
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    /** Requeries before dispatch; a stale/unavailable action cannot reach the bridge. */
    bool OpenAction(const std::string& ActionId);
    const std::vector<PerfectWorld::SchoolFollowupAction>& GetActions() const { return Actions; }

private:
    FText Text(const std::string& Key) const;
    FText Summary() const;
    void Close();
    void FocusAction(int32 Index);
    TSharedPtr<IA8ResultsWorld> World;
    PerfectWorld::TowerSessionView View;
    std::vector<PerfectWorld::SchoolFollowupAction> Actions;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    TSharedPtr<SScrollBox> Scroll;
    TArray<TSharedPtr<SWidget>> Buttons;
    TSharedPtr<SWidget> Back;
    FText Feedback;
};
