#include "PerfectWorldResidentsR6Bindings.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldNpcLooks.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldNpcPresenceTests.h"

namespace
{
bool R64Load(FAutomationTestBase& Test, PerfectWorld::ContentDb& Db, const TCHAR* Tail, PerfectWorld::NpcSchedule& Schedule)
{
    const FString Id = FString(TEXT("npc_schedule.")) + Tail;
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule") / (Id + TEXT(".json")))))
    { Test.AddError(TEXT("R64 missing resident schedule")); return false; }
    const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value) { Test.AddError(TEXT("R64 invalid schedule JSON")); return false; }
    const std::string Key(TCHAR_TO_UTF8(*Id));
    Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Key);
    if (!Loaded.Ok()) { Test.AddError(FString::Printf(TEXT("R64 schedule contract: %hs"), Loaded.Error.c_str())); return false; }
    Schedule = *Loaded.Value;
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR64ResidentsBindingsTest, "PerfectWorld.ResidentsR6.BindingsAndRegistration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR64ResidentsBindingsTest::RunTest(const FString& Parameters)
{
    using namespace PWLane;
    const FR64Home* Homes[] = {&R64MadhabiHome(), &R64ArupHome()};
    const TCHAR* Tails[] = {TEXT("madhabi_das"), TEXT("arup_basu")};
    FLaneBuilding Buildings[2];
    FNpBindingProvider Providers[2];
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const FR64Home& Home = *Homes[Index];
        if (!TestTrue(TEXT("R64 real -Y catalogue row resolves"), MakeResidentsR6Building(Home, Buildings[Index]))) return false;
        TestEqual(TEXT("R64 -Y lane side"), Buildings[Index].Side, -1);
        FLaneKitFrontage Kit;
        if (!TestTrue(TEXT("R64 recognised kit frontage"), IsKitFrontage(Buildings[Index], Kit))) return false;
        const auto Layout = MakeKitFrontageLayout(Buildings[Index], Kit);
        if (!TestTrue(TEXT("R64 layout valid"), Layout.Ok())) return false;
        FLaneScene Scene;
        BuildKitFrontage(Scene, Buildings[Index], Kit);
        const FLaneDoorSpec* Emitted = Scene.Doors.FindByPredicate([&](const FLaneDoorSpec& Spec) { return Spec.Id == FName(Home.Door); });
        if (!TestNotNull(TEXT("R64 presenter emitted the hung front door"), Emitted)) return false;
        const LI::Opening* Front = nullptr;
        for (const auto& Opening : Layout.Openings) if (Opening.DoorId == TCHAR_TO_UTF8(Home.Door)) Front = &Opening;
        if (!TestNotNull(TEXT("R64 layout hangs the door"), Front)) return false;
        TestTrue(TEXT("R64 hinge at the front opening"), FVector::Dist2D(Emitted->Hinge, KitPoint(Buildings[Index], Layout, Front->HingeU, Front->HingeV)) < 0.5 && !Emitted->bTwoWay);

        Providers[Index] = NpResidentsR6Bindings(Buildings[Index], Home);
        const auto* Route = Providers[Index].Routes.Find(FName(Home.Route));
        const auto* HomeAnchor = Providers[Index].Anchors.Find(FName(Home.HomeLocation));
        const auto* Bed = LI::FindAnchor(Layout, Layout.BuildingId + ".anchor.bed_side");
        if (!TestTrue(TEXT("R64 route, home anchor and kit bed-side resolve"), Route && HomeAnchor && Bed)) return false;
        TestTrue(TEXT("R64 visible home is the real kit bed_side"), HomeAnchor->Kind == ENpBindingKind::Local &&
            HomeAnchor->Feet.GetLocation().Equals(KitPoint(Buildings[Index], Layout, Bed->U, Bed->V), 0.01));
        const double Measured = ResidentsR6RouteLength(*Route);
        AddInfo(FString::Printf(TEXT("R64 %s: visible route %.2f cm; authored off-lane allowance %.0f cm; never padded."), Home.Person, Measured, Home.AuthoredRouteCm));
        TestTrue(TEXT("R64 visible route within the authored allowance"), Measured > 600.0 && Measured <= Home.AuthoredRouteCm);
        const auto* Away = Providers[Index].Anchors.Find(FName(Home.AwayLocation));
        TestTrue(TEXT("R64 away is off-lane behind the threshold alias"), Away && Away->Kind == ENpBindingKind::OffLane && Away->ExitAnchor == FName(Home.FrontAlias));
        TestTrue(TEXT("R64 yield zone owned for the front door"), Providers[Index].Doorways.Num() == 1 && Providers[Index].Doorways[0].Id == FName(Home.Door));
        for (const FName Anchor : {FName(*(FString(Home.Building) + TEXT(".anchor.seat_1"))), FName(*(FString(Home.Building) + TEXT(".anchor.seat_2")))})
            TestTrue(TEXT("R64 kit seat anchors exposed"), Providers[Index].Anchors.Contains(Anchor));

        // Full 34 cm body sweep of the route against the built scene's blockers (body and head bands).
        for (int32 Leg = 1; Leg < Route->Points.Num(); ++Leg)
        {
            const double Span = FVector::Dist(Route->Points[Leg - 1], Route->Points[Leg]);
            const int32 Samples = FMath::Max(1, FMath::CeilToInt(Span / 2));
            for (int32 Step = 0; Step <= Samples; ++Step)
            {
                const auto Feet = FMath::Lerp(Route->Points[Leg - 1], Route->Points[Leg], static_cast<double>(Step) / Samples);
                for (const FBox& Box : Scene.Blocks.BlockingBoxes())
                {
                    if (Box.Max.Z <= Feet.Z + 45 || Box.Min.Z >= Feet.Z + Home.HeightCm) continue;
                    const double Du = FMath::Max3(0.0, Feet.X - Box.Max.X, Box.Min.X - Feet.X);
                    const double Dv = FMath::Max3(0.0, Feet.Y - Box.Max.Y, Box.Min.Y - Feet.Y);
                    if (Du * Du + Dv * Dv < 34 * 34 - 0.01)
                    { AddError(FString::Printf(TEXT("R64 %s route lacks body/head clearance at %s"), Home.Person, *Feet.ToString())); return false; }
                }
            }
        }
        Buildings[Index].Side = 1;
        TestTrue(TEXT("R64 a wrong-side building is refused"), NpResidentsR6Bindings(Buildings[Index], Home).Anchors.IsEmpty());
        Buildings[Index].Side = -1;
        TestTrue(TEXT("R64 the other home's building is refused"), NpResidentsR6Bindings(Buildings[Index], *Homes[1 - Index]).Anchors.IsEmpty());
    }

    NpPresenceTest::FNpWorld Fixture;
    if (!TestNotNull(TEXT("R64 presence subsystem"), Fixture.Presence)) return false;
    auto& Bindings = Fixture.Presence->Bindings();
    if (!TestTrue(TEXT("R64 providers register"), Bindings.RegisterProvider(FName(Homes[0]->HomeLocation), MoveTemp(Providers[0])) &&
        Bindings.RegisterProvider(FName(Homes[1]->HomeLocation), MoveTemp(Providers[1])))) return false;
    for (int32 Person = 0; Person < 2; ++Person)
    {
        const FR64Home& Home = *Homes[Person];
        PerfectWorld::NpcSchedule Schedule;
        if (!R64Load(*this, Fixture.Db, Tails[Person], Schedule)) return false;
        for (const auto& Def : Schedule.Routes)
            TestNotNull(TEXT("R64 authored route resolves"), Bindings.FindRoute(FName(UTF8_TO_TCHAR(Def.Id.c_str()))));
        // Monday 00:00 to the next Monday: every minute of a full week resolves to a binding.
        for (PerfectWorld::GameInstant At = 7 * 1440 - 360; At < 14 * 1440 - 360; ++At)
        {
            const auto Intent = PerfectWorld::ResolveNpcIntentAt(Schedule.Person, Schedule.Travel, At, false, false);
            if (!Intent.Ok() || Bindings.Resolve(Schedule, Intent.Intent).Kind == ENpBindingKind::Missing)
            { AddError(FString::Printf(TEXT("R64 %s unresolved at %lld"), Home.Person, static_cast<long long>(At))); return false; }
        }
        TestTrue(TEXT("R64 authored height is in the cast table"), FMath::IsNearlyEqual(PWNpc::StandingHeightCm(FName(Home.Person), Home.Outfit), Home.HeightCm, 0.001));
        TestTrue(TEXT("R64 validated scheduled registration"), Fixture.Presence->RegisterScheduledPersonFromContent(
            Fixture.Db, FName(Home.Person), FText::FromString(Home.Name), Home.Outfit));
        TestTrue(TEXT("R64 duplicate registration idempotent"), Fixture.Presence->RegisterScheduledPersonFromContent(
            Fixture.Db, FName(Home.Person), FText::FromString(Home.Name), Home.Outfit));
    }
    TestEqual(TEXT("R64 two people registered once"), Fixture.Presence->RegisteredPersonCount(), 2);
    // Wednesday 16:30: both are home and awake.
    Fixture.Presence->Reconcile(2 * 1440 + 16 * 60 + 30 - 360);
    TestNotNull(TEXT("R64 Madhabi physically present at home"), Fixture.Presence->FindPerson(FName(Homes[0]->Person)));
    TestNotNull(TEXT("R64 Arup physically present at home"), Fixture.Presence->FindPerson(FName(Homes[1]->Person)));
    return true;
}
#endif
