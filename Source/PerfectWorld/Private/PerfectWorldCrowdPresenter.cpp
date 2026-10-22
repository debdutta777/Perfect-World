#include "PerfectWorldCrowdPresenter.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldCrowdState.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "UObject/ConstructorHelpers.h"

#include <cfloat>

DEFINE_LOG_CATEGORY_STATIC(LogR75PCrowd, Log, All);

namespace PWCrowd
{
APWCrowdPresenter* SpawnCrowdPresenter(UWorld& World)
{
    // Setup-only lookup prevents duplicate lane construction hooks. Never searched per frame.
    for (TActorIterator<APWCrowdPresenter> It(&World); It; ++It) return *It;
    auto* Life = UPWLaneLifeSubsystem::Get(&World);
    auto* Game = World.GetGameInstance();
    auto* Content = Game ? Game->GetSubsystem<UPWContentSubsystem>() : nullptr;
    const auto* Db = Content ? Content->GetDb() : nullptr;
    if (!Life || !Db)
    {
        UE_LOG(LogR75PCrowd, Error, TEXT("Crowd unavailable: loaded lane world/content missing."));
        return nullptr;
    }
    const auto Calendars = PerfectWorld::LoadSchoolCalendars(*Db);
    const auto* Calendar = Calendars.Ok() ? Calendars.Set.Find(
        PerfectWorld::NeighbourhoodSchoolInstitutionId, "stage.higher_secondary") : nullptr;
    if (!Calendar)
    {
        UE_LOG(LogR75PCrowd, Error, TEXT("Crowd unavailable: authoritative school calendar missing/invalid."));
        return nullptr;
    }
    // The school timetable is authoritative content: students converge on the gate from 15 minutes
    // before the gates open until the first bell, and leave from dismissal for 40 minutes.
    PerfectWorld::CrowdSchoolTiming Timing;
    const auto Bells = PerfectWorld::LoadBellSchedules(*Db);
    const auto* Bell = Bells.Ok() ? Bells.Set.Find(PerfectWorld::NeighbourhoodSchoolInstitutionId, "science") : nullptr;
    if (Bell) Timing = PerfectWorld::CrowdSchoolTimingFromBell(*Bell);
    else UE_LOG(LogR75PCrowd, Warning, TEXT("Crowd: bell_schedule.nabapara_xi_science missing; using its documented 09:15-10:00 / 15:50-16:30 windows."));
    auto* Presenter = World.SpawnActor<APWCrowdPresenter>();
    if (Presenter && !Presenter->Initialise(Life->GetWorldState().WorldSeed, *Calendar, Timing))
    {
        Presenter->Destroy();
        return nullptr;
    }
    return Presenter;
}
}

APWCrowdPresenter::APWCrowdPresenter()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("R75PCrowdRoot"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> R75PCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> R75PPaint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (R75PCube.Succeeded()) Cube = R75PCube.Object;
    if (R75PPaint.Succeeded()) BaseMaterial = R75PPaint.Object;
}

bool APWCrowdPresenter::Initialise(const uint64 Seed, const PerfectWorld::SchoolCalendar& Calendar,
    const PerfectWorld::CrowdSchoolTiming& Timing)
{
    if (Model) return false;
    WorldSeed = Seed;
    SchoolCalendar = Calendar;
    SchoolTiming = Timing;
    Model = MakeUnique<PerfectWorld::AmbientCrowd>(Seed, std::string(PerfectWorld::NeighbourhoodId),
        PerfectWorld::BuildNeighbourhoodPlan(Seed), Calendar, Timing);
    if (!Model->Problems().empty())
    {
        for (const auto& Problem : Model->Problems())
            UE_LOG(LogR75PCrowd, Error, TEXT("%s"), UTF8_TO_TCHAR(Problem.c_str()));
        Model.Reset();
        return false;
    }
    return true;
}

const PerfectWorld::CrowdRouteSlot* APWCrowdPresenter::Route(const uint32 Slot) const
{
    if (Model) for (const auto& Value : Model->RouteSlots()) if (Value.Slot == Slot) return &Value;
    return nullptr;
}

int32 APWCrowdPresenter::ActorCount() const
{
    int32 Count = 0;
    for (const auto& Pair : People) if (Pair.Value->Actor.IsValid()) ++Count;
    return Count;
}
int32 APWCrowdPresenter::FarFigureCount() const
{
    int32 Count = 0;
    for (const auto& Pair : People) if (!Pair.Value->Instances.IsEmpty()) ++Count;
    return Count;
}
int32 APWCrowdPresenter::CubeInstanceCount() const
{
    int32 Count = 0;
    for (const auto& Pair : People) Count += Pair.Value->Instances.Num();
    return Count;
}

