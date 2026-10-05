#include "AssetInternal.h"

#include <algorithm>

namespace PerfectWorld::AssetDetail
{
namespace
{
std::string AssetMm(std::int64_t value)
{
    return std::to_string(value) + " mm";
}

std::string AssetDescribe(const AssetBox& box)
{
    return "[" + std::to_string(box.Min.X) + "," + std::to_string(box.Min.Y) + "," + std::to_string(box.Min.Z) + "]..[" +
           std::to_string(box.Max.X) + "," + std::to_string(box.Max.Y) + "," + std::to_string(box.Max.Z) + "]";
}

void AssetCheckDimensions(AssetReader& root, AssetBodyContext& context)
{
    const AssetContract& contract = *context.Contract;
    const std::optional<AssetVec> dims = root.Vec3("dimensions_mm", true);
    const std::string pivot = root.String("pivot", true).value_or("bottom_center");
    if (pivot != "bottom_center")
    {
        root.Report("pivot must be \"bottom_center\" (the box is centred on X and Y, with Z from the floor up)");
    }
    const std::string front = root.String("front", true).value_or("+x");
    if (front != "+x" && front != "-x" && front != "+y" && front != "-y")
    {
        root.Report("front must be one of +x, -x, +y, -y");
    }
    if (!dims)
    {
        return;
    }
    if (dims->X < 1 || dims->Y < 1 || dims->Z < 1)
    {
        root.Report("dimensions_mm must be positive on every axis");
        return;
    }
    if (dims->X % 2 != 0 || dims->Y % 2 != 0)
    {
        root.Report("dimensions_mm X and Y must be even, so the bottom-centre pivot lands on a whole millimetre");
        return;
    }
    const std::array<std::int64_t, 3> size{dims->X, dims->Y, dims->Z};
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
        if (size[axis] < contract.MinDimensionsMm[axis] || size[axis] > contract.MaxDimensionsMm[axis])
        {
            static const char* const Names[] = {"X (depth)", "Y (width)", "Z (height)"};
            root.Report(std::string("dimension ") + Names[axis] + " " + AssetMm(size[axis]) + " is outside " + AssetMm(contract.MinDimensionsMm[axis]) +
                        ".." + AssetMm(contract.MaxDimensionsMm[axis]) + " allowed by contract " + contract.Key);
        }
    }
    if (contract.ExactHeightMm && dims->Z != *contract.ExactHeightMm)
    {
        root.Report("height " + AssetMm(dims->Z) + " must be exactly " + AssetMm(*contract.ExactHeightMm) + " (contract " + contract.Key + ")");
    }
    context.HaveDims = true;
    context.Dims = AssetBox{{-dims->X / 2, -dims->Y / 2, 0}, {dims->X / 2, dims->Y / 2, dims->Z}};
}

void AssetCheckEnvelope(AssetReader& root, AssetBodyContext& context)
{
    std::optional<AssetReader> envelope = root.Child("envelope_mm", true);
    if (!envelope)
    {
        return;
    }
    const std::optional<AssetBox> inner = AssetReadBox(*envelope, "inner", true);
    const std::optional<AssetBox> outer = AssetReadBox(*envelope, "outer", true);
    envelope->Finish();
    if (outer)
    {
        context.HaveOuter = true;
        context.Outer = *outer;
        if (context.HaveDims && !AssetBoxInside(context.Dims, *outer))
        {
            envelope->Report("the outer envelope " + AssetDescribe(*outer) + " must contain the dimensions box " + AssetDescribe(context.Dims));
        }
    }
    if (inner && context.HaveDims && !AssetBoxInside(*inner, context.Dims))
    {
        envelope->Report("the inner envelope " + AssetDescribe(*inner) + " must lie inside the dimensions box " + AssetDescribe(context.Dims));
    }
    if (inner && outer && !AssetBoxInside(*inner, *outer))
    {
        envelope->Report("the inner envelope must lie inside the outer envelope");
    }
}

/** "paper.3" -> the ramp "paper" and step 3. */
bool AssetSplitColor(const std::string& color, std::string& ramp)
{
    const std::size_t dot = color.rfind('.');
    if (dot == std::string::npos || dot + 2 != color.size() || color[dot + 1] < '0' || color[dot + 1] > '3')
    {
        return false;
    }
    ramp = color.substr(0, dot);
    return true;
}

bool AssetIsTileId(const std::string& tile)
{
    return tile.size() > 5 && tile.rfind("tile.", 0) == 0 && AssetIsLowerId(tile.substr(5));
}

void AssetCheckCuboid(AssetReader& cuboid, const AssetBodyContext& context, bool allowReserved, std::vector<std::string>& names,
                      std::optional<AssetBox>& span)
{
    const std::optional<std::string> name = cuboid.String("name", true);
    if (name && !AssetIsLowerId(*name))
    {
        cuboid.Report("name \"" + *name + "\" must be lowercase letters, digits and '_'");
    }
    if (name && AssetContainsText(names, *name))
    {
        cuboid.Report("duplicate cuboid name \"" + *name + "\"");
    }
    if (name)
    {
        names.push_back(*name);
    }
    const std::optional<AssetVec> origin = cuboid.Vec3("min_mm", true);
    const std::optional<AssetVec> size = cuboid.Vec3("size_mm", true);
    const std::optional<std::string> tile = cuboid.String("tile", false);
    const std::optional<std::string> tint = cuboid.String("tint", false);
    const std::optional<std::string> color = cuboid.String("color", false);
    cuboid.Bool("soft", false);
    if (const JsonValue* faces = cuboid.Member("faces", false))
    {
        if (!faces->IsObject())
        {
            cuboid.Report("field \"faces\" must be an object such as {\"-z\": \"hidden\"}");
        }
        else
        {
            for (const auto& [face, value] : faces->Object)
            {
                const bool knownFace = face.size() == 2 && (face[0] == '+' || face[0] == '-') && (face[1] == 'x' || face[1] == 'y' || face[1] == 'z');
                if (!knownFace || !value.IsString() || value.String != "hidden")
                {
                    cuboid.Report("faces entries look like \"-z\": \"hidden\" (got \"" + face + "\")");
                }
            }
        }
    }

    if (tile.has_value() == color.has_value())
    {
        cuboid.Report("a cuboid needs exactly one of \"tile\" (with an optional \"tint\") or \"color\"");
    }
    if (tile && !AssetIsTileId(*tile))
    {
        cuboid.Report("tile \"" + *tile + "\" must look like tile.name");
    }
    if (tint && !tile)
    {
        cuboid.Report("\"tint\" only goes with a \"tile\"");
    }
    std::string ramp;
    if (tint)
    {
        ramp = *tint;
        if (!AssetIsKnownRamp(ramp))
        {
            cuboid.Report("unknown palette ramp \"" + ramp + "\"");
        }
    }
    if (color && !(AssetSplitColor(*color, ramp) && AssetIsKnownRamp(ramp)))
    {
        cuboid.Report("color \"" + *color + "\" must be <palette ramp>.<step 0-3>, e.g. paper.3");
        ramp.clear();
    }
    if (AssetIsReservedRamp(ramp) && !allowReserved)
    {
        cuboid.Report("the reserved ramp \"" + ramp + "\" is only for the Gate kit (a teal-violet pixel always means Gate)");
    }

    if (!origin || !size)
    {
        return;
    }
    if (size->X < 1 || size->Y < 1 || size->Z < 1)
    {
        cuboid.Report("size_mm must be positive on every axis");
        return;
    }
    const AssetBox box{*origin, {origin->X + size->X, origin->Y + size->Y, origin->Z + size->Z}};
    if (context.HaveOuter && !AssetBoxInside(box, context.Outer))
    {
        cuboid.Report("cuboid " + AssetDescribe(box) + " lies outside the outer envelope " + AssetDescribe(context.Outer));
    }
    if (!span)
    {
        span = box;
        return;
    }
    span->Min = {std::min(span->Min.X, box.Min.X), std::min(span->Min.Y, box.Min.Y), std::min(span->Min.Z, box.Min.Z)};
    span->Max = {std::max(span->Max.X, box.Max.X), std::max(span->Max.Y, box.Max.Y), std::max(span->Max.Z, box.Max.Z)};
}
} // namespace

