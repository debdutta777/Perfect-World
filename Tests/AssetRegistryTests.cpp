#include "AssetRegistry.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace PerfectWorld;

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

namespace
{
const char* const AssetDoorContract = R"JSON({
  "schema": "pw.contract/1", "family": "arch.door", "version": 1, "category": "arch", "subcategories": ["door"], "texel_class": "A",
  "dimensions_mm": { "min": [50, 800, 2100], "max": [200, 1200, 2100] }, "exact_height_mm": 2100,
  "requires": { "solid": true, "interactions": 1, "seats": 0, "standing_anchor": true, "camera_clearance": true, "seated_clearance": false,
                "anchor_height_mm": [350, 1850] },
  "clearance": { "camera_probe_mm": 120, "min_camera_gap_mm": 150, "boom_mm": 2500, "room_mm": [2400, 2400], "min_seated_headroom_mm": 1100 }
})JSON";

const char* const AssetDoor = R"JSON({
  "schema": "pw.asset/1", "id": "arch.door.wooden_panel", "display_name": "Door", "status": "active", "contract": "arch.door@1",
  "milestone": "M1", "tags": ["home"],
  "dimensions_mm": [100, 1000, 2100], "pivot": "bottom_center", "front": "+x",
  "envelope_mm": { "inner": { "min": [-30, -390, 10], "max": [30, 390, 1990] }, "outer": { "min": [-60, -510, 0], "max": [60, 510, 2110] } },
  "block": { "class": "A", "cuboids": [
    { "name": "leaf", "min_mm": [-50, -500, 0], "size_mm": [100, 1000, 2100], "tile": "tile.wood_panel", "tint": "teak" } ] },
  "gameplay": {
    "surface": "mat.surface.wood_teak",
    "collision": [ { "shape": "box", "center_mm": [0, 0, 1050], "half_extent_mm": [50, 500, 1050], "channels": ["visibility", "camera", "pawn"] } ],
    "nav": { "area": "passage" },
    "interactions": [ { "id": "open", "verb": "verb.open", "stance_mm": [700, 0, 0], "facing_deg": 180, "anchor_mm": [30, 300, 1000], "priority": 1 } ],
    "clearance": { "first_person": { "camera_probe_mm": 120, "gap_mm": 150 },
                   "third_person": { "camera_probe_mm": 120, "boom_mm": 2500, "room_mm": [2400, 2400] } }
  },
  "art": { "status": "block", "enabled": true, "mesh": null, "lods": [], "materials": [], "skeleton": null, "import": null, "provenance": null }
})JSON";

const char* const AssetSeatContract = R"JSON({
  "schema": "pw.contract/1", "family": "prop.seat", "version": 1, "category": "prop", "subcategories": ["furniture"], "texel_class": "P",
  "dimensions_mm": { "min": [250, 250, 10], "max": [2400, 2400, 1200] },
  "requires": { "solid": false, "interactions": 1, "seats": 1, "camera_clearance": true, "seated_clearance": true },
  "clearance": { "camera_probe_mm": 120, "min_camera_gap_mm": 150, "boom_mm": 2500, "room_mm": [2400, 2400], "min_seated_headroom_mm": 1100 },
  "seat_poses": ["pose.sit_chair_45", "pose.sit_stool_30"]
})JSON";

