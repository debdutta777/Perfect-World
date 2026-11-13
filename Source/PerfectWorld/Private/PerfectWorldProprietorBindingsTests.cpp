// R4-02: the two proprietors' spatial bindings (Debashis Pal, repair workshop; Mahua Saha, hardware store). The real providers are
// derived from the real kit frontages and checked against the real schedule files for every minute of a week:
//  - WeekResolves: no zone, route or travel transition on any day resolves to a missing binding; home is off-lane and leaves at the
//    shop's footpath anchor, which is the front door anchor's position; every work stay is at its own anchor.
//  - RoutesAreAuthoredAndClear: every visible polyline equals its authored length, is connected, begins and ends at its anchors,
//    and keeps at least 34 cm from every collision box of the built shop at standing height.
//  - WrongFrontageRefused: neither provider is made for any other building.
// Run in the editor under "PerfectWorld.ProprietorBindings".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "GameClock.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NpcBehaviour.h"
#include "NpcScheduleContent.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldProprietorBindings.h"

namespace PWProprietorBindingsTest
{
constexpr double PtCapsuleRadiusCm = 34.0;
constexpr double PtStandingHeightCm = 171.0;
constexpr PerfectWorld::GameInstant PtWeekStart = 7 * PerfectWorld::MinutesPerDay - PerfectWorld::CampaignStartHour * PerfectWorld::MinutesPerHour;
constexpr int32 PtWeekMinutes = 7 * 1440;

struct FPtCase
{
    const TCHAR* ScheduleFile;
    const TCHAR* BuildingId;
    const TCHAR* DoorId;
    const TCHAR* HomeId;
    const TCHAR* FootpathId;
    const TCHAR* FrontDoorAnchor;
    const TCHAR* CounterAnchor;
    int32 AnchorsExpected; // work anchors a week of stays must touch
    FNpBindingProvider (*Bind)(const PWLane::FLaneBuilding&);
};

const FPtCase PtCases[] = {
    {TEXT("npc_schedule.debashis_pal"), TEXT("building.nabapara.repair_workshop"), TEXT("door.nabapara.repair_workshop_front"),
     TEXT("location.nabapara.pal_home"), TEXT("location.nabapara.repair_workshop_footpath"), TEXT("anchor.repair_workshop.front_door"),
     TEXT("anchor.repair_workshop.staff_counter"), 4, &PWLane::NpDebashisBindings},
    {TEXT("npc_schedule.mahua_saha"), TEXT("building.nabapara.hardware_store"), TEXT("door.nabapara.hardware_store_front"),
     TEXT("location.nabapara.saha_home"), TEXT("location.nabapara.hardware_store_footpath"), TEXT("anchor.hardware_store.front_door"),
     TEXT("anchor.hardware_store.staff_counter"), 3, &PWLane::NpMahuaBindings},
};

bool PtLoadSchedule(FAutomationTestBase& T, const TCHAR* FileId, PerfectWorld::NpcSchedule& Out)
{
    FString Text;
    const FString Path = FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule") / (FString(FileId) + TEXT(".json"));
    if (!FFileHelper::LoadFileToString(Text, *Path)) { T.AddError(FString::Printf(TEXT("cannot read %s"), *Path)); return false; }
    const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value.has_value()) { T.AddError(FString::Printf(TEXT("%s does not parse"), *Path)); return false; }
    const std::string Key(TCHAR_TO_UTF8(FileId));
    PerfectWorld::ContentDb Db;
    Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Key);
    if (!Loaded.Ok()) { T.AddError(FString::Printf(TEXT("%s does not load: %hs"), FileId, Loaded.Error.c_str())); return false; }
    Out = *Loaded.Value;
    return true;
}

double PtLength(const TArray<FVector>& Points)
{
    double Total = 0.0;
    for (int32 I = 0; I + 1 < Points.Num(); ++I) { Total += FVector::Dist2D(Points[I], Points[I + 1]); }
    return Total;
}

