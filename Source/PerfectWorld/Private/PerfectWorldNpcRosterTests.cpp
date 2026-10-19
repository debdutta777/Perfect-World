// The roster gate (UPGRADE_PLAN UP-04): every person the lane can show must stand at their authored height, be reachable by
// the interaction rules, have an authored conversation or an explained unavailability, offer Leave, and go back to work after
// the talk. The roster below is the one list of those people; a schedule file that is not on it, or a person on it who has
// no schedule or conversation without a stated reason, fails here instead of surfacing as an unreachable block in play.
// Run in the editor under "PerfectWorld.NpcRoster" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldNpcPresenceTests.h"
#include "PerfectWorldMiraPresenceTests.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Algo/AnyOf.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneLifeConversation.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldLaneStationeryShop.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "WorldReducer.h"

namespace PWRosterTest
{
/** One person the lane shows, or is meant to show. Heights are LANE_LIFE_PLAN section B's "Block identity" column. */
struct FRosterPerson
{
    const TCHAR* Id;
    const TCHAR* Name;
    double HeightCm;
    EPWNpcOutfit Outfit;
    /** The lane places this person today (KolkataLane.cpp, LaneLifeFarida.cpp, the presence subsystem). */
    bool bPlacedByLane;
    /** A file in Data/Content/npc_schedule moves them; the household adult is static and has none. */
    bool bScheduled;
    /** The core has an authored catalogue for them. */
    bool bConversation;
    /** Why there is no conversation, when bConversation is false. Required in that case. */
    const TCHAR* WhyNoConversation;
    /** Minute since day 0 midnight; Mira's probe is Tuesday 17:00. */
    int32 ProbeMinute;
};

// Placed people are listed by rising probe minute: the fixture's presenter only moves forward in time.
const FRosterPerson RosterPeople[] = {
    {TEXT("person.household_adult"), TEXT("Your parent"), 168.0, EPWNpcOutfit::ParentHousehold, true, false, true, TEXT(""), 0},
    {TEXT("person.care_worker"), TEXT("Care worker"), 162.0, EPWNpcOutfit::HouseholdAdult, true, true, true, TEXT(""), 450},
    {TEXT("person.anindita_sen"), TEXT("Anindita"), 154.0, EPWNpcOutfit::ShopProprietor, true, true, true, TEXT(""), 540},
    {TEXT("person.farida_qadri"), TEXT("Farida"), 158.0, EPWNpcOutfit::ShopProprietor, true, true, true, TEXT(""), 685},
    {TEXT("person.mira_sen"), TEXT("Mira"), 160.0, EPWNpcOutfit::HouseholdAdult, true, true, true, TEXT(""), 1440 + 1020},
    {TEXT("person.subrata_dey"), TEXT("Subrata"), 181.0, EPWNpcOutfit::HouseholdAdult, false, true, true, TEXT(""), 0},
    {TEXT("person.nirmal_bose"), TEXT("Nirmal"), 174.0, EPWNpcOutfit::HouseholdAdult, false, true, true, TEXT(""), 0},
    {TEXT("person.rupa_ghosh"), TEXT("Rupa"), 157.0, EPWNpcOutfit::HouseholdAdult, false, true, true, TEXT(""), 0},
    {TEXT("person.sahana_mitra"), TEXT("Sahana"), 164.0, EPWNpcOutfit::HouseholdAdult, false, true, true, TEXT(""), 0},
    {TEXT("person.purnima_das"), TEXT("Purnima Das"), 155.0, EPWNpcOutfit::HouseholdAdult, false, true, true, TEXT(""), 0},
    {TEXT("person.arun_paul"), TEXT("Arun Paul"), 176.0, EPWNpcOutfit::HouseholdAdult, false, true, true, TEXT(""), 0},
    {TEXT("person.rekha_bose"), TEXT("Rekha Bose"), 161.0, EPWNpcOutfit::HouseholdAdult, false, true, true, TEXT(""), 0},
    {TEXT("person.debashis_pal"), TEXT("Debashis"), 171.0, EPWNpcOutfit::ShopProprietor, false, true, true, TEXT(""), 0},
    {TEXT("person.mahua_saha"), TEXT("Mahua"), 162.0, EPWNpcOutfit::ShopProprietor, false, true, true, TEXT(""), 0},
};

constexpr double PlayerCapsuleHalf = 82.5; // SCALE_AUDIT: the walking player is 165 cm tall and 68 cm wide
constexpr double PlayerCapsuleRadius = 34.0;
constexpr double DoorHeightCm = 210.0;

PerfectWorld::GameInstant InstantOf(const int32 Minute)
{
    return static_cast<PerfectWorld::GameInstant>(Minute) - 360; // day 0: instant 0 is 06:00
}

std::string Narrow(const TCHAR* Text) { return std::string(TCHAR_TO_UTF8(Text)); }

/** The "person" id of every npc_schedule file in the content tree. */
TArray<FString> ScheduleFilePeople(TArray<FString>& OutFileNames)
{
    TArray<FString> People;
    const FString Dir = FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule");
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), true, false);
    for (const FString& File : Files)
    {
        FString Text;
        if (!FFileHelper::LoadFileToString(Text, *(Dir / File))) { continue; }
        const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
        const PerfectWorld::JsonValue* Person = Parsed.Value ? Parsed.Value->Find("person") : nullptr;
        if (Person == nullptr || !Person->IsString()) { continue; }
        People.Add(UTF8_TO_TCHAR(Person->String.c_str()));
        OutFileNames.Add(File);
    }
    return People;
}

