#include "PerfectWorldAcceptanceCommerce.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Input/Events.h"
#include "PerfectWorldAcceptanceProprietorWork.h"
#include "PerfectWorldBuyPresenter.h"
#include "PerfectWorldCommerceCounter.h"
#include "PerfectWorldCommerceText.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneTeaShop.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldProprietorBindings.h"
#include "UI/SPWBuyPanel.h"
#include "UI/SPWInventoryPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;
struct FR88CommerceRun
{
    FName Shop;
    FName Item = FName(TEXT("item.jhalmuri"));
    FString ItemName = TEXT("Jhalmuri");
    int64 WalletBefore = 0;
    PerfectWorld::CommerceReceipt Receipt;
    PerfectWorld::GameInstant PausedAt = 0;
};

FStepDef R88Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step; Step.Name = Name; Step.Run = MoveTemp(Run); Step.TimeoutSeconds = 30;
    return Step;
}

bool R88StoreProvider(UPWAcceptanceSubsystem& Sub, const bool bShantiSmoke, FNpBindingProvider& Out, FVector& Outside)
{
    if (bShantiSmoke)
    {
        auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        if (Presence == nullptr) return false;
        const auto* Door = Presence->Bindings().FindAnchor(FName(PWLane::TeaShopAnchorDoor));
        const auto* Route = Presence->Bindings().FindRoute(FName(TEXT("route.shanti_chai.counter_to_door")));
        if (Door == nullptr || Route == nullptr) return false;
        Out.Anchors.Add(FName(PWLane::TeaShopAnchorDoor), *Door);
        Out.Routes.Add(FName(TEXT("route.shanti_chai.counter_to_door")), *Route);
        Outside = -Door->Feet.GetRotation().GetForwardVector();
        for (TActorIterator<APWCounterBuyActor> It(Sub.GetPlayWorld()); It; ++It)
            if (It->GetShopId() == FName(TEXT("shop.shanti_chai")))
            {
                FNpAnchorBinding Customer;
                Customer.Feet = FTransform(It->GetInteractionPoint() - FVector(0, 0, 110));
                Out.Anchors.Add(FName(TEXT("r88.customer")), Customer);
                return true;
            }
        return false;
    }
    PWLane::FLaneBuilding Building;
    if (!PWLane::MakeProprietorBuilding(PWLane::MahuaBuildingId, Building)) return false;
    Out = PWLane::NpMahuaBindings(Building);
    Outside = -Building.Frame.V;
    return !Out.Anchors.IsEmpty();
}

APWCounterBuyActor* R88Counter(UPWAcceptanceSubsystem& Sub, const bool bShantiSmoke)
{
    FNpBindingProvider Provider; FVector Outside;
    if (!R88StoreProvider(Sub, bShantiSmoke, Provider, Outside)) return nullptr;
    const auto* Customer = Provider.Anchors.Find(FName(bShantiSmoke ? TEXT("r88.customer") : PWLane::MahuaAnchorCustomer));
    if (Customer == nullptr) return nullptr;
    for (TActorIterator<APWCounterBuyActor> It(Sub.GetPlayWorld()); It; ++It)
        if (FVector::Dist2D(It->GetInteractionPoint(), Customer->Feet.GetLocation()) < 250) return *It;
    return nullptr;
}

FStepDef R88Walk(const TCHAR* Name, const TCHAR* AnchorId, const bool bShantiSmoke, const int32 RouteIndex = -1)
{
    return PWAcceptance::MakeWalkStep(Name, [AnchorId, RouteIndex, bShantiSmoke](UPWAcceptanceSubsystem& Sub, FVector& Point, FString& Why)
    {
        FNpBindingProvider Provider; FVector Outside;
        if (R88StoreProvider(Sub, bShantiSmoke, Provider, Outside))
        {
            if (RouteIndex >= 0)
            {
                const auto* Route = Provider.Routes.Find(FName(AnchorId));
                if (Route != nullptr && Route->Points.IsValidIndex(RouteIndex)) { Point = Route->Points[RouteIndex]; return true; }
            }
            else if (const auto* Anchor = Provider.Anchors.Find(FName(AnchorId))) { Point = Anchor->Feet.GetLocation(); return true; }
        }
        Why = TEXT("R88 canonical customer route is unavailable");
        return false;
    }, 30, 40);
}

