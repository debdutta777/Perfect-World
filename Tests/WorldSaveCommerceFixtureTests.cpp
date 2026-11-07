// R8-04: frozen historical saves through the commerce migration. world_v9_doors_29.json was written by the UNCHANGED
// Tools/DoorFixtures generator built against `git archive 5ffc9dda1ee7ca4f37d25b372699c37b0d7b3d79` (schema 9, hash 6,
// 29 doors), the parent of the commerce persistence change. world_v9_doors_29_migrated_v10.json is its expected migrated
// result as this build's writer saves it; set PW_WRITE_MIGRATED=<path> to write that text (only when the migration is
// meant to change, for example when a new door roster edition is appended).
#include "WorldSave.h"

#include "WorldHash.h"
#include "WorldReducer.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <tuple>

using namespace PerfectWorld;

#include "WorldSaveTestSupport.h"

void TestWorldSaveCommerceFixtures();

#ifdef PW_COMMERCE_FIXTURE_DIR
namespace
{
std::string R84Fixture(const char* name)
{
    std::ifstream in(std::string(PW_COMMERCE_FIXTURE_DIR) + "/" + name, std::ios::binary);
    Check(static_cast<bool>(in), "the frozen fixture is readable");
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

void R84FrozenV9()
{
    const std::string text = R84Fixture("world_v9_doors_29.json");
    Check(text.size() == 4660 && text.find("\"schemaVersion\": 9,") != std::string::npos &&
              text.find("\"hashVersion\": 6,") != std::string::npos && text.find("\"commerce\"") == std::string::npos,
          "the frozen v9 fixture is the untouched historical writer's output");
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(text, loaded);
    Check(result.Succeeded() && result.SourceVersion == 9 && !result.HashVerified && !result.Migrated,
          "frozen v9 authenticates its hash-6 checksum and loads honestly unverified");
    const GameInstant saved = loaded.World.Clock.Now();
    Check(saved == 760 && loaded.World.Seq == 13, "the fixture's clock and command count");
    const CommerceState& commerce = loaded.World.Commerce;
    Check(commerce.Funding == CommerceFundingPolicy{1, 15000, 15000} && commerce.WalletPaise == 15000 && commerce.InitialisedAt == 760 &&
              commerce.AllowanceSettledThrough == 760 && commerce.NextAllowanceDue == 760 + CommerceWeekMinutes &&
              commerce.TransactionRevision == 0 && commerce.RecentReceipts.empty() && commerce.Inventory == CommerceState{}.Inventory &&
              commerce.FoldedReceiptCount == 0 && commerce.NextTeaEligibleAt == 760 && commerce.NextConsumptionEligibleAt == 760,
          "rank 1 gets its starting wallet once, an empty bag and its first allowance a week after the save");
    Check(loaded.World.Journal.Encounters.size() == 1 && loaded.World.Journal.Encounters[0].FirstMetMinute == 720,
          "the journal survives");
    Check(loaded.World.Conversations.size() == 1, "the conversation survives");
    Check(QueryDoorOpen(loaded.World.Doors, "door.nabapara.home_front") == true && loaded.World.Doors.Doors[0].SwingSide == -1 &&
              QueryDoorOpen(loaded.World.Doors, "door.nabapara.care_common") == false,
          "the door facts survive");

    const std::string migrated = SerializeWorldSave(loaded);
    if (const char* path = std::getenv("PW_WRITE_MIGRATED"); path != nullptr && path[0] != '\0')
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << migrated;
        Check(static_cast<bool>(out), "the migrated result is written");
    }
    const std::string frozen = R84Fixture("world_v9_doors_29_migrated_v10.json");
    Check(migrated == frozen, "the migration produces exactly the frozen expected v10 save");
    WorldSaveData frozenLoaded;
    const WorldSaveResult frozenResult = DeserializeWorldSave(frozen, frozenLoaded);
    Check(frozenResult.Succeeded() && frozenResult.HashVerified && frozenResult.SourceVersion == 10 && frozenLoaded.World.Commerce == commerce,
          "the expected migrated save is a certified v10 world");
    CheckSame(loaded, frozenLoaded, "the expected migrated save holds the migrated world");

    for (std::size_t cut = 0; cut < text.size(); ++cut)
        Check(ErrorOf(text.substr(0, cut)) != WorldSaveError::None, "every prefix of the v9 fixture is refused");
    Check(ErrorOf(ReplaceOnce(text, "encounter=26450,720,", "encounter=26450,721,")) == WorldSaveError::HashMismatch,
          "a tampered journal minute fails the historical checksum");
    Check(ErrorOf(ReplaceOnce(text, "home_front,1,-1", "home_front,1,1")) == WorldSaveError::HashMismatch,
          "a tampered door fact fails the historical checksum");
    Check(ErrorOf(ReplaceOnce(text, "\"seed\": \"0x000000004e4c3031\"", "\"seed\": \"0x000000004e4c3032\"")) == WorldSaveError::HashMismatch,
          "the rest of the historical world is authenticated");
    Check(ErrorOf(ReplaceOnce(text, "\"stateHash\": \"0x49b5048af20be33a\"", "\"stateHash\": \"0x49b5048af20be33b\"")) == WorldSaveError::HashMismatch,
          "a tampered checksum is refused");
    Check(ErrorOf(ReplaceOnce(text, "\"journal\": ", "\"commerce\": \"pw.commerce/1\\n\",\n    \"journal\": ")) == WorldSaveError::Malformed,
          "a v9 record cannot smuggle in commerce");
    Check(ErrorOf(ReplaceOnce(text, "\"schemaVersion\": 9,", "\"schemaVersion\": 10,")) == WorldSaveError::Malformed,
          "a v9 record relabelled v10 lacks commerce");
    WorldSaveData relabelled;
    const WorldSaveResult relabelledResult = DeserializeWorldSave(ReplaceOnce(text, "\"hashVersion\": 6,", "\"hashVersion\": 7,"), relabelled);
    Check(relabelledResult.Succeeded() && !relabelledResult.HashVerified, "relabelling the hash version never earns certification");
}

/** Every earlier frozen save still loads, authenticated in its own encoding, and receives the same one-time initialisation. */
void R84EarlierFixtures()
{
    for (const auto& [name, schema] : {std::tuple{"world_v8_doors_10.json", 8}, std::tuple{"world_v8_doors_11.json", 8},
                                       std::tuple{"world_v8_doors_16.json", 8}, std::tuple{"world_v8_doors_20.json", 8},
                                       std::tuple{"world_v9_doors_23.json", 9}, std::tuple{"world_v9_doors_26.json", 9},
                                       std::tuple{"world_v9_doors_29.json", 9}})
    {
        const std::string text = R84Fixture(name);
        WorldSaveData loaded;
        const WorldSaveResult result = DeserializeWorldSave(text, loaded);
        Check(result.Succeeded() && result.SourceVersion == schema && !result.HashVerified && !result.Migrated, "an earlier fixture loads honestly");
        const std::int32_t rank = loaded.Creation.TraitRanks[2];
        Check(loaded.World.Commerce == InitialiseCommerce(rank, loaded.World.Clock.Now()).State, "an earlier fixture gets its household's commerce");
        WorldSaveData again;
        Check(DeserializeWorldSave(SerializeWorldSave(loaded), again).HashVerified && again.World.Commerce == loaded.World.Commerce,
              "an earlier fixture's first save is certified v10 with the same commerce");
        Check(ErrorOf(ReplaceOnce(text, "home_front,1,-1", "home_front,1,1")) == WorldSaveError::HashMismatch,
              "an earlier fixture is still authenticated before migrating");
    }
}
} // namespace

void TestWorldSaveCommerceFixtures()
{
    R84FrozenV9();
    R84EarlierFixtures();
}
#else
void TestWorldSaveCommerceFixtures()
{
    Check(false, "PW_COMMERCE_FIXTURE_DIR must be defined by CMakeLists.txt for the frozen commerce fixtures");
}
#endif
