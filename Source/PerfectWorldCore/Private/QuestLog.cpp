#include "QuestLog.h"

#include "Conversation.h"
#include "Doors.h"
#include "LaneJournal.h"
#include "WorldState.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <limits>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::string_view QwFactPrefix = "fact.";
constexpr std::string_view QwActivityPrefix = "activity.";

/** One authored place on the lane that somebody lives or works at, with the door whose hours say when it can be reached. */
struct QwPlaceRow
{
    std::string_view Suffix;
    std::string_view LocationId;
    std::string_view DoorId;
    std::array<std::string_view, 2> People;
};

// Place and people come from the authored npc_schedule homes and workplaces; the hours come from the door table at run time.
constexpr std::array<QwPlaceRow, 9> QwPlaces = {{
    {"caretaker_flat", "location.nabapara.caretaker_flat", "door.nabapara.caretaker_flat_front", {"person.subrata_dey", ""}},
    {"tall_flats_unit_a", "location.nabapara.tall_flats_unit_a", "door.nabapara.tall_flats_unit_a", {"person.nirmal_bose", ""}},
    {"tall_flats_unit_b", "location.nabapara.tall_flats_unit_b", "door.nabapara.tall_flats_unit_b", {"person.sahana_mitra", ""}},
    {"tea_row_house", "location.nabapara.tea_row_house", "door.nabapara.tea_row_house_front", {"person.rupa_ghosh", ""}},
    {"old_house", "location.nabapara.old_house", "door.nabapara.old_house_front", {"person.prabir_chatterjee", ""}},
    {"tall_flats_upper_a", "location.nabapara.tall_flats_upper_a", "door.nabapara.tall_flats_upper_a", {"person.ishita_banerjee", ""}},
    {"repair_workshop", "location.nabapara.repair_workshop", "door.nabapara.repair_workshop_front", {"person.debashis_pal", ""}},
    {"hardware_store", "location.nabapara.hardware_store", "door.nabapara.hardware_store_front", {"person.mahua_saha", ""}},
    {"stationery", "location.nabapara.stationery", "door.nabapara.stationery_front", {"person.farida_qadri", "person.anindita_sen"}},
}};

/** A remembered arrangement that is also a daily obligation. */
struct QwObligationRule
{
    std::string_view AgreementId;
    std::string_view TextKey;
    bool WeekdaysOnly;
    /** Minute of the day it falls due, or -1 for an open-ended arrangement. */
    std::int32_t DueMinute;
};

constexpr std::array<QwObligationRule, 7> QwObligations = {{
    {FactEveningRotaAgreed, "quest.today.evening_jobs", false, DayLoopEveningStartMinute},
    {FactRotaDishesWeekdays, "quest.today.evening_dishes", true, DayLoopEveningStartMinute},
    {FactRotaAlternateDays, "quest.today.evening_alternate", false, DayLoopEveningStartMinute},
    {FactRotaSweepAndCook, "quest.today.evening_sweep", false, DayLoopEveningStartMinute},
    {"fact.care_jobs_breakfast_weekdays", "quest.today.care_breakfast", true, QuestBreakfastJobsMinute},
    {"fact.care_jobs_turns", "quest.today.care_turns", false, -1},
    {"fact.care_jobs_study_room", "quest.today.care_study_room", true, -1},
}};

bool QwIsWeekday(const GameInstant now) noexcept
{
    const Weekday day = GameClock::ToCalendar(now).DayOfWeek;
    return day >= Weekday::Monday && day <= Weekday::Friday;
}

std::int32_t QwMinuteOfDay(const GameInstant now) noexcept
{
    return static_cast<std::int32_t>(now - GameClock::StartOfDay(now));
}

bool QwInsideAnyWindow(const std::vector<QuestWindow>& windows, const std::int32_t minute) noexcept
{
    return std::any_of(windows.begin(), windows.end(),
                       [minute](const QuestWindow& window) { return minute >= window.OpenMinute && minute < window.CloseMinute; });
}

/** True once anyone at this place has been spoken to (a conversation session has ever opened). */
bool QwPlaceMet(const WorldState& world, const QwPlaceRow& row)
{
    for (const std::string_view person : row.People)
    {
        if (person.empty()) continue;
        const auto found = world.Conversations.find(person);
        if (found != world.Conversations.end() && found->second.SessionCount > 0) return true;
    }
    return false;
}

