#include "AssetRegistry.h"

#include "AssetInternal.h"

#include <algorithm>

namespace PerfectWorld
{
using namespace AssetDetail;

// ---------------------------------------------------------------------------
// Database queries
// ---------------------------------------------------------------------------

const AssetRecord* AssetDb::Find(std::string_view id) const
{
    const auto it = Assets.find(id);
    return it == Assets.end() ? nullptr : &it->second;
}

const AssetContract* AssetDb::FindContract(std::string_view key) const
{
    const auto it = Contracts.find(key);
    return it == Contracts.end() ? nullptr : &it->second;
}

std::size_t AssetDb::AssetCount() const
{
    return Assets.size();
}

std::size_t AssetDb::ContractCount() const
{
    return Contracts.size();
}

std::size_t AssetDb::CountInCategory(std::string_view category) const
{
    return static_cast<std::size_t>(
        std::count_if(Assets.begin(), Assets.end(), [&](const auto& entry) { return entry.second.Category == category; }));
}

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------

namespace
{
/** ASSET_PIPELINE.md §3.1 rule 3: an ID never says how or by whom a thing is drawn. */
std::string AssetBannedWord(const std::string& id)
{
    static const char* const Banned[] = {"block", "kenney", "ai", "v2", "final"};
    for (const std::string& segment : AssetSplit(id, '.'))
    {
        for (const std::string& token : AssetSplit(segment, '_'))
        {
            for (const char* word : Banned)
            {
                if (token == word)
                {
                    return word;
                }
            }
        }
    }
    return "";
}

std::string AssetNormalizePath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

bool AssetPathEndsWith(const std::string& path, const std::string& suffix)
{
    return path.size() >= suffix.size() && path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
}
} // namespace

bool IsValidAssetId(std::string_view id)
{
    const std::vector<std::string> parts = AssetSplit(id, '.');
    if (parts.size() < 3 || parts.size() > 4)
    {
        return false;
    }
    for (std::size_t i = 0; i < parts.size(); ++i)
    {
        if (!AssetSegmentOk(parts[i], i == 0))
        {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Asset header: identity, status, tags, contract link, art
// ---------------------------------------------------------------------------

namespace
{
void AssetCheckTags(AssetReader& root)
{
    static const char* const ToneTags[] = {"tone.injury", "tone.poverty", "tone.care", "tone.grief", "tone.intimacy", "tone.violence"};
    const JsonValue* tags = root.Array("tags", true);
    if (tags == nullptr)
    {
        return;
    }
    for (const JsonValue& tag : tags->Array)
    {
        const bool plain = tag.IsString() && !tag.String.empty() &&
                           std::all_of(tag.String.begin(), tag.String.end(), [](char c) {
                               return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
                           });
        if (!plain)
        {
            root.Report("field \"tags\" must hold lowercase words (letters, digits, '_' and '.')");
        }
        else if (tag.String.rfind("tone.", 0) == 0 &&
                 std::none_of(std::begin(ToneTags), std::end(ToneTags), [&](const char* t) { return tag.String == t; }))
        {
            root.Report("unknown tone tag \"" + tag.String + "\"");
        }
    }
}

void AssetCheckArt(AssetReader& root)
{
    std::optional<AssetReader> art = root.Child("art", true);
    if (!art)
    {
        return;
    }
    const std::string status = art->String("status", true).value_or("");
    if (status != "block" && status != "real")
        art->Report("status must be \"block\" or \"real\"");
    art->Bool("enabled", true);
    for (const char* name : {"mesh", "skeleton", "import", "provenance"})
    {
        const JsonValue* value = art->Member(name, true);
        if (value != nullptr && !value->IsNull() && !value->IsString())
            art->Report(std::string("field \"") + name + "\" must be null or a path string");
        if (status == "block" && value != nullptr && !value->IsNull())
            art->Report(std::string("a block-status asset must have null \"") + name + "\"");
        if (status == "real" && value != nullptr && value->IsNull() && (std::string(name) == "mesh" || std::string(name) == "provenance"))
            art->Report(std::string("a real-status asset needs a \"") + name + "\"");
    }
    for (const char* name : {"lods", "materials"})
    {
        const JsonValue* list = art->Array(name, true);
        if (list != nullptr && status == "block" && !list->Array.empty())
            art->Report(std::string("a block-status asset must have an empty \"") + name + "\"");
    }
    art->Finish();
}

class AssetLoader
{
public:
    AssetLoadResult Result;

    struct PendingAsset
    {
        std::string File;
        JsonValue Json;
    };
    std::vector<PendingAsset> Pending;
    std::vector<std::pair<const AssetRecord*, std::string>> Replacements;

    void LoadFile(const ContentFile& file)
    {
        std::string_view text = file.Text;
        if (text.substr(0, 3) == "\xEF\xBB\xBF")
        {
            text.remove_prefix(3);
        }
        const std::string path = AssetNormalizePath(file.Path);
        const JsonParseResult parsed = ParseJson(text);
        if (!parsed.Value)
        {
            Result.Issues.push_back({path, "", "not valid JSON at line " + std::to_string(parsed.Line) + ": " + parsed.Error});
            return;
        }
        const JsonValue* schema = parsed.Value->IsObject() ? parsed.Value->Find("schema") : nullptr;
        if (schema == nullptr || !schema->IsString())
        {
            Result.Issues.push_back({path, "", "a registry file is one object with a string \"schema\""});
        }
        else if (schema->String == "pw.contract/1")
        {
            LoadContract(*parsed.Value, path);
        }
        else if (schema->String == "pw.asset/1")
        {
            Pending.push_back({path, *parsed.Value});
        }
        else
        {
            Result.Issues.push_back({path, "", "unknown schema \"" + schema->String + "\" (expected pw.asset/1 or pw.contract/1)"});
        }
    }

    void LoadContract(const JsonValue& json, const std::string& path)
    {
        std::optional<AssetContract> contract = AssetParseContract(json, path, Result.Issues);
        if (!contract)
        {
            return;
        }
        const std::string expected = "/Contracts/" + contract->Family + (contract->Version == 1 ? "" : "@" + std::to_string(contract->Version)) + ".json";
        if (!AssetPathEndsWith(path, expected))
        {
            Result.Issues.push_back({path, contract->Key, "the file must be Contracts/<family>.json, i.e. ending in " + expected});
        }
        const auto inserted = Result.Db.Contracts.emplace(contract->Key, *contract);
        if (!inserted.second)
        {
            Result.Issues.push_back({path, contract->Key, "duplicate contract (first defined in " + inserted.first->second.File + ")"});
        }
    }

    void LoadAsset(const PendingAsset& pending)
    {
        AssetSink sink(Result.Issues, pending.File, "");
        AssetReader root(pending.Json, "", sink);
        root.Member("schema", true);
        const std::optional<std::string> id = root.String("id", true);
        if (!id)
        {
            root.Finish();
            return;
        }
        sink.Id = *id;
        const std::vector<std::string> parts = AssetSplit(*id, '.');
        const bool idOk = IsValidAssetId(*id);
        if (!idOk)
        {
            root.Report("ID \"" + *id + "\" must be category.subcategory.name[.variant]: 3-4 segments of at most 32 lowercase letters, digits and '_'");
        }
        else
        {
            if (const std::string banned = AssetBannedWord(*id); !banned.empty())
                root.Report("ID \"" + *id + "\" says how a thing is drawn (\"" + banned + "\"); an ID says what a thing is");
            std::string rest = parts[2];
            for (std::size_t i = 3; i < parts.size(); ++i)
                rest += "." + parts[i];
            const std::string expected = "/Assets/" + parts[0] + "/" + parts[1] + "/" + rest + ".json";
            if (!AssetPathEndsWith(pending.File, expected))
                root.Report("the file must be Assets/<category>/<subcategory>/<name>.json, i.e. ending in " + expected);
        }

        AssetRecord record;
        record.Id = *id;
        record.File = pending.File;
        record.Data = pending.Json;
        if (idOk)
        {
            record.Category = parts[0];
            record.Subcategory = parts[1];
        }
        root.String("display_name", true);
        const std::string status = root.String("status", true).value_or("");
        if (status != "active" && status != "retired")
            root.Report("status must be \"active\" or \"retired\"");
        const std::optional<std::string> replacedBy = root.String("replaced_by", false);
        if (status == "retired" && !replacedBy)
            root.Report("a retired asset needs \"replaced_by\" (ASSET_PIPELINE.md 3.1 rule 2)");
        if (status == "active" && replacedBy)
            root.Report("only a retired asset has \"replaced_by\"");
        const std::string milestone = root.String("milestone", true).value_or("M1");
        const bool milestoneOk = milestone.size() >= 2 && milestone[0] == 'M' &&
                                 std::all_of(milestone.begin() + 1, milestone.end(), [](char c) { return c >= '0' && c <= '9'; }) &&
                                 std::stoi(milestone.substr(1)) <= 10;
        if (!milestoneOk)
            root.Report("milestone must be M0..M10");
        AssetCheckTags(root);
        AssetCheckArt(root);

        const std::optional<std::string> contractKey = root.String("contract", true);
        const AssetContract* contract = nullptr;
        if (contractKey)
        {
            record.ContractKey = *contractKey;
            contract = Result.Db.FindContract(*contractKey);
            if (contract == nullptr)
            {
                root.Report("unknown family: contract \"" + *contractKey + "\" does not exist (no Contracts file declares it)");
            }
        }
        if (contract != nullptr && idOk)
        {
            if (contract->Category != record.Category)
                root.Report("category \"" + record.Category + "\" does not match contract " + contract->Key + " (" + contract->Category + ")");
            else if (!AssetContainsText(contract->Subcategories, record.Subcategory))
                root.Report("subcategory \"" + record.Subcategory + "\" is not one of the subcategories of contract " + contract->Key);
            if (record.Subcategory == "door" && contract->Family.rfind("arch.door", 0) != 0)
                root.Report("a door must use an arch.door contract so that its height is checked");
        }
        if (contract != nullptr)
        {
            AssetCheckBody(root, *contract, *id, record.Subcategory);
        }
        else
        {
            // Without a contract nothing more can be judged; accept the body's names so they are not also reported as unknown.
            for (const char* name : {"dimensions_mm", "pivot", "front", "envelope_mm", "block", "gameplay"})
                root.Member(name, false);
        }
        root.Finish();

        const auto inserted = Result.Db.Assets.emplace(*id, std::move(record));
        if (!inserted.second)
        {
            root.Report("duplicate ID (first defined in " + inserted.first->second.File + ")");
        }
        else if (replacedBy)
        {
            Replacements.emplace_back(&inserted.first->second, *replacedBy);
        }
    }

    void Finish()
    {
        for (const PendingAsset& pending : Pending)
        {
            LoadAsset(pending);
        }
        for (const auto& [record, target] : Replacements)
        {
            if (Result.Db.Find(target) == nullptr)
                Result.Issues.push_back({record->File, record->Id, "replaced_by names " + target + ", which does not exist"});
        }
    }
};
} // namespace

AssetLoadResult LoadAssets(const std::vector<ContentFile>& files)
{
    AssetLoader loader;
    // Contracts first, so an asset's contract is known whatever order the files arrive in.
    for (const ContentFile& file : files)
    {
        loader.LoadFile(file);
    }
    loader.Finish();
    return std::move(loader.Result);
}
} // namespace PerfectWorld
