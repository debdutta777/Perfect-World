#include "PerfectWorldTrafficPresenter.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldTrafficVehicles.h"

#include <algorithm>
#include <cfloat>
#include <limits>

namespace
{
PerfectWorld::NeighbourhoodTrafficTime R72PNext(PerfectWorld::NeighbourhoodTrafficTime Now)
{
    Now.Millisecond += PerfectWorld::NeighbourhoodTrafficStepMs;
    if (Now.Millisecond >= 60000)
    {
        if (Now.Minute == std::numeric_limits<int64>::max()) return {Now.Minute, 59900};
        ++Now.Minute;
        Now.Millisecond -= 60000;
    }
    return Now;
}
}

namespace PWTraffic
{
PerfectWorld::NeighbourhoodTrafficTime FromCampaignTime(PerfectWorld::GameInstant Now)
{
    constexpr int64 Offset = PerfectWorld::CampaignStartHour * PerfectWorld::MinutesPerHour;
    Now = FMath::Max<int64>(0, Now);
    return {Now > std::numeric_limits<int64>::max() - Offset ? std::numeric_limits<int64>::max() : Now + Offset, 0};
}

FVector RoadPoint(const PerfectWorld::NeighbourhoodVehicle& Vehicle)
{
    return FVector(Vehicle.Position.X, Vehicle.Position.Y, Vehicle.Position.Z);
}
FVector LaneDirection(const PerfectWorld::NeighbourhoodPlan& Plan, std::string_view LaneId)
{
    const auto* Lane = PerfectWorld::FindNeighbourhoodFeature(Plan, LaneId);
    if (!Lane || Lane->Geometry.size() != 2) return FVector::ZeroVector;
    const auto A = Lane->Geometry[0], B = Lane->Geometry[1];
    return FVector(B.X - A.X, B.Y - A.Y, 0).GetSafeNormal();
}
APWTrafficVehicle* FindVehicle(UWorld* World, std::string_view Id)
{
    if (World) for (TActorIterator<APWTrafficVehicle> It(World); It; ++It)
        if (It->GetTrafficId() == Id) return *It;
    return nullptr;
}
}

APWTrafficPresenter::APWTrafficPresenter()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}

void APWTrafficPresenter::RemoveVehicles()
{
    for (auto& Slot : Slots) if (auto* Vehicle = Slot.Actor.Get()) Vehicle->Destroy();
    Slots.clear();
}

void APWTrafficPresenter::RemoveUnobservedRetirees()
{
    for (std::size_t I = Slots.size(); I-- > 0;)
    {
        auto* Actor = Slots[I].Actor.Get();
        if (!Actor || (Slots[I].bRetiring &&
            !IsObserved(Actor->GetActorLocation(), Actor->GetBody()->Bounds.BoxExtent, Actor)))
        {
            if (Actor) Actor->Destroy();
            Slots.erase(Slots.begin() + I);
        }
    }
}

void APWTrafficPresenter::ResetTraffic(uint64 Seed, PerfectWorld::NeighbourhoodTrafficTime Now)
{
    RemoveVehicles();
    LastPawnPositions.Reset();
    WorldSeed = Seed;
    Plan = PerfectWorld::BuildNeighbourhoodPlan(Seed);
    Model = MakeUnique<PerfectWorld::NeighbourhoodTraffic>(Plan, Seed);
    // First VehiclesAt performs reconstruction WITH occupancy filtering. Calling Reconstruct
    // here would bypass the core's first-frame filter on an identical-time query.
    Now.Minute = FMath::Max<int64>(0, Now.Minute);
    Now.Millisecond = FMath::Clamp(Now.Millisecond, 0, 59999) / 100 * 100;
    ModelTime = PresentedTime = Now;
    SegmentSeconds = PWTraffic::R72PStepSeconds;
    bCollisionHeld = false;
}

void APWTrafficPresenter::EndPlay(const EEndPlayReason::Type Reason)
{
    RemoveVehicles();
    Super::EndPlay(Reason);
}

