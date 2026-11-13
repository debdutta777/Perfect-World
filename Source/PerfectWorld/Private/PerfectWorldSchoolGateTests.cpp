// R7-03 school gate (PerfectWorldSchoolGate.h): the register submits the arrival through a real portable world bound to the real bell
// schedule and calendar, and shows the core's own result with its reason; the notice never decides attendance. Also the timetable board
// (bell times read from the authoritative content) and the words.
// Run in the editor under "PerfectWorld.SchoolGate" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldSchoolGate.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GameClock.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldSchoolTestSupport.h"

namespace
{
using namespace PWSchool;
using PWSchoolTest::FScTestContent;
using PWSchoolTest::FScTestCoreGate;
using PWSchoolTest::FScTestWorld;

/** Monday 2 June 2042 is day 0; minute is the minute of that day. */
PerfectWorld::GameInstant ScGDay0(int32 Minute)
{
    return PerfectWorld::GameClock::StartOfDay(0) + Minute;
}

const TCHAR* ScGOutcomeKey(PerfectWorld::AttendanceOutcome Outcome)
{
    switch (Outcome)
    {
    case PerfectWorld::AttendanceOutcome::OnTime: return TEXT("school.arrival.on_time");
    case PerfectWorld::AttendanceOutcome::Late: return TEXT("school.arrival.late");
    case PerfectWorld::AttendanceOutcome::VeryLate: return TEXT("school.arrival.very_late");
    case PerfectWorld::AttendanceOutcome::Absent: return TEXT("school.arrival.absent");
    }
    return TEXT("");
}

/** A player with an interaction component and the register, in a test world. */
struct FScGFixture
{
    FScTestWorld Holder;
    APerfectWorldPlayerCharacter* Player = nullptr;
    UPWInteractionComponent* Interaction = nullptr;
    APWSchoolGateActor* Register = nullptr;

    bool Init()
    {
        if (Holder.World == nullptr)
        {
            return false;
        }
        FActorSpawnParameters Spawn;
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Player = Holder.World->SpawnActor<APerfectWorldPlayerCharacter>(APerfectWorldPlayerCharacter::StaticClass(), FTransform(FVector(0.0, 0.0, 500.0)), Spawn);
        Interaction = Player != nullptr ? Player->FindComponentByClass<UPWInteractionComponent>() : nullptr;
        Register = Holder.World->SpawnActor<APWSchoolGateActor>(FVector(6750.0, 5000.0, -78.0), FRotator::ZeroRotator, Spawn);
        if (Register != nullptr)
        {
            Register->Configure(FVector(6750.0, 5000.0, -78.0));
        }
        return Player != nullptr && Interaction != nullptr && Register != nullptr;
    }
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolGateWordsTest, "PerfectWorld.SchoolGate.Words",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolGateWordsTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("minute 570"), ScFormatMinute(570), FString(TEXT("09:30")));
    TestEqual(TEXT("midnight"), ScFormatMinute(0), FString(TEXT("00:00")));
    TestEqual(TEXT("last minute"), ScFormatMinute(1439), FString(TEXT("23:59")));
    for (const TCHAR* Key : {TEXT("school.arrival.on_time"), TEXT("school.arrival.late"), TEXT("school.arrival.very_late"), TEXT("school.arrival.absent"),
                             TEXT("school.day.holiday"), TEXT("school.day.break"), TEXT("school.day.non_school_weekday"), TEXT("school.day.outside_year"),
                             TEXT("school.day.outside_term")})
    {
        TestFalse(*FString::Printf(TEXT("%s has words"), Key), ScReasonText(Key).IsEmpty());
    }
    TestTrue(TEXT("an unknown key has none"), ScReasonText(TEXT("school.nonsense")).IsEmpty());

