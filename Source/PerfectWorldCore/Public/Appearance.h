#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * How a character looks.
 *
 * Stored as engine-neutral data, never as a chosen sprite or a chosen mesh:
 * named option IDs plus plain colour values. The same saved record therefore
 * drives a composed 2D sprite today and a 3D morph-target rig later without
 * invalidating a single save. Nothing here costs creation points, and nothing
 * here may affect trait costs or supernatural potential.
 *
 * Continuous values are limited to the three colours, matching the decision of
 * 2026-09-20. Proportions stay discrete until the 3D rig exists, because
 * continuous proportions in 2D would need runtime mesh deformation.
 */
constexpr std::int32_t AppearanceSchemaVersion = 1;
constexpr std::size_t FaceShapeCount = 4;
constexpr std::size_t HairStyleCount = 4;
constexpr std::size_t BuildCount = 3;
constexpr std::size_t UniformVariantCount = 2;

/** A plain 8-bit-per-channel colour. No engine type appears in the rules. */
struct ColorValue
{
    std::int32_t R = 0;
    std::int32_t G = 0;
    std::int32_t B = 0;

    friend bool operator==(const ColorValue& a, const ColorValue& b) noexcept
    {
        return a.R == b.R && a.G == b.G && a.B == b.B;
    }
};

/** One discrete choice a picker can offer. */
struct AppearanceOption
{
    std::string_view Id;
    std::string_view DisplayName;
};

/**
 * Which art layer a colour drives. The art must be supplied flat greyscale for
 * every tinted layer, so recording the mapping here keeps the rules and the
 * art brief describing the same thing.
 */
enum class AppearanceColorChannel
{
    SkinTone,
    HairColor,
    EyeColor
};

struct AppearanceRecord
{
    std::int32_t SchemaVersion = AppearanceSchemaVersion;
    std::string FaceShapeId;
    std::string HairStyleId;
    std::string BuildId;
    std::string UniformVariantId;
    ColorValue SkinTone;
    ColorValue HairColor;
    ColorValue EyeColor;

    /** A record with every documented default, valid on its own. */
    [[nodiscard]] PERFECTWORLDCORE_API static AppearanceRecord Default();
};

[[nodiscard]] PERFECTWORLDCORE_API const std::array<AppearanceOption, FaceShapeCount>& FaceShapes() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const std::array<AppearanceOption, HairStyleCount>& HairStyles() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const std::array<AppearanceOption, BuildCount>& Builds() noexcept;
[[nodiscard]] PERFECTWORLDCORE_API const std::array<AppearanceOption, UniformVariantCount>& UniformVariants() noexcept;

/** True when the ID names one of the authored options for that picker. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsKnownFaceShape(std::string_view id) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsKnownHairStyle(std::string_view id) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsKnownBuild(std::string_view id) noexcept;
[[nodiscard]] PERFECTWORLDCORE_API bool IsKnownUniformVariant(std::string_view id) noexcept;

/** True when every channel is inside 0-255. */
[[nodiscard]] PERFECTWORLDCORE_API bool IsValidColor(const ColorValue& color) noexcept;

/** Clamps each channel into 0-255 rather than rejecting a slider overshoot. */
[[nodiscard]] PERFECTWORLDCORE_API ColorValue ClampColor(const ColorValue& color) noexcept;

/**
 * Describes why a record is unusable, or an empty string when it is valid.
 * Colours are clamped rather than refused, so only unknown option IDs and an
 * unsupported schema version can fail.
 */
[[nodiscard]] PERFECTWORLDCORE_API std::string DescribeAppearanceProblem(const AppearanceRecord& record);
} // namespace PerfectWorld
