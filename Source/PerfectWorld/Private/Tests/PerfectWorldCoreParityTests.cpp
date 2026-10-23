// Cross-build parity: the portable core must give the same answers inside
// Unreal as in the standalone CTest suite (SYSTEMS.md risk table: "CTest and
// UE builds hash differently"). Each test recomputes a value that a CTest
// pins, from the core compiled into this module, and compares. Run them in
// the editor (Session Frontend, or the Unreal MCP AutomationTestToolset)
// under "PerfectWorld.Core".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "AwakenedProgression.h"
#include "ContentRegistry.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RealmGenerator.h"
#include "SaveStore.h"

#include <cstdint>
#include <string>
#include <vector>

namespace PerfectWorldParity
{
// The same seed and fingerprint as Tests/RealmGeneratorTests.cpp.
constexpr std::uint64_t RealmSeed = 0x0DEE'9EA1'2042'0602ULL;

std::uint64_t Fingerprint(const PerfectWorld::RealmChunkTerrain& Terrain)
{
    std::uint64_t Hash = 1469598103934665603ULL;
    auto Add = [&Hash](std::uint64_t Value) {
        for (int Byte = 0; Byte < 8; ++Byte)
        {
            Hash ^= (Value >> (8 * Byte)) & 0xFF;
            Hash *= 1099511628211ULL;
        }
    };
    for (const std::int32_t Height : Terrain.SurfaceHeightMillimetres)
    {
        Add(static_cast<std::uint64_t>(static_cast<std::int64_t>(Height)));
    }
    Add(static_cast<std::uint64_t>(Terrain.Theme));
    Add(static_cast<std::uint64_t>(Terrain.Danger));
    Add(Terrain.Feature ? 1 + static_cast<std::uint64_t>(Terrain.Feature->Kind) * 1024 + Terrain.Feature->LocalBlockX * 32 +
                              Terrain.Feature->LocalBlockY
                        : 0);
    return Hash;
}
} // namespace PerfectWorldParity

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerfectWorldRealmGeneratorParityTest, "PerfectWorld.Core.RealmGeneratorMatchesCTest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerfectWorldRealmGeneratorParityTest::RunTest(const FString& Parameters)
{
    using namespace PerfectWorld;
    TestEqual(TEXT("golden height at the origin"), RealmSurfaceHeightMillimetres(PerfectWorldParity::RealmSeed, 1, 0, 0), -185530);
    TestEqual(TEXT("golden height at (1000, 2000)"), RealmSurfaceHeightMillimetres(PerfectWorldParity::RealmSeed, 1, 1000, 2000), 86204);

    const std::int64_t Edge = RealmFloorChunksPerEdge(1);
    const RealmChunk Chunks[] = {{1, 0, 0}, {1, 1, 0}, {1, 12345, 67890}, {1, Edge - 1, Edge - 1}, {3, 500, 500}, {12, 7, 9}};
    std::uint64_t Combined = 0;
    for (const RealmChunk& Chunk : Chunks)
    {
        const std::optional<RealmChunkTerrain> Terrain = GenerateRealmChunk(PerfectWorldParity::RealmSeed, Chunk);
        if (!TestTrue(TEXT("chunk generates"), Terrain.has_value()))
        {
            return false;
        }
        Combined = Combined * 31 + PerfectWorldParity::Fingerprint(*Terrain);
    }
    TestEqual(TEXT("six-chunk fingerprint matches the CTest golden 0x1c7660d9a8f60d2f"), static_cast<uint64>(Combined),
              static_cast<uint64>(0x1c7660d9a8f60d2fULL));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerfectWorldAwakenedCareerParityTest, "PerfectWorld.Core.AwakenedCareerMatchesCTest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerfectWorldAwakenedCareerParityTest::RunTest(const FString& Parameters)
{
    using namespace PerfectWorld;
    // The maximum-effort career from Tests/AwakenedProgressionTests.cpp: four
    // hours on every dimension every day, every assessment passed at once.
    std::array<std::int32_t, PowerDimensionCount> Start{};
    Start.fill(40);
    std::optional<AwakenedProfile> Profile = Awaken({20, 20, 20, 20, 20}, Start, 0);
    if (!TestTrue(TEXT("awakening succeeds"), Profile.has_value()))
    {
        return false;
    }
    std::int64_t Day = 0;
    while (Profile->Grade != AwakenedGrade::S && Day < 40 * 365)
    {
        ++Day;
        for (std::int32_t Dimension = 0; Dimension < PowerDimensionCount; ++Dimension)
        {
            Practise(*Profile, static_cast<PowerDimension>(Dimension), 240, Day, 150, true);
        }
        if (CheckAssessmentEligibility(*Profile, Day, true) == AssessmentEligibility::Eligible)
        {
            SitAssessment(*Profile, Day, true, true);
        }
    }
    TestTrue(TEXT("S is reached"), Profile->Grade == AwakenedGrade::S);
    TestEqual(TEXT("S on the same game day as CTest (1079)"), static_cast<int64>(Day), static_cast<int64>(1079));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerfectWorldContentParityTest, "PerfectWorld.Core.ContentLoadsInEngine",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerfectWorldContentParityTest::RunTest(const FString& Parameters)
{
    using namespace PerfectWorld;
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/Content"));
    TArray<FString> Paths;
    IFileManager::Get().FindFilesRecursive(Paths, *Root, TEXT("*.json"), true, false);
    Paths.Sort();

    std::vector<ContentFile> Files;
    for (const FString& Path : Paths)
    {
        FString Text;
        if (!TestTrue(FString::Printf(TEXT("read %s"), *Path), FFileHelper::LoadFileToString(Text, *Path)))
        {
            return false;
        }
        FString Relative = Path;
        FPaths::MakePathRelativeTo(Relative, *(Root + TEXT("/")));
        Files.push_back({std::string("/") + TCHAR_TO_UTF8(*Relative), std::string(TCHAR_TO_UTF8(*Text))});
    }

    const ContentLoadResult Result = LoadContent(Files);
    for (const ContentIssue& Issue : Result.Issues)
    {
        AddError(FString::Printf(TEXT("%s %s: %s"), UTF8_TO_TCHAR(Issue.File.c_str()), UTF8_TO_TCHAR(Issue.Id.c_str()),
                                 UTF8_TO_TCHAR(Issue.Message.c_str())));
    }
    TestTrue(TEXT("Data/Content validates inside Unreal"), Result.Ok());
    TestEqual(TEXT("179 content files, as pwcontent reports"), static_cast<int32>(Files.size()), 179);
    TestEqual(TEXT("215 records, as pwcontent reports"), static_cast<int32>(Result.Db.Count()), 215);
    return true;
}

// The slot store as the game uses it: an absolute UTF-8 path from Unreal's
// project folders, here under a Bengali folder name (Tests/SaveStoreTests.cpp
// covers the same mechanics in CTest).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerfectWorldSaveStoreParityTest, "PerfectWorld.Core.SaveStoreWorksInEngine",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPerfectWorldSaveStoreParityTest::RunTest(const FString& Parameters)
{
    using namespace PerfectWorld;
    const FString Folder = FPaths::ConvertRelativePathToFull(
        FPaths::ProjectIntermediateDir() / TEXT("AutomationSaves") / TEXT("কলকাতা"));
    IFileManager::Get().DeleteDirectory(*Folder, false, true);
    const FString PrimaryPath = Folder / TEXT("slot_test.json");
    const std::string Primary = TCHAR_TO_UTF8(*PrimaryPath);
    const SaveValidator Validator = [](std::string_view Text, std::string& Error) {
        if (Text.substr(0, 3) != "pw:")
        {
            Error = "not a test record";
            return false;
        }
        return true;
    };

    TestTrue(TEXT("first write"), WriteSaveFile(Primary, "pw:first", Validator).Ok);
    TestTrue(TEXT("second write"), WriteSaveFile(Primary, "pw:second", Validator).Ok);
    const SaveReadResult Loaded = ReadSaveFile(Primary, Validator);
    TestTrue(TEXT("the primary loads"), Loaded.Status == SaveReadStatus::Loaded && Loaded.Text == "pw:second");
    TestTrue(TEXT("the previous version is kept as .bak"), IFileManager::Get().FileExists(*(PrimaryPath + TEXT(".bak"))));

    TestFalse(TEXT("an invalid record is refused"), WriteSaveFile(Primary, "garbage", Validator).Ok);
    TestEqual(TEXT("a refused write leaves the slot alone"), FString(UTF8_TO_TCHAR(ReadSaveFile(Primary, Validator).Text.c_str())),
              FString(TEXT("pw:second")));

    FFileHelper::SaveStringToFile(TEXT("garbage"), *PrimaryPath);
    const SaveReadResult Recovered = ReadSaveFile(Primary, Validator);
    TestTrue(TEXT("a corrupt primary falls back to .bak"),
             Recovered.Status == SaveReadStatus::RecoveredFromBackup && Recovered.Text == "pw:first" && !Recovered.Message.empty());

    TestTrue(TEXT("delete"), DeleteSaveFiles(Primary).Ok);
    TestTrue(TEXT("a deleted slot is missing"), ReadSaveFile(Primary, Validator).Status == SaveReadStatus::Missing);
    IFileManager::Get().DeleteDirectory(*Folder, false, true);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
