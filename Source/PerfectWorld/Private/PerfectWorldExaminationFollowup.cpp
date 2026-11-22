#include "PerfectWorldExaminationBridge.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldSchoolCampus.h"

bool P406Exam::P407UAtSchool(UWorld* World)
{
    const auto* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
    const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
    if (Pawn == nullptr) return false;
    const auto Campus = PWSchool::ScReadCampus();
    const auto Position = Pawn->GetActorLocation();
    return Campus.bValid && Campus.Site.ContainsXY(Position.X, Position.Y) &&
        FMath::Abs(Position.Z - PWSchool::ScGroundZ) < 300.0;
}

void P406Exam::P407UAppendFollowups(FModel& Model, const PerfectWorld::WorldSim& Sim, bool bAtSchool)
{
    using namespace PerfectWorld;
    Model.P407UFollowups = Sim.P407UFollowups();
    Model.Offers.Add(P407UFollowupText("phase2.a8.continuity"));
    Model.Lines.Add(FText::FromString(TEXT("Class XI, school, rest and home or care-home routines continue. Every appointment is optional. Reading and booking cost no time. For tasks, arrival and starting are free; finishing spends the displayed minutes once. Attendance-only meetings charge their displayed duration on Attend. You may refuse every offer.")));
    Model.Lines.Add(FText::FromString(TEXT("Close to walk, then reopen Examination from pause. If you miss a session, book the next free slot shown here without a penalty. Cancel releases the reservation; leaving a task records no consequence. An expired task can be rebooked.")));
    const auto add = [&](const P407UFollowupView& Row, const FString& Label, P407UFollowupAction Kind,
        bool Enabled, bool Wait = false, const std::string& Choice = {})
    {
        FAction Action;
        Action.Label = FText::FromString(Label);
        Action.bEnabled = Enabled;
        Action.bP407UFollowup = true;
        Action.bP407UWait = Wait;
        Action.bP506ReturnToWorld = Kind == P407UFollowupAction::P502Leave;
        Action.P407UCommand = {Kind, Row.Action.Id, Row.At, Choice, Row.P502Revision};
        Model.Actions.Add(MoveTemp(Action));
    };
    // Show the consequential task before optional attendance-only enquiries.
    for (const bool TaskPass : {true, false})
    for (const auto& Row : Model.P407UFollowups)
    {
        const bool Task = !Row.Action.P502TaskId.empty();
        if (Task != TaskPass) continue;
        const auto Title = P407UFollowupText(Row.Action.TitleTextKey).ToString();
        const bool Workshop = Row.Action.Destination == "location.nabapara.repair_workshop";
        const bool SchoolSession = Row.Action.Destination == "place.school" || (Task && Workshop);
        if (Task && Workshop)
        {
            // Keep authored destination IDs stable. This order uses the explicitly permitted school host.
            Model.Lines.Add(FText::FromString(TEXT("Bench repair with Debashis Pal: this session is hosted at Nabapara Vidyalaya using the repair workshop's task. The workshop remains the reference institution; no workshop visit or travel is recorded.")));
            Model.Lines.Add(FText::FromString(TEXT("Finish one repair step to record the step and workshop reference. This session grants no object, skill award, wage, placement or licence.")));
        }
        else Model.Lines.Add(P407UFollowupText(Row.Action.BriefTextKey));
        if (Row.Action.P502TaskId == "volunteer_enquiry")
            Model.Lines.Add(FText::FromString(TEXT("Clinic enquiry hosted at the Nabapara Vidyalaya notice desk. This is a request and proposed calendar note, not a clinic visit or a standing volunteer commitment.")));
        const auto Time = GameClock::ToCalendar(Row.At);
        const auto Place = SchoolSession ? TEXT("Nabapara Vidyalaya (school-hosted session)") :
            Row.Action.Destination == "place.home" ? TEXT("your home or care-home study room") : TEXT("Nabapara Lane repair workshop");
        const auto Details = FString::Printf(TEXT("%s | %04d-%02d-%02d %02d:%02d | %s | %d minutes | Rs 0"),
            *Title, Time.Year, Time.Month, Time.Day, Time.Hour, Time.Minute, Place, Row.Action.DurationMinutes);
        Model.Lines.Add(FText::FromString(Details));
        if (Row.P502Record)
        {
            Model.Lines.Add(FText::FromString(TEXT("Completed and recorded once:")));
            for (const auto& Record : Row.P502Record->Records) Model.Lines.Add(P506TaskWords(Record));
            continue;
        }
        if (Row.P502Stage == P502TaskStage::LegacyAttendance)
            Model.Lines.Add(FText::FromString(TEXT("Earlier attendance is retained. It does not count as a completed task; a new task session may be booked when offered.")));
        else if (Row.Attended && !Task)
        {
            Model.Lines.Add(FText::FromString(TEXT("Session attended and recorded.")));
            continue;
        }
        for (const auto& Conflict : Row.P502Conflicts)
            Model.Lines.Add(FText::FromString(TEXT("Time conflict: ") + P506TaskWords(Conflict).ToString()));
        if (Row.P502AlternativeAt != 0)
        {
            const auto Next = GameClock::ToCalendar(Row.P502AlternativeAt);
            Model.Lines.Add(FText::FromString(FString::Printf(TEXT("Next free alternative: %04d-%02d-%02d %02d:%02d"),
                Next.Year, Next.Month, Next.Day, Next.Hour, Next.Minute)));
        }
        for (const auto& Alternative : Row.P502Alternatives)
            Model.Lines.Add(FText::FromString(TEXT("Finishing may displace: ") + P506TaskWords(Alternative.Id).ToString()));
        if (Row.CanBook)
            add(Row, TEXT("Book / rebook: ") + Title, P407UFollowupAction::Book, true);
        if (Row.Booked)
        {
            Model.Lines.Add(FText::FromString(TEXT("Reservation retained. Cancel or leave it unused without completing the task.")));
            if (Row.At > Sim.State().Clock.Now())
                add(Row, FString::Printf(TEXT("Wait here until %s - %lld ordinary minutes pass, including school and home time"),
                    *Title, static_cast<long long>(Row.At - Sim.State().Clock.Now())), P407UFollowupAction::Attend,
                    SchoolSession && bAtSchool, true);
            if (SchoolSession && Row.P502Stage != P502TaskStage::Arrived && Row.P502Stage != P502TaskStage::Active &&
                Row.P502Stage != P502TaskStage::Paused)
                add(Row, (Task ? TEXT("Arrive for task: ") : TEXT("Attend and finish meeting: ")) + Title + (bAtSchool ? TEXT("") : TEXT(" (visit school first)")),
                    P407UFollowupAction::Attend, Row.CanAttend && bAtSchool);
            if (!SchoolSession)
                Model.Lines.Add(FText::FromString(TEXT("Reservation only at this destination. Use the ordinary home study/rest activities or visit the workshop independently; this screen does not record that visit.")));
        }
        if (Task && (Row.P502Stage == P502TaskStage::Arrived || Row.P502Stage == P502TaskStage::Active ||
            Row.P502Stage == P502TaskStage::Paused))
        {
            Model.Screen = EScreen::P407USession;
            Model.Title = FText::FromString(TEXT("School follow-up task"));
            if (Row.P502Stage == P502TaskStage::Arrived)
                add(Row, TEXT("Start: ") + Title, P407UFollowupAction::P502Start, Row.P502CanStart && bAtSchool);
            if (Row.P502Stage == P502TaskStage::Paused)
                add(Row, TEXT("Resume: ") + Title, P407UFollowupAction::P502Resume, Row.P502CanResume && bAtSchool);
            if (Row.P502Stage == P502TaskStage::Active)
            {
                for (const auto& Choice : Row.P502Choices)
                {
                    FString Consequence;
                    for (const auto& Record : Choice.Records)
                        Consequence += (Consequence.IsEmpty() ? TEXT("") : TEXT("; ")) + P506TaskWords(Record).ToString();
                    add(Row, FString::Printf(TEXT("Finish: %s | %d minutes | Records: %s"),
                        *P506TaskWords(Choice.Id).ToString(), Row.Action.DurationMinutes, *Consequence),
                        P407UFollowupAction::P502Resolve, Row.P502CanResolve && bAtSchool, false, Choice.Id);
                }
                add(Row, TEXT("Leave task and return to the world (no consequence; resume or rebook later)"),
                    P407UFollowupAction::P502Leave, true);
            }
        }
        if (Row.Booked)
            add(Row, TEXT("Refuse / cancel booking: ") + Title, P407UFollowupAction::Cancel, true);
    }
    FAction School;
    School.bP506SchoolDay = true;
    School.Label = FText::FromString(TEXT("Open ordinary school timetable (reading costs no time; visit school first)"));
    School.bEnabled = bAtSchool;
    Model.Actions.Add(MoveTemp(School));
}
