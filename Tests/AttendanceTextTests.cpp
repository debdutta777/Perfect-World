// ROADMAP M2-07: the attendance record's text form: round trips, determinism, truncation, corruption.
#include "AttendanceTestSupport.h"

using namespace PerfectWorld;

namespace
{
constexpr const char* School = "institution.nabapara_vidyalaya";

AttendanceRecord WeekRecord()
{
    const BellSchedule& s = *RealSchedules().Find(School, "science");
    AttendanceRecord record = StartAttendanceRecord(0);
    Check(RecordAttendanceArrival(record, s, AttendanceAtMinute(0, 590)).Ok, "Monday");
    Check(RecordAttendanceArrival(record, s, AttendanceAtMinute(1, 603)).Ok, "Tuesday");
    Check(RecordAttendanceArrival(record, s, AttendanceAtMinute(2, 640)).Ok, "Wednesday");
    Check(RecordAttendanceAbsence(record, s, AttendanceAtMinute(3, 600)).Ok, "Thursday");
    Check(RecordAttendanceArrival(record, s, AttendanceAtMinute(4, 600), 800).Ok, "Friday, leaving early");
    return record;
}

std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "the text to corrupt exists");
    if (at != std::string::npos)
        text.replace(at, from.size(), to);
    return text;
}

void TestRoundTrips()
{
    const BellSchedule& s = *RealSchedules().Find(School, "science");
    std::vector<AttendanceRecord> records{StartAttendanceRecord(0), StartAttendanceRecord(9), WeekRecord()};
    AttendanceRecord trimmed = StartAttendanceRecord(3);
    std::int64_t recorded = 0;
    for (std::int64_t day = 3; recorded < 450; ++day)
    {
        if (!AttendanceIsSchoolDay(day))
            continue;
        Check(RecordAttendanceArrival(trimmed, s, AttendanceAtMinute(day, 560 + static_cast<int>(recorded * 7 % 90))).Ok, "a day for the long record");
        ++recorded;
    }
    Check(trimmed.SchoolDays == 450 && trimmed.Days.size() == AttendanceKeptDays, "the long record is trimmed");
    records.push_back(trimmed);
    AttendanceRng rng{99};
    for (int i = 0; i < 300; ++i)
    {
        AttendanceRecord random = StartAttendanceRecord(rng.Pick(0, 20));
        std::int64_t day = random.FirstDay;
        for (int step = 0; step < 12; ++step)
        {
            day += rng.Pick(0, 4);
            (void)RecordAttendanceArrival(random, s, AttendanceAtMinute(day, static_cast<int>(rng.Pick(0, 1439))),
                                          rng.Pick(0, 2) == 0 ? static_cast<std::int32_t>(rng.Pick(0, 1440)) : AttendanceStaysUntilDismissal);
        }
        records.push_back(random);
    }
    for (const AttendanceRecord& record : records)
    {
        Check(ValidateAttendanceRecord(record).empty(), "the record is valid");
        const std::string text = SerialiseAttendanceRecord(record);
        Check(text == SerialiseAttendanceRecord(record), "the same value gives the same bytes");
        const std::optional<AttendanceRecord> back = ParseAttendanceRecord(text);
        Check(back.has_value() && *back == record, "text round trips");
        Check(back.has_value() && SerialiseAttendanceRecord(*back) == text && HashAttendanceRecord(*back) == HashAttendanceRecord(record),
              "serialising the parsed record gives the same bytes and hash");
    }
    const std::string fresh = SerialiseAttendanceRecord(StartAttendanceRecord(0));
    Check(fresh.rfind("pw.attendance 1\nrange 0 0\ntotals 0 0 0 0 0 0 0\ndays 0\nend ", 0) == 0, "the documented layout");
    const std::string week = SerialiseAttendanceRecord(WeekRecord());
    Check(week.find("\nday 0 ontime 590 6 63\n") != std::string::npos && week.find("\nday 1 late 603 6 63\n") != std::string::npos &&
              week.find("\nday 2 verylate 640 6 62\n") != std::string::npos && week.find("\nday 3 absent 1440 6 0\n") != std::string::npos &&
              week.find("\nday 4 ontime 600 6 7\n") != std::string::npos,
          "the day lines carry outcome, arrival, periods held and the mask");
}

