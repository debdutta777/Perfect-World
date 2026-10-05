#include "SaveStore.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>

using namespace PerfectWorld;
namespace fs = std::filesystem;

namespace
{
fs::path SaveStoreTestRoot;

void SaveStoreTestCleanup()
{
    if (!SaveStoreTestRoot.empty())
    {
        std::error_code ec;
        fs::remove_all(SaveStoreTestRoot, ec);
    }
}
} // namespace

int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        SaveStoreTestCleanup();
        std::exit(1);
    }
}

namespace
{
// The store takes UTF-8 strings, so tests convert native paths the same way
// the Unreal side would hand them over.
std::string Utf8(const fs::path& path)
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

fs::path NativePath(const std::string& utf8)
{
    return fs::path(std::u8string(utf8.begin(), utf8.end()));
}

// Test files are written and read raw, bypassing the store, so each case can
// build exactly the disk state it needs (a crash mid-save, a damaged file).
void PutFile(const std::string& path, const std::string& text)
{
    std::ofstream out(NativePath(path), std::ios::binary | std::ios::trunc);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    Check(static_cast<bool>(out), "test file written");
}

std::string GetFile(const std::string& path)
{
    std::ifstream in(NativePath(path), std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool Exists(const std::string& path)
{
    std::error_code ec;
    return fs::exists(NativePath(path), ec);
}

// A stand-in for the real parser: a record is valid when it starts "SAVE:".
bool LooksLikeSave(std::string_view text, std::string& error)
{
    if (text.substr(0, 5) == "SAVE:")
    {
        return true;
    }
    error = "That is not a save record.";
    return false;
}

const SaveValidator Validator = LooksLikeSave;

// Each case works in its own folder so no state leaks between them.
std::string FreshSlot(const char* caseName, const char* fileName = "slot.json")
{
    const fs::path folder = SaveStoreTestRoot / caseName;
    std::error_code ec;
    fs::remove_all(folder, ec);
    fs::create_directories(folder, ec);
    Check(!ec, "case folder created");
    return Utf8(folder / fileName);
}

bool ContainsPath(const std::string& message, const std::string& primary)
{
    return message.find(Utf8(NativePath(primary).parent_path())) != std::string::npos ||
           message.find(".json") != std::string::npos || message.find(".bak") != std::string::npos ||
           message.find(".tmp") != std::string::npos;
}
} // namespace

int main()
{
    const long long stamp = std::chrono::system_clock::now().time_since_epoch().count();
    SaveStoreTestRoot = fs::temp_directory_path() / ("pw_savestore_tests_" + std::to_string(stamp));
    {
        std::error_code ec;
        fs::create_directories(SaveStoreTestRoot, ec);
        Check(!ec, "test root created");
    }

    // File naming: appended to the whole name, not swapped for the extension.
    Check(SaveBackupPath("a/creation_slot_1.json") == "a/creation_slot_1.json.bak", "backup path appends .bak");
    Check(SaveTempPath("a/creation_slot_1.json") == "a/creation_slot_1.json.tmp", "temp path appends .tmp");

    // An empty slot reads as Missing, not as an error.
    {
        const std::string slot = FreshSlot("missing");
        const SaveReadResult read = ReadSaveFile(slot, Validator);
        Check(read.Status == SaveReadStatus::Missing, "no files reads as Missing");
        Check(!read.Succeeded() && !static_cast<bool>(read), "Missing is not a success");
        Check(read.Text.empty(), "Missing has no text");
        Check(!read.Message.empty(), "Missing explains itself");
        Check(!ContainsPath(read.Message, slot), "Missing message has no path");
        Check(!ReadSaveFile(slot).Succeeded(), "Missing with no validator is still Missing");
    }

    // First save, second save, and the single .bak.
    {
        const std::string slot = FreshSlot("basic");
        const SaveOperationResult first = WriteSaveFile(slot, "SAVE:one", Validator);
        Check(first.Ok && static_cast<bool>(first), "first write succeeds");
        Check(GetFile(slot) == "SAVE:one", "first write is on disk");
        Check(!Exists(SaveBackupPath(slot)), "first write makes no backup");
        Check(!Exists(SaveTempPath(slot)), "first write leaves no temp");

        const SaveReadResult read1 = ReadSaveFile(slot, Validator);
        Check(read1.Status == SaveReadStatus::Loaded && read1.Text == "SAVE:one", "first save loads");
        Check(read1.Message.empty(), "a plain load has no message");
        Check(read1.Succeeded() && static_cast<bool>(read1), "Loaded is a success");

        Check(WriteSaveFile(slot, "SAVE:two", Validator).Ok, "second write succeeds");
        Check(GetFile(slot) == "SAVE:two", "primary holds the new version");
        Check(GetFile(SaveBackupPath(slot)) == "SAVE:one", "backup holds the previous version");
        Check(!Exists(SaveTempPath(slot)), "second write leaves no temp");

        Check(WriteSaveFile(slot, "SAVE:three", Validator).Ok, "third write succeeds");
        Check(GetFile(SaveBackupPath(slot)) == "SAVE:two", "only the one previous version is kept");
        Check(ReadSaveFile(slot, Validator).Text == "SAVE:three", "latest save loads");
    }

    // A stale temp (an earlier crash mid-write) is never a save.
    {
        const std::string slot = FreshSlot("staletemp");
        Check(WriteSaveFile(slot, "SAVE:good", Validator).Ok, "good slot written");
        PutFile(SaveTempPath(slot), "SAVE:stale half-finished");
        const SaveReadResult read = ReadSaveFile(slot, Validator);
        Check(read.Status == SaveReadStatus::Loaded && read.Text == "SAVE:good", "stale temp beside a good slot is ignored");

        Check(WriteSaveFile(slot, "SAVE:next", Validator).Ok, "write over a stale temp succeeds");
        Check(!Exists(SaveTempPath(slot)), "the stale temp is gone after a write");
        Check(GetFile(slot) == "SAVE:next" && GetFile(SaveBackupPath(slot)) == "SAVE:good", "stale temp never became the backup");

        const std::string lonely = FreshSlot("staletemp_only");
        PutFile(SaveTempPath(lonely), "SAVE:orphan");
        Check(ReadSaveFile(lonely, Validator).Status == SaveReadStatus::Missing, "a lone temp is not a save: slot is Missing");
        Check(ReadSaveFile(lonely).Status == SaveReadStatus::Missing, "a lone temp is Missing even with no validator");
    }

    // A corrupt primary falls back to the .bak, with a message.
    {
        const std::string slot = FreshSlot("fallback");
        Check(WriteSaveFile(slot, "SAVE:old", Validator).Ok && WriteSaveFile(slot, "SAVE:new", Validator).Ok, "two versions written");
        PutFile(slot, "this is not a save");
        const SaveReadResult read = ReadSaveFile(slot, Validator);
        Check(read.Status == SaveReadStatus::RecoveredFromBackup, "corrupt primary recovers from backup");
        Check(read.Text == "SAVE:old", "recovered text is the backup");
        Check(read.Succeeded(), "recovery is a success");
        Check(!read.Message.empty(), "recovery carries a message");
        Check(!ContainsPath(read.Message, slot), "recovery message has no path");

        // A primary that is gone behaves the same way, with its own wording.
        std::error_code ec;
        fs::remove(NativePath(slot), ec);
        const SaveReadResult gone = ReadSaveFile(slot, Validator);
        Check(gone.Status == SaveReadStatus::RecoveredFromBackup && gone.Text == "SAVE:old", "absent primary recovers from backup");
        Check(!gone.Message.empty() && gone.Message != read.Message, "absent and damaged are worded differently");
    }

    // Missing and corrupt are told apart.
    {
        const std::string both = FreshSlot("bothbad");
        PutFile(both, "garbage");
        PutFile(SaveBackupPath(both), "more garbage");
        const SaveReadResult read = ReadSaveFile(both, Validator);
        Check(read.Status == SaveReadStatus::Corrupt, "corrupt primary and corrupt backup is Corrupt");
        Check(!read.Succeeded() && read.Text.empty(), "Corrupt has no text");
        Check(!read.Message.empty() && !ContainsPath(read.Message, both), "Corrupt message is a plain sentence");

        const std::string noBackup = FreshSlot("corrupt_nobak");
        PutFile(noBackup, "garbage");
        Check(ReadSaveFile(noBackup, Validator).Status == SaveReadStatus::Corrupt, "corrupt primary with no backup is Corrupt");

        const std::string badBackupOnly = FreshSlot("missing_badbak");
        PutFile(SaveBackupPath(badBackupOnly), "garbage");
        Check(ReadSaveFile(badBackupOnly, Validator).Status == SaveReadStatus::Missing,
              "no primary and no valid backup is Missing, not Corrupt");

        Check(ReadSaveFile(Utf8(SaveStoreTestRoot / "never_made" / "x.json"), Validator).Status == SaveReadStatus::Missing,
              "a slot in a folder that does not exist is Missing");
    }

    // An empty primary is a file that exists, so it is damage, not an empty slot.
    {
        const std::string slot = FreshSlot("emptyprimary");
        PutFile(slot, "");
        Check(ReadSaveFile(slot, Validator).Status == SaveReadStatus::Corrupt, "empty primary is Corrupt");
        const SaveReadResult lenient = ReadSaveFile(slot);
        Check(lenient.Status == SaveReadStatus::Loaded && lenient.Text.empty(), "empty validator accepts an empty file");

        PutFile(SaveBackupPath(slot), "SAVE:kept");
        const SaveReadResult recovered = ReadSaveFile(slot, Validator);
        Check(recovered.Status == SaveReadStatus::RecoveredFromBackup && recovered.Text == "SAVE:kept",
              "empty primary recovers from a good backup");

        // Writing an empty string with no validator is allowed and reads back.
        const std::string blank = FreshSlot("emptywrite");
        Check(WriteSaveFile(blank, "").Ok, "empty text can be written when the validator allows it");
        Check(Exists(blank) && ReadSaveFile(blank).Status == SaveReadStatus::Loaded, "empty text reads back");
    }

    // A validator that rejects the new text stops the write cold.
    {
        const std::string slot = FreshSlot("rejected");
        Check(WriteSaveFile(slot, "SAVE:one", Validator).Ok && WriteSaveFile(slot, "SAVE:two", Validator).Ok, "two versions written");
        const SaveOperationResult bad = WriteSaveFile(slot, "oops, not valid", Validator);
        Check(!bad.Ok && !static_cast<bool>(bad), "validator rejection fails the write");
        Check(!bad.Message.empty() && !ContainsPath(bad.Message, slot), "rejection says why without a path");
        Check(bad.Message.find("not a save record") != std::string::npos, "the validator's reason reaches the message");
        Check(GetFile(slot) == "SAVE:two", "primary untouched after rejection");
        Check(GetFile(SaveBackupPath(slot)) == "SAVE:one", "backup untouched after rejection");
        Check(!Exists(SaveTempPath(slot)), "temp removed after rejection");

        // The same on a slot with no earlier file at all.
        const std::string fresh = FreshSlot("rejected_fresh");
        Check(!WriteSaveFile(fresh, "nope", Validator).Ok, "rejection on an empty slot fails");
        Check(!Exists(fresh) && !Exists(SaveTempPath(fresh)) && !Exists(SaveBackupPath(fresh)), "an empty slot stays empty after rejection");
    }

    // The crash between the two renames, built on disk: primary absent, .bak
    // holds the old save, .tmp holds the new one that never got its name.
    {
        const std::string slot = FreshSlot("crash");
        PutFile(SaveBackupPath(slot), "SAVE:old");
        PutFile(SaveTempPath(slot), "SAVE:new");
        const SaveReadResult read = ReadSaveFile(slot, Validator);
        Check(read.Status == SaveReadStatus::RecoveredFromBackup, "crash between renames recovers");
        Check(read.Text == "SAVE:old", "crash recovery returns the old version, not the unfinished temp");
        Check(!read.Message.empty(), "crash recovery carries a message");

        Check(WriteSaveFile(slot, "SAVE:after", Validator).Ok, "the next save after a crash succeeds");
        Check(GetFile(slot) == "SAVE:after", "primary is restored by the next save");
        Check(!Exists(SaveTempPath(slot)), "the orphaned temp is cleared by the next save");
        Check(ReadSaveFile(slot, Validator).Status == SaveReadStatus::Loaded, "slot reads normally again");
    }

    // Failures that happen before any rename must not disturb the slot: here
    // the parent "folder" is really a file, so the folder cannot be created.
    {
        const std::string blocker = FreshSlot("blocked", "iamafile");
        PutFile(blocker, "SAVE:i am a file");
        const std::string under = blocker + "/slot.json";
        const SaveOperationResult result = WriteSaveFile(under, "SAVE:x", Validator);
        Check(!result.Ok && !result.Message.empty(), "a write beneath a file fails with a message");
        Check(!ContainsPath(result.Message, under), "that message has no path either");
        Check(GetFile(blocker) == "SAVE:i am a file", "the blocking file is untouched");
        Check(!WriteSaveFile("", "SAVE:x", Validator).Ok, "an empty path is refused");
        Check(ReadSaveFile("", Validator).Status == SaveReadStatus::Missing, "an empty path reads as Missing");
        Check(!DeleteSaveFiles("").Ok, "an empty path cannot be deleted");
    }

    // Missing parent folders are created, and bytes are stored exactly.
    {
        const std::string nested = Utf8(SaveStoreTestRoot / "deep" / "er" / "still" / "slot.json");
        const std::string exact = std::string("SAVE:line1\r\nline2\n\0nul\xE2\x9C\x93\xFF end", 31);
        Check(exact.size() == 31, "binary test string has the intended length");
        Check(WriteSaveFile(nested, exact, Validator).Ok, "write creates missing parent folders");
        Check(GetFile(nested) == exact, "bytes (CRLF, NUL, high bytes) are stored exactly");
        const SaveReadResult read = ReadSaveFile(nested, Validator);
        Check(read.Status == SaveReadStatus::Loaded && read.Text == exact, "bytes read back exactly");
        Check(WriteSaveFile(nested, exact + "2", Validator).Ok && GetFile(SaveBackupPath(nested)) == exact, "binary text survives rotation into the backup");
    }

    // A non-ASCII folder name (the player's own account folder can be one).
    {
        const fs::path folder = SaveStoreTestRoot / fs::path(u8"São Paulo ☃ কলকাতা");
        const std::string slot = Utf8(folder / fs::path(u8"creation_slot_é.json"));
        Check(WriteSaveFile(slot, "SAVE:unicode one", Validator).Ok, "write into a non-ASCII folder succeeds");
        Check(WriteSaveFile(slot, "SAVE:unicode two", Validator).Ok, "second write into a non-ASCII folder succeeds");
        Check(Exists(slot) && Exists(SaveBackupPath(slot)) && !Exists(SaveTempPath(slot)), "non-ASCII slot has primary and backup, no temp");
        Check(ReadSaveFile(slot, Validator).Text == "SAVE:unicode two", "non-ASCII slot reads back");
        PutFile(slot, "garbage");
        const SaveReadResult recovered = ReadSaveFile(slot, Validator);
        Check(recovered.Status == SaveReadStatus::RecoveredFromBackup && recovered.Text == "SAVE:unicode one", "non-ASCII slot recovers from backup");
        Check(DeleteSaveFiles(slot).Ok && !Exists(slot) && !Exists(SaveBackupPath(slot)), "non-ASCII slot deletes");
    }

    // Delete: all three files go, a missing slot is success, and a stuck backup
    // keeps the primary rather than letting the old save come back to life.
    {
        const std::string slot = FreshSlot("delete");
        Check(WriteSaveFile(slot, "SAVE:one", Validator).Ok && WriteSaveFile(slot, "SAVE:two", Validator).Ok, "two versions written");
        PutFile(SaveTempPath(slot), "SAVE:stale");
        const SaveOperationResult removed = DeleteSaveFiles(slot);
        Check(removed.Ok, "delete succeeds");
        Check(!Exists(slot) && !Exists(SaveBackupPath(slot)) && !Exists(SaveTempPath(slot)), "delete removes primary, backup and temp");
        Check(ReadSaveFile(slot, Validator).Status == SaveReadStatus::Missing, "a deleted slot reads as Missing");
        Check(DeleteSaveFiles(slot).Ok, "deleting a missing slot is success");
        Check(DeleteSaveFiles(Utf8(SaveStoreTestRoot / "no_such_folder" / "x.json")).Ok, "deleting in a missing folder is success");

        const std::string stuck = FreshSlot("delete_stuck");
        PutFile(stuck, "SAVE:primary");
        // A non-empty folder where the .bak should be cannot be removed by a
        // plain remove, which stands in for a locked or undeletable backup.
        std::error_code ec;
        const fs::path stuckBackup = NativePath(SaveBackupPath(stuck));
        fs::create_directories(stuckBackup / "inner", ec);
        Check(!ec, "stuck backup folder built");
        PutFile(Utf8(stuckBackup / "inner" / "file.txt"), "x");
        PutFile(SaveTempPath(stuck), "SAVE:tmp");
        const SaveOperationResult refused = DeleteSaveFiles(stuck);
        Check(!refused.Ok && !refused.Message.empty() && !ContainsPath(refused.Message, stuck), "delete reports a backup it cannot remove");
        Check(GetFile(stuck) == "SAVE:primary", "the primary is kept when the backup cannot be removed");
        Check(!Exists(SaveTempPath(stuck)), "the temp was already removed first");
    }

    // 100 write/read round trips: the .bak is always exactly the previous
    // version, the temp never lingers, and every read is a plain Loaded.
    {
        const std::string slot = FreshSlot("roundtrips");
        std::string previous;
        for (int i = 1; i <= 100; ++i)
        {
            const std::string text = "SAVE:version " + std::to_string(i) + std::string(static_cast<size_t>(i % 7) * 13, '.');
            Check(WriteSaveFile(slot, text, Validator).Ok, "round-trip write succeeds");
            const SaveReadResult read = ReadSaveFile(slot, Validator);
            Check(read.Status == SaveReadStatus::Loaded && read.Text == text, "round-trip read returns what was written");
            Check(!Exists(SaveTempPath(slot)), "no temp lingers between round trips");
            if (i == 1)
            {
                Check(!Exists(SaveBackupPath(slot)), "no backup after the first round trip");
            }
            else
            {
                Check(GetFile(SaveBackupPath(slot)) == previous, "the backup is always the previous version");
            }
            previous = text;
        }
    }

    SaveStoreTestCleanup();
    {
        std::error_code ec;
        Check(!fs::exists(SaveStoreTestRoot, ec), "the test folder is removed at the end");
    }
    std::cout << "PASS: " << Checks << " save store checks\n";
    return 0;
}
