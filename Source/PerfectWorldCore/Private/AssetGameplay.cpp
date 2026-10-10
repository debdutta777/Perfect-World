#include "AssetInternal.h"

#include <algorithm>

namespace PerfectWorld::AssetDetail
{
namespace
{
constexpr std::int64_t AssetBig = 1000000;

/** What the gameplay checks learn as they go and the later ones need. */
struct AssetGameplayState
{
    std::vector<AssetBox> PawnBoxes;
    std::vector<std::string> SeatIds;
    std::size_t SeatCount = 0;
    std::size_t InteractionCount = 0;
    bool HasStandingInteraction = false;
    bool HasGripSocket = false;
};

std::string AssetPoint(const AssetVec& p)
{
    return "[" + std::to_string(p.X) + "," + std::to_string(p.Y) + "," + std::to_string(p.Z) + "]";
}

bool AssetInsideOuter(const AssetBodyContext& context, const AssetVec& p)
{
    return !context.HaveOuter || AssetContains(context.Outer, p);
}

/**
 * Runs one object of an array through a checker, or reports that it is not an object.
 * (Not named `check`: inside Unreal that is the engine's assertion macro.)
 */
template <typename Checker>
void AssetEachObject(AssetReader& parent, const JsonValue* array, std::string_view name, Checker checkObject)
{
    if (array == nullptr)
    {
        return;
    }
    std::size_t index = 0;
    for (const JsonValue& item : array->Array)
    {
        const std::string where = parent.Where() + std::string(name) + "[" + std::to_string(index++) + "]: ";
        if (!item.IsObject())
        {
            parent.Report("\"" + std::string(name) + "\" must contain only objects");
            continue;
        }
        AssetReader reader(item, where, parent.SinkRef());
        checkObject(reader);
        reader.Finish();
    }
}

void AssetCheckCollision(AssetReader& gameplay, const AssetBodyContext& context, AssetGameplayState& state)
{
    const JsonValue* list = gameplay.Array("collision", true);
    bool hasVisible = false;
    AssetEachObject(gameplay, list, "collision", [&](AssetReader& box) {
        const std::string shape = box.String("shape", true).value_or("box");
        if (shape != "box")
        {
            box.Report("shape must be \"box\"");
        }
        const std::optional<AssetVec> center = box.Vec3("center_mm", true);
        const std::optional<AssetVec> half = box.Vec3("half_extent_mm", true);
        bool pawn = false;
        bool unique = true;
        std::vector<std::string> seen;
        if (const JsonValue* channels = box.Array("channels", true))
        {
            for (const JsonValue& channel : channels->Array)
            {
                const bool known = channel.IsString() && (channel.String == "visibility" || channel.String == "camera" || channel.String == "pawn");
                if (!known)
                {
                    box.Report("channels are \"visibility\", \"camera\" and \"pawn\"");
                    continue;
                }
                unique = unique && !AssetContainsText(seen, channel.String);
                seen.push_back(channel.String);
                pawn = pawn || channel.String == "pawn";
                hasVisible = hasVisible || channel.String == "visibility";
            }
            if (!unique || channels->Array.empty())
            {
                box.Report("channels must be a non-empty list without repeats");
            }
        }
        if (!center || !half)
        {
            return;
        }
        if (half->X < 1 || half->Y < 1 || half->Z < 1)
        {
            box.Report("half_extent_mm must be positive on every axis");
            return;
        }
        const AssetBox extent{{center->X - half->X, center->Y - half->Y, center->Z - half->Z},
                              {center->X + half->X, center->Y + half->Y, center->Z + half->Z}};
        if (context.HaveOuter && !AssetBoxInside(extent, context.Outer))
        {
            box.Report("collision box around " + AssetPoint(*center) + " lies outside the outer envelope");
        }
        if (pawn)
        {
            state.PawnBoxes.push_back(extent);
        }
    });
    if (list != nullptr && list->Array.empty())
    {
        gameplay.Report("an asset needs at least one collision box (selection and reach use it)");
    }
    if (list != nullptr && !list->Array.empty() && !hasVisible)
    {
        gameplay.Report("at least one collision box must use the \"visibility\" channel, or the asset cannot be targeted");
    }
    if (list != nullptr && context.Contract->Solid && state.PawnBoxes.empty())
    {
        gameplay.Report("contract " + context.Contract->Key + " is solid, so a collision box needs the \"pawn\" channel");
    }
}

void AssetCheckSeats(AssetReader& gameplay, const AssetBodyContext& context, AssetGameplayState& state)
{
    const AssetContract& contract = *context.Contract;
    const JsonValue* list = gameplay.Array("seats", false);
    if (list != nullptr)
    {
        state.SeatCount = list->Array.size();
    }
    AssetEachObject(gameplay, list, "seats", [&](AssetReader& seat) {
        const std::optional<std::string> id = seat.String("id", true);
        if (id && (!AssetIsLowerId(*id) || AssetContainsText(state.SeatIds, *id)))
        {
            seat.Report("seat id \"" + *id + "\" must be unique lowercase letters, digits and '_'");
        }
        if (id)
        {
            state.SeatIds.push_back(*id);
        }
        const std::string pose = seat.String("pose", true).value_or("");
        const auto range = AssetSeatPelvisRange(pose);
        if (!pose.empty() && !range)
        {
            seat.Report("pose \"" + pose + "\" is not a seat pose");
        }
        else if (!pose.empty() && !AssetContainsText(contract.SeatPoses, pose))
        {
            seat.Report("pose \"" + pose + "\" is not allowed by contract " + contract.Key);
        }
        const std::optional<AssetVec> pelvis = seat.Vec3("pelvis_mm", true);
        if (pelvis && !AssetInsideOuter(context, *pelvis))
        {
            seat.Report("pelvis " + AssetPoint(*pelvis) + " lies outside the outer envelope");
        }
        if (pelvis && range && (pelvis->Z < (*range)[0] || pelvis->Z > (*range)[1]))
        {
            seat.Report("pelvis height " + std::to_string(pelvis->Z) + " mm does not suit " + pose + " (" + std::to_string((*range)[0]) + ".." +
                        std::to_string((*range)[1]) + " mm)");
        }
        if (const std::optional<AssetVec> head = seat.Vec3("head_mm", false); head && !AssetInsideOuter(context, *head))
        {
            seat.Report("head position " + AssetPoint(*head) + " lies outside the outer envelope");
        }
        seat.Int("facing_deg", true, 0, 359);
        const std::optional<AssetVec> exit = seat.Vec3("exit_mm", true);
        if (exit && context.HaveOuter && exit->X >= context.Outer.Min.X && exit->X <= context.Outer.Max.X && exit->Y >= context.Outer.Min.Y &&
            exit->Y <= context.Outer.Max.Y)
        {
            seat.Report("the exit " + AssetPoint(*exit) + " must be on the floor beside the asset, outside its outer envelope");
        }
    });
}

void AssetCheckInteractions(AssetReader& gameplay, const AssetBodyContext& context, AssetGameplayState& state)
{
    const AssetContract& contract = *context.Contract;
    const JsonValue* list = gameplay.Array("interactions", false);
    if (list != nullptr)
    {
        state.InteractionCount = list->Array.size();
    }
    std::vector<std::string> ids;
    AssetEachObject(gameplay, list, "interactions", [&](AssetReader& action) {
        const std::optional<std::string> id = action.String("id", true);
        if (id && (!AssetIsLowerId(*id) || AssetContainsText(ids, *id)))
        {
            action.Report("interaction id \"" + *id + "\" must be unique lowercase letters, digits and '_'");
        }
        if (id)
        {
            ids.push_back(*id);
        }
        const std::optional<std::string> verb = action.String("verb", true);
        if (verb && !(verb->rfind("verb.", 0) == 0 && AssetIsLowerId(verb->substr(5))))
        {
            action.Report("verb \"" + *verb + "\" must look like verb.name");
        }
        action.Int("priority", true, 1, 9);
        action.Int("facing_deg", true, 0, 359);
        const std::optional<AssetVec> stance = action.Vec3("stance_mm", true);
        if (stance)
        {
            for (const AssetBox& pawn : state.PawnBoxes)
            {
                if (stance->X > pawn.Min.X && stance->X < pawn.Max.X && stance->Y > pawn.Min.Y && stance->Y < pawn.Max.Y)
                {
                    action.Report("stance " + AssetPoint(*stance) + " stands inside the asset's own pawn collision, not on clear floor");
                    break;
                }
            }
        }
        const std::optional<std::string> seat = action.String("seat", false);
        const std::optional<AssetVec> anchor = action.Vec3("anchor_mm", false);
        const std::string pose = action.String("pose", false).value_or("");
        if (!pose.empty() && !AssetIsKnownPose(pose))
        {
            action.Report("unknown pose \"" + pose + "\"");
        }
        if (const std::optional<std::string> grip = action.String("grip", false))
        {
            if (!AssetIsKnownGrip(*grip))
                action.Report("unknown grip \"" + *grip + "\"");
            else if (!contract.Grips.empty() && !AssetContainsText(contract.Grips, *grip))
                action.Report("grip \"" + *grip + "\" is not allowed by contract " + contract.Key);
        }
        if (seat.has_value() == anchor.has_value())
        {
            action.Report("an interaction needs exactly one of \"seat\" (a seat id) or \"anchor_mm\" (a point to reach)");
        }
        if (seat)
        {
            return; // seat references are resolved once every seat is known
        }
        state.HasStandingInteraction = true;
        if (anchor && !AssetInsideOuter(context, *anchor))
        {
            action.Report("anchor " + AssetPoint(*anchor) + " lies outside the outer envelope");
        }
        const bool low = pose == "pose.crouch";
        if (anchor && contract.AnchorHeightMm && !low && (anchor->Z < (*contract.AnchorHeightMm)[0] || anchor->Z > (*contract.AnchorHeightMm)[1]))
        {
            action.Report("a standing anchor at height " + std::to_string(anchor->Z) + " mm is outside the reach band " +
                          std::to_string((*contract.AnchorHeightMm)[0]) + ".." + std::to_string((*contract.AnchorHeightMm)[1]) +
                          " mm of contract " + contract.Key + " (use pose.crouch for floor anchors)");
        }
    });
    // Seat references, now that every seat id is known.
    if (list != nullptr)
    {
        for (const JsonValue& item : list->Array)
        {
            const JsonValue* seat = item.IsObject() ? item.Find("seat") : nullptr;
            if (seat != nullptr && seat->IsString() && !AssetContainsText(state.SeatIds, seat->String))
            {
                gameplay.Report("interaction names seat \"" + seat->String + "\", which this asset does not define");
            }
        }
    }
}

void AssetCheckSockets(AssetReader& gameplay, const AssetBodyContext& context, AssetGameplayState& state)
{
    std::vector<std::string> ids;
    AssetEachObject(gameplay, gameplay.Array("sockets", false), "sockets", [&](AssetReader& socket) {
        const std::optional<std::string> id = socket.String("id", true);
        if (id && (!AssetIsLowerId(*id) || AssetContainsText(ids, *id)))
        {
            socket.Report("socket id \"" + *id + "\" must be unique lowercase letters, digits and '_'");
        }
        if (id)
        {
            ids.push_back(*id);
            state.HasGripSocket = state.HasGripSocket || *id == "grip";
        }
        const std::optional<AssetVec> at = socket.Vec3("location_mm", true);
        const std::optional<AssetVec> half = socket.Vec3("half_extent_mm", false);
        if (at && !AssetInsideOuter(context, *at))
        {
            socket.Report("socket location " + AssetPoint(*at) + " lies outside the outer envelope");
        }
        if (at && half)
        {
            const AssetBox extent{{at->X - half->X, at->Y - half->Y, at->Z - half->Z}, {at->X + half->X, at->Y + half->Y, at->Z + half->Z}};
            if (half->X < 1 || half->Y < 1 || half->Z < 1)
                socket.Report("half_extent_mm must be positive on every axis");
            else if (context.HaveOuter && !AssetBoxInside(extent, context.Outer))
                socket.Report("the socket volume around " + AssetPoint(*at) + " reaches outside the outer envelope");
        }
    });
}

/** One camera mode's clearance object. `third` selects the boom fields. */
void AssetCheckClearanceMode(AssetReader& clearance, std::string_view name, bool third, bool seated, const AssetContract& contract)
{
    std::optional<AssetReader> mode = clearance.Child(name, true);
    if (!mode)
    {
        return;
    }
    const std::optional<std::int64_t> probe = mode->Int("camera_probe_mm", true, 1, AssetBig);
    if (probe && *probe != contract.CameraProbeMm)
    {
        mode->Report("camera_probe_mm is " + std::to_string(*probe) + ", but contract " + contract.Key + " fixes the probe at " +
                     std::to_string(contract.CameraProbeMm) + " mm");
    }
    if (third)
    {
        const std::optional<std::int64_t> boom = mode->Int("boom_mm", true, 1, AssetBig);
        if (boom && *boom != contract.BoomMm)
            mode->Report("boom_mm is " + std::to_string(*boom) + ", but contract " + contract.Key + " fixes the boom at " +
                         std::to_string(contract.BoomMm) + " mm");
        const auto room = mode->Pair("room_mm", true, 1, AssetBig);
        if (room && ((*room)[0] < contract.MinRoomMm[0] || (*room)[1] < contract.MinRoomMm[1]))
            mode->Report("room_mm is smaller than the " + std::to_string(contract.MinRoomMm[0]) + " x " + std::to_string(contract.MinRoomMm[1]) +
                         " mm room the boom must be cleared in");
    }
    else
    {
        const std::optional<std::int64_t> gap = mode->Int("gap_mm", true, 1, AssetBig);
        if (gap && *gap < contract.MinCameraGapMm)
            mode->Report("gap_mm is " + std::to_string(*gap) + ", under the " + std::to_string(contract.MinCameraGapMm) + " mm the contract requires");
    }
    if (const std::optional<std::int64_t> headroom = mode->Int("seated_headroom_mm", seated, 1, AssetBig);
        headroom && *headroom < contract.MinSeatedHeadroomMm)
    {
        mode->Report("seated_headroom_mm is " + std::to_string(*headroom) + ", under the " + std::to_string(contract.MinSeatedHeadroomMm) +
                     " mm the contract requires");
    }
    mode->Finish();
}

void AssetCheckClearance(AssetReader& gameplay, const AssetBodyContext& context, const AssetGameplayState& state)
{
    const AssetContract& contract = *context.Contract;
    const bool seated = state.SeatCount > 0 || contract.RequiresSeatedClearance;
    const bool needed = seated || contract.RequiresCameraClearance;
    std::optional<AssetReader> clearance = gameplay.Child("clearance", false);
    if (!clearance)
    {
        if (needed && gameplay.Member("clearance", false) == nullptr)
        {
            gameplay.Report("missing required field \"clearance\": camera " + std::string(seated ? "and seated " : "") +
                            "clearance must be declared for both first_person and third_person");
        }
        return;
    }
    AssetCheckClearanceMode(*clearance, "first_person", false, seated, contract);
    AssetCheckClearanceMode(*clearance, "third_person", true, seated, contract);
    clearance->Finish();
}
} // namespace

void AssetCheckGameplay(AssetReader& root, const AssetBodyContext& context)
{
    const AssetContract& contract = *context.Contract;
    std::optional<AssetReader> gameplay = root.Child("gameplay", true);
    if (!gameplay)
    {
        return;
    }
    AssetGameplayState state;
    if (const std::optional<std::string> surface = gameplay->String("surface", true);
        surface && !(surface->rfind("mat.surface.", 0) == 0 && IsValidAssetId(*surface)))
    {
        gameplay->Report("surface \"" + *surface + "\" must be a mat.surface.<name> ID");
    }
    AssetCheckCollision(*gameplay, context, state);
    if (std::optional<AssetReader> nav = gameplay->Child("nav", true))
    {
        const std::string area = nav->String("area", true).value_or("obstacle");
        if (area != "obstacle" && area != "walkable" && area != "passage" && area != "none")
            nav->Report("area must be obstacle, walkable, passage or none");
        nav->Finish();
    }
    AssetCheckSeats(*gameplay, context, state);
    AssetCheckInteractions(*gameplay, context, state);
    AssetCheckSockets(*gameplay, context, state);

    // Held items: one grip type and a grip socket (ASSET_PIPELINE.md 2.4).
    const std::optional<std::string> grip = gameplay->String("grip", contract.RequiresGripSocket);
    if (grip && !AssetIsKnownGrip(*grip))
        gameplay->Report("unknown grip \"" + *grip + "\"");
    else if (grip && !contract.Grips.empty() && !AssetContainsText(contract.Grips, *grip))
        gameplay->Report("grip \"" + *grip + "\" is not allowed by contract " + contract.Key);
    if (contract.RequiresGripSocket && !state.HasGripSocket)
        gameplay->Report("contract " + contract.Key + " needs a socket with id \"grip\"");

    const std::optional<std::int64_t> radius = gameplay->Int("target_radius_mm", contract.MinTargetRadiusMm > 0, 1, AssetBig);
    if (radius && *radius < contract.MinTargetRadiusMm)
        gameplay->Report("target_radius_mm is " + std::to_string(*radius) + ", under the " + std::to_string(contract.MinTargetRadiusMm) +
                         " mm minimum (small items must be easy to target)");
    const auto knee = gameplay->Pair("knee_clearance_mm", contract.MinKneeClearanceMm.has_value(), 1, AssetBig);
    if (knee && contract.MinKneeClearanceMm && ((*knee)[0] < (*contract.MinKneeClearanceMm)[0] || (*knee)[1] < (*contract.MinKneeClearanceMm)[1]))
        gameplay->Report("knee_clearance_mm (height, depth) is under the " + std::to_string((*contract.MinKneeClearanceMm)[0]) + " x " +
                         std::to_string((*contract.MinKneeClearanceMm)[1]) + " mm the contract requires");

    if (static_cast<std::int64_t>(state.InteractionCount) < contract.MinInteractions)
        gameplay->Report("contract " + contract.Key + " needs at least " + std::to_string(contract.MinInteractions) + " interaction(s)");
    if (static_cast<std::int64_t>(state.SeatCount) < contract.MinSeats)
        gameplay->Report("contract " + contract.Key + " needs at least " + std::to_string(contract.MinSeats) + " seat(s)");
    if (contract.RequiresStandingAnchor && !state.HasStandingInteraction)
        gameplay->Report("contract " + contract.Key + " needs at least one standing interaction with an anchor on clear floor");
    AssetCheckClearance(*gameplay, context, state);
    gameplay->Finish();
}
} // namespace PerfectWorld::AssetDetail