bool APWCrowdPresenter::IsPointVisible(const FVector& Feet) const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (VisibilitySource) return VisibilitySource(Feet);
#endif
    const auto* World = GetWorld();
    const auto* Controller = World ? World->GetFirstPlayerController() : nullptr;
    if (!Controller) return true; // unknown is visible: never pop during view setup
    FVector Eye;
    FRotator Facing;
    Controller->GetPlayerViewPoint(Eye, Facing);
    if (FVector::DistSquared(Eye, Feet + FVector(0, 0, 90)) < FMath::Square(250.0)) return true;
    int32 Width = 0, Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return true;
    // Project an inflated human envelope, including the basket and both camera modes.
    TArray<FVector> Samples;
    Samples.Add(Feet + FVector(0, 0, 90));
    for (double X : {-55.0, 55.0}) for (double Y : {-55.0, 55.0}) for (double Z : {5.0, 190.0})
        Samples.Add(Feet + FVector(X, Y, Z));
    FVector2D Low(DBL_MAX, DBL_MAX), High(-DBL_MAX, -DBL_MAX);
    bool bProjected = false;
    for (const FVector& P : Samples)
    {
        FVector2D Screen;
        if (!Controller->ProjectWorldLocationToScreen(P, Screen)) continue;
        bProjected = true;
        Low.X = FMath::Min(Low.X, Screen.X); Low.Y = FMath::Min(Low.Y, Screen.Y);
        High.X = FMath::Max(High.X, Screen.X); High.Y = FMath::Max(High.Y, Screen.Y);
    }
    if (!bProjected || High.X < -32 || High.Y < -32 || Low.X > Width + 32 || Low.Y > Height + 32) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(R75PCrowdSight), false, this);
    Params.AddIgnoredActor(Controller->GetPawn());
    for (const auto& Pair : People) if (Pair.Value->Actor.IsValid()) Params.AddIgnoredActor(Pair.Value->Actor.Get());
    for (const auto& P : Samples)
        if (!World->LineTraceTestByChannel(Eye, P, ECC_Visibility, Params)) return true;
    return false;
}

void APWCrowdPresenter::RefreshNow()
{
    auto* Life = UPWLaneLifeSubsystem::Get(this);
    auto* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    auto* Player = Controller ? Controller->GetPawn() : nullptr;
    if (!Life || !Player || !Model) return;
    const auto Now = Life->GetNow();
    const bool bDiscontinuity = !bHaveNow || Now < PreviousNow || Now - PreviousNow > 2 ||
        RestoreToken != Life->GetRestorePresentationCount() || WorldSeed != Life->GetWorldState().WorldSeed;
    // Reconstruct from loaded world metadata, without touching persistent RNG.
    if (WorldSeed != Life->GetWorldState().WorldSeed)
    {
        WorldSeed = Life->GetWorldState().WorldSeed;
        Model = MakeUnique<PerfectWorld::AmbientCrowd>(WorldSeed, std::string(PerfectWorld::NeighbourhoodId),
            PerfectWorld::BuildNeighbourhoodPlan(WorldSeed), SchoolCalendar, SchoolTiming);
    }
    PerfectWorld::CrowdPresentationBudget Budget;
    // This bounded 2 Hz count includes named/legacy NPC actors, never crowd figures.
    for (TActorIterator<APWNpcCharacter> It(GetWorld()); It; ++It) ++Budget.NamedActorCount;
    const auto At = Player->GetActorLocation();
    const int32 Fraction = MinuteFractionSource ? FMath::Clamp(MinuteFractionSource(), 0, 999) : 0;
    SampleAt(Now, {FMath::RoundToInt(At.X), FMath::RoundToInt(At.Y), FMath::RoundToInt(At.Z)}, Budget, Fraction, bDiscontinuity);
    RestoreToken = Life->GetRestorePresentationCount();
}

