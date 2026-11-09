#include "PerfectWorldSchoolOffers.h"

#include "Attendance.h"
#include "GameClock.h"
#include "QuestLog.h"
#include "UI/SPWSchoolPlanner.h"

#define LOCTEXT_NAMESPACE "PerfectWorldSchoolOffers"

// P3-05. See the header: only commitments the world really records with a window; no invented promise, duration or cost.

namespace PWSchoolLive
{
namespace
{
FText P35Utf8(const std::string& Text)
{
    return FText::FromString(FString(UTF8_TO_TCHAR(Text.c_str())));
}

FString P35Key(const std::string& Id)
{
    return FString(UTF8_TO_TCHAR(Id.c_str()));
}

/** "worth about 2.4 learning, would use about 8 points of energy and hunger"; empty when the forecast has neither. */
FText P35WorthAndCost(const PerfectWorld::ReceiptForecast& Forecast)
{
    TArray<FText> Pieces;
    if (Forecast.LearningMilliXp > 0)
    {
        Pieces.Add(FText::Format(LOCTEXT("P35Worth", "worth about {0} learning"), FText::AsNumber(static_cast<double>(Forecast.LearningMilliXp) / 1000.0)));
    }
    if (Forecast.ConditionCostMilli > 0)
    {
        const int32 Points = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Forecast.ConditionCostMilli) / 1000.0f));
        Pieces.Add(FText::Format(LOCTEXT("P35Cost", "would use about {0} points of energy and hunger"), FText::AsNumber(Points)));
    }
    return Pieces.IsEmpty() ? FText::GetEmpty() : FText::Join(FText::FromString(TEXT(", ")), Pieces);
}

FText P35Line(const FText& Name, const FText& What, const FText& WorthAndCost)
{
    if (WorthAndCost.IsEmpty())
    {
        return FText::Format(LOCTEXT("P35LinePlain", "{0} ({1})"), Name, What);
    }
    return FText::Format(LOCTEXT("P35LineCost", "{0} ({1}; {2})"), Name, What, WorthAndCost);
}

void P35AddCandidate(FP35Known& Out, const PerfectWorld::ActivityDef& Def, const PerfectWorld::DayLoopState& Day, const PerfectWorld::ActivityCatalogue& Activities,
    const std::string& Id, const PerfectWorld::GameInstant Start, const PerfectWorld::GameInstant KnownAt, const PerfectWorld::GameInstant Now, const FText& What)
{
    // The core's own forecast at the later of the window's start and the day's clock. A window the core would refuse to commit is not a promise that could have been kept.
    const PerfectWorld::DayActivityForecast Forecast = PerfectWorld::ForecastDayActivity(Day, Activities, Def.Id, FMath::Max(Start, Day.Now));
    if (!Forecast.Ok)
    {
        return;
    }
    PerfectWorld::ReceiptCandidate Candidate;
    Candidate.Id = Id;
    Candidate.Source = "source.day_plan";
    Candidate.KnownAt = std::min(KnownAt, Now);
    Candidate.Start = Start;
    Candidate.DurationMinutes = Def.DurationMinutes;
    Candidate.Available = true;
    Candidate.Forecast = Forecast.Receipt;
    if (!PerfectWorld::IsReceiptCandidateValid(Candidate))
    {
        return;
    }
    Out.Lines.Add(P35Key(Id), P35Line(P35Utf8(Def.Name), What, P35WorthAndCost(Forecast.Receipt)));
    Out.Candidates.push_back(std::move(Candidate));
}

FText P35TierLabel(const PerfectWorld::PracticeLabel Label)
{
    switch (Label)
    {
    case PerfectWorld::PracticeLabel::Consolidation:
        return LOCTEXT("P35TierConsolidation", "Consolidation");
    case PerfectWorld::PracticeLabel::LittleNewLearning:
        return LOCTEXT("P35TierLittle", "Little new learning");
    default:
        return LOCTEXT("P35TierUseful", "Useful practice");
    }
}

/** The known commitments a practice of `Def` starting now would pass over, as ids and as readable lines. */
void P35Displaced(const PerfectWorld::WorldState& State, const PerfectWorld::ActivityDef& Def, const FP35Known& Known,
    const PerfectWorld::ReceiptForecast& Forecast, std::vector<PerfectWorld::ReceiptCandidate>& OutCandidates, TArray<FText>& OutLines)
{
    const PerfectWorld::GameInstant Now = State.Clock.Now();
    PerfectWorld::ReceiptCommitment Commitment;
    Commitment.ActivityId = Def.Id;
    Commitment.DecidedAt = Now;
    Commitment.Start = Now;
    Commitment.DurationMinutes = Def.DurationMinutes;
    Commitment.Forecast = Forecast;
    for (const PerfectWorld::ReceiptDisplaced& Passed : PerfectWorld::DecideDisplaced(Commitment, Known.Candidates))
    {
        for (const PerfectWorld::ReceiptCandidate& Candidate : Known.Candidates)
        {
            if (Candidate.Id != Passed.Id)
            {
                continue;
            }
            if (OutCandidates.size() < PerfectWorld::DayLoopMaxAlternatives)
            {
                OutCandidates.push_back(Candidate);
            }
            if (const FText* Line = Known.Lines.Find(P35Key(Candidate.Id)))
            {
                OutLines.Add(*Line);
            }
            break;
        }
    }
}
} // namespace

