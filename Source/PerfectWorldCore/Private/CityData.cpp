#include "CityData.h"

namespace PerfectWorld
{
namespace
{
std::optional<std::string> CityString(const JsonValue& object, std::string_view key, bool required)
{
    const JsonValue* value = object.Find(key);
    if (value == nullptr)
    {
        return required ? std::nullopt : std::optional<std::string>(std::string());
    }
    if (!value->IsString())
    {
        return std::nullopt;
    }
    return value->String;
}

bool CityFlag(const JsonValue& object, std::string_view key, bool& out)
{
    const JsonValue* value = object.Find(key);
    if (value == nullptr)
    {
        out = false;
        return true;
    }
    if (value->Type != JsonValue::Kind::Bool)
    {
        return false;
    }
    out = value->Bool;
    return true;
}

std::optional<HazardRank> CityParseRank(std::string_view name)
{
    for (std::int32_t i = 0; i <= static_cast<std::int32_t>(HazardRank::S); ++i)
    {
        if (HazardRankName(static_cast<HazardRank>(i)) == name)
        {
            return static_cast<HazardRank>(i);
        }
    }
    return std::nullopt;
}

std::optional<GateSiteStatus> CityParseStatus(std::string_view name)
{
    for (std::int32_t i = 0; i <= static_cast<std::int32_t>(GateSiteStatus::Open); ++i)
    {
        if (GateSiteStatusName(static_cast<GateSiteStatus>(i)) == name)
        {
            return static_cast<GateSiteStatus>(i);
        }
    }
    return std::nullopt;
}

std::optional<DistrictRecord> CityReadDistrict(const JsonValue& json)
{
    if (!json.IsObject())
    {
        return std::nullopt;
    }
    DistrictRecord district;
    const auto id = CityString(json, "id", true);
    const auto name = CityString(json, "name", true);
    const auto kind = CityString(json, "kind", true);
    if (!id || !name || !kind || !CityFlag(json, "fictional", district.Fictional))
    {
        return std::nullopt;
    }
    district.Id = *id;
    district.Name = *name;
    if (*kind == "generated")
    {
        district.Kind = DistrictKind::Generated;
        const auto archetype = CityString(json, "archetype", true);
        const JsonValue* seed = json.Find("seed");
        if (!archetype || seed == nullptr || !seed->Integer || *seed->Integer < 0 || *seed->Integer > 4'294'967'295LL)
        {
            return std::nullopt;
        }
        district.Archetype = ParseDistrictArchetype(*archetype);
        if (!district.Archetype)
        {
            return std::nullopt;
        }
        district.Seed = static_cast<std::uint32_t>(*seed->Integer);
    }
    else if (*kind != "authored")
    {
        return std::nullopt;
    }
    return district;
}

std::optional<GateSiteRecord> CityReadGateSite(const JsonValue& json)
{
    if (!json.IsObject())
    {
        return std::nullopt;
    }
    GateSiteRecord site;
    const auto id = CityString(json, "id", true);
    const auto name = CityString(json, "name", true);
    const auto district = CityString(json, "district", true);
    const auto location = CityString(json, "location", false);
    const auto rank = CityString(json, "rank", true);
    const auto status = CityString(json, "status", true);
    if (!id || !name || !district || !location || !rank || !status || !CityFlag(json, "realmAnchor", site.RealmAnchor))
    {
        return std::nullopt;
    }
    const auto parsedRank = CityParseRank(*rank);
    const auto parsedStatus = CityParseStatus(*status);
    if (!parsedRank || !parsedStatus)
    {
        return std::nullopt;
    }
    site.Id = *id;
    site.Name = *name;
    site.DistrictId = *district;
    site.Location = *location;
    site.Rank = *parsedRank;
    site.Status = *parsedStatus;
    return site;
}
} // namespace

const DistrictRecord* CityRecord::FindDistrict(std::string_view id) const
{
    for (const DistrictRecord& district : Districts)
    {
        if (district.Id == id)
        {
            return &district;
        }
    }
    return nullptr;
}

std::vector<const GateSiteRecord*> CityRecord::GateSitesIn(std::string_view districtId) const
{
    std::vector<const GateSiteRecord*> sites;
    for (const GateSiteRecord& site : GateSites)
    {
        if (site.DistrictId == districtId)
        {
            sites.push_back(&site);
        }
    }
    return sites;
}

std::string_view GateSiteStatusName(GateSiteStatus status)
{
    constexpr std::string_view Names[] = {"contained", "sealed", "open"};
    return Names[static_cast<std::size_t>(status)];
}

std::optional<CityRecord> ReadCity(const ContentDb& db, std::string_view id)
{
    const ContentRecord* record = db.Find(id);
    if (record == nullptr || record->Type != "city" || !record->Data.IsObject())
    {
        return std::nullopt;
    }
    const JsonValue& json = record->Data;
    const auto name = CityString(json, "name", true);
    const auto nation = CityString(json, "nation", true);
    const auto state = CityString(json, "state", true);
    const auto role = CityString(json, "role", true);
    const JsonValue* districts = json.Find("districts");
    const JsonValue* gateSites = json.Find("gateSites");
    if (!name || !nation || !state || !role || districts == nullptr || !districts->IsArray() || gateSites == nullptr ||
        !gateSites->IsArray())
    {
        return std::nullopt;
    }

    CityRecord city;
    city.Id = record->Id;
    city.Name = *name;
    city.NationId = *nation;
    city.State = *state;
    city.Role = *role;
    for (const JsonValue& item : districts->Array)
    {
        auto district = CityReadDistrict(item);
        if (!district)
        {
            return std::nullopt;
        }
        city.Districts.push_back(std::move(*district));
    }
    for (const JsonValue& item : gateSites->Array)
    {
        auto site = CityReadGateSite(item);
        if (!site)
        {
            return std::nullopt;
        }
        city.GateSites.push_back(std::move(*site));
    }
    return city;
}

std::vector<CityRecord> ReadAllCities(const ContentDb& db)
{
    std::vector<CityRecord> cities;
    for (const ContentRecord* record : db.OfType("city"))
    {
        if (auto city = ReadCity(db, record->Id))
        {
            cities.push_back(std::move(*city));
        }
    }
    return cities;
}

std::optional<GeneratedDistrict> GenerateCityDistrict(const DistrictRecord& district)
{
    if (district.Kind != DistrictKind::Generated || !district.Archetype)
    {
        return std::nullopt;
    }
    return GenerateDistrict(*district.Archetype, district.Seed);
}
} // namespace PerfectWorld
