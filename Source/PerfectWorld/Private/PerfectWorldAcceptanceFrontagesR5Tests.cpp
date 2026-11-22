#include "PerfectWorldAcceptanceFrontagesR5.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR53FrontagesScenarioTest, "PerfectWorld.KitStalls.Round5Scenario",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FR53FrontagesScenarioTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> R53Steps;
    PWAcceptance::AppendFrontagesRound5Steps(R53Steps);
    TestTrue(TEXT("waits for the world"), !R53Steps.IsEmpty() && R53Steps[0].Name == TEXT("world_ready"));
    TSet<FString> R53Names;
    for (const auto& R53Step : R53Steps)
    {
        TestFalse(TEXT("unique evidence name"), R53Names.Contains(R53Step.Name));
        R53Names.Add(R53Step.Name);
        TestTrue(TEXT("executable step with a bounded timeout"), static_cast<bool>(R53Step.Run) && R53Step.TimeoutSeconds > 0);
    }
    for (const TCHAR* R53Shop : {TEXT("cloth_merchant"), TEXT("narrow_shop"), TEXT("tailor_shop")})
    {
        TestTrue(TEXT("opens a real leaf"), R53Names.Contains(FString(R53Shop) + TEXT("_open")));
        TestTrue(TEXT("closes a real leaf after exit"), R53Names.Contains(FString(R53Shop) + TEXT("_close_after_exit")));
        for (const TCHAR* R53Camera : {TEXT("_first"), TEXT("_third")})
        {
            const FString R53Prefix = FString(R53Shop) + R53Camera;
            TestTrue(TEXT("walks in"), R53Names.Contains(R53Prefix + TEXT("_walk_to_browse")));
            TestTrue(TEXT("uses browse"), R53Names.Contains(R53Prefix + TEXT("_browse")));
            TestTrue(TEXT("walks out"), R53Names.Contains(R53Prefix + TEXT("_walk_out")));
            TestTrue(TEXT("captures interior evidence"), R53Names.Contains(R53Prefix + TEXT("_interior")));
        }
    }
    TestTrue(TEXT("saves mixed leaves"), R53Names.Contains(TEXT("round5_save_mixed_leaves")));
    TestTrue(TEXT("compares the original slot after Continue"), R53Names.Contains(TEXT("round5_continued_doors")));
    return true;
}
#endif
