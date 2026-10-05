#include "CharacterCreation.h"
#include "CreationSaveSerialization.h"
#include "GateSites.h"
#include "WorldHash.h"
#include "WorldSave.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace PerfectWorld;
namespace fs = std::filesystem;

namespace
{
fs::path WorldSaveTestFolder;

void WorldSaveTestCleanup()
{
    if (!WorldSaveTestFolder.empty())
    {
        std::error_code ec;
        fs::remove_all(WorldSaveTestFolder, ec);
    }
}
} // namespace

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        WorldSaveTestCleanup();
        std::exit(1);
    }
}

#include "WorldSaveTestSupport.h"

namespace
{
// ---- Round trip, truncation, corruption ----

void TestRoundTrip()
{
    for (const WorldSaveData& saved : {RichSave(11, 60), RichSave(12, 5), EdgeSave(), []
                                       {
                                           WorldSaveData fresh;
                                           Check(BeginWorldFromCreation(MakeCreation("parent_household"), 77, fresh).Succeeded(), "begin a world");
                                           return fresh;
                                       }()})
    {
        const std::string text = SerializeWorldSave(saved);
        Check(text.front() == '{' && text.back() == '}', "a save is one object with no trailing newline");
        WorldSaveData loaded = Sentinel();
        const WorldSaveResult result = DeserializeWorldSave(text, loaded);
        Check(result.Succeeded() && !result.Migrated && result.SourceVersion == 6 && result.HashVerified, "a v6 save loads, verified");
        CheckSame(saved, loaded, "a v6 save round-trips field by field");
        Check(HashWorldState(loaded.World) == HashWorldState(saved.World), "the loaded hash equals the saved hash");
        Check(SerializeWorldSave(loaded) == text, "saving what was loaded gives byte-identical text");
        Check(ErrorOf(text + "\n") == WorldSaveError::None && ErrorOf(text + "  \r\n\n") == WorldSaveError::None, "trailing whitespace is tolerated");
    }
    Check(SerializeWorldSave(RichSave(11, 60)) == SerializeWorldSave(RichSave(11, 60)), "saving is deterministic");
    const WorldSaveData rich = RichSave(11, 60);
    Check(!rich.World.Flags.empty() && !rich.World.Gates.empty() && !rich.World.Clock.Pending().empty(), "the rich world has flags, Gates and events");
    Check(rich.World.Rng != MakeWorldState(11).Rng, "the rich world's streams have been drawn from");
    const std::string text = SerializeWorldSave(rich);
    Check(text.find("\"schemaVersion\": 6") != std::string::npos && text.find("\"format\": \"perfectworld.save\"") != std::string::npos, "the envelope names its format and version");
    Check(text.find("\"camera\": \"third_person\"") != std::string::npos && text.find("\"stream\": \"crystallisation\"") != std::string::npos, "camera and streams are written by stable name");
    Check(!CameraModeName(CameraMode::FirstPerson).empty() && ParseCameraMode("third_person") == CameraMode::ThirdPerson && !ParseCameraMode("fps"),
          "camera modes have stable names");
}

void TestTruncation()
{
    for (const WorldSaveData& saved : {RichSave(21, 40), EdgeSave(), RichSave(22, 0)})
    {
        const std::string text = SerializeWorldSave(saved);
        for (std::size_t cut = 0; cut < text.size(); ++cut)
        {
            WorldSaveData out = Sentinel();
            const WorldSaveResult result = DeserializeWorldSave(std::string_view(text).substr(0, cut), out);
            if (result.Succeeded() || !Untouched(out) || result.Message.empty())
            {
                std::cerr << "prefix of " << cut << " of " << text.size() << " bytes was not refused\n";
                Check(false, "every proper prefix of a v6 save is refused");
            }
        }
        Check(ErrorOf(text) == WorldSaveError::None, "the whole text loads");
    }
    for (const char* text : {"", " ", "{", "{}", "[]", "null", "not json", "{\"format\": \"perfectworld.save\"}", "{\"schemaVersion\": 6}"})
        Check(ErrorOf(text) != WorldSaveError::None, "garbage is refused");
}

void TestCorruption()
{
    const WorldSaveData rich = RichSave(31, 50);
    const std::string text = SerializeWorldSave(rich);
    const std::size_t flagsAt = text.find("\"flags\"");
    Check(flagsAt != std::string::npos, "flags section found");
    Check(ErrorOf(ReplaceOnce(text, "true", "false", flagsAt)) == WorldSaveError::HashMismatch,
          "a flipped flag that still parses fails the hash check");
    Check(ErrorOf(ReplaceOnce(text, "\"onsets\": ", "\"onsets\": 1", text.find("\"gates\""))) == WorldSaveError::HashMismatch,
          "a changed Gate field that still parses fails the hash check");
    Check(ErrorOf(ReplaceOnce(text, "\"seq\": \"0x", "\"seq\": \"0x1")) == WorldSaveError::Malformed, "a hex value of the wrong length is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"0x", "\"0X")) == WorldSaveError::Malformed, "hex digits must be lowercase with a 0x prefix");
    Check(ErrorOf(ReplaceOnce(text, "\"schemaVersion\": 6", "\"schemaVersion\": 7")) == WorldSaveError::UnsupportedVersion, "a newer envelope is refused as unsupported");
    Check(ErrorOf(ReplaceOnce(text, "\"schemaVersion\": 6", "\"schemaVersion\": 5")) == WorldSaveError::UnsupportedVersion, "an envelope version never written is refused");
    Check(ErrorOf(ReplaceOnce(text, "perfectworld.save", "other.save")) == WorldSaveError::Malformed, "a different format is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"hashVersion\"", "\"hashVersionX\"")) == WorldSaveError::Malformed, "an unknown top-level key is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"seq\": ", "\"seed\": \"0x0000000000000000\", \"seq\": ")) == WorldSaveError::Malformed, "a duplicate key is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"vitality\"", "\"vitalityX\"")) == WorldSaveError::Malformed, "a renamed trait in the creation record is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"householdTemplateId\": \"grandparent_household\"", "\"householdTemplateId\": \"penthouse\"")) == WorldSaveError::InvalidCreation,
          "an unknown household in the creation record is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"camera\": \"third_person\"", "\"camera\": \"over_the_shoulder\"")) == WorldSaveError::InvalidWorld, "an unknown camera mode is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"zone\": \"zone.street", "\"zone\": \"Zone Street")) == WorldSaveError::InvalidWorld, "a bad zone ID is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"yawCdeg\": 9000", "\"yawCdeg\": 36000")) == WorldSaveError::InvalidWorld, "yaw is below one full turn");
    Check(ErrorOf(ReplaceOnce(text, "\"pitchCdeg\": -1234", "\"pitchCdeg\": 9001")) == WorldSaveError::InvalidWorld, "pitch is within a quarter turn");
    Check(ErrorOf(ReplaceOnce(text, "\"posCm\": [120", "\"posCm\": [10000000001")) == WorldSaveError::InvalidWorld, "a position outside the world is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"posCm\": [120", "\"posCm\": [1.5")) == WorldSaveError::Malformed, "a fractional position is refused: no floats in saves");
    Check(ErrorOf(ReplaceOnce(text, "\"stream\": \"tower\"", "\"stream\": \"opportunity\"")) == WorldSaveError::InvalidWorld, "a repeated stream is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"stream\": \"tower\"", "\"stream\": \"weather\"")) == WorldSaveError::InvalidWorld, "an unknown stream is refused");
    const std::size_t wordsAt = text.find("\"words\": [");
    const std::string zeros = "\"0x0000000000000000\"";
    std::string zeroed = text;
    zeroed.replace(wordsAt, text.find(']', wordsAt) - wordsAt + 1, "\"words\": [" + zeros + ", " + zeros + ", " + zeros + ", " + zeros + "]");
    Check(ErrorOf(zeroed) == WorldSaveError::InvalidWorld, "an all-zero stream state is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"now\": ", "\"now\": 9")) == WorldSaveError::InvalidWorld, "a clock past its own scheduled events is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"rank\": \"C\"", "\"rank\": \"Z\"")) == WorldSaveError::InvalidWorld, "an unknown Gate rank is refused");

    // A hash from a different hash version cannot be checked, so the file loads unverified.
    WorldSaveData loaded = Sentinel();
    const std::string currentHashVersion = "\"hashVersion\": " + std::to_string(WorldHashVersion);
    const std::string olderHashVersion = "\"hashVersion\": " + std::to_string(WorldHashVersion - 1);
    const WorldSaveResult unverified = DeserializeWorldSave(ReplaceOnce(text, currentHashVersion, olderHashVersion), loaded);
    Check(unverified.Succeeded() && !unverified.HashVerified, "another hash version loads without the checksum comparison");
    CheckSame(rich, loaded, "and loads the same world");

    // A stream added by a later build is missing from an older file: it starts from the world seed.
    WorldSaveData partial = rich;
    std::string withoutLast = text;
    const std::size_t lastStream = withoutLast.find(",\n      { \"stream\": \"crystallisation\"");
    Check(lastStream != std::string::npos, "last stream found");
    withoutLast.erase(lastStream, withoutLast.find("}", lastStream) - lastStream + 1);
    withoutLast = ReplaceOnce(withoutLast, currentHashVersion, olderHashVersion);
    Check(DeserializeWorldSave(withoutLast, partial).Succeeded() && partial.World.Rng[3] == RandomStream(DeriveRandomStreamSeed(rich.World.WorldSeed, RandomStreamId::Crystallisation)).Capture() &&
              partial.World.Rng[0] == rich.World.Rng[0],
          "a missing stream is seeded from the world seed, the others kept");

    // Any single damaged byte is refused, or else leaves the world exactly as saved (the hash covers the whole world).
    const std::uint64_t want = HashWorldState(rich.World);
    std::size_t refused = 0;
    std::size_t accepted = 0;
    for (const char replacement : {'#', '0', ' '})
    {
        for (std::size_t at = 0; at < text.size(); ++at)
        {
            if (text[at] == replacement)
                continue;
            std::string damaged = text;
            damaged[at] = replacement;
            WorldSaveData out = Sentinel();
            if (DeserializeWorldSave(damaged, out).Succeeded())
            {
                ++accepted;
                Check(HashWorldState(out.World) == want, "a damaged byte never changes the world without being noticed");
            }
            else
            {
                ++refused;
                Check(Untouched(out), "a refused damaged save leaves the target untouched");
            }
        }
    }
    Check(refused > accepted, "most damaged bytes are refused");
}

