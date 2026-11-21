#pragma once

#include "GameClock.h"
#include "SchoolFollowupContent.h"

namespace PerfectWorld
{
enum class P407UFollowupAction : std::uint8_t { Book, Attend, Cancel };
/** Only stable identity and the disclosed appointment instant cross the command boundary. */
struct P407UFollowupCommand
{
    static constexpr std::string_view TypeId = "exam.followup.apply";
    P407UFollowupAction Kind = P407UFollowupAction::Book;
    std::string ActionId;
    GameInstant ExpectedAt = 0;
};
struct P407UFollowupView
{
    SchoolFollowupAction Action;
    GameInstant At = 0;
    bool Booked = false;
    bool Attended = false;
    bool CanBook = false;
    bool CanAttend = false;
};
/** Reservations and attendance use the existing saved/hashed world flag map.
 * Keys: p407u.followup.<appointmentId>.book.<instant> and .done.<instant>.
 * Book is free, Attend charges only the authored session duration, once. An
 * expired booking can be replaced; Cancel releases an optional reservation.
 * No admissions, retake registration, learning or financial awards are implied.
 */
inline constexpr std::string_view P407UFollowupFlagPrefix = "p407u.followup.";
} // namespace PerfectWorld
