#include "PerfectWorldMiraPresenceTests.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldLaneStationeryShop.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

namespace UaMiraPresence
{
const FName UaPerson(TEXT("person.mira_sen"));
const FName UaDesk(TEXT("anchor.stationery.packing_desk"));
const FName UaThreshold(TEXT("location.nabapara.stationery_footpath"));

PerfectWorld::GameInstant UaAt(const int32 Day, const int32 Minute)
{
    return static_cast<PerfectWorld::GameInstant>(Day) * 1440 + Minute - 360;
}

FVector UaFeet(const APWNpcCharacter& Actor)
{
    return Actor.GetActorLocation() - FVector(0.0, 0.0, Actor.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

struct FUaFixture : NpPresenceTest::FNpWorld
{
    FUaShopPhysics UaPhysics;

    bool UaInit(FAutomationTestBase& Test)
    {
        if (!Test.TestNotNull(TEXT("presence"), Presence)) { return false; }
        FString Text;
        if (!Test.TestTrue(TEXT("Mira schedule loads"), FFileHelper::LoadFileToString(Text,
            *(FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule/npc_schedule.mira_sen.json"))))) { return false; }
        const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
        if (!Test.TestTrue(TEXT("Mira schedule parses"), Parsed.Value.has_value())) { return false; }
        const std::string Key = "npc_schedule.mira_sen";
        Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
        PWLane::FLaneBuilding Building;
        Building.Frame = PWLane::MakeLaneFrame(-100.0, 460.0, FVector::ForwardVector, FVector::RightVector);
        Building.Width = 700.0;
        Building.Depth = 700.0;
        Building.Setback = 60.0;
        Building.Storeys = 3;
        Building.Side = 1;
        Building.Index = 2;
        Building.Detail = PWLane::ELaneDetail::Full;
        UaPhysics.UaBuild(*World, Building);
        if (!Test.TestTrue(TEXT("production provider registers"), Presence->Bindings().RegisterProvider(
            FName(TEXT("building.test.ua_mira")), PWLane::NpStationeryBindings(Building)))) { return false; }
        return Test.TestTrue(TEXT("Mira registers"), Presence->RegisterScheduledPersonFromContent(
            Db, UaPerson, FText::FromString(TEXT("Mira")), EPWNpcOutfit::HouseholdAdult));
    }

    FVector UaPoint(const FName Anchor) const { return Presence->Bindings().FindAnchor(Anchor)->Feet.GetLocation(); }

    // Drive walking physics every frame and steering at its authored interval, as NpcRoute does.
    // Mark this actor rendered without rewinding world time or marking every other actor rendered.
    bool UaWalk(FAutomationTestBase& Test, APWNpcCharacter& Actor, const FVector& Target)
    {
        UaPhysics.UaBegin(Actor);
        bool UaContinuous = true;
        for (int32 Step = 0; Step < 9600 && FVector::Dist2D(UaFeet(Actor), Target) > PWNpcMove::ArrivalToleranceCm; ++Step)
        {
            const FVector Before = Actor.GetActorLocation();
            UaPhysics.UaFrame(*World, Actor);
            Presence->Tick(static_cast<float>(FUaShopPhysics::UaFrameSeconds));
            UaContinuous = UaContinuous && FVector::Dist(Before, Actor.GetActorLocation()) <= 5.0;
        }
        Test.TestTrue(TEXT("every rendered movement step is continuous"), UaContinuous);
        Test.TestTrue(TEXT("walk stays on the authored floor"), FMath::Abs(UaFeet(Actor).Z - Target.Z) <= 5.0);
        return Test.TestTrue(*FString::Printf(TEXT("walk reaches the authored endpoint (%.1f cm left, mode %d, blocked %d)"),
            FVector::Dist2D(UaFeet(Actor), Target), static_cast<int32>(Actor.GetCharacterMovement()->MovementMode),
            Actor.GetNpcMovement()->IsPersistentlyBlocked() ? 1 : 0),
            FVector::Dist2D(UaFeet(Actor), Target) <= PWNpcMove::ArrivalToleranceCm);
    }
};

bool UaDay(FAutomationTestBase& Test, const int32 Day)
{
    FUaFixture Fixture;
    if (!Fixture.UaInit(Test)) { return false; }
    Fixture.Presence->Reconcile(UaAt(Day, 950));
    Test.TestNull(TEXT("school journey is initially off lane"), Fixture.Presence->FindPerson(UaPerson));
    Fixture.Presence->Reconcile(UaAt(Day, 989));
    auto* Actor = Fixture.Presence->FindPerson(UaPerson);
    if (!Test.TestNotNull(TEXT("local arrival exists"), Actor)) { return false; }
    Test.TestTrue(TEXT("arrival starts at threshold, even late in Travel"), UaFeet(*Actor).Equals(Fixture.UaPoint(UaThreshold), 1.0));
    Fixture.Presence->Reconcile(UaAt(Day, 990));
    Test.TestTrue(TEXT("packing intent is authoritative at 16:30"), Fixture.Presence->FindIntent(UaPerson)->State == PerfectWorld::NpcState::Work);
    if (!Fixture.UaWalk(Test, *Actor, Fixture.UaPoint(UaDesk))) { return false; }
    Test.TestTrue(TEXT("work retains the arriving identity"), Fixture.Presence->FindPerson(UaPerson) == Actor);
    Fixture.Presence->Reconcile(UaAt(Day, 1108)); // skip the entire 18:00-18:28 journey
    Test.TestTrue(TEXT("departure does not disappear at desk"), Fixture.Presence->FindPerson(UaPerson) == Actor);
    Test.TestTrue(TEXT("departure is physically Travel"), Actor->GetNpcMovement()->GetIntent().State == EPWNpcState::Travel);
    Fixture.World->TimeSeconds += 1.0;
    Fixture.Presence->Tick(0.1f);
    Test.TestTrue(TEXT("unseen departure still needs to reach threshold"), Fixture.Presence->FindPerson(UaPerson) == Actor);
    if (!Fixture.UaWalk(Test, *Actor, Fixture.UaPoint(UaThreshold))) { return false; }
    Test.TestTrue(TEXT("rendered person remains at exit"), Fixture.Presence->FindPerson(UaPerson) == Actor);
    Fixture.World->TimeSeconds += 1.0;
    Fixture.Presence->Tick(0.1f);
    Test.TestNull(TEXT("unseen person retires only at exit"), Fixture.Presence->FindPerson(UaPerson));
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUaMiraTuesday, "PerfectWorld.MiraPresence.TuesdayArrivalWorkDeparture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUaMiraTuesday::RunTest(const FString& Parameters) { return UaMiraPresence::UaDay(*this, 1); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUaMiraThursday, "PerfectWorld.MiraPresence.ThursdayArrivalWorkDeparture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUaMiraThursday::RunTest(const FString& Parameters) { return UaMiraPresence::UaDay(*this, 3); }

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUaMiraOffDay, "PerfectWorld.MiraPresence.OffDayAbsent",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUaMiraOffDay::RunTest(const FString& Parameters)
{
    using namespace UaMiraPresence;
    FUaFixture Fixture;
    if (!Fixture.UaInit(*this)) { return false; }
    for (const int32 Minute : {950, 989, 990, 1020, 1080, 1108})
    {
        Fixture.Presence->Reconcile(UaAt(2, Minute));
        TestNull(TEXT("Wednesday has no shop presence"), Fixture.Presence->FindPerson(UaPerson));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUaMiraDelayed, "PerfectWorld.MiraPresence.DelayedTravelAcross1630",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUaMiraDelayed::RunTest(const FString& Parameters)
{
    using namespace UaMiraPresence;
    FUaFixture Fixture;
    if (!Fixture.UaInit(*this)) { return false; }
    Fixture.Presence->Reconcile(UaAt(1, 989));
    auto* Actor = Fixture.Presence->FindPerson(UaPerson);
    if (!TestNotNull(TEXT("late traveller"), Actor)) { return false; }
    const FVector Before = Actor->GetActorLocation();
    Fixture.Presence->Reconcile(UaAt(1, 1000));
    TestTrue(TEXT("boundary retains actor without relocation"), Fixture.Presence->FindPerson(UaPerson) == Actor && Actor->GetActorLocation().Equals(Before));
    return Fixture.UaWalk(*this, *Actor, Fixture.UaPoint(UaDesk));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUaMiraLoad, "PerfectWorld.MiraPresence.DirectLoad1700",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUaMiraLoad::RunTest(const FString& Parameters)
{
    using namespace UaMiraPresence;
    for (const int32 UaDay : {1, 3})
    {
        FUaFixture Fixture;
        if (!Fixture.UaInit(*this)) { return false; }
        Fixture.Presence->ResetPresentation(UaAt(UaDay, 1020));
        auto* Actor = Fixture.Presence->FindPerson(UaPerson);
        if (!TestNotNull(TEXT("load presents Mira"), Actor)) { return false; }
        TestTrue(TEXT("load starts at threshold"), UaFeet(*Actor).Equals(Fixture.UaPoint(UaThreshold), 1.0));
        TestTrue(TEXT("load never starts at desk"), FVector::Dist2D(UaFeet(*Actor), Fixture.UaPoint(UaDesk)) > 100.0);
        if (!Fixture.UaWalk(*this, *Actor, Fixture.UaPoint(UaDesk))) { return false; }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUaMiraChat, "PerfectWorld.MiraPresence.ConversationPauseRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUaMiraChat::RunTest(const FString& Parameters)
{
    using namespace UaMiraPresence;
    FUaFixture Fixture;
    if (!Fixture.UaInit(*this)) { return false; }
    Fixture.Presence->Reconcile(UaAt(1, 989));
    auto* Actor = Fixture.Presence->FindPerson(UaPerson);
    if (!TestNotNull(TEXT("arriving Mira"), Actor)) { return false; }
    const FVector Before = Actor->GetActorLocation();
    Actor->GetNpcMovement()->SetLocalConversationActive(true);
    Fixture.Presence->Reconcile(UaAt(1, 1020));
    Actor->GetNpcMovement()->TickPresentation(0.1f);
    TestTrue(TEXT("chat holds across paid time"), Actor->GetNpcMovement()->GetPresentedState() == EPWNpcState::Chat && Actor->GetActorLocation().Equals(Before));
    Actor->GetNpcMovement()->SetLocalConversationActive(false);
    Fixture.Presence->Reconcile(UaAt(1, 1020));
    if (!Fixture.UaWalk(*this, *Actor, Fixture.UaPoint(UaDesk))) { return false; }
    Actor->GetNpcMovement()->SetLocalConversationActive(true);
    Fixture.Presence->Reconcile(UaAt(1, 1108));
    TestTrue(TEXT("conversation holds departure too"), Fixture.Presence->FindPerson(UaPerson) == Actor && Actor->GetNpcMovement()->GetPresentedState() == EPWNpcState::Chat);
    Actor->GetNpcMovement()->SetLocalConversationActive(false);
    Fixture.Presence->Tick(0.1f);
    return Fixture.UaWalk(*this, *Actor, Fixture.UaPoint(UaThreshold));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUaMiraOccupied, "PerfectWorld.MiraPresence.OccupiedThresholdDefersArrival",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUaMiraOccupied::RunTest(const FString& Parameters)
{
    using namespace UaMiraPresence;
    FUaFixture Fixture;
    if (!Fixture.UaInit(*this)) { return false; }
    auto* Blocker = APWNpcCharacter::SpawnAt(*Fixture.World, FTransform(Fixture.UaPoint(UaThreshold)),
        FName(TEXT("person.test.ua_threshold_blocker")), FText::FromString(TEXT("Passerby")));
    if (!TestNotNull(TEXT("threshold occupant"), Blocker)) { return false; }
    Fixture.Presence->ResetPresentation(UaAt(1, 1020));
    Fixture.Presence->Tick(0.1f);
    TestNull(TEXT("occupied threshold does not cause a desk fallback"), Fixture.Presence->FindPerson(UaPerson));
    Blocker->SetActorEnableCollision(false);
    Blocker->Destroy();
    Fixture.Presence->Tick(0.1f);
    auto* Actor = Fixture.Presence->FindPerson(UaPerson);
    if (!TestNotNull(TEXT("arrival retries without another clock change"), Actor)) { return false; }
    TestTrue(TEXT("retry still starts at threshold"), UaFeet(*Actor).Equals(Fixture.UaPoint(UaThreshold), 1.0));
    return Fixture.UaWalk(*this, *Actor, Fixture.UaPoint(UaDesk));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUaMiraDuplicate, "PerfectWorld.MiraPresence.NoDuplicates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FUaMiraDuplicate::RunTest(const FString& Parameters)
{
    using namespace UaMiraPresence;
    FUaFixture Fixture;
    if (!Fixture.UaInit(*this)) { return false; }
    Fixture.Presence->Reconcile(UaAt(1, 1020));
    auto* Actor = Fixture.Presence->FindPerson(UaPerson);
    if (!TestNotNull(TEXT("Mira exists"), Actor)) { return false; }
    for (int32 Repeat = 0; Repeat < 5; ++Repeat)
    {
        TestTrue(TEXT("registration is idempotent"), Fixture.Presence->RegisterScheduledPersonFromContent(
            Fixture.Db, UaPerson, FText::FromString(TEXT("Mira")), EPWNpcOutfit::HouseholdAdult));
        Fixture.Presence->Reconcile(UaAt(1, 1020));
        Fixture.Presence->Tick(0.1f);
    }
    int32 Count = 0;
    for (TActorIterator<APWNpcCharacter> It(Fixture.World); It; ++It) { Count += It->GetPersonId() == UaPerson ? 1 : 0; }
    TestEqual(TEXT("one Mira actor"), Count, 1);
    TestTrue(TEXT("same Mira actor"), Fixture.Presence->FindPerson(UaPerson) == Actor);
    return true;
}

#endif
