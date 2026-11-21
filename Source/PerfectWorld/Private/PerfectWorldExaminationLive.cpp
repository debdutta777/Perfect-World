#include "PerfectWorldExaminationBridge.h"

#include "Engine/World.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "Tower.h"

namespace
{
P406Exam::FAction P406Action(const TCHAR* P406Label, PerfectWorld::TowerSessionAction P406Kind, bool P406Enabled = true)
{
    P406Exam::FAction P406Row;
    P406Row.Label = FText::FromString(P406Label);
    P406Row.Kind = P406Kind;
    P406Row.bEnabled = P406Enabled;
    return P406Row;
}

class FP406LiveWorld final : public P406Exam::IWorld
{
public:
    explicit FP406LiveWorld(UWorld& P406World) : P406Owner(UPWLaneLifeSubsystem::Get(&P406World)) {}

    virtual P406Exam::FModel Read() const override
    {
        using namespace PerfectWorld;
        using namespace P406Exam;
        FModel P406Model;
        const auto* P406Life = P406Owner.Get();
        if (P406Life == nullptr) return P406Model;
        const auto& P406Sim = P406Life->GetSim();
        P406Model.World = P406Sim.ExaminationWorldView();
        P406Model.Session = P406Sim.ExaminationView();
        const auto& P406World = P406Model.World;
        const auto& P406Session = P406Model.Session;
        P406Model.Title = FText::FromString(TEXT("Tower Examination"));
        if (P406World.Finished)
        {
            const auto P406Finish = P406Sim.ExaminationResult();
            if (!P406Finish.Ok())
            {
                P406Model.Lines.Add(P406RefusalText(PWLaneLife::ToFString(P406Finish.Issue.Code)));
                return P406Model;
            }
            P406Model.Screen = EScreen::Result;
            const TCHAR* P406Outcome = TEXT("Examination result");
            switch (P406Finish.Participation)
            {
            case TowerParticipation::Completed: P406Outcome = TEXT("Examination completed: all three floors cleared."); break;
            case TowerParticipation::Ejected: P406Outcome = TEXT("Examination ended: your reserve was exhausted."); break;
            case TowerParticipation::Withdrawn: P406Outcome = TEXT("You withdrew. Evidence from cleared floors is retained."); break;
            case TowerParticipation::Deferred: P406Outcome = TEXT("You deferred this year's examination. Your ordinary life continues."); break;
            case TowerParticipation::Declined: P406Outcome = TEXT("You declined this year's examination. Other qualification routes remain open."); break;
            default: break;
            }
            P406Model.Lines.Add(FText::FromString(P406Outcome));
            const auto& P406Score = P406Finish.Score;
            P406Model.Lines.Add(FText::FromString(FString::Printf(
                TEXT("Cleared floors: %d | Score: %d | National rank: %d | Decile: %d"),
                P406Score.FloorsCleared, P406Score.ScorePoints, P406Score.NationalRank, P406Score.Decile)));
            P406Model.Lines.Add(FText::FromString(TEXT("Cleared floors contribute depth and demonstrated discipline; common judgment contributes to the recorded evidence. Defeat costs rank, never life.")));
            P406Model.Lines.Add(FText::FromString(FString::Printf(TEXT("Mission clock: %lld minutes | Outside time charged: %lld minutes"),
                static_cast<long long>(P406Score.MissionMinutes), static_cast<long long>(P406World.OutsideMinutes))));
            for (const auto& P406Credential : P406Score.Credentials)
            {
                const auto* P406Route = FindTowerRoute(P406Credential.RouteId);
                P406Model.Lines.Add(FText::FromString(FString::Printf(TEXT("Evidence: %s | %d / 1000"),
                    P406Route != nullptr ? *PWLaneLife::ToFString(P406Route->Task) : TEXT("Cleared route"), P406Credential.ScoreMilli)));
            }
            P407UAppendFollowups(P406Model, P406Sim, P407UAtSchool(P406Life->GetWorld()));
            return P406Model;
        }
        if (P406World.DecisionRequired || P406World.CanDefer || P406World.CanDecline)
        {
            P406Model.Screen = EScreen::Decision;
            P406Model.Title = FText::FromString(TEXT("06:00 - 2 February 2043"));
            P406Model.Lines.Add(FText::FromString(TEXT("Decide whether to sit the national examination this year. Reading and choosing your participation cost no time.")));
            if (!P406World.CanSit) P406Model.Lines.Add(P406RefusalText(PWLaneLife::ToFString(P406World.SitRefusal)));
            P406Model.Actions.Add(P406Action(TEXT("Sit"), TowerSessionAction::Enter, P406World.CanSit));
            P406Model.Actions.Add(P406Action(TEXT("Defer"), TowerSessionAction::Defer, P406World.CanDefer));
            P406Model.Actions.Add(P406Action(TEXT("Decline"), TowerSessionAction::Decline, P406World.CanDecline));
            return P406Model;
        }
        if (!P406World.CanWithdraw)
        {
            P406Model.Lines.Add(P406RefusalText(PWLaneLife::ToFString(P406World.SitRefusal)));
            return P406Model;
        }
        P406Model.Lines.Add(FText::FromString(FString::Printf(TEXT("Floor %d | Mission clock: %lld minutes | Outside time on leaving now: %lld minutes"),
            P406Session.NextFloor, static_cast<long long>(P406Session.Result.MissionMinutes), static_cast<long long>(P406World.OutsideMinutes))));
        P406Model.Lines.Add(FText::FromString(TEXT("Progress is recorded after each accepted action. A saved session resumes the same passage. Reading costs no time; outside time is charged once when you leave.")));
        if (!P406Session.Choices.empty())
        {
            P406Model.Screen = EScreen::Beat;
            P406Model.Lines.Add(P406RunnerText(P406Session.PromptTextKey, TEXT("The passage prompt text is unavailable.")));
            P406Model.Lines.Add(FText::FromString(FString::Printf(TEXT("Reserve: %d"), P406Session.Conditions.Reserve)));
            for (const auto& P406Clue : P406Session.ClueTextKeys)
                P406Model.Lines.Add(P406RunnerText(P406Clue, TEXT("A passage clue text is unavailable.")));
            for (const auto& P406Choice : P406Session.Choices)
            {
                auto P406Row = P406Action(TEXT(""), TowerSessionAction::Choose);
                P406Row.ChoiceId = PWLaneLife::ToFString(P406Choice.Id);
                const auto P406Words = P406RunnerText(P406Choice.TextKey, TEXT("Choice text unavailable"));
                P406Row.Label = FText::FromString(FString::Printf(TEXT("%s | %d mission minutes | %d strain"),
                    *P406Words.ToString(), P406Choice.MissionMinutes, P406Choice.Strain));
                P406Model.Actions.Add(MoveTemp(P406Row));
            }
        }
        else
        {
            P406Model.Screen = EScreen::Routes;
            P406Model.Lines.Add(FText::FromString(TEXT("Opening a route costs zero mission minutes and zero strain. Each passage choice shows its own costs before you act.")));
            for (const auto& P406Id : P406Session.AvailableRouteIds)
            {
                const auto* P406Route = FindTowerRoute(P406Id);
                if (P406Route == nullptr) continue;
                auto P406Row = P406Action(TEXT(""), TowerSessionAction::OpenRoute);
                P406Row.Label = PWLaneLife::ToText(P406Route->Task);
                P406Row.RouteId = PWLaneLife::ToFString(P406Id);
                P406Row.Method = P406Route->Method;
                P406Model.Actions.Add(P406Row);
                if (P406Route->AllowsCombat)
                {
                    P406Row.Method = TowerMethod::Combat;
                    P406Row.Label = FText::FromString(P406Row.Label.ToString() + TEXT(" (combat method)"));
                    P406Model.Actions.Add(MoveTemp(P406Row));
                }
            }
        }
        return P406Model;
    }

