#include "PerfectWorldAcceptanceHomesR6.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR64HomesScenarioTest, "PerfectWorld.ResidentsR6.HomesScenarioPlan",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FR64HomesScenarioTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> Steps;
    PWAcceptance::AppendHomesRound6Steps(Steps);
    TestTrue(TEXT("waits for the world"), !Steps.IsEmpty() && Steps[0].Name == TEXT("world_ready"));
    TSet<FString> Names;
    for (const auto& Step : Steps)
    {
        TestFalse(TEXT("unique evidence name"), Names.Contains(Step.Name));
        Names.Add(Step.Name);
        TestTrue(TEXT("executable bounded step"), static_cast<bool>(Step.Run) && Step.TimeoutSeconds > 0);
    }
    for (const TCHAR* Home : {TEXT("alley"), TEXT("stair")})
    {
        const FString Prefix = FString(TEXT("R64_")) + Home;
        TestTrue(TEXT("absent refusal by the bell"), Names.Contains(Prefix + TEXT("_absent_refusal")));
        TestTrue(TEXT("asleep refusal by the bell"), Names.Contains(Prefix + TEXT("_asleep_refusal")));
        for (const TCHAR* Camera : {TEXT("_first"), TEXT("_third")})
        {
            const FString Visit = Prefix + Camera;
            TestTrue(TEXT("ring, admission, sweep, entry, Talk, Leave, exit"),
                Names.Contains(Visit + TEXT("_ring_admission")) && Names.Contains(Visit + TEXT("_open_sweep")) && Names.Contains(Visit + TEXT("_enter")) &&
                Names.Contains(Visit + TEXT("_height")) && Names.Contains(Visit + TEXT("_Talk")) && Names.Contains(Visit + TEXT("_agree_paid_Talk_Leave")) &&
                Names.Contains(Visit + TEXT("_unrestricted_exit_open")) && Names.Contains(Visit + TEXT("_exit")));
        }
    }
    TestTrue(TEXT("saves inside the stair flats"), Names.Contains(TEXT("R64_save_agreements")));
    TestTrue(TEXT("Continue remembers both agreements"), Names.Contains(TEXT("R64_continue_stair_remembered_Leave")) && Names.Contains(TEXT("R64_continue_alley_remembered_Leave")));
    return true;
}
#endif
