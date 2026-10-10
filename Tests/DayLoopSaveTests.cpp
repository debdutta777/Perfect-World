#include "DayLoopTestSupport.h"

#include "Checkpoints.h"
#include "CreationSaveSerialization.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>

using namespace PerfectWorld;
namespace fs = std::filesystem;

namespace
{
fs::path TempRoot;

void Cleanup()
{
    if (!TempRoot.empty())
    {
        std::error_code ec;
        fs::remove_all(TempRoot, ec);
    }
}

std::string Utf8(const fs::path& path)
{
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

std::string GetFile(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::vector<DayCommand> FiveDays()
{
    std::vector<DayCommand> all;
    for (std::int64_t day = 0; day < 5; ++day)
    {
        const std::vector<DayCommand> script = ScriptedDay(day);
        all.insert(all.end(), script.begin(), script.end());
    }
    return all;
}

/** The state before the first command and after each one. */
std::vector<DayLoopState> StatesAlong(const std::vector<DayCommand>& script)
{
    std::vector<DayLoopState> states;
    DayLoopState state = MorningState();
    states.push_back(state);
    for (const DayCommand& command : script)
    {
        MustApply(state, command, "the script is accepted");
        states.push_back(state);
    }
    return states;
}

void Play(DayLoopState& state, const std::vector<DayCommand>& script, std::size_t from)
{
    for (std::size_t i = from; i < script.size(); ++i)
        MustApply(state, script[i], "the rest of the script is accepted");
}

void TestRoundTrip(const std::vector<DayLoopState>& states)
{
    std::set<std::uint64_t> hashes;
    for (const DayLoopState& state : states)
    {
        const std::string text = SerialiseDayLoop(state);
        Check(!text.empty() && text.rfind("pw.dayloop/1\n", 0) == 0 && text.size() >= 4 && text.compare(text.size() - 4, 4, "end\n") == 0,
              "an envelope starts with its header and ends with end");
        const DayLoopParse parsed = ParseDayLoop(text);
        Check(parsed.Ok(), "an envelope parses");
        Check(parsed.State == state, "the parsed state equals the saved one");
        Check(SerialiseDayLoop(parsed.State) == text, "the text is canonical");
        const std::uint64_t hash = HashDayLoop(state);
        Check(hash != 0 && hash == HashDayLoop(parsed.State), "the hash survives the round trip");
        char digits[17];
        for (int i = 15; i >= 0; --i)
            digits[15 - i] = "0123456789abcdef"[(hash >> (4 * i)) & 0xF];
        digits[16] = 0;
        Check(text.find(std::string("hash ") + digits + "\nend\n") != std::string::npos, "the envelope stores the hash");
        hashes.insert(hash);
    }
    Check(hashes.size() == states.size(), "every command boundary has its own hash");
}

void TestResumeEverywhere(const std::vector<DayCommand>& script, const char* what)
{
    const std::vector<DayLoopState> states = StatesAlong(script);
    const std::uint64_t finalHash = HashDayLoop(states.back());
    for (std::size_t k = 0; k < states.size(); ++k)
    {
        DayLoopParse resumed = ParseDayLoop(SerialiseDayLoop(states[k]));
        Check(resumed.Ok(), what);
        Play(resumed.State, script, k);
        Check(HashDayLoop(resumed.State) == finalHash && resumed.State == states.back(),
              "saving at a command boundary and resuming gives the same final hash");
    }
}

void TestTruncationAndEdits(const DayLoopState& state)
{
    const std::string text = SerialiseDayLoop(state);
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!ParseDayLoop(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Check(!ParseDayLoop(text + "\n").Ok() && !ParseDayLoop(text + "end\n").Ok() && !ParseDayLoop("x" + text).Ok(),
          "extra text is refused");
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        std::string edited = text;
        edited[i] = edited[i] == 'x' ? 'y' : 'x';
        Check(!ParseDayLoop(edited).Ok(), "any single edited byte is refused");
    }
}

void TestSections(const DayLoopState& state)
{
    const std::string text = SerialiseDayLoop(state);
    for (const char* name : {"runner", "practice", "receipts", "attendance"})
    {
        const std::string head = std::string("section ") + name + ' ';
        const std::size_t at = text.find(head);
        Check(at != std::string::npos, "the envelope names the section");
        const std::size_t payload = text.find('\n', at) + 1;
        std::string edited = text;
        edited[payload] = '#'; // the module's own text no longer starts as it must
        const DayLoopParse parsed = ParseDayLoop(edited);
        Check(!parsed.Ok() && parsed.Error.find(std::string(name) + " section") == 0,
              "a section its module refuses is refused, naming the section");
        std::string longer = text;
        longer.replace(at + head.size(), text.find('\n', at) - at - head.size(), "99999");
        Check(!ParseDayLoop(longer).Ok(), "a section length past the text is refused");
    }
    std::string wrongHash = text;
    const std::size_t hashAt = wrongHash.find("\nhash ") + 6;
    wrongHash[hashAt] = wrongHash[hashAt] == '0' ? '1' : '0';
    const DayLoopParse parsed = ParseDayLoop(wrongHash);
    Check(!parsed.Ok() && parsed.Error.find("hash") != std::string::npos, "a wrong hash is refused");
    Check(!ParseDayLoop("").Ok() && !ParseDayLoop("pw.dayloop/2\n").Ok() && !ParseDayLoop("\n").Ok(), "other texts are refused");
}

void TestSummaryFromText(const std::vector<DayLoopState>& states)
{
    DayLoopState running;
    for (const DayLoopState& state : states)
    {
        const std::optional<DayLoopSlotSummary> summary = ReadDayLoopSlotSummary(SerialiseDayLoop(state));
        Check(summary && *summary == SummariseDayLoop(state), "the load screen reads the summary from the envelope");
        if (state.Runner.Active() && state.Commitment.ActivityId == "activity.study_at_home")
            running = state;
    }
    const DayLoopSlotSummary mid = SummariseDayLoop(running);
    Check(mid.HasCommitment && mid.CommitmentStarted && mid.CommitmentActivityId == "activity.study_at_home" &&
              mid.LastKind == DayCommandKind::Start && mid.LastActivityId == "activity.study_at_home" && mid.Phase == DayPhase::FreeBlock,
          "mid-study the summary shows the commitment and the last action");
    const DayLoopSlotSummary morning = SummariseDayLoop(states.front());
    Check(!morning.HasCommitment && !morning.HasLastAction && morning.Phase == DayPhase::Plan && morning.Seq == 0,
          "a new life has no commitment and no last action");
    Check(!ReadDayLoopSlotSummary("garbage").has_value(), "a damaged envelope has no summary");
}

void TestCheckpoints(const std::vector<DayCommand>& script)
{
    TempRoot = fs::temp_directory_path() /
               ("pw_dayloop_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::create_directories(TempRoot, ec);
    Check(!ec, "the temp folder exists");
    std::atexit(Cleanup);
    const std::string folder = Utf8(TempRoot);

    // Life slots, written through the same store.
    std::vector<std::string> slotNames;
    std::vector<std::string> slotBytes;
    for (std::int32_t slot = 0; slot < SaveSlotCount; ++slot)
    {
        slotNames.push_back(SaveSlotFileName(slot));
        slotBytes.push_back("{\"life\": " + std::to_string(slot) + "}\n");
        Check(WriteSaveFile(folder + "/" + slotNames.back(), slotBytes.back()).Ok, "a life slot is written");
        Check(WriteSaveFile(folder + "/" + slotNames.back(), slotBytes.back()).Ok, "and again, so it has a backup");
    }
    for (std::int32_t index = 0; index < CheckpointCount; ++index)
    {
        const std::string name = CheckpointFileName(index);
        Check(name == "checkpoint_" + std::to_string(index + 1) + ".txt" && IsCheckpointFileName(name) &&
                  IsCheckpointFileName(name + ".bak") && IsCheckpointFileName(name + ".tmp"),
              "checkpoint names have their own form");
        for (const std::string& slotName : slotNames)
            Check(name != slotName && name + ".bak" != slotName + ".bak" && !IsCheckpointFileName(slotName) &&
                      !IsCheckpointFileName(slotName + ".bak"),
                  "a checkpoint file name is never a life slot's");
    }
    Check(CheckpointFileName(-1).empty() && CheckpointFileName(CheckpointCount).empty() &&
              CheckpointPath(folder, CheckpointCount).empty() && !IsCheckpointFileName("checkpoint_4.txt") &&
              !IsCheckpointFileName("checkpoint_0.txt") && !IsCheckpointFileName("checkpoint_1.json"),
          "only three checkpoints exist");

    // Five days with a checkpoint at every wake.
    DayLoopState state = MorningState();
    std::vector<std::pair<std::int32_t, std::uint64_t>> written;
    std::vector<DayLoopState> atWake;
    for (const DayCommand& command : script)
    {
        const DayLoopResult result = MustApply(state, command, "the five days are accepted");
        Check(result.CheckpointDue == (command.Kind == DayCommandKind::Wake), "a checkpoint is due exactly at each wake");
        if (result.CheckpointDue)
        {
            Check(WriteDayLoopCheckpoint(folder, result.CheckpointIndex, state).Ok, "the checkpoint is written");
            written.push_back({result.CheckpointIndex, HashDayLoop(state)});
            atWake.push_back(state);
        }
    }
    Check(written.size() == 5 && written[0].first == 0 && written[1].first == 1 && written[2].first == 2 && written[3].first == 0 &&
              written[4].first == 1,
          "the checkpoints rotate through three files");
    Check(state.Wakes == 5 && state.Attendance.OnTimeDays == 5 && state.Attendance.PeriodsAttended == 30, "five school days attended");

    // The last write to each file is what it holds.
    const std::int32_t lastWake[3] = {3, 4, 2};
    for (std::int32_t index = 0; index < CheckpointCount; ++index)
    {
        const DayLoopCheckpointRead read = ReadDayLoopCheckpoint(folder, index);
        Check(read.Status == SaveReadStatus::Loaded && read.Succeeded() && HashDayLoop(read.State) == written[static_cast<std::size_t>(lastWake[index])].second,
              "each checkpoint file holds the latest wake that rotated onto it");
    }

    // Resuming from an earlier checkpoint reproduces the uninterrupted run.
    const DayLoopCheckpointRead earlier = ReadDayLoopCheckpoint(folder, 2); // the third wake
    DayLoopState resumed = earlier.State;
    Check(resumed == atWake[2] && resumed.Seq == 3 * 29, "checkpoint 3 is the morning of the fourth day");
    Play(resumed, script, 3 * 29);
    Check(HashDayLoop(resumed) == HashDayLoop(state), "resuming from a checkpoint reaches the same final hash");

    // A damaged primary falls back to the previous write of that checkpoint.
    {
        std::ofstream out(TempRoot / CheckpointFileName(1), std::ios::binary | std::ios::trunc);
        out << "damaged";
    }
    const DayLoopCheckpointRead recovered = ReadDayLoopCheckpoint(folder, 1);
    Check(recovered.Status == SaveReadStatus::RecoveredFromBackup && HashDayLoop(recovered.State) == written[1].second,
          "a damaged checkpoint is recovered from its backup (the previous wake)");

    // Refusals leave every checkpoint as it was.
    const std::string before = GetFile(TempRoot / CheckpointFileName(0));
    Check(!WriteSaveFile(folder + "/" + CheckpointFileName(0), "not an envelope", DayLoopSaveValidator()).Ok &&
              GetFile(TempRoot / CheckpointFileName(0)) == before,
          "text that does not parse never replaces a checkpoint");
    DayLoopState invalid = state;
    invalid.Phase = DayPhase::Sleep;
    Check(!WriteDayLoopCheckpoint(folder, 0, invalid).Ok && !WriteDayLoopCheckpoint(folder, -1, state).Ok &&
              !WriteDayLoopCheckpoint(folder, CheckpointCount, state).Ok && GetFile(TempRoot / CheckpointFileName(0)) == before,
          "an invalid state or index is never written");
    Check(!ReadDayLoopCheckpoint(folder, 7).Succeeded(), "an index out of range reads nothing");

    // Checkpoints never touched a life slot, and nothing else is in the folder.
    for (std::int32_t slot = 0; slot < SaveSlotCount; ++slot)
        Check(GetFile(TempRoot / slotNames[static_cast<std::size_t>(slot)]) == slotBytes[static_cast<std::size_t>(slot)],
              "a checkpoint never overwrites a life slot");
    std::set<std::string> expected;
    for (const std::string& name : slotNames)
        expected.insert({name, name + ".bak"});
    for (const fs::directory_entry& entry : fs::directory_iterator(TempRoot))
    {
        const std::string name = entry.path().filename().string();
        Check(expected.count(name) != 0 || IsCheckpointFileName(name), "every file in the folder is a slot or a checkpoint");
    }
}
} // namespace

void TestDayLoopSave()
{
    const std::vector<DayCommand> day = ScriptedDay();
    const std::vector<DayLoopState> states = StatesAlong(day);
    TestRoundTrip(states);
    TestSummaryFromText(states);
    TestResumeEverywhere(day, "an envelope from any boundary of the day parses");
    // Truncating at every byte: a new life, a pending commitment with alternatives, a running activity, and the end of the day.
    for (const std::size_t index : {std::size_t(0), std::size_t(18), std::size_t(19), std::size_t(29)})
        TestTruncationAndEdits(states[index]);
    TestSections(states[19]);
    const std::vector<DayCommand> week = FiveDays();
    TestResumeEverywhere(week, "an envelope from any boundary of the week parses");
    TestCheckpoints(week);
    Cleanup();
}
