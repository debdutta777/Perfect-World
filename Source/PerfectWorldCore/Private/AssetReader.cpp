#include "AssetInternal.h"

#include <algorithm>

namespace PerfectWorld::AssetDetail
{
bool AssetContains(const AssetBox& box, const AssetVec& p)
{
    return p.X >= box.Min.X && p.X <= box.Max.X && p.Y >= box.Min.Y && p.Y <= box.Max.Y && p.Z >= box.Min.Z && p.Z <= box.Max.Z;
}

bool AssetBoxInside(const AssetBox& inner, const AssetBox& outer)
{
    return AssetContains(outer, inner.Min) && AssetContains(outer, inner.Max);
}

bool AssetContainsText(const std::vector<std::string>& list, std::string_view text)
{
    return std::find(list.begin(), list.end(), text) != list.end();
}

bool AssetIsLowerId(std::string_view text)
{
    return !text.empty() && text.front() >= 'a' && text.front() <= 'z' &&
           std::all_of(text.begin(), text.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

// ---------------------------------------------------------------------------
// Vocabulary. These are the names ASSET_PIPELINE.md freezes; the tile list
// joins them when the style kit (M1-08) exists.
// ---------------------------------------------------------------------------

bool AssetIsKnownRamp(std::string_view name)
{
    static const char* const Ramps[] = {"ink",         "paper",       "plaster_cream", "plaster_ochre", "plaster_mint", "plaster_rose",
                                        "chalk_blue",  "terracotta",  "red_oxide",     "jhilmil_green", "monsoon_leaf", "moss",
                                        "school_navy", "school_sky",  "concrete",      "asphalt",       "steel",        "teak",
                                        "jute",        "taxi_yellow", "signal_red",    "river",         "marigold",     "dusk_indigo",
                                        "gate_teal",   "gate_violet"};
    return std::any_of(std::begin(Ramps), std::end(Ramps), [&](const char* r) { return name == r; });
}

bool AssetIsReservedRamp(std::string_view name)
{
    return name == "gate_teal" || name == "gate_violet";
}

bool AssetIsKnownPose(std::string_view name)
{
    static const char* const Poses[] = {"pose.stand",       "pose.crouch",     "pose.reach",       "pose.write",   "pose.hold_phone",
                                        "pose.sit_chair_45", "pose.sit_bench_42", "pose.sit_stool_30", "pose.sit_edge_45", "pose.sit_floor",
                                        "pose.lie_back"};
    return std::any_of(std::begin(Poses), std::end(Poses), [&](const char* p) { return name == p; });
}

bool AssetIsKnownGrip(std::string_view name)
{
    static const char* const Grips[] = {"grip.power", "grip.pinch", "grip.pen",         "grip.cup",       "grip.phone",     "grip.hook",
                                        "grip.flat_support", "grip.two_hand_carry", "grip.tool_twist", "grip.eat_hand", "grip.help_hand"};
    return std::any_of(std::begin(Grips), std::end(Grips), [&](const char* g) { return name == g; });
}

std::optional<std::array<std::int64_t, 2>> AssetSeatPelvisRange(std::string_view pose)
{
    // ASSET_PIPELINE.md 2.4 seat heights (chair 45, bench 42, stool 30, bed edge 45, floor 0), with 50 mm of give.
    struct Entry
    {
        const char* Pose;
        std::int64_t Lo;
        std::int64_t Hi;
    };
    static const Entry Table[] = {{"pose.sit_chair_45", 400, 500}, {"pose.sit_bench_42", 370, 470}, {"pose.sit_stool_30", 250, 350},
                                  {"pose.sit_edge_45", 400, 500},  {"pose.sit_floor", 0, 100},     {"pose.lie_back", 0, 700}};
    for (const Entry& entry : Table)
    {
        if (pose == entry.Pose)
        {
            return std::array<std::int64_t, 2>{entry.Lo, entry.Hi};
        }
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// AssetReader
// ---------------------------------------------------------------------------

const JsonValue* AssetReader::Member(std::string_view name, bool required)
{
    Known.emplace_back(name);
    const JsonValue* value = Object->Find(name);
    if (value == nullptr && required)
    {
        Report("missing required field \"" + std::string(name) + "\"");
    }
    return value;
}

std::optional<std::int64_t> AssetReader::Int(std::string_view name, bool required, std::int64_t lo, std::int64_t hi)
{
    const JsonValue* value = Member(name, required);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    if (!value->Integer)
    {
        Report("field \"" + std::string(name) + "\" must be a whole number (integer millimetres, degrees or a count)");
        return std::nullopt;
    }
    if (*value->Integer < lo || *value->Integer > hi)
    {
        Report("field \"" + std::string(name) + "\" is " + std::to_string(*value->Integer) + ", outside " + std::to_string(lo) + ".." +
               std::to_string(hi));
        return std::nullopt;
    }
    return *value->Integer;
}

std::optional<std::string> AssetReader::String(std::string_view name, bool required)
{
    const JsonValue* value = Member(name, required);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    if (!value->IsString() || value->String.empty())
    {
        Report("field \"" + std::string(name) + "\" must be a non-empty string");
        return std::nullopt;
    }
    return value->String;
}

std::optional<bool> AssetReader::Bool(std::string_view name, bool required)
{
    const JsonValue* value = Member(name, required);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    if (value->Type != JsonValue::Kind::Bool)
    {
        Report("field \"" + std::string(name) + "\" must be true or false");
        return std::nullopt;
    }
    return value->Bool;
}

std::optional<AssetVec> AssetReadVec3(const JsonValue& value, const std::string& where, AssetSink& sink)
{
    const bool shape = value.IsArray() && value.Array.size() == 3 && value.Array[0].Integer && value.Array[1].Integer && value.Array[2].Integer;
    if (!shape)
    {
        sink.Add(where + " must be [x, y, z], three whole numbers of millimetres");
        return std::nullopt;
    }
    for (const JsonValue& item : value.Array)
    {
        if (*item.Integer < -AssetCoordLimitMm || *item.Integer > AssetCoordLimitMm)
        {
            sink.Add(where + " has a coordinate beyond 200 m");
            return std::nullopt;
        }
    }
    return AssetVec{*value.Array[0].Integer, *value.Array[1].Integer, *value.Array[2].Integer};
}

std::optional<AssetVec> AssetReader::Vec3(std::string_view name, bool required)
{
    const JsonValue* value = Member(name, required);
    return value == nullptr ? std::nullopt : AssetReadVec3(*value, Context + "field \"" + std::string(name) + "\"", *Sink);
}

std::optional<std::array<std::int64_t, 2>> AssetReader::Pair(std::string_view name, bool required, std::int64_t lo, std::int64_t hi)
{
    const JsonValue* value = Member(name, required);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    if (!value->IsArray() || value->Array.size() != 2 || !value->Array[0].Integer || !value->Array[1].Integer)
    {
        Report("field \"" + std::string(name) + "\" must be two whole numbers");
        return std::nullopt;
    }
    const std::array<std::int64_t, 2> pair{*value->Array[0].Integer, *value->Array[1].Integer};
    if (pair[0] < lo || pair[0] > hi || pair[1] < lo || pair[1] > hi)
    {
        Report("field \"" + std::string(name) + "\" is outside " + std::to_string(lo) + ".." + std::to_string(hi));
        return std::nullopt;
    }
    return pair;
}

const JsonValue* AssetReader::Array(std::string_view name, bool required)
{
    const JsonValue* value = Member(name, required);
    if (value != nullptr && !value->IsArray())
    {
        Report("field \"" + std::string(name) + "\" must be an array");
        return nullptr;
    }
    return value;
}

std::optional<AssetReader> AssetReader::Child(std::string_view name, bool required)
{
    const JsonValue* value = Member(name, required);
    if (value == nullptr)
    {
        return std::nullopt;
    }
    if (!value->IsObject())
    {
        Report("field \"" + std::string(name) + "\" must be an object");
        return std::nullopt;
    }
    return AssetReader(*value, Context + std::string(name) + ": ", *Sink);
}

void AssetReader::Finish()
{
    for (const auto& [name, value] : Object->Object)
    {
        if (std::find(Known.begin(), Known.end(), name) == Known.end())
        {
            Report("unknown field \"" + name + "\"");
        }
    }
}

std::optional<AssetBox> AssetReadBox(AssetReader& parent, std::string_view name, bool required)
{
    std::optional<AssetReader> child = parent.Child(name, required);
    if (!child)
    {
        return std::nullopt;
    }
    const std::optional<AssetVec> lo = child->Vec3("min", true);
    const std::optional<AssetVec> hi = child->Vec3("max", true);
    child->Finish();
    if (!lo || !hi)
    {
        return std::nullopt;
    }
    if (lo->X >= hi->X || lo->Y >= hi->Y || lo->Z >= hi->Z)
    {
        child->Report("\"min\" must be below \"max\" on every axis");
        return std::nullopt;
    }
    return AssetBox{*lo, *hi};
}
} // namespace PerfectWorld::AssetDetail
