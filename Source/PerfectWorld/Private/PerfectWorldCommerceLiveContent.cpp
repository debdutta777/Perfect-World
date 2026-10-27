#include "PerfectWorldCommerceLive.h"

#include "CollisionQueryParams.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "PerfectWorldCommerceCounter.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

namespace PWCommerce
{
bool FCmLiveContent::Load(const PerfectWorld::ContentDb& Db)
{
    auto Loaded = PerfectWorld::LoadCommerceContent(Db);
    if (!Loaded.Ok()) return false;
    Data = MoveTemp(Loaded.Catalogue);
    Shops.resize(Data.Shops.size());
    Bindings.resize(Data.Shops.size());
    for (size_t Index = 0; Index < Data.Shops.size(); ++Index)
    {
        const auto& Shop = Data.Shops[Index];
        Entries.insert(Entries.end(), Shop.Entries.begin(), Shop.Entries.end());
        auto& Row = Shops[Index];
        Row.Keepers.resize(Shop.Keepers.size());
        for (size_t KeeperIndex = 0; KeeperIndex < Shop.Keepers.size(); ++KeeperIndex)
        {
            const auto& Keeper = Shop.Keepers[KeeperIndex];
            auto& Target = Row.Keepers[KeeperIndex];
            for (const auto* Record : Db.OfType("npc_schedule"))
            {
                auto Schedule = PerfectWorld::LoadNpcSchedule(Db, Record->Id);
                if (Schedule.Ok() && Schedule.Value->Person.PersonId == Keeper.PersonId)
                {
                    Target.Schedule = MoveTemp(*Schedule.Value);
                    break;
                }
            }
            Target.Anchors.push_back(Keeper.CounterAnchor);
        }
        Bindings[Index].ShopId = Shop.Id;
        Bindings[Index].CounterAnchorId = Shop.CounterAnchor;
    }
    return true;
}

void FCmLiveContent::Refresh(const PerfectWorld::GameInstant Now, const UPWNpcPresenceSubsystem* Presence,
    const PerfectWorld::WorldSim* Sim)
{
    // R88: wallet text is evaluated every Slate frame; schedule bindings only change
    // with game time or registration. A load/reset passes no sim and always rebinds.
    bool RegistrationChanged = false;
    for (auto& Shop : Shops)
        for (auto& Keeper : Shop.Keepers)
        {
            const bool Registered = Presence != nullptr &&
                Presence->IsRegistered(PWLaneLife::ToName(Keeper.Schedule.Person.PersonId));
            RegistrationChanged |= Registered != Keeper.bR88Registered;
            Keeper.bR88Registered = Registered;
        }
    if (Sim != nullptr && R88RefreshedAt == Now && !RegistrationChanged) return;
    R88RefreshedAt = Now;
    for (size_t Index = 0; Index < Data.Shops.size(); ++Index)
    {
        const auto& Shop = Data.Shops[Index];
        auto& Binding = Bindings[Index];
        Binding.OpenMinute = 0;
        Binding.CloseMinute = PerfectWorld::IsCommerceShopOpen(Shop, Now) ? PerfectWorld::MinutesPerDay : 0;
        // R88: the reducer releases history by the keeper bound when service began.
        // Retain that identity through closure/breaks, including the close command.
        if (Sim != nullptr && Sim->CommerceService(Shop.Id).Requested)
        {
            for (auto& Keeper : Shops[Index].Keepers)
                if (Keeper.Schedule.Person.PersonId == Binding.KeeperId)
                {
                    Binding.Schedule = Keeper.bR88Registered ? &Keeper.Schedule.Person : nullptr;
                    Binding.Travel = Keeper.bR88Registered ? &Keeper.Schedule.Travel : nullptr;
                    break;
                }
            continue;
        }
        Binding.Schedule = nullptr;
        Binding.Travel = nullptr;
        Binding.KeeperId = {};
        Binding.KeeperAnchors = {};
        // R8-03 binds one daily window; R8-02 owns split weekly windows. Ask its portable query,
        // then bind an open/closed projection at this exact instant. No opening-hour rule lives here.
        for (auto& Keeper : Shops[Index].Keepers)
        {
            if (Keeper.Schedule.Person.PersonId.empty() || !Keeper.bR88Registered) continue;
            Binding.KeeperId = Keeper.Schedule.Person.PersonId;
            Binding.Schedule = &Keeper.Schedule.Person;
            Binding.Travel = &Keeper.Schedule.Travel;
            Binding.KeeperAnchors = Keeper.Anchors;
            // Prefer a keeper whose portable schedule currently places them at their counter.
            const auto Intent = PerfectWorld::ResolveNpcIntentAt(*Binding.Schedule, *Binding.Travel, Now, false, false);
            if (Intent.Ok() && Intent.Intent.State == PerfectWorld::NpcState::Work &&
                Intent.Intent.AnchorId == Keeper.Anchors.front()) break;
        }
    }
}

PerfectWorld::WorldCommerceCatalogues FCmLiveContent::Catalogues() const
{
    return {Data.Items, Entries, Bindings};
}

namespace
{
FVector R88CustomerPoint(const PerfectWorld::CommerceShop& Shop, const FNpLaneNpcBindings& Bindings, bool& Found)
{
    // Prefer published customer anchors. The shop content's counter is the keeper's work point.
    const FString Counter = PWLaneLife::ToFString(Shop.CounterAnchor);
    FString Customer = Counter;
    Customer.RemoveFromEnd(TEXT(".counter"));
    Customer += TEXT(".customer");
    if (const auto* Anchor = Bindings.FindAnchor(FName(*Customer)))
    {
        Found = Anchor->Kind == ENpBindingKind::Local;
        return Anchor->Feet.GetLocation() + FVector(0, 0, 110);
    }
    const auto* Anchor = Bindings.FindAnchor(PWLaneLife::ToName(Shop.CounterAnchor));
    Found = Anchor != nullptr && Anchor->Kind == ENpBindingKind::Local;
    // For a counter without a separate customer anchor, face the point across the counter from its keeper.
    return Found ? Anchor->Feet.GetLocation() + Anchor->Feet.GetRotation().GetForwardVector() * 140 + FVector(0, 0, 110)
        : FVector::ZeroVector;
}
}

void R88SpawnLiveCounters(UWorld& World)
{
    const auto* Instance = World.GetGameInstance();
    const auto* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    auto* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Content == nullptr || Content->GetDb() == nullptr || Presence == nullptr) return;
    const auto Loaded = PerfectWorld::LoadCommerceContent(*Content->GetDb());
    if (!Loaded.Ok()) return;
    TArray<FCmCounterSpec> Specs;
    for (const auto& Shop : Loaded.Catalogue.Shops)
    {
        bool Found = false;
        const FVector Point = R88CustomerPoint(Shop, Presence->Bindings(), Found);
        if (!Found)
        {
            UE_LOG(LogTemp, Warning, TEXT("R88 %hs: no registered counter anchor; commerce remains unavailable."), Shop.Id.c_str());
            continue;
        }
        FString Suffix = PWLaneLife::ToFString(Shop.Id);
        Suffix.RemoveFromStart(TEXT("shop."));
        Specs.Add({PWLaneLife::ToName(Shop.Id), FName(*(TEXT("interaction.nabapara.") + Suffix + TEXT(".buy"))), Point});
    }
    SpawnCommerceCounters(World, Specs);
}

