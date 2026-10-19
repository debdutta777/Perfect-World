// Subrata Dey's complete spatial bindings (UPGRADE_PLAN UP-03b). His schedule (npc_schedule.subrata_dey) names the shared entry of
// the tall flats (front door, noticeboard, stair), two off-lane places (his flat, behind its own front door, and the lane round)
// and four routes. These tests run the real provider (NpSubrataBindings) against the real file for every minute of a week and
// against the real lane (rows, props and barriers built exactly as BuildKolkataLane builds them, no world needed):
//  - EveryZoneAndRouteResolves: no zone, route or travel transition on any weekday resolves to a missing binding, and the
//    off-lane stays are the two places the presenter can appear at and leave by.
//  - RoutesAndClearance: every journey is one connected polyline, the exact reverse of the opposite journey, never longer than the
//    schedule's length, meeting the presenter's threshold rule (UaResolveArrival / UaResolveDeparture), crossing walls only through
//    the front doorway, and staying at least 34 cm from every collision box of the built lane.
//  - HomeAndLaneRound: the two long walks meet their authored lengths exactly; the passage legs are reported against the schedule's
//    500 cm ceilings; the anchors and waiting spots stand clear.
//  - ProviderIsolation: the provider coexists with the care home's and the stationery's, and refuses the wrong buildings.
// Run in the editor under "PerfectWorld.SubrataBindings" (Session Frontend, or the Unreal MCP AutomationTestToolset).

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
#include "PerfectWorldSubrataBindings.h"

namespace PWSubrataBindingsTest
{
constexpr double SbtCapsuleRadiusCm = 34.0;
constexpr PerfectWorld::GameInstant SbtWeekStart = 7 * PerfectWorld::MinutesPerDay - PerfectWorld::CampaignStartHour * PerfectWorld::MinutesPerHour;
constexpr int32 SbtWeekMinutes = 7 * 1440;

bool SbtLoadSchedule(FAutomationTestBase& T, PerfectWorld::NpcSchedule& Out)
{
    FString Text;
    const FString Path = FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule/npc_schedule.subrata_dey.json");
    if (!FFileHelper::LoadFileToString(Text, *Path)) { T.AddError(FString::Printf(TEXT("cannot read %s"), *Path)); return false; }
    const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value.has_value()) { T.AddError(FString::Printf(TEXT("%s does not parse"), *Path)); return false; }
    const std::string Key("npc_schedule.subrata_dey");
    PerfectWorld::ContentDb Db;
    Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Key);
    if (!Loaded.Ok()) { T.AddError(FString::Printf(TEXT("%s does not load: %hs"), *Path, Loaded.Error.c_str())); return false; }
    Out = *Loaded.Value;
    return true;
}

/** The lane exactly as BuildKolkataLane builds it (rows, props, barriers), built once: collision for every clearance check. */
const PWLane::FLaneScene& SbtLaneScene()
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

struct FSbtFixture
{
    PWLane::FLaneBuilding Tall;
    PWLane::FLaneBuilding Home;
    FNpBindingProvider Provider;
    FNpLaneNpcBindings Bindings;

    bool Init(FAutomationTestBase& T)
    {
        if (!T.TestTrue(TEXT("the tall flats are the catalogue's kit frontage"), PWLane::MakeSubrataBuilding(PWLane::SubrataTallFlatsId, Tall)) ||
            !T.TestTrue(TEXT("the caretaker's flat is the catalogue's kit frontage"), PWLane::MakeSubrataBuilding(PWLane::SubrataCaretakerFlatId, Home)))
        {
            return false;
        }
        const PWLane::FLaneScene& Scene = SbtLaneScene();
        if (!T.TestTrue(TEXT("the lane built both front doors"),
                        PWLane::HasKitDoor(Scene, PWLane::SubrataTallFlatsDoorId) && PWLane::HasKitDoor(Scene, PWLane::SubrataCaretakerDoorId)))
        {
            return false;
        }
        Provider = PWLane::NpSubrataBindings(Tall, Home);
        return T.TestFalse(TEXT("the provider is not empty"), Provider.Anchors.IsEmpty()) &&
               T.TestTrue(TEXT("the provider registers"), Bindings.RegisterProvider(FName(TEXT("building.test.subrata")), Provider));
    }

    /** Collision a standing capsule can hit when walking at FloorZ. */
    TArray<FBox> Solids(const double FloorZ) const
    {
        TArray<FBox> Out;
        for (const FBox& Box : SbtLaneScene().Blocks.BlockingBoxes())
        {
            if (Box.Max.Z > FloorZ + 2.0 && Box.Min.Z < FloorZ + 165.0) { Out.Add(Box); }
        }
        return Out;
    }

