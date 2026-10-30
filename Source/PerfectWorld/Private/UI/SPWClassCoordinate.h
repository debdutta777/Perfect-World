#pragma once

#include "CoreMinimal.h"
#include "UI/SPWClassInvestigate.h"

namespace PWClassCoordinate
{
/** Who is in the group. Capacity and load are the case's own numbers; DraftLoad is plain arithmetic over the assignment shown. */
struct FMemberRow
{
    FString Id;
    FText Name;
    int32 Capacity = 0;
    bool bSelf = false;
    bool bHeard = false;
    int32 DraftLoad = 0;
    bool bOver = false;
    /** The core's quote for talking to this member now. */
    int32 TalkUnits = 0;
    PerfectWorld::SchoolIssue TalkIssue;
};

struct FTaskRow
{
    FString Id;
    FText Name;
    int32 Load = 0;
    /** The members who suit the task, in words. */
    FText Suits;
    /** The member the task is given to (draft while assigning, committed afterwards); empty when not yet given. */
    FString Assignee;
    FText AssigneeName;
    bool bSuited = true;
};

struct FResolutionRow
{
    PerfectWorld::CoordinateResolution Resolution = PerfectWorld::CoordinateResolution::Leave;
    FText Name;
    FText Hint;
    int32 Units = 0;
    PerfectWorld::SchoolIssue Issue;
};

/** A fact the player knows. Whether it is the critical one is never part of this row. */
struct FFactRow
{
    FText Holder;
    FText Text;
};

struct FModel
{
    bool bActive = false;
    FString CaseId;
    FText CaseName;
    FText CaseBrief;
    FText Role;
    FText Scarce;
    FText Goal;
    FText Method;
    PerfectWorld::CoordinateBeat Beat = PerfectWorld::CoordinateBeat::Assign;
    int32 UnitsLeft = 0;
    int32 UnitsTotal = 0;
    TArray<FMemberRow> Members;
    TArray<FTaskRow> Tasks;
    TArray<FResolutionRow> Resolutions;
    TArray<FFactRow> Facts;
    /** Every task has a member in the draft (or the assignment is committed). */
    bool bDraftComplete = false;
    int32 AssignUnits = 0;
    PerfectWorld::SchoolIssue AssignIssue;
    PerfectWorld::SchoolIssue EndExchangeIssue;
    int32 CheckUnits = 0;
    PerfectWorld::SchoolIssue CheckIssue;
    int32 PlainUnits = 0;
    PerfectWorld::SchoolIssue PlainIssue;
};

/** task id -> member id: what the player has pencilled in before committing. Not authoritative; only Commit sends it to the core. */
using FDraft = TMap<FString, FString>;

/** Reads the running exposure through the bridge's quotes. An idle or foreign run gives an inactive model. */
FModel BuildModel(const PWClassA3::ILessonBridge& Bridge, const PWClassA3::FFrame& Frame, const FDraft& Draft);
} // namespace PWClassCoordinate

/**
 * StructureCoordinate lesson: four ordered beats (assign, exchange, settle, submit) over a small group project. The player pencils in
 * who does what with live load against capacity, talks to classmates one at a time to learn what they know, chooses how to settle
 * what binds the group and hands in with or without a check. Which fact is critical, what binds the group and whether the parts
 * join are never shown: they are what the exchange and the check are for.
 */
class SPWClassCoordinate : public SPWClassLessonShell
{
public:
    SLATE_BEGIN_ARGS(SPWClassCoordinate) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(TSharedPtr<PWClassA3::ILessonBridge>, Bridge)
        /** The logical size of the screen the panel may use. The pause menu passes the size it already measures. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
        SLATE_EVENT(FPWClassLessonClosed, OnLessonClosed)
        /** Focus goes back here after OnClose. */
        SLATE_ARGUMENT(TWeakPtr<SWidget>, ReturnFocus)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    const PWClassCoordinate::FModel& GetModel() const { return Model; }
    const PWClassCoordinate::FDraft& GetDraft() const { return Draft; }
    /** Pencils the task to the next member (wrapping); a pencil mark costs nothing and changes nothing in the world. */
    bool CycleTaskAssignee(const FString& TaskId);
    bool SetTaskAssignee(const FString& TaskId, const FString& MemberId);
    PerfectWorld::SchoolSessionResult ExecuteCommitAssignment();
    PerfectWorld::SchoolSessionResult ExecuteExchange(const FString& MemberId);
    PerfectWorld::SchoolSessionResult ExecuteEndExchange();
    PerfectWorld::SchoolSessionResult ExecuteSettle(PerfectWorld::CoordinateResolution Resolution);
    PerfectWorld::SchoolSessionResult ExecuteSubmit(bool bCheck);

protected:
    virtual PerfectWorld::SchoolStructure ShellStructure() const override { return PerfectWorld::SchoolStructure::Coordinate; }
    virtual FText ShellSubtitle() const override;
    virtual int32 ShellGoalCount() const override;
    virtual FText ShellGoalName(int32 Index) const override;
    virtual FText ShellGoalHint(int32 Index) const override;
    virtual PerfectWorld::SchoolLessonGoal ShellGoalAt(int32 Index) const override;
    virtual void ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const override;
    virtual void ShellFrameChanged() override;
    virtual void ShellBuildPlay(const TSharedRef<SVerticalBox>& Into) override;
    virtual TArray<FText> ShellOutcomeDetail(const PerfectWorld::SchoolLessonOutcome& Outcome) const override;

private:
    PWClassCoordinate::FModel Model;
    PWClassCoordinate::FDraft Draft;
    /** Which exercise the draft belongs to (session and case), so a new exercise starts with a clean draft. */
    FString DraftOwner;
};
