#include "PerfectWorldResidentsR5Bindings.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "LaneTallFlatsLayout.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneTallFlats.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldNpcPresenceTests.h"
#include "PerfectWorldRupaBindings.h"

namespace
{
bool R52Load(FAutomationTestBase& Test, PerfectWorld::ContentDb& Db, const TCHAR* Tail,
             PerfectWorld::NpcSchedule& Schedule)
{
    const FString Id = FString(TEXT("npc_schedule.")) + Tail;
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule") / (Id + TEXT(".json")))))
    { Test.AddError(TEXT("R52 missing resident schedule")); return false; }
    const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value) { Test.AddError(TEXT("R52 invalid schedule JSON")); return false; }
    const std::string Key(TCHAR_TO_UTF8(*Id));
    Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Key);
    if (!Loaded.Ok()) { Test.AddError(FString::Printf(TEXT("R52 schedule contract: %hs"), Loaded.Error.c_str())); return false; }
    Schedule = *Loaded.Value;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR52ResidentsBindingsTest, "PerfectWorld.ResidentsR5.BindingsAndRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR52ResidentsBindingsTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    FLaneBuilding Old, Tall;
    if (!TestTrue(TEXT("R52 real catalogue rows"), MakeRupaBuilding(R52OldHouse, Old) && MakeRupaBuilding(R52TallFlats, Tall))) return false;
    const auto Layout = PerfectWorld::LaneTallFlats::GenerateLayout();
    if (!TestTrue(TEXT("R52 layout valid"), Layout.Ok())) return false;
    const LI::Opening* Side = nullptr;
    for (const auto& Opening : Layout.Upper.Openings) if (Opening.DoorId == TCHAR_TO_UTF8(R52UpperDoor)) Side = &Opening;
    if (!TestNotNull(TEXT("R52 existing upper door hung"), Side)) return false;
    TestTrue(TEXT("R52 actual side opening"), Side->Plan == LI::Rect{615, 640, 668, 762} && Side->Kind == LI::OpeningKind::Door);
    FLaneScene Scene;
    BuildTallFlatsUpstairs(Scene, Tall);
    const FLaneDoorSpec* Emitted = Scene.Doors.FindByPredicate([](const FLaneDoorSpec& Spec) { return Spec.Id == FName(R52UpperDoor); });
    if (!TestNotNull(TEXT("R52 presenter emitted door"), Emitted)) return false;
    TestTrue(TEXT("R52 hinge at upper side opening, not infilled front or unit A"),
        Emitted->Hinge.Equals(KitPoint(Tall, Layout.Upper, 627, 668), 0.01) && !Emitted->bTwoWay);
    const FVector Into = FRotator(0, Emitted->ClosedYaw + 90, 0).Vector() * Emitted->PreferredSwing;
    TestTrue(TEXT("R52 leaf swings into upper home"), FVector::DotProduct(Into, Tall.Frame.U) > 0.99);

    auto Prabir = NpPrabirR5Bindings(Old);
    auto Ishita = NpIshitaR5Bindings(Tall);
    const auto* Upper = Ishita.Routes.Find(FName(R52UpperRoute));
    if (!TestNotNull(TEXT("R52 stair route present"), Upper)) return false;
    const double Measured = ResidentsR5RouteLength(*Upper);
    TestTrue(TEXT("R52 stair allowance is sufficient; never clamp geometry"), Measured <= 1680.0);
    // Independent frozen geometry measurement: a changed layout must produce an explicit integration decision.
    TestNearlyEqual(TEXT("R52 full bed-side to shared-front nosing polyline"), Measured, 1639.14153793, 1.0);
    TestTrue(TEXT("R52 all treads retained"), Upper->Points.Num() == static_cast<int32>(Layout.Stair.Steps.size()) + 8);
    const auto* Home = Ishita.Anchors.Find(FName(R52IshitaHome));
    const auto* Bed = LI::FindAnchor(Layout.Upper, Layout.Upper.BuildingId + ".anchor.bed_side");
    if (!TestTrue(TEXT("R52 kit bed-side and logical home resolve"), Home && Bed)) return false;
    TestTrue(TEXT("R52 visible standing home is a real kit anchor"), Home->Kind == ENpBindingKind::Local &&
        Home->Feet.GetLocation().Equals(KitPoint(Tall, Layout.Upper, Bed->U, Bed->V), 0.01));
    const auto* Away = Ishita.Anchors.Find(FName(R52IshitaAway));
    TestTrue(TEXT("R52 away uses the shared threshold alias"), Away && Away->Kind == ENpBindingKind::OffLane && Away->ExitAnchor == FName(R52IshitaFront));
    const auto* Work = Ishita.Routes.Find(FName(R52WorkRoute));
    TestTrue(TEXT("R52 work leg entirely off-lane"), Work && Work->bOffLane && Work->Points.IsEmpty());
    TestTrue(TEXT("R52 no ground unit ownership"), !Ishita.Anchors.Contains(FName(TEXT("location.nabapara.tall_flats_unit_a"))));
    FLaneKitFrontage OldKit;
    if (!TestTrue(TEXT("R52 old house kit"), IsKitFrontage(Old, OldKit))) return false;
    BuildKitFrontage(Scene, Old, OldKit);
    for (const auto* Provider : {&Prabir, &Ishita}) for (const auto& Pair : Provider->Routes)
    {
        const auto& Route = Pair.Value;
        for (int32 Leg = 1; Leg < Route.Points.Num(); ++Leg)
        {
            const double Span = FVector::Dist(Route.Points[Leg - 1], Route.Points[Leg]);
            const int32 Samples = FMath::Max(1, FMath::CeilToInt(Span / 2));
            for (int32 Index = 0; Index <= Samples; ++Index)
            {
                const auto Feet = FMath::Lerp(Route.Points[Leg - 1], Route.Points[Leg], static_cast<double>(Index) / Samples);
                for (const FBox& Box : Scene.Blocks.BlockingBoxes())
                {
                    // Treads/kerbs below the 45 cm step-up envelope are intentional support, not walls.
                    if (Box.Max.Z <= Feet.Z + 45 || Box.Min.Z >= Feet.Z + 169) continue;
                    const double Du = FMath::Max3(0.0, Feet.X - Box.Max.X, Box.Min.X - Feet.X);
                    const double Dv = FMath::Max3(0.0, Feet.Y - Box.Max.Y, Box.Min.Y - Feet.Y);
                    if (Du * Du + Dv * Dv < 34 * 34 - 0.01)
                    { AddError(FString::Printf(TEXT("R52 route %s lacks body/head clearance at %s"), *Pair.Key.ToString(), *Feet.ToString())); return false; }
                }
            }
        }
    }

    NpPresenceTest::FNpWorld Fixture;
    if (!TestNotNull(TEXT("R52 presence subsystem"), Fixture.Presence)) return false;
    auto& Bindings = Fixture.Presence->Bindings();
    if (!TestTrue(TEXT("R52 providers register"), Bindings.RegisterProvider(FName(R52PrabirHome), MoveTemp(Prabir)) &&
        Bindings.RegisterProvider(FName(R52IshitaHome), MoveTemp(Ishita)))) return false;
    const TCHAR* People[] = {R52Prabir, R52Ishita};
    const TCHAR* Tails[] = {TEXT("prabir_chatterjee"), TEXT("ishita_banerjee")};
    for (int32 Person = 0; Person < 2; ++Person)
    {
        PerfectWorld::NpcSchedule Schedule;
        if (!R52Load(*this, Fixture.Db, Tails[Person], Schedule)) return false;
        for (const auto& Def : Schedule.Routes)
        {
            const auto* Route = Bindings.FindRoute(FName(UTF8_TO_TCHAR(Def.Id.c_str())));
            if (!TestNotNull(TEXT("R52 authored route resolves"), Route)) return false;
            const double Length = ResidentsR5RouteLength(*Route);
            if (!Route->bOffLane && FMath::Abs(Length - Def.LengthCm) > 1.0)
                AddInfo(FString::Printf(TEXT("R52 %hs: physical %.2f cm; authored %d cm; delta %.2f cm. Retained allowance, no padded detour."),
                    Def.Id.c_str(), Length, static_cast<int32>(Def.LengthCm), Def.LengthCm - Length));
            if (Route->bOffLane) AddInfo(FString::Printf(TEXT("R52 %hs: %d cm authored beyond threshold, no visible polyline."), Def.Id.c_str(), static_cast<int32>(Def.LengthCm)));
        }
        for (PerfectWorld::GameInstant At = 7 * 1440 - 360; At < 14 * 1440 - 360; ++At)
        {
            const auto Intent = PerfectWorld::ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, At, false, false);
            if (!Intent.Ok() || Bindings.Resolve(Schedule, Intent.Intent).Kind == ENpBindingKind::Missing)
            { AddError(FString::Printf(TEXT("R52 %s unresolved at %lld"), People[Person], static_cast<long long>(At))); return false; }
        }
        if (Person == 1)
        {
            PerfectWorld::NpcIntent Journey;
            Journey.State = PerfectWorld::NpcState::Travel;
            Journey.AnchorId = TCHAR_TO_UTF8(R52IshitaFront);
            Journey.RouteId = std::string("route:") + TCHAR_TO_UTF8(R52IshitaHome) + "->" + TCHAR_TO_UTF8(R52IshitaFront);
            Journey.ProgressPermille = 0;
            const auto Down = Bindings.Resolve(Schedule, Journey);
            Journey.AnchorId = TCHAR_TO_UTF8(R52IshitaHome);
            Journey.RouteId = std::string("route:") + TCHAR_TO_UTF8(R52IshitaFront) + "->" + TCHAR_TO_UTF8(R52IshitaHome);
            const auto Up = Bindings.Resolve(Schedule, Journey);
            TestTrue(TEXT("R52 up/down share exact reversed stair geometry"), Down.RoutePoints.Num() == Up.RoutePoints.Num() &&
                Down.RoutePoints.Num() >= 2 && Down.RoutePoints[0].Equals(Up.RoutePoints.Last()) && Down.RoutePoints.Last().Equals(Up.RoutePoints[0]));
        }
        const auto Outfit = Person == 0 ? EPWNpcOutfit::HouseholdAdult : EPWNpcOutfit::ParentHousehold;
        TestTrue(TEXT("R52 validated scheduled registration"), Fixture.Presence->RegisterScheduledPersonFromContent(
            Fixture.Db, FName(People[Person]), FText::FromString(Tails[Person]), Outfit));
        TestTrue(TEXT("R52 duplicate registration idempotent"), Fixture.Presence->RegisterScheduledPersonFromContent(
            Fixture.Db, FName(People[Person]), FText::FromString(Tails[Person]), Outfit));
    }
    TestEqual(TEXT("R52 two people registered once"), Fixture.Presence->RegisteredPersonCount(), 2);
    Fixture.Presence->Reconcile(1440 + 16 * 60 - 360);
    TestNotNull(TEXT("R52 Prabir physically present at home"), Fixture.Presence->FindPerson(FName(R52Prabir)));
    TestNotNull(TEXT("R52 Ishita physically present at home"), Fixture.Presence->FindPerson(FName(R52Ishita)));
    Old.Side = -1; Tall.Side = -1;
    TestTrue(TEXT("R52 wrong buildings refused"), NpPrabirR5Bindings(Old).Anchors.IsEmpty() && NpIshitaR5Bindings(Tall).Anchors.IsEmpty());
    return true;
}
#endif
