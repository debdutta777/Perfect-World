#include "PerfectWorldExaminationBridge.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldSchoolCampus.h"

bool P406Exam::P407UAtSchool(UWorld* World)
{
    const auto* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
    const APawn* Pawn = nullptr;
    if (Controller != nullptr) Pawn = Controller->GetPawn();
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
    Model.Lines.Add(FText::FromString(TEXT("Choose a next step to reserve it. Appointments are optional enquiries; support and admissions are not awarded here. School sessions are attended on campus. You can close this screen, walk there, and reopen Examination from pause.")));
    const auto add = [&](const P407UFollowupView& Row, const FString& Label, P407UFollowupAction Kind, bool Enabled, bool Wait = false)
    {
        FAction Action;
        Action.Label = FText::FromString(Label);
        Action.bEnabled = Enabled;
        Action.bP407UFollowup = true;
        Action.bP407UWait = Wait;
        Action.P407UCommand = {Kind, Row.Action.Id, Row.At};
        Model.Actions.Add(MoveTemp(Action));
    };
    for (const auto& Row : Model.P407UFollowups)
    {
        const auto Title = P407UFollowupText(Row.Action.TitleTextKey).ToString();
        Model.Offers.Add(P407UFollowupText(Row.Action.BriefTextKey));
        if (Row.Attended)
        {
            Model.Lines.Add(FText::FromString(Title + TEXT(" — session attended and recorded.")));
            continue;
        }
        const auto Time = GameClock::ToCalendar(Row.At);
        const auto Place = Row.Action.Destination == "place.school" ? TEXT("Nabapara Vidyalaya") :
            Row.Action.Destination == "place.home" ? TEXT("your home or care-home study room") : TEXT("Nabapara repair workshop");
        const auto Details = FString::Printf(TEXT("%s | %04d-%02d-%02d %02d:%02d | %s | %d minutes | Rs 0"),
            *Title, Time.Year, Time.Month, Time.Day, Time.Hour, Time.Minute, Place, Row.Action.DurationMinutes);
        if (Row.CanBook)
            add(Row, TEXT("Book: ") + Details, P407UFollowupAction::Book, true);
        else if (Row.Booked)
        {
            Model.Lines.Add(FText::FromString(TEXT("Booked: ") + Details));
            if (Row.At > Sim.State().Clock.Now())
                add(Row, FString::Printf(TEXT("Wait until %s — %lld ordinary minutes pass; school and home time also passes"),
                    *Title, static_cast<long long>(Row.At - Sim.State().Clock.Now())), P407UFollowupAction::Attend,
                    Row.Action.Destination != "place.school" || bAtSchool, true);
            // Other destinations retain saved reservations; this order plays school sessions.
            if (Row.Action.Destination == "place.school")
                add(Row, FString::Printf(TEXT("Attend: %s — record the %d-minute session%s"), *Title,
                    Row.Action.DurationMinutes, bAtSchool ? TEXT("") : TEXT(" (visit the school first)")),
                    P407UFollowupAction::Attend, Row.CanAttend && bAtSchool);
            add(Row, TEXT("Cancel booking: ") + Title, P407UFollowupAction::Cancel, true);
        }
        else Model.Lines.Add(FText::FromString(Title + TEXT(" — no slot is available in the next 32 days.")));
    }
}
