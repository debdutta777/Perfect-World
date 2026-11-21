// Rupa Ghosh's complete spatial bindings (NR-01). Her schedule (npc_schedule.rupa_ghosh) names two off-lane places (her home behind
// the tea row house's own front door, and her workplace) and one route. These tests run the real provider (NpRupaBindings) against
// the real file for every minute of a week and against the real lane (rows, props and barriers built exactly as BuildKolkataLane
// builds them, no world needed):
//  - EveryZoneAndRouteResolves: no zone, route or travel transition on any weekday resolves to a missing binding; both places are
//    off-lane and leave the lane at a Local footpath anchor; every stay is where its binding says.
//  - RouteAndClearance: the walk is one connected polyline, the exact reverse of the walk home, never longer than the schedule's
//    length, meeting the presenter's threshold rule (UaResolveArrival / UaResolveDeparture), crossing walls only through the front
//    doorway, and staying at least 34 cm from every collision box of the built lane.
//  - HomeAndWork: the doorstep is 45 cm outside the door; the walk meets its authored 1540 cm exactly; the stand points and the
//    doorway zone's waiting spots stand clear.
//  - ProviderIsolation: the provider coexists with the care home's, the stationery's and the caretaker's, and refuses the wrong buildings.
// Run in the editor under "PerfectWorld.RupaBindings" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Algo/Reverse.h"
#include "GameClock.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "PerfectWorldLaneBarriers.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldLaneProps.h"
#include "PerfectWorldLaneStationeryShop.h"
#include "PerfectWorldRupaBindings.h"
#include "PerfectWorldSubrataBindings.h"

namespace PWRupaBindingsTest
{
constexpr double RgtCapsuleRadiusCm = 34.0;
constexpr PerfectWorld::GameInstant RgtWeekStart = 7 * PerfectWorld::MinutesPerDay - PerfectWorld::CampaignStartHour * PerfectWorld::MinutesPerHour;
constexpr int32 RgtWeekMinutes = 7 * 1440;

bool RgtLoadSchedule(FAutomationTestBase& T, PerfectWorld::NpcSchedule& Out)
{
    FString Text;
    const FString Path = FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule/npc_schedule.rupa_ghosh.json");
    if (!FFileHelper::LoadFileToString(Text, *Path)) { T.AddError(FString::Printf(TEXT("cannot read %s"), *Path)); return false; }
    const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value.has_value()) { T.AddError(FString::Printf(TEXT("%s does not parse"), *Path)); return false; }
    const std::string Key("npc_schedule.rupa_ghosh");
    PerfectWorld::ContentDb Db;
    Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Key);
    if (!Loaded.Ok()) { T.AddError(FString::Printf(TEXT("%s does not load: %hs"), *Path, Loaded.Error.c_str())); return false; }
    Out = *Loaded.Value;
    return true;
}

/** The lane exactly as BuildKolkataLane builds it (rows, props, barriers), built once: collision for every clearance check. */
const PWLane::FLaneScene& RgtLaneScene()
{
    static const TUniquePtr<PWLane::FLaneScene> Scene = []
    {
        TUniquePtr<PWLane::FLaneScene> Built = MakeUnique<PWLane::FLaneScene>();
        PWLane::BuildLaneRows(*Built);
        PWLane::BuildLaneProps(*Built);
        PWLane::BuildLaneBarriers(*Built);
        return Built;
    }();
    return *Scene;
}

struct FRgtFixture
{
    PWLane::FLaneBuilding RowHouse;
    FNpBindingProvider Provider;
    FNpLaneNpcBindings Bindings;

    bool Init(FAutomationTestBase& T)
    {
        if (!T.TestTrue(TEXT("the row house is the catalogue's kit frontage"), PWLane::MakeRupaBuilding(PWLane::RupaBuildingId, RowHouse))) { return false; }
        if (!T.TestTrue(TEXT("the lane built the row house's front door"), PWLane::HasKitDoor(RgtLaneScene(), PWLane::RupaDoorId))) { return false; }
        Provider = PWLane::NpRupaBindings(RowHouse);
        return T.TestFalse(TEXT("the provider is not empty"), Provider.Anchors.IsEmpty()) &&
               T.TestTrue(TEXT("the provider registers"), Bindings.RegisterProvider(FName(TEXT("building.test.rupa")), Provider));
    }

