#include "PerfectWorldAmbientBindings.h"

#include "Engine/World.h"
#include "LaneAmbient.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldVendorCart.h"

namespace
{
FName AmAmbientName(std::string_view Id) { return FName(UTF8_TO_TCHAR(std::string(Id).c_str())); }
FVector AmAmbientWorld(const PerfectWorld::AmbientPoint& P)
{
    return FVector(P.X, P.Y, PWLane::GroundZ + P.Height);
}
void AmAmbientAnchor(FNpBindingProvider& Provider, std::string_view Id, const FVector& Point)
{
    FNpAnchorBinding Anchor;
    Anchor.Feet = FTransform(Point);
    if (Id.starts_with("location."))
    {
        const FName Portal = AmAmbientName(std::string(Id) + ".threshold");
        Provider.Anchors.Add(Portal, Anchor);
        Anchor.Kind = ENpBindingKind::OffLane;
        Anchor.ExitAnchor = Portal;
    }
    Provider.Anchors.Add(AmAmbientName(Id), Anchor);
}
}
namespace PWLane
{
FNpBindingProvider NpAmbientBindings()
{
    FNpBindingProvider Provider;
    for (const auto& Route : PerfectWorld::LaneAmbientRoutes())
    {
        TArray<FVector> Points;
        for (std::size_t I = 0; I < Route.Count; ++I) { Points.Add(AmAmbientWorld(Route.Points[I])); }
        AmAmbientAnchor(Provider, Route.From, Points[0]);
        AmAmbientAnchor(Provider, Route.To, Points.Last());
        Provider.Routes.Add(AmAmbientName(Route.Id), FNpRouteBinding{MoveTemp(Points), false});
    }
    return Provider;
}
bool SpawnAmbientLife(UWorld& World)
{
    auto* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Presence == nullptr || !Presence->Bindings().RegisterProvider(FName(TEXT("provider.ambient_lane")), NpAmbientBindings())) { return false; }
    const FText Names[] = {NSLOCTEXT("PerfectWorldLane", "AmbientPurnima", "Purnima Das"),
        NSLOCTEXT("PerfectWorldLane", "AmbientArun", "Arun Paul"), NSLOCTEXT("PerfectWorldLane", "AmbientRekha", "Rekha Bose")};
    for (std::size_t I = 0; I < PerfectWorld::AmbientPeople.size(); ++I)
    {
        if (!Presence->RegisterScheduledPerson(AmAmbientName(PerfectWorld::AmbientPeople[I]), Names[I], EPWNpcOutfit::HouseholdAdult)) { return false; }
    }
    if (APWVendorCart::GetOrSpawn(World) == nullptr) { return false; }
    if (const auto* Life = UPWLaneLifeSubsystem::Get(&World)) { Presence->Reconcile(Life->GetNow()); }
    return true;
}
}