FP35Known P35KnownCommitments(const PerfectWorld::WorldState& State, const PerfectWorld::ActivityCatalogue& Activities)
{
    using namespace PerfectWorld;
    FP35Known Out;
    const GameInstant Now = State.Clock.Now();
    const DayLoopState& Day = State.DayLoop;

    // The activity the player has committed to (not yet started, or running). A lesson in progress is the school's own and is not an alternative.
    bool bCommitmentCoversPlan = false;
    std::string CommittedActivity;
    if (Day.HasCommitment && Day.Commitment.ActivityId != AttendanceClassActivityId)
    {
        if (const ActivityDef* Def = Activities.Find(Day.Commitment.ActivityId))
        {
            const bool bRunning = Day.Runner.Active() && Day.Runner.ActivityId == Def->Id;
            const GameInstant Start = bRunning ? Day.Runner.StartedAt : Day.Now;
            if (Start + Def->DurationMinutes > Now)
            {
                const FText What = bRunning
                    ? FText::Format(LOCTEXT("P35Running", "under way since {0}"), PWSchoolPlanner::ClockText(static_cast<int32>(Start - GameClock::StartOfDay(Start))))
                    : LOCTEXT("P35Committed", "you have committed to it");
                P35AddCandidate(Out, *Def, Day, Activities, std::string("day_commitment.") + Def->Id, Start, Day.Commitment.DecidedAt, Now, What);
                bCommitmentCoversPlan = true;
                CommittedActivity = Def->Id;
            }
        }
    }

    // The morning plan's items that are not done yet. The core's quest log already matches plan items to receipts, so this repeats none of that logic.
    const QuestLogView Log = MakeQuestLog(State, Now);
    for (const QuestItem& Item : Log.Today)
    {
        if (Item.ActivityId.empty() || Item.Id == QuestIdCommitment || Item.Status == QuestStatus::Done || Item.DueAt < 0 || Item.ActivityId == AttendanceClassActivityId)
        {
            continue;
        }
        const ActivityDef* Def = Activities.Find(Item.ActivityId);
        if (Def == nullptr || Item.DueAt + Def->DurationMinutes <= Now)
        {
            continue;
        }
        if (bCommitmentCoversPlan && Item.ActivityId == CommittedActivity)
        {
            // The first open plan item of the activity is the one the commitment is carrying out.
            bCommitmentCoversPlan = false;
            continue;
        }
        const GameInstant PlannedAt = Day.HasLastAction && Day.Last.Kind == DayCommandKind::PlanDay ? Day.Last.At : Now;
        const FText What = FText::Format(LOCTEXT("P35Planned", "your plan today at {0}"), PWSchoolPlanner::ClockText(static_cast<int32>(Item.DueAt - GameClock::StartOfDay(Item.DueAt))));
        P35AddCandidate(Out, *Def, Day, Activities, Item.Id, Item.DueAt, PlannedAt, Now, What);
    }
    return Out;
}

FP35PracticeStatus P35ReadPractice(const PerfectWorld::WorldState& State, const PerfectWorld::ActivityCatalogue* Activities)
{
    FP35PracticeStatus Status;
    if (Activities == nullptr)
    {
        return Status;
    }
    Status.Def = Activities->Find(P35StudyActivityId);
    if (Status.Def == nullptr)
    {
        return Status;
    }
    const PerfectWorld::DayLoopState& Day = State.DayLoop;
    bool bPaperOpen = false;
    for (const PerfectWorld::SchoolWorkRecord& Record : State.SchoolWork.Records)
    {
        bPaperOpen = bPaperOpen || (Record.StartedAt && !Record.SubmittedAt);
    }
    if (Day.Asleep)
    {
        Status.BlockedReason = LOCTEXT("P35Asleep", "You are asleep. Practice is for when you are up.");
    }
    else if (Day.HasCommitment || Day.Runner.Active())
    {
        Status.BlockedReason = LOCTEXT("P35Busy", "You are already in the middle of something. Finish it first.");
    }
    else if (State.SchoolSessions.Active)
    {
        Status.BlockedReason = LOCTEXT("P35LessonOpen", "A lesson is open. Finish it first.");
    }
    else if (bPaperOpen)
    {
        Status.BlockedReason = LOCTEXT("P35PaperOpen", "A paper is open. Hand it in or set it aside first.");
    }
    else if (!PerfectWorld::OpenConversationPersonId(State).empty())
    {
        Status.BlockedReason = LOCTEXT("P35Talking", "Finish the conversation first.");
    }
    Status.Forecast = PerfectWorld::ForecastDayActivity(Day, *Activities, P35StudyActivityId, Day.Now);
    if (Status.BlockedReason.IsEmpty() && !Status.Forecast.Ok)
    {
        Status.BlockedReason = LOCTEXT("P35NotNow", "Practice cannot be recorded right now. Try again later.");
    }
    return Status;
}

