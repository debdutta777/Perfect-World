#include "PerfectWorldAcceptanceStreetsR7.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR72StreetsScenarioTest, "PerfectWorld.Neighbourhood.Streets.Round7Scenario",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FR72StreetsScenarioTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> Steps;
    PWAcceptance::AppendStreetsRound7Steps(Steps);
    TestTrue(TEXT("waits for the world first"), !Steps.IsEmpty() && Steps[0].Name == TEXT("world_ready"));
    TSet<FString> Names;
    for (const PWAcceptance::FStepDef& Step : Steps)
    {
        TestFalse(*FString::Printf(TEXT("unique evidence name %s"), *Step.Name), Names.Contains(Step.Name));
        Names.Add(Step.Name);
        TestTrue(TEXT("executable bounded step"), static_cast<bool>(Step.Run) && Step.TimeoutSeconds > 0.0);
    }
    for (const TCHAR* Required : {TEXT("streets_present"), TEXT("declared_start_home"), TEXT("camera_first"), TEXT("camera_third"),
                                  TEXT("first_home_to_school_gate_arrived"), TEXT("first_school_gate_to_main_road_arrived"),
                                  TEXT("third_main_road_to_school_gate_arrived"), TEXT("third_school_gate_to_home_arrived"),
                                  TEXT("first_home_to_school_gate_school_gate"), TEXT("first_school_gate_to_main_road_bus_stop"),
                                  TEXT("third_main_road_to_school_gate_school_gate")})
    {
        TestTrue(*FString::Printf(TEXT("the scenario has step %s"), Required), Names.Contains(Required));
    }
    int32 Walks = 0;
    for (const FString& Name : Names)
    {
        Walks += Name.Contains(TEXT("_wp")) ? 1 : 0;
    }
    TestTrue(TEXT("the routes are walked corner by corner"), Walks >= 20);
    return true;
}
#endif