void APWTrafficPresenter::GatherPedestrians(std::vector<PerfectWorld::NeighbourhoodPedestrianOccupancy>& Out)
{
    TMap<TWeakObjectPtr<APawn>, FVector> Current;
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* Pawn = *It;
        FVector Centre, Extent;
        Pawn->GetActorBounds(true, Centre, Extent);
        if (const auto* Capsule = Pawn->FindComponentByClass<UCapsuleComponent>())
        {
            Centre = Capsule->GetComponentLocation();
            Extent = FVector(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight());
        }
        if (Centre.Z - Extent.Z > PerfectWorld::NeighbourhoodRoadZCm + 300 || Centre.Z + Extent.Z < PerfectWorld::NeighbourhoodRoadZCm) continue;
        const TWeakObjectPtr<APawn> Key(Pawn);
        const auto* Previous = LastPawnPositions.Find(Key);
        const FVector From = Previous ? *Previous : Centre;
        const FVector Future = Centre + Pawn->GetVelocity() * PWTraffic::R72PStepSeconds;
        Current.Add(Key, Centre);
        const double Radius = FMath::Max(Extent.X, Extent.Y) + 3;
        Out.push_back({{
            FMath::FloorToInt(FMath::Min3(From.X, Centre.X, Future.X) - Radius),
            FMath::FloorToInt(FMath::Min3(From.Y, Centre.Y, Future.Y) - Radius),
            FMath::CeilToInt(FMath::Max3(From.X, Centre.X, Future.X) + Radius),
            FMath::CeilToInt(FMath::Max3(From.Y, Centre.Y, Future.Y) + Radius)}});
    }
    LastPawnPositions = MoveTemp(Current);
    if (AppendCrowdOccupancy) AppendCrowdOccupancy(Out);
}

void APWTrafficPresenter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto* Life = UPWLaneLifeSubsystem::Get(this);
    if (!Life) return;
    if (!Model || WorldSeed != Life->GetWorldState().WorldSeed || RestoreCount != Life->GetRestorePresentationCount())
    {
        RestoreCount = Life->GetRestorePresentationCount();
        ResetTraffic(Life->GetWorldState().WorldSeed, PWTraffic::FromCampaignTime(Life->GetNow()));
    }
    if (Life->GetClockHold() != EPWClockHold::None) return;
    const auto* Controller = GetWorld()->GetFirstPlayerController();
    const auto* Player = Controller ? Controller->GetPawn() : nullptr;
    if (!Player) return;
    std::vector<PerfectWorld::NeighbourhoodPedestrianOccupancy> Occupancy;
    GatherPedestrians(Occupancy);
    // Re-anchor only when BOTH corridors are unobserved. Visible metric motion never
    // chases the accelerated calendar; phases and crowd crossing permission use PresentedTime.
    const auto CalendarTime = PWTraffic::FromCampaignTime(Life->GetNow());
    if (FMath::Abs(CalendarTime.Minute - ModelTime.Minute) > 2 && !AreRoadsObserved())
        ResetTraffic(WorldSeed, CalendarTime);
    PresentFrame(DeltaSeconds, Player->GetActorLocation(), Occupancy);
    const int32 Hour = Life->GetMinuteOfDay() / 60;
    for (const auto& Slot : Slots) if (auto* Vehicle = Slot.Actor.Get()) Vehicle->SetNight(Hour < 5 || Hour >= 18);
}