// ---- Migration of creation-only files ----

std::string RanksText(const std::array<int, TraitCount>& ranks)
{
    std::string out = "  \"traitRanks\": {\n";
    const auto& definitions = CharacterCreator::TraitDefinitions();
    for (std::size_t i = 0; i < TraitCount; ++i)
        out += "    \"" + std::string(definitions[i].Id) + "\": " + std::to_string(ranks[i]) + (i + 1 < TraitCount ? ",\n" : "\n");
    return out + "  }\n}\n";
}

/** A creation-only file as the given version wrote it. Versions 1 to 4 are written out by hand, as those builds laid them out. */
std::string LegacyText(int version, bool started, const std::array<int, TraitCount>& ranks)
{
    std::string out = "{\n  \"schemaVersion\": " + std::to_string(version) + ",\n  \"age\": 16,\n  \"hasStarted\": " + (started ? "true" : "false") + ",\n";
    if (version >= 4)
        out += std::string("  \"resumePoint\": \"") + (started ? "in_world" : "allocation") + "\",\n";
    if (version >= 2)
        out += "  \"householdTemplateId\": \"appointed_guardian\",\n";
    out += "  \"identity\": {\n    \"givenName\": \"Ananya\",\n    \"familyName\": \"Sen\",\n    \"pronouns\": \"she/her\",\n    \"homeCity\": \"Kolkata\",\n    \"primaryLanguage\": \"Bengali\"\n  },\n";
    if (version < 3)
        out += "  \"cosmetics\": { \"hair\": \"long\", \"skin\": \"warm\" },\n";
    else
        out += "  \"appearance\": {\n    \"schemaVersion\": 1,\n    \"faceShapeId\": \"face_heart\",\n    \"hairStyleId\": \"hair_long_braid\",\n    \"buildId\": \"build_sturdy\",\n"
               "    \"uniformVariantId\": \"uniform_casual\",\n    \"skinTone\": { \"r\": 123, \"g\": 84, \"b\": 61 },\n    \"hairColor\": { \"r\": 20, \"g\": 16, \"b\": 14 },\n"
               "    \"eyeColor\": { \"r\": 90, \"g\": 120, \"b\": 60 }\n  },\n";
    return out + RanksText(ranks);
}

