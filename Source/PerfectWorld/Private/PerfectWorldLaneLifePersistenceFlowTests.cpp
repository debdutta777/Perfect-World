#include "PerfectWorldLaneLifePersistence.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Components/BoxComponent.h"
#include "CreationSaveSerialization.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/EngineBaseTypes.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "PerfectWorldBlockTestGameMode.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldNpcPresenceTests.h"
#include "PerfectWorldPlayerCharacter.h"
#include "WorldHash.h"

#include <filesystem>
#include <fstream>

namespace
{
struct Pb2FlowSlot
{
    NpPresenceTest::FNpWorld Scene;
    UPWLaneLifeSubsystem* Life = nullptr;
    PerfectWorld::CharacterCreator Creation;
    std::optional<PerfectWorld::PlayerPlacement> Placement;
    std::string Path;
    std::string Message;
    PerfectWorld::PlayerPlacement SavedPlayer{"zone.street.nabapara_lane", {100, 200, 85}, 12345, -1200,
        PerfectWorld::CameraMode::ThirdPerson};

    Pb2FlowSlot()
    {
        Life = Scene.World != nullptr ? Scene.World->GetSubsystem<UPWLaneLifeSubsystem>() : nullptr;
        const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
            TEXT("Automation/PersistenceFlow") / FGuid::NewGuid().ToString(EGuidFormats::Digits) / TEXT("slot.json"));
        Path = TCHAR_TO_UTF8(*File);
    }
    ~Pb2FlowSlot()
    {
        (void)PerfectWorld::DeleteSaveFiles(Path);
        std::error_code Error;
        std::filesystem::remove(std::filesystem::path(std::u8string(Path.begin(), Path.end())).parent_path(), Error);
    }
    bool Start()
    {
        return Life != nullptr && Creation.ApplyPreset("balanced_scholar").Succeeded() && Creation.StartLife().Succeeded();
    }
    bool Save(const PerfectWorld::WorldState& State) const
    {
        const auto Capture = PWLaneLifePersistence::CaptureWorldSave(Creation.CaptureSave(), State, SavedPlayer);
        return Capture.Succeeded() && PWLaneLifePersistence::WriteWorldSlot(Path, Capture.Text).Ok;
    }
    bool Load() { return Life->LoadWorldSlot(Path, Creation, Placement, Message); }
};

AActor* Pb2Box(UWorld& World, const FVector& Centre, const FVector& Extent)
{
    auto* Actor = World.SpawnActor<AActor>();
    if (Actor == nullptr) return nullptr;
    auto* Box = NewObject<UBoxComponent>(Actor);
    Box->SetBoxExtent(Extent);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionResponseToAllChannels(ECR_Block);
    Actor->SetRootComponent(Box);
    Box->RegisterComponent();
    Actor->SetActorLocation(Centre);
    return Actor;
}

// No BeginPlay, scene assembly, viewport or real slots. Only collision primitives and the production entry seam.
struct Pb2EntryScene
{
    UWorld* World = nullptr;
    UGameInstance* Instance = nullptr;
    APlayerController* Player = nullptr;
    APerfectWorldPlayerCharacter* Pawn = nullptr;
    UPerfectWorldCharacterCreatorSubsystem* Creator = nullptr;