bool APWTrafficPresenter::IsObserved(const FVector& Centre, const FVector& Extent, const AActor* Ignored) const
{
    const auto* Controller = GetWorld()->GetFirstPlayerController();
    if (!Controller) return false;
    FVector Eye;
    FRotator View;
    Controller->GetPlayerViewPoint(Eye, View);
    const FVector EyeOffset = Eye - Centre;
    if (FMath::Abs(EyeOffset.X) <= Extent.X && FMath::Abs(EyeOffset.Y) <= Extent.Y &&
        FMath::Abs(EyeOffset.Z) <= Extent.Z) return true;
    int32 Width = 0, Height = 0;
    Controller->GetViewportSize(Width, Height);
    if (Width <= 0 || Height <= 0) return true;
    FVector2D Low(DBL_MAX, DBL_MAX), High(-DBL_MAX, -DBL_MAX);
    TArray<FVector> Points;
    for (double X : {-1.0, 1.0}) for (double Y : {-1.0, 1.0}) for (double Z : {-1.0, 1.0})
    {
        const FVector Point = Centre + FVector(X * Extent.X, Y * Extent.Y, Z * Extent.Z);
        FVector2D Screen;
        if (!Controller->ProjectWorldLocationToScreen(Point, Screen)) continue;
        Low.X = FMath::Min(Low.X, Screen.X); Low.Y = FMath::Min(Low.Y, Screen.Y);
        High.X = FMath::Max(High.X, Screen.X); High.Y = FMath::Max(High.Y, Screen.Y);
        Points.Add(Point);
    }
    if (Points.IsEmpty() || High.X < -64 || High.Y < -64 || Low.X > Width + 64 || Low.Y > Height + 64) return false;
    Points.Add(Centre);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(R72PVisibility), false, this);
    Params.AddIgnoredActor(Controller->GetPawn());
    if (Ignored) Params.AddIgnoredActor(Ignored);
    for (const FVector& Point : Points)
    {
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Point, ECC_Visibility, Params)) return true;
    }
    return false;
}

bool APWTrafficPresenter::AreRoadsObserved() const
{
    for (const auto& Slot : Slots) if (const auto* Actor = Slot.Actor.Get())
        if (IsObserved(Actor->GetActorLocation(), Actor->GetBody()->Bounds.BoxExtent, Actor)) return true;
    for (const auto& Lane : Plan.Features)
    {
        if (Lane.Kind != PerfectWorld::NeighbourhoodFeatureKind::VehicleLane || Lane.Geometry.size() != 2 ||
            (Lane.StreetClass != PerfectWorld::NeighbourhoodStreetClass::MainRoad &&
             Lane.StreetClass != PerfectWorld::NeighbourhoodStreetClass::ParaRoad)) continue;
        const auto A = Lane.Geometry[0], B = Lane.Geometry[1];
        const FVector Centre((A.X + B.X) / 2.0, (A.Y + B.Y) / 2.0, -50);
        const FVector Extent(FMath::Max(FMath::Abs(B.X - A.X) / 2.0, Lane.WidthCm / 2.0),
            FMath::Max(FMath::Abs(B.Y - A.Y) / 2.0, Lane.WidthCm / 2.0), 150);
        if (IsObserved(Centre, Extent, nullptr)) return true;
    }
    return false;
}

bool APWTrafficPresenter::IsMapEdge(const PerfectWorld::NeighbourhoodVehicle& V) const
{
    const auto* Lane = PerfectWorld::FindNeighbourhoodFeature(Plan, V.LaneId);
    if (!Lane || Lane->Geometry.size() != 2) return false;
    const auto A = Lane->Geometry[0], B = Lane->Geometry[1];
    const int Length = FMath::Abs(B.X - A.X) + FMath::Abs(B.Y - A.Y);
    const bool bStart = V.AlongLaneCm <= V.LengthCm / 2 + 100;
    const bool bEnd = Length - V.AlongLaneCm <= V.LengthCm / 2 + 100;
    // The para's eastern endpoint is INSIDE the T, never a visible spawn/despawn edge.
    return (bStart && (Lane->StreetClass == PerfectWorld::NeighbourhoodStreetClass::MainRoad || A.X < 0)) ||
        (bEnd && (Lane->StreetClass == PerfectWorld::NeighbourhoodStreetClass::MainRoad || B.X < 0));
}