/** Distance in the floor plane from an axis-aligned leg to a box (exact for legs parallel to the axes). */
double PtLegToBoxCm(const FVector& A, const FVector& B, const FBox& Box)
{
    const double Dx = FMath::Max3(0.0, FMath::Min(A.X, B.X) - Box.Max.X, Box.Min.X - FMath::Max(A.X, B.X));
    const double Dy = FMath::Max3(0.0, FMath::Min(A.Y, B.Y) - Box.Max.Y, Box.Min.Y - FMath::Max(A.Y, B.Y));
    return FMath::Sqrt(Dx * Dx + Dy * Dy);
}
} // namespace PWProprietorBindingsTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProprietorBindingsWeekTest, "PerfectWorld.ProprietorBindings.WeekResolves",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProprietorBindingsWeekTest::RunTest(const FString& Parameters)
{
    using namespace PWProprietorBindingsTest;
    for (const FPtCase& Case : PtCases)
    {
        PerfectWorld::NpcSchedule Schedule;
        PWLane::FLaneBuilding Building;
        if (!PtLoadSchedule(*this, Case.ScheduleFile, Schedule) ||
            !TestTrue(TEXT("the catalogue row resolves"), PWLane::MakeProprietorBuilding(Case.BuildingId, Building))) { return false; }
        FNpBindingProvider Provider = Case.Bind(Building);
        if (!TestFalse(TEXT("the provider is derived"), Provider.Anchors.IsEmpty())) { return false; }
        FNpLaneNpcBindings Bindings;
        if (!TestTrue(TEXT("the provider registers"), Bindings.RegisterProvider(FName(Case.BuildingId), MoveTemp(Provider)))) { return false; }

        TSet<FName> Places;
        for (const auto& Entry : Schedule.Person.Routine) { Places.Add(FName(UTF8_TO_TCHAR(Entry.Zone.c_str()))); }
        Places.Add(FName(UTF8_TO_TCHAR(Schedule.Person.HomeZone.c_str())));
        for (const auto& Route : Schedule.Routes)
        {
            Places.Add(FName(UTF8_TO_TCHAR(Route.From.c_str())));
            Places.Add(FName(UTF8_TO_TCHAR(Route.To.c_str())));
            TestNotNull(*FString::Printf(TEXT("route %hs is bound"), Route.Id.c_str()), Bindings.FindRoute(FName(UTF8_TO_TCHAR(Route.Id.c_str()))));
        }
        for (const FName Place : Places) { TestNotNull(*FString::Printf(TEXT("%s is bound"), *Place.ToString()), Bindings.FindAnchor(Place)); }

        const FNpAnchorBinding* Home = Bindings.FindAnchor(FName(Case.HomeId));
        const FNpAnchorBinding* Footpath = Bindings.FindAnchor(FName(Case.FootpathId));
        const FNpAnchorBinding* Door = Bindings.FindAnchor(FName(Case.FrontDoorAnchor));
        if (!TestTrue(TEXT("home, footpath and front door are bound"), Home != nullptr && Footpath != nullptr && Door != nullptr)) { return false; }
        TestTrue(TEXT("home is off the lane and leaves at the footpath anchor"), Home->Kind == ENpBindingKind::OffLane && Home->ExitAnchor == FName(Case.FootpathId));
        TestTrue(TEXT("the footpath anchor is the front door's position: nobody appears elsewhere"), Footpath->Feet.Equals(Door->Feet, 0.01));
        const FNpRouteBinding* HomeWalk = Bindings.FindRoute(FName(UTF8_TO_TCHAR(Schedule.Routes.back().Id.c_str())));
        TestTrue(TEXT("the home walk is an off-lane segment"), HomeWalk != nullptr && HomeWalk->bOffLane && HomeWalk->Points.IsEmpty());

        TSet<FString> Reported;
        TSet<FName> WorkAnchorsTouched;
        int32 LocalTravel = 0;
        int32 OffLaneTravel = 0;
        for (int32 Minute = 0; Minute < PtWeekMinutes; ++Minute)
        {
            const auto Result = PerfectWorld::ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, PtWeekStart + Minute, false, false);
            if (!Result.Ok()) { AddError(TEXT("an intent is missing")); return false; }
            const FNpResolvedBinding Resolved = Bindings.Resolve(Schedule, Result.Intent);
            if (Resolved.Kind == ENpBindingKind::Missing)
            {
                if (!Reported.Contains(Resolved.Diagnostic)) { Reported.Add(Resolved.Diagnostic); AddError(Resolved.Diagnostic); }
                continue;
            }
            if (Result.Intent.State == PerfectWorld::NpcState::Travel)
            {
                (Resolved.Kind == ENpBindingKind::Local ? LocalTravel : OffLaneTravel) += 1;
            }
            else if (Result.Intent.AnchorId.rfind("anchor.", 0) == 0)
            {
                const FNpAnchorBinding* Anchor = Bindings.FindAnchor(FName(UTF8_TO_TCHAR(Result.Intent.AnchorId.c_str())));
                if (Anchor == nullptr || Resolved.Kind != ENpBindingKind::Local || !Resolved.Feet.Equals(Anchor->Feet, 0.01))
                {
                    const FString Key = FString::Printf(TEXT("%hs stay is not at its anchor"), Result.Intent.AnchorId.c_str());
                    if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
                }
                WorkAnchorsTouched.Add(FName(UTF8_TO_TCHAR(Result.Intent.AnchorId.c_str())));
            }
            else if (Resolved.Kind != ENpBindingKind::OffLane || Resolved.ExitAnchor != FName(Case.FootpathId))
            {
                const FString Key = FString::Printf(TEXT("%hs is not an off-lane stay leaving at the footpath"), Result.Intent.AnchorId.c_str());
                if (!Reported.Contains(Key)) { Reported.Add(Key); AddError(Key); }
            }
        }
        TestEqual(TEXT("the week touches every work anchor"), WorkAnchorsTouched.Num(), Case.AnchorsExpected);
        TestTrue(TEXT("some travel is visible on the lane and some is off it"), LocalTravel > 0 && OffLaneTravel > 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProprietorBindingsRoutesTest, "PerfectWorld.ProprietorBindings.RoutesAreAuthoredAndClear",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProprietorBindingsRoutesTest::RunTest(const FString& Parameters)
{
    using namespace PWProprietorBindingsTest;
    for (const FPtCase& Case : PtCases)
    {
        PerfectWorld::NpcSchedule Schedule;
        PWLane::FLaneBuilding Building;
        if (!PtLoadSchedule(*this, Case.ScheduleFile, Schedule) ||
            !TestTrue(TEXT("the catalogue row resolves"), PWLane::MakeProprietorBuilding(Case.BuildingId, Building))) { return false; }
        FNpBindingProvider Provider = Case.Bind(Building);
        if (!TestFalse(TEXT("the provider is derived"), Provider.Anchors.IsEmpty())) { return false; }
        PWLane::FLaneKitFrontage Kit;
        if (!TestTrue(TEXT("the building is a kit frontage"), PWLane::IsKitFrontage(Building, Kit))) { return false; }
        PWLane::FLaneScene Scene;
        PWLane::BuildKitFrontage(Scene, Building, Kit);
        const double FloorZ = Provider.Anchors.FindChecked(FName(Case.FrontDoorAnchor)).Feet.GetLocation().Z;
        TArray<FBox> Solids;
        for (const FBox& Box : Scene.Blocks.BlockingBoxes())
        {
            if (Box.Max.Z > FloorZ + 2.0 && Box.Min.Z < FloorZ + PtStandingHeightCm) { Solids.Add(Box); }
        }
        TestTrue(TEXT("the built shop has collision"), Solids.Num() >= 3);
        for (const auto& Def : Schedule.Routes)
        {
            const FNpRouteBinding* Route = Provider.Routes.Find(FName(UTF8_TO_TCHAR(Def.Id.c_str())));
            if (!TestNotNull(*FString::Printf(TEXT("route %hs is bound"), Def.Id.c_str()), Route)) { return false; }
            if (Route->bOffLane) { continue; }
            TestTrue(*FString::Printf(TEXT("%hs equals its authored %d cm"), Def.Id.c_str(), Def.LengthCm),
                     FMath::Abs(PtLength(Route->Points) - Def.LengthCm) <= 1.0);
            const FNpAnchorBinding* From = Provider.Anchors.Find(FName(UTF8_TO_TCHAR(Def.From.c_str())));
            const FNpAnchorBinding* To = Provider.Anchors.Find(FName(UTF8_TO_TCHAR(Def.To.c_str())));
            if (!TestTrue(TEXT("both ends are bound"), From != nullptr && To != nullptr && Route->Points.Num() >= 2)) { return false; }
            TestTrue(*FString::Printf(TEXT("%hs begins and ends at its anchors"), Def.Id.c_str()),
                     FVector::Dist2D(Route->Points[0], From->Feet.GetLocation()) < 1.0 && FVector::Dist2D(Route->Points.Last(), To->Feet.GetLocation()) < 1.0);
            for (int32 I = 0; I + 1 < Route->Points.Num(); ++I)
            {
                for (const FBox& Box : Solids)
                {
                    if (PtLegToBoxCm(Route->Points[I], Route->Points[I + 1], Box) < PtCapsuleRadiusCm - 0.5)
                    {
                        AddError(FString::Printf(TEXT("%hs leg %d passes within 34 cm of a collision box"), Def.Id.c_str(), I));
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProprietorBindingsRefusalTest, "PerfectWorld.ProprietorBindings.WrongFrontageRefused",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FProprietorBindingsRefusalTest::RunTest(const FString& Parameters)
{
    using namespace PWProprietorBindingsTest;
    PWLane::FLaneBuilding Workshop;
    PWLane::FLaneBuilding Store;
    if (!TestTrue(TEXT("both rows resolve"), PWLane::MakeProprietorBuilding(PWLane::DebashisBuildingId, Workshop) &&
                                                 PWLane::MakeProprietorBuilding(PWLane::MahuaBuildingId, Store))) { return false; }
    TestTrue(TEXT("the workshop is on the -Y side and the store on the +Y side"), Workshop.Side == -1 && Store.Side == 1);
    TestTrue(TEXT("Debashis is not made for the store"), PWLane::NpDebashisBindings(Store).Anchors.IsEmpty());
    TestTrue(TEXT("Mahua is not made for the workshop"), PWLane::NpMahuaBindings(Workshop).Anchors.IsEmpty());
    PWLane::FLaneBuilding Moved = Workshop;
    Moved.Width += 10.0;
    TestTrue(TEXT("a perturbed footprint is refused"), PWLane::NpDebashisBindings(Moved).Anchors.IsEmpty());
    FNpLaneNpcBindings Bindings;
    TestTrue(TEXT("the two providers coexist"), Bindings.RegisterProvider(FName(PWLane::DebashisBuildingId), PWLane::NpDebashisBindings(Workshop)) &&
                                                   Bindings.RegisterProvider(FName(PWLane::MahuaBuildingId), PWLane::NpMahuaBindings(Store)));
    TestFalse(TEXT("their footpath anchors differ"),
              Bindings.FindAnchor(FName(PWLane::DebashisFootpathId))->Feet.Equals(Bindings.FindAnchor(FName(PWLane::MahuaFootpathId))->Feet, 1.0));
    return true;
}
#endif
