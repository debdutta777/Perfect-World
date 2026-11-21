#include "PerfectWorldCrowdTests.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Engine/World.h"
#include "PerfectWorldAcceptanceCrowdR7.h"
#include "Json.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "WorldHash.h"

namespace
{
struct FR75PTestWorld
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ~FR75PTestWorld() { if (World) World->DestroyWorld(false); }
    FR75PTestWorld() = default;
    FR75PTestWorld(const FR75PTestWorld&) = delete;
    FR75PTestWorld& operator=(const FR75PTestWorld&) = delete;
};

bool R75PCalendar(PerfectWorld::SchoolCalendar& Out)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() /
        TEXT("Data/Content/calendar/calendar.nabapara_2042_43.json")))) return false;
    const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value) return false;
    PerfectWorld::ContentRecord Record;
    Record.Type = "calendar";
    Record.Id = "calendar.nabapara_2042_43";
    Record.Data = *Parsed.Value;
    std::vector<std::string> Problems;
    Out = PerfectWorld::ParseSchoolCalendarRecord(Record, Problems);
    return Problems.empty() && PerfectWorld::ValidateSchoolCalendar(Out).empty();
}

APWCrowdPresenter* R75PPresenter(UWorld& World, const PerfectWorld::SchoolCalendar& Calendar)
{
    auto* Presenter = World.SpawnActor<APWCrowdPresenter>();
    if (!Presenter || !Presenter->Initialise(705, Calendar)) return nullptr;
    Presenter->SetVisibilityForAutomation([](const FVector&) { return false; });
    return Presenter;
}

bool R75PHasBlock(const PWCrowd::FLook& Look, TFunctionRef<bool(const FPWNpcBlock&)> Match)
{
    for (const auto& Block : Look.Blocks) if (Match(Block)) return true;
    return false;
}