std::uint64_t RefFnv1a(const std::string& bytes)
{
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (const char c : bytes)
        h = (h ^ static_cast<std::uint64_t>(static_cast<unsigned char>(c))) * 0x100000001b3ULL;
    return h;
}

void CheckMigrated(const std::string& text, int version, bool started, bool care, const char* what)
{
    WorldSaveData data = Sentinel();
    const WorldSaveResult result = DeserializeWorldSave(text, data);
    Check(result.Succeeded() && result.Migrated && result.SourceVersion == version && !result.HashVerified, what);
    const CalendarTime now = data.World.Clock.Calendar();
    Check(data.World.Clock.Now() == 0 && now.Year == 2042 && now.Month == 6 && now.Day == 2 && now.Hour == 6 && now.Minute == 0, what);
    Check(data.World.Flags.empty() && data.World.Gates.empty() && data.World.Clock.Pending().empty() && data.World.Seq == 0, what);
    Check(data.World.WorldSeed == RefFnv1a(text) && data.World.Rng == RandomStreams(RefFnv1a(text)).Capture(), what);
    const std::string household = version >= 2 ? "appointed_guardian" : "parent_household";
    const std::string kit(care ? InstitutionalHomeKitId : CharacterCreator::FindHousehold(household)->HomeKitId);
    Check(data.Player.ZoneId == "zone.home." + kit && data.Player.ZoneId == OriginHomeZoneId(kit), what);
    Check(data.Player.PositionCm == std::array<std::int64_t, 3>{} && data.Player.Camera == CameraMode::FirstPerson && data.Player.YawCdeg == 0, what);
    Check(data.Creation.HasStarted == started && data.Creation.Identity.GivenName == "Ananya" && data.Creation.HouseholdTemplateId == household, what);
    Check(data.Creation.Appearance.FaceShapeId == (version >= 3 ? "face_heart" : AppearanceRecord::Default().FaceShapeId), what);
    Check(data.Creation.Resume == (started ? ResumePoint::InWorld : ResumePoint::Allocation), what);

    // Migrating the same file again gives the same world, and what it saves loads as a plain v6 file with the same hash.
    WorldSaveData again = Sentinel();
    Check(DeserializeWorldSave(text, again).Succeeded(), what);
    CheckSame(data, again, what);
    WorldSaveData reloaded = Sentinel();
    const WorldSaveResult second = DeserializeWorldSave(SerializeWorldSave(data), reloaded);
    Check(second.Succeeded() && !second.Migrated && second.HashVerified, what);
    CheckSame(data, reloaded, what);

    // A cut-off legacy file is refused, as it always was (the prefix missing only the final newline is a whole record).
    for (std::size_t cut = 0; cut + 2 < text.size(); ++cut)
    {
        WorldSaveData out = Sentinel();
        if (DeserializeWorldSave(std::string_view(text).substr(0, cut), out).Succeeded() || !Untouched(out))
            Check(false, "a truncated creation-only save is refused");
    }
}

