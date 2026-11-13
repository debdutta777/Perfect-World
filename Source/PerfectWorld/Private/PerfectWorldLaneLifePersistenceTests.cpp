#include "PerfectWorldLaneLifePersistence.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "CreationSaveSerialization.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "WorldHash.h"
#include "WorldReducer.h"

#include <filesystem>
#include <fstream>
#include <system_error>

namespace
{
namespace PsStorage = PWLaneLifePersistence;
namespace PsCore = PerfectWorld;

std::filesystem::path PsNative(const std::string& Path)
{
    return std::filesystem::path(std::u8string(Path.begin(), Path.end()));
}

struct PsSlotFixture
{
    std::string Path;
    PsCore::CharacterCreator Creator;
    PsCore::WorldSim Sim{81};
    PsCore::PlayerPlacement Player{"zone.street.nabapara_lane", {120, -3400, 110}, 35999, -1234,
        PsCore::CameraMode::ThirdPerson};

    PsSlotFixture()
    {
        const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
            TEXT("Automation/Persistence") / FGuid::NewGuid().ToString(EGuidFormats::Digits));
        const FString File = Folder / TEXT("creation_slot_1.json");
        Path = TCHAR_TO_UTF8(*File);
    }
    ~PsSlotFixture()
    {
        (void)PsCore::DeleteSaveFiles(Path);
        std::error_code Error;
        std::filesystem::remove(PsNative(Path).parent_path(), Error);
    }
    bool Start()
    {
        return Creator.ApplyPreset("balanced_scholar").Succeeded() && Creator.StartLife().Succeeded();
    }
    PsStorage::CaptureResult Capture() const
    {
        return PsStorage::CaptureWorldSave(Creator.CaptureSave(), Sim.State(), Player);
    }
};

