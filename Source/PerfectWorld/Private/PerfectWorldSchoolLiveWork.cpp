#include "PerfectWorldSchoolLive.h"

#include "Engine/World.h"
#include "PerfectWorldLaneLifeSubsystem.h"

#define LOCTEXT_NAMESPACE "PerfectWorldSchoolLiveWork"

// S03: the study-desk bridge and the week-planner bridge. Reads are portable views over the live world; mutations are refused (unrouted).

namespace PWSchoolLive
{
namespace
{
const UPWLaneLifeSubsystem* S03WorkLife(const TWeakObjectPtr<UWorld>& World)
{
    UWorld* Live = World.Get();
    return Live != nullptr ? UPWLaneLifeSubsystem::Get(Live) : nullptr;
}

PerfectWorld::SchoolIssue S03WorkRefusal()
{
    return {std::string(TCHAR_TO_UTF8(UnwiredCode)), "school.work.apply / school.week.* are not routed through the reducer"};
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

    virtual PerfectWorld::SchoolWorkResult Apply(const PerfectWorld::SchoolWorkCommand&) override
    {
        PerfectWorld::SchoolWorkResult Result;
        Result.Issue = S03WorkRefusal(); // S03Port: school.work.apply through the reducer, one transaction
        return Result;
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

    virtual PerfectWorld::SchoolIssue StartPractice(std::string_view) override { return S03WorkRefusal(); }

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
        PerfectWorld::WeekPlan Base;
        Base.Days.resize(7);
        const std::string Stream = TCHAR_TO_UTF8(*Content->Stream);
        OutPreview.Build = PerfectWorld::BuildSchoolWeekPlan(State, OutPreview.Standing, OutPreview.Sessions, Base,
            PerfectWorld::SchoolLessonBindings(Content->Lessons, Stream), Content->Cats);
        return true; // Known stays empty: no source of known promises is routed yet (A6 handoff, risk 4)
    }

    virtual FPWPlannerOutcome SetOrder(const PerfectWorld::SchoolStandingOrder&, bool) override
    {
        FPWPlannerOutcome Outcome; // S03Port: school.week.order
        Outcome.Code = UnwiredCode;
        return Outcome;
    }

    virtual FPWPlannerResolution Resolve() override
    {
        FPWPlannerResolution Resolution; // S03Port: school.week.resolve, and publish the SchoolWeekResult for the recap
        Resolution.Outcome.Code = UnwiredCode;
        return Resolution;
    }

private:
    TWeakObjectPtr<UWorld> World;
};
} // namespace

TSharedRef<PWA5::IStudyWorld> MakeStudyWorld(UWorld& World)
{
    return MakeShared<FS03StudyWorld>(World);
}

TSharedRef<IPWSchoolPlannerWorld> MakePlannerWorld(UWorld& World)
{
    // The planner words a refusal through phase2.a6.refusal.<code>; serve ours.
    PWSchoolPlanner::SetTextProvider([](const FString& Key, FText& Out)
    {
        if (Key == FString(TEXT("phase2.a6.refusal.")) + UnwiredCode)
        {
            Out = UnwiredText();
            return true;
        }
        return false;
    });
    return MakeShared<FS03PlannerWorld>(World);
}
} // namespace PWSchoolLive

#undef LOCTEXT_NAMESPACE
