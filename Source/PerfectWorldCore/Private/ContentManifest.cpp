#include "ContentManifest.h"

#include "ContentSchemas.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <sstream>
#include <tuple>

namespace PerfectWorld
{
namespace
{
bool RecordLess(const ContentManifestRecord& a, const ContentManifestRecord& b)
{
    return std::tie(a.Id, a.Kind) < std::tie(b.Id, b.Kind);
}

bool FileLess(const ContentManifestFile& a, const ContentManifestFile& b)
{
    if (a.Path != b.Path)
    {
        return a.Path < b.Path;
    }
    if (a.Records.size() != b.Records.size())
    {
        return a.Records.size() < b.Records.size();
    }
    for (std::size_t i = 0; i < a.Records.size(); ++i)
    {
        if (RecordLess(a.Records[i], b.Records[i]))
        {
            return true;
        }
        if (RecordLess(b.Records[i], a.Records[i]))
        {
            return false;
        }
    }
    return false;
}

void CollectRecordsFromObject(const ContentSchema::TypeSchema& schema, const JsonValue& object,
                              std::vector<ContentManifestRecord>& out)
{
    if (!object.IsObject())
    {
        return;
    }
    if (!schema.IdPrefix.empty())
    {
        const JsonValue* idValue = object.Find("id");
        if (idValue != nullptr && idValue->IsString() && !idValue->String.empty())
        {
            out.push_back({idValue->String, schema.Type});
        }
    }
    for (const ContentSchema::FieldSpec& field : schema.Fields)
    {
        if (field.Nested == nullptr)
        {
            continue;
        }
        const JsonValue* value = object.Find(field.Name);
        if (value == nullptr)
        {
            continue;
        }
        if (field.Kind == ContentSchema::FieldKind::Object && value->IsObject())
        {
            CollectRecordsFromObject(*field.Nested, *value, out);
        }
        else if (field.Kind == ContentSchema::FieldKind::ObjectArray && value->IsArray())
        {
            for (const JsonValue& item : value->Array)
            {
                if (item.IsObject())
                {
                    CollectRecordsFromObject(*field.Nested, item, out);
                }
            }
        }
    }
}

void EscapeJsonString(std::ostringstream& out, std::string_view text)
{
    out << '"';
    for (const unsigned char c : text)
    {
        switch (c)
        {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (c < 0x20)
            {
                const char* hex = "0123456789abcdef";
                out << "\\u00" << hex[(c >> 4) & 0xF] << hex[c & 0xF];
            }
            else
            {
                out << static_cast<char>(c);
            }
            break;
        }
    }
    out << '"';
}

std::string NormalizeBomText(std::string_view text)
{
    if (text.size() >= 3 && text.substr(0, 3) == "\xEF\xBB\xBF")
    {
        return std::string(text.substr(3));
    }
    if (text.size() >= 2 && static_cast<unsigned char>(text[0]) == 0xFF && static_cast<unsigned char>(text[1]) == 0xFE)
    {
        std::string out;
        out.reserve(text.size() / 2);
        for (std::size_t i = 2; i + 1 < text.size(); i += 2)
        {
            const std::uint16_t unit = static_cast<std::uint16_t>(static_cast<unsigned char>(text[i]) |
                                                                  (static_cast<unsigned char>(text[i + 1]) << 8));
            if (unit < 0x80)
            {
                out.push_back(static_cast<char>(unit));
            }
            else if (unit < 0x800)
            {
                out.push_back(static_cast<char>(0xC0 | (unit >> 6)));
                out.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
            }
            else
            {
                out.push_back(static_cast<char>(0xE0 | (unit >> 12)));
                out.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
            }
        }
        return out;
    }
    return std::string(text);
}
} // namespace

std::size_t ContentManifest::FileCount() const
{
    return Files.size();
}

std::size_t ContentManifest::RecordCount() const
{
    std::size_t total = 0;
    for (const ContentManifestFile& file : Files)
    {
        total += file.Records.size();
    }
    return total;
}

std::size_t ContentManifest::CountOfKind(std::string_view kind) const
{
    std::size_t total = 0;
    for (const ContentManifestFile& file : Files)
    {
        for (const ContentManifestRecord& record : file.Records)
        {
            if (record.Kind == kind)
            {
                ++total;
            }
        }
    }
    return total;
}

const ContentManifestFile* ContentManifest::FindFile(std::string_view path) const
{
    const std::string normalized = NormalizeContentManifestPath(path);
    for (const ContentManifestFile& file : Files)
    {
        if (file.Path == normalized)
        {
            return &file;
        }
    }
    return nullptr;
}

bool IsContentManifestPath(std::string_view path)
{
    std::string normalized(path);
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    if (normalized == ContentManifestFilename)
    {
        return true;
    }
    const std::string suffix = "/" + std::string(ContentManifestFilename);
    return normalized.size() >= suffix.size() &&
           normalized.compare(normalized.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string NormalizeContentManifestPath(std::string_view path)
{
    std::string raw(path);
    std::replace(raw.begin(), raw.end(), '\\', '/');
    std::string collapsed;
    collapsed.reserve(raw.size());
    for (const char c : raw)
    {
        if (c == '/' && !collapsed.empty() && collapsed.back() == '/')
        {
            continue;
        }
        collapsed.push_back(c);
    }
    while (collapsed.rfind("./", 0) == 0)
    {
        collapsed.erase(0, 2);
    }
    constexpr std::string_view marker = "/Data/Content/";
    if (const std::size_t pos = collapsed.rfind(marker); pos != std::string::npos)
    {
        collapsed = collapsed.substr(pos + marker.size() - 1);
    }
    else if (collapsed.rfind("Data/Content/", 0) == 0)
    {
        collapsed = collapsed.substr(std::string_view("Data/Content").size());
    }
    if (!collapsed.empty() && collapsed.front() != '/')
    {
        collapsed.insert(collapsed.begin(), '/');
    }
    return collapsed;
}

ContentManifest BuildContentManifest(const ContentDb& db)
{
    std::map<std::string, std::vector<ContentManifestRecord>> byFile;
    for (const auto& [key, record] : db.Records)
    {
        if (IsContentManifestPath(record.File))
        {
            continue;
        }
        const std::string path = NormalizeContentManifestPath(record.File);
        const std::string id = record.Id.empty() ? key : record.Id;
        byFile[path].push_back({id, record.Type});
    }
    ContentManifest manifest;
    manifest.Files.reserve(byFile.size());
    for (auto& [path, records] : byFile)
    {
        std::sort(records.begin(), records.end(), RecordLess);
        manifest.Files.push_back({path, std::move(records)});
    }
    return manifest;
}

ContentManifest BuildContentManifest(const ContentLoadResult& loaded)
{
    ContentManifest manifest = BuildContentManifest(loaded.Db);
    constexpr std::string_view duplicatePrefix = "duplicate ID (first defined in ";
    bool addedDuplicate = false;
    for (const ContentIssue& issue : loaded.Issues)
    {
        if (issue.Id.empty() || issue.File.empty() || issue.Message.rfind(duplicatePrefix, 0) != 0)
        {
            continue;
        }
        const ContentRecord* first = loaded.Db.Find(issue.Id);
        const std::string kind = first != nullptr ? first->Type : std::string();
        const std::string path = NormalizeContentManifestPath(issue.File);
        auto it = std::find_if(manifest.Files.begin(), manifest.Files.end(),
                               [&](const ContentManifestFile& f) { return f.Path == path; });
        if (it == manifest.Files.end())
        {
            manifest.Files.push_back({path, {{issue.Id, kind}}});
        }
        else
        {
            it->Records.push_back({issue.Id, kind});
        }
        addedDuplicate = true;
    }
    if (addedDuplicate)
    {
        for (ContentManifestFile& file : manifest.Files)
        {
            std::sort(file.Records.begin(), file.Records.end(), RecordLess);
        }
        std::sort(manifest.Files.begin(), manifest.Files.end(), FileLess);
    }
    return manifest;
}

ContentManifest BuildContentManifest(const std::vector<ContentFile>& files)
{
    std::map<std::string, std::vector<ContentManifestRecord>> byFile;
    for (const ContentFile& file : files)
    {
        if (IsContentManifestPath(file.Path))
        {
            continue;
        }
        const std::string path = NormalizeContentManifestPath(file.Path);
        std::vector<ContentManifestRecord>& records = byFile[path];

        std::string_view text = file.Text;
        if (text.substr(0, 3) == "\xEF\xBB\xBF")
        {
            text.remove_prefix(3);
        }
        const JsonParseResult parsed = ParseJson(text);
        if (!parsed.Value || !parsed.Value->IsObject())
        {
            continue;
        }
        const JsonValue& root = *parsed.Value;
        const JsonValue* typeVal = root.Find("type");
        if (typeVal == nullptr || !typeVal->IsString())
        {
            continue;
        }
        if (const ContentSchema::TypeSchema* schema = ContentSchema::FindTopLevel(typeVal->String))
        {
            CollectRecordsFromObject(*schema, root, records);
        }
        else
        {
            const JsonValue* idVal = root.Find("id");
            if (idVal != nullptr && idVal->IsString() && !idVal->String.empty())
            {
                records.push_back({idVal->String, typeVal->String});
            }
        }
    }

    ContentManifest manifest;
    manifest.Files.reserve(byFile.size());
    for (auto& [path, records] : byFile)
    {
        std::sort(records.begin(), records.end(), RecordLess);
        manifest.Files.push_back({path, std::move(records)});
    }
    return manifest;
}

std::vector<ContentManifestDuplicate> FindContentManifestDuplicates(const ContentManifest& manifest)
{
    std::vector<ContentManifestFile> sortedFiles = manifest.Files;
    for (ContentManifestFile& file : sortedFiles)
    {
        file.Path = NormalizeContentManifestPath(file.Path);
        std::sort(file.Records.begin(), file.Records.end(), RecordLess);
    }
    std::sort(sortedFiles.begin(), sortedFiles.end(), FileLess);

    std::vector<ContentManifestDuplicate> duplicates;
    std::map<std::string, std::pair<std::string, std::string>> seenById;
    std::string previousPath;

    for (const ContentManifestFile& file : sortedFiles)
    {
        if (!previousPath.empty() && file.Path == previousPath && file.Records.empty())
        {
            duplicates.push_back({"", "file", previousPath, file.Path});
        }
        previousPath = file.Path;

        for (const ContentManifestRecord& record : file.Records)
        {
            const auto [it, inserted] = seenById.emplace(record.Id, std::pair{file.Path, record.Kind});
            if (!inserted)
            {
                duplicates.push_back({record.Id, record.Kind, it->second.first, file.Path});
            }
        }
    }

    std::sort(duplicates.begin(), duplicates.end(), [](const ContentManifestDuplicate& a, const ContentManifestDuplicate& b) {
        return std::tie(a.Id, a.FirstFile, a.SecondFile, a.Kind) < std::tie(b.Id, b.FirstFile, b.SecondFile, b.Kind);
    });
    return duplicates;
}

ContentManifestDiff DiffContentManifests(const ContentManifest& expected, const ContentManifest& actual)
{
    ContentManifestDiff diff;
    diff.Duplicates = FindContentManifestDuplicates(actual);
    for (const ContentManifestDuplicate& expDup : FindContentManifestDuplicates(expected))
    {
        if (std::find(diff.Duplicates.begin(), diff.Duplicates.end(), expDup) == diff.Duplicates.end())
        {
            diff.Duplicates.push_back(expDup);
        }
    }
    std::sort(diff.Duplicates.begin(), diff.Duplicates.end(), [](const ContentManifestDuplicate& a, const ContentManifestDuplicate& b) {
        return std::tie(a.Id, a.FirstFile, a.SecondFile, a.Kind) < std::tie(b.Id, b.FirstFile, b.SecondFile, b.Kind);
    });

    auto buildRecordMap = [](const ContentManifest& manifest) {
        std::vector<ContentManifestFile> files = manifest.Files;
        for (ContentManifestFile& file : files)
        {
            file.Path = NormalizeContentManifestPath(file.Path);
            std::sort(file.Records.begin(), file.Records.end(), RecordLess);
        }
        std::sort(files.begin(), files.end(), FileLess);

        std::map<std::string, ContentManifestEntryRef> byId;
        for (const ContentManifestFile& file : files)
        {
            for (const ContentManifestRecord& record : file.Records)
            {
                byId.emplace(record.Id, ContentManifestEntryRef{record.Id, record.Kind, file.Path});
            }
        }
        return byId;
    };

    const std::map<std::string, ContentManifestEntryRef> expectedById = buildRecordMap(expected);
    const std::map<std::string, ContentManifestEntryRef> actualById = buildRecordMap(actual);

    for (const auto& [id, expRef] : expectedById)
    {
        const auto actIt = actualById.find(id);
        if (actIt == actualById.end())
        {
            diff.Removed.push_back(expRef);
        }
        else if (expRef.File != actIt->second.File || expRef.Kind != actIt->second.Kind)
        {
            diff.Renamed.push_back({id, expRef.Kind, actIt->second.Kind, expRef.File, actIt->second.File});
        }
    }

    for (const auto& [id, actRef] : actualById)
    {
        if (expectedById.find(id) == expectedById.end())
        {
            diff.Added.push_back(actRef);
        }
    }

    for (const ContentManifestFile& expFile : expected.Files)
    {
        if (expFile.Records.empty() && actual.FindFile(expFile.Path) == nullptr)
        {
            diff.Removed.push_back({"", "file", NormalizeContentManifestPath(expFile.Path)});
        }
    }
    for (const ContentManifestFile& actFile : actual.Files)
    {
        if (actFile.Records.empty() && expected.FindFile(actFile.Path) == nullptr)
        {
            diff.Added.push_back({"", "file", NormalizeContentManifestPath(actFile.Path)});
        }
    }

    const auto entryLess = [](const ContentManifestEntryRef& a, const ContentManifestEntryRef& b) {
        return std::tie(a.Id, a.File, a.Kind) < std::tie(b.Id, b.File, b.Kind);
    };
    std::sort(diff.Added.begin(), diff.Added.end(), entryLess);
    std::sort(diff.Removed.begin(), diff.Removed.end(), entryLess);
    std::sort(diff.Renamed.begin(), diff.Renamed.end(), [](const ContentManifestRename& a, const ContentManifestRename& b) {
        return std::tie(a.Id, a.FromFile, a.ToFile, a.FromKind, a.ToKind) <
               std::tie(b.Id, b.FromFile, b.ToFile, b.FromKind, b.ToKind);
    });
    return diff;
}

std::string FormatContentManifestDiff(const ContentManifestDiff& diff)
{
    std::ostringstream out;
    for (const ContentManifestEntryRef& item : diff.Added)
    {
        out << "added: " << item.File << ": " << item.Id << " (" << item.Kind << ")\n";
    }
    for (const ContentManifestEntryRef& item : diff.Removed)
    {
        out << "removed: " << item.File << ": " << item.Id << " (" << item.Kind << ")\n";
    }
    for (const ContentManifestRename& item : diff.Renamed)
    {
        out << "renamed: " << item.FromFile << " -> " << item.ToFile << ": " << item.Id << " (";
        if (item.FromKind == item.ToKind)
        {
            out << item.ToKind;
        }
        else
        {
            out << item.FromKind << " -> " << item.ToKind;
        }
        out << ")\n";
    }
    for (const ContentManifestDuplicate& item : diff.Duplicates)
    {
        out << "duplicate: " << item.SecondFile << ": " << item.Id << " (" << item.Kind << ") (first in " << item.FirstFile << ")\n";
    }
    return out.str();
}

std::string FormatContentManifestJson(const ContentManifest& manifest)
{
    std::vector<ContentManifestFile> sorted = manifest.Files;
    for (ContentManifestFile& file : sorted)
    {
        file.Path = NormalizeContentManifestPath(file.Path);
        std::sort(file.Records.begin(), file.Records.end(), RecordLess);
    }
    std::sort(sorted.begin(), sorted.end(), FileLess);

    std::size_t recordCount = 0;
    for (const ContentManifestFile& file : sorted)
    {
        recordCount += file.Records.size();
    }

    std::ostringstream out;
    out << "{\n";
    out << "  \"schema\": ";
    EscapeJsonString(out, ContentManifestSchema);
    out << ",\n";
    out << "  \"fileCount\": " << sorted.size() << ",\n";
    out << "  \"recordCount\": " << recordCount << ",\n";
    out << "  \"files\": [\n";
    for (std::size_t i = 0; i < sorted.size(); ++i)
    {
        const ContentManifestFile& file = sorted[i];
        out << "    {\n";
        out << "      \"path\": ";
        EscapeJsonString(out, file.Path);
        out << ",\n";
        out << "      \"records\": [";
        if (!file.Records.empty())
        {
            out << '\n';
            for (std::size_t r = 0; r < file.Records.size(); ++r)
            {
                const ContentManifestRecord& record = file.Records[r];
                out << "        { \"id\": ";
                EscapeJsonString(out, record.Id);
                out << ", \"kind\": ";
                EscapeJsonString(out, record.Kind);
                out << " }" << (r + 1 < file.Records.size() ? "," : "") << '\n';
            }
            out << "      ";
        }
        out << "]\n";
        out << "    }" << (i + 1 < sorted.size() ? "," : "") << '\n';
    }
    out << "  ]\n";
    out << "}\n";
    return out.str();
}

ContentManifestParseResult ParseContentManifestJson(std::string_view text)
{
    ContentManifestParseResult result;
    const std::string normalizedText = NormalizeBomText(text);
    const JsonParseResult parsed = ParseJson(normalizedText);
    if (!parsed.Value || !parsed.Value->IsObject())
    {
        result.Error = parsed.Value ? "manifest root must be a JSON object"
                                    : "invalid JSON at line " + std::to_string(parsed.Line) + ": " + parsed.Error;
        return result;
    }

    const JsonValue& root = *parsed.Value;
    for (const auto& [key, value] : root.Object)
    {
        if (key != "schema" && key != "fileCount" && key != "recordCount" && key != "files")
        {
            result.Error = "unknown field \"" + key + "\" in manifest";
            return result;
        }
    }

    const JsonValue* schema = root.Find("schema");
    if (schema == nullptr || !schema->IsString() || schema->String != ContentManifestSchema)
    {
        result.Error = "manifest requires \"schema\": \"" + std::string(ContentManifestSchema) + "\"";
        return result;
    }

    const JsonValue* filesVal = root.Find("files");
    if (filesVal == nullptr || !filesVal->IsArray())
    {
        result.Error = "manifest requires a \"files\" array";
        return result;
    }

    ContentManifest manifest;
    manifest.Files.reserve(filesVal->Array.size());
    std::string previousPath;

    for (const JsonValue& fileItem : filesVal->Array)
    {
        if (!fileItem.IsObject())
        {
            result.Error = "each entry in \"files\" must be an object";
            return result;
        }
        for (const auto& [key, value] : fileItem.Object)
        {
            if (key != "path" && key != "records")
            {
                result.Error = "unknown field \"" + key + "\" in manifest file entry";
                return result;
            }
        }
        const JsonValue* pathVal = fileItem.Find("path");
        const JsonValue* recordsVal = fileItem.Find("records");
        if (pathVal == nullptr || !pathVal->IsString() || pathVal->String.empty())
        {
            result.Error = "manifest file entry requires a non-empty string \"path\"";
            return result;
        }
        if (recordsVal == nullptr || !recordsVal->IsArray())
        {
            result.Error = "manifest file entry \"" + pathVal->String + "\" requires a \"records\" array";
            return result;
        }

        ContentManifestFile entry;
        entry.Path = NormalizeContentManifestPath(pathVal->String);
        if (!previousPath.empty() && entry.Path < previousPath)
        {
            result.Error = "manifest files are not sorted at \"" + entry.Path + "\"";
            return result;
        }
        previousPath = entry.Path;

        for (const JsonValue& recItem : recordsVal->Array)
        {
            if (!recItem.IsObject())
            {
                result.Error = "each entry in \"records\" for \"" + entry.Path + "\" must be an object";
                return result;
            }
            for (const auto& [key, value] : recItem.Object)
            {
                if (key != "id" && key != "kind")
                {
                    result.Error = "unknown field \"" + key + "\" in manifest record entry";
                    return result;
                }
            }
            const JsonValue* idVal = recItem.Find("id");
            const JsonValue* kindVal = recItem.Find("kind");
            if (idVal == nullptr || !idVal->IsString() || idVal->String.empty() ||
                kindVal == nullptr || !kindVal->IsString() || kindVal->String.empty())
            {
                result.Error = "manifest record in \"" + entry.Path + "\" requires non-empty string \"id\" and \"kind\"";
                return result;
            }
            ContentManifestRecord record{idVal->String, kindVal->String};
            if (!entry.Records.empty() && RecordLess(record, entry.Records.back()))
            {
                result.Error = "manifest records are not sorted in \"" + entry.Path + "\" at \"" + record.Id + "\"";
                return result;
            }
            entry.Records.push_back(std::move(record));
        }

        manifest.Files.push_back(std::move(entry));
    }

    if (const JsonValue* fileCountVal = root.Find("fileCount"))
    {
        if (!fileCountVal->Integer || *fileCountVal->Integer < 0 ||
            static_cast<std::size_t>(*fileCountVal->Integer) != manifest.FileCount())
        {
            result.Error = "\"fileCount\" does not match the number of files in \"files\"";
            return result;
        }
    }
    if (const JsonValue* recordCountVal = root.Find("recordCount"))
    {
        if (!recordCountVal->Integer || *recordCountVal->Integer < 0 ||
            static_cast<std::size_t>(*recordCountVal->Integer) != manifest.RecordCount())
        {
            result.Error = "\"recordCount\" does not match the number of records in \"files\"";
            return result;
        }
    }

    result.Manifest = std::move(manifest);
    return result;
}
} // namespace PerfectWorld
