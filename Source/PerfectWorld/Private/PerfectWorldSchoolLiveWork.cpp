#include "PerfectWorldSchoolLive.h"

#include "Engine/World.h"
#include "PerfectWorldLaneLifeSubsystem.h"

#define LOCTEXT_NAMESPACE "PerfectWorldSchoolLiveWork"

// S03/S05: the study-desk bridge and the week-planner bridge. Reads are portable views over the live world; mutations are the real reducer commands (S05).

namespace PWSchoolLive
{
namespace
{
const UPWLaneLifeSubsystem* S03WorkLife(const TWeakObjectPtr<UWorld>& World)
{
    UWorld* Live = World.Get();
    return Live != nullptr ? UPWLaneLifeSubsystem::Get(Live) : nullptr;
}

UPWLaneLifeSubsystem* S05LiveLife(const TWeakObjectPtr<UWorld>& World)
{
    UWorld* Live = World.Get();
    return Live != nullptr ? UPWLaneLifeSubsystem::Get(Live) : nullptr;
}

PerfectWorld::SchoolIssue S05StartPracticeRefusal()
{
    // No practice offers exist on the desk yet (PracticeOffers is empty), so a start can only be a stale click.
    return {"school.practice.unavailable", "no practice offer is routed on the live desk"};
}

/** A5's desk bridge. Records come from QuerySchoolAcademicRecord; Apply and practice are refused until routed. */
class FS03StudyWorld final : public PWA5::IStudyWorld
{
public:
    explicit FS03StudyWorld(UWorld& InWorld) : World(&InWorld) {}

    virtual bool CanUseDesk(FText& OutReason) const override
    {
        if (S03WorkLife(World) == nullptr)
        {
            OutReason = LOCTEXT("S03NoWorld", "There is no world to study in yet.");
            return false;
        }
        return true; // the desk actor is the home's own; reachability is its interaction reach
    }

    virtual bool ReadRecord(PerfectWorld::SchoolAcademicRecordView& Out, FText& OutReason) const override
    {
        UWorld* Live = World.Get();
        const UPWLaneLifeSubsystem* Life = S03WorkLife(World);
        FString Why;
        const FS03Content* Content = Live != nullptr ? SharedContent(*Live, Why) : nullptr;
        if (Life == nullptr || Content == nullptr)
        {
            OutReason = FText::FromString(Why.IsEmpty() ? TEXT("There is no world to read.") : Why);
            return false;
        }
        const PerfectWorld::WorldState& State = Life->GetWorldState();
        Out = PerfectWorld::QuerySchoolAcademicRecord(WorkOf(State), Content->Papers, State.Clock.Now());
        return true;
    }

    virtual PerfectWorld::SchoolWorkResult Apply(const PerfectWorld::SchoolWorkCommand& Command) override
    {
        PerfectWorld::SchoolWorkResult Result;
        UPWLaneLifeSubsystem* Life = S05LiveLife(World);
        if (Life == nullptr)
        {
            Result.Issue = {"school.work.unavailable", "no world"};
            return Result;
        }
        // school.work.apply: the reducer reserves the paper block at Begin and settles the time once at Submit or Cancel.
        const PerfectWorld::CommandResult Done = Life->SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, Command});
        if (!Done.Accepted)
        {
            Result.Issue = {Done.Why.Code, Done.Why.Message};
            return Result;
        }
        const std::optional<PerfectWorld::SchoolWorkResult>& Last = Life->GetSim().LastSchoolWorkResult();
        if (!Last.has_value())
        {
            Result.Issue = {"school.work.unavailable", "the reducer accepted the command but published no result"};
            return Result;
        }
        return *Last;
    }

    virtual PerfectWorld::AssessmentQuote Quote(std::string_view WorkId, const PerfectWorld::AssessmentAnswer& Answer) const override
    {
        const UPWLaneLifeSubsystem* Life = S03WorkLife(World);
        if (Life == nullptr)
        {
            return PerfectWorld::AssessmentQuote{};
        }
        return PerfectWorld::QuoteSchoolWorkAnswer(WorkOf(Life->GetWorldState()), WorkId, Answer);
    }

    virtual FText Text(std::string_view Key, const FText& Fallback) const override
    {
        UWorld* Live = World.Get();
        FString Why;
        const FS03Content* Content = Live != nullptr ? SharedContent(*Live, Why) : nullptr;
        if (Content != nullptr)
        {
            if (const std::string* Found = PerfectWorld::FindSchoolLessonText(Content->Lessons, Key))
            {
                return PWLaneLife::ToText(*Found);
            }
        }
        return Fallback;
    }

    /** Read-only path: no case context is authored for the live desk yet, so the public prompt is empty (the panel shows the group's own name). */
    virtual FText GroupPrompt(std::string_view, std::size_t) const override { return FText::GetEmpty(); }

