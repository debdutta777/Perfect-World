#include "SaveStore.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

namespace PerfectWorld
{
namespace
{
namespace fs = std::filesystem;

// The strings we are given are UTF-8; fs::path wants the native encoding. Going
// through u8string keeps a folder such as "Savés" intact on Windows, where
// a plain char constructor would reinterpret it in the ANSI code page.
fs::path SaveStoreToPath(const std::string& utf8)
{
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

enum class SaveStoreFileState
{
    Absent,     // nothing is there
    Unreadable, // something is there (a folder, a locked file) but it cannot be read
    Read        // contents loaded
};

SaveStoreFileState SaveStoreReadWhole(const fs::path& path, std::string& contents)
{
    contents.clear();
    std::error_code ec;
    const fs::file_status status = fs::status(path, ec);
    if (ec || !fs::exists(status))
    {
        // A failed status call and a missing file both mean there is nothing
        // we can open; "no file" is the honest reading for the slot.
        return SaveStoreFileState::Absent;
    }
    if (!fs::is_regular_file(status))
    {
        return SaveStoreFileState::Unreadable;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return SaveStoreFileState::Unreadable;
    }
    contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    if (in.bad())
    {
        contents.clear();
        return SaveStoreFileState::Unreadable;
    }
    return SaveStoreFileState::Read;
}

bool SaveStoreAccepts(const SaveValidator& validator, std::string_view text, std::string& error)
{
    error.clear();
    return !validator || validator(text, error);
}

void SaveStoreRemoveQuietly(const fs::path& path)
{
    std::error_code ec;
    fs::remove(path, ec);
}

SaveOperationResult SaveStoreFail(std::string message)
{
    SaveOperationResult result;
    result.Ok = false;
    result.Message = std::move(message);
    return result;
}

bool SaveStoreExists(const fs::path& path)
{
    std::error_code ec;
    return fs::exists(fs::symlink_status(path, ec)) && !ec;
}
} // namespace

std::string SaveBackupPath(const std::string& primary)
{
    return primary + ".bak";
}

std::string SaveTempPath(const std::string& primary)
{
    return primary + ".tmp";
}

SaveOperationResult WriteSaveFile(const std::string& primary, std::string_view text, const SaveValidator& validator)
{
    if (primary.empty())
    {
        return SaveStoreFail("This save slot has no file name.");
    }
    const fs::path primaryPath = SaveStoreToPath(primary);
    const fs::path backupPath = SaveStoreToPath(SaveBackupPath(primary));
    const fs::path tempPath = SaveStoreToPath(SaveTempPath(primary));

    std::error_code ec;
    if (primaryPath.has_parent_path())
    {
        fs::create_directories(primaryPath.parent_path(), ec);
        if (ec)
        {
            return SaveStoreFail("The save folder could not be created.");
        }
    }

    // Stage 1: write the new text beside the slot. Binary mode, so the bytes on
    // disk are exactly the bytes given, with no newline translation. A stale
    // temp from an earlier crash is simply overwritten.
    {
        std::ofstream out(tempPath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            SaveStoreRemoveQuietly(tempPath);
            return SaveStoreFail("This save slot could not be written to disk. The earlier save is unchanged.");
        }
        out.write(text.data(), static_cast<std::streamsize>(text.size()));
        out.flush();
        if (!out)
        {
            out.close();
            SaveStoreRemoveQuietly(tempPath);
            return SaveStoreFail("This save slot could not be written to disk. The earlier save is unchanged.");
        }
    }

    // Stage 2: parse-back. Read the temp file again and insist it is what we
    // meant to write and that the caller's validator accepts it. A full disk or
    // a failing drive shows up here, before anything good has been touched.
    std::string readBack;
    if (SaveStoreReadWhole(tempPath, readBack) != SaveStoreFileState::Read || readBack != text)
    {
        SaveStoreRemoveQuietly(tempPath);
        return SaveStoreFail("The new save did not match what was written, so it was discarded. The earlier save is unchanged.");
    }
    std::string reason;
    if (!SaveStoreAccepts(validator, readBack, reason))
    {
        SaveStoreRemoveQuietly(tempPath);
        return SaveStoreFail("The new save did not pass its check and was not stored. The earlier save is unchanged." +
                             (reason.empty() ? std::string() : " " + reason));
    }

    // Stage 3: two renames. The first keeps the previous primary as the one
    // .bak; the second puts the new file in place. Between them the primary is
    // absent, but the .bak holds the old version and the temp holds the new, and
    // ReadSaveFile falls back to the .bak, so a crash here loses nothing.
    const bool hadPrimary = SaveStoreExists(primaryPath);
    if (hadPrimary)
    {
        fs::rename(primaryPath, backupPath, ec);
        if (ec)
        {
            SaveStoreRemoveQuietly(tempPath);
            return SaveStoreFail("The earlier save could not be set aside, so the new save was not stored. The earlier save is unchanged.");
        }
    }
    fs::rename(tempPath, primaryPath, ec);
    if (ec)
    {
        SaveStoreRemoveQuietly(tempPath);
        if (!hadPrimary)
        {
            return SaveStoreFail("The new save could not be put in place.");
        }
        // Best effort: copy (not move) the old version back, so the .bak stays
        // as a second copy whether or not the restore works.
        std::error_code restoreEc;
        fs::copy_file(backupPath, primaryPath, fs::copy_options::overwrite_existing, restoreEc);
        if (restoreEc)
        {
            return SaveStoreFail("The new save could not be put in place, and the earlier save could not be restored automatically. "
                                 "A backup copy of the earlier save is kept.");
        }
        return SaveStoreFail("The new save could not be put in place. The earlier save was restored.");
    }

    SaveOperationResult result;
    result.Ok = true;
    return result;
}

SaveReadResult ReadSaveFile(const std::string& primary, const SaveValidator& validator)
{
    SaveReadResult result;
    if (primary.empty())
    {
        result.Status = SaveReadStatus::Missing;
        result.Message = "This save slot has no file name.";
        return result;
    }

    std::string text;
    std::string reason;
    const SaveStoreFileState primaryState = SaveStoreReadWhole(SaveStoreToPath(primary), text);
    if (primaryState == SaveStoreFileState::Read && SaveStoreAccepts(validator, text, reason))
    {
        result.Status = SaveReadStatus::Loaded;
        result.Text = std::move(text);
        return result;
    }

    // The primary is absent, unreadable or invalid. The .bak is the previous
    // good save, and also what is left if a crash fell between the two renames.
    std::string backupText;
    std::string backupReason;
    if (SaveStoreReadWhole(SaveStoreToPath(SaveBackupPath(primary)), backupText) == SaveStoreFileState::Read &&
        SaveStoreAccepts(validator, backupText, backupReason))
    {
        result.Status = SaveReadStatus::RecoveredFromBackup;
        result.Text = std::move(backupText);
        result.Message = primaryState == SaveStoreFileState::Absent
                             ? "The latest save was missing, so the previous save was loaded."
                             : "The latest save was damaged, so the previous save was loaded.";
        return result;
    }

    if (primaryState == SaveStoreFileState::Absent)
    {
        result.Status = SaveReadStatus::Missing;
        result.Message = "This save slot is empty.";
        return result;
    }
    // A zero-byte or unreadable primary lands here too: a file is present, so
    // this is damage, not an empty slot.
    result.Status = SaveReadStatus::Corrupt;
    result.Message = "This save is damaged and no earlier save could be loaded." +
                     (reason.empty() ? std::string() : " " + reason);
    return result;
}

SaveOperationResult DeleteSaveFiles(const std::string& primary)
{
    if (primary.empty())
    {
        return SaveStoreFail("This save slot has no file name.");
    }
    std::error_code ec;
    // fs::remove reports "nothing to remove" as false with no error, so a slot
    // that is already gone counts as deleted.
    fs::remove(SaveStoreToPath(SaveTempPath(primary)), ec);
    if (ec)
    {
        return SaveStoreFail("A leftover file from an unfinished save could not be removed. The save slot is unchanged.");
    }
    fs::remove(SaveStoreToPath(SaveBackupPath(primary)), ec);
    if (ec)
    {
        // Stop before the primary: deleting it while the older copy survives
        // would let the backup resurrect a slot the player just deleted.
        return SaveStoreFail("The earlier save could not be removed, so the save slot was kept.");
    }
    fs::remove(SaveStoreToPath(primary), ec);
    if (ec)
    {
        return SaveStoreFail("This save slot could not be removed.");
    }
    SaveOperationResult result;
    result.Ok = true;
    return result;
}
} // namespace PerfectWorld