    Pb2EntryScene()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Instance = NewObject<UGameInstance>();
        Instance->AddToRoot();
        Instance->Init();
        if (World == nullptr) return;
        World->SetGameInstance(Instance);
        World->GetWorldSettings()->DefaultGameMode = APerfectWorldBlockTestGameMode::StaticClass();
        World->SetGameMode(FURL());
        Creator = Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>();
        Player = World->SpawnActor<APlayerController>();
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Pawn = World->SpawnActor<APerfectWorldPlayerCharacter>(FVector(3000, 3000, 100), FRotator::ZeroRotator, Spawn);
        if (Player != nullptr && Pawn != nullptr) Player->Possess(Pawn);
        // Lane probe floor and built origin floor are separate: origin floor is GroundZ + HomeFloorW (-185).
        Pb2Box(*World, FVector(500, 300, -10), FVector(2000, 200, 10));
        Pb2Box(*World, FVector(0, -1000, -195), FVector(3000, 550, 10));
    }
    ~Pb2EntryScene()
    {
        if (World != nullptr) World->DestroyWorld(false);
        Instance->Shutdown();
        Instance->RemoveFromRoot();
    }
    bool Start(const bool Care)
    {
        if (Creator == nullptr || Player == nullptr || Pawn == nullptr) return false;
        Creator->BeginNewCreation();
        if (Care && !Creator->ApplyPreset(FName(TEXT("independent_generalist")))) return false;
        return Creator->StartLife();
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(Pb2FlowRoundTrip, "PerfectWorld.PersistenceFlow.RestoresWorldOnce",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool Pb2FlowRoundTrip::RunTest(const FString& Parameters)
{
    Pb2FlowSlot Slot;
    if (!TestTrue(TEXT("started"), Slot.Start()) || !TestTrue(TEXT("bound actual NPC"), Slot.Scene.BindCare()) ||
        !TestTrue(TEXT("registered actual NPC"), Slot.Scene.RegisterCare())) return false;
    const std::string Person(PerfectWorld::HouseholdAdultId);
    const std::string DoorId(PerfectWorld::AuthoredDoors()[0].Id);
    auto Saved = MakeUnique<PerfectWorld::WorldSim>(81);
    TestTrue(TEXT("midday, care worker present"), Saved->Submit({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{360}}).Accepted);
    TestTrue(TEXT("saved negative swing"), Saved->Submit({PerfectWorld::CommandSource::Presentation,
        PerfectWorld::DoorSettle{DoorId, true, -1}}).Accepted);
    TestTrue(TEXT("saved memory session"), Saved->Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationOpen{Person, PerfectWorld::PersonAvailability::Awake}}).Accepted);
    TestTrue(TEXT("saved paid memory"), Saved->Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationChoose{Person, "choice.household_adult.talk_rota", 1}}).Accepted);
    TestTrue(TEXT("saved agreement fact"), Saved->Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationChoose{Person, "choice.household_adult.propose_dishes", 1}}).Accepted);
    TestTrue(TEXT("saved session closed"), Saved->Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationLeave{Person}}).Accepted);
    const auto Snapshot = Saved->State();
    Slot.Life->AdoptSavedWorld(MoveTemp(Saved));
    auto* Door = Slot.Scene.World->SpawnActor<APWDoorActor>();
    if (!TestNotNull(TEXT("door actor"), Door)) return false;
    Door->Configure(FName(UTF8_TO_TCHAR(DoorId.c_str())), 1, true);
    Door->SetActorLocation(FVector(12000, 12000, 0));
    Slot.Life->RegisterDoor(*Door);
    if (!TestTrue(TEXT("saved live snapshot"), Slot.Save(Slot.Life->GetWorldState()))) return false;

    auto Changed = MakeUnique<PerfectWorld::WorldSim>(Slot.Life->GetWorldState());
    TestTrue(TEXT("advance after save"), Changed->Submit({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{11}}).Accepted);
    TestTrue(TEXT("flip after save"), Changed->Submit({PerfectWorld::CommandSource::Presentation,
        PerfectWorld::DoorSettle{DoorId, false, 0}}).Accepted);
    TestTrue(TEXT("reopen after save"), Changed->Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationOpen{Person, PerfectWorld::PersonAvailability::Awake}}).Accepted);
    TestTrue(TEXT("paid choice after save"), Changed->Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationChoose{Person, "choice.household_adult.revisit_rota", 2}}).Accepted);
    TestTrue(TEXT("close changed session"), Changed->Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationLeave{Person}}).Accepted);
    Slot.Life->AdoptSavedWorld(MoveTemp(Changed));
    const uint64 Resets = Slot.Life->GetRestorePresentationCount();
    const auto* PriorNpc = Slot.Scene.Presence->FindPerson(FName(TEXT("person.care_worker")));
    if (!TestNotNull(TEXT("NPC before restore"), PriorNpc)) return false;
    int32 Broadcasts = 0;
    const auto Handle = Slot.Life->OnClockChanged.AddLambda([&Broadcasts](int64) { ++Broadcasts; });
    const bool Loaded = Slot.Load();
    Slot.Life->OnClockChanged.Remove(Handle);
    if (!TestTrue(TEXT("load succeeds"), Loaded)) return false;
    TestTrue(TEXT("entire world equals saved state"), PerfectWorld::HashWorldState(Slot.Life->GetWorldState()) == PerfectWorld::HashWorldState(Snapshot));
    TestTrue(TEXT("saved clock"), Slot.Life->GetNow() == Snapshot.Clock.Now());
    TestTrue(TEXT("readout derives restored time"), Slot.Life->GetMinuteOfDay() == PerfectWorld::DoorMinuteOfDay(Snapshot.Clock.Now()));
    TestTrue(TEXT("door actor open and negative swing"), Door->IsOpen() && Door->GetSwingSide() == -1 && !Door->IsMoving());
    TestTrue(TEXT("NPC presenter reset exactly once"), Slot.Life->GetRestorePresentationCount() == Resets + 1);
    const auto* RestoredNpc = Slot.Scene.Presence->FindPerson(FName(TEXT("person.care_worker")));
    TestTrue(TEXT("actual NPC replaced"), RestoredNpc != nullptr && RestoredNpc != PriorNpc);
    TestEqual(TEXT("clock published once"), Broadcasts, 1);
    TestTrue(TEXT("placement retained for entry"), Slot.Placement && *Slot.Placement == Slot.SavedPlayer);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(Pb2FlowRefusal, "PerfectWorld.PersistenceFlow.RefusalLeavesWorldUntouched",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool Pb2FlowRefusal::RunTest(const FString& Parameters)
{
    Pb2FlowSlot Slot;
    if (!TestTrue(TEXT("started"), Slot.Start()) || !TestTrue(TEXT("saved once, no backup"), Slot.Save(Slot.Life->GetWorldState()))) return false;
    const auto Hash = PerfectWorld::HashWorldState(Slot.Life->GetWorldState());
    const auto Resets = Slot.Life->GetRestorePresentationCount();
    Slot.Placement = Slot.SavedPlayer;
    const auto Creation = Slot.Creation.CaptureSave();
    {
        std::ofstream File(std::filesystem::path(std::u8string(Slot.Path.begin(), Slot.Path.end())), std::ios::binary | std::ios::trunc);
        File << "corrupt";
    }
    TestFalse(TEXT("damaged slot refused"), Slot.Load());
    TestTrue(TEXT("player-facing refusal"), !Slot.Message.empty());
    TestTrue(TEXT("world unchanged"), Hash == PerfectWorld::HashWorldState(Slot.Life->GetWorldState()));
    TestTrue(TEXT("NPCs untouched"), Resets == Slot.Life->GetRestorePresentationCount());
    TestTrue(TEXT("pending placement unchanged"), Slot.Placement && *Slot.Placement == Slot.SavedPlayer);
    TestTrue(TEXT("creator unchanged"), PerfectWorld::SerializeCreationSave(Slot.Creation.CaptureSave()) == PerfectWorld::SerializeCreationSave(Creation));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(Pb2FlowTalk, "PerfectWorld.PersistenceFlow.ClosesSavedTalkWithoutCharge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool Pb2FlowTalk::RunTest(const FString& Parameters)
{
    Pb2FlowSlot Slot;
    if (!TestTrue(TEXT("started"), Slot.Start())) return false;
    PerfectWorld::WorldSim Saved(81);
    const std::string Person(PerfectWorld::HouseholdAdultId);
    TestTrue(TEXT("evening"), Saved.Submit({PerfectWorld::CommandSource::Player, PerfectWorld::AdvanceTime{720}}).Accepted);
    TestTrue(TEXT("open"), Saved.Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationOpen{Person, PerfectWorld::PersonAvailability::Awake}}).Accepted);
    TestTrue(TEXT("paid choice"), Saved.Submit({PerfectWorld::CommandSource::Player,
        PerfectWorld::ConversationChoose{Person, "choice.household_adult.talk_rota", 1}}).Accepted);
    if (!TestTrue(TEXT("saved while open"), Slot.Save(Saved.State())) || !TestTrue(TEXT("loaded"), Slot.Load())) return false;
    const auto& Before = Saved.State().Conversations.at(Person);
    const auto& After = Slot.Life->GetWorldState().Conversations.at(Person);
    TestFalse(TEXT("restored session closed"), After.Open);
    TestTrue(TEXT("memory and receipts retained"), Before.Facts == After.Facts && Before.Receipts == After.Receipts &&
        Before.TakenChoices == After.TakenChoices && Before.SessionCount == After.SessionCount);
    TestFalse(TEXT("old callback refused"), Slot.Life->ChooseConversation(FName(UTF8_TO_TCHAR(Person.c_str())),
        FName(TEXT("choice.household_adult.talk_rota")), 1).bAccepted);
    TestTrue(TEXT("no second time charge"), Slot.Life->GetNow() == Saved.State().Clock.Now());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(Pb2FlowPlacement, "PerfectWorld.PersistenceFlow.PlacementRestoreAndFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool Pb2FlowPlacement::RunTest(const FString& Parameters)
{
    Pb2EntryScene Scene;
    if (!TestTrue(TEXT("family origin started"), Scene.Start(false))) return false;
    PerfectWorld::PlayerPlacement Saved{"zone.street.nabapara_lane", {400, 300, 85}, 12345, -1200, PerfectWorld::CameraMode::ThirdPerson};
    bool UsedOrigin = false;
    if (!TestTrue(TEXT("supported placement restored"), PWLaneLifePersistence::PlacePlayerAtSavedPlacement(*Scene.Player, Saved, UsedOrigin))) return false;
    TestFalse(TEXT("exact restore uses no fallback"), UsedOrigin);
    TestTrue(TEXT("restored centre grounded"), Scene.Pawn->GetActorLocation().Equals(FVector(400, 300, PWPlayer::CapsuleHalfHeight + 2), 0.1));
    TestTrue(TEXT("restored view"), Scene.Player->GetControlRotation().Equals(FRotator(-12, 123.45, 0), 0.01));
    TestFalse(TEXT("third person restored"), Scene.Pawn->IsFirstPerson());
    auto* Obstacle = Pb2Box(*Scene.World, FVector(400, 300, 85), FVector(50, 50, 85));
    if (!TestNotNull(TEXT("blocked resume fixture"), Obstacle)) return false;
    TestTrue(TEXT("blocked capsule falls back"), PWLaneLifePersistence::PlacePlayerAtSavedPlacement(*Scene.Player, Saved, UsedOrigin) && UsedOrigin);
    const FVector FamilyOrigin = Scene.Pawn->GetActorLocation();
    TestTrue(TEXT("fallback moved away"), FVector::Dist(FamilyOrigin, FVector(400, 300, 85)) > 100);
    Saved.ZoneId = "zone.street.other";
    Saved.PositionCm = {800, 300, 85};
    TestTrue(TEXT("different zone falls back"), PWLaneLifePersistence::PlacePlayerAtSavedPlacement(*Scene.Player, Saved, UsedOrigin) && UsedOrigin);
    Saved.ZoneId = "zone.street.nabapara_lane";
    Saved.PositionCm = {800, 300, 1000};
    TestTrue(TEXT("no nearby floor falls back"), PWLaneLifePersistence::PlacePlayerAtSavedPlacement(*Scene.Player, Saved, UsedOrigin) && UsedOrigin);
    if (!TestTrue(TEXT("care origin started"), Scene.Start(true))) return false;
    Saved.ZoneId = PerfectWorld::OriginHomeZoneId(TCHAR_TO_UTF8(*Scene.Creator->GetOpeningHome().HomeKitId.ToString()));
    Saved.PositionCm = {};
    Saved.Camera = PerfectWorld::CameraMode::FirstPerson;
    TestTrue(TEXT("care origin marker resolved"), PWLaneLifePersistence::PlacePlayerAtSavedPlacement(*Scene.Player, Saved, UsedOrigin) && UsedOrigin);
    TestTrue(TEXT("care and family resolve differently"), FVector::Dist(FamilyOrigin, Scene.Pawn->GetActorLocation()) > 100);
    TestTrue(TEXT("first person restored"), Scene.Pawn->IsFirstPerson());
    const FVector PriorLocation = Scene.Pawn->GetActorLocation();
    Pb2Box(*Scene.World, FVector(0, -1000, -100), FVector(3000, 550, 200));
    TestFalse(TEXT("no safe origin refuses entry"), PWLaneLifePersistence::PlacePlayerAtSavedPlacement(*Scene.Player, Saved, UsedOrigin));
    TestTrue(TEXT("failed entry never teleports"), Scene.Pawn->GetActorLocation().Equals(PriorLocation));
    return true;
}

#endif
