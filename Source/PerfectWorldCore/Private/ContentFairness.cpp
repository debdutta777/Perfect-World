#include "ContentSchemas.h"

#include <algorithm>
#include <array>

// The fairness rule (SCHOOL_AND_PROGRESSION.md: "Religion, language, family
// background and stream must never carry innate stat multipliers"). A key is
// refused when its name, or the name of an enclosing key, says it modifies
// something (multiplier, bonus, XP, rate...) and the key itself names one of
// those dimensions. Keys are split into lowercase words first, so
// "religionMultiplier", "stream_bonus" and "XPByFamily" are all caught while a
// plain list called "streams" is not.
namespace PerfectWorld::ContentSchema
{
namespace
{
std::vector<std::string> FairKeyTokens(std::string_view key)
{
    std::vector<std::string> tokens;
    std::string current;
    const auto flush = [&]() {
        if (!current.empty())
        {
            tokens.push_back(current);
            current.clear();
        }
    };
    const auto isUpper = [](char c) { return c >= 'A' && c <= 'Z'; };
    const auto isLower = [](char c) { return c >= 'a' && c <= 'z'; };
    const auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    for (std::size_t i = 0; i < key.size(); ++i)
    {
        const char c = key[i];
        if (!isUpper(c) && !isLower(c) && !isDigit(c))
        {
            flush();
            continue;
        }
        if (isUpper(c) && !current.empty())
        {
            const bool nextLower = i + 1 < key.size() && isLower(key[i + 1]);
            if (!isUpper(key[i - 1]) || nextLower)
                flush();
        }
        current.push_back(isUpper(c) ? static_cast<char>(c - 'A' + 'a') : c);
    }
    flush();
    return tokens;
}

struct FairDimension
{
    std::string_view Token;
    std::string_view Name;
};

constexpr std::array<FairDimension, 12> FairDimensions{{
    {"religion", "religion"}, {"religions", "religion"}, {"religious", "religion"}, {"faith", "religion"},
    {"language", "language"}, {"languages", "language"}, {"tongue", "language"},
    {"stream", "stream"}, {"streams", "stream"},
    {"family", "family"}, {"families", "family"},
    {"caste", "caste"},
}};

constexpr std::array<std::string_view, 27> FairModifierTokens{
    "multiplier", "multipliers", "multiply", "mult", "bonus", "bonuses", "modifier", "modifiers", "modify", "bias", "boost",
    "penalty", "penalties", "factor", "factors", "scale", "scaling", "advantage", "discount", "buff", "nerf", "adjust",
    "adjustment", "xp", "rate", "weight", "grant"};

std::string_view FairDimensionIn(const std::vector<std::string>& tokens)
{
    for (const std::string& token : tokens)
        for (const FairDimension& dimension : FairDimensions)
            if (token == dimension.Token)
                return dimension.Name;
    return {};
}

bool FairHasModifier(const std::vector<std::string>& tokens)
{
    for (const std::string& token : tokens)
        if (std::find(FairModifierTokens.begin(), FairModifierTokens.end(), token) != FairModifierTokens.end())
            return true;
    return false;
}

struct FairContext
{
    const std::string& File;
    const std::string& Id;
    std::vector<ContentIssue>& Issues;
};

void FairReport(const FairContext& ctx, const std::string& path, std::string_view dimension)
{
    ctx.Issues.push_back({ctx.File, ctx.Id,
                          "fairness rule: " + path + " keys a modifier on " + std::string(dimension) +
                              "; religion, language, family and stream never carry stat multipliers"});
}

void FairScan(const FairContext& ctx, const JsonValue& value, const std::string& path, bool underModifier)
{
    if (value.IsArray())
    {
        for (std::size_t i = 0; i < value.Array.size(); ++i)
        {
            const JsonValue& item = value.Array[i];
            const std::string itemPath = path + "[" + std::to_string(i) + "]";
            if (item.IsString() && underModifier)
            {
                if (const std::string_view dimension = FairDimensionIn(FairKeyTokens(item.String)); !dimension.empty())
                    FairReport(ctx, itemPath, dimension);
            }
            else
                FairScan(ctx, item, itemPath, underModifier);
        }
        return;
    }
    if (!value.IsObject())
        return;
    for (const auto& [key, child] : value.Object)
    {
        const std::vector<std::string> tokens = FairKeyTokens(key);
        const bool modifying = underModifier || FairHasModifier(tokens);
        const std::string childPath = path.empty() ? key : path + "." + key;
        if (const std::string_view dimension = FairDimensionIn(tokens); !dimension.empty() && modifying)
            FairReport(ctx, childPath, dimension);
        else if (child.IsString() && modifying)
        {
            // A modifier table written as values: "modifiers": [{"on": "religion", ...}].
            if (const std::string_view named = FairDimensionIn(FairKeyTokens(child.String)); !named.empty())
                FairReport(ctx, childPath, named);
        }
        FairScan(ctx, child, childPath, modifying);
    }
}
} // namespace

void CheckFairness(const JsonValue& root, const std::string& file, std::vector<ContentIssue>& issues)
{
    const JsonValue* id = root.Find("id");
    const std::string idText = id != nullptr && id->IsString() ? id->String : std::string();
    FairScan({file, idText, issues}, root, "", false);
}
} // namespace PerfectWorld::ContentSchema
