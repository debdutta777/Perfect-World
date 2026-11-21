#pragma once

// Authored presentation copy for the three school streams on the stream screen:
// the card summary, the sample school day, its two captions, and the routes
// copy. Docs/UI_FRONTEND_SPEC.md section 4 is the source of every string. The
// interface shows these records and writes none of the copy itself. All strings
// are English source text; localisation happens in the Unreal layer, keyed by
// the stable stream IDs from SchoolStreams(). A stream changes the timetable
// and nothing else, so every stream has the same row count and the same shared
// copy; only the card summary and the class-time activity differ.

#include "CreationIdentity.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace PerfectWorld
{
/** Assembly, class time, shared work, after school. No bell times: this is an illustration, not a schedule. */
constexpr std::size_t StreamSampleRowCount = 4;

struct StreamSampleRow
{
    /** E.g. "Class time". */
    std::string_view Label;
    /** E.g. "Model water flow and investigate a measurement." */
    std::string_view Description;
};

struct StreamPresentationEntry
{
    /** A SchoolStreamDefinition::Id. */
    std::string_view StreamId;
    /** One sentence on the stream card. */
    std::string_view CardSummary;
    std::array<StreamSampleRow, StreamSampleRowCount> SampleDay{};
    /** Under the sample-day heading: it is an illustration of a school, not a fixed schedule. */
    std::string_view SampleDayCaption;
    /** After the sample-day rows: how much foreground play a school day needs. */
    std::string_view ClosingCaption;
    /** Heading over the routes copy. */
    std::string_view RouteHeading;
    /** Later study can open other routes; nothing is promised or qualified automatically. */
    std::string_view RouteFlexibilityText;
    /** Every stream starts with the same Academics XP; trait XP is added separately. */
    std::string_view EqualStartingXPText;
};

/** All three streams, in SchoolStreams() order. */
[[nodiscard]] PERFECTWORLDCORE_API const std::array<StreamPresentationEntry, SchoolStreamCount>& StreamPresentations() noexcept;
/** Null when the ID names no stream. */
[[nodiscard]] PERFECTWORLDCORE_API const StreamPresentationEntry* StreamPresentation(std::string_view streamId) noexcept;
} // namespace PerfectWorld
