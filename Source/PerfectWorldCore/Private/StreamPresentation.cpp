// School stream presentation copy. Transcribed from Docs/UI_FRONTEND_SPEC.md
// section 4 (Stream): the card summaries, the common sample-day rows with each
// stream's class-time activity, the two captions and the routes copy.

#include "StreamPresentation.h"

#include <algorithm>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view StreamSampleCaption =
    "An illustration of school life at Nabapara Vidyalaya, not a fixed schedule.";
constexpr std::string_view StreamClosingCaption =
    "At most two classes a day need foreground play. Other periods follow your timetable.";
constexpr std::string_view StreamRouteHeading = "Room to change direction";
constexpr std::string_view StreamRouteFlexibility =
    "Later study and prerequisite courses can open other routes, including engineering from Humanities.";
constexpr std::string_view StreamEqualXp =
    "Every stream starts with the same Academics XP. Trait XP is added separately.";

constexpr StreamPresentationEntry StreamEntry(std::string_view id, std::string_view cardSummary,
                                              std::string_view classActivity)
{
    StreamPresentationEntry entry;
    entry.StreamId = id;
    entry.CardSummary = cardSummary;
    entry.SampleDay = {{
        {"Assembly", "Meet your class and hear the day's notices."},
        {"Class time", classActivity},
        {"Shared work", "Prepare an explanation with classmates."},
        {"After school", "Make time for a club, a household task or rest."},
    }};
    entry.SampleDayCaption = StreamSampleCaption;
    entry.ClosingCaption = StreamClosingCaption;
    entry.RouteHeading = StreamRouteHeading;
    entry.RouteFlexibilityText = StreamRouteFlexibility;
    entry.EqualStartingXPText = StreamEqualXp;
    return entry;
}

// In SchoolStreams() order.
constexpr std::array<StreamPresentationEntry, SchoolStreamCount> StreamTable{{
    StreamEntry("science", "Explore physical systems through models and practical investigation.",
                "Model water flow and investigate a measurement."),
    StreamEntry("commerce", "Explore accounts, exchange and the decisions behind them.",
                "Reconcile a daybook and explain a discrepancy."),
    StreamEntry("humanities", "Explore people, places and ideas through evidence and interpretation.",
                "Compare two accounts and build a supported interpretation."),
}};
} // namespace

const std::array<StreamPresentationEntry, SchoolStreamCount>& StreamPresentations() noexcept
{
    return StreamTable;
}

const StreamPresentationEntry* StreamPresentation(std::string_view streamId) noexcept
{
    const auto found = std::find_if(StreamTable.begin(), StreamTable.end(),
        [streamId](const StreamPresentationEntry& entry) { return entry.StreamId == streamId; });
    return found == StreamTable.end() ? nullptr : &*found;
}
} // namespace PerfectWorld
