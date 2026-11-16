#include "PerfectWorldAcceptanceSchool.h"

#include "Algo/Reverse.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "PerfectWorldAcceptanceSchoolR7Steps.h"
#include "PerfectWorldBuyPresenter.h"
#include "PerfectWorldCommerceCounter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldGuardNotice.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneTeaShop.h"
#include "PerfectWorldSchoolActors.h"
#include "PerfectWorldSchoolPresenter.h"
#include "UI/SPWBuyPanel.h"
#include "UI/SPWClassLesson.h"

// P313: production bridges, real walking and interactions. No fake, copied sim or placement override.
namespace
{
using namespace PWAcceptance;

FStepDef P313Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 30;
    Step.Run = MoveTemp(Run);
    return Step;
}

APWCounterBuyActor* P313Counter(UWorld* World)
{
    for (TActorIterator<APWCounterBuyActor> It(World); It; ++It)
        if (It->GetShopId() == FName(TEXT("shop.shanti_chai"))) return *It;
    return nullptr;
}

APWSchoolSeatActor* P313Seat(UWorld* World)
{
    for (TActorIterator<APWSchoolSeatActor> It(World); It; ++It) return *It;
    return nullptr;
}

TSharedPtr<SPWClassLesson> P313Lesson(UWorld* World)
{
    const auto* Host = UPWSchoolPresenter::Get(World);
    return Host != nullptr && Host->GetKind() == EPWSchoolPanel::Lesson
        ? StaticCastSharedPtr<SPWClassLesson>(Host->GetPanel()) : nullptr;
}

void P313ToShop(TArray<FStepDef>& Out, const FString& Prefix)
{
    using namespace SchoolR7;
    const auto P = ScAPoints();
    auto Exit = PWSchool::ScClassroomSeatRoute(PWSchool::ScReadCampus(), P);
    Algo::Reverse(Exit);
    Exit.Add(P.GateCentre + FVector(0, 100, 0));
    Exit.Add(P.GateCentre - FVector(0, 100, 0));
    Out.Add(ScAPointsWalk(Prefix + TEXT("_leave_classroom"), Exit, 90));
    Out.Add(ScARouteWalk(Prefix + TEXT("_walk_to_shop"), "walk.nabapara.school_gate",
        "walk.door.nabapara.shanti_chai_shutter", 0, 300));
    const auto Door = [](UPWAcceptanceSubsystem& Sub) -> APWDoorActor*
    {
        for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
            if (It->GetDoorId() == FName(PWLane::TeaShopShutterDoorId)) return *It;
        return nullptr;
    };
    Out.Add(MakeInteractStep(Prefix + TEXT("_open_shop"),
        [Door](UPWAcceptanceSubsystem& Sub) -> AActor* { return Door(Sub); },
        [Door](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Found = Door(Sub);
            Detail = TEXT("shop shutter open");
            return Found != nullptr && Found->IsOpen() && !Found->IsMoving();
        },
        [Door](UPWAcceptanceSubsystem& Sub)
        {
            const auto* Found = Door(Sub);
            return Found != nullptr && Found->IsOpen() && !Found->IsMoving();
        }, 20));
    // The canonical Shanti customer aisle: TeaShopData::RouteTable, counter_to_door points 5 and 4.
    Out.Add(ScAPointsWalk(Prefix + TEXT("_customer_aisle"),
        {FVector(950, 610, ScAFloor), FVector(950, 810, ScAFloor)}, 30));
    Out.Add(MakeWalkStep(Prefix + TEXT("_approach_counter"),
        [](UPWAcceptanceSubsystem& Sub, FVector& Point, FString& Why)
        {
            const auto* Counter = P313Counter(Sub.GetPlayWorld());
            if (Counter == nullptr) { Why = TEXT("live Shanti counter missing"); return false; }
            Point = Counter->GetInteractionPoint() - FVector(0, 0, 110);
            return true;
        }, 30, 30));
}
}

