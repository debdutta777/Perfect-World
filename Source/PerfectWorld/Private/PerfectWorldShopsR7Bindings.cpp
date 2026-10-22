#include "PerfectWorldShopsR7Bindings.h"

#include "Engine/World.h"
#include "NeighbourhoodPlan.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcPilotBridge.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldShopDisplayActor.h"

#include <string>
#include <string_view>

namespace PWLane
{
namespace
{
FName R74Name(const std::string_view Id) { return FName(UTF8_TO_TCHAR(std::string(Id).c_str())); }

/** "person.bimal_modak" -> "bimal_modak". */
std::string R74Stem(const std::string_view PersonId)
{
    constexpr std::string_view Prefix = "person.";
    return std::string(PersonId.substr(PersonId.starts_with(Prefix) ? Prefix.size() : 0));
}

FVector R74Point(const PerfectWorld::NeighbourhoodPoint& Point)
{
    return FVector(static_cast<double>(Point.X), static_cast<double>(Point.Y), static_cast<double>(Point.Z));
}

/** The staff face +Y: toward the customers on the para road side. */
FTransform R74Feet(const FVector& Location) { return FTransform(FRotator(0.0, 90.0, 0.0), Location); }

double R74Length(const TArray<FVector>& Points)
{
    double Cm = 0.0;
    for (int32 Index = 1; Index < Points.Num(); ++Index) { Cm += FVector::Dist(Points[Index - 1], Points[Index]); }
    return Cm;
}

FString R74Clock(const int32 Minute) { return FString::Printf(TEXT("%02d:%02d"), Minute / 60, Minute % 60); }

bool R74IsPitch(const PerfectWorld::ShopStaffR7& Staff) { return Staff.WorkplaceId.starts_with("pitch."); }

/** The plan's owner counter for a workplace (never the assistant's extra spot). */
const PerfectWorld::NeighbourhoodFeature* R74PlanCounter(const PerfectWorld::NeighbourhoodPlan& Plan, const PerfectWorld::ShopStaffR7& Staff)
{
    return PerfectWorld::FindNeighbourhoodFeature(Plan, Staff.PlanCounterId);
}

bool R74Refuse(const PerfectWorld::ShopStaffR7& Staff, const TCHAR* Why)
{
    UE_LOG(LogTemp, Error, TEXT("R74 shop bindings for %s refused: %s"), *R74Name(Staff.PersonId).ToString(), Why);
    return false;
}

bool R74AddStaff(FNpBindingProvider& Provider, const PerfectWorld::NeighbourhoodPlan& Plan, const PerfectWorld::ShopStaffR7& Staff)
{
    const auto* Counter = R74PlanCounter(Plan, Staff);
    const auto* Entrance = PerfectWorld::FindNeighbourhoodFeature(Plan, Staff.EntranceId);
    if (Counter == nullptr || Counter->Geometry.empty() || Entrance == nullptr || Entrance->Geometry.empty())
    {
        return R74Refuse(Staff, TEXT("the plan has no counter or entrance"));
    }
    const FVector PlanCounterPoint = R74Point(Counter->Geometry[0]);
    const FVector EntrancePoint = R74Point(Entrance->Geometry[0]);
    const FVector Feet = R74StaffFeet(Staff);
    // The owner stands on the plan's counter; the assistant stands exactly 150 cm east of it, on the same line.
    const FVector Expected = R74IsCounterOwner(Staff) ? PlanCounterPoint : PlanCounterPoint + FVector(150.0, 0.0, 0.0);
    if (!Feet.Equals(Expected, 0.5) || !FMath::IsNearlyEqual(EntrancePoint.X, PlanCounterPoint.X, 0.5) ||
        EntrancePoint.Y <= PlanCounterPoint.Y)
    {
        return R74Refuse(Staff, TEXT("the planned counter or entrance moved from the portable contract"));
    }
    FNpAnchorBinding CounterBinding;
    CounterBinding.Feet = R74Feet(Feet);
    Provider.Anchors.Add(R74Name(Staff.CounterId), CounterBinding);
    if (!Provider.Anchors.Contains(R74Name(Staff.EntranceId)))
    {
        FNpAnchorBinding EntranceBinding;
        EntranceBinding.Feet = R74Feet(EntrancePoint);
        Provider.Anchors.Add(R74Name(Staff.EntranceId), EntranceBinding);
    }
    FNpAnchorBinding Home;
    Home.Kind = ENpBindingKind::OffLane;
    Home.ExitAnchor = R74Name(Staff.EntranceId);
    Provider.Anchors.Add(R74Name(Staff.HomeId), Home);

    // Counter -> entrance: straight along X first when the staff member stands off the entrance line, then along +Y.
    FNpRouteBinding ToEntrance;
    ToEntrance.Points.Add(Feet);
    if (!FMath::IsNearlyEqual(Feet.X, EntrancePoint.X, 0.5)) { ToEntrance.Points.Add(FVector(EntrancePoint.X, Feet.Y, Feet.Z)); }
    ToEntrance.Points.Add(EntrancePoint);
    if (FMath::Abs(R74Length(ToEntrance.Points) - static_cast<double>(Staff.EntranceRouteCm)) > 1.0)
    {
        return R74Refuse(Staff, TEXT("the counter-to-entrance polyline differs from the schedule's frozen length"));
    }
    Provider.Routes.Add(R74CounterRouteId(Staff), MoveTemp(ToEntrance));
    // The 1680 cm home leg is the schedule's off-lane abstraction: no visible polyline.
    Provider.Routes.Add(R74HomeRouteId(Staff), FNpRouteBinding{{}, true});
    return true;
}
} // namespace

bool R74IsCounterOwner(const PerfectWorld::ShopStaffR7& Staff) { return Staff.CounterId == Staff.PlanCounterId; }

FName R74CounterRouteId(const PerfectWorld::ShopStaffR7& Staff)
{
    return R74Name("route.r74." + R74Stem(Staff.PersonId) + ".counter_entrance");
}

FName R74HomeRouteId(const PerfectWorld::ShopStaffR7& Staff)
{
    return R74Name("route.r74." + R74Stem(Staff.PersonId) + ".entrance_home");
}

FVector R74StaffFeet(const PerfectWorld::ShopStaffR7& Staff) { return R74Point(Staff.CounterPosition); }

FName R74BrowseTag(const PerfectWorld::ShopStaffR7& Staff)
{
    constexpr std::string_view Prefix = "entrance.nabapara.";
    const std::string_view Entrance = Staff.EntranceId;
    const std::string Stem(Entrance.starts_with(Prefix) ? Entrance.substr(Prefix.size()) : Entrance);
    return R74Name("interaction.nabapara." + Stem + ".browse");
}

FString R74PostedHoursText(const PerfectWorld::ShopStaffR7& Staff)
{
    TArray<FString> Parts;
    for (const auto& Window : Staff.Opening) { Parts.Add(R74Clock(Window.Open) + TEXT("-") + R74Clock(Window.Close)); }
    return FString::Join(Parts, TEXT(" and "));
}

FString R74BrowseNotice(const PerfectWorld::ShopStaffR7& Staff)
{
    const TCHAR* Display = TEXT("Goods are set out behind the counter.");
    if (Staff.WorkplaceId == "location.nabapara.sweet_shop") { Display = TEXT("Trays of sweets in the glass case; jars on the shelves behind."); }
    else if (Staff.WorkplaceId == "location.nabapara.pharmacy") { Display = TEXT("Boxes and strips in the glass counter; shelves of packs behind."); }
    else if (Staff.WorkplaceId == "pitch.nabapara.tea") { Display = TEXT("A coal stove and kettle, jars of leaf and rows of cups."); }
    else if (Staff.WorkplaceId == "pitch.nabapara.vendor") { Display = TEXT("Puffed rice, peanuts and spices set out for mixing."); }
    return FString(Display) + TEXT("\nHours ") + R74PostedHoursText(Staff) +
        TEXT(".\nPurchasing is unavailable: nothing can be ordered, bought or handed over yet.");
}

FVector R74BrowsePoint(const PerfectWorld::ShopStaffR7& Staff)
{
    // Shops: the counter's customer face lies about 120 cm in front of the staff spot. Pitches: about 95 cm.
    const double Front = R74IsPitch(Staff) ? 105.0 : 240.0;
    const FVector Feet = R74StaffFeet(Staff);
    const double OwnerX = R74IsCounterOwner(Staff) ? Feet.X : Feet.X - 150.0;
    return FVector(OwnerX, Feet.Y + Front, Feet.Z + 100.0);
}

FVector R74CustomerPoint(const PerfectWorld::ShopStaffR7& Staff)
{
    const FVector Feet = R74StaffFeet(Staff);
    // The assistant stands 150 cm east of the flap's axis: the customer point is the flap's east half, 120 cm west of her.
    const double CustomerX = R74IsCounterOwner(Staff) ? Feet.X : Feet.X - 120.0;
    return FVector(CustomerX, Feet.Y + (R74IsPitch(Staff) ? 120.0 : 130.0), Feet.Z);
}

FNpBindingProvider R74ShopBindings(const PerfectWorld::NeighbourhoodPlan& Plan, const FName WorkplaceId)
{
    FNpBindingProvider Provider;
    int32 Staffed = 0;
    for (const auto& Staff : PerfectWorld::ShopStaffR7Definitions())
    {
        if (R74Name(Staff.WorkplaceId) != WorkplaceId) { continue; }
        if (!R74AddStaff(Provider, Plan, Staff)) { return {}; }
        ++Staffed;
    }
    return Staffed > 0 ? Provider : FNpBindingProvider();
}

bool R74SpawnShopStaff(UWorld& World, const FLaneScene& Scene)
{
    (void)Scene;
    auto* Presence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (Presence == nullptr) { return false; }
    const auto Plan = PerfectWorld::BuildNeighbourhoodPlan(0);
    TArray<FName> Workplaces;
    for (const TCHAR* Id : {R74SweetShopId, R74PharmacyId, R74TeaId, R74VendorId}) { Workplaces.Add(FName(Id)); }
    // Build and check everything first so a refusal registers nothing.
    TArray<TPair<FName, FNpBindingProvider>> Providers;
    for (const FName Workplace : Workplaces)
    {
        FNpBindingProvider Provider = R74ShopBindings(Plan, Workplace);
        if (Provider.Anchors.IsEmpty() || Provider.Routes.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("R74: %s has no valid providers; the shop staff are not spawned."), *Workplace.ToString());
            return false;
        }
        Providers.Emplace(Workplace, MoveTemp(Provider));
    }
    for (const auto& Staff : PerfectWorld::ShopStaffR7Definitions())
    {
        if (!R55IsWorkPilot(R74Name(Staff.PersonId)))
        {
            UE_LOG(LogTemp, Error, TEXT("R74: %s is not a work pilot; extend R55IsWorkPilot with IsShopStaffR7."), *R74Name(Staff.PersonId).ToString());
            return false;
        }
    }
    for (auto& Entry : Providers)
    {
        if (!Presence->Bindings().RegisterProvider(Entry.Key, MoveTemp(Entry.Value))) { return false; }
    }
    for (const auto& Staff : PerfectWorld::ShopStaffR7Definitions())
    {
        if (!Presence->RegisterScheduledPerson(R74Name(Staff.PersonId), FText::FromString(UTF8_TO_TCHAR(std::string(Staff.DisplayName).c_str())),
                                               EPWNpcOutfit::ShopProprietor))
        {
            UE_LOG(LogTemp, Error, TEXT("R74: %s was not registered as a scheduled person."), *R74Name(Staff.PersonId).ToString());
            return false;
        }
    }
    if (const auto* Life = UPWLaneLifeSubsystem::Get(&World)) { Presence->Reconcile(Life->GetNow()); }
    return true;
}

int32 R74SpawnShopDisplays(UWorld& World, const FLaneScene& Scene)
{
    (void)Scene;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    int32 Placed = 0;
    for (const auto& Staff : PerfectWorld::ShopStaffR7Definitions())
    {
        if (!R74IsCounterOwner(Staff)) { continue; }
        const FVector Point = R74BrowsePoint(Staff);
        auto* Browse = World.SpawnActor<APWShopBrowseActor>(Point, FRotator::ZeroRotator, Params);
        if (Browse == nullptr) { continue; }
        Browse->Configure(R74Name(Staff.WorkplaceId), R74BrowseTag(Staff), FText::FromString(TEXT("Look at the display")),
                          FText::FromString(R74BrowseNotice(Staff)), Point);
        ++Placed;
    }
    return Placed;
}
} // namespace PWLane