void TestHashChanges()
{
    const AttendanceRecord base = WeekRecord();
    const std::uint64_t hash = HashAttendanceRecord(base);
    const auto differs = [&](AttendanceRecord changed, const char* message) { Check(HashAttendanceRecord(changed) != hash, message); };
    AttendanceRecord r = base; r.FirstDay += 1; differs(r, "first day is hashed");
    r = base; r.NextDay += 1; differs(r, "next day is hashed");
    r = base; r.SchoolDays += 1; differs(r, "school days are hashed");
    r = base; r.OnTimeDays += 1; differs(r, "on time days are hashed");
    r = base; r.LateDays += 1; differs(r, "late days are hashed");
    r = base; r.VeryLateDays += 1; differs(r, "very late days are hashed");
    r = base; r.AbsentDays += 1; differs(r, "absent days are hashed");
    r = base; r.PeriodsHeld += 1; differs(r, "periods held are hashed");
    r = base; r.PeriodsAttended += 1; differs(r, "periods attended are hashed");
    r = base; r.Days[1].Day += 1; differs(r, "an entry day is hashed");
    r = base; r.Days[1].Outcome = AttendanceOutcome::OnTime; differs(r, "an entry outcome is hashed");
    r = base; r.Days[1].ArrivalMinute += 1; differs(r, "an entry arrival is hashed");
    r = base; r.Days[1].PeriodsHeld += 1; differs(r, "an entry's periods held are hashed");
    r = base; r.Days[1].PeriodMask ^= 1; differs(r, "an entry mask is hashed");
    r = base; r.Days.pop_back(); differs(r, "the entry count is hashed");
}