void APWCrowdPresenter::SampleAt(const PerfectWorld::GameInstant Now, const PerfectWorld::NeighbourhoodPoint Centre,
    const PerfectWorld::CrowdPresentationBudget Budget, const int32 Fraction, const bool bReconstruct)
{
    if (!Model) return;
    const double Start = FPlatformTime::Seconds();
    const bool bDiscontinuity = bReconstruct || !bHaveNow || Now < PreviousNow || Now - PreviousNow > 2;
    Latest = Model->CrowdAt(Now, Centre, 10000, Budget, Fraction);
    // Consume the model's event interval only in continuous play; jumps reconstruct directly.
    if (bHaveNow && !bDiscontinuity && Now > PreviousNow)
    {
        auto Events = Model->EntranceEvents(PreviousNow, Now);
        for (const auto& Event : Events.Events)
            if (Event.Kind == PerfectWorld::CrowdEntranceEventKind::Exit)
                if (auto* Record = People.Find(UTF8_TO_TCHAR(Event.PersonId.c_str()))) (*Record)->bRetiring = true;
        // A two-minute local interval is below one page's cap (166 templates).
        if (!Events.Complete || !Events.Error.empty())
            UE_LOG(LogR75PCrowd, Warning, TEXT("Crowd entrance interval incomplete; snapshot reconciliation retained."));
    }
    Reconcile(Latest, bDiscontinuity);
    PreviousNow = Now;
    bHaveNow = true;
    LastUpdateMs = (FPlatformTime::Seconds() - Start) * 1000;
}

void APWCrowdPresenter::Reconcile(const PerfectWorld::CrowdSnapshot& View, const bool bDiscontinuity)
{
    TMap<FString, const PerfectWorld::CrowdPerson*> Wanted;
    int32 ActorLimit = 0;
    for (const auto& Person : View.People)
    {
        Wanted.Add(UTF8_TO_TCHAR(Person.Id.c_str()), &Person);
        if (Person.Lod == PerfectWorld::CrowdLod::FullActor) ++ActorLimit;
    }
    for (auto It = People.CreateIterator(); It; ++It)
    {
        auto& Record = *It.Value();
        if (Record.Actor.IsValid()) Record.Feet = Record.Actor->Feet();
        const auto* Found = Wanted.Find(It.Key());
        const bool bVisible = IsPointVisible(Record.Feet);
        if (!Found || (*Found)->Lod == PerfectWorld::CrowdLod::Hidden)
        {
            Record.bRetiring = true;
            Record.bFrozen = true;
            if (Record.Actor.IsValid()) Record.Actor->SetClockRunning(false);
            if (!bVisible)
            {
                if (Record.Actor.IsValid()) Record.Actor->Destroy();
                ReleaseFar(Record);
                It.RemoveCurrent();
            }
            continue;
        }
        // The shared mover's own snap threshold is 500 cm. Freeze before reaching it when either end is visible.
        if (bDiscontinuity || (Record.Actor.IsValid() &&
            FVector::Dist(Record.Feet, PWCrowd::Point((*Found)->Position)) > 400 &&
            (bVisible || IsPointVisible(PWCrowd::Point((*Found)->Position))))) Record.bFrozen = true;
        if (Record.bFrozen)
        {
            if (Record.Actor.IsValid()) Record.Actor->SetClockRunning(false);
            // No visible catch-up after load, sleep or clock mode jump. Both ends must be unseen.
            if (!PWCrowd::MayChangePresence(bVisible, IsPointVisible(PWCrowd::Point((*Found)->Position)))) continue;
            if (Record.Actor.IsValid()) Record.Actor->Destroy();
            ReleaseFar(Record);
            It.RemoveCurrent();
            continue;
        }
        Record.Target = **Found;
        Record.bRetiring = false;
        if (Record.Actor.IsValid())
        {
            // A far-ranked actor must release its slot for a nearer person. Demote
            // at its actual feet/pose even in view; no identity or position changes.
            if (Record.Target.Lod != PerfectWorld::CrowdLod::FullActor && !Record.Actor->IsTalkHeld()) Demote(Record);
            else if (const auto* Path = Route(Record.Target.Slot)) Record.Actor->Present(Record.Target, *Path);
        }
    }
    // Hard budget reduction (e.g. a named arrival) transfers the exact current pose to ISM.
    // Identity stays on screen; this is a representation swap, not a person despawn.
    int32 Count = ActorCount();
    for (auto& Pair : People)
        if (Count > ActorLimit && Pair.Value->Actor.IsValid() && !Pair.Value->Actor->IsTalkHeld())
        { Demote(*Pair.Value); --Count; }
    for (auto& Pair : People)
        if (Count > ActorLimit && Pair.Value->Actor.IsValid()) { Demote(*Pair.Value); --Count; }

    for (const auto& Person : View.People)
    {
        if (Person.Lod == PerfectWorld::CrowdLod::Hidden) continue;
        const FString Id(UTF8_TO_TCHAR(Person.Id.c_str()));
        auto* Existing = People.Find(Id);
        if (!Existing)
        {
            // Total transient records capped at 80 leaves enough ISM capacity for every hard demotion.
            if (People.Num() >= 80 || IsPointVisible(PWCrowd::Point(Person.Position))) continue;
            auto Record = MakeShared<FR75PCrowdRecord>();
            Record->Target = Person;
            Record->Look = PWCrowd::MakeLook(Person);
            Record->Feet = PWCrowd::Point(Person.Position);
            Record->Distance = double(Person.RouteDistanceCm);
            Record->Facing = PWCrowd::Point(Person.Heading).Rotation();
            People.Add(Id, Record);
            Existing = People.Find(Id);
        }
        auto& Record = **Existing;
        if (Record.bFrozen || Record.Actor.IsValid()) continue;
        if (Person.Lod == PerfectWorld::CrowdLod::FullActor && Count < ActorLimit && !IsPointVisible(Record.Feet))
        {
            Record.Actor = APWCrowdFigure::Spawn(*GetWorld(), Person, Record.Feet, Record.Facing);
            if (Record.Actor.IsValid())
            {
                ReleaseFar(Record);
                Record.Actor->ApplyPose(Record.Pose);
                if (const auto* Path = Route(Person.Slot)) Record.Actor->Present(Person, *Path);
                Record.Actor->SetClockRunning(false); // the presenter releases movement on its next tick
                ++Count;
            }
        }
        if (!Record.Actor.IsValid() && Record.Instances.IsEmpty()) MakeFar(Record);
    }
}

