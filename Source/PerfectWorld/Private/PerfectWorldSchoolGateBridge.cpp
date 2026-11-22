#include "PerfectWorldSchoolGate.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NeighbourhoodPlan.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldLaneLifeSubsystem.h"

// How the school gate reaches the world: the per-world registry of gate seams, the school content binding, the shared sign-in step and
// the default gate over UPWLaneLifeSubsystem. See PerfectWorldSchoolGate.h.

namespace PWSchool
{
namespace
{
TMap<TWeakObjectPtr<UWorld>, TSharedPtr<IScGateWorld>>& ScRegistry()
{
    static TMap<TWeakObjectPtr<UWorld>, TSharedPtr<IScGateWorld>> Registry;
    return Registry;
}

void ScPruneRegistry()
{
    for (auto It = ScRegistry().CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
}

/** The default seam: the live lane-life world, with the school content bound to a copy that is adopted on an accepted arrival. */
class FScLaneLifeGate final : public IScGateWorld
{
public:
    FScLaneLifeGate(UWorld& InWorld, const FString& InStream) : World(&InWorld), Stream(InStream) {}

    virtual bool QueryArrival(PerfectWorld::SchoolArrivalView& OutView, FString& OutWhy) override
    {
        const UPWLaneLifeSubsystem* Life = nullptr;
        const FScSchoolContent* Content = Prepare(Life, OutWhy);
        if (Content == nullptr)
        {
            return false;
        }
        OutView = PerfectWorld::QuerySchoolArrival(Life->GetWorldState(), Content->Binding);
        return true;
    }

    virtual FScSignInResult SignIn() override
    {
        FScSignInResult Result;
        const UPWLaneLifeSubsystem* Reader = nullptr;
        FString Why;
        const FScSchoolContent* Content = Prepare(Reader, Why);
        if (Content == nullptr)
        {
            Result.Code = TEXT("school.missing_content");
            Result.Message = Why;
            return Result;
        }
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World.Get());
        TUniquePtr<PerfectWorld::WorldSim> Next;
        Result = ScSignInOnCopy(Life->GetWorldState(), Content->Binding, Next);
        if (Next.IsValid())
        {
            Life->AdoptSavedWorld(MoveTemp(Next));
        }
        return Result;
    }

    virtual const PerfectWorld::BellSchedule* GetBellSchedule() override
    {
        const UPWLaneLifeSubsystem* Life = nullptr;
        FString Why;
        const FScSchoolContent* Content = Prepare(Life, Why);
        return Content != nullptr ? Content->School : nullptr;
    }

private:
    /** The lane-life world and the bound content, or null with a reason. */
    const FScSchoolContent* Prepare(const UPWLaneLifeSubsystem*& OutLife, FString& OutWhy) const
    {
        UWorld* Live = World.Get();
        OutLife = Live != nullptr ? UPWLaneLifeSubsystem::Get(Live) : nullptr;
        if (OutLife == nullptr)
        {
            OutWhy = TEXT("there is no lane-life world");
            return nullptr;
        }
        return ScSharedSchoolContent(*Live, Stream, OutWhy);
    }

    TWeakObjectPtr<UWorld> World;
    FString Stream;
};

FString ScWhyUtf8(const std::string& Text)
{
    return PWLaneLife::ToFString(Text);
}
} // namespace

bool FScSchoolContent::Load(const PerfectWorld::ContentDb& Db, const FString& Stream, FString& OutWhy)
{
    Activities = PerfectWorld::LoadActivityCatalogue(Db);
    Bells = PerfectWorld::LoadBellSchedules(Db);
    Calendars = PerfectWorld::LoadSchoolCalendars(Db);
    School = nullptr;
    Calendar = nullptr;
    Binding = PerfectWorld::DayLoopCatalogues();
    if (!Activities.Ok())
    {
        OutWhy = TEXT("activity catalogue: ") + ScWhyUtf8(Activities.Error);
        return false;
    }
    if (!Bells.Ok())
    {
        OutWhy = TEXT("bell schedules: ") + ScWhyUtf8(Bells.Error);
        return false;
    }
    if (!Calendars.Ok())
    {
        OutWhy = TEXT("school calendars: ") + ScWhyUtf8(Calendars.Error);
        return false;
    }
    const FTCHARToUTF8 StreamUtf8(*Stream);
    School = Bells.Set.Find(PerfectWorld::NeighbourhoodSchoolInstitutionId, std::string_view(StreamUtf8.Get(), StreamUtf8.Length()));
    if (School == nullptr)
    {
        OutWhy = FString::Printf(TEXT("no bell schedule for Nabapara Vidyalaya, stream %s"), *Stream);
        return false;
    }
    Calendar = Calendars.Set.Find(School->InstitutionId, School->StageId);
    if (Calendar == nullptr)
    {
        School = nullptr;
        OutWhy = TEXT("no school calendar matches the bell schedule's institution and stage");
        return false;
    }
    Binding = PerfectWorld::DayLoopCatalogues{&Activities.Catalogue, School, Calendar};
    return true;
}

namespace ScGate
{
void Install(UWorld* World, TSharedPtr<IScGateWorld> Gate)
{
    if (World == nullptr)
    {
        return;
    }
    ScPruneRegistry();
    if (Gate.IsValid())
    {
        ScRegistry().Add(World, MoveTemp(Gate));
    }
    else
    {
        ScRegistry().Remove(World);
    }
}

void Remove(UWorld* World)
{
    if (World != nullptr)
    {
        ScRegistry().Remove(World);
    }
}

TSharedPtr<IScGateWorld> Resolve(UWorld* World)
{
    if (World == nullptr)
    {
        return nullptr;
    }
    if (const TSharedPtr<IScGateWorld>* Found = ScRegistry().Find(World))
    {
        return *Found;
    }
    ScPruneRegistry();
    TSharedPtr<IScGateWorld> Made = MakeLaneLifeGate(*World);
    ScRegistry().Add(World, Made);
    return Made;
}
} // namespace ScGate

TSharedRef<IScGateWorld> MakeLaneLifeGate(UWorld& World, const FString& Stream)
{
    return MakeShared<FScLaneLifeGate>(World, Stream);
}

/**
 * The content a gate binds to a world must outlive every WorldSim that borrows it (the core keeps pointers, not copies), so it is built
 * once per stream and kept for the life of the process. The records are immutable.
 */
const FScSchoolContent* ScSharedSchoolContent(UWorld& World, const FString& Stream, FString& OutWhy)
{
    static TMap<FString, TUniquePtr<FScSchoolContent>> Cache;
    if (const TUniquePtr<FScSchoolContent>* Found = Cache.Find(Stream))
    {
        return Found->Get();
    }
    UGameInstance* Instance = World.GetGameInstance();
    const UPWContentSubsystem* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    const PerfectWorld::ContentDb* Db = Content != nullptr ? Content->GetDb() : nullptr;
    if (Db == nullptr)
    {
        OutWhy = TEXT("the validated content database is not available");
        return nullptr;
    }
    TUniquePtr<FScSchoolContent> Loaded = MakeUnique<FScSchoolContent>();
    if (!Loaded->Load(*Db, Stream, OutWhy))
    {
        return nullptr;
    }
    return Cache.Add(Stream, MoveTemp(Loaded)).Get();
}

bool ScAdvanceLaneClock(UWorld& World, PerfectWorld::GameInstant Target, FString& OutWhy, const FString& Stream)
{
    UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&World);
    const FScSchoolContent* Content = ScSharedSchoolContent(World, Stream, OutWhy);
    if (Life == nullptr || Content == nullptr)
    {
        if (Life == nullptr)
        {
            OutWhy = TEXT("there is no lane-life world");
        }
        return false;
    }
    if (Life->GetNow() >= Target)
    {
        OutWhy = TEXT("the clock already stands at or after the target");
        return false;
    }
    if (!PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty() || Life->HasMovingDoors())
    {
        OutWhy = TEXT("the world is not at rest (a conversation is open or a door is moving)");
        return false;
    }
    TUniquePtr<PerfectWorld::WorldSim> Sim = MakeUnique<PerfectWorld::WorldSim>(Life->GetWorldState(), PerfectWorld::MaxEventsPerAdvance, Content->Binding);
    constexpr int32 MaxRounds = 64;
    const auto Settle = [&Sim]
    {
        for (int32 Round = 0; Round < MaxRounds && !PerfectWorld::IsWorldQuiescent(Sim->State()); ++Round)
        {
            if (!Sim->Submit(PerfectWorld::Command{PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}}).Accepted)
            {
                return;
            }
        }
    };
    Settle();
    for (int32 Round = 0; Round < MaxRounds && Sim->State().Clock.Now() < Target; ++Round)
    {
        PerfectWorld::AdvanceTime Advance;
        Advance.Minutes = Target - Sim->State().Clock.Now();
        const PerfectWorld::CommandResult Result = Sim->Submit(PerfectWorld::Command{PerfectWorld::CommandSource::System, Advance});
        if (!Result.Accepted)
        {
            OutWhy = TEXT("the core refused time.advance: ") + ScWhyUtf8(Result.Why.Code);
            return false;
        }
        Settle();
    }
    if (Sim->State().Clock.Now() != Target)
    {
        OutWhy = TEXT("the advance did not reach the target instant");
        return false;
    }
    Life->AdoptSavedWorld(MoveTemp(Sim));
    return true;
}