    static FVector Local(const PWLane::FLaneBuilding& B, const FVector& World)
    {
        const FVector Offset = World - B.Frame.Origin;
        return FVector(FVector::DotProduct(Offset, B.Frame.U), FVector::DotProduct(Offset, B.Frame.V), Offset.Z);
    }
};

PerfectWorld::NpcIntent SbtTravelIntent(const PerfectWorld::FaridaTravelPath& Path, const bool bReverse, const int32 Permille)
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

double SbtLength2D(const TArray<FVector>& Points)
{
    double Total = 0.0;
    for (int32 I = 0; I + 1 < Points.Num(); ++I) { Total += FVector::Dist2D(Points[I], Points[I + 1]); }
    return Total;
}

bool SbtSamePoints(const TArray<FVector>& A, const TArray<FVector>& B)
{
    if (A.Num() != B.Num()) { return false; }
    for (int32 I = 0; I < A.Num(); ++I) { if (!A[I].Equals(B[I], 0.5)) { return false; } }
    return true;
}

/** Distance in the floor plane from an axis-aligned leg to a box (exact for legs parallel to the axes, which all of these are). */
double SbtLegToBoxCm(const FVector& A, const FVector& B, const FBox& Box)
{
    const double Dx = FMath::Max3(0.0, FMath::Min(A.X, B.X) - Box.Max.X, Box.Min.X - FMath::Max(A.X, B.X));
    const double Dy = FMath::Max3(0.0, FMath::Min(A.Y, B.Y) - Box.Max.Y, Box.Min.Y - FMath::Max(A.Y, B.Y));
    return FMath::Sqrt(Dx * Dx + Dy * Dy);
}

double SbtPointToSolidsCm(const FVector& P, const TArray<FBox>& Solids)
{
    double Best = TNumericLimits<double>::Max();
    for (const FBox& Box : Solids) { Best = FMath::Min(Best, SbtLegToBoxCm(P, P, Box)); }
    return Best;
}

