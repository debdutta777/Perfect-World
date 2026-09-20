#include "Appearance.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
// Face shapes and builds are deliberately neutral descriptions of structure.
// They are not flattering-versus-unflattering options and must never be
// treated as a quality ranking.
constexpr std::array<AppearanceOption, FaceShapeCount> FaceShapeOptions{{
    {"face_round", "Round"},
    {"face_oval", "Oval"},
    {"face_square", "Square"},
    {"face_heart", "Heart"},
}};

constexpr std::array<AppearanceOption, HairStyleCount> HairStyleOptions{{
    {"hair_short_crop", "Short crop"},
    {"hair_wavy_bob", "Wavy bob"},
    {"hair_long_braid", "Long braid"},
    {"hair_curly_loose", "Loose curls"},
}};

constexpr std::array<AppearanceOption, BuildCount> BuildOptions{{
    {"build_slight", "Slight"},
    {"build_average", "Average"},
    {"build_sturdy", "Sturdy"},
}};

constexpr std::array<AppearanceOption, UniformVariantCount> UniformVariantOptions{{
    {"uniform_school_navy", "School uniform, navy and light blue"},
    {"uniform_casual", "Casual home wear"},
}};

template <std::size_t Count>
bool Contains(const std::array<AppearanceOption, Count>& options, std::string_view id) noexcept
{
    return std::find_if(options.begin(), options.end(),
                        [id](const AppearanceOption& o) { return o.Id == id; }) != options.end();
}
} // namespace

const std::array<AppearanceOption, FaceShapeCount>& FaceShapes() noexcept { return FaceShapeOptions; }
const std::array<AppearanceOption, HairStyleCount>& HairStyles() noexcept { return HairStyleOptions; }
const std::array<AppearanceOption, BuildCount>& Builds() noexcept { return BuildOptions; }
const std::array<AppearanceOption, UniformVariantCount>& UniformVariants() noexcept { return UniformVariantOptions; }

bool IsKnownFaceShape(std::string_view id) noexcept
{
    return Contains(FaceShapeOptions, id);
}

bool IsKnownHairStyle(std::string_view id) noexcept
{
    return Contains(HairStyleOptions, id);
}

bool IsKnownBuild(std::string_view id) noexcept
{
    return Contains(BuildOptions, id);
}

bool IsKnownUniformVariant(std::string_view id) noexcept
{
    return Contains(UniformVariantOptions, id);
}

bool IsValidColor(const ColorValue& color) noexcept
{
    const auto InRange = [](std::int32_t channel) { return channel >= 0 && channel <= 255; };
    return InRange(color.R) && InRange(color.G) && InRange(color.B);
}

ColorValue ClampColor(const ColorValue& color) noexcept
{
    return {std::clamp(color.R, 0, 255), std::clamp(color.G, 0, 255), std::clamp(color.B, 0, 255)};
}

AppearanceRecord AppearanceRecord::Default()
{
    AppearanceRecord record;
    record.SchemaVersion = AppearanceSchemaVersion;
    record.FaceShapeId = std::string(FaceShapeOptions[1].Id);
    record.HairStyleId = std::string(HairStyleOptions[0].Id);
    record.BuildId = std::string(BuildOptions[1].Id);
    record.UniformVariantId = std::string(UniformVariantOptions[0].Id);
    // A mid-range starting point for each slider. These are defaults a player
    // moves immediately, not a statement about which values are normal.
    record.SkinTone = {166, 124, 91};
    record.HairColor = {36, 28, 24};
    record.EyeColor = {74, 52, 38};
    return record;
}

std::string DescribeAppearanceProblem(const AppearanceRecord& record)
{
    if (record.SchemaVersion < 1 || record.SchemaVersion > AppearanceSchemaVersion)
    {
        return "The appearance record uses an unsupported version.";
    }
    if (!IsKnownFaceShape(record.FaceShapeId))
    {
        return "Unknown face shape: " + record.FaceShapeId;
    }
    if (!IsKnownHairStyle(record.HairStyleId))
    {
        return "Unknown hair style: " + record.HairStyleId;
    }
    if (!IsKnownBuild(record.BuildId))
    {
        return "Unknown build: " + record.BuildId;
    }
    if (!IsKnownUniformVariant(record.UniformVariantId))
    {
        return "Unknown uniform variant: " + record.UniformVariantId;
    }
    return std::string();
}
} // namespace PerfectWorld