void TestRefusals()
{
    const AttendanceRecord record = WeekRecord();
    const std::string text = SerialiseAttendanceRecord(record);
    Check(ParseAttendanceRecord(text).has_value(), "the intact text parses");
    Check(!ParseAttendanceRecord("").has_value(), "empty text is refused");

    // Every truncation, at every byte.
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!ParseAttendanceRecord(std::string_view(text).substr(0, length)).has_value(), "a truncated text is refused");
    // Every single-byte change.
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        std::string changed = text;
        changed[i] = changed[i] == 'x' ? 'y' : 'x';
        Check(!ParseAttendanceRecord(changed).has_value(), "a changed byte is refused");
        changed[i] = text[i] >= '0' && text[i] <= '8' ? static_cast<char>(text[i] + 1) : '0';
        if (changed[i] != text[i])
            Check(!ParseAttendanceRecord(changed).has_value(), "a changed digit is refused");
    }
    // Extra bytes, line endings and layout.
    Check(!ParseAttendanceRecord(text + "x").has_value() && !ParseAttendanceRecord(text + "\n").has_value() &&
              !ParseAttendanceRecord(text + text).has_value(),
          "trailing text is refused");
    Check(!ParseAttendanceRecord(text.substr(0, text.size() - 1)).has_value(), "a missing final newline is refused");
    std::string crlf;
    for (const char c : text)
        crlf += c == '\n' ? std::string("\r\n") : std::string(1, c);
    Check(!ParseAttendanceRecord(crlf).has_value(), "CRLF is refused");
    Check(!ParseAttendanceRecord(Replace(text, "pw.attendance 1", "pw.attendance 2")).has_value(), "another version is refused");
    Check(!ParseAttendanceRecord(Replace(text, "pw.attendance 1", "pw.attendence 1")).has_value(), "another header is refused");
    Check(!ParseAttendanceRecord(Replace(text, "range 0 5", "range  0 5")).has_value(), "a double space is refused");
    Check(!ParseAttendanceRecord(Replace(text, "range 0 5", "range 00 5")).has_value(), "a leading zero is refused");
    Check(!ParseAttendanceRecord(Replace(text, "range 0 5", "range +0 5")).has_value() && !ParseAttendanceRecord(Replace(text, "range 0 5", "range -0 5")).has_value(),
          "a sign is refused");
    Check(!ParseAttendanceRecord(Replace(text, "range 0 5", "totals 0 5")).has_value(), "a renamed field is refused");
    Check(!ParseAttendanceRecord(Replace(text, "\ndays 5\n", "\ndays 4\n")).has_value() && !ParseAttendanceRecord(Replace(text, "\ndays 5\n", "\ndays 6\n")).has_value(),
          "a wrong count is refused");
    Check(!ParseAttendanceRecord(Replace(text, "\ndays 5\n", "\ndays 99999999999999999999\n")).has_value(), "a huge count is refused");
    Check(!ParseAttendanceRecord(Replace(text, "ontime 590", "early 590")).has_value(), "an unknown outcome is refused");
    Check(!ParseAttendanceRecord(Replace(text, "day 0 ontime", "day 1 ontime")).has_value(), "days out of order are refused");

    // Inconsistent records: valid text and hash, invalid content.
    const auto refusedContent = [&](AttendanceRecord bad, const char* message) {
        Check(!ValidateAttendanceRecord(bad).empty(), message);
        Check(!ParseAttendanceRecord(SerialiseAttendanceRecord(bad)).has_value(), message);
    };
    AttendanceRecord r = record; r.SchoolDays += 1; refusedContent(r, "school days that are not the sum of outcomes");
    r = record; r.OnTimeDays += 1; r.SchoolDays += 1; refusedContent(r, "totals that differ from the entries");
    r = record; r.PeriodsAttended = r.PeriodsHeld + 1; refusedContent(r, "more attended than held");
    r = record; r.PeriodsHeld += 1; refusedContent(r, "held that differs from the entries");
    r = record; r.Days.pop_back(); refusedContent(r, "a missing entry");
    r = record; std::swap(r.Days[0], r.Days[1]); refusedContent(r, "entries out of order");
    r = record; r.Days[2].PeriodMask = 1u << 6; refusedContent(r, "a period that was not held");
    r = record; r.Days[3].PeriodMask = 1; refusedContent(r, "an absent day that attended a period");
    r = record; r.Days[1].PeriodMask = 0; refusedContent(r, "an attended day with no period");
    r = record; r.Days[0].ArrivalMinute = 1440; refusedContent(r, "an on-time day with no arrival");
    r = record; r.Days[3].ArrivalMinute = 600; refusedContent(r, "an absent day with an arrival");
    r = record; r.Days[0].PeriodsHeld = 0; refusedContent(r, "a day with no periods held");
    r = record; r.Days[0].PeriodsHeld = 9; refusedContent(r, "a day with too many periods held");
    r = record; r.NextDay = 4; refusedContent(r, "entries beyond the next day");
    r = record; r.FirstDay = 1; refusedContent(r, "entries before the first day");
    r = record; r.Days[0].Day = 5; r.Days[1].Day = 6; refusedContent(r, "a weekend entry");
    r = record; r.SchoolDays = 6; r.AbsentDays = 2; refusedContent(r, "more school days than the range holds");
    r = record; r.NextDay = ScheduleMaxInstant / MinutesPerDay + 5; refusedContent(r, "a day past the clock");
    r = record; r.OnTimeDays = -1; r.SchoolDays = 1; refusedContent(r, "a negative total");
    r = record; r.Days.resize(AttendanceKeptDays + 1, record.Days[0]); refusedContent(r, "more entries than are kept");
}
} // namespace

void TestText()
{
    TestRoundTrips();
    TestHashChanges();
    TestRefusals();
}