const QwPlaceRow* QwPlaceOfPerson(const std::string_view personId) noexcept
{
    for (const QwPlaceRow& row : QwPlaces)
    {
        for (const std::string_view person : row.People)
        {
            if (!person.empty() && person == personId) return &row;
        }
    }
    return nullptr;
}

/** Copies the door's visiting hours, or a business's opening hours, into the item. Nothing when the door has none. */
void QwTakeWindows(const std::string_view doorId, QuestItem& item)
{
    const DoorDef* door = FindDoor(doorId);
    if (door == nullptr) return;
    if (!door->VisitingWindows.empty())
    {
        item.WindowKind = QuestWindowKind::Visiting;
        for (const DoorOpeningWindow& window : door->VisitingWindows) item.Windows.push_back({window.OpenMinute, window.CloseMinute});
    }
    else if (door->Access == DoorAccess::BusinessHours)
    {
        if (!door->OpeningWindows.empty())
        {
            for (const DoorOpeningWindow& window : door->OpeningWindows) item.Windows.push_back({window.OpenMinute, window.CloseMinute});
        }
        else if (door->CloseMinute > door->OpenMinute)
        {
            item.Windows.push_back({door->OpenMinute, door->CloseMinute});
        }
        if (!item.Windows.empty()) item.WindowKind = QuestWindowKind::Opening;
    }
}

std::string_view QwWithoutPrefix(const std::string_view text, const std::string_view prefix) noexcept
{
    return text.starts_with(prefix) ? text.substr(prefix.size()) : text;
}

/** Orders by due time (rows with none last), then id, so equal worlds give equal logs. */
void QwSortToday(std::vector<QuestItem>& items)
{
    std::stable_sort(items.begin(), items.end(), [](const QuestItem& a, const QuestItem& b) {
        const GameInstant da = a.DueAt >= 0 ? a.DueAt : std::numeric_limits<GameInstant>::max();
        const GameInstant db = b.DueAt >= 0 ? b.DueAt : std::numeric_limits<GameInstant>::max();
        return da != db ? da < db : a.Id < b.Id;
    });
}

QuestItem QwSchoolItem(const WorldState& world, const GameInstant now, const QuestLogContext& context, const GameInstant dayStart)
{
    const std::int32_t gates = context.School != nullptr ? context.School->GatesOpenMinute : QuestDefaultGatesOpenMinute;
    const std::int32_t bell = context.School != nullptr ? BellFirstBell(*context.School) : QuestDefaultFirstBellMinute;
    std::int32_t dismissal = QuestDefaultDismissalMinute;
    if (context.School != nullptr && !context.School->Bells.empty()) dismissal = context.School->Bells.back();

    QuestItem item;
    item.Id = std::string(QuestIdSchoolBell);
    item.Section = QuestSection::Today;
    item.TextKey = item.Id;
    item.PlaceKey = "place.school";
    item.DueAt = dayStart + bell;
    item.WeekdaysOnly = context.Calendar == nullptr;

    const std::int64_t campaignDay = GameClock::DayNumber(now) - GameClock::DayNumber(0);
    const AttendanceDay* recorded = nullptr;
    for (auto at = world.DayLoop.Attendance.Days.rbegin(); at != world.DayLoop.Attendance.Days.rend(); ++at)
    {
        if (at->Day == campaignDay) { recorded = &*at; break; }
        if (at->Day < campaignDay) break;
    }
    const std::int32_t minute = QwMinuteOfDay(now);
    if (recorded != nullptr)
        item.Status = recorded->Outcome == AttendanceOutcome::Absent ? QuestStatus::Missed : QuestStatus::Done;
    else if (minute >= dismissal)
        item.Status = QuestStatus::Missed;
    else if (minute >= gates)
        item.Status = QuestStatus::Due;
    else
        item.Status = QuestStatus::Open;
    return item;
}

