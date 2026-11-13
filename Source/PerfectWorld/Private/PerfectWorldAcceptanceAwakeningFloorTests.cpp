// The R9-06 awakening_floor script: unique, bounded, executable steps in the intended order. It does not run the game; the packaged run does.
// Run in the editor under "PerfectWorld.AwakeningFloor" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldAcceptanceAwakeningFloor.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwFloorScenarioTest, "PerfectWorld.AwakeningFloor.AcceptanceScript",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwFloorScenarioTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> Steps;
    PWAcceptance::AppendAwakeningFloorSteps(Steps);
    TArray<FString> Order;
    TSet<FString> Seen;
    TestTrue(TEXT("waits for the world first"), !Steps.IsEmpty() && Steps[0].Name == TEXT("world_ready"));
    for (const PWAcceptance::FStepDef& Step : Steps)
    {
        TestFalse(*FString::Printf(TEXT("step '%s' has a unique name"), *Step.Name), Seen.Contains(Step.Name));
        Seen.Add(Step.Name);
        TestTrue(*FString::Printf(TEXT("step '%s' has a body and a timeout"), *Step.Name), static_cast<bool>(Step.Run) && Step.TimeoutSeconds > 0.0);
        Order.Add(Step.Name);
    }
    const auto Index = [&Order](const TCHAR* Name) { return Order.IndexOfByKey(FString(Name)); };
    for (const TCHAR* Name : {TEXT("awakening_floor_built"), TEXT("declared_start_forecourt_terminal"), TEXT("enter_the_virtual_floor"), TEXT("help_the_classmate_across"),
                              TEXT("protect_with_the_barrier"), TEXT("coordinate_the_carriers"), TEXT("session_complete_after_helping"), TEXT("return_to_the_campus_first"),
                              TEXT("withdraw_at_the_crossing"), TEXT("withdraw_at_the_corridor"), TEXT("decline_the_confrontation"), TEXT("return_to_the_campus_second"),
                              TEXT("bridge_saw_every_choice")})
    {
        TestTrue(*FString::Printf(TEXT("awakening_floor has step %s"), Name), Index(Name) != INDEX_NONE);
    }
    TestTrue(TEXT("the floor is checked before the teleport, the teleport before entering"),
             Index(TEXT("awakening_floor_built")) < Index(TEXT("declared_start_forecourt_terminal")) &&
             Index(TEXT("declared_start_forecourt_terminal")) < Index(TEXT("enter_the_virtual_floor")));
    TestTrue(TEXT("the helping pass completes before the withdrawal pass starts"),
             Index(TEXT("return_to_the_campus_first")) < Index(TEXT("fresh_session_for_the_withdrawal_route")) &&
             Index(TEXT("fresh_session_for_the_withdrawal_route")) < Index(TEXT("withdraw_at_the_crossing")));
    TestTrue(TEXT("first person for the helping pass, third person for the withdrawal pass"),
             Index(TEXT("camera_first")) < Index(TEXT("enter_the_virtual_floor")) && Index(TEXT("camera_third")) > Index(TEXT("return_to_the_campus_first")) &&
             Index(TEXT("camera_third")) < Index(TEXT("withdraw_at_the_crossing")));
    for (const TCHAR* Shot : {TEXT("shot_terminal_forecourt"), TEXT("shot_floor_arrival_first"), TEXT("shot_station1_after_first"), TEXT("shot_station3_after_first"),
                              TEXT("shot_back_on_campus_first"), TEXT("shot_floor_arrival_third"), TEXT("shot_safe_area_with_teacher"), TEXT("shot_station3_handover_third"),
                              TEXT("shot_back_on_campus_third")})
    {
        TestTrue(*FString::Printf(TEXT("screenshot %s"), Shot), Index(Shot) != INDEX_NONE);
    }
    TestTrue(TEXT("the bridge's records are checked last"), Index(TEXT("bridge_saw_every_choice")) == Order.Num() - 1);
    TestTrue(TEXT("only one declared teleport"), Order.FilterByPredicate([](const FString& Name) { return Name.Contains(TEXT("declared_start")); }).Num() == 1);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