    /** Collision a standing capsule can hit when walking at FloorZ. */
    TArray<FBox> Solids(const double FloorZ) const
    {
        TArray<FBox> Out;
        for (const FBox& Box : RgtLaneScene().Blocks.BlockingBoxes())
        {
            if (Box.Max.Z > FloorZ + 2.0 && Box.Min.Z < FloorZ + 165.0) { Out.Add(Box); }
        }
        return Out;
    }

    FVector Local(const FVector& World) const
    {
        const FVector Offset = World - RowHouse.Frame.Origin;
        return FVector(FVector::DotProduct(Offset, RowHouse.Frame.U), FVector::DotProduct(Offset, RowHouse.Frame.V), Offset.Z);
    }
};

PerfectWorld::NpcIntent RgtTravelIntent(const PerfectWorld::FaridaTravelPath& Path, const bool bReverse, const int32 Permille)
{
    PerfectWorld::NpcIntent Intent;
    Intent.State = PerfectWorld::NpcState::Travel;
    const std::string& From = bReverse ? Path.To : Path.From;
    const std::string& To = bReverse ? Path.From : Path.To;
    Intent.AnchorId = To;
    Intent.RouteId = "route:" + From + "->" + To;
    Intent.ProgressPermille = Permille;
    Intent.ResumeState = Intent.State;
    return Intent;
}

double RgtLength2D(const TArray<FVector>& Points)
{
    double Total = 0.0;
    for (int32 I = 0; I + 1 < Points.Num(); ++I) { Total += FVector::Dist2D(Points[I], Points[I + 1]); }
    return Total;
}

/** Distance in the floor plane from an axis-aligned leg to a box (exact for legs parallel to the axes, which both of these are). */
double RgtLegToBoxCm(const FVector& A, const FVector& B, const FBox& Box)
{
    const double Dx = FMath::Max3(0.0, FMath::Min(A.X, B.X) - Box.Max.X, Box.Min.X - FMath::Max(A.X, B.X));
    const double Dy = FMath::Max3(0.0, FMath::Min(A.Y, B.Y) - Box.Max.Y, Box.Min.Y - FMath::Max(A.Y, B.Y));
    return FMath::Sqrt(Dx * Dx + Dy * Dy);
}

double RgtPointToSolidsCm(const FVector& P, const TArray<FBox>& Solids)
{
    double Best = TNumericLimits<double>::Max();
    for (const FBox& Box : Solids) { Best = FMath::Min(Best, RgtLegToBoxCm(P, P, Box)); }
    return Best;
}

