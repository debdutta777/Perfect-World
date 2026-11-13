#include "PerfectWorldAcceptanceFrontagesR6.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR61FrontagesScenarioTest, "PerfectWorld.KitStalls.Round6Scenario",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FR61FrontagesScenarioTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> R61Steps;
    PWAcceptance::AppendFrontagesRound6Steps(R61Steps);
    TestTrue(TEXT("waits for the world"), !R61Steps.IsEmpty() && R61Steps[0].Name == TEXT("world_ready"));
    TSet<FString> R61Names;
    for (const auto& R61Step : R61Steps)
    {
        TestFalse(TEXT("unique evidence name"), R61Names.Contains(R61Step.Name));
        R61Names.Add(R61Step.Name);
        TestTrue(TEXT("executable bounded step"), static_cast<bool>(R61Step.Run) && R61Step.TimeoutSeconds > 0);
    }
    TestTrue(TEXT("opens a real corner leaf"), R61Names.Contains(TEXT("corner_stalls_open")));
    TestTrue(TEXT("closes after exit"), R61Names.Contains(TEXT("corner_stalls_close_after_exit")));
    TestTrue(TEXT("saves the corner leaf open"), R61Names.Contains(TEXT("corner_stalls_saved_open")));
    for (const TCHAR* R61Camera : {TEXT("_first"), TEXT("_third")})
    {
        const FString R61Prefix = FString(TEXT("corner_stalls")) + R61Camera;
        TestTrue(TEXT("walks inside to the browse stand"), R61Names.Contains(R61Prefix + TEXT("_walk_to_browse")));
        TestTrue(TEXT("uses the production browse actor"), R61Names.Contains(R61Prefix + TEXT("_browse")));
        TestTrue(TEXT("walks back out"), R61Names.Contains(R61Prefix + TEXT("_walk_out")));
        TestTrue(TEXT("captures the interior"), R61Names.Contains(R61Prefix + TEXT("_interior")));
        for (const TCHAR* R61Home : {TEXT("alley_house"), TEXT("stair_flats")})
        {
            const FString R61HomePrefix = FString(R61Home) + R61Camera;
            TestTrue(TEXT("uses each private door"), R61Names.Contains(R61HomePrefix + TEXT("_private_knock")));
            TestTrue(TEXT("captures each refusal"), R61Names.Contains(R61HomePrefix + TEXT("_private_refusal")));
            TestFalse(TEXT("no admission to an unoccupied home"), R61Names.Contains(R61HomePrefix + TEXT("_walk_to_browse")));
        }
    }
    TestTrue(TEXT("saves mixed physical pairs"), R61Names.Contains(TEXT("round6_save_mixed_leaves")));
    TestTrue(TEXT("compares the original slot after Continue"), R61Names.Contains(TEXT("round6_continued_doors")));
    return true;
}
#endif