void QwAddToday(QuestLogView& log, const WorldState& world, const GameInstant now, const QuestLogContext& context,
                const std::vector<LaneJournalAgreement>& agreements)
{
    const GameInstant dayStart = GameClock::StartOfDay(now);
    const std::int32_t minute = QwMinuteOfDay(now);
    const std::int64_t campaignDay = GameClock::DayNumber(now) - GameClock::DayNumber(0);

    if (AttendanceIsSchoolDay(context.Calendar, campaignDay)) log.Today.push_back(QwSchoolItem(world, now, context, dayStart));

    for (const LaneJournalAgreement& agreement : agreements)
    {
        for (const QwObligationRule& rule : QwObligations)
        {
            if (rule.AgreementId != agreement.AgreementId) continue;
            if (rule.WeekdaysOnly && !QwIsWeekday(now)) break;
            QuestItem item;
            item.TextKey = std::string(rule.TextKey);
            item.Id = item.TextKey;
            item.Section = QuestSection::Today;
            item.PersonId = agreement.PersonId;
            item.WhoLabel = std::string(LaneJournalDisplayName(agreement.PersonId));
            item.PlaceKey = agreement.PersonId == HouseholdAdultId ? "place.home" : "place.care_home";
            item.WeekdaysOnly = rule.WeekdaysOnly;
            if (rule.DueMinute >= 0) item.DueAt = dayStart + rule.DueMinute;
            // No per-day completion evidence exists for household jobs, so they are only ever Open or Due.
            item.Status = rule.DueMinute >= 0 && minute >= rule.DueMinute ? QuestStatus::Due : QuestStatus::Open;
            log.Today.push_back(std::move(item));
            break;
        }
    }

    const DayLoopState& day = world.DayLoop;
    if (day.Planned)
    {
        std::vector<std::pair<std::string, std::int32_t>> seen; // activity id, plan items of it so far
        for (const DayPlanItem& plan : day.Plan)
        {
            auto counter = std::find_if(seen.begin(), seen.end(), [&plan](const auto& row) { return row.first == plan.ActivityId; });
            if (counter == seen.end())
            {
                seen.emplace_back(plan.ActivityId, 0);
                counter = std::prev(seen.end());
            }
            const std::int32_t nth = ++counter->second;
            std::int32_t completed = 0;
            for (const ReceiptEntry& entry : day.Receipts.Entries)
            {
                if (entry.ActivityId == plan.ActivityId && entry.Start >= dayStart && entry.Start < dayStart + MinutesPerDay) ++completed;
            }
            const std::string suffix(QwWithoutPrefix(plan.ActivityId, QwActivityPrefix));
            QuestItem item;
            item.TextKey = "quest.today.plan." + suffix;
            item.Id = item.TextKey + "." + std::to_string(nth);
            item.Section = QuestSection::Today;
            item.ActivityId = plan.ActivityId;
            item.DueAt = dayStart + plan.StartMinute;
            // A plan is an intention, never an obligation: it is Done or waiting, never Missed.
            item.Status = nth <= completed ? QuestStatus::Done : (now >= item.DueAt ? QuestStatus::Due : QuestStatus::Open);
            log.Today.push_back(std::move(item));
        }
    }

    QwSortToday(log.Today);

    if (day.HasCommitment)
    {
        QuestItem item;
        item.Id = std::string(QuestIdCommitment);
        item.TextKey = item.Id;
        item.Section = QuestSection::Today;
        item.Status = QuestStatus::Due;
        item.ActivityId = day.Commitment.ActivityId;
        if (!day.Commitment.PlaceKind.empty()) item.PlaceKey = "place." + day.Commitment.PlaceKind;
        log.Today.insert(log.Today.begin(), std::move(item));
    }
}