/** Fault fixture writes raw bytes, deliberately bypassing the production validator. */
bool PsRawWrite(const std::string& Path, const std::string_view Text)
{
    std::ofstream Out(PsNative(Path), std::ios::binary | std::ios::trunc);
    Out.write(Text.data(), static_cast<std::streamsize>(Text.size()));
    Out.flush();
    return static_cast<bool>(Out);
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(PsPersistenceRoundTrip, "PerfectWorld.Persistence.RoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool PsPersistenceRoundTrip::RunTest(const FString& Parameters)
{
    PsSlotFixture Slot;
    if (!TestTrue(TEXT("started creation"), Slot.Start())) { return false; }
    const std::string Person(PsCore::HouseholdAdultId);
    if (!TestTrue(TEXT("evening clock"), Slot.Sim.Submit({PsCore::CommandSource::Player, PsCore::AdvanceTime{720}}).Accepted) ||
        !TestTrue(TEXT("open talk"), Slot.Sim.Submit({PsCore::CommandSource::Player,
            PsCore::ConversationOpen{Person, PsCore::PersonAvailability::Awake}}).Accepted) ||
        !TestTrue(TEXT("paid choice"), Slot.Sim.Submit({PsCore::CommandSource::Player,
            PsCore::ConversationChoose{Person, "choice.household_adult.talk_rota", 1}}).Accepted)) { return false; }
    const auto Doors = PsCore::AuthoredDoors();
    if (!TestTrue(TEXT("door catalogue has two leaves"), Doors.size() >= 3)) { return false; }
    TestTrue(TEXT("negative swing"), Slot.Sim.Submit({PsCore::CommandSource::Presentation,
        PsCore::DoorSettle{std::string(Doors[0].Id), true, -1}}).Accepted);
    TestTrue(TEXT("positive swing"), Slot.Sim.Submit({PsCore::CommandSource::Presentation,
        PsCore::DoorSettle{std::string(Doors[2].Id), true, 1}}).Accepted);
    const auto Captured = Slot.Capture();
    if (!TestTrue(TEXT("capture accepted"), Captured.Succeeded()) ||
        !TestTrue(TEXT("slot written"), PsStorage::WriteWorldSlot(Slot.Path, Captured.Text).Ok)) { return false; }
    const auto Loaded = PsStorage::ReadWorldSlot(Slot.Path);
    if (!TestTrue(TEXT("slot read"), Loaded.Succeeded())) { return false; }
    TestTrue(TEXT("v8 certified"), Loaded.Validation.SourceVersion == 8 && Loaded.Validation.HashVerified && !Loaded.Validation.Migrated);
    TestTrue(TEXT("whole world survives"), PsCore::HashWorldState(Loaded.Data->World) == PsCore::HashWorldState(Slot.Sim.State()));
    TestTrue(TEXT("clock survives paid choice"), Loaded.Data->World.Clock.Now() == Slot.Sim.State().Clock.Now());
    TestTrue(TEXT("doors and both swing signs survive"), Loaded.Data->World.Doors == Slot.Sim.State().Doors);
    TestTrue(TEXT("conversation memory and receipts survive"), Loaded.Data->World.Conversations == Slot.Sim.State().Conversations);
    TestTrue(TEXT("placement and camera survive"), Loaded.Data->Player == Slot.Player);
    TestTrue(TEXT("creation survives"), PsCore::SerializeCreationSave(Loaded.Data->Creation) == PsCore::SerializeCreationSave(Slot.Creator.CaptureSave()));

    // Exercise the restore policy on a detached candidate, never the presentation bridge.
    PsCore::WorldSim Restored(Loaded.Data->World);
    const auto SavedConversation = Restored.State().Conversations.at(Person);
    TestTrue(TEXT("saved reply is open"), SavedConversation.Open && !SavedConversation.Receipts.empty());
    TestTrue(TEXT("close without replay"), Restored.Submit({PsCore::CommandSource::Player, PsCore::ConversationLeave{Person}}).Accepted);
    const auto& Closed = Restored.State().Conversations.at(Person);
    TestTrue(TEXT("leave retains memory and receipts"), !Closed.Open && Closed.Facts == SavedConversation.Facts &&
        Closed.TakenChoices == SavedConversation.TakenChoices && Closed.Receipts == SavedConversation.Receipts &&
        Closed.SessionCount == SavedConversation.SessionCount);
    TestFalse(TEXT("old paid callback refused after load"), Restored.Submit({PsCore::CommandSource::Player,
        PsCore::ConversationChoose{Person, "choice.household_adult.talk_rota", 1}}).Accepted);
    TestTrue(TEXT("no second charge"), Restored.State().Clock.Now() == Loaded.Data->World.Clock.Now());
    TestTrue(TEXT("reader left live conversation open"), Slot.Sim.State().Conversations.at(Person).Open);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(PsPersistenceInterrupted, "PerfectWorld.Persistence.InterruptedWrite",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool PsPersistenceInterrupted::RunTest(const FString& Parameters)
{
    PsSlotFixture Slot;
    if (!TestTrue(TEXT("started"), Slot.Start())) { return false; }
    const auto Old = Slot.Capture();
    if (!TestTrue(TEXT("original saved"), PsStorage::WriteWorldSlot(Slot.Path, Old.Text).Ok)) { return false; }
    TestTrue(TEXT("clock moves"), Slot.Sim.Submit({PsCore::CommandSource::Player, PsCore::AdvanceTime{17}}).Accepted);
    const auto Next = Slot.Capture();
    bool PsSawTemp = false;
    const auto PsRejectBeforeRename = [&PsSawTemp, &Slot](std::string_view Text, std::string& Reason)
    {
        PsCore::WorldSaveData Candidate;
        PsSawTemp = PsCore::DeserializeWorldSave(Text, Candidate).Succeeded() &&
            std::filesystem::exists(PsNative(PsCore::SaveTempPath(Slot.Path)));
        Reason = "Simulated interruption after temp flush and before replacement.";
        return false;
    };
    // The exact shared writer used by WriteWorldSlot; its validator runs after temp flush/read-back.
    TestFalse(TEXT("injected write fails"), PsCore::WriteSaveFile(Slot.Path, Next.Text, PsRejectBeforeRename).Ok);
    TestTrue(TEXT("fault reached flushed temp"), PsSawTemp);
    auto Loaded = PsStorage::ReadWorldSlot(Slot.Path);
    TestTrue(TEXT("prior primary survives failure"), Loaded.Succeeded() && PsCore::SerializeWorldSave(*Loaded.Data) == Old.Text);
    TestTrue(TEXT("crash leaves complete uncommitted temp"), PsRawWrite(PsCore::SaveTempPath(Slot.Path), Next.Text));
    Loaded = PsStorage::ReadWorldSlot(Slot.Path);
    TestTrue(TEXT("temp never wins"), Loaded.Succeeded() && PsCore::SerializeWorldSave(*Loaded.Data) == Old.Text);
    std::error_code Error;
    std::filesystem::rename(PsNative(Slot.Path), PsNative(PsCore::SaveBackupPath(Slot.Path)), Error);
    if (!TestFalse(TEXT("simulate power loss between the two renames"), static_cast<bool>(Error))) { return false; }
    Loaded = PsStorage::ReadWorldSlot(Slot.Path);
    TestTrue(TEXT("old backup loads when primary absent"), Loaded.Succeeded() &&
        Loaded.Status == PsCore::SaveReadStatus::RecoveredFromBackup && PsCore::SerializeWorldSave(*Loaded.Data) == Old.Text);
    TestTrue(TEXT("retry writes successfully"), PsStorage::WriteWorldSlot(Slot.Path, Next.Text).Ok);
    const auto Backup = PsCore::ReadSaveFile(PsCore::SaveBackupPath(Slot.Path), PsCore::WorldSaveValidator());
    TestTrue(TEXT("retry retains previous usable backup"), Backup.Succeeded() && Backup.Text == Old.Text);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(PsPersistenceRefusal, "PerfectWorld.Persistence.RefusesDamageWithoutMutation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool PsPersistenceRefusal::RunTest(const FString& Parameters)
{
    PsSlotFixture Slot;
    if (!TestTrue(TEXT("started"), Slot.Start())) { return false; }
    const auto Original = Slot.Capture();
    if (!TestTrue(TEXT("saved"), PsStorage::WriteWorldSlot(Slot.Path, Original.Text).Ok)) { return false; }
    std::string Tampered = Original.Text;
    const auto Hash = Tampered.find("\"stateHash\": \"0x");
    if (!TestTrue(TEXT("hash located"), Hash != std::string::npos)) { return false; }
    const auto Digit = Hash + std::string_view("\"stateHash\": \"0x").size();
    Tampered[Digit] = Tampered[Digit] == '0' ? '1' : '0';
    for (const std::string& Broken : {Tampered, Original.Text.substr(0, Original.Text.size() / 2)})
    {
        TestTrue(TEXT("fault file written"), PsRawWrite(Slot.Path, Broken));
        const auto Loaded = PsStorage::ReadWorldSlot(Slot.Path);
        TestTrue(TEXT("refused with no candidate and player message"), !Loaded.Succeeded() && !Loaded.Data && !Loaded.Message.empty());
        TestTrue(TEXT("live world and creator untouched"), Slot.Capture().Text == Original.Text);
        TestFalse(TEXT("invalid text refused on write"), PsStorage::WriteWorldSlot(Slot.Path, Broken).Ok);
    }
    TestTrue(TEXT("restore valid fixture"), PsRawWrite(Slot.Path, Original.Text));
    auto BadWorld = Slot.Sim.State();
    BadWorld.Doors.Doors.front().SwingSide = 9;
    const auto Refused = PsStorage::CaptureWorldSave(Slot.Creator.CaptureSave(), BadWorld, Slot.Player);
    TestTrue(TEXT("invalid lane yields no snapshot"), !Refused.Succeeded() && Refused.Text.empty() && !Refused.Data);
    TestFalse(TEXT("empty snapshot cannot replace slot"), PsStorage::WriteWorldSlot(Slot.Path, Refused.Text).Ok);
    auto BadPlacement = Slot.Player;
    BadPlacement.PitchCdeg = 9001;
    TestFalse(TEXT("placement validated during capture"), PsStorage::CaptureWorldSave(Slot.Creator.CaptureSave(), Slot.Sim.State(), BadPlacement).Succeeded());
    auto BadCreation = Slot.Creator.CaptureSave();
    BadCreation.Age = 3;
    TestFalse(TEXT("creation validated during capture"), PsStorage::CaptureWorldSave(BadCreation, Slot.Sim.State(), Slot.Player).Succeeded());
    auto Unsettled = Slot.Sim.State();
    TestTrue(TEXT("due-now fixture event scheduled"), Unsettled.Clock.Schedule(Unsettled.Clock.Now(), "sys/persistence/test") > 0);
    TestFalse(TEXT("unsettled capture refused"), PsStorage::CaptureWorldSave(Slot.Creator.CaptureSave(), Unsettled, Slot.Player).Succeeded());
    const auto Loaded = PsStorage::ReadWorldSlot(Slot.Path);
    TestTrue(TEXT("prior file survived refusals"), Loaded.Succeeded() && PsCore::SerializeWorldSave(*Loaded.Data) == Original.Text);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(PsPersistenceBackup, "PerfectWorld.Persistence.BackupRecoveryAndRepeatedSave",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool PsPersistenceBackup::RunTest(const FString& Parameters)
{
    PsSlotFixture Slot;
    if (!TestTrue(TEXT("started"), Slot.Start())) { return false; }
    const auto First = Slot.Capture();
    if (!TestTrue(TEXT("first save"), PsStorage::WriteWorldSlot(Slot.Path, First.Text).Ok)) { return false; }
    std::string Previous = First.Text;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        TestTrue(TEXT("advance"), Slot.Sim.Submit({PsCore::CommandSource::Player, PsCore::AdvanceTime{1}}).Accepted);
        const auto Next = Slot.Capture();
        if (!TestTrue(TEXT("repeated replacement"), PsStorage::WriteWorldSlot(Slot.Path, Next.Text).Ok)) { return false; }
        const auto Backup = PsCore::ReadSaveFile(PsCore::SaveBackupPath(Slot.Path), PsCore::WorldSaveValidator());
        TestTrue(TEXT("backup is previous primary"), Backup.Succeeded() && Backup.Text == Previous);
        Previous = Next.Text;
    }
    TestTrue(TEXT("damage primary"), PsRawWrite(Slot.Path, "damaged"));
    const auto Recovered = PsStorage::ReadWorldSlot(Slot.Path);
    if (!TestTrue(TEXT("backup recovery reported"), Recovered.Succeeded() &&
        Recovered.Status == PsCore::SaveReadStatus::RecoveredFromBackup && !Recovered.Message.empty())) { return false; }
    const auto RecoveryText = PsCore::SerializeWorldSave(*Recovered.Data);
    // Block temp creation to fail a retry while a damaged primary and good backup coexist.
    std::error_code Error;
    std::filesystem::create_directory(PsNative(PsCore::SaveTempPath(Slot.Path)), Error);
    if (!TestFalse(TEXT("temp obstruction created"), static_cast<bool>(Error))) { return false; }
    TestFalse(TEXT("retry fails safely"), PsStorage::WriteWorldSlot(Slot.Path, Previous).Ok);
    std::filesystem::remove(PsNative(PsCore::SaveTempPath(Slot.Path)), Error);
    const auto StillUsable = PsStorage::ReadWorldSlot(Slot.Path);
    TestTrue(TEXT("only usable backup protected"), StillUsable.Succeeded() && PsCore::SerializeWorldSave(*StillUsable.Data) == RecoveryText);
    TestTrue(TEXT("retry after recovery"), PsStorage::WriteWorldSlot(Slot.Path, Previous).Ok);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(PsPersistenceLegacy, "PerfectWorld.Persistence.LegacyCreatorAndWorldVersions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool PsPersistenceLegacy::RunTest(const FString& Parameters)
{
    PsSlotFixture Slot;
    if (!TestTrue(TEXT("started"), Slot.Start())) { return false; }
    const auto Current = Slot.Capture();
    if (!TestTrue(TEXT("create directory"), PsStorage::WriteWorldSlot(Slot.Path, Current.Text).Ok)) { return false; }
    const std::string CreatorText = PsCore::SerializeCreationSave(Slot.Creator.CaptureSave());
    for (int32 Version = 1; Version <= PsCore::WorldSaveLastCreationOnlyVersion; ++Version)
    {
        std::string Legacy = CreatorText;
        const auto Schema = Legacy.find("\"schemaVersion\": 5");
        if (!TestTrue(TEXT("creator schema found"), Schema != std::string::npos)) { return false; }
        Legacy.replace(Schema, std::string_view("\"schemaVersion\": 5").size(), "\"schemaVersion\": " + std::to_string(Version));
        TestTrue(TEXT("legacy fixture written"), PsRawWrite(Slot.Path, Legacy));
        const auto Loaded = PsStorage::ReadWorldSlot(Slot.Path);
        TestTrue(TEXT("creator-only migration metadata"), Loaded.Succeeded() && Loaded.Validation.Migrated &&
            Loaded.Validation.SourceVersion == Version && !Loaded.Validation.HashVerified);
        if (Loaded.Data)
        {
            TestTrue(TEXT("fresh origin world"), Loaded.Data->Creation.HasStarted && Loaded.Data->World.Clock.Now() == 0 &&
                Loaded.Data->World.Doors == PsCore::MakeInitialDoorState() && Loaded.Data->World.Conversations.empty() &&
                Loaded.Data->Player.ZoneId.starts_with("zone.home."));
        }
        TestTrue(TEXT("legacy file never rewritten"), PsCore::ReadSaveFile(Slot.Path).Text == Legacy);
    }
    for (int32 Version : {6, 7})
    {
        std::string Historical = Current.Text;
        const auto Begin = Historical.find(Version == 6 ? ",\n    \"dayloop\":" : ",\n    \"conversations\":");
        const auto End = Historical.find("\n  }", Begin);
        if (!TestTrue(TEXT("historical sections found"), Begin != std::string::npos && End != std::string::npos)) { return false; }
        Historical.erase(Begin, End - Begin);
        Historical.replace(Historical.find("\"schemaVersion\": 8"), std::string_view("\"schemaVersion\": 8").size(),
            "\"schemaVersion\": " + std::to_string(Version));
        const auto HashVersion = Historical.find("\"hashVersion\": 5");
        if (!TestTrue(TEXT("historical hash version found"), HashVersion != std::string::npos)) { return false; }
        Historical.replace(HashVersion, std::string_view("\"hashVersion\": 5").size(),
            "\"hashVersion\": " + std::to_string(Version == 6 ? 3 : 4));
        TestTrue(TEXT("historical world written"), PsRawWrite(Slot.Path, Historical));
        const auto Loaded = PsStorage::ReadWorldSlot(Slot.Path);
        TestTrue(TEXT("historical world loads without certification"), Loaded.Succeeded() &&
            Loaded.Validation.SourceVersion == Version && !Loaded.Validation.Migrated && !Loaded.Validation.HashVerified);
    }
    const PsCore::CharacterCreator Draft;
    TestTrue(TEXT("legacy draft file written"), PsRawWrite(Slot.Path, PsCore::SerializeCreationSave(Draft.CaptureSave())));
    const auto LoadedDraft = PsStorage::ReadWorldSlot(Slot.Path);
    TestTrue(TEXT("draft remains unstarted"), LoadedDraft.Succeeded() && LoadedDraft.Validation.Migrated &&
        !LoadedDraft.Data->Creation.HasStarted);
    return true;
}

#endif
