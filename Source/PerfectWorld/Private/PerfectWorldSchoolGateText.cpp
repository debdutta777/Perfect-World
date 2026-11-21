#include "PerfectWorldSchoolGate.h"

#include "PerfectWorldLaneLifeSubsystem.h"

// Words for the school gate and the timetable board. Everything here maps the core's own codes, keys and minutes to text; none of it
// decides an outcome (on time, late, absent and closed days are the portable core's, R6-07).

namespace PWSchool
{
namespace
{
/** "independence_day" becomes "Independence day". */
FString ScHumanise(const FString& Id)
{
    FString Text = Id.Replace(TEXT("_"), TEXT(" "));
    if (Text.Len() > 0)
    {
        Text.GetCharArray()[0] = FChar::ToUpper(Text[0]);
    }
    return Text;
}

FString ScSubjectName(const std::string& Token)
{
    return ScHumanise(PWLaneLife::ToFString(Token));
}

FText ScClosureText(const PerfectWorld::SchoolArrivalView& View)
{
    return FText::FromString(ScHumanise(PWLaneLife::ToFString(View.ClosureId)));
}
} // namespace

FString ScFormatMinute(int32 MinuteOfDay)
{
    const int32 Minute = FMath::Clamp(MinuteOfDay, 0, 24 * 60 - 1);
    return FString::Printf(TEXT("%02d:%02d"), Minute / 60, Minute % 60);
}

FText ScReasonText(const FString& Key)
{
    if (Key == TEXT("school.arrival.on_time"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "OnTime", "You are on time.");
    }
    if (Key == TEXT("school.arrival.late"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "Late", "You are late: a late mark is recorded.");
    }
    if (Key == TEXT("school.arrival.very_late"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "VeryLate", "You are very late: a late mark is recorded and assembly is missed.");
    }
    if (Key == TEXT("school.arrival.absent"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "Absent", "School has finished for the day: you are marked absent.");
    }
    if (Key == TEXT("school.day.holiday"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "Holiday", "No school today: it is a holiday.");
    }
    if (Key == TEXT("school.day.break"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "Break", "No school today: the school is on a break.");
    }
    if (Key == TEXT("school.day.non_school_weekday"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "Weekend", "No school today: it is the weekend.");
    }
    if (Key == TEXT("school.day.outside_year"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "OutsideYear", "The school year is not in session.");
    }
    if (Key == TEXT("school.day.outside_term"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "OutsideTerm", "No school today: it is between terms.");
    }
    return FText::GetEmpty();
}

FText ScTodayNote(const PerfectWorld::SchoolArrivalView& View)
{
    if (!View.HasSchool || View.DayKind == PerfectWorld::SchoolDayKind::SchoolDay)
    {
        return FText::GetEmpty();
    }
    const FText Base = ScReasonText(PWLaneLife::ToFString(View.ReasonTextKey));
    if (Base.IsEmpty())
    {
        return FText::GetEmpty();
    }
    if (View.ClosureId.empty())
    {
        return Base;
    }
    return FText::Format(NSLOCTEXT("PerfectWorldSchool", "TodayClosure", "{0} ({1})"), Base, ScClosureText(View));
}

FText ScSignInMessage(const FScSignInResult& Result)
{
    const FString Key = PWLaneLife::ToFString(Result.View.ReasonTextKey);
    if (Result.bAccepted && Result.View.Recorded.has_value() && Result.View.Recorded->Outcome == PerfectWorld::AttendanceOutcome::Absent)
    {
        // The core records an arrival at or after dismissal as an absence (its arrival minute is a sentinel, not a time of day).
        return ScReasonText(Key);
    }
    if (Result.bAccepted && Result.View.Recorded.has_value())
    {
        return FText::Format(NSLOCTEXT("PerfectWorldSchool", "SignedIn", "Signed in at {0}. {1}"),
                             FText::FromString(ScFormatMinute(Result.View.Recorded->ArrivalMinute)), ScReasonText(Key));
    }
    if (Result.Code == TEXT("school.gate_closed"))
    {
        return FText::Format(NSLOCTEXT("PerfectWorldSchool", "GateClosed", "The school gate has not opened yet. It opens at {0}."),
                             FText::FromString(ScFormatMinute(Result.View.GatesOpenMinute)));
    }
    if (Result.Code == TEXT("school.not_school_day"))
    {
        const FText Note = ScTodayNote(Result.View);
        if (!Note.IsEmpty())
        {
            return Note;
        }
    }
    if (Result.Code == TEXT("school.already_recorded") && Result.View.Recorded.has_value() &&
        Result.View.Recorded->Outcome != PerfectWorld::AttendanceOutcome::Absent)
    {
        return FText::Format(NSLOCTEXT("PerfectWorldSchool", "AlreadySigned", "You already signed in today, at {0}. {1}"),
                             FText::FromString(ScFormatMinute(Result.View.Recorded->ArrivalMinute)), ScReasonText(Key));
    }
    if (Result.Code == TEXT("school.missing_content") || Result.Code == TEXT("school.invalid_content"))
    {
        return NSLOCTEXT("PerfectWorldSchool", "NoTimetable", "The school's timetable could not be loaded.");
    }
    if (!Result.Message.IsEmpty())
    {
        return FText::FromString(Result.Message);
    }
    return FText::FromString(Result.Code);
}

TArray<FString> ScBellBoardLines(const PerfectWorld::BellSchedule& Schedule)
{
    TArray<FString> Lines;
    Lines.Add(FString::Printf(TEXT("Timetable: %s"), *ScHumanise(PWLaneLife::ToFString(Schedule.Stream))));
    Lines.Add(FString::Printf(TEXT("%s  Gates open"), *ScFormatMinute(Schedule.GatesOpenMinute)));
    for (const PerfectWorld::BellBlock& Block : Schedule.Blocks)
    {
        const FString Span = FString::Printf(TEXT("%s-%s"), *ScFormatMinute(Block.StartMinute), *ScFormatMinute(Block.EndMinute));
        switch (Block.Kind)
        {
        case PerfectWorld::BellBlockKind::Assembly:
            Lines.Add(FString::Printf(TEXT("%s  Assembly (first bell)"), *Span));
            break;
        case PerfectWorld::BellBlockKind::Period:
            Lines.Add(FString::Printf(TEXT("%s  Period %d: %s"), *Span, PerfectWorld::BellPeriodIndex(Schedule, Block) + 1,
                                      *ScSubjectName(Block.Subject)));
            break;
        case PerfectWorld::BellBlockKind::Break:
            Lines.Add(FString::Printf(TEXT("%s  Break"), *Span));
            break;
        }
    }
    if (!Schedule.Bells.empty())
    {
        Lines.Add(FString::Printf(TEXT("%s  Dismissal"), *ScFormatMinute(Schedule.Bells.back())));
    }
    return Lines;
}
} // namespace PWSchool