void TestMigration()
{
    CharacterCreator scholar;
    Check(scholar.ApplyPreset("balanced_scholar").Succeeded(), "preset");
    std::array<int, TraitCount> scholarRanks{};
    for (std::size_t i = 0; i < TraitCount; ++i)
        scholarRanks[i] = scholar.GetTraitRank(CharacterCreator::TraitDefinitions()[i].Id);
    Check(scholar.GetTraitRank("household_resources") >= 1, "the scholar preset lives in a family household");
    const std::array<int, TraitCount> noRanks{};

    for (int version = 1; version <= 4; ++version)
    {
        CheckMigrated(LegacyText(version, true, scholarRanks), version, true, false, "a started creation-only file migrates");
        CheckMigrated(LegacyText(version, false, noRanks), version, false, true, "an unstarted draft in care migrates to the children's home");
    }
    // The current creation writer's output is the newest creation-only file; when M0-18 adds v5 this becomes the v5 fixture.
    CharacterCreationSave current = MakeCreation("appointed_guardian", "Ananya");
    const std::string currentText = SerializeCreationSave(current);
    CheckMigrated(currentText, SaveSchemaVersion, true, false, "a file from the current creation writer migrates");
    Check(SaveSchemaVersion <= WorldSaveLastCreationOnlyVersion, "creation-only versions stay below the world envelope");

    // The older shapes keep their meaning: v1 gets the default household, v1 and v2 the default appearance, a draft resumes at allocation.
    WorldSaveData v1;
    Check(DeserializeWorldSave(LegacyText(1, true, scholarRanks), v1).Succeeded() && v1.Creation.SchemaVersion == SaveSchemaVersion, "a migrated record is held at the current creation version");

    WorldSaveData begun = Sentinel();
    Check(BeginWorldFromCreation(current, 5, begun).Succeeded() && begun.World.WorldSeed == 5 && begun.World.Rng == RandomStreams(5).Capture() &&
              begun.World.Clock.Now() == 0 && begun.Player.ZoneId == "zone.home.staff_quarters",
          "beginning a life seeds the world and places the player in the household's home");

    // Refusals leave the target alone: wrong age, unknown household, an unsupported creation version.
    WorldSaveData out = Sentinel();
    CharacterCreationSave bad = current;
    bad.Age = 17;
    Check(BeginWorldFromCreation(bad, 1, out).Error == WorldSaveError::InvalidCreation && Untouched(out), "a wrong opening age is refused");
    bad = current;
    bad.HouseholdTemplateId = "penthouse";
    Check(BeginWorldFromCreation(bad, 1, out).Error == WorldSaveError::InvalidCreation && Untouched(out), "an unknown household is refused");
    bad = current;
    bad.SchemaVersion = SaveSchemaVersion + 1;
    Check(BeginWorldFromCreation(bad, 1, out).Error == WorldSaveError::UnsupportedVersion && Untouched(out), "a newer creation version is refused");
    Check(ErrorOf(ReplaceOnce(LegacyText(4, true, scholarRanks), "\"schemaVersion\": 4", "\"schemaVersion\": 99")) == WorldSaveError::UnsupportedVersion, "a creation-only file from the future is refused");
    Check(ErrorOf(ReplaceOnce(LegacyText(4, true, scholarRanks), "\"age\": 16", "\"age\": 17")) == WorldSaveError::InvalidCreation, "a creation-only file with the wrong age is refused");
}

