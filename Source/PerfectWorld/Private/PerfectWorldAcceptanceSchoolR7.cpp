#include "PerfectWorldAcceptanceSchoolR7.h"

#include "PerfectWorldAcceptanceSchoolR7Steps.h"
#include "PerfectWorldLaneLifeSubsystem.h"

// school_round7 and school_round7_late (see the header). The step builders are in PerfectWorldAcceptanceSchoolR7Steps.cpp.

namespace PWAcceptance
{
using namespace SchoolR7;
void AppendSchoolRound7Steps(TArray<FStepDef>& Out)
{
    ScAWalkToGate(Out);
    Out.Add(MakeShotStep(TEXT("shot_school_gate_from_the_road")));
    const PWSchool::FScPoints P = ScAPoints();
    TArray<FVector> ToBoard;
    ToBoard.Add(P.BoardStand);
    Out.Add(ScAPointsWalk(TEXT("walk_to_timetable_board"), ToBoard, 40.0, &P.BoardPoint));
    Out.Add(ScAReadBoardStep());
    Out.Add(MakeShotStep(TEXT("shot_timetable_board")));
    TArray<FVector> ToRegister;
    ToRegister.Add(FVector(P.GateCentre.X, P.GateCentre.Y - 100.0, ScAFloor));
    ToRegister.Add(FVector(P.GateCentre.X, P.GateCentre.Y + 150.0, ScAFloor));
    ToRegister.Add(P.SignInStand);
    Out.Add(ScAPointsWalk(TEXT("walk_through_gate_to_register"), ToRegister, 60.0, &P.SignInPoint));

    // Before the gate opens the core refuses, in its own words (skipped when the walk took long enough to reach the opening).
    const TSharedRef<int32> Calls = MakeShared<int32>(1);
    Out.Add(ScABaselineStep(TEXT("register_baseline_1"), Calls));
    Out.Add(ScASignInStep(TEXT("sign_in_before_the_gate_opens"), Calls, TEXT("school.gate_closed"), [](UPWAcceptanceSubsystem& Sub)
    {
        PerfectWorld::SchoolArrivalView View;
        FString Why;
        return ScAView(Sub, View, Why) && View.GatesOpenMinute <= UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld())->GetMinuteOfDay();
    }));
    Out.Add(MakeShotStep(TEXT("shot_gate_closed_result")));
    Out.Add(ScAClockStep(TEXT("clock_to_the_on_time_window_0940"), 9 * 60 + 40, 9 * 60 + 58));
    Out.Add(ScABaselineStep(TEXT("register_baseline_2"), Calls));
    Out.Add(ScASignInStep(TEXT("sign_in_at_the_gate"), Calls, FString(), nullptr));
    Out.Add(ScARecordStep(TEXT("core_recorded_on_time"), PerfectWorld::AttendanceOutcome::OnTime, TEXT("on time")));
    Out.Add(MakeShotStep(TEXT("shot_signed_in_on_time")));

    // The office, then the classroom in both cameras.
    TArray<FVector> ToOffice;
    ToOffice.Add(FVector(P.GateCentre.X, P.GateCentre.Y + 1000.0, ScAFloor));
    ToOffice.Add(FVector(P.OfficeDoor.X, P.OfficeDoor.Y - 250.0, ScAFloor));
    ToOffice.Add(FVector(P.OfficeDoor.X, P.OfficeDoor.Y - 60.0, ScAFloor));
    ToOffice.Add(P.OfficeInside);
    Out.Add(ScAPointsWalk(TEXT("walk_into_the_office"), ToOffice, 120.0));
    Out.Add(MakeShotStep(TEXT("shot_office_first")));
    for (const bool bThird : {false, true})
    {
        const FString Cam = bThird ? TEXT("_third") : TEXT("_first");
        Out.Add(ScACamera(TEXT("camera") + Cam, bThird));
        TArray<FVector> ToDoor;
        ToDoor.Add(FVector(P.OfficeDoor.X, P.OfficeDoor.Y - 150.0, ScAFloor));
        ToDoor.Add(FVector(P.ClassroomDoor.X, P.ClassroomDoor.Y - 150.0, ScAFloor));
        ToDoor.Add(FVector(P.ClassroomDoor.X, P.ClassroomDoor.Y - 40.0, ScAFloor));
        Out.Add(ScAPointsWalk(TEXT("classroom_walk_to_the_doorway") + Cam, ToDoor, 60.0));
        Out.Add(MakeShotStep(TEXT("shot_classroom_doorway") + Cam));
        TArray<FVector> Inside;
        Inside.Add(FVector(P.ClassroomDoor.X, P.ClassroomDoor.Y + 60.0, ScAFloor));
        Inside.Add(P.ClassroomAisle);
        Inside.Add(P.ClassroomFront);
        const FVector Board(P.ClassroomFront.X, P.ClassroomFront.Y + 400.0, ScAFloor + 170.0);
        Out.Add(ScAPointsWalk(TEXT("classroom_walk_in_along_the_aisle") + Cam, Inside, 60.0, &Board));
        Out.Add(MakeShotStep(TEXT("shot_classroom_interior") + Cam));
        TArray<FVector> Back;
        Back.Add(P.ClassroomAisle);
        Back.Add(FVector(P.ClassroomDoor.X, P.ClassroomDoor.Y - 60.0, ScAFloor));
        Out.Add(ScAPointsWalk(TEXT("classroom_walk_out") + Cam, Back, 40.0));
    }

    // And across to the playground.
    TArray<FVector> ToField;
    ToField.Add(FVector(P.ClassroomDoor.X, P.ClassroomDoor.Y - 250.0, ScAFloor));
    ToField.Add(FVector(8200.0, P.ClassroomDoor.Y - 250.0, ScAFloor));
    ToField.Add(P.Playground);
    Out.Add(ScAPointsWalk(TEXT("walk_to_the_playground"), ToField, 90.0));
    Out.Add(MakeShotStep(TEXT("shot_playground")));
}

void AppendSchoolRound7LateSteps(TArray<FStepDef>& Out)
{
    ScAWalkToGate(Out);
    const PWSchool::FScPoints P = ScAPoints();
    TArray<FVector> ToRegister;
    ToRegister.Add(FVector(P.GateCentre.X, P.GateCentre.Y + 150.0, ScAFloor));
    ToRegister.Add(P.SignInStand);
    Out.Add(ScAPointsWalk(TEXT("walk_through_gate_to_register"), ToRegister, 60.0, &P.SignInPoint));
    // 10:01 is one minute after the first bell, inside the core's five-minute bell grace: the core's answer is Late.
    Out.Add(ScAClockStep(TEXT("clock_to_one_minute_after_the_first_bell_1001"), 10 * 60 + 1, 10 * 60 + 2));
    const TSharedRef<int32> Calls = MakeShared<int32>(1);
    Out.Add(ScABaselineStep(TEXT("register_baseline"), Calls));
    Out.Add(ScASignInStep(TEXT("sign_in_late_at_the_gate"), Calls, FString(), nullptr));
    Out.Add(ScARecordStep(TEXT("core_recorded_late"), PerfectWorld::AttendanceOutcome::Late, TEXT("late")));
    Out.Add(MakeShotStep(TEXT("shot_signed_in_late")));
}
} // namespace PWAcceptance