namespace PWAcceptance
{
void P313AppendGuardsResumeSteps(TArray<FStepDef>& Out)
{
    using namespace SchoolR7;
    const auto Session = MakeShared<uint64>(0);
    const auto Binding = MakeShared<FString>();
    const auto Clock = MakeShared<PerfectWorld::GameInstant>(0);
    const auto Wallet = MakeShared<int64>(0);
    P313AppendLessonEntrySteps(Out);
    Out.Add(P313Action(TEXT("P313_back_out_keeps_lesson"), [Session, Binding, Clock, Wallet](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Lesson = P313Lesson(Sub.GetPlayWorld());
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Lesson || Life == nullptr) return FStepOutcome::Fail(TEXT("no live lesson"));
        *Session = Lesson->GetDisplayedSession();
        *Binding = PWLaneLife::ToFString(Lesson->GetFrame().Binding.Id);
        *Clock = Life->GetNow();
        *Wallet = Life->GetWorldState().Commerce.WalletPaise;
        Lesson->RequestClose();
        if (UPWSchoolPresenter::IsOpenIn(Sub.GetPlayWorld()) || !Life->GetWorldState().SchoolSessions.Active)
            return FStepOutcome::Fail(TEXT("Back did not return to play with the lesson open"));
        return FStepOutcome::Pass(TEXT("Back keeps the core session and restores play"));
    }));
    P313ToShop(Out, TEXT("P313_held"));
    Out.Add(MakeInteractStep(TEXT("P313_buy_refused_with_classroom_direction"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return P313Counter(Sub.GetPlayWorld()); },
        [Session, Clock, Wallet](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            const auto* Counter = P313Counter(Sub.GetPlayWorld());
            if (Life == nullptr || Counter == nullptr) return false;
            Detail = Counter->GetLastNotice().ToString();
            const auto& Active = Life->GetWorldState().SchoolSessions.Active;
            return !UPWBuyPresenter::IsOpenIn(Sub.GetPlayWorld()) && Active && Active->Session == *Session &&
                Detail == P313GuardNotice(TEXT("school.lesson_open")).ToString() &&
                Life->GetNow() == *Clock && Life->GetWorldState().Commerce.WalletPaise == *Wallet;
        }, nullptr, 20));
    Out.Add(MakeShotStep(TEXT("P313_shot_directing_notice")));
    Out.Add(ScAPointsWalk(TEXT("P313_leave_shop_aisle"),
        {FVector(950, 810, ScAFloor), FVector(950, 610, ScAFloor), FVector(950, 410, ScAFloor)}, 40));
    Out.Add(ScARouteWalk(TEXT("P313_return_to_school"), "walk.door.nabapara.shanti_chai_shutter",
        "walk.nabapara.school_gate", 0, 300));
    const auto P = ScAPoints();
    TArray<FVector> Entry{P.GateCentre + FVector(0, 150, 0)};
    Entry.Append(PWSchool::ScClassroomSeatRoute(PWSchool::ScReadCampus(), P));
    Out.Add(ScAPointsWalk(TEXT("P313_return_to_seat"), Entry, 90));
    Out.Add(MakeInteractStep(TEXT("P313_seat_resumes_same_session"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return P313Seat(Sub.GetPlayWorld()); },
        [Session, Binding](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto Lesson = P313Lesson(Sub.GetPlayWorld());
            Detail = TEXT("seat resumes the original binding with Leave early available");
            return Lesson && Lesson->GetDisplayedSession() == *Session &&
                PWLaneLife::ToFString(Lesson->GetFrame().Binding.Id) == *Binding && Lesson->CanLeaveEarly();
        }, nullptr, 20));
    Out.Add(P313Action(TEXT("P313_leave_early_releases_guard"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto Lesson = P313Lesson(Sub.GetPlayWorld());
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Lesson || Life == nullptr || Lesson->PressLeaveEarly() || !Lesson->IsLeaveArmed() || !Lesson->PressLeaveEarly())
            return FStepOutcome::Fail(TEXT("two-step Leave early did not close the run"));
        if (Life->GetWorldState().SchoolSessions.Active) return FStepOutcome::Fail(TEXT("lesson guard still active"));
        UPWSchoolPresenter::Get(Sub.GetPlayWorld())->Close();
        return FStepOutcome::Pass(TEXT("Leave early removed the active run"));
    }));
    P313ToShop(Out, TEXT("P313_released"));
    Out.Add(MakeInteractStep(TEXT("P313_buy_opens_after_exit"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return P313Counter(Sub.GetPlayWorld()); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            Detail = TEXT("live counter can serve again");
            return UPWBuyPresenter::IsOpenIn(Sub.GetPlayWorld());
        }, nullptr, 30));
    Out.Add(P313Action(TEXT("P313_purchase_succeeds_after_exit"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Host = UPWBuyPresenter::Get(Sub.GetPlayWorld());
        const auto Panel = Host != nullptr ? Host->GetPanel() : nullptr;
        if (!Panel) return FStepOutcome::Fail(TEXT("no live buy panel"));
        if (Rt.Phase == 0)
        {
            const int32 Index = Panel->GetService().Goods.IndexOfByPredicate([](const auto& Good) { return Good.ItemId == FName(TEXT("item.biscuits")); });
            if (Index == INDEX_NONE) return FStepOutcome::Fail(TEXT("Shanti biscuits unavailable"));
            Panel->SelectGood(Index);
            Rt.NextPhase();
        }
        if (!Panel->CanConfirmNow()) return FStepOutcome::Wait();
        if (!Panel->Confirm() || Panel->GetStage() != PWBuy::EStage::Receipt || Panel->GetSubmitCount() != 1)
            return FStepOutcome::Fail(TEXT("live purchase did not produce one receipt"));
        const auto& Receipt = Panel->GetReceipt().Receipt;
        if (Receipt.ItemId != "item.biscuits" || Receipt.BalanceAfterPaise >= Receipt.BalanceBeforePaise)
            return FStepOutcome::Fail(TEXT("no authoritative wallet debit"));
        Host->Close();
        return FStepOutcome::Pass(TEXT("one accepted purchase and wallet debit after leaving the lesson"));
    }));
}
}
