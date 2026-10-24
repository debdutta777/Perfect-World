// The packaged acceptance driver's two safety properties (AW-01): it is inert unless the command line asks for it, and its result
// file is honest after every step (a partial file says completed=false; a failed step never passes; hostile text stays valid
// JSON). The walking itself cannot be unit tested; the integrator proves it by running the packaged scenario.
// Run in the editor under "PerfectWorld.Acceptance" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
// The core's JSON reader. A bare "Json.h" can resolve to the engine's Json module header in a unity blob; ContentRegistry.h
// includes the core one from its own directory, so this always reaches PerfectWorld::ParseJson.
#include "ContentRegistry.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptanceTest
{
using PWAcceptance::FResultWriter;
using PWAcceptance::FStepRecord;

/** A throw-away output directory under Saved/Automation, removed when the test ends. */
struct FAwtTempDir
{
    FString Path;
    FAwtTempDir() : Path(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("AwAcceptance_") + FGuid::NewGuid().ToString(EGuidFormats::Digits))) {}
    ~FAwtTempDir() { IFileManager::Get().DeleteDirectory(*Path, false, true); }
};

bool AwtLoad(FAutomationTestBase& Test, const FString& File, PerfectWorld::JsonValue& Out)
{
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *File))
    {
        Test.AddError(FString::Printf(TEXT("the result file %s does not exist"), *File));
        return false;
    }
    const PerfectWorld::JsonParseResult Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
    if (!Parsed.Value.has_value())
    {
        Test.AddError(FString::Printf(TEXT("the result file is not valid JSON: %hs"), Parsed.Error.c_str()));
        return false;
    }
    Out = *Parsed.Value;
    return true;
}

bool AwtBool(const PerfectWorld::JsonValue& Root, const char* Key)
{
    const PerfectWorld::JsonValue* Value = Root.Find(Key);
    return Value != nullptr && Value->Bool;
}