/** The first minute of the day (every 5) at which this person's catalogue serves the availability, or -1. */
int32 FirstServedMinute(const PerfectWorld::ConversationCatalogue& Catalogue, const std::string& Id,
                        const PerfectWorld::PersonAvailability Availability)
{
    const PerfectWorld::ConversationState Fresh;
    for (int32 Minute = 0; Minute < 1440; Minute += 5)
    {
        if (PerfectWorld::SelectConversationNode(Catalogue, Id, Minute, Fresh, Availability) != nullptr) { return Minute; }
    }
    return -1;
}

/** How many minutes of the day (every 5) have no node for this availability: a session would refuse to open there. */
int32 UncoveredMinutes(const PerfectWorld::ConversationCatalogue& Catalogue, const std::string& Id,
                       const PerfectWorld::PersonAvailability Availability)
{
    const PerfectWorld::ConversationState Fresh;
    int32 Gaps = 0;
    for (int32 Minute = 0; Minute < 1440; Minute += 5)
    {
        Gaps += PerfectWorld::SelectConversationNode(Catalogue, Id, Minute, Fresh, Availability) == nullptr ? 1 : 0;
    }
    return Gaps;
}

/** A player 100 cm in front of a person can reach their interaction point (the rule is PWInteraction::ReachCm from the capsule axis). */
double ReachFromNominalPlayerCm(const APWNpcCharacter& Npc)
{
    const FVector Feet = Npc.GetActorLocation() - FVector(0.0, 0.0, Npc.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const FVector PlayerFeet = Feet + FVector(100.0, 0.0, 0.0);
    const FVector AxisBottom = PlayerFeet + FVector(0.0, 0.0, PlayerCapsuleRadius);
    const FVector AxisTop = PlayerFeet + FVector(0.0, 0.0, 2.0 * PlayerCapsuleHalf - PlayerCapsuleRadius);
    const FVector Point = Npc.GetInteractionPoint();
    return FVector::Dist(FMath::ClosestPointOnSegment(Point, AxisBottom, AxisTop), Point);
}

int32 CountActorsWithId(UWorld& World, const FName Id)
{
    int32 Count = 0;
    for (TActorIterator<APWNpcCharacter> It(&World); It; ++It) { Count += It->GetPersonId() == Id ? 1 : 0; }
    return Count;
}

/** Everything about one placed person that does not depend on how they were placed. Returns false if the actor cannot be examined. */
bool CheckPlacedPerson(FAutomationTestBase& Test, UWorld& World, APWNpcCharacter* Npc, const FRosterPerson& Person)
{
    const FString Label = Person.Id;
    if (!Test.TestNotNull(*(Label + TEXT(": the person is physically present")), Npc)) { return false; }
    const FName Id(Person.Id);
    Test.TestTrue(*(Label + TEXT(": identity is the roster id")), Npc->GetPersonId() == Id);
    Test.TestFalse(*(Label + TEXT(": has a display name")), Npc->GetDisplayName().IsEmpty());
    Test.TestEqual(*(Label + TEXT(": exactly one actor carries the id")), CountActorsWithId(World, Id), 1);

    // Height: one query decides the capsule, the actor's own record and the authored table.
    Test.TestNearlyEqual(*(Label + TEXT(": actor height is the authored height")), Npc->GetStandingHeightCm(), Person.HeightCm, 0.01);
    Test.TestNearlyEqual(*(Label + TEXT(": capsule is the authored height")),
                         2.0 * Npc->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Person.HeightCm, 0.01);
    Test.TestNearlyEqual(*(Label + TEXT(": capsule radius equals the player's")),
                         static_cast<double>(Npc->GetCapsuleComponent()->GetScaledCapsuleRadius()), PlayerCapsuleRadius, 0.01);

    // Interaction: targeted by the Talk verb, ahead of doors, with its point on the body and inside a nominal reach.
    const FText Verb = Npc->GetInteractionVerb(nullptr);
    Test.TestTrue(*(Label + TEXT(": the verb offers to talk to them")), Verb.ToString().StartsWith(TEXT("Talk")));
    Test.TestEqual(*(Label + TEXT(": people outrank doors")), Npc->GetInteractionPriority(), PWNpc::InteractionPriority);
    const FVector Feet = Npc->GetActorLocation() - FVector(0.0, 0.0, Npc->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    const double PointHeight = Npc->GetInteractionPoint().Z - Feet.Z;
    Test.TestTrue(*(Label + TEXT(": the interaction point is on the body (waist to shoulder)")),
                  PointHeight > 0.4 * Person.HeightCm && PointHeight < 0.85 * Person.HeightCm);
    Test.TestTrue(*(Label + TEXT(": a player one metre away can reach it")),
                  ReachFromNominalPlayerCm(*Npc) <= PWInteraction::ReachCm);
    return true;
}

/** The core's conversation with this person at the world's clock: opens, offers Leave, leaves, and the person resumes. */
void TalkAndResume(FAutomationTestBase& Test, NpPresenceTest::FNpWorld& Fixture, APWNpcCharacter& Npc, const FRosterPerson& Person,
                   const PerfectWorld::GameInstant At)
{
    const FString Label = Person.Id;
    const std::string Id = Narrow(Person.Id);
    const FName PersonName(Person.Id);
    PerfectWorld::WorldSim Sim(17);
    if (!Test.TestTrue(*(Label + TEXT(": the world clock reaches the probe instant")),
                       Sim.Submit({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{At}}).Accepted)) { return; }
    Fixture.Presence->Reconcile(Sim.State().Clock.Now());
    const PerfectWorld::NpcIntent* Intent = Fixture.Presence->FindIntent(PersonName);
    if (!Test.TestNotNull(*(Label + TEXT(": has a schedule intent now")), Intent)) { return; }
    const PerfectWorld::PersonAvailability Availability = Intent->Availability;
    const FVector Before = Npc.GetActorLocation();

    Test.TestTrue(*(Label + TEXT(": the conversation opens")),
                  Sim.Submit({PerfectWorld::CommandSource::Player, PerfectWorld::ConversationOpen{Id, Availability}}).Accepted);
    Npc.GetNpcMovement()->SetLocalConversationActive(true);
    Test.TestTrue(*(Label + TEXT(": they hold still and chat while the panel is open")),
                  Npc.GetNpcMovement()->GetPresentedState() == EPWNpcState::Chat);

    const PerfectWorld::ConversationCatalogue Catalogue = FaCatalogueForPerson(PersonName);
    const auto Found = Sim.State().Conversations.find(Id);
    if (Test.TestTrue(*(Label + TEXT(": the core holds the open session")), Found != Sim.State().Conversations.end() && Found->second.Open))
    {
        const PerfectWorld::ConversationNodeDef* Node = PerfectWorld::FindConversationNode(Catalogue, Found->second.NodeId);
        Test.TestTrue(*(Label + TEXT(": they say an authored line")), Node != nullptr && !Node->Line.empty());
        bool bLeaveOffered = false;
        for (const PerfectWorld::ConversationChoiceView& Choice : PerfectWorld::VisibleConversationChoices(Catalogue, Found->second))
        {
            bLeaveOffered = bLeaveOffered || Choice.bIsLeave;
        }
        Test.TestTrue(*(Label + TEXT(": Leave is offered")), bLeaveOffered);
    }
    Test.TestTrue(*(Label + TEXT(": Leave is accepted")),
                  Sim.Submit({PerfectWorld::CommandSource::Player, PerfectWorld::ConversationLeave{Id}}).Accepted);
    Npc.GetNpcMovement()->SetLocalConversationActive(false);

    Fixture.Presence->Reconcile(Sim.State().Clock.Now());
    Test.TestTrue(*(Label + TEXT(": the same actor goes on after the talk")), Fixture.Presence->FindPerson(PersonName) == &Npc);
    Test.TestTrue(*(Label + TEXT(": they are no longer chatting once the panel closes")),
                  Npc.GetNpcMovement()->GetPresentedState() != EPWNpcState::Chat);
    Test.TestTrue(*(Label + TEXT(": the talk did not move them")), Npc.GetActorLocation().Equals(Before, 1.0));
}

/** A sleeping person is still targeted, declines politely by name, and opens nothing. */
void CheckDecline(FAutomationTestBase& Test, APWNpcCharacter& Npc, const FRosterPerson& Person)
{
    const FString Label = Person.Id;
    FPWNpcIntentView Asleep;
    Asleep.State = EPWNpcState::Sleep;
    Asleep.bTalkable = false;
    Asleep.DeclineReasonKey = FName(TEXT("npc.reason.asleep"));
    Npc.GetNpcMovement()->SetIntent(Asleep);
    Test.TestTrue(*(Label + TEXT(": asleep, the Talk verb stays")), Npc.GetInteractionVerb(nullptr).ToString().StartsWith(TEXT("Talk")));
    const FString Decline = Npc.GetDeclineText().ToString();
    Test.TestTrue(*(Label + TEXT(": asleep, the notice names them and says why")),
                  Decline.Contains(Npc.GetDisplayName().ToString()) && Decline.Contains(TEXT("asleep")));
    Npc.Interact(nullptr); // no pawn: it must decline quietly and open nothing
}
} // namespace PWRosterTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWRosterHeightsTest, "PerfectWorld.NpcRoster.AuthoredHeights",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWRosterHeightsTest::RunTest(const FString& Parameters)
{
    using namespace PWRosterTest;
    for (const FRosterPerson& Person : RosterPeople)
    {
        const FString Label = Person.Id;
        const FName Id(Person.Id);
        TestTrue(*(Label + TEXT(": the cast table has them")), PWNpc::HasAuthoredHeight(Id));
        TestNearlyEqual(*(Label + TEXT(": standing height")), PWNpc::StandingHeightCm(Id, Person.Outfit), Person.HeightCm, 0.001);
        TestNearlyEqual(*(Label + TEXT(": the authored height wins over the outfit")),
                        PWNpc::StandingHeightCm(Id, EPWNpcOutfit::ParentHousehold), Person.HeightCm, 0.001);
        TestNearlyEqual(*(Label + TEXT(": capsule half height")), PWNpc::CapsuleHalfHeightCm(Id, Person.Outfit), Person.HeightCm * 0.5, 0.001);
        TestTrue(*(Label + TEXT(": the head clears a 210 cm door by at least 15 cm")), Person.HeightCm <= DoorHeightCm - 15.0);
        TestTrue(*(Label + TEXT(": the capsule is taller than it is wide")), Person.HeightCm * 0.5 > PlayerCapsuleRadius);
        const FVector Feet(100.0, 200.0, 15.0);
        TestTrue(*(Label + TEXT(": the spawn capsule rests 2 cm above the floor")),
                 PWNpc::SpawnCapsuleCentre(Feet, Id, Person.Outfit).Equals(Feet + FVector(0.0, 0.0, Person.HeightCm * 0.5 + 2.0), 0.001));
    }
    // The three figures the plan names, spelled out so a table edit cannot silently change them.
    TestNearlyEqual(TEXT("Mira is 160 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.mira_sen")), EPWNpcOutfit::HouseholdAdult), 160.0, 0.001);
    TestNearlyEqual(TEXT("Anindita is 154 cm, not Farida's 158"),
                    PWNpc::StandingHeightCm(FName(TEXT("person.anindita_sen")), EPWNpcOutfit::ShopProprietor), 154.0, 0.001);
    TestNearlyEqual(TEXT("Subrata is 181 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.subrata_dey")), EPWNpcOutfit::HouseholdAdult), 181.0, 0.001);

    // A person the table does not know keeps the outfit's default, so a test person or a new resident still gets a body.
    const FName Stranger(TEXT("person.not_in_the_table"));
    TestFalse(TEXT("a stranger has no authored height"), PWNpc::HasAuthoredHeight(Stranger));
    TestNearlyEqual(TEXT("stranger, household adult outfit"), PWNpc::StandingHeightCm(Stranger, EPWNpcOutfit::HouseholdAdult), 162.0, 0.001);
    TestNearlyEqual(TEXT("stranger, parent outfit"), PWNpc::StandingHeightCm(Stranger, EPWNpcOutfit::ParentHousehold), 168.0, 0.001);
    TestNearlyEqual(TEXT("stranger, proprietor outfit"), PWNpc::StandingHeightCm(Stranger, EPWNpcOutfit::ShopProprietor), 158.0, 0.001);
    TestNearlyEqual(TEXT("no id at all uses the outfit"), PWNpc::StandingHeightCm(NAME_None, EPWNpcOutfit::ParentHousehold), 168.0, 0.001);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWRosterContentTest, "PerfectWorld.NpcRoster.SchedulesAndConversationsAreOnTheRoster",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWRosterContentTest::RunTest(const FString& Parameters)
{
    using namespace PWRosterTest;
    TArray<FString> FileNames;
    const TArray<FString> FilePeople = ScheduleFilePeople(FileNames);
    TestTrue(TEXT("the schedule files were found and parsed"), FilePeople.Num() >= 4);
    for (int32 Index = 0; Index < FilePeople.Num(); ++Index)
    {
        const bool bOnRoster = Algo::AnyOf(RosterPeople, [&](const FRosterPerson& P) { return FilePeople[Index] == P.Id && P.bScheduled; });
        TestTrue(*(FileNames[Index] + TEXT(": its person is on the roster as scheduled")), bOnRoster);
    }

    for (const FRosterPerson& Person : RosterPeople)
    {
        const FString Label = Person.Id;
        TestEqual(*(Label + TEXT(": a schedule file exists exactly when the roster says so")), FilePeople.Contains(Person.Id), Person.bScheduled);
        const std::string Id = Narrow(Person.Id);
        const bool bHasCatalogue = PerfectWorld::ConversationCatalogueForPerson(Id).has_value();
        TestEqual(*(Label + TEXT(": the core has a catalogue exactly when the roster says so")), bHasCatalogue, Person.bConversation);
        if (!Person.bConversation)
        {
            TestTrue(*(Label + TEXT(": no conversation, so the unavailability is explained")), FCString::Strlen(Person.WhyNoConversation) > 20);
            TestFalse(*(Label + TEXT(": and the lane does not place someone it cannot talk to")), Person.bPlacedByLane);
            continue;
        }
        if (!bHasCatalogue) { continue; }
        const PerfectWorld::ConversationCatalogue Catalogue = FaCatalogueForPerson(FName(Person.Id));
        TestTrue(*(Label + TEXT(": awake, they have something to say")), FirstServedMinute(Catalogue, Id, PerfectWorld::PersonAvailability::Awake) >= 0);

        // Whenever a node serves, opening it offers Leave and Leave ends the session.
        for (const PerfectWorld::PersonAvailability Availability :
             {PerfectWorld::PersonAvailability::Awake, PerfectWorld::PersonAvailability::Busy, PerfectWorld::PersonAvailability::Asleep})
        {
            const int32 Minute = FirstServedMinute(Catalogue, Id, Availability);
            if (Minute < 0)
            {
                AddWarning(FString::Printf(TEXT("%s: availability %hs has no conversation node at any time; a session would refuse to open."),
                                           Person.Id, PerfectWorld::PersonAvailabilityId(Availability).data()));
                continue;
            }
            PerfectWorld::ConversationState State;
            TestTrue(*(Label + TEXT(": opens")), PerfectWorld::OpenConversation(Catalogue, State, Id, Minute, Availability) == PerfectWorld::ConversationError::None);
            bool bLeave = false;
            for (const PerfectWorld::ConversationChoiceView& Choice : PerfectWorld::VisibleConversationChoices(Catalogue, State))
            {
                bLeave = bLeave || Choice.bIsLeave;
            }
            TestTrue(*(Label + TEXT(": Leave is always offered")), bLeave);
            TestTrue(*(Label + TEXT(": Leave closes the session")), PerfectWorld::LeaveConversation(State) == PerfectWorld::ConversationError::None && !State.Open);
            const int32 Gaps = UncoveredMinutes(Catalogue, Id, Availability);
            if (Gaps > 0)
            {
                AddWarning(FString::Printf(TEXT("%s: availability %hs has no node for %d of 288 five-minute slots."),
                                           Person.Id, PerfectWorld::PersonAvailabilityId(Availability).data(), Gaps));
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWRosterSpawnedTest, "PerfectWorld.NpcRoster.SpawnedPeopleAreTalkable",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWRosterSpawnedTest::RunTest(const FString& Parameters)
{
    using namespace PWRosterTest;
    NpPresenceTest::FNpWorld Fixture;
    UaMiraPresence::FUaShopPhysics UaShopPhysics;
    if (!TestNotNull(TEXT("presence subsystem"), Fixture.Presence)) { return false; }

    // Anindita's and Mira's schedules join the fixture's database (it carries the care worker's and Farida's).
    for (const TCHAR* FileId : {TEXT("npc_schedule.anindita_sen"), TEXT("npc_schedule.mira_sen")})
    {
        FString Text;
        if (!TestTrue(FString::Printf(TEXT("%s loads"), FileId),
                      FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / TEXT("Data/Content/npc_schedule") / (FString(FileId) + TEXT(".json")))))) { return false; }
        const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
        if (!TestTrue(TEXT("and parses"), Parsed.Value.has_value())) { return false; }
        const std::string Key(TCHAR_TO_UTF8(FileId));
        Fixture.Db.Records.emplace(Key, PerfectWorld::ContentRecord{"npc_schedule", Key, Key, *Parsed.Value, {}});
    }

    // The providers the lane registers, in the lane's order: bindings first, then the people.
    if (!TestTrue(TEXT("care home bindings"), Fixture.BindCare())) { return false; }
    {
        FNpBindingProvider Tea;
        FNpAnchorBinding Counter;
        Counter.Feet = FTransform(FVector(0.0, 0.0, 0.0));
        Tea.Anchors.Add(FName(TEXT("anchor.shanti_chai.counter")), Counter);
        FNpAnchorBinding Wash;
        Wash.Feet = FTransform(FVector(150.0, 0.0, 0.0));
        Tea.Anchors.Add(FName(TEXT("anchor.shanti_chai.wash")), Wash);
        if (!TestTrue(TEXT("tea shop bindings"), Fixture.Presence->Bindings().RegisterProvider(FName(TEXT("building.test.tea")), MoveTemp(Tea)))) { return false; }
    }
    {
        PWLane::FLaneBuilding Building;
        Building.Frame = PWLane::MakeLaneFrame(-100.0, 460.0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
        Building.Width = 700.0;
        Building.Depth = 700.0;
        Building.Setback = 60.0;
        Building.Storeys = 3;
        Building.Side = 1;
        Building.Index = 2;
        Building.Detail = PWLane::ELaneDetail::Full;
        UaShopPhysics.UaBuild(*Fixture.World, Building);
        if (!TestTrue(TEXT("the stationery building is the lane's"), PWLane::IsStationeryBuilding(Building))) { return false; }
        if (!TestTrue(TEXT("stationery bindings"), Fixture.Presence->Bindings().RegisterProvider(
                FName(TEXT("building.test.stationery")), PWLane::NpStationeryBindings(Building)))) { return false; }
    }

    for (const FRosterPerson& Person : RosterPeople)
    {
        if (!Person.bPlacedByLane)
        {
            if (Person.bScheduled)
            {
                AddWarning(FString::Printf(TEXT("%s has a schedule and a conversation but the lane does not place them yet "
                                                "(integrator: register their bindings and RegisterScheduledPerson)."), Person.Id));
            }
            continue;
        }
        const FName Id(Person.Id);
        if (Id == FName(TEXT("person.mira_sen")))
        {
            // Earlier probes never ticked physics: their positions cannot represent Tuesday.
            // Start a fresh Tuesday presentation, then run the other shop resident's physics.
            Fixture.Presence->ResetPresentation(InstantOf(Person.ProbeMinute));
            APWNpcCharacter* UaAnindita = Fixture.Presence->FindPerson(FName(TEXT("person.anindita_sen")));
            if (!TestNotNull(TEXT("Mira's mother is present before the Tuesday probe"), UaAnindita)) { continue; }
            UaShopPhysics.UaBegin(*UaAnindita);
            for (int32 UaFrame = 0; UaFrame < 2400; ++UaFrame)
            {
                UaShopPhysics.UaFrame(*Fixture.World, *UaAnindita);
                Fixture.Presence->Tick(static_cast<float>(UaMiraPresence::FUaShopPhysics::UaFrameSeconds));
            }
            const FVector UaThreshold = Fixture.Presence->Bindings().FindAnchor(
                FName(TEXT("location.nabapara.stationery_footpath")))->Feet.GetLocation();
            TestTrue(TEXT("Anindita walked clear of Mira's arrival threshold"),
                FVector::Dist2D(UaAnindita->GetActorLocation(), UaThreshold) > 2.0 * PWNpc::CapsuleRadius);
        }
        APWNpcCharacter* Npc = nullptr;
        if (!Person.bScheduled)
        {
            // The household adult is placed statically by KolkataLane.cpp with this id and outfit.
            Npc = APWNpcCharacter::SpawnAt(*Fixture.World, FTransform(FRotator::ZeroRotator, FVector(2000.0, 2000.0, 0.0)), Id,
                                           FText::FromString(Person.Name), Person.Outfit);
            if (CheckPlacedPerson(*this, *Fixture.World, Npc, Person)) { CheckDecline(*this, *Npc, Person); }
            continue;
        }
        if (!TestTrue(*(FString(Person.Id) + TEXT(": registers from the validated schedule")),
                      Fixture.Presence->RegisterScheduledPersonFromContent(Fixture.Db, Id, FText::FromString(Person.Name), Person.Outfit))) { continue; }
        const PerfectWorld::GameInstant At = InstantOf(Person.ProbeMinute);
        Fixture.Presence->Reconcile(At);
        Npc = Fixture.Presence->FindPerson(Id);
        if (!CheckPlacedPerson(*this, *Fixture.World, Npc, Person)) { continue; }
        if (Id == FName(TEXT("person.mira_sen")))
        {
            const auto* UaDoor = Fixture.Presence->Bindings().FindAnchor(FName(TEXT("location.nabapara.stationery_footpath")));
            const auto* UaDesk = Fixture.Presence->Bindings().FindAnchor(FName(TEXT("anchor.stationery.packing_desk")));
            const FVector UaFeet = Npc->GetActorLocation() - FVector(0.0, 0.0, Npc->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
            TestTrue(TEXT("Tuesday 17:00 presents Mira at the threshold"), UaFeet.Equals(UaDoor->Feet.GetLocation(), 1.0));
            TestTrue(TEXT("Tuesday 17:00 resolves Mira's packing work"),
                Fixture.Presence->FindIntent(Id)->State == PerfectWorld::NpcState::Work);
            UaShopPhysics.UaBegin(*Npc);
            for (int32 UaFrame = 0; UaFrame < 2400; ++UaFrame)
            {
                UaShopPhysics.UaFrame(*Fixture.World, *Npc);
                Fixture.Presence->Tick(static_cast<float>(UaMiraPresence::FUaShopPhysics::UaFrameSeconds));
            }
            TestTrue(TEXT("Mira finishes the ingress route at her desk before Talk"),
                FVector::Dist2D(Npc->GetActorLocation(), UaDesk->Feet.GetLocation()) <= PWNpcMove::ArrivalToleranceCm);
        }

        // Stable identity: reconciling the same instant and ticking presentation keep the same actor.
        Fixture.Presence->Reconcile(At);
        Fixture.Presence->Tick(0.1f);
        TestTrue(*(FString(Person.Id) + TEXT(": the same actor after a second reconcile and tick")), Fixture.Presence->FindPerson(Id) == Npc);
        TestTrue(*(FString(Person.Id) + TEXT(": the schedule lets them be talked to, or declines in words")),
                 Npc->GetNpcMovement()->IsTalkable() || !Npc->GetDeclineText().IsEmpty());

        TalkAndResume(*this, Fixture, *Npc, Person, At);
        CheckDecline(*this, *Npc, Person);
    }
    return true;
}

#endif
