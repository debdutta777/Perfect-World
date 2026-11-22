#include "WeekCompression.h"

#include "StateDiff.h"
#include "WeekCompressionInternal.h"

#include <utility>

// The equivalence comparison and the weekly receipt (ROADMAP M3-02).
namespace PerfectWorld
{
namespace
{
std::string WeekCompareTwoDigits(std::int32_t value)
{
    return (value < 10 ? "0" : "") + std::to_string(value);
}

/** "2042-06-02 06:00" */
std::string WeekCompareInstant(GameInstant instant)
{
    const CalendarTime t = GameClock::ToCalendar(instant);
    return std::to_string(t.Year) + "-" + WeekCompareTwoDigits(t.Month) + "-" + WeekCompareTwoDigits(t.Day) + " " +
           WeekCompareTwoDigits(t.Hour) + ":" + WeekCompareTwoDigits(t.Minute);
}

std::string WeekCompareIssueText(const WeekPlanIssue& issue)
{
    std::string text = std::string(WeekErrorCode(issue.Error));
    if (issue.Day >= 0)
        text += " day " + std::to_string(issue.Day);
    if (issue.Entry >= 0)
        text += " entry " + std::to_string(issue.Entry);
    if (!issue.Detail.empty())
        text += " (" + issue.Detail + ")";
    return text;
}
} // namespace

WeekEquivalence CompareWeekResults(const WeekResult& a, const WeekResult& b)
{
    WeekEquivalence report;
    if (!a.Ok() || !b.Ok())
    {
        const std::string left = a.Ok() ? "ok" : WeekCompareIssueText(a.Issue);
        const std::string right = b.Ok() ? "ok" : WeekCompareIssueText(b.Issue);
        if (left != right)
            report.Differences.push_back("outcome: " + left + " -> " + right);
        return report;
    }
    const WeekLedger& x = a.Ledger;
    const WeekLedger& y = b.Ledger;
    if (x.StartInstant != y.StartInstant || x.EndInstant != y.EndInstant)
        report.Differences.push_back("end_minute");
    if (x.ChargedByMode != y.ChargedByMode)
        report.Differences.push_back("charged_by_mode");
    if (x.TotalCharged != y.TotalCharged)
        report.Differences.push_back("total_charged");
    if (x.Completed != y.Completed)
        report.Differences.push_back("completed");
    if (x.DecidedByOrder != y.DecidedByOrder)
        report.Differences.push_back("decided_by_order");
    if (x.Credits != y.Credits)
        report.Differences.push_back("credits");
    if (x.Evidence != y.Evidence)
        report.Differences.push_back("evidence");
    if (a.EntriesResolved != b.EntriesResolved)
        report.Differences.push_back("entries_resolved");
    if (!(a.Disclosures == b.Disclosures))
        report.Differences.push_back("disclosures");

    StateDiffOptions options;
    for (const std::string_view path : WeekEquivalenceIgnoredPaths)
        options.IgnorePaths.emplace_back(path);
    const StateDiffReport diff = DiffWorldStates(a.World, b.World, options);
    for (const StateDiffEntry& entry : diff.Entries)
        report.Differences.push_back("state " + entry.Path + ": " + entry.A + " -> " + entry.B);
    return report;
}

std::string_view CompareWeekToTimeLedger(const WeekResult& result, const TimeLedger& ledger)
{
    const WeekLedger& mine = result.Ledger;
    if (mine.EndInstant - mine.StartInstant != ledger.EndMinute)
        return "end_minute";
    if (mine.ChargedByMode != ledger.ChargedByMode)
        return "charged_by_mode";
    if (mine.TotalCharged != ledger.TotalCharged)
        return "total_charged";
    if (mine.Completed != ledger.Completed)
        return "completed";
    // Arriving at school takes no time, so the contract does not see it; a decision about it is still disclosed here.
    std::int32_t untimed = 0;
    for (const WeekDisclosure& disclosure : result.Disclosures)
        untimed += disclosure.Kind == WeekEntryKind::School ? 1 : 0;
    if (mine.DecidedByOrder - untimed != ledger.DecidedByOrder)
        return "decided_by_order";
    return {};
}

std::string RenderWeekReceipt(const WeekResult& result)
{
    std::string text = "week receipt\n";
    if (!result.Ok())
    {
        text += "  refused: " + WeekCompareIssueText(result.Issue) + "\n  nothing was changed\n";
        return text;
    }
    const WeekLedger& ledger = result.Ledger;
    text += "  resolved " + std::to_string(result.EntriesResolved) + " entries, " + WeekCompareInstant(ledger.StartInstant) + " to " +
            WeekCompareInstant(ledger.EndInstant) + " (" + std::to_string(ledger.EndInstant - ledger.StartInstant) + " minutes)\n";
    text += "  charged:";
    for (std::size_t mode = 0; mode < ClockModeCount; ++mode)
    {
        if (ledger.ChargedByMode[mode] != 0)
            text += " " + std::string(ClockModeName(static_cast<ClockMode>(mode))) + " " + std::to_string(ledger.ChargedByMode[mode]);
    }
    text += " total " + std::to_string(ledger.TotalCharged) + "\n";
    if (!ledger.Credits.empty())
    {
        text += "  learning:";
        for (const SkillCredit& credit : ledger.Credits)
            text += " " + credit.SkillId + " " + std::to_string(credit.MilliXp);
        text += "\n";
    }
    text += "  decided_by_order: " + std::to_string(ledger.DecidedByOrder) + "\n";
    for (const WeekDisclosure& disclosure : result.Disclosures)
        text += "    day " + std::to_string(disclosure.Day) + " entry " + std::to_string(disclosure.Entry) + " " + disclosure.What +
                " by " + disclosure.OrderId + " at " + WeekCompareInstant(disclosure.At) + "\n";
    if (result.Stop != WeekStop::None)
        text += "  stopped before day " + std::to_string(result.StopDay) + " entry " + std::to_string(result.StopEntry) + ": " +
                std::string(WeekStopName(result.Stop)) + " (interrupts used " + std::to_string(result.InterruptsUsed) + " of " +
                std::to_string(WeekMaxInterrupts) + ")\n";
    else
        text += "  completed the whole plan\n";
    return text;
}
} // namespace PerfectWorld