// ---- Random command sequences ----

void TestRandomSequences()
{
    const CharacterCreationSave creation = MakeCreation("parent_household");
    constexpr int Sequences = 300;
    int saves = 0;
    for (int index = 0; index < Sequences; ++index)
    {
        std::mt19937_64 random(7'000'003ULL * static_cast<std::uint64_t>(index + 1));
        const std::uint64_t seed = random();
        WorldSim sim(seed);
        const int length = 10 + static_cast<int>(random() % 50);
        for (int i = 0; i < length; ++i)
        {
            (void)sim.Submit(RandomCommand(random, sim));
            if (i % 9 != 0)
                continue;
            WorldSaveData saved = SaveOf(sim, creation);
            const std::string text = SerializeWorldSave(saved);
            WorldSaveData loaded = Sentinel();
            const WorldSaveResult result = DeserializeWorldSave(text, loaded);
            Check(result.Succeeded() && result.HashVerified, "a mid-sequence save loads");
            Check(HashWorldState(loaded.World) == HashWorldState(sim.State()), "the loaded hash equals the live hash");
            if (index % 10 == 0)
                CheckSame(saved, loaded, "a mid-sequence save round-trips field by field");
            ++saves;
        }
        // The loaded world is the replayed world: the command log reaches the same hash (the reducer draws nothing).
        WorldSaveData endSave = SaveOf(sim, creation);
        WorldSaveData endLoaded = Sentinel();
        Check(DeserializeWorldSave(SerializeWorldSave(endSave), endLoaded).Succeeded(), "the final save loads");
        Check(HashWorldState(endLoaded.World) == ReplayCommandLog(seed, sim.Log()).Hash, "the loaded hash equals the replayed hash");

        // With streams drawn from, the load continues each sequence exactly where the save left it.
        DrawFromStreams(endSave.World, random);
        Check(DeserializeWorldSave(SerializeWorldSave(endSave), endLoaded).Succeeded(), "a save with drawn streams loads");
        Check(HashWorldState(endLoaded.World) == HashWorldState(endSave.World) && endLoaded.World.Rng != sim.State().Rng, "drawn stream states are saved");
        RandomStreams before;
        RandomStreams after;
        Check(before.Restore(endSave.World.Rng) && after.Restore(endLoaded.World.Rng), "stream states restore");
        for (std::size_t s = 0; s < RandomStreamCount; ++s)
        {
            for (int n = 0; n < 5; ++n)
                Check(before.Stream(static_cast<RandomStreamId>(s)).NextU64() == after.Stream(static_cast<RandomStreamId>(s)).NextU64(), "a reload continues the sequence");
        }
    }
    Check(saves > Sequences, "many mid-sequence saves were taken");
}