void QwAddPromises(QuestLogView& log, const GameInstant now, const std::vector<LaneJournalAgreement>& agreements)
{
    const std::int32_t minute = QwMinuteOfDay(now);
    for (const LaneJournalAgreement& agreement : agreements)
    {
        QuestItem item;
        item.Id = "quest.promise." + std::string(QwWithoutPrefix(agreement.AgreementId, QwFactPrefix));
        item.Section = QuestSection::Promises;
        item.TextKey = agreement.ShortTextKey;
        item.PersonId = agreement.PersonId;
        item.WhoLabel = std::string(LaneJournalDisplayName(agreement.PersonId));
        item.WeekdaysOnly = agreement.AgreementId == FactRotaDishesWeekdays || agreement.AgreementId == "fact.care_jobs_breakfast_weekdays" ||
                            agreement.AgreementId == "fact.care_jobs_study_room";
        if (const QwPlaceRow* place = QwPlaceOfPerson(agreement.PersonId))
        {
            item.PlaceKey = std::string(place->LocationId);
            QwTakeWindows(place->DoorId, item);
        }
        else if (agreement.PersonId == HouseholdAdultId)
            item.PlaceKey = "place.home";
        else if (agreement.PersonId == "person.care_worker")
            item.PlaceKey = "place.care_home";
        item.Status = QwInsideAnyWindow(item.Windows, minute) ? QuestStatus::Due : QuestStatus::Open;
        log.Promises.push_back(std::move(item));
    }
}

void QwAddDiscoveries(QuestLogView& log, const WorldState& world, const GameInstant now)
{
    const std::int32_t minute = QwMinuteOfDay(now);
    for (const QwPlaceRow& row : QwPlaces)
    {
        if (QwPlaceMet(world, row)) continue;
        QuestItem item;
        item.Id = "quest.discover." + std::string(row.Suffix);
        item.TextKey = item.Id;
        item.Section = QuestSection::Discoveries;
        item.PlaceKey = std::string(row.LocationId);
        QwTakeWindows(row.DoorId, item);
        item.Status = QwInsideAnyWindow(item.Windows, minute) ? QuestStatus::Due : QuestStatus::Open;
        log.Discoveries.push_back(std::move(item));
    }
}

template <typename Log>
auto QwFindIn(Log& log, const std::string_view id) noexcept -> decltype(&log.Today[0])
{
    for (auto* section : {&log.Today, &log.Promises, &log.Discoveries})
    {
        for (auto& item : *section)
        {
            if (item.Id == id) return &item;
        }
    }
    return nullptr;
}
} // namespace

QuestLogView MakeQuestLog(const WorldState& world, const GameInstant now)
{
    return MakeQuestLog(world, now, QuestLogContext{});
}

QuestLogView MakeQuestLog(const WorldState& world, GameInstant now, const QuestLogContext& context)
{
    if (now < 0) now = 0;
    QuestLogView log;
    log.Now = now;
    log.Day = GameClock::DayNumber(now);
    const LaneJournalView journal = LaneJournalToday(world, log.Day);
    QwAddToday(log, world, now, context, journal.AgreementsRemembered);
    QwAddPromises(log, now, journal.AgreementsRemembered);
    QwAddDiscoveries(log, world, now);
    return log;
}

const QuestItem* QuestLogFind(const QuestLogView& log, const std::string_view id) noexcept
{
    return QwFindIn(log, id);
}

QuestItem* QuestLogFind(QuestLogView& log, const std::string_view id) noexcept
{
    return QwFindIn(log, id);
}

bool QuestLogTrack(QuestLogView& log, const std::string_view id) noexcept
{
    bool found = false;
    for (auto* section : {&log.Today, &log.Promises, &log.Discoveries})
    {
        for (QuestItem& item : *section)
        {
            item.Tracked = !id.empty() && item.Id == id;
            found = found || item.Tracked;
        }
    }
    return found;
}

const QuestItem* QuestLogTracked(const QuestLogView& log) noexcept
{
    for (const auto* section : {&log.Today, &log.Promises, &log.Discoveries})
    {
        for (const QuestItem& item : *section)
        {
            if (item.Tracked) return &item;
        }
    }
    return nullptr;
}

std::string_view QuestSectionName(const QuestSection section) noexcept
{
    switch (section)
    {
    case QuestSection::Today: return "today";
    case QuestSection::Promises: return "promises";
    case QuestSection::Discoveries: return "discoveries";
    }
    return "today";
}

std::string_view QuestStatusName(const QuestStatus status) noexcept
{
    switch (status)
    {
    case QuestStatus::Open: return "open";
    case QuestStatus::Due: return "due";
    case QuestStatus::Done: return "done";
    case QuestStatus::Missed: return "missed";
    }
    return "open";
}
} // namespace PerfectWorld