std::string R88CounterApproach(UWorld& World, const FName ShopId)
{
    const auto* Controller = World.GetFirstPlayerController();
    const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
    if (Pawn == nullptr) return {};
    for (TActorIterator<APWCounterBuyActor> It(&World); It; ++It)
    {
        if (It->GetShopId() != ShopId) continue;
        const FVector Point = It->GetInteractionPoint();
        if (FVector::Dist2D(Pawn->GetActorLocation(), Point) > PWInteraction::ReachCm ||
            FMath::Abs(Pawn->GetActorLocation().Z - Point.Z) > PWInteraction::ReachCm) return {};
        FVector Eye; FRotator Facing;
        Pawn->GetActorEyesViewPoint(Eye, Facing);
        FCollisionQueryParams Params(SCENE_QUERY_STAT(R88CounterSight), false, Pawn);
        FHitResult Hit;
        if (World.LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params) &&
            FVector::Dist(Hit.ImpactPoint, Point) > PWInteraction::SightSlackCm) return {};
        auto* Life = UPWLaneLifeSubsystem::Get(&World);
        const auto* Sim = Life != nullptr ? Life->R88CommerceSim() : nullptr;
        return Sim != nullptr ? Sim->CommerceService(PWLaneLife::ToStd(ShopId)).CounterAnchorId : std::string();
    }
    return {};
}
} // namespace PWCommerce