FName RgtName(const std::string& Id) { return FName(UTF8_TO_TCHAR(Id.c_str())); }
} // namespace PWRupaBindingsTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRupaBindingsResolveTest, "PerfectWorld.RupaBindings.EveryZoneAndRouteResolves",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRupaBindingsResolveTest::RunTest(const FString& Parameters)
{
    using namespace PWRupaBindingsTest;
    PerfectWorld::NpcSchedule Rupa;
    FRgtFixture Fx;
    if (!RgtLoadSchedule(*this, Rupa) || !Fx.Init(*this)) { return false; }

    TSet<FName> Places;
    for (const auto& Entry : Rupa.Person.Routine) { Places.Add(RgtName(Entry.Zone)); }
    Places.Add(RgtName(Rupa.Person.HomeZone));
    for (const auto& Route : Rupa.Routes)
    {
        Places.Add(RgtName(Route.From));
        Places.Add(RgtName(Route.To));
        const FNpRouteBinding* Line = Fx.Bindings.FindRoute(RgtName(Route.Id));
        TestTrue(*FString::Printf(TEXT("route %hs is bound to a visible polyline"), Route.Id.c_str()), Line != nullptr && !Line->bOffLane && Line->Points.Num() >= 2);
    }
    for (const auto& Path : Rupa.Paths) { Places.Add(RgtName(Path.From)); Places.Add(RgtName(Path.To)); }
    for (const FName Place : Places) { TestNotNull(*FString::Printf(TEXT("%s is bound"), *Place.ToString()), Fx.Bindings.FindAnchor(Place)); }

    for (const TCHAR* Off : {PWLane::RupaPlaceHome, PWLane::RupaPlaceWork})
    {
        const FNpAnchorBinding* Place = Fx.Bindings.FindAnchor(FName(Off));
        const FNpAnchorBinding* Exit = Place != nullptr ? Fx.Bindings.FindAnchor(Place->ExitAnchor) : nullptr;
        TestTrue(*FString::Printf(TEXT("%s is off the lane and leaves it at a Local footpath anchor"), Off),
                 Place != nullptr && Place->Kind == ENpBindingKind::OffLane && Exit != nullptr && Exit->Kind == ENpBindingKind::Local);
    }

    // Every minute of the week: the resolved binding is never missing, and the stays are where the schedule says.
    TSet<FString> Reported;
    TMap<FString, int32> StaysBy;
    int32 LocalTravel = 0;
    for (int32 Minute = 0; Minute < RgtWeekMinutes; ++Minute)
    {
        const int32 Day = Minute / 1440;
        const auto Result = PerfectWorld::ResolveNpcIntentAt(Rupa.Person, Rupa.Travel, RgtWeekStart + Minute, false, false);
        if (!Result.Ok())
        {
            const FString Key = FString::Printf(TEXT("no intent on day %d minute %d"), Day, Minute % 1440);
            if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
            continue;
        }
        const PerfectWorld::NpcIntent& Intent = Result.Intent;
        const FNpResolvedBinding Resolved = Fx.Bindings.Resolve(Rupa, Intent);
        if (Resolved.Kind == ENpBindingKind::Missing)
        {
            if (!Reported.Contains(Resolved.Diagnostic)) { Reported.Add(Resolved.Diagnostic); AddError(FString::Printf(TEXT("day %d %02d:%02d: %s"), Day, (Minute % 1440) / 60, Minute % 60, *Resolved.Diagnostic)); }
            continue;
        }
        if (Intent.State == PerfectWorld::NpcState::Travel)
        {
            if (Resolved.Kind == ENpBindingKind::Local) { ++LocalTravel; }
            continue;
        }
        const FNpAnchorBinding* Stay = Fx.Bindings.FindAnchor(RgtName(Intent.AnchorId));
        if (!(Stay != nullptr && Resolved.Kind == ENpBindingKind::OffLane && Resolved.ExitAnchor == Stay->ExitAnchor))
        {
            const FString Key = FString::Printf(TEXT("%hs stay is not where its binding says"), Intent.AnchorId.c_str());
            if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
        }
        StaysBy.FindOrAdd(FString(UTF8_TO_TCHAR(Intent.AnchorId.c_str()))) += 1;
    }
    for (const TCHAR* Zone : {TEXT("location.nabapara.tea_row_house"), TEXT("location.nabapara.rupa_work")})
    {
        TestTrue(*FString::Printf(TEXT("the week contains stays at %s"), Zone), StaysBy.FindOrAdd(FString(Zone)) > 0);
    }
    TestTrue(TEXT("some travel is visible on the lane"), LocalTravel > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRupaBindingsRouteTest, "PerfectWorld.RupaBindings.RouteAndClearance",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRupaBindingsRouteTest::RunTest(const FString& Parameters)
{
    using namespace PWRupaBindingsTest;
    PerfectWorld::NpcSchedule Rupa;
    FRgtFixture Fx;
    if (!RgtLoadSchedule(*this, Rupa) || !Fx.Init(*this)) { return false; }
    const TArray<FBox> Solids = Fx.Solids(PWLane::GroundZ);
    TestTrue(TEXT("the lane has collision around the route (walls, props)"), Solids.Num() >= 10);

    int32 Journeys = 0;
    for (const auto& Path : Rupa.Paths)
    {
        TArray<FVector> Forward;
        for (const bool bReverse : {false, true})
        {
            const std::string& Source = bReverse ? Path.To : Path.From;
            const std::string& Dest = bReverse ? Path.From : Path.To;
            const FString Name = FString::Printf(TEXT("%hs->%hs"), Source.c_str(), Dest.c_str());
            TArray<FVector> Points;
            for (const int32 Permille : {0, 250, 500, 750, 1000})
            {
                const FNpResolvedBinding Resolved = Fx.Bindings.Resolve(Rupa, RgtTravelIntent(Path, bReverse, Permille));
                if (!TestTrue(*FString::Printf(TEXT("%s resolves at %d permille (%s)"), *Name, Permille, *Resolved.Diagnostic), Resolved.Kind != ENpBindingKind::Missing)) { return true; }
                Points = Resolved.RoutePoints;
            }
            ++Journeys;
            if (!TestTrue(*FString::Printf(TEXT("%s is a polyline of at least two points"), *Name), Points.Num() >= 2)) { continue; }
            double AuthoredCm = 0.0;
            for (const std::string& RouteId : Path.Routes) { for (const auto& Def : Rupa.Routes) { if (Def.Id == RouteId) { AuthoredCm += Def.LengthCm; } } }
            TestTrue(*FString::Printf(TEXT("%s is %.1f cm, not longer than the authored %.0f cm"), *Name, RgtLength2D(Points), AuthoredCm), RgtLength2D(Points) <= AuthoredCm + 1.0);

            // The presenter's threshold rule: an off-lane origin appears at its exit anchor, an off-lane destination is left by it.
            const FNpAnchorBinding* From = Fx.Bindings.FindAnchor(RgtName(Source));
            const FNpAnchorBinding* To = Fx.Bindings.FindAnchor(RgtName(Dest));
            if (!TestTrue(*FString::Printf(TEXT("%s has both ends bound"), *Name), From != nullptr && To != nullptr)) { continue; }
            const FNpAnchorBinding* FromEnd = From->Kind == ENpBindingKind::OffLane ? Fx.Bindings.FindAnchor(From->ExitAnchor) : From;
            const FNpAnchorBinding* ToEnd = To->Kind == ENpBindingKind::OffLane ? Fx.Bindings.FindAnchor(To->ExitAnchor) : To;
            if (!TestTrue(*FString::Printf(TEXT("%s has both ends on the lane"), *Name), FromEnd != nullptr && ToEnd != nullptr)) { continue; }
            TestTrue(*FString::Printf(TEXT("%s begins at the origin's lane end"), *Name), Points[0].Equals(FromEnd->Feet.GetLocation(), 1.0));
            TestTrue(*FString::Printf(TEXT("%s ends at the destination's lane end"), *Name), Points.Last().Equals(ToEnd->Feet.GetLocation(), 1.0));

            for (int32 I = 0; I + 1 < Points.Num(); ++I)
            {
                TestTrue(*FString::Printf(TEXT("%s leg %d has length"), *Name, I), FVector::Dist2D(Points[I], Points[I + 1]) > 1.0);
                for (const FBox& Box : Fx.Solids(FMath::Min(Points[I].Z, Points[I + 1].Z)))
                {
                    const double Gap = RgtLegToBoxCm(Points[I], Points[I + 1], Box);
                    if (Gap < RgtCapsuleRadiusCm - 0.01) { AddError(FString::Printf(TEXT("%s leg %d comes within %.1f cm of a collision box (needs 34)"), *Name, I, Gap)); }
                }
                // The front wall is crossed only by the doorway (the walk starts at the doorstep, outside it, so it never crosses).
                const double Length = FVector::Dist2D(Points[I], Points[I + 1]);
                for (double Along = 0.0; Along <= Length; Along += 1.0)
                {
                    const FVector L = Fx.Local(FMath::Lerp(Points[I], Points[I + 1], Length > 0.0 ? Along / Length : 0.0));
                    if (L.X >= -0.5 && L.X <= Fx.RowHouse.Width + 0.5 && L.Y > 0.5 && L.Y < PerfectWorld::LaneInterior::WallThicknessCm - 0.5)
                    {
                        AddError(FString::Printf(TEXT("%s enters the front wall at U %.0f"), *Name, L.X));
                        break;
                    }
                }
            }
            if (!bReverse) { Forward = Points; }
            else
            {
                TArray<FVector> Back = Points;
                Algo::Reverse(Back);
                bool bSame = Back.Num() == Forward.Num();
                for (int32 I = 0; bSame && I < Back.Num(); ++I) { bSame = Back[I].Equals(Forward[I], 0.5); }
                TestTrue(*FString::Printf(TEXT("%s is exactly the reverse of the opposite journey"), *Name), bSame);
            }
        }
    }
    TestTrue(TEXT("the journey was checked in both directions"), Journeys == static_cast<int32>(Rupa.Paths.size()) * 2 && Journeys == 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRupaBindingsHomeTest, "PerfectWorld.RupaBindings.HomeAndWork",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRupaBindingsHomeTest::RunTest(const FString& Parameters)
{
    using namespace PWRupaBindingsTest;
    FRgtFixture Fx;
    if (!Fx.Init(*this)) { return false; }
    const FNpRouteBinding* Leg = Fx.Bindings.FindRoute(FName(PWLane::RupaRouteHomeToWork));
    const FNpAnchorBinding* HomeFoot = Fx.Bindings.FindAnchor(FName(PWLane::RupaHomeFootpath));
    const FNpAnchorBinding* WorkFoot = Fx.Bindings.FindAnchor(FName(PWLane::RupaWorkFootpath));
    if (!TestTrue(TEXT("the route and both footpath anchors exist"), Leg && HomeFoot && WorkFoot)) { return false; }

    // Home is behind her own front door: the exit anchor is the doorstep, 45 cm outside the door, level with the footpath.
    const FVector Door = HomeFoot->Feet.GetLocation();
    const FVector DoorLocal = Fx.Local(Door);
    TestTrue(*FString::Printf(TEXT("the home exit is %.1f cm in front of the front face (local V %.1f, wanted %d)"), -DoorLocal.Y, DoorLocal.Y, PWLane::RupaDoorstepCm),
             FMath::Abs(DoorLocal.Y - PWLane::RupaDoorstepCm) < 1.0);
    TestTrue(TEXT("the walk begins at the doorstep and ends at the work threshold"),
             Leg->Points[0].Equals(Door, 1.0) && Leg->Points.Last().Equals(WorkFoot->Feet.GetLocation(), 1.0));
    TestTrue(*FString::Printf(TEXT("home_to_work is %.1f cm, the authored %.0f"), RgtLength2D(Leg->Points), PWLane::RupaHomeToWorkCm),
             FMath::Abs(RgtLength2D(Leg->Points) - PWLane::RupaHomeToWorkCm) < 1.0);
    TestTrue(*FString::Printf(TEXT("the work threshold is on the footpath line (Y %.1f)"), WorkFoot->Feet.GetLocation().Y),
             FMath::Abs(WorkFoot->Feet.GetLocation().Y - PWLane::RupaFootpathLineY) < 0.5 && WorkFoot->Feet.GetLocation().X < PWLane::LaneX1 - 200.0);

    const TArray<FBox> Solids = Fx.Solids(PWLane::GroundZ + PWLane::FootpathHeight);
    for (const FNpAnchorBinding* Anchor : {HomeFoot, WorkFoot})
    {
        const double Clear = RgtPointToSolidsCm(Anchor->Feet.GetLocation(), Solids);
        TestTrue(*FString::Printf(TEXT("a stand point (%s) is %.1f cm from the nearest solid"), *Anchor->Feet.GetLocation().ToString(), Clear), Clear >= RgtCapsuleRadiusCm - 0.01);
    }
    TestEqual(TEXT("one doorway zone (the row house's front door)"), Fx.Provider.Doorways.Num(), 1);
    for (const FNpDoorwayZone& Zone : Fx.Provider.Doorways)
    {
        TestTrue(*FString::Printf(TEXT("%s is the row house's door"), *Zone.Id.ToString()), Zone.Id == FName(PWLane::RupaDoorId));
        TestTrue(*FString::Printf(TEXT("%s has a valid volume and two waiting spots"), *Zone.Id.ToString()), Zone.Bounds.IsValid && Zone.WaitingFeet.Num() == 2);
        for (const FVector& Wait : Zone.WaitingFeet)
        {
            TestTrue(*FString::Printf(TEXT("waiting spot %s is clear of solids"), *Wait.ToString()), RgtPointToSolidsCm(Wait, Solids) >= RgtCapsuleRadiusCm - 0.01);
            TestFalse(*FString::Printf(TEXT("waiting spot %s is outside its own volume"), *Wait.ToString()), Zone.Bounds.IsInsideOrOn(Wait));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRupaBindingsIsolationTest, "PerfectWorld.RupaBindings.ProviderIsolation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRupaBindingsIsolationTest::RunTest(const FString& Parameters)
{
    using namespace PWRupaBindingsTest;
    FRgtFixture Fx;
    if (!Fx.Init(*this)) { return false; }

    PWLane::FLaneBuilding Stationery;
    Stationery.Frame = PWLane::MakeLaneFrame(-100.0, 460.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
    Stationery.Width = 700.0;
    Stationery.Depth = 700.0;
    Stationery.Storeys = 3;
    Stationery.Detail = PWLane::ELaneDetail::Full;
    Stationery.Wall = PWLane::ELaneColour::WallButter;
    Stationery.Side = 1;
    Stationery.Index = 2;
    Stationery.Setback = 60.0;
    Stationery.Seed = PWLane::StationeryGeneratorSeed;
    PWLane::FLaneBuilding Tall;
    PWLane::FLaneBuilding Caretaker;
    FNpLaneNpcBindings Together;
    TestTrue(TEXT("Rupa's provider registers"), Together.RegisterProvider(FName(TEXT("building.nabapara.tea_row_house")), Fx.Provider));
    TestTrue(TEXT("the stationery's provider registers beside it"), Together.RegisterProvider(FName(TEXT("building.nabapara.stationery")), PWLane::NpStationeryBindings(Stationery)));
    TestTrue(TEXT("the children's home's provider registers beside it"),
             Together.RegisterProvider(FName(TEXT("building.nabapara.care_home")), NpCareHomeBindings(PWLane::MakeLaneFrame(-1400.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0)))));
    if (PWLane::MakeSubrataBuilding(PWLane::SubrataTallFlatsId, Tall) && PWLane::MakeSubrataBuilding(PWLane::SubrataCaretakerFlatId, Caretaker))
    {
        TestTrue(TEXT("the caretaker's provider registers beside it (no anchor, route or doorway id is shared)"),
                 Together.RegisterProvider(FName(PWLane::SubrataProviderId), PWLane::NpSubrataBindings(Tall, Caretaker)));
    }
    TestFalse(TEXT("a second copy under another id is refused (ambiguous ownership)"), Together.RegisterProvider(FName(TEXT("building.test.again")), Fx.Provider));

    // Wrong or perturbed buildings give an empty provider, never a half-bound one.
    TestTrue(TEXT("the stationery is not the row house"), PWLane::NpRupaBindings(Stationery).Anchors.IsEmpty());
    PWLane::FLaneBuilding Narrow = Fx.RowHouse;
    Narrow.Width += 100.0;
    TestTrue(TEXT("a perturbed row house is refused"), PWLane::NpRupaBindings(Narrow).Anchors.IsEmpty());
    TestTrue(TEXT("a -Y kit row is not a Rupa building"), !PWLane::MakeRupaBuilding(PWLane::SubrataCaretakerFlatId, Narrow));

    const FNpBindingProvider Again = PWLane::NpRupaBindings(Fx.RowHouse);
    TestEqual(TEXT("same anchors"), Again.Anchors.Num(), Fx.Provider.Anchors.Num());
    const FNpRouteBinding* A = Again.Routes.Find(FName(PWLane::RupaRouteHomeToWork));
    const FNpRouteBinding* B = Fx.Provider.Routes.Find(FName(PWLane::RupaRouteHomeToWork));
    bool bSame = A != nullptr && B != nullptr && A->Points.Num() == B->Points.Num();
    for (int32 I = 0; bSame && I < A->Points.Num(); ++I) { bSame = A->Points[I].Equals(B->Points[I], 0.5); }
    TestTrue(TEXT("the route is identical on a rebuild"), bSame);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
