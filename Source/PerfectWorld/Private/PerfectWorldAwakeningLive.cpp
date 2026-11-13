#include "PerfectWorldAwakeningBridge.h"

#include "Engine/World.h"
#include "PerfectWorldAwakeningCatalogue.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldSchoolPresenter.h"
#include "TutorialDungeon.h"

namespace PWAwakening
{
namespace
{
// P38: floor choices keep their presentation IDs. The immutable core catalogue supplies command IDs.
class P38FLiveFloor final : public IAwFloorBridge
{
public:
    explicit P38FLiveFloor(UWorld& InWorld) : World(&InWorld) {}

    UPWLaneLifeSubsystem* P38Life() const { return UPWLaneLifeSubsystem::Get(World.Get()); }

    virtual bool CanEnter(FString& OutWhy) override
    {
        const auto* Life = P38Life();
        if (Life == nullptr)
        {
            OutWhy = TEXT("The supervised floor is unavailable.");
            return false;
        }
        const auto& State = Life->GetWorldState();
        if (State.Tutorial.Phase == PerfectWorld::TutorialDungeonPhase::Active) return true;
        auto Preview = State.Tutorial;
        const auto Eligibility = PerfectWorld::BeginTutorialDungeon(Preview, State.Awakening, Life->GetNow());
        if (Eligibility.Ok()) return true;
        const auto Appointment = PerfectWorld::GameClock::ToCalendar(State.Awakening.MakeUp.At.value_or(State.Awakening.AppointmentAt));
        OutWhy = FString::Printf(TEXT("Your supervised appointment is %02d/%02d/%04d at %02d:%02d (%lld minutes)."),
            Appointment.Day, Appointment.Month, Appointment.Year, Appointment.Hour, Appointment.Minute,
            static_cast<long long>(PerfectWorld::TutorialSessionMinutes));
        return false;
    }

    virtual FAwEnterResult Enter() override
    {
        FAwEnterResult Result;
        auto* Life = P38Life();
        if (Life == nullptr) return Result;
        const auto& State = Life->GetWorldState();
        Result.bMakeUp = State.Awakening.MakeUp.Phase == PerfectWorld::AwakeningMakeUpPhase::Scheduled;
        if (State.Tutorial.Phase == PerfectWorld::TutorialDungeonPhase::Active)
        {
            Result.bAccepted = true;
            Result.bResumed = true; // a loaded supervised session keeps its original start and conduct.
            return Result;
        }
        const auto Done = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::TutorialEnter{}});
        Result.bAccepted = Done.Accepted;
        Result.Code = PWLaneLife::ToFString(Done.Why.Code);
        Result.Message = PWLaneLife::ToFString(Done.Why.Message);
        return Result;
    }

    virtual FAwFloorStatus GetStatus() override
    {
        FAwFloorStatus Result;
        const auto* Life = P38Life();
        if (Life == nullptr) return Result;
        const auto& Tutorial = Life->GetWorldState().Tutorial;
        Result.bSessionOpen = Tutorial.Phase == PerfectWorld::TutorialDungeonPhase::Active;
        Result.bSessionComplete = Tutorial.Phase == PerfectWorld::TutorialDungeonPhase::Completed;
        for (int32 Index = 0; Index < SituationCount; ++Index)
            Result.Phase[Index] = Index < Tutorial.NextSituation ? EAwPhase::Complete
                : Index == Tutorial.NextSituation && Result.bSessionOpen
                    ? (Tutorial.RecoveryPending ? EAwPhase::Recovering : EAwPhase::Active) : EAwPhase::Locked;
        return Result;
    }

    virtual FAwChoiceResult SubmitChoice(const FName Situation, const FName Choice) override
    {
        FAwChoiceResult Result;
        auto* Life = P38Life();
        const int32 Index = AwSituationIndex(Situation);
        const auto* FloorSituation = AwFindSituation(Situation);
        if (Life == nullptr || FloorSituation == nullptr || Index == INDEX_NONE) return Result;
        PerfectWorld::Command Command{PerfectWorld::CommandSource::Player, PerfectWorld::TutorialWithdraw{}};
        if (Choice != AwWithdrawDef().Id)
        {
            int32 ChoiceIndex = INDEX_NONE;
            for (int32 Row = 0; Row < FloorSituation->Choices.Num(); ++Row)
                if (FloorSituation->Choices[Row].Id == Choice) ChoiceIndex = Row;
            if (ChoiceIndex == INDEX_NONE)
            {
                Result.Code = TEXT("tutorial.wrong_choice_or_station");
                Result.Message = TEXT("That action is not offered at this station.");
                return Result;
            }
            const auto& Core = PerfectWorld::TutorialSituations()[Index];
            Command.Payload = PerfectWorld::TutorialAct{std::string(Core.StationId), std::string(Core.Choices[ChoiceIndex].Id)};
            Result.Consequence = PWLaneLife::ToName(Core.Choices[ChoiceIndex].ConsequenceId);
        }
        const auto Done = Life->SubmitAwakeningCommand(Command);
        Result.bAccepted = Done.Accepted;
        Result.Code = PWLaneLife::ToFString(Done.Why.Code);
        Result.Message = PWLaneLife::ToFString(Done.Why.Message);
        const auto Status = GetStatus();
        Result.Phase = Status.Phase[Index];
        Result.bSessionComplete = Status.bSessionComplete;
        if (Done.Accepted && Status.bSessionComplete) P38ShowResult();
        return Result;
    }

    virtual void NoteAssistance(FName, FName) override
    {
        auto* Life = P38Life();
        if (Life == nullptr) return;
        if (!Life->GetWorldState().Tutorial.RecoveryPending)
        {
            const auto Assisted = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::TutorialAssist{}});
            if (!Assisted.Accepted)
            {
                Life->ShowPlayerNotice(PWLaneLife::ToText(Assisted.Why.Message));
                return;
            }
        }
        // The contextual reset completed; retry has no conduct or time charge.
        const auto Recovered = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::TutorialRecover{}});
        if (!Recovered.Accepted) Life->ShowPlayerNotice(PWLaneLife::ToText(Recovered.Why.Message));
    }

    virtual void Leave() override
    {
        auto* Life = P38Life();
        if (Life == nullptr || Life->GetWorldState().Awakening.Granted) return;
        const auto Done = Life->SubmitAwakeningCommand({PerfectWorld::CommandSource::Player, PerfectWorld::TutorialWithdraw{}});
        if (Done.Accepted) P38ShowResult();
        else Life->ShowPlayerNotice(PWLaneLife::ToText(Done.Why.Message));
    }

private:
    void P38ShowResult()
    {
        if (auto* Presenter = UPWSchoolPresenter::Get(World.Get())) Presenter->OpenAwakeningResult();
    }
    TWeakObjectPtr<UWorld> World;
};
} // namespace

TSharedRef<IAwFloorBridge> P38MakeLiveFloorBridge(UWorld& World)
{
    return MakeShared<P38FLiveFloor>(World);
}
} // namespace PWAwakening
