#pragma once

#include "StateDiff.h"

#include <cstdint>
#include <string>
#include <string_view>

// Shared by StateDiff.cpp (the world sections and the save-level sections) and
// StateDiffLines.cpp (the day-loop envelope and the line-by-line text comparison).
namespace PerfectWorld
{
/** Collects the entries of one diff, honouring the ignored paths. */
class StateDiffSink
{
public:
    StateDiffSink(StateDiffReport& report, const StateDiffOptions& options) : Report_(report), Options_(options) {}

    /** True when `path` is an ignored path or lies below one. */
    [[nodiscard]] bool Ignored(std::string_view path) const;
    /** Records a difference unless the path is ignored. */
    void Add(std::string_view section, std::string_view path, std::string a, std::string b);
    /** Records the difference when the two texts are not equal. */
    void Field(std::string_view section, std::string_view path, const std::string& a, const std::string& b);
    [[nodiscard]] std::size_t Count() const noexcept { return Report_.Entries.size(); }

private:
    StateDiffReport& Report_;
    const StateDiffOptions& Options_;
};

/** "0x" and sixteen lowercase hex digits. */
[[nodiscard]] std::string StateDiffHex(std::uint64_t value);

/** Compares two day loops by their envelope sections; fills the "dayloop" section. */
void StateDiffDayLoop(StateDiffSink& sink, const DayLoopState& a, const DayLoopState& b);
} // namespace PerfectWorld