void APWTrafficPresenter::BeginSegment(const FVector& PlayerPosition, std::span<const PerfectWorld::NeighbourhoodPedestrianOccupancy> Occupancy)
{
    const PerfectWorld::NeighbourhoodPoint Centre{FMath::RoundToInt(PlayerPosition.X), FMath::RoundToInt(PlayerPosition.Y), -200};
    const auto NextTime = R72PNext(ModelTime);
    std::vector<PerfectWorld::NeighbourhoodPedestrianOccupancy> Blocked(Occupancy.begin(), Occupancy.end());
    // A para route ends inside the T. Keep a visible retired body stationary until it can
    // disappear unseen, and keep subsequent model traffic away from that physical body.
    for (const auto& Slot : Slots) if (Slot.bRetiring) if (const auto* Actor = Slot.Actor.Get())
    {
        const auto& B = Actor->GetBody()->Bounds;
        Blocked.push_back({{FMath::FloorToInt(B.Origin.X - B.BoxExtent.X - 2),
            FMath::FloorToInt(B.Origin.Y - B.BoxExtent.Y - 2),
            FMath::CeilToInt(B.Origin.X + B.BoxExtent.X + 2),
            FMath::CeilToInt(B.Origin.Y + B.BoxExtent.Y + 2)}});
    }
    const auto Vehicles = Model->VehiclesAt(NextTime, Centre, PWTraffic::R72PRadiusCm, Blocked);
    PresentedTime = ModelTime;
    ModelTime = NextTime;
    for (std::size_t I = Slots.size(); I-- > 0;)
    {
        auto& Slot = Slots[I];
        auto* Actor = Slot.Actor.Get();
        const auto Found = std::find_if(Vehicles.begin(), Vehicles.end(), [&](const auto& V) { return V.Id == Slot.Target.Id; });
        if (!Actor || (Found == Vehicles.end() && (IsMapEdge(Slot.Target) ||
            !IsObserved(Actor->GetActorLocation(), Actor->GetBody()->Bounds.BoxExtent, Actor))))
        {
            if (Actor) Actor->Destroy();
            Slots.erase(Slots.begin() + I);
            continue;
        }
        Slot.From = Actor->GetActorLocation();
        if (Found != Vehicles.end()) Slot.Target = *Found;
        Slot.bRetiring = Found == Vehicles.end();
        Slot.To = Slot.bRetiring ? Slot.From : Actor->CentreAt(PWTraffic::RoadPoint(Slot.Target));
    }
    for (const auto& V : Vehicles)
    {
        if (Slots.size() >= Model->ActorCapHint()) break;
        if (std::any_of(Slots.begin(), Slots.end(), [&](const auto& S) { return S.Target.Id == V.Id; })) continue;
        const FVector Direction = PWTraffic::LaneDirection(Plan, V.LaneId);
        const FVector Extent(V.LengthCm / 2.0, V.WidthCm / 2.0, 150);
        const FVector WorldExtent(FMath::Abs(Direction.X) * Extent.X + FMath::Abs(Direction.Y) * Extent.Y,
            FMath::Abs(Direction.Y) * Extent.X + FMath::Abs(Direction.X) * Extent.Y, Extent.Z);
        if (!IsMapEdge(V) && IsObserved(PWTraffic::RoadPoint(V) + FVector(0, 0, 150), WorldExtent, nullptr)) continue;
        auto* Actor = GetWorld()->SpawnActor<APWTrafficVehicle>();
        if (!Actor) continue;
        Actor->Build(V);
        const FVector At = Actor->CentreAt(PWTraffic::RoadPoint(V));
        const FQuat Heading = Direction.Rotation().Quaternion();
        if (!Actor->CanPlace(At, Heading)) { Actor->Destroy(); continue; }
        Actor->SetActorLocationAndRotation(At, Heading, false);
        Actor->GetBody()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Actor->SetActorHiddenInGame(false);
        Slots.push_back({Actor, V, At, At});
    }
}

