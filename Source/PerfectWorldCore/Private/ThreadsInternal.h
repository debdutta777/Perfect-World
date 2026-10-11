#pragma once

// Private helpers shared by the Threads*.cpp and ContentThreadSchemas.cpp files (ROADMAP M3-06):
// the name tables that the content schema, the reader and the writer must agree on, the ID and
// fairness word rules, and the lookups the validator and the runner both use. Not public API.

#include "Threads.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace PerfectWorld::ThreadInternal
{
inline constexpr std::array<std::string_view, 4> SituationNames{"school", "home", "neighbourhood", "friendship"};
inline constexpr std::array<std::string_view, 8> KindNames{"debt", "promise", "grudge", "ambition", "secret", "obligation", "attraction", "rivalry"};
inline constexpr std::array<std::string_view, 4> ChannelNames{"ambient", "message", "conversation", "scene"};
inline constexpr std::array<std::string_view, 4> OutcomeNames{"repaired", "settled", "distance", "loss"};
inline constexpr std::array<std::string_view, 5> ConditionNames{"days_open", "days_in_stage", "calendar", "relationship", "event"};
inline constexpr std::array<std::string_view, 3> WhenNames{"during", "started", "ended"};
inline constexpr std::array<std::string_view, 4> DimensionNames{"trust", "warmth", "respect", "owed"};
inline constexpr std::array<std::string_view, 2> CompareNames{"at_least", "at_most"};
inline constexpr std::array<std::string_view, 2> WindowNames{"open", "stage"};
inline constexpr std::array<std::string_view, 4> EffectNames{"escalate", "ease", "resolve", "express"};

template <typename E, std::size_t N>
std::optional<E> ParseName(const std::array<std::string_view, N>& names, std::string_view text)
{
    for (std::size_t i = 0; i < N; ++i)
    {
        if (names[i] == text)
            return static_cast<E>(i);
    }
    return std::nullopt;
}

template <std::size_t N>
std::vector<std::string> NameList(const std::array<std::string_view, N>& names)
{
    return std::vector<std::string>(names.begin(), names.end());
}

/** A role, stage, resolution, trigger or scene ID: lowercase letters, digits and underscores, 1 to 64 characters. */
[[nodiscard]] bool IsThreadId(std::string_view text);

/** The first word of the ID (split on _ . : -) that no thread may depend on, or empty. */
[[nodiscard]] std::string_view ForbiddenWordIn(std::string_view text);

/** The calendar entry with this ID: a term, break, holiday, assessment or event, in that order; nullptr if none. */
[[nodiscard]] const CalendarSpan* FindCalendarSpan(const SchoolCalendar& calendar, std::string_view id);

/** The slot of the first Event condition of trigger `trigger` within the template's Counts. */
[[nodiscard]] std::size_t FirstEventSlot(const ThreadTemplate& templ, std::size_t trigger);

/** Reads one record and validates it against the calendar it names; returns every problem, empty when valid. */
[[nodiscard]] std::vector<std::string> ReadAndValidateThread(const ContentDb& content, const ContentRecord& record, ThreadTemplate& out);

/** Appends an Expressed event for `expression` at `at` (Threads.cpp). `trigger` is the firing trigger's index or -1. */
void PushExpressed(std::vector<ThreadEvent>& events, const ThreadInstance& instance, const ThreadExpression& expression, GameInstant at,
                   std::int32_t trigger);

/** The order of the resolved history: by time, then instance number (Threads.cpp). */
[[nodiscard]] bool ResolvedBefore(const ThreadResolved& a, const ThreadResolved& b);

/** Appends Resolved, records the thread as resolved and removes it from Active; `index` is invalid afterwards (Threads.cpp). */
void CloseThread(ThreadSet& set, std::size_t index, const ThreadResolution& resolution, GameInstant at, std::vector<ThreadEvent>& events);

/** The rules of ValidateThreadTemplate, split by area (ThreadTemplateRules.cpp). */
void ValidateThreadShape(const ThreadTemplate& templ, std::vector<std::string>& problems);
void ValidateThreadTriggers(const ThreadTemplate& templ, const SchoolCalendar* calendar, std::vector<std::string>& problems);
} // namespace PerfectWorld::ThreadInternal