const char* const AssetStool = R"JSON({
  "schema": "pw.asset/1", "id": "prop.furniture.stool_wooden_low", "display_name": "Stool", "status": "active", "contract": "prop.seat@1",
  "milestone": "M1", "tags": ["home"],
  "dimensions_mm": [300, 300, 320], "pivot": "bottom_center", "front": "+x",
  "envelope_mm": { "inner": { "min": [-130, -130, 280], "max": [130, 130, 310] }, "outer": { "min": [-340, -340, 0], "max": [340, 340, 420] } },
  "block": { "class": "P", "cuboids": [
    { "name": "top", "min_mm": [-150, -150, 280], "size_mm": [300, 300, 40], "tile": "tile.wood_plank", "tint": "teak" },
    { "name": "leg", "min_mm": [-150, -150, 0], "size_mm": [40, 40, 280], "color": "teak.1" } ] },
  "gameplay": {
    "surface": "mat.surface.wood_teak",
    "collision": [ { "shape": "box", "center_mm": [0, 0, 160], "half_extent_mm": [150, 150, 160], "channels": ["visibility", "pawn"] } ],
    "nav": { "area": "obstacle" },
    "interactions": [ { "id": "sit", "verb": "verb.sit", "stance_mm": [400, 0, 0], "facing_deg": 180, "seat": "seat", "priority": 1 } ],
    "seats": [ { "id": "seat", "pose": "pose.sit_stool_30", "pelvis_mm": [0, 0, 300], "facing_deg": 0, "exit_mm": [400, 0, 0] } ],
    "clearance": { "first_person": { "camera_probe_mm": 120, "gap_mm": 150, "seated_headroom_mm": 1100 },
                   "third_person": { "camera_probe_mm": 120, "boom_mm": 2500, "room_mm": [2400, 2400], "seated_headroom_mm": 1100 } }
  },
  "art": { "status": "block", "enabled": true, "mesh": null, "lods": [], "materials": [], "skeleton": null, "import": null, "provenance": null }
})JSON";

const char* const AssetItemContract = R"JSON({
  "schema": "pw.contract/1", "family": "item.handheld", "version": 1, "category": "item", "subcategories": ["pen"], "texel_class": "S",
  "dimensions_mm": { "min": [10, 10, 5], "max": [600, 600, 600] },
  "requires": { "solid": false, "interactions": 1, "seats": 0, "camera_clearance": false, "seated_clearance": false,
                "grip_socket": true, "target_radius_mm": 100 },
  "clearance": { "camera_probe_mm": 120, "min_camera_gap_mm": 150, "boom_mm": 2500, "room_mm": [2400, 2400], "min_seated_headroom_mm": 1100 },
  "grips": ["grip.pen", "grip.pinch"]
})JSON";

const char* const AssetPen = R"JSON({
  "schema": "pw.asset/1", "id": "item.pen.ballpoint_blue", "display_name": "Pen", "status": "active", "contract": "item.handheld@1",
  "milestone": "M1", "tags": ["small_item"],
  "dimensions_mm": [14, 150, 14], "pivot": "bottom_center", "front": "+y",
  "envelope_mm": { "inner": { "min": [-6, -70, 1], "max": [6, 70, 13] }, "outer": { "min": [-20, -90, 0], "max": [20, 90, 30] } },
  "block": { "class": "S", "cuboids": [ { "name": "barrel", "min_mm": [-7, -75, 0], "size_mm": [14, 150, 14], "color": "school_sky.2" } ] },
  "gameplay": {
    "surface": "mat.surface.plastic_smooth",
    "collision": [ { "shape": "box", "center_mm": [0, 0, 7], "half_extent_mm": [7, 75, 7], "channels": ["visibility"] } ],
    "nav": { "area": "none" },
    "interactions": [ { "id": "pick_up", "verb": "verb.pick_up", "stance_mm": [400, 0, 0], "facing_deg": 180, "grip": "grip.pen", "anchor_mm": [0, 0, 7], "priority": 1 } ],
    "grip": "grip.pen", "target_radius_mm": 100,
    "sockets": [ { "id": "grip", "location_mm": [0, -20, 7] } ]
  },
  "art": { "status": "block", "enabled": true, "mesh": null, "lods": [], "materials": [], "skeleton": null, "import": null, "provenance": null }
})JSON";

/** Replaces the one occurrence of `from` in `text`; the test fails if there is not exactly one. */
std::string Edit(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, ("test setup: text to replace is present: " + from).c_str());
    Check(text.find(from, at + 1) == std::string::npos, ("test setup: text to replace is unique: " + from).c_str());
    text.replace(at, from.size(), to);
    return text;
}

