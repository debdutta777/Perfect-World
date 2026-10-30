// Mira Sen's complete spatial bindings (UPGRADE_PLAN UP-01). Her schedule (npc_schedule.mira_sen) names the packing desk, the
// shop's counter and front door, and the off-lane places home, school and mira_out. These tests run the real stationery provider
// (NpStationeryBindings) against the real file for every minute of a week and against the real built shop:
//  - EveryZoneAndRouteResolves: no zone, route or travel transition on any weekday resolves to a missing binding.
//  - ThresholdToDeskPaths: every journey that touches the lane is one connected polyline whose length is the authored length,
//    that is the exact reverse of the opposite journey, that begins/ends at the shop threshold, that crosses the partition
//    only through its doorway, and that keeps at least 34 cm from every collision box of the built shop.
//  - AninditaUnchanged: removing Mira's additions from the provider changes none of Anindita's results.
// Run in the editor under "PerfectWorld.MiraBindings" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Algo/Reverse.h"
#include "GameClock.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldLaneStationeryShop.h"

namespace PWMiraBindingsTest
{
constexpr double MbCapsuleRadiusCm = 34.0;
const FName MbDeskId(TEXT("anchor.stationery.packing_desk"));
const FName MbFrontDoorId(TEXT("anchor.stationery.front_door"));
const FName MbFootpathId(TEXT("location.nabapara.stationery_footpath"));

/** One week starting at the campaign's first 06:00 on a day boundary, the span the portable schedule validation uses. */
constexpr PerfectWorld::GameInstant MbWeekStart = 7 * PerfectWorld::MinutesPerDay - PerfectWorld::CampaignStartHour * PerfectWorld::MinutesPerHour;
constexpr int32 MbWeekMinutes = 7 * 1440;

bool MbLoadSchedule(FAutomationTestBase& T, const TCHAR* FileId, PerfectWorld::NpcSchedule& Out)
{
    FString Text;
    const FString Path = FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule") / (FString(FileId) + TEXT(".json"));
    if (!FFileHelper::LoadFileToString(Text, *Path))
    {
        T.AddError(FString::Printf(TEXT("cannot read %s"), *Path));
        return false;
    }
    const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value.has_value())
    {
        T.AddError(FString::Printf(TEXT("%s does not parse"), *Path));
        return false;
    }
    const std::string Key(TCHAR_TO_UTF8(FileId));
    PerfectWorld::ContentDb Db;
    Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Key);
    if (!Loaded.Ok())
    {
        T.AddError(FString::Printf(TEXT("%s does not load: %hs"), FileId, Loaded.Error.c_str()));
        return false;
    }
    Out = *Loaded.Value;
    return true;
}

/** The stationery building exactly as the lane builds it, its provider as the lane registers it, and the built shop's collision. */
struct FMbShop
{
    PWLane::FLaneBuilding Building;
    FNpBindingProvider Provider;
    FNpLaneNpcBindings Bindings;
    PWLane::FLaneScene Scene;