TArray<PWA5::FPracticeOffer> P35PracticeOffers(const PerfectWorld::WorldState& State, const PerfectWorld::ActivityCatalogue* Activities)
{
    TArray<PWA5::FPracticeOffer> Offers;
    const FP35PracticeStatus Status = P35ReadPractice(State, Activities);
    if (Status.Def == nullptr)
    {
        return Offers;
    }
    FText Costs;
    if (Status.Forecast.Ok)
    {
        TArray<FText> Parts;
        Parts.Add(P35TierLabel(Status.Forecast.Practice.Label));
        const FText WorthAndCost = P35WorthAndCost(Status.Forecast.Receipt);
        if (!WorthAndCost.IsEmpty())
        {
            Parts.Add(WorthAndCost);
        }
        Parts.Add(FText::Format(LOCTEXT("P35Minutes", "{0} minutes pass"), FText::AsNumber(Status.Def->DurationMinutes)));
        if (Status.Forecast.Practice.BudgetLimited)
        {
            Parts.Add(LOCTEXT("P35Limited", "today's learning limit trims it"));
        }
        Costs = FText::Join(FText::FromString(TEXT("; ")), Parts);
        std::vector<PerfectWorld::ReceiptCandidate> Passed;
        TArray<FText> PassedLines;
        P35Displaced(State, *Status.Def, P35KnownCommitments(State, *Activities), Status.Forecast.Receipt, Passed, PassedLines);
        if (PassedLines.IsEmpty())
        {
            Costs = FText::Format(LOCTEXT("P35CostsOnly", "{0}."), Costs);
        }
        else
        {
            Costs = FText::Format(LOCTEXT("P35CostsPasses", "{0}. It passes over: {1}."), Costs, FText::Join(FText::FromString(TEXT("; ")), PassedLines));
        }
    }
    FText Quote = Costs;
    if (!Status.CanStart())
    {
        Quote = Costs.IsEmpty() ? Status.BlockedReason : FText::Format(LOCTEXT("P35BlockedCosts", "{0} {1}"), Status.BlockedReason, Costs);
    }
    Offers.Add(PWA5::StudyOffer(*Status.Def, Quote, Status.CanStart()));
    return Offers;
}

std::vector<PerfectWorld::ReceiptCandidate> P35PracticeAlternatives(const PerfectWorld::WorldState& State,
    const PerfectWorld::ActivityCatalogue& Activities, const PerfectWorld::ActivityDef& Def)
{
    std::vector<PerfectWorld::ReceiptCandidate> Passed;
    TArray<FText> Unused;
    const PerfectWorld::DayActivityForecast Forecast = PerfectWorld::ForecastDayActivity(State.DayLoop, Activities, Def.Id, State.DayLoop.Now);
    P35Displaced(State, Def, P35KnownCommitments(State, Activities), Forecast.Receipt, Passed, Unused);
    return Passed;
}

FText P35GroupContext(const PerfectWorld::AssessmentCatalogue& Papers, const PerfectWorld::ModelCatalogue& Models, const std::string_view PaperId, const std::size_t Group)
{
    const PerfectWorld::AssessmentPaperDef* Paper = Papers.FindPaper(PaperId);
    if (Paper == nullptr || Group >= Paper->Questions.size())
    {
        return FText::GetEmpty();
    }
    const PerfectWorld::AssessmentQuestion& Question = Paper->Questions[Group];
    if (Question.Name.empty())
    {
        return FText::GetEmpty();
    }
    // Only authored public words: the group's name, and the model case's brief. Trap, marking and the other structures' answers stay in the core.
    if (Question.Structure == PerfectWorld::AssessmentStructure::Model)
    {
        if (const PerfectWorld::ModelCaseDef* Case = Models.FindCase(Question.CaseId))
        {
            if (!Case->Brief.empty())
            {
                return FText::Format(LOCTEXT("P35GroupBrief", "{0}: {1}"), P35Utf8(Question.Name), P35Utf8(Case->Brief));
            }
        }
    }
    return P35Utf8(Question.Name);
}
} // namespace PWSchoolLive

#undef LOCTEXT_NAMESPACE
