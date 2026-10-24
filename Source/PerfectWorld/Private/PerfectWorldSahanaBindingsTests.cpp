#include "PerfectWorldSahanaBindings.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Json.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldLaneBarriers.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneProps.h"
#include "PerfectWorldNirmalBindings.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldSubrataBindings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(ShBindingsTest, "PerfectWorld.SahanaBindings.WeekAndThreshold",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool ShBindingsTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("authored height hook present"), PWNpc::HasAuthoredHeight(FName(PWLane::SahanaPersonId)) &&
        FMath::IsNearlyEqual(PWNpc::StandingHeightCm(FName(PWLane::SahanaPersonId), EPWNpcOutfit::HouseholdAdult),
            static_cast<double>(PWLane::SahanaStandingHeightCm)));
    PWLane::FLaneBuilding Tall;
    if (!TestTrue(TEXT("catalogue tall flats"), PWLane::MakeSubrataBuilding(PWLane::SubrataTallFlatsId, Tall))) { return false; }
    FNpBindingProvider Provider = PWLane::NpSahanaBindings(Tall);
    const FNpAnchorBinding* Home = Provider.Anchors.Find(FName(PWLane::SahanaHomeId));
    const FNpRouteBinding* Route = Provider.Routes.Find(FName(PWLane::SahanaUnitRoute));
    if (!TestTrue(TEXT("home and route exist"), Home != nullptr && Route != nullptr)) { return false; }
    TestTrue(TEXT("home hidden behind unit B door"), Home->Kind == ENpBindingKind::OffLane && Home->ExitAnchor == FName(PWLane::SahanaDoorstepId));
    const FVector Doorstep = Provider.Anchors.FindChecked(Home->ExitAnchor).Feet.GetLocation();
    TestTrue(TEXT("route begins at unit doorstep"), Route->Points.Num() == 2 && Route->Points[0].Equals(Doorstep));
    TestTrue(TEXT("unit exit is inside, front exit outside"), Doorstep.Y > Route->Points.Last().Y + 100);
    TestTrue(TEXT("route within authored ceiling"), FVector::Dist(Route->Points[0], Route->Points.Last()) <= 500);
    TestTrue(TEXT("no unit A ownership"), !Provider.Anchors.Contains(FName(TEXT("location.nabapara.tall_flats_unit_a"))));
    TestTrue(TEXT("errand is off-lane beyond the threshold"), Provider.Anchors.FindChecked(FName(PWLane::SahanaErrandId)).Kind == ENpBindingKind::OffLane &&
        Provider.Routes.FindChecked(FName(PWLane::SahanaErrandRoute)).bOffLane);
    // A separate doorstep from Nirmal's: two residents never stand on one point.
    FNpBindingProvider Nirmal = PWLane::NpNirmalBindings(Tall);
    const FNpAnchorBinding* NirmalStep = Nirmal.Anchors.Find(FName(PWLane::NirmalDoorstepId));
    if (!TestTrue(TEXT("unit A provider exists"), NirmalStep != nullptr)) { return false; }
    TestTrue(TEXT("doorsteps are distinct"), FVector::Dist(NirmalStep->Feet.GetLocation(), Doorstep) > 5.0);
    PWLane::FLaneScene Scene;
    PWLane::BuildLaneRows(Scene);
    PWLane::BuildLaneProps(Scene);
    PWLane::BuildLaneBarriers(Scene);
    TestTrue(TEXT("both required doors built"), PWLane::HasKitDoor(Scene, PWLane::SahanaDoorId) &&
        PWLane::HasKitDoor(Scene, PWLane::SubrataTallFlatsDoorId));
    // Sample the actual passage polyline against the built lane's collision boxes at standing capsule height.
    for (int32 Sample = 0; Sample <= 32; ++Sample)
    {
        const FVector Feet = FMath::Lerp(Route->Points[0], Route->Points.Last(), Sample / 32.0);
        for (const FBox& Box : Scene.Blocks.BlockingBoxes())
        {
            if (Box.Max.Z <= Feet.Z + 2 || Box.Min.Z >= Feet.Z + PWLane::SahanaStandingHeightCm) { continue; }
            const double Dx = FMath::Max3(0.0, Feet.X - Box.Max.X, Box.Min.X - Feet.X);
            const double Dy = FMath::Max3(0.0, Feet.Y - Box.Max.Y, Box.Min.Y - Feet.Y);
            if (Dx * Dx + Dy * Dy < 34.0 * 34.0)
            { AddError(TEXT("passage route lacks standing capsule clearance")); return false; }
        }
    }
    FNpLaneNpcBindings Bindings;
    TestTrue(TEXT("provider registers"), Bindings.RegisterProvider(FName(PWLane::SahanaHomeId), MoveTemp(Provider)));
    TestTrue(TEXT("coexists with Nirmal's provider"), Bindings.RegisterProvider(FName(PWLane::NirmalHomeId), MoveTemp(Nirmal)));
    PWLane::FLaneBuilding Caretaker;
    if (!TestTrue(TEXT("caretaker catalogue row"), PWLane::MakeSubrataBuilding(PWLane::SubrataCaretakerFlatId, Caretaker))) { return false; }
    TestTrue(TEXT("coexists with Subrata provider"), Bindings.RegisterProvider(FName(PWLane::SubrataProviderId),
        PWLane::NpSubrataBindings(Tall, Caretaker)));
    FString Text;
    if (!TestTrue(TEXT("schedule readable"), FFileHelper::LoadFileToString(Text,
        *(FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule/npc_schedule.sahana_mitra.json"))))) { return false; }
    const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value) { AddError(TEXT("schedule JSON refused")); return false; }
    PerfectWorld::ContentDb Db;
    const std::string Id = "npc_schedule.sahana_mitra";
    Db.Records.emplace(Id, PerfectWorld::ContentRecord{"npc_schedule", Id, Id, *Parsed.Value, {}});
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Id);
    if (!TestTrue(TEXT("typed schedule loads"), Loaded.Ok())) { return false; }
    for (PerfectWorld::GameInstant At = 7 * 1440 - 360; At < 14 * 1440 - 360; ++At)
    {
        const auto Intent = PerfectWorld::ResolveNpcIntentAt(Loaded.Value->Person, Loaded.Value->Travel, At, false, false);
        if (!Intent.Ok() || Bindings.Resolve(*Loaded.Value, Intent.Intent).Kind == ENpBindingKind::Missing)
        { AddError(FString::Printf(TEXT("unresolved at %lld"), static_cast<long long>(At))); return false; }
    }
    Tall.Side = -1;
    TestTrue(TEXT("wrong frontage refused"), PWLane::NpSahanaBindings(Tall).Anchors.IsEmpty());
    return true;
}
#endif
