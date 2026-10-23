// The R7-03 scenarios' scripts (school_round7, school_round7_late): unique, bounded, executable steps in the intended order. They do not
// run the game; the packaged run does (Docs: acceptance runner).
// Run in the editor under "PerfectWorld.SchoolCampus" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldAcceptanceSchoolR7.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScSchoolScenarioTest, "PerfectWorld.SchoolCampus.AcceptanceScripts",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FScSchoolScenarioTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> OnTime;
    TArray<PWAcceptance::FStepDef> Late;
    PWAcceptance::AppendSchoolRound7Steps(OnTime);
    PWAcceptance::AppendSchoolRound7LateSteps(Late);

    const auto Names = [this](const TArray<PWAcceptance::FStepDef>& Steps, const TCHAR* Label)
    {
        TArray<FString> Order;
        TSet<FString> Seen;
        TestTrue(*FString::Printf(TEXT("%s: waits for the world first"), Label), !Steps.IsEmpty() && Steps[0].Name == TEXT("world_ready"));
        for (const PWAcceptance::FStepDef& Step : Steps)
        {
            TestFalse(*FString::Printf(TEXT("%s: step '%s' has a unique name"), Label, *Step.Name), Seen.Contains(Step.Name));
            Seen.Add(Step.Name);
            TestTrue(*FString::Printf(TEXT("%s: step '%s' has a body and a timeout"), Label, *Step.Name), static_cast<bool>(Step.Run) && Step.TimeoutSeconds > 0.0);
            Order.Add(Step.Name);
        }
        return Order;
    };
    const TArray<FString> A = Names(OnTime, TEXT("school_round7"));
    const TArray<FString> B = Names(Late, TEXT("school_round7_late"));
    const auto Index = [](const TArray<FString>& Order, const TCHAR* Name) { return Order.IndexOfByKey(FString(Name)); };

    // On time: home, the plan's route, timetable, a refusal before the gate opens, the clock, the sign-in, the core's record, then the rooms.
    for (const TCHAR* Name : {TEXT("home_open_front_door"), TEXT("walk_home_to_school_gate"), TEXT("read_timetable_board"), TEXT("sign_in_before_the_gate_opens"),
                              TEXT("clock_to_the_on_time_window_0940"), TEXT("sign_in_at_the_gate"), TEXT("core_recorded_on_time"), TEXT("shot_signed_in_on_time"),
                              TEXT("walk_into_the_office"), TEXT("shot_playground")})
    {
        TestTrue(*FString::Printf(TEXT("school_round7 has step %s"), Name), Index(A, Name) != INDEX_NONE);
    }
    TestTrue(TEXT("the walk to the gate comes before signing in, which comes before the classroom"),
             Index(A, TEXT("walk_home_to_school_gate")) < Index(A, TEXT("sign_in_at_the_gate")) && Index(A, TEXT("sign_in_at_the_gate")) < Index(A, TEXT("shot_classroom_interior_first")));
    TestTrue(TEXT("the refusal before the gate opens precedes the clock move"), Index(A, TEXT("sign_in_before_the_gate_opens")) < Index(A, TEXT("clock_to_the_on_time_window_0940")));
    TestTrue(TEXT("the core's record is checked straight after the sign-in"), Index(A, TEXT("core_recorded_on_time")) == Index(A, TEXT("sign_in_at_the_gate")) + 1);
    for (const TCHAR* Camera : {TEXT("_first"), TEXT("_third")})
    {
        for (const TCHAR* Stem : {TEXT("classroom_walk_to_the_doorway"), TEXT("shot_classroom_doorway"), TEXT("classroom_walk_in_along_the_aisle"), TEXT("shot_classroom_interior"), TEXT("classroom_walk_out")})
        {
            TestTrue(*FString::Printf(TEXT("the classroom is walked in the %s camera: %s"), Camera, Stem), Index(A, *(FString(Stem) + Camera)) != INDEX_NONE);
        }
    }
    TestTrue(TEXT("the third-person camera is chosen before its classroom walk"), Index(A, TEXT("camera_third")) < Index(A, TEXT("classroom_walk_to_the_doorway_third")));

    // Late: same walk, the late sign-in, the core's Late record, a screenshot; no classroom.
    for (const TCHAR* Name : {TEXT("walk_home_to_school_gate"), TEXT("clock_to_one_minute_after_the_first_bell_1001"), TEXT("sign_in_late_at_the_gate"), TEXT("core_recorded_late"), TEXT("shot_signed_in_late")})
    {
        TestTrue(*FString::Printf(TEXT("school_round7_late has step %s"), Name), Index(B, Name) != INDEX_NONE);
    }
    TestTrue(TEXT("late: the clock moves after the walk and before the sign-in"),
             Index(B, TEXT("walk_home_to_school_gate")) < Index(B, TEXT("clock_to_one_minute_after_the_first_bell_1001")) &&
             Index(B, TEXT("clock_to_one_minute_after_the_first_bell_1001")) < Index(B, TEXT("sign_in_late_at_the_gate")));
    TestEqual(TEXT("late: no classroom steps"), Index(B, TEXT("shot_classroom_interior_first")), static_cast<int32>(INDEX_NONE));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
