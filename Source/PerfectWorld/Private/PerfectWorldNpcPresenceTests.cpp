#include "PerfectWorldNpcPresenceTests.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "WorldReducer.h"

namespace NpPresenceTest
{
const FName NpCarePerson(TEXT("person.care_worker"));

PerfectWorld::GameInstant NpAt(const int32 Day, const int32 Minute)
{
    return static_cast<PerfectWorld::GameInstant>(Day) * PerfectWorld::MinutesPerDay + Minute - 360;
}

FNpWorld::FNpWorld()
{
    World = UWorld::CreateWorld(EWorldType::Game, false);
    // An actor's last render time starts at 0 and this world never ticks, so at world time 0 every actor counts as just
    // rendered (AActor::WasRecentlyRendered), renderer or not. Ten seconds in, an actor nothing has drawn is unseen.
    if (World != nullptr) { World->TimeSeconds = 10.0; }
    // Every game world also gets the lane life subsystem, which reconciles the presenter at the lane clock's start (instant
    // 90, 07:30) as it initialises. A person registered after that is resolved at once, so the tests bind providers before
    // registering people, in the order the Kolkata lane uses.
    Presence = World != nullptr ? World->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
    for (const TCHAR* Id : {TEXT("npc_schedule.care_worker"), TEXT("npc_schedule.farida_qadri")})
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule") / (FString(Id) + TEXT(".json"))))) { continue; }
        const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
        if (!Parsed.Value) { continue; }
        const std::string Key(TCHAR_TO_UTF8(Id));
        Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
    }
}

FNpWorld::~FNpWorld()
{
    if (World != nullptr) { World->DestroyWorld(false); }
}

bool FNpWorld::RegisterCare() const
{
    return Presence != nullptr && Presence->RegisterScheduledPersonFromContent(Db, NpCarePerson,
        FText::FromString(TEXT("Care worker")), EPWNpcOutfit::HouseholdAdult);
}