TMap<FString, FVector> R75PPositions(const APWCrowdPresenter& Presenter)
{
    TMap<FString, FVector> Result;
    for (const auto& Person : Presenter.ObservedPeople()) Result.Add(Person.Id, Person.Feet);
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR75PCrowdLooksTest, "PerfectWorld.Crowd.CohortOutfits",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR75PCrowdLooksTest::RunTest(const FString& Parameters)
{
    using PerfectWorld::CrowdCohort;
    using PerfectWorld::CrowdOutfit;
    PerfectWorld::CrowdPerson Person;
    Person.AppearanceSeed = 705;
    Person.Cohort = CrowdCohort::Student;
    for (const auto Outfit : {CrowdOutfit::WhiteShirtBlueTrousers, CrowdOutfit::WhiteShirtBlueSkirt})
    {
        Person.Outfit = Outfit;
        const auto Look = PWCrowd::MakeLook(Person);
        TestTrue(TEXT("white shirt"), R75PHasBlock(Look, [](const auto& B) { return B.Tint == FColor(242, 242, 234); }));
        TestTrue(TEXT("blue lower garment"), R75PHasBlock(Look, [](const auto& B) { return B.Tint == FColor(38, 65, 124); }));
        TestTrue(TEXT("school bag behind torso"), R75PHasBlock(Look, [](const auto& B) {
            return B.Centre.X < -10 && B.Tint == FColor(76, 62, 47) && B.Size.Z == 35; }));
        const auto Rebuilt = PWCrowd::MakeLook(Person);
        TestEqual(TEXT("reloaded height"), Rebuilt.Height, Look.Height);
        TestEqual(TEXT("reloaded block count"), Rebuilt.Blocks.Num(), Look.Blocks.Num());
        for (int32 I = 0; I < Look.Blocks.Num(); ++I)
            TestTrue(TEXT("reloaded look"), Rebuilt.Blocks[I].Centre == Look.Blocks[I].Centre &&
                Rebuilt.Blocks[I].Tint == Look.Blocks[I].Tint && Rebuilt.Blocks[I].Size == Look.Blocks[I].Size);
    }
    Person.Cohort = CrowdCohort::Vendor;
    TestTrue(TEXT("vendor basket"), R75PHasBlock(PWCrowd::MakeLook(Person), [](const auto& B) {
        return B.Tint == FColor(166, 115, 64) && B.Size == FVector(28, 23, 17); }));
    Person.Cohort = CrowdCohort::OfficeCommuter;
    const auto Adult = PWCrowd::MakeLook(Person);
    TestTrue(TEXT("commuter shoulder bag"), R75PHasBlock(Adult, [](const auto& B) { return B.Size == FVector(17, 8, 24); }));
    Person.Cohort = CrowdCohort::Child;
    TestTrue(TEXT("children are shorter"), PWCrowd::MakeLook(Person).Height < Adult.Height - 25);
    Person.Cohort = CrowdCohort::Elder;
    TestTrue(TEXT("elder grey hair"), R75PHasBlock(PWCrowd::MakeLook(Person), [](const auto& B) { return B.Tint == FColor(176, 176, 173); }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR75PCrowdPresenceTest, "PerfectWorld.Crowd.PresenceCapsAndTimeJumps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR75PCrowdPresenceTest::RunTest(const FString& Parameters)
{
    FR75PTestWorld Scene;
    PerfectWorld::SchoolCalendar Calendar;
    if (!TestNotNull(TEXT("world"), Scene.World) || !TestTrue(TEXT("authoritative calendar"), R75PCalendar(Calendar))) return false;
    auto* Presenter = R75PPresenter(*Scene.World, Calendar);
    if (!TestNotNull(TEXT("presenter"), Presenter)) return false;
    const auto At = PerfectWorld::CalendarDayStartInstant(0) + 9 * 60 + 40; // inside the 09:15-10:00 arrival window
    const PerfectWorld::NeighbourhoodPoint Gate{6500, 4850, PerfectWorld::NeighbourhoodFootpathZCm};
    Presenter->SetVisibilityForAutomation([](const FVector&) { return true; });
    Presenter->SampleAt(At, Gate);
    TestEqual(TEXT("initial reconstruction never visibly spawns"), Presenter->ObservedPeople().Num(), 0);
    Presenter->SetVisibilityForAutomation([](const FVector&) { return false; });
    Presenter->SampleAt(At, Gate);
    TestTrue(TEXT("offscreen reconstruction produces figures"), Presenter->ObservedPeople().Num() > 0);
    TestTrue(TEXT("default actor cap"), Presenter->ActorCount() <= 36);
    TestTrue(TEXT("near people can become actors"), Presenter->ActorCount() > 0);
    TestTrue(TEXT("far figure cap"), Presenter->FarFigureCount() <= 80);
    TestTrue(TEXT("far people use ISM"), Presenter->FarFigureCount() > 0 && Presenter->CubeInstanceCount() > 0);
    const auto Before = R75PPositions(*Presenter);
    Presenter->SetVisibilityForAutomation([](const FVector&) { return true; });
    Presenter->SampleAt(At + 1440, Gate, {}, 0, true);
    TestEqual(TEXT("visible old roster retained across reload/day jump"), Presenter->ObservedPeople().Num(), Before.Num());
    for (const auto& Person : Presenter->ObservedPeople())
    {
        const auto* Old = Before.Find(Person.Id);
        TestTrue(TEXT("visible identity and feet never teleport"), Old && Old->Equals(Person.Feet, .01));
    }
    Presenter->SetVisibilityForAutomation([](const FVector&) { return false; });
    PerfectWorld::CrowdPresentationBudget Budget;
    Budget.NamedActorCount = 48;
    Presenter->SampleAt(At + 1440, Gate, Budget);
    TestEqual(TEXT("named actors reserve the entire actor budget"), Presenter->ActorCount(), 0);
    TestTrue(TEXT("far cap after forced demotion"), Presenter->FarFigureCount() <= 80);
    for (const auto& Person : Presenter->ObservedPeople()) TestFalse(TEXT("old daily identities retired offscreen"), Before.Contains(Person.Id));
    Presenter->SampleAt(At + 1440, {1000000, 1000000, 0}, Budget);
    TestEqual(TEXT("radius cull only after occlusion"), Presenter->ObservedPeople().Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR75PCrowdReloadTest, "PerfectWorld.Crowd.ReloadAndEntranceBoundaries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR75PCrowdReloadTest::RunTest(const FString& Parameters)
{
    FR75PTestWorld Scene;
    PerfectWorld::SchoolCalendar Calendar;
    if (!Scene.World || !R75PCalendar(Calendar)) return false;
    auto* First = R75PPresenter(*Scene.World, Calendar);
    auto* Reloaded = R75PPresenter(*Scene.World, Calendar);
    if (!First || !Reloaded) return false;
    const PerfectWorld::NeighbourhoodPoint Gate{6500, 4850, PerfectWorld::NeighbourhoodFootpathZCm};
    const auto Start = PerfectWorld::CalendarDayStartInstant(0);
    PerfectWorld::CrowdPresentationBudget Budget;
    Budget.AmbientActorLimit = 0; // compare pure ISM rosters without actors occupying one another's capsules
    First->SampleAt(Start + 554, Gate, Budget); // one minute before arrival begins (gates 09:30 less 15)
    First->SetVisibilityForAutomation([](const FVector&) { return true; });
    First->SampleAt(Start + 555, Gate, Budget);
    int32 Students = 0;
    for (const auto& Person : First->ObservedPeople()) Students += Person.Cohort == PerfectWorld::CrowdCohort::Student;
    TestEqual(TEXT("an entrance event does not permit visible pop-in"), Students, 0);
    First->SetVisibilityForAutomation([](const FVector&) { return false; });
    First->SampleAt(Start + 575, Gate, Budget, 315, true);
    Reloaded->SampleAt(Start + 575, Gate, Budget, 315, true);
    TestTrue(TEXT("identical model state after independent reconstruction"), First->Snapshot() == Reloaded->Snapshot());
    const auto Old = R75PPositions(*First);
    TestEqual(TEXT("same presented roster size"), Old.Num(), Reloaded->ObservedPeople().Num());
    for (const auto& Person : Reloaded->ObservedPeople())
    {
        const auto* Feet = Old.Find(Person.Id);
        TestTrue(TEXT("same route feet by identity"), Feet && Feet->Equals(Person.Feet, .01));
    }
    First->SetVisibilityForAutomation([](const FVector&) { return true; });
    First->SampleAt(Start + 600, Gate, Budget); // first bell: arrival ends
    TestEqual(TEXT("visible exits wait for occlusion"), First->ObservedPeople().Num(), Old.Num());
    First->SetVisibilityForAutomation([](const FVector&) { return false; });
    First->SampleAt(Start + 600, Gate, Budget);
    Students = 0;
    for (const auto& Person : First->ObservedPeople()) Students += Person.Cohort == PerfectWorld::CrowdCohort::Student;
    TestEqual(TEXT("students retire offscreen at session end"), Students, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR75PCrowdExchangeTest, "PerfectWorld.Crowd.ShortExchangeRecordsNothing",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR75PCrowdExchangeTest::RunTest(const FString& Parameters)
{
    FR75PTestWorld Scene;
    if (!Scene.World) return false;
    auto* Life = UPWLaneLifeSubsystem::Get(Scene.World);
    PerfectWorld::CrowdPerson Person;
    Person.Id = "ambient.test.only";
    Person.Cohort = PerfectWorld::CrowdCohort::Student;
    Person.Outfit = PerfectWorld::CrowdOutfit::WhiteShirtBlueTrousers;
    Person.ReceivesAvoidanceAndInteraction = true;
    Person.ShortExchangeTextKey = "ambient.exchange.heading_to_school";
    auto* Figure = APWCrowdFigure::Spawn(*Scene.World, Person, FVector(10000, 0, 0), FRotator::ZeroRotator);
    auto* Pawn = Scene.World->SpawnActor<APerfectWorldPlayerCharacter>(FVector(10120, 0, 85), FRotator::ZeroRotator);
    if (!Life || !Figure || !Pawn) return false;
    auto* Interaction = Pawn->FindComponentByClass<UPWInteractionComponent>();
    if (!Interaction) return false;
    const auto Before = PerfectWorld::HashWorldState(Life->GetWorldState());
    const auto Journal = Life->GetWorldState().Journal;
    TestEqual(TEXT("anonymous verb"), Figure->GetInteractionVerb(Pawn).ToString(), FString(TEXT("Talk")));
    Figure->Interact(Pawn);
    TestTrue(TEXT("notice matches the model text key"), Interaction->GetNotice().EqualTo(PWCrowd::ExchangeLine(Person.ShortExchangeTextKey)));
    TestTrue(TEXT("brief stop for the exchange"), Figure->IsTalkHeld());
    TestFalse(TEXT("no anonymous journal facts"), PerfectWorld::CrowdPerson::RecordsJournalFacts);
    TestTrue(TEXT("actual journal untouched"), Journal == Life->GetWorldState().Journal);
    TestEqual(TEXT("all world facts, conversations, RNG and clock untouched"), PerfectWorld::HashWorldState(Life->GetWorldState()), Before);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR75PCrowdPolylineTest, "PerfectWorld.Crowd.AuthoredBends",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR75PCrowdPolylineTest::RunTest(const FString& Parameters)
{
    PerfectWorld::CrowdRouteSlot Route;
    Route.Route.LengthCm = 2000;
    Route.Segments = {{"first", {0, 0, -185}, {1000, 0, -185}, 1000},
        {"second", {1000, 0, -185}, {1000, 1000, -185}, 1000}};
    const auto Forward = PWCrowd::Approach(Route, 1500, false);
    const auto Reverse = PWCrowd::Approach(Route, 500, true);
    TestEqual(TEXT("forward keeps the turn"), Forward.Num(), 3);
    TestTrue(TEXT("forward corner"), Forward[1] == FVector(1000, 0, -185));
    TestTrue(TEXT("forward stops on graph"), Forward.Last() == FVector(1000, 500, -185));
    TestEqual(TEXT("reverse keeps the turn"), Reverse.Num(), 3);
    TestTrue(TEXT("reverse stops on graph"), Reverse.Last() == FVector(500, 0, -185));
    TestFalse(TEXT("old pose in view prevents reconstruction"), PWCrowd::MayChangePresence(true, false));
    TestFalse(TEXT("new pose in view prevents reconstruction"), PWCrowd::MayChangePresence(false, true));
    TestTrue(TEXT("both poses unseen permit reconstruction"), PWCrowd::MayChangePresence(false, false));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR75PCrowdScenarioTest, "PerfectWorld.Crowd.Round7Scenario",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR75PCrowdScenarioTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> Steps;
    PWAcceptance::AppendCrowdRound7Steps(Steps);
    TSet<FString> Names;
    TestTrue(TEXT("starts with real world readiness"), !Steps.IsEmpty() && Steps[0].Name == TEXT("world_ready"));
    for (const auto& Step : Steps)
    {
        TestFalse(TEXT("unique evidence names"), Names.Contains(Step.Name));
        Names.Add(Step.Name);
        TestTrue(TEXT("bounded executable steps"), bool(Step.Run) && Step.TimeoutSeconds > 0 && Step.TimeoutSeconds <= 30);
    }
    for (const TCHAR* Name : {TEXT("crowd_schoolday_0940_census"), TEXT("crowd_first_person_visible_band"),
        TEXT("crowd_schoolgate_first"), TEXT("crowd_third_person_visible_band"), TEXT("crowd_schoolgate_third"),
        TEXT("crowd_walk_to_anonymous_student"), TEXT("crowd_talk_records_nothing"), TEXT("crowd_short_exchange")})
        TestTrue(TEXT("required scenario evidence"), Names.Contains(Name));
    return true;
}
#endif