int32 AwtCount(const PerfectWorld::JsonValue& Root, const char* Key)
{
    const PerfectWorld::JsonValue* Value = Root.Find(Key);
    return Value != nullptr ? static_cast<int32>(Value->Array.size()) : -1;
}
} // namespace PWAcceptanceTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcceptanceInactiveWithoutArgsTest, "PerfectWorld.Acceptance.InactiveWithoutBothArguments",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAcceptanceInactiveWithoutArgsTest::RunTest(const FString& Parameters)
{
    TestFalse(TEXT("a null command line asks for nothing"), PWAcceptance::ParseArgs(nullptr).bActive);
    TestFalse(TEXT("an empty command line asks for nothing"), PWAcceptance::ParseArgs(TEXT("")).bActive);
    TestFalse(TEXT("ordinary engine flags ask for nothing"), PWAcceptance::ParseArgs(TEXT("-RenderOffScreen -log -windowed")).bActive);
    TestFalse(TEXT("a scenario without an output directory does nothing"), PWAcceptance::ParseArgs(TEXT("-PWAcceptance=boot_walk_interact")).bActive);
    TestFalse(TEXT("an output directory without a scenario does nothing"), PWAcceptance::ParseArgs(TEXT("-PWAcceptanceOut=D:/x")).bActive);
    TestFalse(TEXT("an empty scenario does nothing"), PWAcceptance::ParseArgs(TEXT("-PWAcceptance= -PWAcceptanceOut=D:/x")).bActive);
    TestFalse(TEXT("an empty output directory does nothing"), PWAcceptance::ParseArgs(TEXT("-PWAcceptance=boot_walk_interact -PWAcceptanceOut= -log")).bActive);
    TestFalse(TEXT("the commit and slot alone do nothing"), PWAcceptance::ParseArgs(TEXT("-PWAcceptanceCommit=abc -PWAcceptanceSlot=2")).bActive);

    // Under the command line this test runs with, the driver must not exist either (skipped if someone deliberately runs it with the flags).
    if (!PWAcceptance::ParseArgs(FCommandLine::Get()).bActive)
    {
        const UPWAcceptanceSubsystem* Defaults = GetDefault<UPWAcceptanceSubsystem>();
        TestFalse(TEXT("the subsystem is not created without the arguments"), Defaults->ShouldCreateSubsystem(nullptr));
        UGameInstance* Instance = NewObject<UGameInstance>();
        Instance->AddToRoot();
        Instance->Init();
        TestNull(TEXT("a game instance carries no acceptance driver"), Instance->GetSubsystem<UPWAcceptanceSubsystem>());
        Instance->RemoveFromRoot();
    }
#if UE_BUILD_SHIPPING
    TestFalse(TEXT("a Shipping build never allows the driver"), PWAcceptance::IsBuildAllowed());
#else
    TestTrue(TEXT("a non-Shipping build allows the driver"), PWAcceptance::IsBuildAllowed());
#endif
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcceptanceReadsArgsTest, "PerfectWorld.Acceptance.ReadsEveryArgumentSafely",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAcceptanceReadsArgsTest::RunTest(const FString& Parameters)
{
    const PWAcceptance::FArgs Plain = PWAcceptance::ParseArgs(TEXT("-RenderOffScreen -PWAcceptance=boot_walk_interact -PWAcceptanceOut=D:/PWAcc/run1"));
    TestTrue(TEXT("both arguments make it active"), Plain.bActive);
    TestEqual(TEXT("scenario"), Plain.Scenario, FString(TEXT("boot_walk_interact")));
    TestTrue(TEXT("the output directory is read"), Plain.OutDir.EndsWith(TEXT("PWAcc/run1")));
    TestEqual(TEXT("the commit defaults to unknown"), Plain.Commit, FString(TEXT("unknown")));
    TestEqual(TEXT("the start defaults to auto"), Plain.Start, FString(TEXT("auto")));
    TestEqual(TEXT("the slot defaults to the first empty one"), Plain.Slot, -1);

    const PWAcceptance::FArgs Full = PWAcceptance::ParseArgs(
        TEXT("-PWAcceptance=Stationery_Shift -PWAcceptanceOut=\"D:/PW Acc/run 2\" -PWAcceptanceCommit=abc123 -PWAcceptanceStart=NEW -PWAcceptanceSlot=3"));
    TestTrue(TEXT("active"), Full.bActive);
    TestEqual(TEXT("the scenario is lower-cased"), Full.Scenario, FString(TEXT("stationery_shift")));
    TestTrue(TEXT("a quoted directory keeps its spaces"), Full.OutDir.EndsWith(TEXT("PW Acc/run 2")));
    TestEqual(TEXT("commit"), Full.Commit, FString(TEXT("abc123")));
    TestEqual(TEXT("start"), Full.Start, FString(TEXT("new")));
    TestEqual(TEXT("slot"), Full.Slot, 3);

    const PWAcceptance::FArgs Odd = PWAcceptance::ParseArgs(TEXT("-PWAcceptance=../evil\\name -PWAcceptanceOut=D:/x -PWAcceptanceStart=sideways -PWAcceptanceSlot=-4"));
    TestTrue(TEXT("a hostile scenario name is still read"), Odd.bActive);
    TestFalse(TEXT("but it can never contain a path separator or a dot"), Odd.Scenario.Contains(TEXT("/")) || Odd.Scenario.Contains(TEXT("\\")) || Odd.Scenario.Contains(TEXT(".")));
    TestEqual(TEXT("an unknown start falls back to auto"), Odd.Start, FString(TEXT("auto")));
    TestEqual(TEXT("a negative slot falls back to the first empty one"), Odd.Slot, -1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcceptanceResultFileTest, "PerfectWorld.Acceptance.ResultFileIsHonestAfterEveryStep",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAcceptanceResultFileTest::RunTest(const FString& Parameters)
{
    using namespace PWAcceptanceTest;
    FAwtTempDir Dir;
    const TArray<FString> Planned = {TEXT("alpha"), TEXT("beta"), TEXT("gamma")};
    PerfectWorld::JsonValue Root;

    FResultWriter Writer(Dir.Path, TEXT("demo"), TEXT("abc123"), Planned);
    if (!AwtLoad(*this, Writer.GetPath(), Root)) { return false; }
    TestTrue(TEXT("the file exists the moment the run starts, before any step"), AwtCount(Root, "steps") == 0);
    TestFalse(TEXT("not completed"), AwtBool(Root, "completed"));
    TestFalse(TEXT("not passed"), AwtBool(Root, "passed"));
    TestEqual(TEXT("the plan is in the file"), AwtCount(Root, "planned_steps"), 3);
    TestEqual(TEXT("the commit is in the file"), FString(UTF8_TO_TCHAR(Root.Find("commit")->String.c_str())), FString(TEXT("abc123")));
    TestEqual(TEXT("the scenario is in the file"), FString(UTF8_TO_TCHAR(Root.Find("scenario")->String.c_str())), FString(TEXT("demo")));

    TestTrue(TEXT("a step is written"), Writer.AddStep({TEXT("alpha"), true, TEXT("fine"), TEXT("demo__alpha.png")}));
    if (!AwtLoad(*this, Writer.GetPath(), Root)) { return false; }
    TestEqual(TEXT("the partial file holds the finished step"), AwtCount(Root, "steps"), 1);
    TestFalse(TEXT("a partial file is not completed (a power-off here is visible as incomplete)"), AwtBool(Root, "completed"));
    TestFalse(TEXT("a partial file is not passed"), AwtBool(Root, "passed"));
    TestFalse(TEXT("no temporary file is left behind"), IFileManager::Get().FileExists(*(Writer.GetPath() + TEXT(".tmp"))));

    const FString Nasty = FString(TEXT("quote \" backslash \\ newline \n tab \t bell ")) + FString::Chr(7) + TEXT(" caf\u00e9 \u65e5\u672c");
    TestTrue(TEXT("a failed step with hostile text is written"), Writer.AddStep({TEXT("beta"), false, Nasty, FString()}));
    if (!AwtLoad(*this, Writer.GetPath(), Root)) { return false; }
    TestEqual(TEXT("two steps"), AwtCount(Root, "steps"), 2);
    const PerfectWorld::JsonValue& Second = Root.Find("steps")->Array[1];
    TestFalse(TEXT("the failed step is recorded as failed"), AwtBool(Second, "ok"));
    TestEqual(TEXT("hostile text survives the round trip exactly"), FString(UTF8_TO_TCHAR(Second.Find("detail")->String.c_str())), Nasty);

    TestTrue(TEXT("completion is written"), Writer.Complete());
    if (!AwtLoad(*this, Writer.GetPath(), Root)) { return false; }
    TestTrue(TEXT("completed is true once the script finished"), AwtBool(Root, "completed"));
    TestFalse(TEXT("a failed step never passes, whatever else happened"), AwtBool(Root, "passed"));

    // A run that finished every planned step, all ok, passes; one that stopped early or skipped a step does not.
    const TArray<FString> Two = {TEXT("one"), TEXT("two")};
    FResultWriter Good(Dir.Path, TEXT("good"), TEXT("abc123"), Two);
    Good.AddStep({TEXT("one"), true, TEXT("ok"), FString()});
    TestFalse(TEXT("not passed before the last step"), Good.IsPassed());
    Good.AddStep({TEXT("two"), true, TEXT("ok"), FString()});
    TestFalse(TEXT("not passed before completion is written"), Good.IsPassed());
    Good.Complete();
    TestTrue(TEXT("passed when every planned step ran ok and the script completed"), Good.IsPassed());
    if (!AwtLoad(*this, Good.GetPath(), Root)) { return false; }
    TestTrue(TEXT("the file says passed"), AwtBool(Root, "passed"));

    FResultWriter Short(Dir.Path, TEXT("short"), TEXT("abc123"), Two);
    Short.AddStep({TEXT("one"), true, TEXT("ok"), FString()});
    Short.Complete();
    TestFalse(TEXT("an early finish with a planned step missing does not pass"), Short.IsPassed());

    FResultWriter Renamed(Dir.Path, TEXT("renamed"), TEXT("abc123"), Two);
    Renamed.AddStep({TEXT("one"), true, TEXT("ok"), FString()});
    Renamed.AddStep({TEXT("surprise"), true, TEXT("ok"), FString()});
    Renamed.Complete();
    TestFalse(TEXT("steps out of the plan do not pass"), Renamed.IsPassed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAcceptanceScenarioNamesTest, "PerfectWorld.Acceptance.ScenariosAreNamedAndWellFormed",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAcceptanceScenarioNamesTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> Steps;
    TestFalse(TEXT("an unknown scenario is refused"), PWAcceptance::BuildScenario(TEXT("no_such_scenario"), Steps));
    for (const TCHAR* Name : {TEXT("boot_walk_interact"), TEXT("stationery_shift")})
    {
        Steps.Reset();
        TestTrue(FString::Printf(TEXT("%s is built"), Name), PWAcceptance::BuildScenario(Name, Steps));
        TestTrue(FString::Printf(TEXT("%s has steps"), Name), Steps.Num() >= 4);
        TSet<FString> Seen;
        for (const PWAcceptance::FStepDef& Step : Steps)
        {
            TestFalse(FString::Printf(TEXT("%s: step '%s' has a unique name"), Name, *Step.Name), Seen.Contains(Step.Name));
            Seen.Add(Step.Name);
            TestTrue(FString::Printf(TEXT("%s: step '%s' has a body and a timeout"), Name, *Step.Name), static_cast<bool>(Step.Run) && Step.TimeoutSeconds > 0.0);
        }
        TestEqual(FString::Printf(TEXT("%s starts by waiting for the world"), Name), Steps[0].Name, FString(TEXT("world_ready")));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