    FScTestContent Content;
    if (!Content.Init(*this))
    {
        return false;
    }
    const PerfectWorld::BellSchedule& Bell = *Content.School.School;
    const TArray<FString> Lines = ScBellBoardLines(Bell);
    // 1 title + gates + every block + dismissal.
    TestEqual(TEXT("one line per block, plus title, gates and dismissal"), Lines.Num(), static_cast<int32>(Bell.Blocks.size()) + 3);
    TestEqual(TEXT("the title names the stream"), Lines[0], FString(TEXT("Timetable: Science")));
    TestEqual(TEXT("gates"), Lines[1], FString(TEXT("09:30  Gates open")));
    TestEqual(TEXT("the first bell is the assembly"), Lines[2], FString(TEXT("10:00-10:15  Assembly (first bell)")));
    TestEqual(TEXT("period 1"), Lines[3], FString(TEXT("10:20-11:05  Period 1: Mathematics")));
    TestEqual(TEXT("dismissal"), Lines.Last(), FString(TEXT("15:50  Dismissal")));
    bool bBreak = false;
    for (const FString& Line : Lines)
    {
        bBreak |= Line == TEXT("12:50-13:20  Break");
    }
    TestTrue(TEXT("the break"), bBreak);
    // Every bell the schedule rings appears in the board's text.
    const FString All = FString::Join(Lines, TEXT("\n"));
    for (const int32 BellMinute : Bell.Bells)
    {
        TestTrue(*FString::Printf(TEXT("the %s bell is on the board"), *ScFormatMinute(BellMinute)), All.Contains(ScFormatMinute(BellMinute)));
    }
    // The other streams read from their own records.
    FScTestContent Commerce;
    if (Commerce.Init(*this, TEXT("commerce")))
    {
        TestEqual(TEXT("commerce title"), ScBellBoardLines(*Commerce.School.School)[0], FString(TEXT("Timetable: Commerce")));
    }
    FScTestContent Missing;
    FString Why;
    TestTrue(TEXT("records load"), PWSchoolTest::ScLoadSchoolRecords(*this, Missing.Db));
    TestFalse(TEXT("an unknown stream does not bind"), Missing.School.Load(Missing.Db, TEXT("astrology"), Why));
    TestFalse(TEXT("and says why"), Why.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolGateCoreTest, "PerfectWorld.SchoolGate.SignInThroughTheCore",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolGateCoreTest::RunTest(const FString& Parameters)
{
    FScTestContent Content;
    if (!Content.Init(*this))
    {
        return false;
    }
    FScGFixture Fixture;
    if (!TestTrue(TEXT("a player and a register"), Fixture.Init()))
    {
        return false;
    }
    const auto NewWorld = [&](PerfectWorld::GameInstant At) -> TSharedRef<FScTestCoreGate>
    {
        TSharedRef<FScTestCoreGate> Gate = MakeShared<FScTestCoreGate>(Content.School);
        ScGate::Install(Fixture.Holder.World, Gate);
        TestTrue(TEXT("the world's clock moves to the case's instant"), Gate->AdvanceTo(At));
        Fixture.Interaction->ShowNotice(FText::GetEmpty());
        return Gate;
    };
    const auto Notice = [&]() { return Fixture.Interaction->GetNotice().ToString(); };

    // 07:30, two hours before the gate opens: refused by the core, nothing recorded, nothing changed.
    {
        const TSharedRef<FScTestCoreGate> Gate = NewWorld(ScGDay0(450));
        const uint64 Before = Gate->State().Seq;
        Fixture.Register->Interact(Fixture.Player);
        TestEqual(TEXT("the register called the world once"), Gate->SignInCalls, 1);
        TestFalse(TEXT("07:30: not accepted"), Fixture.Register->GetLastResult().bAccepted);
        TestEqual(TEXT("07:30: the core's own refusal code"), Fixture.Register->GetLastResult().Code, FString(TEXT("school.gate_closed")));
        TestTrue(TEXT("07:30: the notice says when the gate opens"), Notice().Contains(TEXT("09:30")) && Notice().Contains(TEXT("not opened")));
        TestEqual(TEXT("07:30: no attendance was written"), static_cast<int32>(Gate->State().DayLoop.Attendance.Days.size()), 0);
        TestFalse(TEXT("07:30: a refused sign-in leaves the day unplanned (the planning step is adopted only with an accepted arrival)"), Gate->State().DayLoop.Planned);
        TestEqual(TEXT("07:30: the world did not move"), Gate->State().Seq, Before);
    }

    // On time: 09:45, and exactly at the first bell (10:00 is inclusive).
    for (const int32 Minute : {585, 600})
    {
        const TSharedRef<FScTestCoreGate> Gate = NewWorld(ScGDay0(Minute));
        Fixture.Register->Interact(Fixture.Player);
        const FString At = ScFormatMinute(Minute);
        TestTrue(*FString::Printf(TEXT("%s: accepted"), *At), Fixture.Register->GetLastResult().bAccepted);
        TestEqual(*FString::Printf(TEXT("%s: the core recorded one day"), *At), static_cast<int32>(Gate->State().DayLoop.Attendance.Days.size()), 1);
        if (Gate->State().DayLoop.Attendance.Days.empty())
        {
            continue;
        }
        const PerfectWorld::AttendanceDay& Day = Gate->State().DayLoop.Attendance.Days.front();
        TestTrue(*FString::Printf(TEXT("%s: the core says on time"), *At), Day.Outcome == PerfectWorld::AttendanceOutcome::OnTime && Day.ArrivalMinute == Minute);
        TestEqual(*FString::Printf(TEXT("%s: reason key"), *At), PWLaneLife::ToFString(Fixture.Register->GetLastResult().View.ReasonTextKey), FString(TEXT("school.arrival.on_time")));
        TestTrue(*FString::Printf(TEXT("%s: the notice shows the arrival time and 'on time'"), *At), Notice().Contains(At) && Notice().Contains(TEXT("on time")));
        TestTrue(*FString::Printf(TEXT("%s: the day was planned for the player (an empty plan the core asked for)"), *At), Fixture.Register->GetLastResult().bPlannedDay);

        // Signing in twice: the core refuses, the record is untouched.
        Fixture.Interaction->ShowNotice(FText::GetEmpty());
        Fixture.Register->Interact(Fixture.Player);
        TestFalse(*FString::Printf(TEXT("%s: a second sign-in is refused"), *At), Fixture.Register->GetLastResult().bAccepted);
        TestEqual(*FString::Printf(TEXT("%s: with the core's code"), *At), Fixture.Register->GetLastResult().Code, FString(TEXT("school.already_recorded")));
        TestTrue(*FString::Printf(TEXT("%s: the notice says so and repeats the recorded time"), *At), Notice().Contains(TEXT("already signed in")) && Notice().Contains(At));
        TestEqual(*FString::Printf(TEXT("%s: still one day"), *At), static_cast<int32>(Gate->State().DayLoop.Attendance.Days.size()), 1);
    }

    // Late (within the five-minute bell grace), very late, and after dismissal: the words follow the core's classification.
    const struct { int32 Minute; const TCHAR* Phrase; } Cases[] = {{603, TEXT("late")}, {605, TEXT("late")}, {606, TEXT("very late")}, {620, TEXT("very late")}, {955, TEXT("absent")}};
    for (const auto& Case : Cases)
    {
        const TSharedRef<FScTestCoreGate> Gate = NewWorld(ScGDay0(Case.Minute));
        Fixture.Register->Interact(Fixture.Player);
        const FString At = ScFormatMinute(Case.Minute);
        if (!TestTrue(*FString::Printf(TEXT("%s: accepted (the core records every arrival)"), *At), Fixture.Register->GetLastResult().bAccepted) ||
            Gate->State().DayLoop.Attendance.Days.empty())
        {
            continue;
        }
        const PerfectWorld::AttendanceDay& Day = Gate->State().DayLoop.Attendance.Days.front();
        // The notice and the reason key both come from the day the core wrote, never from the UI.
        TestEqual(*FString::Printf(TEXT("%s: reason key is the core's outcome"), *At), PWLaneLife::ToFString(Fixture.Register->GetLastResult().View.ReasonTextKey), FString(ScGOutcomeKey(Day.Outcome)));
        TestTrue(*FString::Printf(TEXT("%s: the notice says '%s'"), *At, Case.Phrase), Notice().Contains(Case.Phrase));
        if (Day.Outcome != PerfectWorld::AttendanceOutcome::Absent) // an absence has no arrival time
        {
            TestTrue(*FString::Printf(TEXT("%s: the notice shows the recorded arrival minute"), *At), Notice().Contains(ScFormatMinute(Day.ArrivalMinute)));
        }
    }
    {
        const TSharedRef<FScTestCoreGate> Gate = NewWorld(ScGDay0(603));
        Fixture.Register->Interact(Fixture.Player);
        TestTrue(TEXT("10:03 is Late in the core's record"), !Gate->State().DayLoop.Attendance.Days.empty() &&
                 Gate->State().DayLoop.Attendance.Days.front().Outcome == PerfectWorld::AttendanceOutcome::Late);
    }

    // Closed days come from the calendar: Independence Day (a Friday) and a Saturday.
    {
        const TSharedRef<FScTestCoreGate> Gate = NewWorld(PerfectWorld::GameClock::FromCalendar(2042, 8, 15, 9, 45));
        Fixture.Register->Interact(Fixture.Player);
        TestFalse(TEXT("Independence Day: not accepted"), Fixture.Register->GetLastResult().bAccepted);
        TestEqual(TEXT("Independence Day: the core's code"), Fixture.Register->GetLastResult().Code, FString(TEXT("school.not_school_day")));
        TestTrue(TEXT("Independence Day: the notice names the holiday"), Notice().Contains(TEXT("holiday")) && Notice().Contains(TEXT("Independence day")));
        TestEqual(TEXT("Independence Day: nothing recorded"), static_cast<int32>(Gate->State().DayLoop.Attendance.Days.size()), 0);
    }
    {
        const TSharedRef<FScTestCoreGate> Gate = NewWorld(PerfectWorld::GameClock::FromCalendar(2042, 6, 7, 9, 45));
        Fixture.Register->Interact(Fixture.Player);
        TestFalse(TEXT("Saturday: not accepted"), Fixture.Register->GetLastResult().bAccepted);
        TestTrue(TEXT("Saturday: the notice says weekend"), Notice().Contains(TEXT("weekend")));
        TestEqual(TEXT("Saturday: nothing recorded"), static_cast<int32>(Gate->State().DayLoop.Attendance.Days.size()), 0);
    }
    TestEqual(TEXT("the register's verb"), Fixture.Register->GetInteractionVerb(Fixture.Player).ToString(), FString(TEXT("Sign in at the gate")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolGateBoardTest, "PerfectWorld.SchoolGate.TimetableBoard",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolGateBoardTest::RunTest(const FString& Parameters)
{
    FScTestContent Content;
    if (!Content.Init(*this))
    {
        return false;
    }
    FScGFixture Fixture;
    if (!TestTrue(TEXT("a player"), Fixture.Init()))
    {
        return false;
    }
    TSharedRef<FScTestCoreGate> Gate = MakeShared<FScTestCoreGate>(Content.School);
    Gate->bHideSchedule = true;
    ScGate::Install(Fixture.Holder.World, Gate);
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APWSchoolBoardActor* Board = Fixture.Holder.World->SpawnActor<APWSchoolBoardActor>(FVector(5800.0, 4830.0, 0.0), FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("the board"), Board))
    {
        return false;
    }
    Board->Configure(FVector(5800.0, 4830.0, 0.0), FVector(5800.0, 4835.0, 0.0), -90.0, 8.0f);
    TestTrue(TEXT("with no schedule the board stays blank"), Board->GetPostedText().IsEmpty() && !Board->Refresh());
    Fixture.Interaction->ShowNotice(FText::GetEmpty());
    Board->Interact(Fixture.Player);
    TestTrue(TEXT("and Interact says it is not up yet"), Fixture.Interaction->GetNotice().ToString().Contains(TEXT("not been put up")));

    Gate->bHideSchedule = false;
    Board->Tick(1.0f);
    const FString Expected = FString::Join(ScBellBoardLines(*Content.School.School), TEXT("\n"));
    TestEqual(TEXT("once the content is available the board shows the bell schedule's own lines"), Board->GetPostedText(), Expected);
    TestTrue(TEXT("with the gates, assembly and dismissal"), Expected.Contains(TEXT("Gates open")) && Expected.Contains(TEXT("Assembly")) && Expected.Contains(TEXT("Dismissal")));
    Fixture.Interaction->ShowNotice(FText::GetEmpty());
    Board->Interact(Fixture.Player);
    const FString Shown = Fixture.Interaction->GetNotice().ToString();
    TestTrue(TEXT("Interact shows the whole timetable"), Shown.Contains(Expected));
    TestFalse(TEXT("on an ordinary school day it adds nothing about today"), Shown.Contains(TEXT("No school")));
    TestEqual(TEXT("verb"), Board->GetInteractionVerb(Fixture.Player).ToString(), FString(TEXT("Read the timetable")));

    // On a holiday the board adds the core's own note.
    TestTrue(TEXT("the world moves to Independence Day"), Gate->AdvanceTo(PerfectWorld::GameClock::FromCalendar(2042, 8, 15, 9, 0)));
    Fixture.Interaction->ShowNotice(FText::GetEmpty());
    Board->Interact(Fixture.Player);
    TestTrue(TEXT("a holiday is noted beneath the timetable"), Fixture.Interaction->GetNotice().ToString().Contains(TEXT("holiday")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolGateNoWorldTest, "PerfectWorld.SchoolGate.WithoutALaneWorld",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolGateNoWorldTest::RunTest(const FString& Parameters)
{
    FScGFixture Fixture;
    if (!TestTrue(TEXT("a player and a register"), Fixture.Init()))
    {
        return false;
    }
    // No gate seam installed and no lane-life world: the default gate answers honestly instead of pretending.
    Fixture.Register->Interact(Fixture.Player);
    TestFalse(TEXT("nothing is accepted"), Fixture.Register->GetLastResult().bAccepted);
    TestEqual(TEXT("the code says the school content is missing"), Fixture.Register->GetLastResult().Code, FString(TEXT("school.missing_content")));
    TestFalse(TEXT("the player is still told something"), Fixture.Interaction->GetNotice().IsEmpty());
    TestEqual(TEXT("the register was used once"), Fixture.Register->GetSignInCount(), 1);
    PerfectWorld::SchoolArrivalView View;
    FString Why;
    const TSharedPtr<IScGateWorld> Gate = ScGate::Resolve(Fixture.Holder.World);
    TestTrue(TEXT("a world always resolves a gate"), Gate.IsValid());
    TestFalse(TEXT("which cannot query without the lane-life world"), Gate.IsValid() && Gate->QueryArrival(View, Why));
    TestFalse(TEXT("and says why"), Why.IsEmpty());
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