ContentFile DoorContractFile(const std::string& text = AssetDoorContract)
{
    return {"/Contracts/arch.door.json", text};
}

ContentFile DoorFile(const std::string& text = AssetDoor)
{
    return {"/Assets/arch/door/wooden_panel.json", text};
}

AssetLoadResult LoadDoor(const std::string& text)
{
    return LoadAssets({DoorContractFile(), DoorFile(text)});
}

AssetLoadResult LoadStool(const std::string& text)
{
    return LoadAssets({{"/Contracts/prop.seat.json", AssetSeatContract}, {"/Assets/prop/furniture/stool_wooden_low.json", text}});
}

AssetLoadResult LoadPen(const std::string& text)
{
    return LoadAssets({{"/Contracts/item.handheld.json", AssetItemContract}, {"/Assets/item/pen/ballpoint_blue.json", text}});
}

bool HasIssue(const AssetLoadResult& result, const std::string& text)
{
    for (const ContentIssue& issue : result.Issues)
    {
        if (issue.Message.find(text) != std::string::npos && !issue.File.empty())
        {
            return true;
        }
    }
    return false;
}

void Report(const AssetLoadResult& result)
{
    for (const ContentIssue& issue : result.Issues)
    {
        std::cerr << "  " << issue.File << ": " << issue.Id << ": " << issue.Message << '\n';
    }
}
} // namespace