void AssetCheckBlock(AssetReader& root, const AssetBodyContext& context)
{
    std::optional<AssetReader> block = root.Child("block", true);
    if (!block)
    {
        return;
    }
    const std::string texelClass = block->String("class", true).value_or(context.Contract->TexelClass);
    if (texelClass != context.Contract->TexelClass)
    {
        block->Report("texel class " + texelClass + " does not match contract " + context.Contract->Key + " (" + context.Contract->TexelClass + ")");
    }
    const JsonValue* cuboids = block->Array("cuboids", true);
    if (cuboids == nullptr)
    {
        block->Finish();
        return;
    }
    if (cuboids->Array.empty() || cuboids->Array.size() > 64)
    {
        block->Report("a block model has 1 to 64 cuboids");
    }
    const bool allowReserved = context.Subcategory == "gatekit";
    std::vector<std::string> names;
    std::optional<AssetBox> span;
    std::size_t index = 0;
    for (const JsonValue& item : cuboids->Array)
    {
        const std::string where = block->Where() + "cuboids[" + std::to_string(index++) + "]: ";
        if (!item.IsObject())
        {
            block->Report("cuboids must contain only objects");
            continue;
        }
        AssetReader cuboid(item, where, block->SinkRef());
        AssetCheckCuboid(cuboid, context, allowReserved, names, span);
        cuboid.Finish();
    }
    block->Finish();
    if (span && context.HaveDims && (span->Min.X != context.Dims.Min.X || span->Min.Y != context.Dims.Min.Y || span->Min.Z != context.Dims.Min.Z ||
                                     span->Max.X != context.Dims.Max.X || span->Max.Y != context.Dims.Max.Y || span->Max.Z != context.Dims.Max.Z))
    {
        block->Report("the cuboids span " + AssetDescribe(*span) + " but dimensions_mm describes " + AssetDescribe(context.Dims));
    }
}

void AssetCheckBody(AssetReader& root, const AssetContract& contract, const std::string& assetId, const std::string& subcategory)
{
    AssetBodyContext context;
    context.Contract = &contract;
    context.Id = assetId;
    context.Subcategory = subcategory;
    AssetCheckDimensions(root, context);
    AssetCheckEnvelope(root, context);
    AssetCheckBlock(root, context);
    AssetCheckGameplay(root, context);
}
} // namespace PerfectWorld::AssetDetail