bool FNpWorld::BindCare() const
{
    if (Presence == nullptr) { return false; }
    const auto Frame = PWLane::MakeLaneFrame(-1400.0, -400.0, FVector::ForwardVector, -FVector::RightVector);
    return Presence->Bindings().RegisterProvider(FName(TEXT("building.test.care")), NpCareHomeBindings(Frame));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpPresenceDuplicateTest, "PerfectWorld.NpcPresence.NoDuplicates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpPresenceDuplicateTest::RunTest(const FString& Parameters)
{
    NpPresenceTest::FNpWorld Fixture;
    if (!TestTrue(TEXT("care bindings registered"), Fixture.BindCare())) { return false; }
    if (!TestTrue(TEXT("validated care schedule registers"), Fixture.RegisterCare())) { return false; }
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 450));
    APWNpcCharacter* First = Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson);
    if (!TestNotNull(TEXT("person spawned at bound common room"), First)) { return false; }
    TestTrue(TEXT("second registration idempotent"), Fixture.RegisterCare());
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 450));
    Fixture.Presence->Tick(0.1f);
    TestTrue(TEXT("same actor retained"), First == Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    TestEqual(TEXT("one stable record"), Fixture.Presence->RegisteredPersonCount(), 1);
    int32 Count = 0;
    for (TActorIterator<APWNpcCharacter> It(Fixture.World); It; ++It) { if (It->GetPersonId() == NpPresenceTest::NpCarePerson) { ++Count; } }
    TestEqual(TEXT("one physical person"), Count, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpPresenceWeekTest, "PerfectWorld.NpcPresence.WeekdayWeekendBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpPresenceWeekTest::RunTest(const FString& Parameters)
{
    NpPresenceTest::FNpWorld Fixture;
    if (!TestTrue(TEXT("care bindings registered"), Fixture.BindCare())) { return false; }
    if (!TestTrue(TEXT("care schedule loaded"), Fixture.RegisterCare())) { return false; }
    const auto NpAnchorAt = [&Fixture](const int32 Day, const int32 Minute)
    {
        Fixture.Presence->Reconcile(NpPresenceTest::NpAt(Day, Minute));
        const auto* Intent = Fixture.Presence->FindIntent(NpPresenceTest::NpCarePerson);
        return Intent != nullptr ? FString(UTF8_TO_TCHAR(Intent->AnchorId.c_str())) : FString();
    };
    TestEqual(TEXT("Monday before desk travel"), NpAnchorAt(0, 479), FString(TEXT("anchor.care_home.common_room")));
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 480));
    TestTrue(TEXT("Monday exact end begins travel"), Fixture.Presence->FindIntent(NpPresenceTest::NpCarePerson)->State == PerfectWorld::NpcState::Travel);
    TestEqual(TEXT("Monday desk arrival"), NpAnchorAt(0, 485), FString(TEXT("anchor.care_home.staff_desk")));
    TestEqual(TEXT("Saturday still common at Monday's desk time"), NpAnchorAt(5, 485), FString(TEXT("anchor.care_home.common_room")));
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(5, 495));
    TestTrue(TEXT("Saturday exact end begins travel"), Fixture.Presence->FindIntent(NpPresenceTest::NpCarePerson)->State == PerfectWorld::NpcState::Travel);
    TestEqual(TEXT("Saturday desk arrival"), NpAnchorAt(5, 500), FString(TEXT("anchor.care_home.staff_desk")));
    TestEqual(TEXT("Sunday uses weekend routine"), NpAnchorAt(6, 485), FString(TEXT("anchor.care_home.common_room")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpPresenceArrivalTest, "PerfectWorld.NpcPresence.AbsentThenArrivesAndDeparts",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpPresenceArrivalTest::RunTest(const FString& Parameters)
{
    NpPresenceTest::FNpWorld Fixture;
    if (!TestTrue(TEXT("care bindings registered"), Fixture.BindCare())) { return false; }
    if (!TestTrue(TEXT("care schedule loaded"), Fixture.RegisterCare())) { return false; }
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 360));
    TestNull(TEXT("off-lane at first"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 400));
    TestNull(TEXT("still traversing explicit off-lane leg"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 418));
    TestNotNull(TEXT("enters local route after construction"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 420));
    TestNotNull(TEXT("present at duty boundary"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 928));
    TestNull(TEXT("unseen off-lane departure retires actor"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(1, 420));
    TestNotNull(TEXT("returns next day"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    Fixture.Presence->ResetPresentation(NpPresenceTest::NpAt(1, 360));
    TestNull(TEXT("reset removes transient physical presence"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpPresenceMissingTest, "PerfectWorld.NpcPresence.MissingBindingNoSpawn",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpPresenceMissingTest::RunTest(const FString& Parameters)
{
    NpPresenceTest::FNpWorld Fixture;
    // The missing binding is reported once: at registration when the presenter already has the lane clock's instant (07:30,
    // she is in the common room), otherwise at the reconcile; the same diagnostic at the same minute is never repeated.
    AddExpectedError(TEXT("missing binding 'anchor.care_home.common_room'"), EAutomationExpectedErrorFlags::Contains, 1);
    if (!TestTrue(TEXT("schedule loaded"), Fixture.RegisterCare())) { return false; }
    Fixture.Presence->Reconcile(NpPresenceTest::NpAt(0, 450));
    TestNull(TEXT("never uses default origin"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    TestTrue(TEXT("care bindings registered late"), Fixture.BindCare());
    Fixture.Presence->Tick(0.1f);
    TestNotNull(TEXT("late provider recovers on same minute"), Fixture.Presence->FindPerson(NpPresenceTest::NpCarePerson));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNpPresencePaidJumpTest, "PerfectWorld.NpcPresence.PaidConversationBoundary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FNpPresencePaidJumpTest::RunTest(const FString& Parameters)
{
    NpPresenceTest::FNpWorld Fixture;
    if (!TestNotNull(TEXT("presentation subsystem"), Fixture.Presence)) { return false; }
    const FName Person(TEXT("person.farida_qadri"));
    // Bindings first, then the person (the presenter may already have the lane clock's instant).
    FNpBindingProvider Provider;
    FNpAnchorBinding Counter;
    Counter.Feet = FTransform(FVector(0.0, 0.0, 0.0));
    Provider.Anchors.Add(FName(TEXT("anchor.shanti_chai.counter")), Counter);
    FNpAnchorBinding Wash;
    Wash.Feet = FTransform(FVector(150.0, 0.0, 0.0));
    Provider.Anchors.Add(FName(TEXT("anchor.shanti_chai.wash")), Wash);
    if (!TestTrue(TEXT("tea shop bindings registered"),
        Fixture.Presence->Bindings().RegisterProvider(FName(TEXT("building.test.tea")), MoveTemp(Provider)))) { return false; }
    if (!TestTrue(TEXT("Farida uses generic loader"), Fixture.Presence->RegisterScheduledPersonFromContent(Fixture.Db, Person,
        FText::FromString(TEXT("Farida")), EPWNpcOutfit::ShopProprietor))) { return false; }
    PerfectWorld::WorldSim Sim(17);
    const auto Before = NpPresenceTest::NpAt(0, 685); // 11:25 -> paid tea -> 11:35, after morning service.
    TestTrue(TEXT("advance to service"), Sim.Submit({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{Before}}).Accepted);
    Fixture.Presence->Reconcile(Sim.State().Clock.Now());
    APWNpcCharacter* Actor = Fixture.Presence->FindPerson(Person);
    if (!TestNotNull(TEXT("Farida at counter"), Actor)) { return false; }
    const FVector Start = Actor->GetActorLocation();
    Actor->GetNpcMovement()->SetLocalConversationActive(true);
    TestTrue(TEXT("conversation opens in core"), Sim.Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationOpen{"person.farida_qadri", PerfectWorld::PersonAvailability::Awake}}).Accepted);
    const auto Paid = Sim.Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationChoose{"person.farida_qadri", "choice.farida_qadri.sit_with_tea", 1}});
    if (!TestTrue(TEXT("paid choice accepted"), Paid.Accepted)) { return false; }
    Fixture.Presence->Reconcile(Sim.State().Clock.Now());
    TestEqual(TEXT("exact ten-minute cost comes from core"), Sim.State().Clock.Now() - Before, PerfectWorld::GameInstant(10));
    TestTrue(TEXT("local conversation holds"), Actor->GetNpcMovement()->GetPresentedState() == EPWNpcState::Chat);
    TestTrue(TEXT("leave command"), Sim.Submit({PerfectWorld::CommandSource::Player, PerfectWorld::ConversationLeave{"person.farida_qadri"}}).Accepted);
    Actor->GetNpcMovement()->SetLocalConversationActive(false);
    Fixture.Presence->Reconcile(Sim.State().Clock.Now());
    const auto* Intent = Fixture.Presence->FindIntent(Person);
    TestTrue(TEXT("new work anchor after paid boundary"), Intent != nullptr && Intent->AnchorId == "anchor.shanti_chai.wash");
    TestTrue(TEXT("same physical actor, no teleport on reconciliation"), Fixture.Presence->FindPerson(Person) == Actor && Actor->GetActorLocation().Equals(Start));
    return true;
}

#endif
