#pragma once

#include <functional>
#include <string>
#include <string_view>

#ifndef PERFECTWORLDCORE_API
#define PERFECTWORLDCORE_API
#endif

namespace PerfectWorld
{
/**
 * The portable slot store (ROADMAP M1-04): how one save slot reaches the disk
 * and comes back, with no Unreal types so CTest can attack it in a temp folder.
 *
 * A slot is one text file, its "<file>.bak" (the version before the latest
 * save) and, only while a save is in flight, "<file>.tmp". The whole job is to
 * make sure a crash, a full disk or a bad write can never leave the player
 * with no readable record when one existed a moment earlier:
 *
 *   write: temp file -> read it back -> byte-compare -> validate -> rename the
 *          old primary to .bak -> rename the temp over the primary.
 *   read:  the primary if it validates, else the .bak, and say which.
 *
 * Paths are UTF-8 strings (what the Unreal side already produces), converted
 * to native paths inside, so a non-ASCII folder name survives on Windows.
 * Every filesystem call uses the error_code form because the sources also
 * compile as an Unreal module with exceptions off.
 *
 * Messages are plain sentences for the player: they never contain a path.
 */

/**
 * Decides whether a slot's text is a usable record (the caller parses it).
 * Returns true to accept; on false, writes a short reason into `error`. An
 * empty validator accepts any text, which suits tests of the file mechanics.
 */
using SaveValidator = std::function<bool(std::string_view text, std::string& error)>;

/** "<primary>.bak": the previous version, appended to the whole file name. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SaveBackupPath(const std::string& primary);
/** "<primary>.tmp": the in-flight write. Never read back as a save. */
[[nodiscard]] PERFECTWORLDCORE_API std::string SaveTempPath(const std::string& primary);

struct SaveOperationResult
{
    bool Ok = false;
    std::string Message;

    explicit operator bool() const { return Ok; }
};

enum class SaveReadStatus
{
    Loaded,              // the primary file validated
    RecoveredFromBackup, // the primary was missing or unusable; the .bak validated
    Missing,             // no primary file and no usable backup: an empty slot
    Corrupt              // a primary file exists but nothing on disk validates
};

struct SaveReadResult
{
    SaveReadStatus Status = SaveReadStatus::Missing;
    /** The validated text; empty unless Succeeded(). */
    std::string Text;
    /** Plain-sentence note for the player; empty for a plain Loaded. */
    std::string Message;

    /** True when Text holds a validated record, whether primary or backup. */
    [[nodiscard]] bool Succeeded() const
    {
        return Status == SaveReadStatus::Loaded || Status == SaveReadStatus::RecoveredFromBackup;
    }
    explicit operator bool() const { return Succeeded(); }
};

/**
 * Writes `text` as the slot's new primary. Any failure before the first
 * rename (folder, write, read-back, compare, validator) leaves the primary and
 * .bak exactly as they were and removes the temp file. Missing parent folders
 * are created.
 */
[[nodiscard]] PERFECTWORLDCORE_API SaveOperationResult WriteSaveFile(
    const std::string& primary, std::string_view text, const SaveValidator& validator = {});

/** Never reads the .tmp file: a stale one is a failed write, not a save. */
[[nodiscard]] PERFECTWORLDCORE_API SaveReadResult ReadSaveFile(const std::string& primary, const SaveValidator& validator = {});

/**
 * Removes the slot: temp, then .bak, then primary. If the .bak cannot be
 * removed the primary is left alone, so a half-deleted slot still reads back.
 * Deleting a slot that does not exist succeeds.
 */
[[nodiscard]] PERFECTWORLDCORE_API SaveOperationResult DeleteSaveFiles(const std::string& primary);
} // namespace PerfectWorld