    virtual PerfectWorld::CommandResult Submit(const P406Exam::FAction& P406ActionRow) override
    {
        auto* P406Life = P406Owner.Get();
        if (P406Life == nullptr) return {};
        if (P406ActionRow.bP407UFollowup)
        {
            if (P406ActionRow.bP407UWait)
            {
                // Wait is ordinary time; it never records attendance.
                for (const auto& P407URow : P406Life->GetSim().P407UFollowups())
                    if (P407URow.Action.Id == P406ActionRow.P407UCommand.ActionId && P407URow.Booked && !P407URow.Attended &&
                        P407URow.At == P406ActionRow.P407UCommand.ExpectedAt && P407URow.At > P406Life->GetNow())
                        return P406Life->P406SubmitExaminationCommand({PerfectWorld::CommandSource::Player,
                            PerfectWorld::AdvanceTime{P407URow.At - P406Life->GetNow()}});
                PerfectWorld::CommandResult P407URefused;
                P407URefused.Why = {"p407u.stale", "Read the current appointment time again."};
                return P407URefused;
            }
            if (P406ActionRow.P407UCommand.Kind == PerfectWorld::P407UFollowupAction::Attend &&
                !P407UAtSchool(P406Life->GetWorld()))
            {
                PerfectWorld::CommandResult P407URefused;
                P407URefused.Why = {"p407u.place", "Visit the school campus for this session."};
                return P407URefused;
            }
            return P406Life->P406SubmitExaminationCommand({PerfectWorld::CommandSource::Player, P406ActionRow.P407UCommand});
        }
        const auto P406View = P406Life->GetSim().ExaminationView();
        PerfectWorld::TowerSessionCommand P406Command;
        P406Command.Kind = P406ActionRow.Kind;
        P406Command.RouteId = TCHAR_TO_UTF8(*P406ActionRow.RouteId);
        P406Command.Method = P406ActionRow.Method;
        P406Command.ChoiceId = TCHAR_TO_UTF8(*P406ActionRow.ChoiceId);
        P406Command.Runner = P406View.Runner;
        P406Command.Beat = P406View.Beat;
        return P406Life->P406SubmitExaminationCommand({PerfectWorld::CommandSource::Player, P406Command});
    }
private:
    TWeakObjectPtr<UPWLaneLifeSubsystem> P406Owner;
};
}

TSharedRef<P406Exam::IWorld> P406Exam::P406MakeLiveWorld(UWorld& World)
{
    return MakeShared<FP406LiveWorld>(World);
}
