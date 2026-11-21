#pragma once

#include "UI/SPWStudyDesk.h"

/** An abbreviated Assessment runner, not a second structure simulation. Selections
 * are local planning only; all mutations use SchoolWorkCommand through IStudyWorld. */
class SPWAcademicAssessment : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWAcademicAssessment) : _Space(FVector2D(1920, 1080)) {}
        SLATE_ARGUMENT(TSharedPtr<PWA5::IStudyWorld>, World)
        SLATE_ARGUMENT(std::string, WorkId)
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()
    void Construct(const FArguments& InArgs);
    void Refresh();
    /** Same callback used by the buttons; returns the authoritative result. */
    PerfectWorld::SchoolWorkResult Send(PerfectWorld::SchoolWorkCommand Command);
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
private:
    TSharedRef<SWidget> BuildBody();
    TSharedRef<SWidget> BuildPaper(const PerfectWorld::SchoolWorkView& Work);
    TSharedRef<SWidget> BuildFeedback(const PerfectWorld::AssessmentMarking& Marking);
    TSharedRef<SWidget> Action(const FText& Label, PerfectWorld::SchoolWorkCommand Command, bool bEnabled);
    TSharedPtr<PWA5::IStudyWorld> World;
    std::string WorkId;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    TSharedPtr<SBox> Body;
    TSharedPtr<SWidget> BackButton;
    TMap<FString, TSharedPtr<SWidget>> Controls;
    FString FocusKey;
    PerfectWorld::AssessmentAnswer Draft;
    FText Notice;
};