int main()
{
    // Identity: the ID format of ASSET_PIPELINE.md 3.1.
    {
        for (const char* good : {"prop.furniture.bed_plank_single", "arch.door.wooden_panel", "item.cup.clay_bhar.small", "ui.icon.gate_rank_f",
                                 "prop.a.b2"})
        {
            Check(IsValidAssetId(good), "a well-formed ID is accepted");
        }
        for (const char* bad : {"", "prop", "prop.furniture", "Prop.furniture.bed", "prop.furniture.Bed", "prop.furniture.bed-plank",
                                "prop.furniture.bed plank", "prop.furniture..bed", "1prop.furniture.bed", "prop.furniture.bed.a.b",
                                "prop._x.bed", "prop.furniture.bed."})
        {
            Check(!IsValidAssetId(bad), "a malformed ID is refused");
        }
        Check(!IsValidAssetId("prop.furniture." + std::string(33, 'a')), "a segment over 32 characters is refused");
        Check(IsValidAssetId("prop.furniture." + std::string(32, 'a')), "a segment of exactly 32 characters is accepted");
    }

    // A valid door and contract load cleanly and are queryable.
    {
        const AssetLoadResult good = LoadDoor(AssetDoor);
        Report(good);
        Check(good.Ok(), "a valid door loads");
        Check(good.Db.AssetCount() == 1 && good.Db.ContractCount() == 1, "one asset and one contract are registered");
        Check(good.Db.CountInCategory("arch") == 1 && good.Db.CountInCategory("prop") == 0, "assets are counted by category");
        const AssetRecord* door = good.Db.Find("arch.door.wooden_panel");
        Check(door != nullptr && door->Category == "arch" && door->Subcategory == "door" && door->ContractKey == "arch.door@1", "the record knows its parts");
        const AssetContract* contract = good.Db.FindContract("arch.door@1");
        Check(contract != nullptr && contract->ExactHeightMm == 2100 && contract->Solid && contract->CameraProbeMm == 120, "the contract is parsed");
        Check(good.Db.Find("arch.door.nothing") == nullptr && good.Db.FindContract("arch.door@2") == nullptr, "unknown names find nothing");
        Check(LoadAssets({DoorFile(), DoorContractFile()}).Ok(), "file order does not matter");
        Check(LoadAssets({{"\\Contracts\\arch.door.json", AssetDoorContract}, {"\\Assets\\arch\\door\\wooden_panel.json", AssetDoor}}).Ok(),
              "backslash paths are accepted");
    }

    // The five bad cases the M1-07 check names.
    {
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "[30, 300, 1000]", "[30, 900, 1000]")), "anchor [30,900,1000] lies outside the outer envelope"),
              "an anchor outside the bounds fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, ",\n                   \"third_person\": { \"camera_probe_mm\": 120, \"boom_mm\": 2500, \"room_mm\": [2400, 2400] }", "")),
                       "missing required field \"third_person\""),
              "missing third-person clearance fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, ",\n                   \"third_person\": { \"camera_probe_mm\": 120, \"boom_mm\": 2500, \"room_mm\": [2400, 2400] }", "")),
                       "first_person") == false,
              "the declared first-person mode is not blamed");
        std::string noClearance = Edit(AssetDoor, "\"clearance\": { \"first_person\"", "\"clearance_unused\": { \"first_person\"");
        Check(HasIssue(LoadDoor(noClearance), "missing required field \"clearance\""), "missing clearance fails (and the stray field is unknown)");
        Check(HasIssue(LoadDoor(noClearance), "unknown field \"clearance_unused\""), "the misnamed field is refused");
        const std::string shortDoor = Edit(Edit(Edit(AssetDoor, "\"dimensions_mm\": [100, 1000, 2100]", "\"dimensions_mm\": [100, 1000, 2000]"),"\"size_mm\": [100, 1000, 2100]", "\"size_mm\": [100, 1000, 2000]"),
                                           "\"half_extent_mm\": [50, 500, 1050]", "\"half_extent_mm\": [50, 500, 1000]");
        Check(HasIssue(LoadDoor(shortDoor), "height 2000 mm must be exactly 2100 mm"), "a 2.00 m door fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "arch.door@1", "arch.window@1")), "unknown family"), "an unknown family fails");
        Check(HasIssue(LoadAssets({DoorContractFile(), DoorFile(), {"/x/Assets/arch/door/wooden_panel.json", AssetDoor}}), "duplicate ID"),
              "a duplicate ID fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"tags\"", "\"mood\": 1, \"tags\"")), "unknown field \"mood\""), "unknown fields fail");
    }

    // Shape: integers only, known fields, required fields.
    {
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"dimensions_mm\": [100, 1000, 2100]", "\"dimensions_mm\": [100, 1000.5, 2100]")), "three whole numbers of millimetres"),
              "a fractional dimension fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "[30, 300, 1000]", "[30, 3e2, 1000]")), "whole numbers"), "an exponent is not a whole number");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"facing_deg\": 180", "\"facing_deg\": 450")), "outside 0..359"), "angles are range-checked");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"milestone\": \"M1\",", "")), "missing required field \"milestone\""), "required fields are required");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"milestone\": \"M1\"", "\"milestone\": \"M11\"")), "milestone must be"), "milestones are M0..M10");
        Check(HasIssue(LoadDoor("{ \"id\": \"a.b.c\" }"), "string \"schema\""), "a file with no schema is refused");
        Check(HasIssue(LoadDoor(R"({"schema": "pw.asset/2"})"), "unknown schema"), "an unknown schema version is refused");
        Check(HasIssue(LoadDoor("{ not json"), "not valid JSON"), "malformed JSON is refused");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"pivot\": \"bottom_center\"", "\"pivot\": \"center\"")), "pivot must be"), "only the bottom-centre pivot exists");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"front\": \"+x\"", "\"front\": \"up\"")), "front must be"), "front is an axis");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"dimensions_mm\": [100, 1000, 2100]", "\"dimensions_mm\": [101, 1000, 2100]")), "must be even"), "odd X or Y breaks the centred pivot");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"dimensions_mm\": [100, 1000, 2100]", "\"dimensions_mm\": [100, 1000, 999999]")), "beyond 200 m"), "coordinates are bounded");
    }

    // Identity: the ID, the path, the status.
    {
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "arch.door.wooden_panel", "arch.door.Wooden")), "must be category.subcategory.name"), "an uppercase ID fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "arch.door.wooden_panel", "arch.door.block_panel")), "says how a thing is drawn"), "an ID naming the art fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "arch.door.wooden_panel", "arch.door.final")), "says how a thing is drawn"), "so does 'final'");
        Check(!HasIssue(LoadDoor(Edit(AssetDoor, "arch.door.wooden_panel", "arch.door.chai_stall")), "says how a thing is drawn"),
              "'ai' only counts as a whole word, so 'chai' is fine");
        Check(HasIssue(LoadAssets({DoorContractFile(), {"/Assets/arch/door/other.json", AssetDoor}}), "the file must be Assets/<category>/<subcategory>/<name>.json"),
              "the path must match the ID");
        Check(HasIssue(LoadAssets({DoorContractFile(), {"/Assets/arch/window/wooden_panel.json", AssetDoor}}), "the file must be Assets/"),
              "the folder must match the subcategory");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"status\": \"active\"", "\"status\": \"retired\"")), "needs \"replaced_by\""), "retirement names a replacement");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"status\": \"active\"", "\"status\": \"retired\", \"replaced_by\": \"arch.door.gone\"")), "which does not exist"),
              "the replacement must exist");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"status\": \"active\"", "\"status\": \"active\", \"replaced_by\": \"arch.door.gone\"")), "only a retired asset"),
              "an active asset has no replacement");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"home\"", "\"tone.sparkly\"")), "unknown tone tag"), "tone tags are the six reviewed ones");
        Check(LoadDoor(Edit(AssetDoor, "\"home\"", "\"tone.care\"")).Ok(), "a real tone tag is accepted");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"status\": \"block\"", "\"status\": \"real\"")), "a real-status asset needs"),
              "real art needs a mesh and provenance");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"mesh\": null", "\"mesh\": \"/Game/Art/x\"")), "a block-status asset must have null"),
              "block art has no mesh");
    }

    // Contracts and families.
    {
        Check(HasIssue(LoadAssets({DoorContractFile(Edit(AssetDoorContract, "\"texel_class\": \"A\"", "\"texel_class\": \"Z\"")), DoorFile()}), "texel_class must be"),
              "texel classes are A, P and S");
        Check(HasIssue(LoadAssets({DoorContractFile(), {"/Contracts/other.json", AssetDoorContract}}), "duplicate contract"), "a contract is declared once");
        Check(HasIssue(LoadAssets({{"/Contracts/wrong.json", AssetDoorContract}, DoorFile()}), "the file must be Contracts/<family>.json"),
              "a contract's path must match its family");
        Check(HasIssue(LoadAssets({DoorContractFile(Edit(AssetDoorContract, "\"category\": \"arch\"", "\"category\": \"prop\"")), DoorFile()}),
                       "must be the first part of the family"),
              "a contract's category is its family prefix");
        Check(HasIssue(LoadAssets({DoorContractFile(Edit(AssetDoorContract, "[50, 800, 2100]", "[50, 800, 2200]")), DoorFile()}), "min <= max"),
              "contract dimension ranges are ordered");
        Check(HasIssue(LoadAssets({DoorContractFile(Edit(AssetDoorContract, "\"solid\": true,", "\"solid\": true, \"mood\": 1,")), DoorFile()}), "unknown field"),
              "contracts refuse unknown fields too");
        Check(HasIssue(LoadAssets({DoorContractFile(), {"/Assets/prop/furniture/wooden_panel.json", Edit(AssetDoor, "arch.door.wooden_panel", "prop.furniture.wooden_panel")}}),
                       "does not match contract"),
              "the asset's category must match its contract");
        Check(HasIssue(LoadAssets({DoorContractFile(), {"/Assets/arch/wall/wooden_panel.json", Edit(AssetDoor, "arch.door.wooden_panel", "arch.wall.wooden_panel")}}),
                       "not one of the subcategories"),
              "the asset's subcategory must be in its contract");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"dimensions_mm\": [100, 1000, 2100]", "\"dimensions_mm\": [100, 700, 2100]")), "outside 800 mm..1200 mm"), "dimensions respect the contract range");
    }

    // A door must be 2,100 mm even under a contract with a loose height (ASSET_PIPELINE.md 1.2 reference dimensions).
    {
        const std::string looseContract = Edit(Edit(AssetDoorContract, "[50, 800, 2100]", "[50, 800, 1500]"), "\"exact_height_mm\": 2100,", "");
        const AssetLoadResult loose = LoadAssets({{"/Contracts/arch.door.json", looseContract}, DoorFile()});
        Check(loose.Ok(), "setup: a loose door contract accepts the 2,100 mm door");
        const std::string windowContract = Edit(Edit(looseContract, "arch.door", "arch.window"), "[\"door\"]", "[\"window\"]");
        Check(HasIssue(LoadAssets({{"/Contracts/arch.window.json", windowContract}, {"/Assets/arch/door/wooden_panel.json", Edit(AssetDoor, "arch.door@1", "arch.window@1")}}),
                       "a door must use an arch.door contract"),
              "an asset in the door subcategory must use a door contract");
    }

    // Block model.
    {
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"size_mm\": [100, 1000, 2100]", "\"size_mm\": [100, 1000, 2200]")), "outside the outer envelope"),
              "a cuboid outside the envelope fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"size_mm\": [100, 1000, 2100]", "\"size_mm\": [100, 1000, 2000]")), "the cuboids span"),
              "cuboids must span the declared dimensions");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"tint\": \"teak\"", "\"tint\": \"magenta\"")), "unknown palette ramp"), "tints must be palette ramps");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"tint\": \"teak\"", "\"tint\": \"gate_teal\"")), "reserved ramp"), "Gate ramps stay out of the ordinary world");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, ", \"tint\": \"teak\"", ", \"color\": \"teak.2\"")), "exactly one of"), "a cuboid has a tile or a colour, not both");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"tile\": \"tile.wood_panel\", \"tint\": \"teak\"", "\"color\": \"teak.9\"")), "step 0-3"),
              "colour steps are 0 to 3");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"class\": \"A\"", "\"class\": \"P\"")), "texel class P does not match"), "the texel class follows the contract");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"tile\": \"tile.wood_panel\"", "\"tile\": \"wood\"")), "must look like tile.name"), "tile IDs are tile.name");
        Check(LoadDoor(Edit(AssetDoor, "\"tint\": \"teak\"", "\"tint\": \"teak\", \"faces\": { \"-z\": \"hidden\" }")).Ok(), "hidden faces are accepted");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"tint\": \"teak\"", "\"tint\": \"teak\", \"faces\": { \"-q\": \"hidden\" }")), "faces entries look like"),
              "face names are checked");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "[-60, -510, 0], \"max\": [60, 510, 2110]", "[-60, -510, 0], \"max\": [60, 510, 2000]")), "must contain the dimensions box"),
              "the outer envelope must contain the dimensions box");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "[-30, -390, 10], \"max\": [30, 390, 1990]", "[-30, -390, 10], \"max\": [30, 390, 3000]")), "inner envelope"),
              "the inner envelope must be inside the box");
    }

    // Collision, interactions and reach.
    {
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"half_extent_mm\": [50, 500, 1050]", "\"half_extent_mm\": [50, 900, 1050]")), "collision box around"),
              "a collision box outside the envelope fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "[\"visibility\", \"camera\", \"pawn\"]", "[\"pawn\"]")), "\"visibility\" channel"), "something must be targetable");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "[\"visibility\", \"camera\", \"pawn\"]", "[\"visibility\", \"camera\"]")), "needs the \"pawn\" channel"),
              "a solid family needs a pawn box");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "[\"visibility\", \"camera\", \"pawn\"]", "[\"visibility\", \"smell\"]")), "channels are"), "channels are a closed set");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "[30, 300, 1000]", "[30, 300, 100]")), "reach band"), "a standing anchor must be within reach height");
        Check(LoadDoor(Edit(Edit(AssetDoor, "[30, 300, 1000]", "[30, 300, 100]"), "\"priority\": 1", "\"pose\": \"pose.crouch\", \"priority\": 1")).Ok(),
              "a crouch anchor may be near the floor");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"stance_mm\": [700, 0, 0]", "\"stance_mm\": [0, 0, 0]")), "not on clear floor"), "the stance must be clear floor");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"anchor_mm\": [30, 300, 1000], ", "")), "exactly one of"), "an interaction needs an anchor or a seat");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"anchor_mm\": [30, 300, 1000]", "\"anchor_mm\": [30, 300, 1000], \"seat\": \"x\"")), "exactly one of"),
              "an interaction cannot have both");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"verb.open\"", "\"open\"")), "verb.name"), "verbs are verb.name");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"priority\": 1", "\"priority\": 12")), "outside 1..9"), "priorities are 1 to 9");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"interactions\": [ { \"id\": \"open\", \"verb\": \"verb.open\", \"stance_mm\": [700, 0, 0], \"facing_deg\": 180, \"anchor_mm\": [30, 300, 1000], \"priority\": 1 } ],", "")),
                       "needs at least 1 interaction"),
              "the contract's interaction count is enforced");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"surface\": \"mat.surface.wood_teak\"", "\"surface\": \"wood\"")), "mat.surface."), "surfaces are mat.surface IDs");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"area\": \"passage\"", "\"area\": \"lava\"")), "area must be"), "nav areas are a closed set");
    }

    // Camera clearance in both modes (first and third person).
    {
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"gap_mm\": 150", "\"gap_mm\": 100")), "under the 150 mm"), "first-person gap below 150 mm fails");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"first_person\": { \"camera_probe_mm\": 120", "\"first_person\": { \"camera_probe_mm\": 60")), "fixes the probe at 120"),
              "the probe radius is the contract's");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"boom_mm\": 2500", "\"boom_mm\": 1500")), "fixes the boom at 2500"), "the boom length is the contract's");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"room_mm\": [2400, 2400]", "\"room_mm\": [2400, 1800]")), "room the boom must be cleared in"),
              "the boom must have been cleared in a 2.4 m room");
        Check(HasIssue(LoadDoor(Edit(AssetDoor, "\"third_person\": {", "\"third_person\": { \"extra\": 1,")), "unknown field \"extra\""), "clearance objects refuse unknown fields");
    }

    // Seats and seated clearance.
    {
        const AssetLoadResult good = LoadStool(AssetStool);
        Report(good);
        Check(good.Ok(), "a valid stool loads");
        Check(HasIssue(LoadStool(Edit(AssetStool, "\"pelvis_mm\": [0, 0, 300]", "\"pelvis_mm\": [0, 0, 900]")), "outside the outer envelope")
                  && HasIssue(LoadStool(Edit(AssetStool, "\"pelvis_mm\": [0, 0, 300]", "\"pelvis_mm\": [0, 0, 400]")), "does not suit pose.sit_stool_30"),
              "the pelvis must be inside the bounds and at the pose's seat height");
        Check(HasIssue(LoadStool(Edit(AssetStool, ", \"seated_headroom_mm\": 1100 },\n                   \"third_person\"", " },\n                   \"third_person\"")),
                       "missing required field \"seated_headroom_mm\""),
              "seated clearance is required in first person");
        Check(HasIssue(LoadStool(Edit(AssetStool, "\"room_mm\": [2400, 2400], \"seated_headroom_mm\": 1100", "\"room_mm\": [2400, 2400]")),
                       "missing required field \"seated_headroom_mm\""),
              "seated clearance is required in third person");
        Check(HasIssue(LoadStool(Edit(AssetStool, "\"room_mm\": [2400, 2400], \"seated_headroom_mm\": 1100", "\"room_mm\": [2400, 2400], \"seated_headroom_mm\": 900")),
                       "under the 1100 mm"),
              "seated headroom below the contract fails");
        std::string noClearance = Edit(AssetStool, "\"clearance\": { \"first_person\"", "\"clearance_unused\": { \"first_person\"");
        Check(HasIssue(LoadStool(noClearance), "camera and seated clearance must be declared for both first_person and third_person"),
              "a seat with no clearance at all fails");
        Check(HasIssue(LoadStool(Edit(AssetStool, "\"exit_mm\": [400, 0, 0] }", "\"exit_mm\": [0, 0, 0] }")), "outside its outer envelope"),
              "the exit must be beside the asset");
        Check(HasIssue(LoadStool(Edit(AssetStool, "\"seat\": \"seat\"", "\"seat\": \"chair\"")), "which this asset does not define"), "an interaction's seat must exist");
        Check(HasIssue(LoadStool(Edit(AssetStool, "\"pose\": \"pose.sit_stool_30\"", "\"pose\": \"pose.sit_edge_45\"")), "not allowed by contract"),
              "the seat pose must be one the contract lists");
        Check(HasIssue(LoadStool(Edit(AssetStool, "\"pose\": \"pose.sit_stool_30\"", "\"pose\": \"pose.levitate\"")), "is not a seat pose"), "poses are known");
        Check(HasIssue(LoadStool(Edit(AssetStool, "\"seats\": [ { \"id\": \"seat\", \"pose\": \"pose.sit_stool_30\", \"pelvis_mm\": [0, 0, 300], \"facing_deg\": 0, \"exit_mm\": [400, 0, 0] } ],", "")),
                       "needs at least 1 seat"),
              "the contract's seat count is enforced");
    }

    // Held items: grips, sockets and target radius.
    {
        const AssetLoadResult good = LoadPen(AssetPen);
        Report(good);
        Check(good.Ok(), "a valid pen loads");
        Check(HasIssue(LoadPen(Edit(AssetPen, "\"sockets\": [ { \"id\": \"grip\", \"location_mm\": [0, -20, 7] } ]", "\"sockets\": []")), "needs a socket with id \"grip\""),
              "a held item needs a grip socket");
        Check(HasIssue(LoadPen(Edit(AssetPen, "\"target_radius_mm\": 100", "\"target_radius_mm\": 50")), "under the 100 mm minimum"), "small items stay easy to target");
        Check(HasIssue(LoadPen(Edit(AssetPen, "\"grip\": \"grip.pen\", \"target_radius_mm\"", "\"grip\": \"grip.cup\", \"target_radius_mm\"")), "not allowed by contract"),
              "the grip must be one the contract lists");
        Check(HasIssue(LoadPen(Edit(AssetPen, "\"grip\": \"grip.pen\", \"target_radius_mm\"", "\"grip\": \"grip.nope\", \"target_radius_mm\"")), "unknown grip"), "grips are known");
        Check(HasIssue(LoadPen(Edit(AssetPen, "\"location_mm\": [0, -20, 7]", "\"location_mm\": [0, -200, 7]")), "outside the outer envelope"),
              "a socket must lie inside the bounds");
        Check(HasIssue(LoadPen(Edit(AssetPen, "\"mat.surface.plastic_smooth\"", "\"mat.surface.plastic_smooth\", \"surface\": \"mat.surface.b\"")), "not valid JSON"),
              "a duplicate key is a parse error");
    }

    // Reserved Gate ramps are allowed in the Gate kit only.
    {
        const std::string gateContract = Edit(Edit(AssetDoorContract, "arch.door", "arch.gate"), "[\"door\"]", "[\"gatekit\"]");
        const std::string looseGate = Edit(Edit(gateContract, "[50, 800, 2100]", "[50, 800, 1500]"), "\"exact_height_mm\": 2100,", "");
        const std::string rift = Edit(Edit(Edit(AssetDoor, "arch.door.wooden_panel", "arch.gatekit.rift_frame"), "arch.door@1", "arch.gate@1"), "\"tint\": \"teak\"", "\"tint\": \"gate_violet\"");
        Check(LoadAssets({{"/Contracts/arch.gate.json", looseGate}, {"/Assets/arch/gatekit/rift_frame.json", rift}}).Ok(), "the Gate kit may use the reserved ramps");
    }

    std::cout << "AssetRegistryTests: " << Checks << " checks passed\n";
    return 0;
}
