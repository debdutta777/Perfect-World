#include "Checkpoints.h"

#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view CheckpointStem = "checkpoint_";
constexpr std::string_view CheckpointExtension = ".txt";
} // namespace

bool IsValidCheckpointIndex(std::int32_t index) noexcept
{
    return index >= 0 && index < CheckpointCount;
}

std::string CheckpointFileName(std::int32_t index)
{
    if (!IsValidCheckpointIndex(index))
        return {};
    return std::string(CheckpointStem) + std::to_string(index + 1) + std::string(CheckpointExtension);
}

bool IsCheckpointFileName(std::string_view name)
{
    for (const std::string_view suffix : {std::string_view(".bak"), std::string_view(".tmp")})
    {
        if (name.size() > suffix.size() && name.substr(name.size() - suffix.size()) == suffix)
        {
            name.remove_suffix(suffix.size());
            break;
        }
    }
    for (std::int32_t index = 0; index < CheckpointCount; ++index)
    {
        if (name == CheckpointFileName(index))
            return true;
    }
    return false;
}

std::string CheckpointPath(const std::string& folder, std::int32_t index)
{
    const std::string name = CheckpointFileName(index);
    if (name.empty())
        return {};
    if (folder.empty())
        return name;
    const char last = folder.back();
    return last == '/' || last == '\\' ? folder + name : folder + '/' + name;
}

SaveValidator DayLoopSaveValidator()
{
    return [](std::string_view text, std::string& error)
    {
        const DayLoopParse parsed = ParseDayLoop(text);
        if (!parsed.Ok())
        {
            error = "The saved day could not be read: " + parsed.Error;
            return false;
        }
        return true;
    };
}

SaveOperationResult WriteDayLoopCheckpoint(const std::string& folder, std::int32_t index, const DayLoopState& state)
{
    SaveOperationResult result;
    const std::string path = CheckpointPath(folder, index);
    if (path.empty())
    {
        result.Message = "That is not one of the recovery checkpoints.";
        return result;
    }
    const std::string text = SerialiseDayLoop(state);
    if (text.empty())
    {
        result.Message = "The day is not in a state that can be saved.";
        return result;
    }
    return WriteSaveFile(path, text, DayLoopSaveValidator());
}

DayLoopCheckpointRead ReadDayLoopCheckpoint(const std::string& folder, std::int32_t index)
{
    DayLoopCheckpointRead out;
    const std::string path = CheckpointPath(folder, index);
    if (path.empty())
    {
        out.Message = "That is not one of the recovery checkpoints.";
        return out;
    }
    SaveReadResult read = ReadSaveFile(path, DayLoopSaveValidator());
    out.Status = read.Status;
    out.Message = std::move(read.Message);
    if (read.Succeeded())
    {
        DayLoopParse parsed = ParseDayLoop(read.Text);
        out.State = std::move(parsed.State);
    }
    return out;
}
} // namespace PerfectWorld