FName SbtName(const std::string& Id) { return FName(UTF8_TO_TCHAR(Id.c_str())); }
} // namespace PWSubrataBindingsTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubrataBindingsResolveTest, "PerfectWorld.SubrataBindings.EveryZoneAndRouteResolves",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSubrataBindingsResolveTest::RunTest(const FString& Parameters)
{
    using namespace PWSubrataBindingsTest;
    PerfectWorld::NpcSchedule Subrata;
    FSbtFixture Fx;
    if (!SbtLoadSchedule(*this, Subrata) || !Fx.Init(*this)) { return false; }

    TSet<FName> Places;
    for (const auto& Entry : Subrata.Person.Routine) { Places.Add(SbtName(Entry.Zone)); }
    Places.Add(SbtName(Subrata.Person.HomeZone));
    for (const auto& Route : Subrata.Routes)
    {
        Places.Add(SbtName(Route.From));
        Places.Add(SbtName(Route.To));
        const FNpRouteBinding* Line = Fx.Bindings.FindRoute(SbtName(Route.Id));
        TestTrue(*FString::Printf(TEXT("route %hs is bound to a visible polyline"), Route.Id.c_str()), Line != nullptr && !Line->bOffLane && Line->Points.Num() >= 2);
    }
    for (const auto& Path : Subrata.Paths) { Places.Add(SbtName(Path.From)); Places.Add(SbtName(Path.To)); }
    for (const FName Place : Places) { TestNotNull(*FString::Printf(TEXT("%s is bound"), *Place.ToString()), Fx.Bindings.FindAnchor(Place)); }

    for (const TCHAR* Off : {PWLane::SubrataPlaceHome, PWLane::SubrataPlaceLaneRound})
    {
        const FNpAnchorBinding* Place = Fx.Bindings.FindAnchor(FName(Off));
        const FNpAnchorBinding* Exit = Place != nullptr ? Fx.Bindings.FindAnchor(Place->ExitAnchor) : nullptr;
        TestTrue(*FString::Printf(TEXT("%s is off the lane and leaves it at a Local footpath anchor"), Off),
                 Place != nullptr && Place->Kind == ENpBindingKind::OffLane && Exit != nullptr && Exit->Kind == ENpBindingKind::Local);
    }
    for (const TCHAR* On : {PWLane::SubrataAnchorFrontDoor, PWLane::SubrataAnchorNoticeboard, PWLane::SubrataAnchorStair})
    {
        const FNpAnchorBinding* Anchor = Fx.Bindings.FindAnchor(FName(On));
        TestTrue(*FString::Printf(TEXT("%s is a Local stand point"), On), Anchor != nullptr && Anchor->Kind == ENpBindingKind::Local);
    }

    // Every minute of the week: the resolved binding is never missing, and the stays are where the schedule says.
    TSet<FString> Reported;
    TMap<FString, int32> StaysBy;
    int32 LocalTravel = 0;
    for (int32 Minute = 0; Minute < SbtWeekMinutes; ++Minute)
    {
        const int32 Day = Minute / 1440;
        const auto Result = PerfectWorld::ResolveNpcIntentAt(Subrata.Person, Subrata.Travel, SbtWeekStart + Minute, false, false);
        if (!Result.Ok())
        {
            const FString Key = FString::Printf(TEXT("no intent on day %d minute %d"), Day, Minute % 1440);
            if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
            continue;
        }
        const PerfectWorld::NpcIntent& Intent = Result.Intent;
        const FNpResolvedBinding Resolved = Fx.Bindings.Resolve(Subrata, Intent);
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
        const FNpAnchorBinding* Stay = Fx.Bindings.FindAnchor(SbtName(Intent.AnchorId));
        const bool bOffLaneStay = Intent.AnchorId == "location.nabapara.caretaker_flat" || Intent.AnchorId == "location.nabapara.lane_round";
        const bool bOk = Stay != nullptr && (bOffLaneStay ? Resolved.Kind == ENpBindingKind::OffLane && Resolved.ExitAnchor == Stay->ExitAnchor
                                                           : Resolved.Kind == ENpBindingKind::Local && Resolved.Feet.Equals(Stay->Feet, 0.01));
        if (!bOk)
        {
            const FString Key = FString::Printf(TEXT("%hs stay is not where its binding says"), Intent.AnchorId.c_str());
            if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
        }
        StaysBy.FindOrAdd(FString(UTF8_TO_TCHAR(Intent.AnchorId.c_str()))) += 1;
    }
    // The sweep is not vacuous: the week really contains every kind of stay and visible travel.
    for (const TCHAR* Zone : {TEXT("location.nabapara.caretaker_flat"), TEXT("location.nabapara.lane_round"), TEXT("anchor.tall_flats.front_door"),
                              TEXT("anchor.tall_flats.noticeboard"), TEXT("anchor.tall_flats.stair")})
    {
        TestTrue(*FString::Printf(TEXT("the week contains stays at %s"), Zone), StaysBy.FindOrAdd(FString(Zone)) > 0);
    }
    TestTrue(TEXT("some travel is visible on the lane"), LocalTravel > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubrataBindingsRoutesTest, "PerfectWorld.SubrataBindings.RoutesAndClearance",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSubrataBindingsRoutesTest::RunTest(const FString& Parameters)
{
    using namespace PWSubrataBindingsTest;
    PerfectWorld::NpcSchedule Subrata;
    FSbtFixture Fx;
    if (!SbtLoadSchedule(*this, Subrata) || !Fx.Init(*this)) { return false; }
    const TArray<FBox> Solids = Fx.Solids(PWLane::GroundZ); // road level: the most boxes a capsule could meet
    TestTrue(TEXT("the lane has collision around the passage (walls and partitions)"), Solids.Num() >= 10);

    const FSbtFixture& Cx = Fx;
    int32 Journeys = 0;
    for (const auto& Path : Subrata.Paths)
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
                const FNpResolvedBinding Resolved = Cx.Bindings.Resolve(Subrata, SbtTravelIntent(Path, bReverse, Permille));
                if (!TestTrue(*FString::Printf(TEXT("%s resolves at %d permille (%s)"), *Name, Permille, *Resolved.Diagnostic), Resolved.Kind != ENpBindingKind::Missing)) { return true; }
                Points = Resolved.RoutePoints;
            }
            ++Journeys;
            if (!TestTrue(*FString::Printf(TEXT("%s is a polyline of at least two points"), *Name), Points.Num() >= 2)) { continue; }
            double AuthoredCm = 0.0;
            for (const std::string& RouteId : Path.Routes) { for (const auto& Def : Subrata.Routes) { if (Def.Id == RouteId) { AuthoredCm += Def.LengthCm; } } }
            for (int32 I = 0; I + 1 < Points.Num(); ++I)
            {
                TestTrue(*FString::Printf(TEXT("%s leg %d has length"), *Name, I), FVector::Dist2D(Points[I], Points[I + 1]) > 1.0);
            }
            // The presenter paces a walk at 140 cm per game minute: a polyline longer than the authored length could never be kept up with.
            TestTrue(*FString::Printf(TEXT("%s is %.1f cm, not longer than the authored %.0f cm"), *Name, SbtLength2D(Points), AuthoredCm), SbtLength2D(Points) <= AuthoredCm + 1.0);

            // The presenter's threshold rule: an off-lane origin appears at its exit anchor, an off-lane destination is left by it.
            const FNpAnchorBinding* From = Cx.Bindings.FindAnchor(SbtName(Source));
            const FNpAnchorBinding* To = Cx.Bindings.FindAnchor(SbtName(Dest));
            if (!TestTrue(*FString::Printf(TEXT("%s has both ends bound"), *Name), From != nullptr && To != nullptr)) { continue; }
            const FNpAnchorBinding* FromEnd = From->Kind == ENpBindingKind::OffLane ? Cx.Bindings.FindAnchor(From->ExitAnchor) : From;
            const FNpAnchorBinding* ToEnd = To->Kind == ENpBindingKind::OffLane ? Cx.Bindings.FindAnchor(To->ExitAnchor) : To;
            if (!TestTrue(*FString::Printf(TEXT("%s has both ends on the lane"), *Name), FromEnd != nullptr && ToEnd != nullptr)) { continue; }
            TestTrue(*FString::Printf(TEXT("%s begins at the origin's lane end"), *Name), Points[0].Equals(FromEnd->Feet.GetLocation(), 1.0));
            TestTrue(*FString::Printf(TEXT("%s ends at the destination's lane end"), *Name), Points.Last().Equals(ToEnd->Feet.GetLocation(), 1.0));

            // Walls and props: every leg keeps a capsule's radius from every collision box, and the front walls are crossed only by the doorway.
            for (int32 I = 0; I + 1 < Points.Num(); ++I)
            {
                const TArray<FBox> LegSolids = Cx.Solids(FMath::Min(Points[I].Z, Points[I + 1].Z));
                for (const FBox& Box : LegSolids)
                {
                    const double Gap = SbtLegToBoxCm(Points[I], Points[I + 1], Box);
                    if (Gap < SbtCapsuleRadiusCm - 0.01) { AddError(FString::Printf(TEXT("%s leg %d comes within %.1f cm of a collision box (needs 34)"), *Name, I, Gap)); }
                }
                const double Length = FVector::Dist2D(Points[I], Points[I + 1]);
                for (double Along = 0.0; Along <= Length; Along += 1.0)
                {
                    const FVector Here = FMath::Lerp(Points[I], Points[I + 1], Length > 0.0 ? Along / Length : 0.0);
                    for (const PWLane::FLaneBuilding* B : {&Cx.Tall, &Cx.Home})
                    {
                        const FVector L = FSbtFixture::Local(*B, Here);
                        if (L.X < -0.5 || L.X > B->Width + 0.5 || L.Y <= 0.5 || L.Y >= PerfectWorld::LaneInterior::WallThicknessCm - 0.5) { continue; }
                        const double Centre = B == &Cx.Tall ? 550.0 : -1000.0; // the flat's door is never crossed by a route
                        if (FMath::Abs(L.X - Centre) > PerfectWorld::LaneInterior::DoorClearWidthCm * 0.5 - SbtCapsuleRadiusCm + 0.01)
                        {
                            AddError(FString::Printf(TEXT("%s crosses a front wall at U %.0f, outside the doorway with a capsule's margin"), *Name, L.X));
                            break;
                        }
                    }
                }
            }
            if (!bReverse) { Forward = Points; }
            else
            {
                TArray<FVector> Back = Points;
                Algo::Reverse(Back);
                TestTrue(*FString::Printf(TEXT("%s is exactly the reverse of the opposite journey"), *Name), SbtSamePoints(Forward, Back));
            }
        }
    }
    TestTrue(TEXT("every schedule journey was checked in both directions"), Journeys == static_cast<int32>(Subrata.Paths.size()) * 2 && Journeys >= 20);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubrataBindingsHomeTest, "PerfectWorld.SubrataBindings.HomeAndLaneRound",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSubrataBindingsHomeTest::RunTest(const FString& Parameters)
{
    using namespace PWSubrataBindingsTest;
    PerfectWorld::NpcSchedule Subrata;
    FSbtFixture Fx;
    if (!SbtLoadSchedule(*this, Subrata) || !Fx.Init(*this)) { return false; }

    const FNpRouteBinding* HomeLeg = Fx.Bindings.FindRoute(FName(PWLane::SubrataRouteHomeToLaneRound));
    const FNpRouteBinding* EntryLeg = Fx.Bindings.FindRoute(FName(PWLane::SubrataRouteLaneRoundToEntry));
    const FNpRouteBinding* BoardLeg = Fx.Bindings.FindRoute(FName(PWLane::SubrataRouteEntryToNoticeboard));
    const FNpRouteBinding* StairLeg = Fx.Bindings.FindRoute(FName(PWLane::SubrataRouteNoticeboardToStair));
    const FNpAnchorBinding* HomeFoot = Fx.Bindings.FindAnchor(FName(PWLane::SubrataHomeFootpath));
    const FNpAnchorBinding* RoundFoot = Fx.Bindings.FindAnchor(FName(PWLane::SubrataLaneRoundFootpath));
    const FNpAnchorBinding* Door = Fx.Bindings.FindAnchor(FName(PWLane::SubrataAnchorFrontDoor));
    const FNpAnchorBinding* Board = Fx.Bindings.FindAnchor(FName(PWLane::SubrataAnchorNoticeboard));
    const FNpAnchorBinding* Stair = Fx.Bindings.FindAnchor(FName(PWLane::SubrataAnchorStair));
    if (!TestTrue(TEXT("all routes and anchors exist"), HomeLeg && EntryLeg && BoardLeg && StairLeg && HomeFoot && RoundFoot && Door && Board && Stair)) { return false; }

    // Home is behind his own front door: the exit anchor is the flat's doorstep, 45 cm outside the door, level with the footpath.
    const PWLane::FLaneBuilding& Home = Fx.Home;
    const FVector Step = Home.Frame.Pt(250.0, PWLane::SubrataDoorstepCm, PWLane::FootpathHeight);
    TestTrue(*FString::Printf(TEXT("the home exit is the caretaker flat's doorstep (%s vs %s)"), *HomeFoot->Feet.GetLocation().ToString(), *Step.ToString()),
             HomeFoot->Feet.GetLocation().Equals(Step, 1.0));
    TestTrue(TEXT("the home leg begins at the doorstep and ends at the lane-round threshold"),
             HomeLeg->Points[0].Equals(HomeFoot->Feet.GetLocation(), 1.0) && HomeLeg->Points.Last().Equals(RoundFoot->Feet.GetLocation(), 1.0));
    TestTrue(TEXT("the entry leg begins at the lane-round threshold and ends at the tall flats' doorstep"),
             EntryLeg->Points[0].Equals(RoundFoot->Feet.GetLocation(), 1.0) && EntryLeg->Points.Last().Equals(Door->Feet.GetLocation(), 1.0));

    // The lengths. The long walks are met exactly; the passage legs are reported against the schedule's ceilings.
    TestTrue(*FString::Printf(TEXT("home_to_lane_round is %.1f cm, the authored %.0f"), SbtLength2D(HomeLeg->Points), PWLane::SubrataHomeToLaneRoundCm),
             FMath::Abs(SbtLength2D(HomeLeg->Points) - PWLane::SubrataHomeToLaneRoundCm) < 1.0);
    TestTrue(*FString::Printf(TEXT("lane_round_to_entry is %.1f cm, the authored %.0f"), SbtLength2D(EntryLeg->Points), PWLane::SubrataLaneRoundToEntryCm),
             FMath::Abs(SbtLength2D(EntryLeg->Points) - PWLane::SubrataLaneRoundToEntryCm) < 1.0);
    struct FCeiling { const TCHAR* Name; const FNpRouteBinding* Leg; double Authored; };
    for (const FCeiling& Row : {FCeiling{PWLane::SubrataRouteEntryToNoticeboard, BoardLeg, PWLane::SubrataEntryToNoticeboardCm},
                                FCeiling{PWLane::SubrataRouteNoticeboardToStair, StairLeg, PWLane::SubrataNoticeboardToStairCm}})
    {
        const double Built = SbtLength2D(Row.Leg->Points);
        TestTrue(*FString::Printf(TEXT("%s is %.1f cm, within the schedule's %.0f cm ceiling"), Row.Name, Built, Row.Authored), Built > 100.0 && Built <= Row.Authored + 1.0);
        if (Built < Row.Authored - 1.0)
        {
            AddWarning(FString::Printf(TEXT("%s is %.1f cm against the schedule's %.0f cm: the walk is paced at %.0f%% of 140 cm/min"), Row.Name, Built, Row.Authored, 100.0 * Built / Row.Authored));
        }
    }

    // Standing and waiting spots stand clear of every solid.
    const TArray<FBox> Solids = Fx.Solids(PWLane::GroundZ + PWLane::FootpathHeight);
    for (const FNpAnchorBinding* Anchor : {HomeFoot, RoundFoot, Door, Board, Stair})
    {
        const double Clear = SbtPointToSolidsCm(Anchor->Feet.GetLocation(), Solids);
        TestTrue(*FString::Printf(TEXT("a stand point (%s) is %.1f cm from the nearest solid"), *Anchor->Feet.GetLocation().ToString(), Clear), Clear >= SbtCapsuleRadiusCm - 0.01);
    }
    TestEqual(TEXT("two doorway zones (the tall flats' and the caretaker's front doors)"), Fx.Provider.Doorways.Num(), 2);
    for (const FNpDoorwayZone& Zone : Fx.Provider.Doorways)
    {
        TestTrue(*FString::Printf(TEXT("%s has a valid volume and two waiting spots"), *Zone.Id.ToString()), Zone.Bounds.IsValid && Zone.WaitingFeet.Num() == 2);
        for (const FVector& Wait : Zone.WaitingFeet)
        {
            TestTrue(*FString::Printf(TEXT("%s waiting spot %s is clear of solids"), *Zone.Id.ToString(), *Wait.ToString()), SbtPointToSolidsCm(Wait, Solids) >= SbtCapsuleRadiusCm - 0.01);
            TestFalse(*FString::Printf(TEXT("%s waiting spot %s is outside its own volume"), *Zone.Id.ToString(), *Wait.ToString()), Zone.Bounds.IsInsideOrOn(Wait));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSubrataBindingsIsolationTest, "PerfectWorld.SubrataBindings.ProviderIsolation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSubrataBindingsIsolationTest::RunTest(const FString& Parameters)
{
    using namespace PWSubrataBindingsTest;
    FSbtFixture Fx;
    if (!Fx.Init(*this)) { return false; }

    // Every other building's provider registers beside it (no anchor, route or doorway id is shared).
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
    FNpLaneNpcBindings Together;
    TestTrue(TEXT("Subrata's provider registers"), Together.RegisterProvider(FName(TEXT("building.nabapara.tall_flats")), Fx.Provider));
    TestTrue(TEXT("the stationery's provider registers beside it"), Together.RegisterProvider(FName(TEXT("building.nabapara.stationery")), PWLane::NpStationeryBindings(Stationery)));
    TestTrue(TEXT("the children's home's provider registers beside it"),
             Together.RegisterProvider(FName(TEXT("building.nabapara.care_home")), NpCareHomeBindings(PWLane::MakeLaneFrame(-1400.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0)))));
    TestFalse(TEXT("a second copy under another id is refused (ambiguous ownership)"), Together.RegisterProvider(FName(TEXT("building.test.again")), Fx.Provider));

    // Wrong or perturbed buildings give an empty provider, never a half-bound one.
    TestTrue(TEXT("swapped buildings are refused"), PWLane::NpSubrataBindings(Fx.Home, Fx.Tall).Anchors.IsEmpty());
    PWLane::FLaneBuilding Narrow = Fx.Tall;
    Narrow.Width += 100.0;
    TestTrue(TEXT("a perturbed tall flats is refused"), PWLane::NpSubrataBindings(Narrow, Fx.Home).Anchors.IsEmpty());
    TestTrue(TEXT("the stationery is not a Subrata building"), !PWLane::MakeSubrataBuilding(TEXT("building.nabapara.stationery"), Narrow));

    // Deterministic: a second build is identical.
    const FNpBindingProvider Again = PWLane::NpSubrataBindings(Fx.Tall, Fx.Home);
    TestEqual(TEXT("same anchors"), Again.Anchors.Num(), Fx.Provider.Anchors.Num());
    TestEqual(TEXT("same routes"), Again.Routes.Num(), Fx.Provider.Routes.Num());
    for (const auto& Route : Fx.Provider.Routes)
    {
        const FNpRouteBinding* Other = Again.Routes.Find(Route.Key);
        TestTrue(*FString::Printf(TEXT("%s is identical on a rebuild"), *Route.Key.ToString()), Other != nullptr && SbtSamePoints(Other->Points, Route.Value.Points));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
