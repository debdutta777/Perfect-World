#include "PerfectWorldSchoolLive.h"

#include "Engine/World.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldSchoolOffers.h" // P35

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

// P35: practice is the one routed activity the core permits at the desk, activity.study_at_home, run through day.apply.
PerfectWorld::SchoolIssue P35PracticeIssue(const char* Code, const std::string& Detail)
{
    return {Code, Detail};
}

/** Undo a commit or start that a later step could not finish, so a refused practice leaves no half-run activity. Best effort; the core guards it. */
void P35Abandon(UPWLaneLifeSubsystem& Life)
{
    PerfectWorld::DayCommand Cancel;
    Cancel.Kind = PerfectWorld::DayCommandKind::Cancel;
    Cancel.Minutes = 0;
    (void)Life.SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::WorldDayCommand{Cancel}});
}

/** A routed practice: (plan an empty day if none yet, free, as the gate does), commit with the known commitments it passes over, start, complete. */
PerfectWorld::SchoolIssue P35RunPractice(UPWLaneLifeSubsystem& Life, const PerfectWorld::ActivityCatalogue& Activities, std::string_view OfferId)
{
    if (OfferId != P35StudyActivityId)
    {
        return P35PracticeIssue("school.practice.unavailable", "no such practice offer");
    }
    const PerfectWorld::WorldState& State = Life.GetWorldState();
    const FP35PracticeStatus Status = P35ReadPractice(State, &Activities);
    if (!Status.CanStart())
    {
        return P35PracticeIssue("school.practice.unavailable", "practice cannot start now");
    }
    const auto Fail = [](const PerfectWorld::CommandResult& Done) { return P35PracticeIssue("school.practice.refused", Done.Why.Code); };
    if (!State.DayLoop.Planned)
    {
        const PerfectWorld::CommandResult Planned = Life.SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::SchoolPlanDay{}});
        if (!Planned.Accepted)
        {
            return Fail(Planned);
        }
    }
    PerfectWorld::DayCommand Step;
    Step.Kind = PerfectWorld::DayCommandKind::Commit;
    Step.ActivityId = std::string(P35StudyActivityId);
    Step.PlaceKind = std::string(P35StudyOfferPlace);
    Step.Alternatives = P35PracticeAlternatives(Life.GetWorldState(), Activities, *Status.Def);
    PerfectWorld::CommandResult Done = Life.SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::WorldDayCommand{Step}});
    if (!Done.Accepted)
    {
        return Fail(Done);
    }
    Step = PerfectWorld::DayCommand{};
    Step.Kind = PerfectWorld::DayCommandKind::Start;
    Done = Life.SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::WorldDayCommand{Step}});
    if (!Done.Accepted)
    {
        P35Abandon(Life);
        return Fail(Done);
    }
    Step = PerfectWorld::DayCommand{};
    Step.Kind = PerfectWorld::DayCommandKind::Complete;
    Done = Life.SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::WorldDayCommand{Step}});
    if (!Done.Accepted)
    {
        P35Abandon(Life);
        return Fail(Done);
    }
    return {};
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

    /** P35: the public context of the selected group from the core's own catalogue (the question's name, and a model case's brief); empty, so hidden, when unknown. Never Trap or marking. */
    virtual FText GroupPrompt(std::string_view WorkId, std::size_t Group) const override
    {
        UWorld* Live = World.Get();
        const UPWLaneLifeSubsystem* Life = S03WorkLife(World);
        FString Why;
        const FS03Content* Content = Live != nullptr ? SharedContent(*Live, Why) : nullptr;
        if (Life == nullptr || Content == nullptr)
        {
            return FText::GetEmpty();
        }
        for (const PerfectWorld::SchoolWorkRecord& Record : WorkOf(Life->GetWorldState()).Records)
        {
            if (Record.Brief.Id == WorkId)
            {
                return P35GroupContext(Content->Papers, Content->Model, Record.Brief.PaperId, Group);
            }
        }
        return FText::GetEmpty();
    }

    /** P35: one offer (study at home) with its real cost and what it passes over, blocked with the reason when it cannot start; none when the catalogue lacks it. */
    virtual TArray<PWA5::FPracticeOffer> PracticeOffers() const override
    {
        UWorld* Live = World.Get();
        const UPWLaneLifeSubsystem* Life = S03WorkLife(World);
        FString Why;
        const FS03Content* Content = Live != nullptr ? SharedContent(*Live, Why) : nullptr;
        if (Life == nullptr || Content == nullptr)
        {
            return {};
        }
        return P35PracticeOffers(Life->GetWorldState(), Content->Cats.Day.Activities);
    }

    virtual PerfectWorld::SchoolIssue StartPractice(std::string_view OfferId) override
    {
        UWorld* Live = World.Get();
        UPWLaneLifeSubsystem* Life = S05LiveLife(World);
        FString Why;
        const FS03Content* Content = Live != nullptr ? SharedContent(*Live, Why) : nullptr;
        if (Life == nullptr || Content == nullptr || Content->Cats.Day.Activities == nullptr)
        {
            return P35PracticeIssue("school.practice.unavailable", "no world to practise in");
        }
        return P35RunPractice(*Life, *Content->Cats.Day.Activities, OfferId);
    }

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
        // P35: Known is what the world really records with a window (open morning-plan items, the committed or running activity); see PerfectWorldSchoolOffers.h.
        if (Content->Cats.Day.Activities != nullptr)
        {
            FP35Known Known = P35KnownCommitments(State, *Content->Cats.Day.Activities);
            OutPreview.Known = std::move(Known.Candidates);
            OutPreview.KnownNames = std::move(Known.Lines);
        }
        return true;
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
