#include "PerfectWorldAcceptanceHomework.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/Children.h"
#include "Misc/App.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneHomeFurniture.h"
#include "PerfectWorldLaneInteriors.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSchoolActors.h"
#include "PerfectWorldSchoolLive.h"
#include "PerfectWorldSchoolPresenter.h"
#include "UI/SPWAcademicAssessment.h"
#include "UI/SPWStudyDesk.h"
#include "Widgets/Text/STextBlock.h"

// P36U: follows PerfectWorldAcceptanceSchool.cpp: real movement/interaction/panels, conclusions from the portable owner.
namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FP36UHomeworkRun
{
    PerfectWorld::SchoolWorkBrief Brief;
    PerfectWorld::SchoolWorkState Issued;
    PerfectWorld::GameInstant BeganAt = 0;
    uint64 RunnerBefore = 0;
};

FStepDef P36UAction(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 30.0;
    Step.Run = MoveTemp(Run);
    return Step;
}

FVector P36UHomePoint(const double U, const double V)
{
    const auto Frame = PWLane::MakeLaneFrame(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
    return PWLane::HomeWorldPoint(Frame, U, V);
}

template <typename TActor>
TActor* P36UFindActor(UWorld* World)
{
    if (World != nullptr)
        for (TActorIterator<TActor> It(World); It; ++It) return *It;
    return nullptr;
}

APWDoorActor* P36UBedroomDoor(UWorld* World)
{
    if (World != nullptr)
        for (TActorIterator<APWDoorActor> It(World); It; ++It)
            if (It->GetDoorId() == FName(TEXT("door.nabapara.home_bedroom"))) return *It;
    return nullptr;
}

template <typename TPanel>
TSharedPtr<TPanel> P36UPanel(UWorld* World, const EPWSchoolPanel Kind)
{
    const auto* Presenter = UPWSchoolPresenter::Get(World);
    return Presenter != nullptr && Presenter->GetKind() == Kind && Presenter->GetPanel().IsValid()
        ? StaticCastSharedPtr<TPanel>(Presenter->GetPanel()) : nullptr;
}

bool P36UHasText(const TSharedRef<SWidget>& Widget, const FString& Wanted)
{
    if (Widget->GetType() == FName(TEXT("STextBlock")) && StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString() == Wanted) return true;
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        if (P36UHasText(Children->GetChildAt(Index), Wanted)) return true;
    return false;
}

bool P36UAdvanceTo(UPWLaneLifeSubsystem& Life, const PerfectWorld::GameInstant Target, FString& Why)
{
    // Use the bound sim: a detached gate-clock copy omits WorkBriefs and would bypass the issuance boundary.
    for (int32 Round = 0; Round < 128; ++Round)
    {
        if (Life.GetNow() > Target) { Why = TEXT("the clock is past the first issue boundary; use a fresh life"); return false; }
        const bool bDue = !PerfectWorld::IsWorldQuiescent(Life.GetWorldState());
        if (!bDue && Life.GetNow() == Target) return true;
        PerfectWorld::Command Command;
        Command.Source = bDue ? PerfectWorld::CommandSource::System : PerfectWorld::CommandSource::Player;
        Command.Payload = bDue ? PerfectWorld::CommandPayload(PerfectWorld::SettleDueEvents{})
            : PerfectWorld::CommandPayload(PerfectWorld::AdvanceTime{Target - Life.GetNow()});
        const auto Done = Life.SubmitSchoolCommand(Command);
        if (!Done.Accepted) { Why = PWLaneLife::ToFString(Done.Why.Code); return false; }
    }
    Why = TEXT("the live clock exhausted its bounded continuation budget");
    return false;
}

void P36UWalk(TArray<FStepDef>& Out, const TCHAR* Name, const double U, const double V)
{
    Out.Add(PWAcceptance::MakeWalkStep(Name, [U, V](UPWAcceptanceSubsystem&, FVector& Target, FString&)
        { Target = P36UHomePoint(U, V); return true; }, 10.0, 25.0));
}
} // namespace