void APWCrowdPresenter::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Start = FPlatformTime::Seconds();
    UntilSample -= DeltaSeconds;
    if (UntilSample <= 0) { RefreshNow(); UntilSample = .5; }
    const auto* Life = UPWLaneLifeSubsystem::Get(this);
    const bool bRunning = Life && Life->GetClockHold() == EPWClockHold::None;
    for (auto& Pair : People)
        if (Pair.Value->Actor.IsValid()) Pair.Value->Actor->SetClockRunning(bRunning && !Pair.Value->bFrozen);
    AdvanceFar(DeltaSeconds, bRunning);
    LastFrameMs = (FPlatformTime::Seconds() - Start) * 1000;
#if !UE_BUILD_SHIPPING
    UntilLog -= DeltaSeconds;
    if (UntilLog <= 0)
    {
        UE_LOG(LogR75PCrowd, Display, TEXT("Crowd actors=%d far_figures=%d cube_instances=%d model_people=%llu update_ms=%.3f presenter_frame_ms=%.3f"),
            ActorCount(), FarFigureCount(), CubeInstanceCount(), static_cast<unsigned long long>(Latest.People.size()), LastUpdateMs, LastFrameMs);
        UntilLog = 5;
    }
#endif
}

TArray<APWCrowdPresenter::FObserved> APWCrowdPresenter::ObservedPeople() const
{
    TArray<FObserved> Result;
    for (const auto& Pair : People)
        Result.Add({Pair.Key, Pair.Value->Target.Cohort,
            Pair.Value->Actor.IsValid() ? Pair.Value->Actor->Feet() : Pair.Value->Feet, Pair.Value->Actor.IsValid()});
    Result.Sort([](const FObserved& A, const FObserved& B) { return A.Id < B.Id; });
    return Result;
}

APWCrowdFigure* APWCrowdPresenter::NearestStudent(const FVector& From) const
{
    APWCrowdFigure* Best = nullptr;
    double Distance = DBL_MAX;
    for (const auto& Pair : People)
    {
        auto* Figure = Pair.Value->Actor.Get();
        if (!Figure || Figure->Person().Cohort != PerfectWorld::CrowdCohort::Student ||
            !Figure->Person().ReceivesAvoidanceAndInteraction) continue;
        const double D = FVector::DistSquared2D(From, Figure->Feet());
        if (D < Distance) { Best = Figure; Distance = D; }
    }
    return Best;
}

void APWCrowdPresenter::EndPlay(const EEndPlayReason::Type Reason)
{
    for (auto& Pair : People) if (Pair.Value->Actor.IsValid()) Pair.Value->Actor->Destroy();
    People.Empty();
    Model.Reset();
    Super::EndPlay(Reason);
}