    bool Init(FAutomationTestBase& T)
    {
        Building.Frame = PWLane::MakeLaneFrame(-100.0, 460.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
        Building.Width = 700.0;
        Building.Depth = 700.0;
        Building.Storeys = 3;
        Building.Detail = PWLane::ELaneDetail::Full;
        Building.Wall = PWLane::ELaneColour::WallButter;
        Building.Side = 1;
        Building.Index = 2;
        Building.Setback = 60.0;
        Building.Seed = PWLane::StationeryGeneratorSeed;
        if (!T.TestTrue(TEXT("the reference building is the lane's stationery building"), PWLane::IsStationeryBuilding(Building))) { return false; }
        Provider = PWLane::NpStationeryBindings(Building);
        if (!T.TestTrue(TEXT("the provider registers"), Bindings.RegisterProvider(FName(TEXT("building.test.mira_stationery")), Provider))) { return false; }
        PWLane::BuildStationeryShop(Scene, Building);
        return T.TestTrue(TEXT("the shop's front door is built"), PWLane::HasStationeryShop(Scene));
    }

    FVector Local(const FVector& World) const
    {
        const FVector Offset = World - Building.Frame.Origin;
        return FVector(FVector::DotProduct(Offset, Building.Frame.U), FVector::DotProduct(Offset, Building.Frame.V), Offset.Z);
    }
};

PerfectWorld::NpcIntent MbTravelIntent(const PerfectWorld::FaridaTravelPath& Path, const bool bReverse, const int32 Permille)
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

double MbPolylineLength(const TArray<FVector>& Points)
{
    double Total = 0.0;
    for (int32 I = 0; I + 1 < Points.Num(); ++I) { Total += FVector::Dist2D(Points[I], Points[I + 1]); }
    return Total;
}

bool MbSamePoints(const TArray<FVector>& A, const TArray<FVector>& B)
{
    if (A.Num() != B.Num()) { return false; }
    for (int32 I = 0; I < A.Num(); ++I)
    {
        if (!A[I].Equals(B[I], 0.5)) { return false; }
    }
    return true;
}

/** Distance in the floor plane from an axis-aligned leg to a box (exact for legs parallel to the axes). */
double MbLegToBoxCm(const FVector& A, const FVector& B, const FBox& Box)
{
    const double Dx = FMath::Max3(0.0, FMath::Min(A.X, B.X) - Box.Max.X, Box.Min.X - FMath::Max(A.X, B.X));
    const double Dy = FMath::Max3(0.0, FMath::Min(A.Y, B.Y) - Box.Max.Y, Box.Min.Y - FMath::Max(A.Y, B.Y));
    return FMath::Sqrt(Dx * Dx + Dy * Dy);
}

bool MbSameResolved(const FNpResolvedBinding& A, const FNpResolvedBinding& B)
{
    return A.Kind == B.Kind && A.Feet.Equals(B.Feet, 0.01) && MbSamePoints(A.RoutePoints, B.RoutePoints) && A.LocalProgress == B.LocalProgress &&
           A.ExitAnchor == B.ExitAnchor && A.Diagnostic == B.Diagnostic && A.bPreferSit == B.bPreferSit;
}
} // namespace PWMiraBindingsTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiraBindingsResolveTest, "PerfectWorld.MiraBindings.EveryZoneAndRouteResolves",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMiraBindingsResolveTest::RunTest(const FString& Parameters)
{
    using namespace PWMiraBindingsTest;
    PerfectWorld::NpcSchedule Mira;
    FMbShop Shop;
    if (!MbLoadSchedule(*this, TEXT("npc_schedule.mira_sen"), Mira) || !Shop.Init(*this)) { return false; }

    // Static coverage: every zone the routine names, every endpoint of every route and travel leg, and every route.
    TSet<FName> Places;
    for (const auto& Entry : Mira.Person.Routine) { Places.Add(FName(UTF8_TO_TCHAR(Entry.Zone.c_str()))); }
    Places.Add(FName(UTF8_TO_TCHAR(Mira.Person.HomeZone.c_str())));
    for (const auto& Route : Mira.Routes)
    {
        Places.Add(FName(UTF8_TO_TCHAR(Route.From.c_str())));
        Places.Add(FName(UTF8_TO_TCHAR(Route.To.c_str())));
        TestNotNull(*FString::Printf(TEXT("route %hs is bound"), Route.Id.c_str()), Shop.Bindings.FindRoute(FName(UTF8_TO_TCHAR(Route.Id.c_str()))));
    }
    for (const auto& Path : Mira.Paths)
    {
        Places.Add(FName(UTF8_TO_TCHAR(Path.From.c_str())));
        Places.Add(FName(UTF8_TO_TCHAR(Path.To.c_str())));
    }
    for (const FName Place : Places)
    {
        TestNotNull(*FString::Printf(TEXT("%s is bound"), *Place.ToString()), Shop.Bindings.FindAnchor(Place));
    }
    for (const TCHAR* Off : {TEXT("location.nabapara.school"), TEXT("location.nabapara.mira_out"), TEXT("location.nabapara.sen_home")})
    {
        const FNpAnchorBinding* Anchor = Shop.Bindings.FindAnchor(FName(Off));
        TestTrue(*FString::Printf(TEXT("%s is off the lane and leaves it at the footpath anchor"), Off),
                 Anchor != nullptr && Anchor->Kind == ENpBindingKind::OffLane && Anchor->ExitAnchor == MbFootpathId);
    }
    for (const TCHAR* Off : {TEXT("route.mira_school.home_to_school"), TEXT("route.mira_school.door_to_school"), TEXT("route.mira_out.home_to_out"),
                             TEXT("route.mira_out.school_to_out"), TEXT("route.sen_home.footpath_to_home")})
    {
        const FNpRouteBinding* Route = Shop.Bindings.FindRoute(FName(Off));
        TestTrue(*FString::Printf(TEXT("%s is an off-lane segment"), Off), Route != nullptr && Route->bOffLane && Route->Points.IsEmpty());
    }
    const FNpAnchorBinding* Footpath = Shop.Bindings.FindAnchor(MbFootpathId);
    const FNpAnchorBinding* Door = Shop.Bindings.FindAnchor(MbFrontDoorId);
    const FNpAnchorBinding* Desk = Shop.Bindings.FindAnchor(MbDeskId);
    if (!TestTrue(TEXT("footpath, front door and packing desk are bound locally"),
                  Footpath != nullptr && Door != nullptr && Desk != nullptr && Footpath->Kind == ENpBindingKind::Local && Desk->Kind == ENpBindingKind::Local)) { return false; }
    TestTrue(TEXT("Mira enters and leaves at the shop threshold: the footpath anchor is the front door's position"), Footpath->Feet.Equals(Door->Feet, 0.01));

    // Dynamic coverage: every minute of every weekday.
    TSet<FString> Reported;
    TSet<int32> DeskDays;
    TSet<int32> OutDays;
    TSet<int32> SchoolDays;
    int32 LocalTravel = 0;
    int32 OffLaneTravel = 0;
    int32 DeskStays = 0;
    for (int32 Minute = 0; Minute < MbWeekMinutes; ++Minute)
    {
        const PerfectWorld::GameInstant At = MbWeekStart + Minute;
        const int32 Day = Minute / 1440;
        const auto Result = PerfectWorld::ResolveNpcIntentAt(Mira.Person, Mira.Travel, At, false, false);
        if (!Result.Ok())
        {
            const FString Key = FString::Printf(TEXT("no intent on day %d minute %d"), Day, Minute % 1440);
            if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
            continue;
        }
        const PerfectWorld::NpcIntent& Intent = Result.Intent;
        const FNpResolvedBinding Resolved = Shop.Bindings.Resolve(Mira, Intent);
        if (Resolved.Kind == ENpBindingKind::Missing)
        {
            const FString Key = FString::Printf(TEXT("day %d %02d:%02d: %s"), Day, (Minute % 1440) / 60, Minute % 60, *Resolved.Diagnostic);
            if (!Reported.Contains(Resolved.Diagnostic)) { Reported.Add(Resolved.Diagnostic); AddError(Key); }
            continue;
        }
        const bool bTravel = Intent.State == PerfectWorld::NpcState::Travel;
        if (bTravel)
        {
            (Resolved.Kind == ENpBindingKind::Local ? LocalTravel : OffLaneTravel) += 1;
            const auto Mentions = [&Intent](const char* Id) { return Intent.RouteId.find(Id) != std::string::npos; };
            if (Mentions("packing_desk")) { DeskDays.Add(Day); }
            if (Mentions("mira_out")) { OutDays.Add(Day); }
            if (Mentions("location.nabapara.school")) { SchoolDays.Add(Day); }
        }
        else if (Intent.AnchorId == "anchor.stationery.packing_desk")
        {
            ++DeskStays;
            if (Resolved.Kind != ENpBindingKind::Local || !Resolved.Feet.Equals(Desk->Feet, 0.01))
            {
                const FString Key = TEXT("packing desk stay is not at the packing desk anchor");
                if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
            }
        }
        else if (Resolved.Kind != ENpBindingKind::OffLane || Resolved.ExitAnchor != MbFootpathId)
        {
            const FString Key = FString::Printf(TEXT("%hs is not an off-lane stay leaving at the footpath"), Intent.AnchorId.c_str());
            if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
        }
    }
    // The sweep is not vacuous: the week really contains the journeys this file is about.
    TestEqual(TEXT("days with a trip to or from the packing desk (Tuesday and Thursday)"), DeskDays.Num(), 2);
    TestEqual(TEXT("days with a trip to or from mira_out (Monday, Wednesday, Friday, Saturday)"), OutDays.Num(), 4);
    TestEqual(TEXT("days with a trip to or from school (Monday to Friday)"), SchoolDays.Num(), 5);
    TestTrue(TEXT("some travel is visible on the lane and some is off it"), LocalTravel > 0 && OffLaneTravel > 0);
    TestTrue(TEXT("she packs at the desk on Tuesday and Thursday"), DeskStays > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiraBindingsPathsTest, "PerfectWorld.MiraBindings.ThresholdToDeskPaths",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMiraBindingsPathsTest::RunTest(const FString& Parameters)
{
    using namespace PWMiraBindingsTest;
    PerfectWorld::NpcSchedule Mira;
    FMbShop Shop;
    if (!MbLoadSchedule(*this, TEXT("npc_schedule.mira_sen"), Mira) || !Shop.Init(*this)) { return false; }
    const FNpAnchorBinding* Door = Shop.Bindings.FindAnchor(MbFrontDoorId);
    const FNpAnchorBinding* Desk = Shop.Bindings.FindAnchor(MbDeskId);
    const PWLane::LI::InteriorLayout& Layout = PWLane::GetStationeryLayout();
    const PWLane::LI::Opening* Doorway = nullptr;
    for (const PWLane::LI::Opening& Opening : Layout.Openings)
    {
        if (Opening.WallId.find("partition") != std::string::npos) { Doorway = &Opening; }
    }
    if (!TestTrue(TEXT("anchors and the partition doorway exist"), Door != nullptr && Desk != nullptr && Doorway != nullptr)) { return false; }
    const double FloorZ = Door->Feet.GetLocation().Z;
    TArray<FBox> Solids;
    for (const FBox& Box : Shop.Scene.Blocks.BlockingBoxes())
    {
        if (Box.Max.Z > FloorZ + 2.0 && Box.Min.Z < FloorZ + 165.0) { Solids.Add(Box); } // what a standing capsule can hit
    }
    TestTrue(TEXT("the built shop has collision for the partition and the desk"), Solids.Num() >= 3);

    int32 ThroughDoorway = 0;
    for (const auto& Path : Mira.Paths)
    {
        double AuthoredLocalCm = 0.0;
        for (const std::string& RouteId : Path.Routes)
        {
            const FNpRouteBinding* Segment = Shop.Bindings.FindRoute(FName(UTF8_TO_TCHAR(RouteId.c_str())));
            if (Segment == nullptr || Segment->bOffLane) { continue; }
            for (const auto& Def : Mira.Routes) { if (Def.Id == RouteId) { AuthoredLocalCm += Def.LengthCm; } }
        }
        TArray<FVector> Forward;
        for (const bool bReverse : {false, true})
        {
            const FString Name = FString::Printf(TEXT("%hs%hs->%hs"), bReverse ? "reversed " : "", (bReverse ? Path.To : Path.From).c_str(),
                                                 (bReverse ? Path.From : Path.To).c_str());
            TArray<FVector> Points;
            for (const int32 Permille : {0, 250, 500, 750, 1000})
            {
                const FNpResolvedBinding Resolved = Shop.Bindings.Resolve(Mira, MbTravelIntent(Path, bReverse, Permille));
                if (!TestTrue(*FString::Printf(TEXT("%s resolves at %d permille (%s)"), *Name, Permille, *Resolved.Diagnostic), Resolved.Kind != ENpBindingKind::Missing)) { return true; }
                Points = Resolved.RoutePoints;
            }
            if (Points.IsEmpty())
            {
                TestTrue(*FString::Printf(TEXT("%s is wholly off the lane, so no authored local length is charged"), *Name), AuthoredLocalCm == 0.0);
                continue;
            }
            // Connected: one polyline, no zero-length leg, and exactly the authored length.
            TestTrue(*FString::Printf(TEXT("%s is a polyline of at least two points"), *Name), Points.Num() >= 2);
            for (int32 I = 0; I + 1 < Points.Num(); ++I)
            {
                TestTrue(*FString::Printf(TEXT("%s leg %d has length"), *Name, I), FVector::Dist2D(Points[I], Points[I + 1]) > 1.0);
            }
            TestTrue(*FString::Printf(TEXT("%s is %.0f cm, the authored %.0f cm"), *Name, MbPolylineLength(Points), AuthoredLocalCm),
                     FMath::Abs(MbPolylineLength(Points) - AuthoredLocalCm) < 1.0);
            // It meets the lane at the threshold: a local endpoint is that anchor, an off-lane endpoint is the front door.
            const auto EndFor = [&](const std::string& Zone) -> FVector
            {
                const FNpAnchorBinding* Anchor = Shop.Bindings.FindAnchor(FName(UTF8_TO_TCHAR(Zone.c_str())));
                return Anchor != nullptr && Anchor->Kind == ENpBindingKind::Local ? Anchor->Feet.GetLocation() : Door->Feet.GetLocation();
            };
            TestTrue(*FString::Printf(TEXT("%s begins where its origin meets the lane"), *Name), Points[0].Equals(EndFor(bReverse ? Path.To : Path.From), 1.0));
            TestTrue(*FString::Printf(TEXT("%s ends where its destination meets the lane"), *Name), Points.Last().Equals(EndFor(bReverse ? Path.From : Path.To), 1.0));
            // Walls: only the partition doorway (U 568-662) may be crossed, and every leg keeps the capsule clear of every solid.
            int32 InBand = 0;
            for (int32 I = 0; I + 1 < Points.Num(); ++I)
            {
                for (const FBox& Box : Solids)
                {
                    const double Gap = MbLegToBoxCm(Points[I], Points[I + 1], Box);
                    if (Gap < MbCapsuleRadiusCm - 0.01)
                    {
                        AddError(FString::Printf(TEXT("%s leg %d comes within %.1f cm of a collision box (needs 34)"), *Name, I, Gap));
                    }
                }
                const double Length = FVector::Dist2D(Points[I], Points[I + 1]);
                for (double Along = 0.0; Along <= Length; Along += 1.0)
                {
                    const FVector Here = Shop.Local(FMath::Lerp(Points[I], Points[I + 1], Length > 0.0 ? Along / Length : 0.0));
                    if (Here.Y > Doorway->Plan.V0 + 0.5 && Here.Y < Doorway->Plan.V1 - 0.5)
                    {
                        ++InBand;
                        if (Here.X < Doorway->Plan.U0 + MbCapsuleRadiusCm - 0.01 || Here.X > Doorway->Plan.U1 - MbCapsuleRadiusCm + 0.01)
                        {
                            AddError(FString::Printf(TEXT("%s crosses the partition at U %.0f, outside the doorway U %d-%d with a capsule's margin"), *Name,
                                                     Here.X, Doorway->Plan.U0, Doorway->Plan.U1));
                            break;
                        }
                    }
                }
            }
            const FVector DeskLocal = Shop.Local(Desk->Feet.GetLocation());
            const bool bTouchesDesk = Points.ContainsByPredicate([&](const FVector& P) { return P.Equals(Desk->Feet.GetLocation(), 1.0); });
            if (bTouchesDesk)
            {
                TestTrue(*FString::Printf(TEXT("%s reaches the desk (U %.0f V %.0f) through the partition doorway"), *Name, DeskLocal.X, DeskLocal.Y), InBand > 0);
                ++ThroughDoorway;
            }
            if (!bReverse) { Forward = Points; }
            else
            {
                TArray<FVector> Back = Points;
                Algo::Reverse(Back);
                TestTrue(*FString::Printf(TEXT("%s is exactly the reverse of the opposite journey"), *Name), MbSamePoints(Forward, Back));
            }
        }
    }
    TestTrue(TEXT("both directions of the threshold-to-desk journeys were checked"), ThroughDoorway >= 8);

    // The visible arrival begins at the threshold and the departure ends there: walk the school <-> desk journey at every permille.
    PerfectWorld::FaridaTravelPath SchoolToDesk;
    SchoolToDesk.From = "location.nabapara.school";
    SchoolToDesk.To = "anchor.stationery.packing_desk";
    const FVector Threshold = Door->Feet.GetLocation();
    for (const bool bDeparture : {false, true})
    {
        const TCHAR* Name = bDeparture ? TEXT("desk to school") : TEXT("school to desk");
        int32 FirstLocal = -1;
        int32 LastLocal = -1;
        for (int32 Permille = 0; Permille <= 1000; ++Permille)
        {
            const FNpResolvedBinding Resolved = Shop.Bindings.Resolve(Mira, MbTravelIntent(SchoolToDesk, bDeparture, Permille));
            if (Resolved.Kind == ENpBindingKind::Missing)
            {
                AddError(FString::Printf(TEXT("%s went missing mid-walk: %s"), Name, *Resolved.Diagnostic));
                return true;
            }
            if (Resolved.Kind == ENpBindingKind::Local)
            {
                FirstLocal = FirstLocal < 0 ? Permille : FirstLocal;
                LastLocal = Permille;
            }
        }
        if (!TestTrue(*FString::Printf(TEXT("%s is visible on the lane for part of the walk"), Name), FirstLocal >= 0)) { continue; }
        const int32 Edge = bDeparture ? LastLocal : FirstLocal; // the permille at which she enters or leaves the lane
        const FNpResolvedBinding AtEdge = Shop.Bindings.Resolve(Mira, MbTravelIntent(SchoolToDesk, bDeparture, Edge));
        const double Off = FVector::Dist2D(AtEdge.Feet.GetLocation(), Threshold);
        TestTrue(*FString::Printf(TEXT("%s: she %s the lane at the shop threshold (%.1f cm off)"), Name, bDeparture ? TEXT("leaves") : TEXT("enters"), Off), Off <= 12.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMiraBindingsAninditaTest, "PerfectWorld.MiraBindings.AninditaUnchanged",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMiraBindingsAninditaTest::RunTest(const FString& Parameters)
{
    using namespace PWMiraBindingsTest;
    PerfectWorld::NpcSchedule Anindita;
    FMbShop Shop;
    if (!MbLoadSchedule(*this, TEXT("npc_schedule.anindita_sen"), Anindita) || !Shop.Init(*this)) { return false; }

    // The provider as it was before Mira: the same one with her school, mira_out and four off-lane legs taken away.
    FNpBindingProvider Before = Shop.Provider;
    for (const TCHAR* Id : {TEXT("location.nabapara.school"), TEXT("location.nabapara.mira_out")}) { Before.Anchors.Remove(FName(Id)); }
    for (const TCHAR* Id : {TEXT("route.mira_school.home_to_school"), TEXT("route.mira_school.door_to_school"), TEXT("route.mira_out.home_to_out"),
                            TEXT("route.mira_out.school_to_out")}) { Before.Routes.Remove(FName(Id)); }
    FNpLaneNpcBindings Old;
    if (!TestTrue(TEXT("the pre-Mira provider registers"), Old.RegisterProvider(FName(TEXT("building.test.old_stationery")), Before))) { return false; }
    TestEqual(TEXT("the new provider adds only Mira's two places"), Shop.Provider.Anchors.Num(), Before.Anchors.Num() + 2);
    TestEqual(TEXT("the new provider adds only Mira's four legs"), Shop.Provider.Routes.Num(), Before.Routes.Num() + 4);
    TestEqual(TEXT("doorway zones are untouched"), Shop.Provider.Doorways.Num(), Before.Doorways.Num());
    for (const TCHAR* Id : {TEXT("anchor.stationery.counter"), TEXT("anchor.stationery.stock_room"), TEXT("anchor.stationery.packing_desk"),
                            TEXT("anchor.stationery.front_door"), TEXT("anchor.stationery.customer"), TEXT("location.nabapara.stationery_footpath"),
                            TEXT("location.nabapara.sen_home")})
    {
        const FNpAnchorBinding* Now = Shop.Provider.Anchors.Find(FName(Id));
        const FNpAnchorBinding* Was = Before.Anchors.Find(FName(Id));
        TestTrue(*FString::Printf(TEXT("%s is bound and unchanged"), Id),
                 Now != nullptr && Was != nullptr && Now->Kind == Was->Kind && Now->Feet.Equals(Was->Feet, 0.01) && Now->ExitAnchor == Was->ExitAnchor &&
                     Now->bPreferSit == Was->bPreferSit);
    }

    TSet<FString> Reported;
    int32 Compared = 0;
    for (int32 Minute = 0; Minute < MbWeekMinutes; ++Minute)
    {
        const auto Result = PerfectWorld::ResolveNpcIntentAt(Anindita.Person, Anindita.Travel, MbWeekStart + Minute, false, false);
        if (!Result.Ok()) { continue; }
        const FNpResolvedBinding Now = Shop.Bindings.Resolve(Anindita, Result.Intent);
        const FNpResolvedBinding Was = Old.Resolve(Anindita, Result.Intent);
        ++Compared;
        if (Now.Kind == ENpBindingKind::Missing || !MbSameResolved(Now, Was))
        {
            const FString Key = FString::Printf(TEXT("%hs / %hs"), Result.Intent.AnchorId.c_str(), Result.Intent.RouteId.c_str());
            if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(FString::Printf(TEXT("Anindita's binding changed or is missing at minute %d: %s (%s)"), Minute, *Key, *Now.Diagnostic)); }
        }
    }
    TestTrue(TEXT("a full week of Anindita's intents was compared"), Compared > 10000);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