TSharedPtr<SWidget> R88FindTag(const TSharedRef<SWidget>& Root, const FName Tag)
{
    if (!Root->GetVisibility().IsVisible()) return nullptr;
    if (Root->GetTag() == Tag) return Root;
    auto* Children = Root->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        if (auto Found = R88FindTag(Children->GetChildAt(Index), Tag)) return Found;
    return nullptr;
}

TSharedPtr<SWidget> R88ScreenTag(const TCHAR* Tag)
{
    if (!FSlateApplication::IsInitialized()) return nullptr;
    for (const auto& Window : FSlateApplication::Get().GetTopLevelWindows())
        if (auto Found = R88FindTag(Window, FName(Tag))) return Found;
    return nullptr;
}

bool R88ContainsText(const TSharedRef<SWidget>& Root, const FString& Text)
{
    if (!Root->GetVisibility().IsVisible()) return false;
    if (Root->GetTypeAsString() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Root)->GetText().ToString().Contains(Text)) return true;
    auto* Children = Root->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        if (R88ContainsText(Children->GetChildAt(Index), Text)) return true;
    return false;
}
}

namespace PWAcceptance
{
void R88AppendCommerceSteps(TArray<FStepDef>& Out, const bool bShantiSmoke)
{
    const auto State = MakeShared<FR88CommerceRun>();
    if (bShantiSmoke) { State->Item = FName(TEXT("item.biscuits")); State->ItemName = TEXT("Biscuits"); }
    const TCHAR* DoorAnchor = bShantiSmoke ? PWLane::TeaShopAnchorDoor : PWLane::MahuaAnchorFrontDoor;
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(R88Action(TEXT("commerce_declared_start_outside_shop"), [bShantiSmoke, DoorAnchor](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        FNpBindingProvider Provider; FVector Outside;
        auto* Pawn = Sub.GetPawn();
        if (Pawn == nullptr || !R88StoreProvider(Sub, bShantiSmoke, Provider, Outside)) return FStepOutcome::Fail(TEXT("no pawn or shop provider"));
        const auto* Door = Provider.Anchors.Find(FName(DoorAnchor));
        if (Door == nullptr) return FStepOutcome::Fail(TEXT("no canonical shop threshold"));
        if (Rt.Phase == 0)
        {
            if (!Pawn->TeleportTo(Door->Feet.GetLocation() + Outside * 150 + FVector(0, 0, PWPlayer::CapsuleHalfHeight + 5), FRotator::ZeroRotator))
                return FStepOutcome::Fail(TEXT("declared start refused"));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Rt.NextPhase();
        }
        return Rt.InPhase() >= 0.8 && Pawn->GetCharacterMovement()->IsMovingOnGround() ?
            FStepOutcome::Pass(TEXT("one declared start; the remaining approach uses physical movement")) : FStepOutcome::Wait();
    }));
    Out.Add(R55PilotTimeStep(TEXT("commerce_clock_to_0900"), 9, 0));
    Out.Add(R88Walk(TEXT("commerce_walk_to_shop_door"), DoorAnchor, bShantiSmoke));
    const auto FindDoor = [bShantiSmoke](UPWAcceptanceSubsystem& Sub) -> APWDoorActor*
    {
        for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
            if (It->GetDoorId() == FName(bShantiSmoke ? PWLane::TeaShopShutterDoorId : PWLane::MahuaDoorId)) return *It;
        return nullptr;
    };
    Out.Add(MakeInteractStep(TEXT("commerce_open_shop_door"),
        [FindDoor](UPWAcceptanceSubsystem& Sub) -> AActor* { return FindDoor(Sub); },
        [FindDoor](UPWAcceptanceSubsystem& Sub, FString& Detail)
        { const auto* Door = FindDoor(Sub); Detail = TEXT("real door open"); return Door != nullptr && Door->IsOpen() && !Door->IsMoving(); },
        [FindDoor](UPWAcceptanceSubsystem& Sub) { const auto* Door = FindDoor(Sub); return Door != nullptr && Door->IsOpen() && !Door->IsMoving(); }, 20));
    Out.Add(R88Walk(TEXT("commerce_walk_inside_aisle"), bShantiSmoke ? TEXT("route.shanti_chai.counter_to_door") :
        TEXT("route.hardware_store.counter_to_door"), bShantiSmoke, bShantiSmoke ? 4 : 3));
    Out.Add(R88Walk(TEXT("commerce_walk_to_counter"), bShantiSmoke ? TEXT("r88.customer") : PWLane::MahuaAnchorCustomer, bShantiSmoke));
    Out.Add(R88Action(TEXT("commerce_real_buy_interaction"), [State, bShantiSmoke](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Counter = R88Counter(Sub, bShantiSmoke);
        auto* Interaction = Sub.GetInteraction();
        if (Counter == nullptr) return FStepOutcome::Fail(bShantiSmoke ? TEXT("Shanti Chai live counter is missing") :
            TEXT("R88 content blocker: Mahua has no commerce_shop; jhalmuri belongs to shop.vendor (unregistered Koushik). No fake sale was substituted."));
        const auto Bridge = PWCommerce::CmBridge::Resolve(Sub.GetPlayWorld());
        if (!Bridge || Interaction == nullptr) return FStepOutcome::Fail(TEXT("no live commerce/interaction bridge"));
        if (Rt.Phase == 0)
        {
            Sub.FaceWorldPoint(Counter->GetInteractionPoint());
            if (Interaction->GetTarget() != Counter)
            {
                const AActor* Held = Interaction->GetTarget();
                Rt.Note = FString::Printf(TEXT("the Buy counter never became the target; held %s, player at %s, counter at %s"),
                    Held != nullptr ? *Held->GetName() : TEXT("nothing"), *Sub.GetPawn()->GetActorLocation().ToCompactString(),
                    *Counter->GetInteractionPoint().ToCompactString());
                Interaction->CycleTarget();
                return FStepOutcome::Wait();
            }
            const auto Wallet = Bridge->ReadWallet();
            if (!Wallet.IsSet()) return FStepOutcome::Fail(TEXT("no live wallet"));
            State->WalletBefore = Wallet.GetValue(); State->Shop = Counter->GetShopId();
            if (!Interaction->TryInteract()) return FStepOutcome::Fail(TEXT("real counter interaction refused"));
            Rt.NextPhase();
        }
        auto* Buy = UPWBuyPresenter::Get(Sub.GetPlayWorld());
        if (Buy != nullptr && Buy->IsOpen()) return FStepOutcome::Pass(TEXT("real Buy interaction opened live panel after core service readiness"));
        // A refusal never opens the panel, so name the core's answer rather than timing out in silence.
        Rt.Note = FString::Printf(TEXT("the panel did not open; core said %s; notice '%s'"),
            UTF8_TO_TCHAR(std::string(PerfectWorld::CommerceErrorCode(Counter->GetLastError())).c_str()), *Counter->GetLastNotice().ToString());
        return FStepOutcome::Wait();
    }));
    Out.Add(R88Action(TEXT("commerce_buy_food_and_see_debit"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Buy = UPWBuyPresenter::Get(Sub.GetPlayWorld());
        const auto Panel = Buy != nullptr ? Buy->GetPanel() : nullptr;
        if (!Panel) return FStepOutcome::Fail(TEXT("no actual buy panel"));
        if (Rt.Phase == 0)
        {
            const int32 Index = Panel->GetService().Goods.IndexOfByPredicate([State](const auto& Good) { return Good.ItemId == State->Item; });
            if (Index == INDEX_NONE) return FStepOutcome::Fail(TEXT("The content catalogue does not sell the scenario's food"));
            Panel->SelectGood(Index); Rt.NextPhase(); return FStepOutcome::Wait();
        }
        if (!Panel->CanConfirmNow()) return FStepOutcome::Wait();
        Panel->Confirm();
        const auto& Receipt = Panel->GetReceipt().Receipt;
        if (Panel->GetSubmitCount() != 1 || PWLaneLife::ToName(Receipt.ItemId) != State->Item || Receipt.BalanceBeforePaise != State->WalletBefore ||
            Receipt.BalanceAfterPaise >= State->WalletBefore) return FStepOutcome::Fail(TEXT("no single purchase receipt and wallet debit"));
        State->Receipt = Receipt;
        return FStepOutcome::Pass(TEXT("one real panel confirmation; receipt shows the authoritative wallet debit"));
    }));
    Out.Add(MakeShotStep(TEXT("shot_commerce_receipt")));
    Out.Add(R88Action(TEXT("commerce_open_pause_inventory"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Sub.GetHud() == nullptr) return FStepOutcome::Fail(TEXT("no pause/world owner"));
        if (Rt.Phase == 0)
        {
            UPWBuyPresenter::Get(Sub.GetPlayWorld())->Close();
            Sub.GetHud()->ToggleInWorldMenu(); Rt.NextPhase(); return FStepOutcome::Wait();
        }
        auto Button = R88ScreenTag(TEXT("R88PauseInventory"));
        if (Rt.Phase == 1 && Button && Button->GetTypeAsString() == TEXT("SButton"))
        { StaticCastSharedPtr<SButton>(Button)->SimulateClick(); State->PausedAt = Life->GetNow(); Rt.NextPhase(); }
        const auto Inventory = R88ScreenTag(PWInventory::PanelTag);
        return Inventory && R88ContainsText(Inventory.ToSharedRef(), PWCommerce::CmFormatPaise(State->Receipt.BalanceAfterPaise)) &&
            R88ContainsText(Inventory.ToSharedRef(), State->ItemName) ?
            FStepOutcome::Pass(TEXT("pause Inventory displays purchased item, receipt and wallet")) : FStepOutcome::Wait();
    }));
    Out.Add(MakeShotStep(TEXT("shot_commerce_inventory")));
    Out.Add(R88Action(TEXT("commerce_consume_from_inventory"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto Bridge = PWCommerce::CmBridge::Resolve(Sub.GetPlayWorld());
        auto Use = R88ScreenTag(PWInventory::UseTag);
        if (!Bridge || Life == nullptr) return FStepOutcome::Fail(TEXT("no live inventory owner"));
        if (Rt.Phase == 0)
        {
            if (!Use || !Use->IsEnabled() || Use->GetTypeAsString() != TEXT("SButton")) return FStepOutcome::Fail(TEXT("no enabled Consume action"));
            StaticCastSharedPtr<SButton>(Use)->SimulateClick(); Rt.NextPhase();
        }
        const auto Inventory = Bridge->ReadInventory();
        const bool Remaining = Inventory.Stacks.ContainsByPredicate([State](const auto& Stack) { return Stack.ItemId == State->Item; });
        if (Remaining || Life->GetNow() != State->PausedAt || Inventory.WalletPaise != State->Receipt.BalanceAfterPaise)
            return FStepOutcome::Fail(TEXT("food was not consumed atomically at zero minutes"));
        Sub.GetHud()->ResumeGame();
        return FStepOutcome::Pass(TEXT("actual Inventory Consume removed food, preserved wallet and spent zero minutes"));
    }));
}
}