// ---- The slot store ----

void TestSlotStore()
{
    WorldSaveTestFolder = fs::temp_directory_path() / "pw_world_save_test";
    WorldSaveTestCleanup();
    const std::u8string folder = (WorldSaveTestFolder / "slot_1").u8string();
    const std::string path = std::string(folder.begin(), folder.end()) + "/life.json";
    const WorldSaveData saved = RichSave(41, 30);
    const std::string text = SerializeWorldSave(saved);
    Check(WriteSaveFile(path, text, WorldSaveValidator()).Ok, "a valid world save is written");
    Check(!WriteSaveFile(path, text.substr(0, text.size() / 2), WorldSaveValidator()).Ok, "a truncated save is refused by the store's validator");
    const SaveReadResult read = ReadSaveFile(path, WorldSaveValidator());
    Check(read.Status == SaveReadStatus::Loaded && read.Text == text, "the refused write left the good file in place");
    WorldSaveData loaded = Sentinel();
    Check(DeserializeWorldSave(read.Text, loaded).Succeeded(), "the stored text loads");
    CheckSame(saved, loaded, "the slot round-trips through the disk");
    const std::string newer = SerializeWorldSave(RichSave(42, 30));
    Check(WriteSaveFile(path, newer, WorldSaveValidator()).Ok, "a second save is written");
    Check(ReadSaveFile(path, WorldSaveValidator()).Text == newer, "the second save is the primary");
    WorldSaveTestCleanup();
    WorldSaveTestFolder.clear();
}
} // namespace

int main()
{
    TestRoundTrip();
    TestTruncation();
    TestCorruption();
    TestMigration();
    TestRandomSequences();
    TestSlotStore();
    std::cout << "PASS: " << Checks << " world save checks\n";
    return 0;
}
