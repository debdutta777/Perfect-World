#include "PerfectWorldLaneLifeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FaridaSchedule.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldLaneTeaShop.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

bool UPWLaneLifeSubsystem::RegisterTeaShopPresence(const PWLane::FLaneBuilding& Building)
{
    auto* PbPresence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
    const auto* PbInstance = GetWorld()->GetGameInstance();
    const auto* PbContent = PbInstance != nullptr ? PbInstance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    if (PbPresence == nullptr || PbContent == nullptr || PbContent->GetDb() == nullptr) { return false; }
    const auto PbLoaded = PerfectWorld::LoadNpcSchedule(*PbContent->GetDb(), PerfectWorld::FaridaScheduleId);
    if (!PbLoaded.Ok()) { return false; }
    FNpBindingProvider PbProvider;
    TArray<PWLane::FTeaShopAnchor> PbAnchors;
    PWLane::GetTeaShopAnchors(Building, PbAnchors);
    for (const auto& PbAnchor : PbAnchors)
    {
        FNpAnchorBinding PbBinding;
        PbBinding.Feet = FTransform(FRotator(0.0, PbAnchor.WorldYawDegrees, 0.0), PbAnchor.World);
        PbBinding.bPreferSit = PbAnchor.Id == FName(TEXT("anchor.shanti_chai.rest"));
        PbProvider.Anchors.Add(PbAnchor.Id, PbBinding);
    }
    TArray<PWLane::FTeaShopRoute> PbSegments;
    PWLane::GetTeaShopRoutes(Building, PbSegments);
    for (const auto& PbSegment : PbSegments)
    {
        PbProvider.Routes.Add(PbSegment.Id, FNpRouteBinding{PbSegment.World, false});
    }
    const FName PbFootpath(TEXT("location.nabapara.shanti_chai_footpath"));
    const auto* PbFootLine = PbProvider.Routes.Find(FName(TEXT("route.shanti_chai.door_to_footpath")));
    if (PbFootLine == nullptr || PbFootLine->Points.IsEmpty()) { return false; }
    FNpAnchorBinding PbFootAnchor;
    PbFootAnchor.Feet = FTransform(PbFootLine->Points.Last());
    PbProvider.Anchors.Add(PbFootpath, PbFootAnchor);
    FNpAnchorBinding PbHome;
    PbHome.Kind = ENpBindingKind::OffLane;
    PbHome.ExitAnchor = PbFootpath;
    PbProvider.Anchors.Add(FName(TEXT("location.nabapara.farida_home")), PbHome);
    PbProvider.Routes.Add(FName(TEXT("route.farida_home.footpath_to_home")), FNpRouteBinding{{}, true});
    // Normalize the helpers' door connectors once, retaining each schedule segment's authored From -> To direction.
    for (const auto& PbDef : PbLoaded.Value->Routes)
    {
        auto* PbLine = PbProvider.Routes.Find(PWLaneLife::ToName(PbDef.Id));
        if (PbLine == nullptr) { return false; }
        if (PbLine->bOffLane) { continue; }
        const auto* PbStart = PbProvider.Anchors.Find(PWLaneLife::ToName(PbDef.From));
        const auto* PbEnd = PbProvider.Anchors.Find(PWLaneLife::ToName(PbDef.To));
        if (PbStart == nullptr || PbEnd == nullptr) { return false; }
        const int32 PbIndex = PbLine->Points.IndexOfByPredicate([PbStart](const FVector& PbPoint)
        {
            return PbPoint.Equals(PbStart->Feet.GetLocation(), 1.0);
        });
        if (PbIndex > 0) { PbLine->Points.RemoveAt(0, PbIndex); }
        else if (PbIndex == INDEX_NONE) { PbLine->Points.Insert(PbStart->Feet.GetLocation(), 0); }
        if (PbLine->Points.IsEmpty() || !PbLine->Points.Last().Equals(PbEnd->Feet.GetLocation(), 1.0))
        {
            PbLine->Points.Add(PbEnd->Feet.GetLocation());
        }
    }
    if (!PbPresence->Bindings().RegisterProvider(FName(TEXT("building.nabapara.shanti_chai")), MoveTemp(PbProvider))) { return false; }
    const bool PbRegistered = PbPresence->RegisterScheduledPerson(FName(TEXT("person.farida_qadri")),
        NSLOCTEXT("PerfectWorldLane", "FaridaName", "Farida"), EPWNpcOutfit::ShopProprietor);
    PbPresence->Reconcile(GetNow());
    return PbRegistered;
}