void APWTrafficPresenter::PresentFrame(double DeltaSeconds, const FVector& PlayerPosition,
    std::span<const PerfectWorld::NeighbourhoodPedestrianOccupancy> Occupancy)
{
    if (!Model || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
    // Run even while a segment is collision-held, so looking away can release retirees.
    RemoveUnobservedRetirees();
    // A hitch slows presentation instead of skipping occupied space or producing a catch-up sprint.
    double Remaining = FMath::Min(DeltaSeconds, PWTraffic::R72PStepSeconds);
    bCollisionHeld = false;
    while (Remaining > 1.e-8)
    {
        if (SegmentSeconds >= PWTraffic::R72PStepSeconds - 1.e-8)
        {
            BeginSegment(PlayerPosition, Occupancy);
            SegmentSeconds = 0;
        }
        const double Used = FMath::Min(Remaining, PWTraffic::R72PStepSeconds - SegmentSeconds);
        const double Alpha = (SegmentSeconds + Used) / PWTraffic::R72PStepSeconds;
        for (const auto& Slot : Slots) if (const auto* Actor = Slot.Actor.Get())
        {
            const FVector At = FMath::Lerp(Slot.From, Slot.To, Alpha);
            const FVector Current = Actor->GetActorLocation();
            if (At.Equals(Current, 0.001)) continue;
            const FVector Extent = Actor->GetBody()->Bounds.BoxExtent;
            // Instanced crowd figures may have no collision primitive. Guard their supplied
            // rectangles at render frequency too, including a newly occupied crossing.
            for (const auto& Person : Occupancy)
            {
                const auto& B = Person.Bounds;
                if (B.X0 > B.X1 || B.Y0 > B.Y1) continue;
                if (FMath::Min(Current.X, At.X) - Extent.X - 1 <= B.X1 &&
                    FMath::Max(Current.X, At.X) + Extent.X + 1 >= B.X0 &&
                    FMath::Min(Current.Y, At.Y) - Extent.Y - 1 <= B.Y1 &&
                    FMath::Max(Current.Y, At.Y) + Extent.Y + 1 >= B.Y0)
                { bCollisionHeld = true; return; }
            }
            if (!At.Equals(Actor->GetActorLocation(), 0.001) && !Actor->CanTravel(At))
            { bCollisionHeld = true; return; }
        }
        for (const auto& Slot : Slots) if (auto* Actor = Slot.Actor.Get())
            if (!Actor->Travel(FMath::Lerp(Slot.From, Slot.To, Alpha))) { bCollisionHeld = true; return; }
        SegmentSeconds += Used;
        Remaining -= Used;
        if (SegmentSeconds >= PWTraffic::R72PStepSeconds - 1.e-8) PresentedTime = ModelTime;
    }
}

PerfectWorld::NeighbourhoodCrossingPhase APWTrafficPresenter::CrossingPhase(std::string_view Id) const
{
    return Model ? Model->CrossingPhaseAt(Id, PresentedTime) : PerfectWorld::NeighbourhoodCrossingPhase::Unknown;
}

bool APWTrafficPresenter::IsCrossingClear(std::string_view Id) const
{
    const auto* Crossing = PerfectWorld::FindNeighbourhoodFeature(Plan, Id);
    if (!Crossing) return false;
    for (const auto& Slot : Slots) if (const auto* Actor = Slot.Actor.Get())
    {
        const auto& B = Actor->GetBody()->Bounds;
        const auto& R = Crossing->Bounds;
        if (B.Origin.X - B.BoxExtent.X < R.X1 && B.Origin.X + B.BoxExtent.X > R.X0 &&
            B.Origin.Y - B.BoxExtent.Y < R.Y1 && B.Origin.Y + B.BoxExtent.Y > R.Y0) return false;
    }
    return true;
}

namespace PWLane
{
APWTrafficPresenter* SpawnTrafficPresenter(UWorld& World)
{
    for (TActorIterator<APWTrafficPresenter> It(&World); It; ++It) return *It;
    return World.SpawnActor<APWTrafficPresenter>();
}
}