FScSignInResult ScSignInOnCopy(const PerfectWorld::WorldState& Live, const PerfectWorld::DayLoopCatalogues& Binding,
                               TUniquePtr<PerfectWorld::WorldSim>& OutNext)
{
    FScSignInResult Result;
    OutNext.Reset();
    TUniquePtr<PerfectWorld::WorldSim> Sim = MakeUnique<PerfectWorld::WorldSim>(Live, PerfectWorld::MaxEventsPerAdvance, Binding);
    const PerfectWorld::SchoolArrivalView Before = Sim->SchoolArrivalToday();
    if (Before.Refusal.Code == "dayloop.plan_first")
    {
        // The core wants today's plan before an arrival; an empty plan is allowed and free.
        const PerfectWorld::CommandResult Planned =
            Sim->Submit(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::SchoolPlanDay{}});
        if (!Planned.Accepted)
        {
            Result.Code = ScWhyUtf8(Planned.Why.Code);
            Result.Message = ScWhyUtf8(Planned.Why.Message);
            Result.View = Before;
            return Result;
        }
        Result.bPlannedDay = true;
    }
    const PerfectWorld::CommandResult Arrived =
        Sim->Submit(PerfectWorld::Command{PerfectWorld::CommandSource::Player, PerfectWorld::SchoolArrive{}});
    Result.View = Sim->SchoolArrivalToday();
    if (!Arrived.Accepted)
    {
        Result.Code = ScWhyUtf8(Arrived.Why.Code);
        Result.Message = ScWhyUtf8(Arrived.Why.Message);
        return Result;
    }
    Result.bAccepted = true;
    OutNext = MoveTemp(Sim);
    return Result;
}
} // namespace PWSchool
