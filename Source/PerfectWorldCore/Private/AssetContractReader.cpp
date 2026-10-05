#include "AssetInternal.h"

// Contract files (schema "pw.contract/1"): what one asset family requires of its assets.
namespace PerfectWorld::AssetDetail
{
std::vector<std::string> AssetSplit(std::string_view text, char separator)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true)
    {
        const std::size_t end = text.find(separator, start);
        parts.emplace_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
        if (end == std::string_view::npos)
        {
            return parts;
        }
        start = end + 1;
    }
}

bool AssetSegmentOk(const std::string& segment, bool first)
{
    if (segment.empty() || segment.size() > 32)
    {
        return false;
    }
    for (std::size_t i = 0; i < segment.size(); ++i)
    {
        const char c = segment[i];
        const bool lower = c >= 'a' && c <= 'z';
        const bool digit = c >= '0' && c <= '9';
        if (i == 0 ? !(lower || (digit && !first)) : !(lower || digit || c == '_'))
        {
            return false;
        }
    }
    return true;
}

namespace
{
constexpr std::int64_t AssetBig = 1000000;

void AssetReadPoseList(AssetReader& root, std::string_view name, std::vector<std::string>& out, bool poses)
{
    const JsonValue* list = root.Array(name, false);
    if (list == nullptr)
    {
        return;
    }
    for (const JsonValue& item : list->Array)
    {
        const bool ok = item.IsString() && (poses ? AssetIsKnownPose(item.String) : AssetIsKnownGrip(item.String));
        if (!ok)
        {
            root.Report("field \"" + std::string(name) + "\" has an unknown " + (poses ? "pose" : "grip") + " (" +
                        (item.IsString() ? item.String : "not a string") + ")");
            continue;
        }
        out.push_back(item.String);
    }
}
} // namespace

std::optional<AssetContract> AssetParseContract(const JsonValue& json, const std::string& file, std::vector<ContentIssue>& issues)
{
    AssetSink sink(issues, file, "");
    AssetReader root(json, "", sink);
    AssetContract c;
    c.File = file;
    root.Member("schema", true);
    const std::optional<std::string> family = root.String("family", true);
    const std::optional<std::int64_t> version = root.Int("version", true, 1, 99);
    if (!family || !version)
    {
        root.Finish();
        return std::nullopt;
    }
    c.Family = *family;
    c.Version = static_cast<int>(*version);
    c.Key = c.Family + "@" + std::to_string(c.Version);
    sink.Id = c.Key;

    const std::vector<std::string> familyParts = AssetSplit(c.Family, '.');
    const bool familyOk = familyParts.size() == 2 && AssetSegmentOk(familyParts[0], true) && AssetSegmentOk(familyParts[1], false);
    if (!familyOk)
    {
        root.Report("family \"" + c.Family + "\" must look like category.name, lowercase");
    }
    c.Category = root.String("category", true).value_or("");
    if (familyOk && c.Category != familyParts[0])
    {
        root.Report("category \"" + c.Category + "\" must be the first part of the family \"" + c.Family + "\"");
    }
    if (const JsonValue* subs = root.Array("subcategories", true))
    {
        for (const JsonValue& item : subs->Array)
        {
            if (item.IsString() && AssetSegmentOk(item.String, false))
                c.Subcategories.push_back(item.String);
            else
                root.Report("field \"subcategories\" must contain lowercase names");
        }
        if (subs->Array.empty())
            root.Report("field \"subcategories\" must name at least one subcategory");
    }
    c.TexelClass = root.String("texel_class", true).value_or("");
    if (c.TexelClass != "A" && c.TexelClass != "P" && c.TexelClass != "S")
    {
        root.Report("texel_class must be A, P or S (ASSET_PIPELINE.md 1.2)");
    }

    if (std::optional<AssetReader> dims = root.Child("dimensions_mm", true))
    {
        const std::optional<AssetVec> lo = dims->Vec3("min", true);
        const std::optional<AssetVec> hi = dims->Vec3("max", true);
        dims->Finish();
        if (lo && hi)
        {
            c.MinDimensionsMm = {lo->X, lo->Y, lo->Z};
            c.MaxDimensionsMm = {hi->X, hi->Y, hi->Z};
            for (std::size_t i = 0; i < 3; ++i)
            {
                if (c.MinDimensionsMm[i] < 1 || c.MinDimensionsMm[i] > c.MaxDimensionsMm[i])
                    root.Report("dimensions_mm needs 1 <= min <= max on every axis");
            }
        }
    }
    if (root.Member("exact_height_mm", false) != nullptr)
    {
        c.ExactHeightMm = root.Int("exact_height_mm", false, 1, AssetCoordLimitMm);
        if (c.ExactHeightMm && (*c.ExactHeightMm < c.MinDimensionsMm[2] || *c.ExactHeightMm > c.MaxDimensionsMm[2]))
            root.Report("exact_height_mm lies outside the contract's own height range");
    }

    if (std::optional<AssetReader> req = root.Child("requires", true))
    {
        c.Solid = req->Bool("solid", true).value_or(false);
        c.MinInteractions = req->Int("interactions", true, 0, 16).value_or(0);
        c.MinSeats = req->Int("seats", true, 0, 16).value_or(0);
        c.RequiresCameraClearance = req->Bool("camera_clearance", true).value_or(false);
        c.RequiresSeatedClearance = req->Bool("seated_clearance", true).value_or(false);
        c.RequiresStandingAnchor = req->Bool("standing_anchor", false).value_or(false);
        c.RequiresGripSocket = req->Bool("grip_socket", false).value_or(false);
        c.MinTargetRadiusMm = req->Int("target_radius_mm", false, 0, AssetBig).value_or(0);
        c.MinKneeClearanceMm = req->Pair("knee_clearance_mm", false, 1, AssetBig);
        c.AnchorHeightMm = req->Pair("anchor_height_mm", false, 0, AssetBig);
        if (c.AnchorHeightMm && (*c.AnchorHeightMm)[0] > (*c.AnchorHeightMm)[1])
            req->Report("anchor_height_mm must be [low, high]");
        req->Finish();
    }
    if (std::optional<AssetReader> clear = root.Child("clearance", true))
    {
        c.CameraProbeMm = clear->Int("camera_probe_mm", true, 1, AssetBig).value_or(0);
        c.MinCameraGapMm = clear->Int("min_camera_gap_mm", true, 1, AssetBig).value_or(0);
        c.BoomMm = clear->Int("boom_mm", true, 1, AssetBig).value_or(0);
        c.MinRoomMm = clear->Pair("room_mm", true, 1, AssetBig).value_or(std::array<std::int64_t, 2>{});
        c.MinSeatedHeadroomMm = clear->Int("min_seated_headroom_mm", true, 1, AssetBig).value_or(0);
        clear->Finish();
    }
    AssetReadPoseList(root, "seat_poses", c.SeatPoses, true);
    AssetReadPoseList(root, "grips", c.Grips, false);
    if (c.MinSeats > 0 && c.SeatPoses.empty())
        root.Report("a contract that requires seats must list its seat_poses");
    if (c.RequiresGripSocket && c.Grips.empty())
        root.Report("a contract that requires a grip socket must list its grips");
    root.Finish();
    return c;
}
} // namespace PerfectWorld::AssetDetail