    virtual TArray<PWA5::FPracticeOffer> PracticeOffers() const override { return {}; }

    virtual PerfectWorld::SchoolIssue StartPractice(std::string_view) override { return S05StartPracticeRefusal(); }

private:
    TWeakObjectPtr<UWorld> World;
};

/** A6's planner bridge. The preview is a real BuildSchoolWeekPlan over the live world and an empty standing plan (nothing can be saved yet). */
class FS03PlannerWorld final : public IPWSchoolPlannerWorld
{
public:
    explicit FS03PlannerWorld(UWorld& InWorld) : World(&InWorld) {}

    virtual bool ReadPreview(FPWPlannerPreview& OutPreview, FString& OutWhy) override
    {
        UWorld* Live = World.Get();
        const UPWLaneLifeSubsystem* Life = S03WorkLife(World);
        if (Live == nullptr || Life == nullptr)
        {
            OutWhy = TEXT("there is no lane-life world");
            return false;
        }
        const FS03Content* Content = SharedContent(*Live, OutWhy);
        if (Content == nullptr)
        {
            return false;
        }
        const PerfectWorld::WorldState& State = Life->GetWorldState();
        OutPreview = FPWPlannerPreview();
        OutPreview.Standing = StandingOf(State);
        OutPreview.Now = State.Clock.Now();
        if (Content->Cats.Day.Activities != nullptr)
        {
            OutPreview.Activities = *Content->Cats.Day.Activities;
        }
        OutPreview.Sessions = SessionsOf(State);
        const PerfectWorld::WeekPlan Base = MakeBase();
        const std::string Stream = TCHAR_TO_UTF8(*Content->Stream);
        OutPreview.Build = PerfectWorld::BuildSchoolWeekPlan(State, OutPreview.Standing, OutPreview.Sessions, Base,
            PerfectWorld::SchoolLessonBindings(Content->Lessons, Stream), Content->Cats);
        return true; // Known stays empty: no source of known promises is routed yet (A6 handoff, risk 4)
    }

    virtual FPWPlannerOutcome SetOrder(const PerfectWorld::SchoolStandingOrder& Order, bool bRemove) override
    {
        FPWPlannerOutcome Outcome;
        UPWLaneLifeSubsystem* Life = S05LiveLife(World);
        if (Life == nullptr)
        {
            Outcome.Code = TEXT("school.week.unavailable");
            return Outcome;
        }
        // school.week.order moves no clock; the core's code goes back unchanged and the planner words it.
        const PerfectWorld::CommandResult Done = Life->SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::SchoolWeekOrder{Order, bRemove}});
        Outcome.bAccepted = Done.Accepted;
        Outcome.Code = Done.Accepted ? FString() : PWLaneLife::ToFString(Done.Why.Code);
        return Outcome;
    }

    virtual FPWPlannerResolution Resolve() override
    {
        FPWPlannerResolution Resolution;
        UPWLaneLifeSubsystem* Life = S05LiveLife(World);
        FPWPlannerPreview Preview;
        FString Why;
        if (Life == nullptr || !ReadPreview(Preview, Why))
        {
            Resolution.Outcome.Code = TEXT("school.week.unavailable");
            return Resolution;
        }
        // The reducer rebuilds the plan from State.SchoolWeek and this base at the same instant, so Preview.Build.Plan is what runs.
        const PerfectWorld::CommandResult Done = Life->SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::SchoolWeekResolve{MakeBase()}});
        Resolution.Outcome.bAccepted = Done.Accepted;
        Resolution.Outcome.Code = Done.Accepted ? FString() : PWLaneLife::ToFString(Done.Why.Code);
        Resolution.Plan = Preview.Build.Plan;
        if (Done.Accepted)
        {
            const std::optional<PerfectWorld::SchoolWeekResult>& Last = Life->GetSim().LastSchoolWeekResult();
            if (Last.has_value())
            {
                Resolution.Result = *Last;
            }
        }
        return Resolution;
    }

private:
    static PerfectWorld::WeekPlan MakeBase()
    {
        PerfectWorld::WeekPlan Base;
        Base.Days.resize(7);
        return Base;
    }

    TWeakObjectPtr<UWorld> World;
};
} // namespace

TSharedRef<PWA5::IStudyWorld> MakeStudyWorld(UWorld& World)
{
    return MakeShared<FS03StudyWorld>(World);
}

TSharedRef<IPWSchoolPlannerWorld> MakePlannerWorld(UWorld& World)
{
    return MakeShared<FS03PlannerWorld>(World);
}
} // namespace PWSchoolLive

#undef LOCTEXT_NAMESPACE
