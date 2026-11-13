#include "PerfectWorldNpcPilotBridgeTests.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldNpcPilotBridge.h"
#include "PerfectWorldNpcPresenceInternal.h"
#include "PerfectWorldNpcPresenceSubsystem.h"

namespace
{
struct FR55PilotWorld
{
    UWorld* World = nullptr;
    UPWNpcPresenceSubsystem* Presence = nullptr;
    PerfectWorld::ContentDb Db;
    bool bReady = false;

    FR55PilotWorld()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        if (World == nullptr) { return; }
        World->TimeSeconds = 10.0;
        Presence = World->GetSubsystem<UPWNpcPresenceSubsystem>();
        if (Presence == nullptr) { return; }
        FNpBindingProvider Provider;
        for (const TCHAR* Id : {TEXT("npc_schedule.debashis_pal"), TEXT("npc_schedule.mahua_saha"), TEXT("npc_schedule.farida_qadri")})
        {
            FString Text;
            if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule") / (FString(Id) + TEXT(".json"))))) { return; }
            const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
            if (!Parsed.Value) { return; }
            const std::string Key(TCHAR_TO_UTF8(Id));
            Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
            const auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Key);
            if (!Loaded.Ok()) { return; }
            // Bridge tests use off-lane bindings so they need neither a viewport nor actor collision.
            for (const auto& Entry : Loaded.Value->Person.Routine)
            {
                FNpAnchorBinding Anchor;
                Anchor.Kind = ENpBindingKind::OffLane;
                Provider.Anchors.Add(FName(UTF8_TO_TCHAR(Entry.Zone.c_str())), Anchor);
            }
            for (const auto& Route : Loaded.Value->Routes)
            {
                FNpAnchorBinding Anchor;
                Anchor.Kind = ENpBindingKind::OffLane;
                Provider.Anchors.Add(FName(UTF8_TO_TCHAR(Route.From.c_str())), Anchor);
                Provider.Anchors.Add(FName(UTF8_TO_TCHAR(Route.To.c_str())), Anchor);
                Provider.Routes.Add(FName(UTF8_TO_TCHAR(Route.Id.c_str())), FNpRouteBinding{{}, true});
            }
        }
        if (!Presence->Bindings().RegisterProvider(FName(TEXT("building.r55.bridge_fixture")), MoveTemp(Provider))) { return; }
        for (const TCHAR* Id : {TEXT("person.debashis_pal"), TEXT("person.mahua_saha"), TEXT("person.farida_qadri")})
        {
            if (!Presence->RegisterScheduledPersonFromContent(Db, FName(Id), FText::FromString(Id), EPWNpcOutfit::ShopProprietor)) { return; }
        }
        bReady = true;
    }
    ~FR55PilotWorld() { if (World != nullptr) { World->DestroyWorld(false); } }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR55PilotSafePauseTest, "PerfectWorld.NpcPilotBridge.SafePauseAndRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR55PilotSafePauseTest::RunTest(const FString&)
{
    FR55PilotWorld Fixture;
    if (!TestTrue(TEXT("authored schedules and isolated presence loaded"), Fixture.bReady)) { return false; }
    for (const auto& Row : {TPair<FName, int64>{FName(TEXT("person.debashis_pal")), 121},
                           TPair<FName, int64>{FName(TEXT("person.mahua_saha")), 181}})
    {
        auto* Presence = Fixture.Presence;
        Presence->ResetPresentation(Row.Value);
        const auto Before = *Presence->FindPilotDecision(Row.Key);
        TestTrue(TEXT("Talk queued"), Presence->RequestPilotChat(Row.Key));
        TestTrue(TEXT("repeated Talk retains original request"), Presence->RequestPilotChat(Row.Key));
        const auto Pending = *Presence->FindPilotDecision(Row.Key);
        TestTrue(TEXT("still working before safe point"), Pending.Intent.State == PerfectWorld::NpcState::Work);
        if (!TestTrue(TEXT("core supplied safe boundary"), Pending.DeferredUntil.has_value())) { return false; }
        TestEqual(TEXT("two minutes until safe point"), *Pending.DeferredUntil, Row.Value + 2);
        TestTrue(TEXT("readable reason"), !R55NpcReasonText(FName(TEXT("npc.reason.next_safe_pause"))).IsEmpty());
        Presence->Reconcile(*Pending.DeferredUntil - 1);
        TestTrue(TEXT("no early Chat"), Presence->FindIntent(Row.Key)->State == PerfectWorld::NpcState::Work);
        Presence->Reconcile(*Pending.DeferredUntil);
        const auto Accepted = *Presence->FindPilotDecision(Row.Key);
        TestTrue(TEXT("accepted Chat is available"), Accepted.Intent.State == PerfectWorld::NpcState::Chat &&
            Accepted.Intent.Availability == PerfectWorld::PersonAvailability::Awake && Accepted.Intent.DeclineReasonKey.empty());
        TestEqual(TEXT("current step finished once"), Accepted.Work.CompletedSteps, Before.Work.CompletedSteps + 1);
        Presence->Reconcile(*Pending.DeferredUntil + 7);
        TestEqual(TEXT("hold earns no steps"), Presence->FindPilotDecision(Row.Key)->Work.CompletedSteps, Accepted.Work.CompletedSteps);
        Presence->ReleasePilotChat(Row.Key);
        const auto Resumed = *Presence->FindPilotDecision(Row.Key);
        TestTrue(TEXT("release resumes work"), Resumed.Intent.State == PerfectWorld::NpcState::Work);
        TestEqual(TEXT("next unfinished ordinal retained"), Resumed.Work.Ordinal, Accepted.Work.Ordinal);
        Presence->Reconcile(*Pending.DeferredUntil + 10);
        TestEqual(TEXT("one resumed step after three minutes"), Presence->FindPilotDecision(Row.Key)->Work.CompletedSteps, Accepted.Work.CompletedSteps + 1);
        Presence->ResetPresentation(*Pending.DeferredUntil + 10);
        TestTrue(TEXT("reset removes transient interruption history"), Presence->FindPilotDecision(Row.Key)->Work.CompletedSteps > Accepted.Work.CompletedSteps + 1);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR55PilotYieldTest, "PerfectWorld.NpcPilotBridge.ChatOutranksObstruction",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR55PilotYieldTest::RunTest(const FString&)
{
    FR55PilotWorld Fixture;
    if (!TestTrue(TEXT("fixture ready"), Fixture.bReady)) { return false; }
    const FName Person(TEXT("person.debashis_pal"));
    auto* Presence = Fixture.Presence;
    Presence->ResetPresentation(121);
    Presence->ReportPilotObstruction(Person, true);
    TestTrue(TEXT("work Yield deferred logically"), Presence->FindPilotDecision(Person)->DeferredUntil == 123);
    Presence->Reconcile(123);
    TestTrue(TEXT("Yield accepted at boundary"), Presence->FindIntent(Person)->State == PerfectWorld::NpcState::Yield);
    TestTrue(TEXT("Chat replaces Yield without overlap"), Presence->RequestPilotChat(Person));
    TestTrue(TEXT("Chat accepted at held safe point"), Presence->FindIntent(Person)->State == PerfectWorld::NpcState::Chat);
    Presence->Reconcile(126);
    Presence->ReleasePilotChat(Person);
    TestTrue(TEXT("remaining obstruction restores Yield"), Presence->FindIntent(Person)->State == PerfectWorld::NpcState::Yield);
    Presence->ReportPilotObstruction(Person, false);
    TestTrue(TEXT("clearance restores work"), Presence->FindIntent(Person)->State == PerfectWorld::NpcState::Work);
    TestEqual(TEXT("no paused progress"), Presence->FindPilotDecision(Person)->Work.CompletedSteps, int64(1));
    Presence->Reconcile(127);
    TestTrue(TEXT("Talk queued again inside next unfinished step"), Presence->RequestPilotChat(Person));
    TestTrue(TEXT("prior same-block pause retained in safe-point math"), Presence->FindPilotDecision(Person)->DeferredUntil == 129);
    Presence->ReleasePilotChat(Person);
    Presence->Reconcile(129);
    TestTrue(TEXT("cancelled request never enters Chat"), Presence->FindIntent(Person)->State == PerfectWorld::NpcState::Work);
    TestEqual(TEXT("cancellation does not consume a step"), Presence->FindPilotDecision(Person)->Work.CompletedSteps, int64(2));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR55PilotBoundaryTest, "PerfectWorld.NpcPilotBridge.ScheduleBoundaryAndReset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR55PilotBoundaryTest::RunTest(const FString&)
{
    FR55PilotWorld Fixture;
    if (!TestTrue(TEXT("fixture ready"), Fixture.bReady)) { return false; }
    auto* Presence = Fixture.Presence;
    const FName Person(TEXT("person.mahua_saha"));
    Presence->ResetPresentation(419); // 12:59, final counter step before the trip to lunch.
    TestTrue(TEXT("Talk queued before lunch journey"), Presence->RequestPilotChat(Person));
    Presence->Reconcile(420);
    TestTrue(TEXT("current schedule replaces expired work under Chat"), Presence->FindPilotDecision(Person)->ScheduleChanged);
    Presence->Reconcile(430); // 13:10, meal at stock room.
    Presence->ReleasePilotChat(Person);
    const auto Meal = *Presence->FindPilotDecision(Person);
    TestTrue(TEXT("release selects current meal and anchor"), Meal.Reason == PerfectWorld::NpcDecisionReason::Meal &&
        Meal.Intent.AnchorId == "anchor.hardware_store.stock_room" && Meal.Work.Phase == PerfectWorld::NpcWorkPhase::None);
    Presence->Reconcile(451);
    TestTrue(TEXT("tea follows meal"), Presence->FindPilotDecision(Person)->Reason == PerfectWorld::NpcDecisionReason::TeaBreak);
    Presence->ResetPresentation(181);
    TestTrue(TEXT("no Chat restored across presentation reset"), Presence->FindIntent(Person)->State == PerfectWorld::NpcState::Work &&
        !Presence->FindPilotDecision(Person)->AcceptedAt);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR55PilotLegacyTest, "PerfectWorld.NpcPilotBridge.NonPilotsUnchanged",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR55PilotLegacyTest::RunTest(const FString&)
{
    FR55PilotWorld Fixture;
    if (!TestTrue(TEXT("fixture ready"), Fixture.bReady)) { return false; }
    auto* Presence = Fixture.Presence;
    const FName Person(TEXT("person.farida_qadri"));
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Fixture.Db, "npc_schedule.farida_qadri");
    if (!TestTrue(TEXT("legacy schedule loaded"), Loaded.Ok())) { return false; }
    for (const int64 At : {int64(121), int64(330), int64(600), int64(970)})
    {
        Presence->Reconcile(At);
        const auto Legacy = PerfectWorld::ResolveNpcIntentAt(Loaded.Value->Person, Loaded.Value->Travel, At, false, false);
        TestTrue(TEXT("pilot chat refuses other identities"), !Presence->RequestPilotChat(Person));
        Presence->ReportPilotObstruction(Person, true);
        Presence->ReleasePilotChat(Person);
        TestTrue(TEXT("presence still equals legacy intent"), Presence->FindIntent(Person) != nullptr && *Presence->FindIntent(Person) == Legacy.Intent);
        TestNull(TEXT("no pilot decision for nonpilot"), Presence->FindPilotDecision(Person));
    }
    auto* Movement = NewObject<UPWNpcMovementComponent>();
    FPWNpcIntentView Work;
    Work.State = EPWNpcState::Work;
    Movement->SetIntent(Work);
    Movement->SetLocalConversationActive(true);
    TestTrue(TEXT("legacy local Chat remains immediate"), Movement->GetPresentedState() == EPWNpcState::Chat);
    Movement->SetLocalConversationActive(false);
    Movement->SetCoreInterruptions(true);
    Movement->SetLocalConversationActive(true);
    TestTrue(TEXT("pilot local Chat cannot bypass core"), Movement->GetPresentedState() == EPWNpcState::Work);
    for (const TCHAR* Id : {TEXT("work.test"), TEXT("work.collect"), TEXT("work.weigh")})
        TestTrue(TEXT("phase has its own cosmetic gesture"), FName(PWNpcGesture::Find(FName(Id)).Id) == FName(Id));
    for (int32 Index = 0; Index <= static_cast<int32>(PerfectWorld::NpcDecisionReason::Yield); ++Index)
    {
        const auto Key = PerfectWorld::NpcDecisionReasonKey(static_cast<PerfectWorld::NpcDecisionReason>(Index));
        const FText Text = R55NpcReasonText(FName(UTF8_TO_TCHAR(std::string(Key).c_str())));
        TestTrue(TEXT("every core reason has readable English"), !Text.IsEmpty() && !Text.ToString().Contains(TEXT("npc.reason.")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR55PilotHistoryTest, "PerfectWorld.NpcPilotBridge.SpanningHistoryRetention",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FR55PilotHistoryTest::RunTest(const FString&)
{
    FR55PilotWorld Fixture;
    if (!TestTrue(TEXT("fixture ready"), Fixture.bReady)) { return false; }
    const auto Loaded = PerfectWorld::LoadNpcSchedule(Fixture.Db, "npc_schedule.debashis_pal");
    if (!TestTrue(TEXT("pilot schedule loaded"), Loaded.Ok())) { return false; }
    FNpPresenceRecord Record;
    Record.Schedule = *Loaded.Value;
    Record.R55Windows = {{PerfectWorld::NpcState::Yield, 121, 124}, {PerfectWorld::NpcState::Chat, 125, 200}};
    const auto History = Record.R55Windows;
    for (const int64 At : {int64(201), int64(202), int64(203)})
    {
        const auto Expected = PerfectWorld::ResolveNpcDecisionAt(Record.Schedule.Person, Record.Schedule.Travel, At, History);
        TestTrue(TEXT("bridge resolves retained history"), R55ResolvePilot(Record, At));
        TestTrue(TEXT("spanning release retains the original acceptance boundary"), Record.R55Decision == Expected);
        TestEqual(TEXT("earlier dependent pause is retained"), static_cast<int32>(Record.R55Windows.size()), 2);
    }
    R55ResolvePilot(Record, 300); // Later schedule segment; neither window contributes any longer.
    TestEqual(TEXT("unneeded old-block windows are pruned"), static_cast<int32>(Record.R55Windows.size()), 0);
    Record.R55Windows.assign(PerfectWorld::NpcMaxInterruptionWindows, {PerfectWorld::NpcState::Chat, 121, 121});
    Record.bR55ChatRequested = true;
    TestTrue(TEXT("history limit refuses without dropping current-block pauses"), !R55ResolvePilot(Record, 121));
    TestEqual(TEXT("history stays bounded at the portable limit"), static_cast<int32>(Record.R55Windows.size()), 128);
    return true;
}
#endif