namespace PWAcceptance
{
void P36UAppendHomeworkSteps(TArray<FStepDef>& Out)
{
    const auto Run = MakeShared<FP36UHomeworkRun>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(P36UAction(TEXT("P36U_cross_first_homework_boundary"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* World = Sub.GetPlayWorld();
        auto* Life = UPWLaneLifeSubsystem::Get(World);
        FString Why;
        const auto* Content = World != nullptr ? PWSchoolLive::SharedContent(*World, Why) : nullptr;
        if (Life == nullptr || Content == nullptr || Sub.GetPawn() == nullptr) return FStepOutcome::Fail(TEXT("no live school content: ") + Why);
        if (!Life->GetWorldState().Awakening.Seeded || Life->GetSim().SchoolContent().WorkBriefs != &Content->WorkBriefs)
            return FStepOutcome::Fail(TEXT("new-life learning or the live homework binding is absent"));
        if (FVector::Dist2D(Sub.GetPawn()->GetActorLocation(), P36UHomePoint(PWLane::HomeFrontDoorU + 35.0, 180.0)) > 220.0)
            return FStepOutcome::Fail(TEXT("start a new family life at the opening home"));
        const std::string Stream = TCHAR_TO_UTF8(*Content->Stream);
        for (const auto& Def : Content->WorkBriefs.Briefs)
            if (Def.Stream == Stream && (Run->Brief.Id.empty() || Def.Brief.AvailableAt < Run->Brief.AvailableAt)) Run->Brief = Def.Brief;
        if (Run->Brief.Id.empty() || Life->GetNow() >= Run->Brief.AvailableAt || !Life->GetWorldState().SchoolWork.Records.empty())
            return FStepOutcome::Fail(TEXT("the first brief is absent or its issue boundary has passed; use -PWAcceptanceStart=new"));
        if (!P36UAdvanceTo(*Life, Run->Brief.AvailableAt + 1, Why)) return FStepOutcome::Fail(Why);
        Run->Issued = Life->GetWorldState().SchoolWork;
        int32 Count = 0;
        for (const auto& Record : Run->Issued.Records)
            if (Record.Brief == Run->Brief && Record.IssuedAt == Run->Brief.AvailableAt) ++Count;
        return Count == 1 ? FStepOutcome::Pass(TEXT("the live commands issued the first brief once at its authored boundary"))
            : FStepOutcome::Fail(TEXT("the core did not issue exactly one matching brief at its boundary"));
    }));

    // The living doorway is open initially. Stay below the household adult, then approach the bedroom from outside its swing.
    P36UWalk(Out, TEXT("P36U_walk_entry"), 730.0, 300.0);
    P36UWalk(Out, TEXT("P36U_walk_living_approach"), 700.0, PWLane::HomeLivingDoorV);
    P36UWalk(Out, TEXT("P36U_walk_living_threshold"), PWLane::HomeLivingPartitionU, PWLane::HomeLivingDoorV);
    P36UWalk(Out, TEXT("P36U_walk_past_adult"), 560.0, 375.0);
    P36UWalk(Out, TEXT("P36U_walk_across_living"), 400.0, 370.0);
    P36UWalk(Out, TEXT("P36U_walk_bedroom_approach"), PWLane::HomeBedroomDoorU, 330.0);
    Out.Add(MakeInteractStep(TEXT("P36U_open_bedroom"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return P36UBedroomDoor(Sub.GetPlayWorld()); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Door = P36UBedroomDoor(Sub.GetPlayWorld());
            Detail = TEXT("the real bedroom door opened and settled");
            return Door != nullptr && Door->IsOpen() && !Door->IsMoving();
        },
        [](UPWAcceptanceSubsystem& Sub)
        {
            const auto* Door = P36UBedroomDoor(Sub.GetPlayWorld());
            return Door != nullptr && Door->IsOpen() && !Door->IsMoving();
        }, 15.0));
    P36UWalk(Out, TEXT("P36U_walk_to_bedroom_threshold"), PWLane::HomeBedroomDoorU, 400.0);
    P36UWalk(Out, TEXT("P36U_walk_through_bedroom"), PWLane::HomeBedroomDoorU, 550.0);
    P36UWalk(Out, TEXT("P36U_walk_beside_bed"), 200.0, 650.0);
    P36UWalk(Out, TEXT("P36U_walk_to_study_desk"), 295.0, 700.0);
    Out.Add(MakeInteractStep(TEXT("P36U_open_home_study_desk"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return P36UFindActor<APWStudyDeskActor>(Sub.GetPlayWorld()); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Desk = P36UFindActor<APWStudyDeskActor>(Sub.GetPlayWorld());
            Detail = TEXT("Study at the desk opened the real home panel");
            return Desk != nullptr && Desk->DidOpenPanel() && P36UPanel<SPWStudyDesk>(Sub.GetPlayWorld(), EPWSchoolPanel::Desk).IsValid();
        }, nullptr, 20.0));
    Out.Add(P36UAction(TEXT("P36U_desk_lists_issued_brief"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* World = Sub.GetPlayWorld();
        const auto* Life = UPWLaneLifeSubsystem::Get(World);
        const auto Desk = P36UPanel<SPWStudyDesk>(World, EPWSchoolPanel::Desk);
        FString Why;
        const auto* Content = World != nullptr ? PWSchoolLive::SharedContent(*World, Why) : nullptr;
        if (!Desk.IsValid() || Life == nullptr || Content == nullptr) return FStepOutcome::Fail(TEXT("no desk or content"));
        if (!(Life->GetWorldState().SchoolWork == Run->Issued)) return FStepOutcome::Fail(TEXT("opening the desk changed issued work"));
        if (Life->GetClockHold() != EPWClockHold::School) return FStepOutcome::Wait();
        if (!UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())) return FStepOutcome::Fail(TEXT("the desk did not hold world input"));
        bool bListed = false;
        for (const auto& View : Desk->P36UGetRecord().Work)
            if (View.Brief.Id == Run->Brief.Id) bListed = View.Available && !View.Overdue && !View.LateSubmission && View.CanBegin;
        if (!bListed) return FStepOutcome::Fail(TEXT("the desk did not list the available, on-time brief with Begin available"));
        for (const auto* Key : {&Run->Brief.TitleTextKey, &Run->Brief.BriefTextKey, &Run->Brief.CriteriaTextKey, &Run->Brief.RecoveryTextKey})
        {
            const auto Found = Content->WorkBriefs.Text.find(*Key);
            if (Found == Content->WorkBriefs.Text.end() || !P36UHasText(Desk.ToSharedRef(), PWLaneLife::ToFString(Found->second)))
                return FStepOutcome::Fail(TEXT("the desk did not render all four authored homework texts"));
        }
        return FStepOutcome::Pass(TEXT("issued row lists core state and all authored texts; reading issued no work"));
    }));
    Out.Add(MakeShotStep(TEXT("P36U_shot_homework_brief")));
    Out.Add(P36UAction(TEXT("P36U_begin_homework_from_desk"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* World = Sub.GetPlayWorld();
        const auto* Life = UPWLaneLifeSubsystem::Get(World);
        const auto Desk = P36UPanel<SPWStudyDesk>(World, EPWSchoolPanel::Desk);
        if (Life == nullptr || !Desk.IsValid() || !Desk->P36UOpenWork(Run->Brief.Id)) return FStepOutcome::Fail(TEXT("the issued desk row could not open"));
        const auto Paper = P36UPanel<SPWAcademicAssessment>(World, EPWSchoolPanel::Paper);
        if (!Paper.IsValid()) return FStepOutcome::Fail(TEXT("the paper panel did not take over"));
        Run->BeganAt = Life->GetNow();
        Run->RunnerBefore = Life->GetWorldState().DayLoop.Runner.Instance;
        PerfectWorld::SchoolWorkCommand Begin;
        Begin.Kind = PerfectWorld::SchoolWorkActionKind::Begin;
        const auto Result = Paper->Send(Begin);
        if (!Result.Ok() || Result.WorldMinutes != 0 || Life->GetNow() != Run->BeganAt)
            return FStepOutcome::Fail(TEXT("Begin refused or charged world time: ") + PWLaneLife::ToFString(Result.Issue.Code));
        for (const auto& Record : Life->GetWorldState().SchoolWork.Records)
            if (Record.Brief.Id == Run->Brief.Id && Record.StartedAt == Run->BeganAt && Record.Paper.Active())
                return FStepOutcome::Pass(TEXT("the desk row opened the paper; Begin reserved its block without moving time"));
        return FStepOutcome::Fail(TEXT("the core holds no active draft for the brief"));
    }));
    Out.Add(P36UAction(TEXT("P36U_answer_homework_group"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* World = Sub.GetPlayWorld();
        const auto* Life = UPWLaneLifeSubsystem::Get(World);
        const auto Paper = P36UPanel<SPWAcademicAssessment>(World, EPWSchoolPanel::Paper);
        if (Life == nullptr || !Paper.IsValid()) return FStepOutcome::Fail(TEXT("no paper panel"));
        const auto Bridge = PWSchoolLive::MakeStudyWorld(*World);
        PerfectWorld::SchoolWorkCommand Command;
        Command.Kind = PerfectWorld::SchoolWorkActionKind::Survey;
        if (!Paper->Send(Command).Ok()) return FStepOutcome::Fail(TEXT("survey refused"));
        PerfectWorld::AssessmentAnswer Answer;
        Answer.Group = 0;
        Answer.Mode = PerfectWorld::AssessmentMode::Work;
        const auto Quote = Bridge->Quote(Run->Brief.Id, Answer);
        Command.Kind = PerfectWorld::SchoolWorkActionKind::Allocate;
        Command.Group = 0;
        Command.Units = Quote.Units;
        if (!Paper->Send(Command).Ok() || !Bridge->Quote(Run->Brief.Id, Answer).Ok()) return FStepOutcome::Fail(TEXT("quoted allocation refused"));
        Command.Kind = PerfectWorld::SchoolWorkActionKind::Answer;
        Command.Answer = Answer;
        const auto Result = Paper->Send(Command);
        return Result.Ok() && Result.WorldMinutes == 0 && Life->GetNow() == Run->BeganAt
            ? FStepOutcome::Pass(TEXT("survey, quoted allocation and a real group answer kept the reserved world clock"))
            : FStepOutcome::Fail(TEXT("answer refused or charged world minutes"));
    }));
    Out.Add(P36UAction(TEXT("P36U_submit_and_read_core_marks"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* World = Sub.GetPlayWorld();
        const auto* Life = UPWLaneLifeSubsystem::Get(World);
        const auto Paper = P36UPanel<SPWAcademicAssessment>(World, EPWSchoolPanel::Paper);
        if (Life == nullptr || !Paper.IsValid()) return FStepOutcome::Fail(TEXT("no paper panel"));
        PerfectWorld::SchoolWorkCommand Submit;
        Submit.Kind = PerfectWorld::SchoolWorkActionKind::Submit;
        const auto Result = Paper->Send(Submit);
        if (!Result.Ok() || !Result.Marking || Result.WorldMinutes != 90 || Life->GetNow() != Run->BeganAt + 90)
            return FStepOutcome::Fail(TEXT("hand-in refused, lacked marks, or did not charge exactly 90 minutes: ") + PWLaneLife::ToFString(Result.Issue.Code));
        PerfectWorld::SchoolAcademicRecordView View;
        FText Why;
        if (!PWSchoolLive::MakeStudyWorld(*World)->ReadRecord(View, Why)) return FStepOutcome::Fail(Why.ToString());
        bool bMarked = false;
        for (const auto& Work : View.Work)
            if (Work.Brief.Id == Run->Brief.Id) bMarked = Work.Marking.has_value() && Work.Paper.Finished && !Work.CanBegin && !Work.CanSubmit;
        const auto& State = Life->GetWorldState();
        const auto SavedWork = State.SchoolWork;
        const auto SavedDay = State.DayLoop;
        const auto SavedSeq = State.Seq;
        if (!bMarked || SavedDay.Runner.Instance != Run->RunnerBefore + 1 || SavedDay.Runner.AppliedInstance != SavedDay.Runner.Instance)
            return FStepOutcome::Fail(TEXT("core marks or the single settled learning completion are absent"));
        const auto Again = Paper->Send(Submit);
        if (Again.Ok() || Again.Issue.Code != "school.work.already_submitted" || Again.WorldMinutes != 0 ||
            Life->GetNow() != Run->BeganAt + 90 || Life->GetWorldState().Seq != SavedSeq ||
            !(Life->GetWorldState().SchoolWork == SavedWork) || !(Life->GetWorldState().DayLoop == SavedDay))
            return FStepOutcome::Fail(TEXT("a repeated hand-in changed the record, day or clock"));
        return FStepOutcome::Pass(TEXT("core paper marked; one learning completion; 90 minutes charged once; repeat refused without mutation"));
    }));
    Out.Add(MakeShotStep(TEXT("P36U_shot_homework_marks")));
    Out.Add(P36UAction(TEXT("P36U_close_homework_panel"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Presenter = UPWSchoolPresenter::Get(Sub.GetPlayWorld());
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Presenter == nullptr || Life == nullptr) return FStepOutcome::Fail(TEXT("no school presenter"));
        if (Rt.Phase == 0)
        {
            const auto Paper = P36UPanel<SPWAcademicAssessment>(Sub.GetPlayWorld(), EPWSchoolPanel::Paper);
            if (!Paper.IsValid()) return FStepOutcome::Fail(TEXT("no marked paper to leave"));
            Paper->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 1)
        {
            const auto Desk = P36UPanel<SPWStudyDesk>(Sub.GetPlayWorld(), EPWSchoolPanel::Desk);
            if (!Desk.IsValid()) return FStepOutcome::Fail(TEXT("Back from the marked paper did not return to the desk"));
            Desk->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Gamepad_FaceButton_Right, FModifierKeysState(), 0, false, 0, 0));
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        const auto* Controller = Sub.GetController();
        if (Presenter->IsOpen() || UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn()) ||
            Controller == nullptr || Controller->bShowMouseCursor)
            return FStepOutcome::Fail(TEXT("panel or cursor still owns play input"));
        // P317: off-screen runs have no OS focus. Closing releases School; the independent focus pause remains valid.
        const bool bFocused = FApp::HasFocus();
        const auto ExpectedHold = bFocused ? EPWClockHold::None : EPWClockHold::Unfocused;
        Rt.Note = FString::Printf(TEXT("panel closed; focus %d; clock hold %d (expected %d)"),
            bFocused, static_cast<int32>(Life->GetClockHold()), static_cast<int32>(ExpectedHold));
        return Life->GetClockHold() == ExpectedHold
            ? FStepOutcome::Pass(bFocused ? TEXT("paper and desk Back restored play input and the free-roam clock")
                : TEXT("paper and desk Back restored play input; clock paused only because the application is unfocused"))
            : FStepOutcome::Wait();
    }));
}
} // namespace PWAcceptance
