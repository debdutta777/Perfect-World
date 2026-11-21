#include "PerfectWorldTrafficTests.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldAcceptanceTrafficR7.h"
#include "PerfectWorldTrafficPresenter.h"
#include "PerfectWorldTrafficVehicles.h"

#include <array>
#include <map>
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
struct R72PTestWorld
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    ~R72PTestWorld() { if (World) World->DestroyWorld(false); }
};

struct R72PPose
{
    std::string Id;
    FVector Location;
    PerfectWorld::NeighbourhoodVehicle Target;
};

std::vector<R72PPose> R72PCapture(const APWTrafficPresenter& Presenter)
{
    std::vector<R72PPose> Result;
    for (const auto& Slot : Presenter.GetSlots()) if (const auto* Vehicle = Slot.Actor.Get())
        Result.push_back({Slot.Target.Id, Vehicle->GetActorLocation(), Slot.Target});
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR72PVehicleBlocksTest, "PerfectWorld.Traffic.Round7.BlockVehicles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR72PVehicleBlocksTest::RunTest(const FString& Parameters)
{
    R72PTestWorld R72PWorld;
    if (!TestNotNull(TEXT("world"), R72PWorld.World)) return false;
    using PerfectWorld::NeighbourhoodVehicleKind;
    const std::array<NeighbourhoodVehicleKind, 5> R72PKinds{NeighbourhoodVehicleKind::YellowTaxi,
        NeighbourhoodVehicleKind::Bus, NeighbourhoodVehicleKind::SharedAuto,
        NeighbourhoodVehicleKind::CycleRickshaw, NeighbourhoodVehicleKind::FictionalTram};
    const int32 R72PLengths[]{440, 900, 280, 260, 1100}, R72PWidths[]{170, 250, 140, 110, 240};
    for (int32 I = 0; I < 5; ++I)
    {
        auto* R72PActor = R72PWorld.World->SpawnActor<APWTrafficVehicle>();
        if (!TestNotNull(TEXT("block vehicle"), R72PActor)) return false;
        PerfectWorld::NeighbourhoodVehicle R72PV;
        R72PV.Id = "R72P.fixture." + std::to_string(I);
        R72PV.Kind = R72PKinds[I]; R72PV.LengthCm = R72PLengths[I]; R72PV.WidthCm = R72PWidths[I];
        R72PActor->Build(R72PV);
        R72PActor->SetNight(true);
        TestEqual(TEXT("tram is two cars inside one actor"), R72PActor->GetCarCount(), I == 4 ? 2 : 1);
        TestEqual(TEXT("portable length preserved"), R72PActor->GetBody()->GetUnscaledBoxExtent().X, R72PLengths[I] / 2.0);
        TestEqual(TEXT("portable width preserved"), R72PActor->GetBody()->GetUnscaledBoxExtent().Y, R72PWidths[I] / 2.0);
        TestFalse(TEXT("no physics simulation"), R72PActor->GetBody()->IsSimulatingPhysics());
        TestFalse(TEXT("cannot carry a character as a moving base"), R72PActor->CanBeBaseForCharacter(nullptr));
        TestEqual(TEXT("blocks a walking capsule"), R72PActor->GetBody()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Block);
        TArray<UStaticMeshComponent*> R72PParts;
        R72PActor->GetComponents(R72PParts);
        TestTrue(TEXT("recognisable block construction"), R72PParts.Num() >= 12);
        for (const auto* R72PPart : R72PParts)
        {
            const FVector R72PHalf = R72PPart->GetRelativeScale3D() * 50;
            const FVector R72PCentre = R72PPart->GetRelativeLocation();
            TestTrue(TEXT("blocks stay inside model length"), FMath::Abs(R72PCentre.X) + R72PHalf.X <= R72PLengths[I] / 2.0 + 0.01);
            TestTrue(TEXT("blocks stay inside model width"), FMath::Abs(R72PCentre.Y) + R72PHalf.Y <= R72PWidths[I] / 2.0 + 0.01);
            TestEqual(TEXT("decoration cannot push"), R72PPart->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
        }
        R72PActor->Destroy();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR72PPresentationTest, "PerfectWorld.Traffic.Round7.CapStopsSmoothness",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR72PPresentationTest::RunTest(const FString& Parameters)
{
    R72PTestWorld R72PWorld;
    if (!TestNotNull(TEXT("world"), R72PWorld.World)) return false;
    auto* R72PBridge = PWLane::SpawnTrafficPresenter(*R72PWorld.World);
    if (!TestNotNull(TEXT("presenter"), R72PBridge)) return false;
    TestTrue(TEXT("one presenter per world"), R72PBridge == PWLane::SpawnTrafficPresenter(*R72PWorld.World));
    TestEqual(TEXT("campaign zero presents the 06:00 service band"), PWTraffic::FromCampaignTime(0).Minute, int64(360));
    TestEqual(TEXT("07:30 calendar matches the rush service band"), PWTraffic::FromCampaignTime(90).Minute, int64(450));
    TestEqual(TEXT("epoch offset saturates without overflow"),
        PWTraffic::FromCampaignTime(std::numeric_limits<int64>::max()).Minute, std::numeric_limits<int64>::max());
    R72PBridge->ResetTraffic(72, {420, 0});
    std::map<std::string, FVector> R72PLast;
    int32 R72PMoving = 0, R72PStopped = 0, R72PWalk = 0, R72PInterpolated = 0;
    for (int32 R72PFrame = 0; R72PFrame < 3600; ++R72PFrame)
    {
        R72PBridge->PresentFrame(0.05, FVector(8000, 4200, 0), {});
        TestTrue(TEXT("including retiring actors, never over cap"), R72PBridge->GetVehicleCount() <= 12);
        for (const auto& R72PSlot : R72PBridge->GetSlots())
        {
            const auto* R72PActor = R72PSlot.Actor.Get();
            if (!TestNotNull(TEXT("live actor"), R72PActor)) return false;
            const FVector R72PAt = R72PActor->GetActorLocation();
            const FVector R72PDir = PWTraffic::LaneDirection(R72PBridge->GetPlan(), R72PSlot.Target.LaneId);
            const auto* R72PLane = PerfectWorld::FindNeighbourhoodFeature(R72PBridge->GetPlan(), R72PSlot.Target.LaneId);
            if (!TestNotNull(TEXT("authored lane"), R72PLane)) return false;
            const FVector R72PGoal = PWTraffic::RoadPoint(R72PSlot.Target);
            TestTrue(TEXT("never extrapolates through a model stop"), FVector::DotProduct(R72PAt - R72PGoal, R72PDir) <= 0.01);
            const double R72PCross = FMath::Abs(FVector::CrossProduct(R72PAt - R72PGoal, R72PDir).Z);
            TestTrue(TEXT("whole body stays off footpaths"), R72PCross + R72PSlot.Target.WidthCm / 2.0 <= R72PLane->WidthCm / 2.0 + 0.01);
            if (R72PSlot.Target.State == PerfectWorld::NeighbourhoodVehicleState::StoppedAtStopLine)
            {
                ++R72PStopped;
                for (const auto& R72PFeature : R72PBridge->GetPlan().Features)
                {
                    if (R72PFeature.Kind != PerfectWorld::NeighbourhoodFeatureKind::StopLine ||
                        R72PFeature.ParentId != R72PSlot.Target.LaneId || R72PFeature.Geometry.empty()) continue;
                    const auto R72PLine = R72PFeature.Geometry[0];
                    const FVector R72PFront = R72PAt + R72PDir * (R72PSlot.Target.LengthCm / 2.0);
                    // Only the next stop line ahead of the model body applies.
                    if (FVector::DotProduct(FVector(R72PLine.X, R72PLine.Y, 0) - R72PGoal, R72PDir) >= 0)
                        TestTrue(TEXT("front remains upstream of supplied stop line"),
                            FVector::DotProduct(R72PFront - FVector(R72PLine.X, R72PLine.Y, R72PAt.Z), R72PDir) <= 0.01);
                }
            }
            const auto R72PPrevious = R72PLast.find(R72PSlot.Target.Id);
            if (R72PPrevious != R72PLast.end())
            {
                const double R72PDistance = FVector::Dist2D(R72PPrevious->second, R72PAt);
                TestTrue(TEXT("at most 700 cm/s; no clock catch-up sprint"), R72PDistance <= 35.01);
                if (R72PDistance > 0.01) ++R72PMoving;
            }
            if (FVector::Dist2D(R72PAt, R72PGoal) > 0.01) ++R72PInterpolated;
            R72PLast[R72PSlot.Target.Id] = R72PAt;
        }
        for (const char* R72PId : {"crossing.nabapara.main", "crossing.nabapara.para"})
            if (R72PBridge->CrossingPhase(R72PId) == PerfectWorld::NeighbourhoodCrossingPhase::Walk)
            { ++R72PWalk; TestTrue(TEXT("walk phase has no presented vehicle body"), R72PBridge->IsCrossingClear(R72PId)); }
    }
    TestTrue(TEXT("moving sample was exercised"), R72PMoving > 0);
    TestTrue(TEXT("stop-line sample was exercised"), R72PStopped > 0);
    TestTrue(TEXT("walk phase was exercised"), R72PWalk > 0);
    TestTrue(TEXT("positions interpolate between fixed steps"), R72PInterpolated > 0);
    const auto R72PBeforeHitch = R72PCapture(*R72PBridge);
    R72PBridge->PresentFrame(2.0, FVector(8000, 4200, 0), {});
    for (const auto& R72PPoseNow : R72PCapture(*R72PBridge))
        for (const auto& R72POld : R72PBeforeHitch) if (R72POld.Id == R72PPoseNow.Id)
            TestTrue(TEXT("hitch drops catch-up time; at most one step of visible travel"),
                FVector::Dist2D(R72POld.Location, R72PPoseNow.Location) <= 70.01);
    const auto R72PTimeBeforeInvalid = R72PBridge->GetPresentedTime();
    R72PBridge->PresentFrame(std::numeric_limits<double>::quiet_NaN(), FVector::ZeroVector, {});
    R72PBridge->PresentFrame(-1, FVector::ZeroVector, {});
    TestTrue(TEXT("invalid frame durations cannot advance traffic"), R72PTimeBeforeInvalid == R72PBridge->GetPresentedTime());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR72PPlayerSafetyTest, "PerfectWorld.Traffic.Round7.PlayerAndCrowdSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR72PPlayerSafetyTest::RunTest(const FString& Parameters)
{
    R72PTestWorld R72PWorld;
    if (!TestNotNull(TEXT("world"), R72PWorld.World)) return false;
    auto* R72PVehicle = R72PWorld.World->SpawnActor<APWTrafficVehicle>();
    auto* R72PPedestrian = R72PWorld.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("vehicle"), R72PVehicle) || !TestNotNull(TEXT("pedestrian"), R72PPedestrian)) return false;
    auto* R72PCapsule = NewObject<UCapsuleComponent>(R72PPedestrian);
    R72PCapsule->SetCapsuleSize(34, 82.5);
    R72PCapsule->SetCollisionObjectType(ECC_Pawn);
    R72PCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    R72PCapsule->SetCollisionResponseToAllChannels(ECR_Block);
    R72PPedestrian->SetRootComponent(R72PCapsule);
    R72PCapsule->RegisterComponent();
    R72PPedestrian->SetActorLocation(FVector(400, 0, -117.5));
    PerfectWorld::NeighbourhoodVehicle R72PV;
    R72PV.Id = "R72P.sweep"; R72PV.LengthCm = 440; R72PV.WidthCm = 170;
    R72PVehicle->Build(R72PV);
    R72PVehicle->SetActorLocation(R72PVehicle->CentreAt(FVector(0, 0, -200)));
    R72PVehicle->GetBody()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    const FVector R72PPlayerBefore = R72PPedestrian->GetActorLocation();
    const FVector R72PVehicleBefore = R72PVehicle->GetActorLocation();
    TestFalse(TEXT("long sweep cannot jump over a capsule"), R72PVehicle->Travel(R72PVehicleBefore + FVector(1000, 0, 0)));
    TestTrue(TEXT("vehicle waits before contact"), R72PVehicle->GetActorLocation().Equals(R72PVehicleBefore));
    TestTrue(TEXT("capsule never shoved"), R72PPedestrian->GetActorLocation().Equals(R72PPlayerBefore));
    TestFalse(TEXT("cannot materialise around the player"), R72PVehicle->CanPlace(R72PVehicle->CentreAt(FVector(400, 0, -200)), FQuat::Identity));
    for (const double R72PYaw : {90.0, 180.0, 270.0})
    {
        const FRotator R72PHeading(0, R72PYaw, 0);
        const FVector R72PDirection = R72PHeading.Vector();
        R72PVehicle->SetActorLocationAndRotation(R72PVehicleBefore, R72PHeading);
        const FVector R72PAt = R72PVehicleBefore + R72PDirection * 400;
        R72PPedestrian->SetActorLocation(FVector(R72PAt.X, R72PAt.Y, -117.5));
        const FVector R72PCapsuleBefore = R72PPedestrian->GetActorLocation();
        TestFalse(TEXT("every lane heading sweeps the long box against the capsule"),
            R72PVehicle->Travel(R72PVehicleBefore + R72PDirection * 1000));
        TestTrue(TEXT("every blocked sweep leaves vehicle still"), R72PVehicle->GetActorLocation().Equals(R72PVehicleBefore));
        TestTrue(TEXT("every blocked sweep leaves pedestrian still"), R72PPedestrian->GetActorLocation().Equals(R72PCapsuleBefore));
        TestFalse(TEXT("rotated spawn cannot enclose player"), R72PVehicle->CanPlace(
            R72PVehicle->CentreAt(FVector(R72PAt.X, R72PAt.Y, -200)), R72PHeading.Quaternion()));
    }
    R72PPedestrian->SetActorLocation(FVector(10000, 10000, -117.5));
    TestTrue(TEXT("obstruction release permits a short swept move"), R72PVehicle->Travel(R72PVehicleBefore + FVector(0, -20, 0)));
    R72PPedestrian->Destroy(); R72PVehicle->Destroy();

    auto* R72PBridge = PWLane::SpawnTrafficPresenter(*R72PWorld.World);
    if (!TestNotNull(TEXT("bridge"), R72PBridge)) return false;
    R72PBridge->ResetTraffic(72, {420, 0});
    const std::array<PerfectWorld::NeighbourhoodPedestrianOccupancy, 1> R72POccupancy{{{{15300, 5163, 16700, 5237}}}};
    for (int32 R72PFrame = 0; R72PFrame < 1200; ++R72PFrame)
    {
        R72PBridge->PresentFrame(0.05, FVector(16000, 5200, 0), R72POccupancy);
        TestTrue(TEXT("unrendered pedestrian crossing remains clear through drive and walk"),
            R72PBridge->IsCrossingClear("crossing.nabapara.main"));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR72PLateOccupancyTest, "PerfectWorld.Traffic.Round7.LateOccupancyAndRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR72PLateOccupancyTest::RunTest(const FString& Parameters)
{
    R72PTestWorld R72PWorld;
    if (!TestNotNull(TEXT("world"), R72PWorld.World)) return false;
    auto* R72PBridge = PWLane::SpawnTrafficPresenter(*R72PWorld.World);
    if (!TestNotNull(TEXT("presenter"), R72PBridge)) return false;
    R72PBridge->ResetTraffic(72, {450, 700});
    for (int32 R72PFrame = 0; R72PFrame < 400; ++R72PFrame)
    {
        R72PBridge->PresentFrame(0.025, FVector(8000, 4200, 0), {});
        for (const auto& R72PSlot : R72PBridge->GetSlots())
        {
            auto* R72PActor = R72PSlot.Actor.Get();
            if (!R72PActor || FVector::Dist2D(R72PActor->GetActorLocation(), R72PSlot.To) < 12) continue;
            const FVector R72PAt = R72PActor->GetActorLocation();
            const FVector R72PDirection = PWTraffic::LaneDirection(R72PBridge->GetPlan(), R72PSlot.Target.LaneId);
            const FVector R72PFront = R72PAt + R72PDirection * (R72PSlot.Target.LengthCm / 2.0 + 3);
            // A new one-cm crowd footprint arrives after the model already chose its next pose.
            const std::array<PerfectWorld::NeighbourhoodPedestrianOccupancy, 1> R72PThin{{{{
                FMath::FloorToInt(R72PFront.X), FMath::FloorToInt(R72PFront.Y),
                FMath::FloorToInt(R72PFront.X) + 1, FMath::FloorToInt(R72PFront.Y) + 1}}}};
            const auto R72PBefore = R72PBridge->GetPresentedTime();
            R72PBridge->PresentFrame(0.025, FVector(8000, 4200, 0), R72PThin);
            TestTrue(TEXT("render frequency occupancy stops a previously planned interpolation"), R72PBridge->IsCollisionHeld());
            TestTrue(TEXT("no travel through new crowd footprint"), R72PActor->GetActorLocation().Equals(R72PAt));
            TestTrue(TEXT("crossing phase clock holds with the stopped presentation"), R72PBefore == R72PBridge->GetPresentedTime());
            R72PBridge->PresentFrame(0.025, FVector(8000, 4200, 0), {});
            TestFalse(TEXT("clearing the obstruction resumes presentation"), R72PBridge->IsCollisionHeld());
            TestTrue(TEXT("release moves smoothly without catch-up"),
                FVector::Dist2D(R72PAt, R72PActor->GetActorLocation()) > 0 &&
                FVector::Dist2D(R72PAt, R72PActor->GetActorLocation()) <= 17.51);
            return true;
        }
    }
    AddError(TEXT("no moving half-segment exercised the late-occupancy seam"));
    return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR72PMovingCapsuleTest, "PerfectWorld.Traffic.Round7.MovingCapsuleSeparation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR72PMovingCapsuleTest::RunTest(const FString& Parameters)
{
    R72PTestWorld R72PWorld;
    if (!TestNotNull(TEXT("world"), R72PWorld.World)) return false;
    auto* R72PBridge = PWLane::SpawnTrafficPresenter(*R72PWorld.World);
    auto* R72PWalker = R72PWorld.World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("presenter"), R72PBridge) || !TestNotNull(TEXT("walker"), R72PWalker)) return false;
    auto* R72PCapsule = NewObject<UCapsuleComponent>(R72PWalker);
    R72PCapsule->SetCapsuleSize(34, 82.5);
    R72PCapsule->SetCollisionObjectType(ECC_Pawn);
    R72PCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    R72PCapsule->SetCollisionResponseToAllChannels(ECR_Block);
    R72PWalker->SetRootComponent(R72PCapsule);
    R72PCapsule->RegisterComponent();
    R72PWalker->SetActorLocation(FVector(15175, 5200, -117.5));
    R72PBridge->ResetTraffic(72, {480, 0});
    int32 R72PSamples = 0;
    for (int32 R72PFrame = 0; R72PFrame < 1200; ++R72PFrame)
    {
        const FVector R72PPrevious = R72PWalker->GetActorLocation();
        const int32 R72PTriangle = R72PFrame % 600;
        const double R72PX = 15175 + (R72PTriangle < 300 ? R72PTriangle : 600 - R72PTriangle) * 5.5;
        R72PWalker->SetActorLocation(FVector(R72PX, 5200, -117.5), true);
        const FVector R72PAt = R72PWalker->GetActorLocation();
        const std::array<PerfectWorld::NeighbourhoodPedestrianOccupancy, 1> R72PSwept{{{{
            FMath::FloorToInt(FMath::Min(R72PPrevious.X, R72PAt.X) - 37), 5163,
            FMath::CeilToInt(FMath::Max(R72PPrevious.X, R72PAt.X) + 37), 5237}}}};
        R72PBridge->PresentFrame(0.05, R72PAt, R72PSwept);
        TestTrue(TEXT("presenter never changes the walking capsule's position"), R72PWalker->GetActorLocation().Equals(R72PAt));
        for (const auto& R72PSlot : R72PBridge->GetSlots()) if (const auto* R72PActor = R72PSlot.Actor.Get())
        {
            ++R72PSamples;
            const auto& R72PB = R72PActor->GetBody()->Bounds;
            const double R72PDx = FMath::Max(0.0, FMath::Abs(R72PAt.X - R72PB.Origin.X) - R72PB.BoxExtent.X);
            const double R72PDy = FMath::Max(0.0, FMath::Abs(R72PAt.Y - R72PB.Origin.Y) - R72PB.BoxExtent.Y);
            TestTrue(TEXT("no presented vehicle overlaps moving player capsule"), R72PDx * R72PDx + R72PDy * R72PDy >= 34 * 34 - 0.01);
        }
    }
    TestTrue(TEXT("moving capsule assertion includes actual traffic"), R72PSamples > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR72PReloadTest, "PerfectWorld.Traffic.Round7.ReloadAndScenario",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR72PReloadTest::RunTest(const FString& Parameters)
{
    R72PTestWorld R72PWorld;
    if (!TestNotNull(TEXT("world"), R72PWorld.World)) return false;
    auto* R72PBridge = PWLane::SpawnTrafficPresenter(*R72PWorld.World);
    if (!TestNotNull(TEXT("bridge"), R72PBridge)) return false;
    std::vector<std::vector<R72PPose>> R72PExpected;
    const FVector R72PCentre(8000, 4200, 0);
    for (int32 R72PPass = 0; R72PPass < 2; ++R72PPass)
    {
        R72PBridge->ResetTraffic(72, {450, 700});
        for (int32 R72PFrame = 0; R72PFrame < 200; ++R72PFrame)
        {
            R72PBridge->PresentFrame(0.025, R72PCentre, {});
            const auto R72PActual = R72PCapture(*R72PBridge);
            if (R72PPass == 0) { R72PExpected.push_back(R72PActual); continue; }
            const auto& R72PWant = R72PExpected[R72PFrame];
            if (!TestTrue(TEXT("same actor count after reconstruction"), R72PActual.size() == R72PWant.size())) return false;
            for (std::size_t I = 0; I < R72PActual.size(); ++I)
            {
                TestTrue(TEXT("same transient identity and model stop state"), R72PActual[I].Target == R72PWant[I].Target);
                TestTrue(TEXT("same interpolated pose"), R72PActual[I].Location.Equals(R72PWant[I].Location, 0.001));
            }
        }
    }
    TArray<PWAcceptance::FStepDef> R72PSteps;
    PWAcceptance::AppendTrafficRound7Steps(R72PSteps);
    TSet<FString> R72PNames;
    for (const auto& R72PStep : R72PSteps)
    {
        TestFalse(TEXT("unique evidence name"), R72PNames.Contains(R72PStep.Name));
        R72PNames.Add(R72PStep.Name);
        TestTrue(TEXT("executable bounded scenario step"), static_cast<bool>(R72PStep.Run) && R72PStep.TimeoutSeconds > 0);
    }
    for (const TCHAR* R72PName : {TEXT("traffic_first_wait_walk"), TEXT("traffic_first_cross"), TEXT("traffic_first_held"),
        TEXT("traffic_third_wait_walk"), TEXT("traffic_third_cross"), TEXT("traffic_third_held")})
        TestTrue(TEXT("both-camera crossing evidence"), R72PNames.Contains(R72PName));
    return true;
}
#endif
