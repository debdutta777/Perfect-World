// Shared helpers of the R7-03 school tests (PerfectWorldSchoolCampusTests.cpp, PerfectWorldSchoolGateTests.cpp).
#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ContentRegistry.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldSchoolGate.h"

namespace PWSchoolTest
{
/** A throw-away game world (play never begins), as the kit tests use. */
struct FScTestWorld
{
    UWorld* World = nullptr;

    FScTestWorld() { World = UWorld::CreateWorld(EWorldType::Game, false); }
    ~FScTestWorld()
    {
        PWSchool::ScGate::Remove(World);
        if (World != nullptr)
        {
            World->DestroyWorld(false);
        }
    }
    FScTestWorld(const FScTestWorld&) = delete;
    FScTestWorld& operator=(const FScTestWorld&) = delete;
};

/** Reads the three record types the school needs straight from Data/Content (no cross-reference validation; the loaders validate each). */
inline bool ScLoadSchoolRecords(FAutomationTestBase& Test, PerfectWorld::ContentDb& OutDb)
{
    for (const TCHAR* Type : {TEXT("activity"), TEXT("bell_schedule"), TEXT("calendar")})
    {
        const FString Dir = FPaths::ProjectDir() / TEXT("Data/Content") / Type;
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.json")), true, false);
        if (Files.IsEmpty())
        {
            Test.AddError(FString::Printf(TEXT("no %s records under %s"), Type, *Dir));
            return false;
        }
        for (const FString& File : Files)
        {
            FString Text;
            if (!FFileHelper::LoadFileToString(Text, *(Dir / File)))
            {
                Test.AddError(FString::Printf(TEXT("cannot read %s"), *(Dir / File)));
                return false;
            }
            const auto Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
            if (!Parsed.Value.has_value())
            {
                Test.AddError(FString::Printf(TEXT("%s does not parse"), *(Dir / File)));
                return false;
            }
            const std::string Id(TCHAR_TO_UTF8(*FPaths::GetBaseFilename(File)));
            OutDb.Records.emplace(Id, PerfectWorld::ContentRecord{std::string(TCHAR_TO_UTF8(Type)), Id, std::string(TCHAR_TO_UTF8(*(Dir / File))), *Parsed.Value, {}});
        }
    }
    return true;
}

/** The content, loaded once for a test; owns the records the bound core borrows. */
struct FScTestContent
{
    PerfectWorld::ContentDb Db;
    PWSchool::FScSchoolContent School;

    bool Init(FAutomationTestBase& Test, const FString& Stream = TEXT("science"))
    {
        FString Why;
        if (!ScLoadSchoolRecords(Test, Db))
        {
            return false;
        }
        if (!School.Load(Db, Stream, Why))
        {
            Test.AddError(FString::Printf(TEXT("the school content does not load: %s"), *Why));
            return false;
        }
        return true;
    }
};

/**
 * A gate over a real portable WorldSim bound to the real content: the same SignIn step the game uses (ScSignInOnCopy), on a world the
 * test owns, so a test can read the attendance record the core wrote instead of trusting what the notice says.
 */
class FScTestCoreGate final : public PWSchool::IScGateWorld
{
public:
    explicit FScTestCoreGate(const PWSchool::FScSchoolContent& InContent, uint64 Seed = 703)
        : Content(InContent), Sim(MakeUnique<PerfectWorld::WorldSim>(Seed, PerfectWorld::MaxEventsPerAdvance, InContent.Binding))
    {
    }

    /** Moves the world's clock forward to an instant, settling due events. False when the core refuses. */
    bool AdvanceTo(PerfectWorld::GameInstant Target)
    {
        for (int32 Round = 0; Round < 64 && !PerfectWorld::IsWorldQuiescent(Sim->State()); ++Round)
        {
            if (!Sim->Submit(PerfectWorld::Command{PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}}).Accepted)
            {
                return false;
            }
        }
        if (Target > Sim->State().Clock.Now())
        {
            PerfectWorld::AdvanceTime Advance;
            Advance.Minutes = Target - Sim->State().Clock.Now();
            if (!Sim->Submit(PerfectWorld::Command{PerfectWorld::CommandSource::System, Advance}).Accepted)
            {
                return false;
            }
        }
        for (int32 Round = 0; Round < 64 && !PerfectWorld::IsWorldQuiescent(Sim->State()); ++Round)
        {
            Sim->Submit(PerfectWorld::Command{PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}});
        }
        return Sim->State().Clock.Now() == Target;
    }

    virtual bool QueryArrival(PerfectWorld::SchoolArrivalView& OutView, FString& OutWhy) override
    {
        OutView = Sim->SchoolArrivalToday();
        return true;
    }

    virtual PWSchool::FScSignInResult SignIn() override
    {
        ++SignInCalls;
        TUniquePtr<PerfectWorld::WorldSim> Next;
        PWSchool::FScSignInResult Result = PWSchool::ScSignInOnCopy(Sim->State(), Content.Binding, Next);
        if (Next.IsValid())
        {
            Sim = MoveTemp(Next);
        }
        return Result;
    }

    virtual const PerfectWorld::BellSchedule* GetBellSchedule() override { return bHideSchedule ? nullptr : Content.School; }

    const PerfectWorld::WorldState& State() const { return Sim->State(); }

    const PWSchool::FScSchoolContent& Content;
    TUniquePtr<PerfectWorld::WorldSim> Sim;
    int32 SignInCalls = 0;
    bool bHideSchedule = false;
};
} // namespace PWSchoolTest

#endif // WITH_DEV_AUTOMATION_TESTS
