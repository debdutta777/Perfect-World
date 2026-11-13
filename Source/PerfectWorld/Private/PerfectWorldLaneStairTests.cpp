// Stair contract on an isolated voxel fixture (PerfectWorld.LaneStair.*). The portable core (LaneStairLayout.h) decides flights,
// landings, slab openings, guards and headroom volumes; this file turns a layout into voxel blocks and collision boxes in a
// bare scene (nothing of the lane is built, no production building is touched) and checks what a walking player meets:
//   Fixture: every collision riser is climbable by the character's step height, no collision box enters a headroom volume,
//            the upper body (above step height) and the eye probe fit over every tread and landing, guards are 100 cm.
//   Walk:    the real character movement walks up and down every fixture with the player's numbers (the engine's step-up,
//            capsule 34 x 165, eye 155), checking head and eye contact, snagging and falling every frame.
// The scale comes from PWPlayer, so measured values (LL-05) flow in by changing those constants. Run in the editor under
// "PerfectWorld.LaneStair". The packaged walk in both cameras is still to be done by a person.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Algo/Reverse.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LaneStairLayout.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldPlayerCharacter.h"

#include <algorithm>

namespace PWLaneStairTest
{
namespace LstCore = PerfectWorld::LaneStair;
using LstRect = PerfectWorld::LaneInterior::Rect;

constexpr double LstHair = 0.01;
constexpr double LstFrameSeconds = 1.0 / 60.0;

LstCore::StairScale LstScale()
{
    LstCore::StairScale S;
    S.BodyHeightCm = FMath::RoundToInt(PWPlayer::CapsuleHalfHeight * 2.0f);
    S.EyeHeightCm = FMath::RoundToInt(PWPlayer::EyeHeightAboveFloor);
    S.CapsuleRadiusCm = FMath::RoundToInt(PWPlayer::CapsuleRadius);
    S.MaxStepUpCm = FMath::RoundToInt(PWPlayer::MaxStepHeight);
    return S;
}

struct FLstCase
{
    const TCHAR* Name;
    LstCore::StairInput Input;
};

LstCore::StairInput LstInput(const char* Id, const LstCore::Arrangement Kind, const LstRect& Footprint, const int32 RiseCm)
{
    LstCore::StairInput In;
    In.StairId = Id;
    In.Kind = Kind;
    In.Footprint = Footprint;
    In.UpperFloorCm = In.LowerFloorCm + RiseCm;
    In.Scale = LstScale();
    return In;
}

TArray<FLstCase> LstCases()
{
    TArray<FLstCase> Cases;
    Cases.Add({TEXT("straight in the 130 passage"), LstInput("stair.fixture.passage", LstCore::Arrangement::Straight, LstRect{285, 415, 25, 675}, 300)});
    LstCore::StairInput Open = LstInput("stair.fixture.open", LstCore::Arrangement::Straight, LstRect{285, 415, 25, 675}, 300);
    Open.Walls = LstCore::Enclosure{false, false, false, false};
    Cases.Add({TEXT("straight, no walls"), Open});
    Cases.Add({TEXT("straight, 360 cm storey"), LstInput("stair.fixture.old", LstCore::Arrangement::Straight, LstRect{0, 140, 0, 760}, 360)});
    Cases.Add({TEXT("switchback"), LstInput("stair.fixture.switchback", LstCore::Arrangement::Switchback, LstRect{0, 212, 0, 450}, 300)});
    LstCore::StairInput Mirrored = LstInput("stair.fixture.mirrored", LstCore::Arrangement::Switchback, LstRect{0, 212, 0, 450}, 300);
    Mirrored.MirrorU = true;
    Cases.Add({TEXT("switchback, mirrored"), Mirrored});
    LstCore::StairInput Half = LstInput("stair.fixture.half", LstCore::Arrangement::Switchback, LstRect{0, 224, 0, 462}, 300);
    Half.Walls = LstCore::Enclosure{false, true, true, false};
    Cases.Add({TEXT("switchback, open side and end"), Half});
    LstCore::StairInput AlongU = LstInput("stair.fixture.along_u", LstCore::Arrangement::Switchback, LstRect{0, 500, 0, 224}, 300);
    AlongU.Direction = LstCore::Climb::PlusU;
    Cases.Add({TEXT("switchback climbing along U"), AlongU});
    return Cases;
}

struct FLstFrameCase
{
    const TCHAR* Name;
    FVector U;
    FVector V;
    double X;
    double Y;
};

const TArray<FLstFrameCase>& LstFrames()
{
    static const TArray<FLstFrameCase> Frames = {
        {TEXT("-Y side of the lane"), FVector(1, 0, 0), FVector(0, 1, 0), 20000.0, 0.0},
        {TEXT("+Y side of the lane"), FVector(-1, 0, 0), FVector(0, -1, 0), 20000.0, 3000.0},
        {TEXT("turned 90 degrees"), FVector(0, 1, 0), FVector(-1, 0, 0), -20000.0, 5000.0}};
    return Frames;
}

bool LstOverlaps(const FBox& A, const FBox& B)
{
    return A.Min.X < B.Max.X - LstHair && B.Min.X < A.Max.X - LstHair && A.Min.Y < B.Max.Y - LstHair && B.Min.Y < A.Max.Y - LstHair &&
           A.Min.Z < B.Max.Z - LstHair && B.Min.Z < A.Max.Z - LstHair;
}

/** The stair as voxel blocks plus the solid boxes that stand for it, in world space. */
struct FLstFixture
{
    PWLane::FLaneFrame Frame;
    LstCore::StairLayout Layout;
    PWLane::FLaneBlocks Blocks;
    TArray<FBox> Collision;
    TMap<int32, double> TopByRiser; // world Z of each walked surface after k risers, k = 0..RiserCount
    double LowerZ = 0.0;
    double UpperZ = 0.0;

    double ZOf(const int32 Mm) const { return LowerZ + Mm / 10.0; }
    FVector P(const double U, const double V, const double Z) const { return Frame.Pt(U, V, Z - PWLane::GroundZ); }
    FBox BoxOf(const LstRect& R, const double Z0, const double Z1) const
    {
        FBox Box(ForceInit);
        for (const int32 U : {R.U0, R.U1})
        {
            for (const int32 V : {R.V0, R.V1})
            {
                Box += P(U, V, Z0);
                Box += P(U, V, Z1);
            }
        }
        return Box;
    }
    FVector Centre(const LstRect& R, const double Z) const { return P(0.5 * (R.U0 + R.U1), 0.5 * (R.V0 + R.V1), Z); }

    void Solid(const LstRect& R, const double Z0, const double Z1, const PWLane::ELaneColour Colour)
    {
        const FBox Box = BoxOf(R, Z0, Z1);
        Blocks.Box(Colour, Box.Min, Box.Max);
        Blocks.PropCollision(Box.Min, Box.Max);
        Collision.Add(Box);
    }

    bool Build(const LstCore::StairInput& In, const PWLane::FLaneFrame& InFrame)
    {
        Frame = InFrame;
        Layout = LstCore::GenerateStairLayout(In);
        if (!Layout.Ok())
        {
            return false;
        }
        LowerZ = PWLane::GroundZ + Layout.LowerFloorCm;
        UpperZ = PWLane::GroundZ + Layout.UpperFloorCm;
        const LstRect& F = Layout.Footprint;
        Solid(LstRect{F.U0 - 25, F.U1 + 25, F.V0 - 25, F.V1 + 25}, LowerZ - 30.0, LowerZ, PWLane::ELaneColour::FloorTile);
        // Walls on the flagged edges, except the open approach edge; the room beyond is not part of the fixture.
        const LstCore::Climb D = Layout.Direction;
        const bool StartU0 = D == LstCore::Climb::PlusU;
        const bool StartU1 = D == LstCore::Climb::MinusU;
        const bool StartV0 = D == LstCore::Climb::PlusV;
        const bool StartV1 = D == LstCore::Climb::MinusV;
        const double WallTop = UpperZ + 300.0;
        if (Layout.Walls.U0 && !StartU0) { Solid(LstRect{F.U0 - 25, F.U0, F.V0 - 25, F.V1 + 25}, LowerZ, WallTop, PWLane::ELaneColour::WallCream); }
        if (Layout.Walls.U1 && !StartU1) { Solid(LstRect{F.U1, F.U1 + 25, F.V0 - 25, F.V1 + 25}, LowerZ, WallTop, PWLane::ELaneColour::WallCream); }
        if (Layout.Walls.V0 && !StartV0) { Solid(LstRect{F.U0 - 25, F.U1 + 25, F.V0 - 25, F.V0}, LowerZ, WallTop, PWLane::ELaneColour::WallCream); }
        if (Layout.Walls.V1 && !StartV1) { Solid(LstRect{F.U0 - 25, F.U1 + 25, F.V1, F.V1 + 25}, LowerZ, WallTop, PWLane::ELaneColour::WallCream); }
        TopByRiser.Add(0, LowerZ);
        TopByRiser.Add(Layout.RiserCount, UpperZ);
        for (const LstCore::Step& S : Layout.Steps)
        {
            Solid(S.Plan, LowerZ, ZOf(S.TopMm), PWLane::ELaneColour::Concrete);
            TopByRiser.Add(S.Riser, ZOf(S.TopMm));
        }
        for (const LstCore::Landing& L : Layout.Landings)
        {
            if (L.Solid)
            {
                Solid(L.Plan, LowerZ, ZOf(L.TopMm), PWLane::ELaneColour::Concrete);
                for (int32 K = 1; K < Layout.RiserCount; ++K)
                {
                    if (Layout.Levels[K] == L.TopMm) { TopByRiser.Add(K, ZOf(L.TopMm)); }
                }
            }
        }
        for (const LstCore::Guard& G : Layout.Guards)
        {
            Solid(G.Plan, ZOf(G.BaseMm), ZOf(G.TopMm), PWLane::ELaneColour::Rail);
        }
        // The upper slab is the footprint minus the openings, cut along every opening edge.
        TArray<int32> Us{F.U0, F.U1};
        TArray<int32> Vs{F.V0, F.V1};
        for (const LstCore::SlabOpening& O : Layout.Openings)
        {
            Us.AddUnique(O.Plan.U0);
            Us.AddUnique(O.Plan.U1);
            Vs.AddUnique(O.Plan.V0);
            Vs.AddUnique(O.Plan.V1);
        }
        Us.Sort();
        Vs.Sort();
        for (int32 I = 0; I + 1 < Us.Num(); ++I)
        {
            for (int32 J = 0; J + 1 < Vs.Num(); ++J)
            {
                const LstRect Cell{Us[I], Us[I + 1], Vs[J], Vs[J + 1]};
                if (!std::any_of(Layout.Openings.begin(), Layout.Openings.end(), [&](const LstCore::SlabOpening& O) { return O.Plan.Contains(Cell); }))
                {
                    Solid(Cell, ZOf(Layout.SlabUndersideMm), UpperZ, PWLane::ELaneColour::WallCream);
                }
            }
        }
        return true;
    }
};

PWLane::FLaneFrame LstMakeFrame(const FLstFrameCase& Case) { return PWLane::MakeLaneFrame(Case.X, Case.Y, Case.U, Case.V); }

bool LstHits(const TArray<FBox>& Boxes, const FBox& Query)
{
    return Boxes.ContainsByPredicate([&](const FBox& Box) { return LstOverlaps(Box, Query); });
}

/** The standing points the fixtures are checked at: every tread, every landing the player stands on. */
struct FLstStand
{
    FVector Feet;
    FString Name;
};

TArray<FLstStand> LstStands(const FLstFixture& Fx)
{
    TArray<FLstStand> Out;
    for (const LstCore::Step& S : Fx.Layout.Steps) { Out.Add({Fx.Centre(S.Plan, Fx.ZOf(S.TopMm)), FString(UTF8_TO_TCHAR(S.Id.c_str()))}); }
    for (const LstCore::Landing& L : Fx.Layout.Landings) { Out.Add({Fx.Centre(L.Plan, Fx.ZOf(L.TopMm)), FString(UTF8_TO_TCHAR(L.Id.c_str()))}); }
    return Out;
}

void LstCheckFixture(FAutomationTestBase& T, const FString& Label, const FLstFixture& Fx)
{
    const double Radius = PWPlayer::CapsuleRadius;
    const double Body = PWPlayer::CapsuleHalfHeight * 2.0;
    const double StepUp = PWPlayer::MaxStepHeight;
    T.TestEqual(*(Label + TEXT(": the contract validates")), static_cast<int32>(LstCore::ValidateStairLayout(Fx.Layout)), 0);

    // Collision steps: every riser the player meets, from the lower floor to the upper slab top, is within the engine's step-up height.
    bool Complete = true;
    for (int32 K = 0; K <= Fx.Layout.RiserCount; ++K) { Complete = Complete && Fx.TopByRiser.Contains(K); }
    T.TestTrue(*(Label + TEXT(": a walked surface after every riser")), Complete);
    if (Complete)
    {
        double Largest = 0.0;
        for (int32 K = 1; K <= Fx.Layout.RiserCount; ++K) { Largest = FMath::Max(Largest, Fx.TopByRiser[K] - Fx.TopByRiser[K - 1]); }
        T.TestTrue(*FString::Printf(TEXT("%s: largest collision riser %.2f cm is under the %.0f cm step-up"), *Label, Largest, StepUp), Largest > 0.0 && Largest <= StepUp - 5.0);
        T.TestTrue(*(Label + TEXT(": the last riser lands on the upper slab top")), FMath::IsNearlyEqual(Fx.TopByRiser[Fx.Layout.RiserCount], Fx.UpperZ, 0.01));
    }

    // Headroom volumes are free of every collision box (slab, guards, walls, treads).
    for (const LstCore::Clearance& C : Fx.Layout.Clearances)
    {
        const FBox Volume = Fx.BoxOf(C.Plan, Fx.ZOf(C.BottomMm), Fx.ZOf(C.TopMm));
        T.TestFalse(*FString::Printf(TEXT("%hs: nothing intrudes on headroom volume of %s"), C.Id.c_str(), *Label), LstHits(Fx.Collision, Volume));
    }

    // The player's own upper body (above the step-up height) and the eye probe fit at every standing point.
    for (const FLstStand& S : LstStands(Fx))
    {
        const FBox Upper(S.Feet + FVector(-Radius, -Radius, StepUp), S.Feet + FVector(Radius, Radius, Body));
        T.TestFalse(*FString::Printf(TEXT("%s: %s, upper body clear"), *Label, *S.Name), LstHits(Fx.Collision, Upper));
        const FVector Eye = S.Feet + FVector(0.0, 0.0, PWPlayer::EyeHeightAboveFloor);
        const FBox Probe(Eye - FVector(PWPlayer::BoomProbeSize), Eye + FVector(PWPlayer::BoomProbeSize));
        T.TestFalse(*FString::Printf(TEXT("%s: %s, eye probe clear"), *Label, *S.Name), LstHits(Fx.Collision, Probe));
    }

    // Guards are the contract's protective height; the voxel blocks hold exactly the collision list.
    for (const LstCore::Guard& G : Fx.Layout.Guards)
    {
        T.TestTrue(*FString::Printf(TEXT("%hs: guard is at least %d cm"), G.Id.c_str(), Fx.Layout.Scale.BarrierHeightCm), G.TopMm - G.BaseMm >= Fx.Layout.Scale.BarrierHeightCm * 10);
    }
#if !UE_BUILD_SHIPPING
    T.TestEqual(*(Label + TEXT(": every collision box is emitted as a blocking block")), Fx.Blocks.BlockingBoxes().Num(), Fx.Collision.Num());
#endif
}

/** A throw-away game world holding the fixture's collision as pawn-blocking boxes. */
struct FLstWorld
{
    UWorld* World = nullptr;
    FLstWorld() { World = UWorld::CreateWorld(EWorldType::Game, false); }
    ~FLstWorld()
    {
        if (World != nullptr) { World->DestroyWorld(false); }
    }
    FLstWorld(const FLstWorld&) = delete;
    FLstWorld& operator=(const FLstWorld&) = delete;

    void Box(const FBox& Box)
    {
        AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity);
        UBoxComponent* Shape = NewObject<UBoxComponent>(Actor, TEXT("Solid"));
        Shape->SetMobility(EComponentMobility::Movable);
        Shape->SetBoxExtent(Box.GetExtent());
        Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Shape->SetCollisionObjectType(ECC_WorldStatic);
        Shape->SetCollisionResponseToAllChannels(ECR_Block);
        Actor->SetRootComponent(Shape);
        Shape->RegisterComponent();
        Actor->SetActorLocation(Box.GetCenter());
    }
};

/** The player's walking line through the stair, in plan (building frame) points from the foot to the head. */
TArray<FVector2D> LstPath(const LstCore::StairLayout& L)
{
    const auto Centre = [](const LstRect& R) { return FVector2D(0.5 * (R.U0 + R.U1), 0.5 * (R.V0 + R.V1)); };
    const LstCore::Landing* Bottom = nullptr;
    const LstCore::Landing* Top = nullptr;
    for (const LstCore::Landing& X : L.Landings)
    {
        if (X.Kind == LstCore::LandingKind::Bottom) { Bottom = &X; }
        if (X.Kind == LstCore::LandingKind::Top) { Top = &X; }
    }
    TArray<FVector2D> Path;
    if (Bottom == nullptr || Top == nullptr) { return Path; }
    Path.Add(Centre(Bottom->Plan));
    if (L.Flights.size() == 2)
    {
        const LstCore::Flight& F1 = L.Flights[0];
        const FVector2D Dir(F1.DirU, F1.DirV);
        const double Length = F1.DirU != 0 ? F1.Plan.Width() : F1.Plan.Depth();
        const FVector2D Turn1 = Centre(F1.Plan) + Dir * (0.5 * Length + 60.0);
        FVector2D Across = Centre(L.Flights[1].Plan) - Centre(F1.Plan);
        Across -= Dir * FVector2D::DotProduct(Across, Dir);
        Path.Add(Turn1);
        Path.Add(Turn1 + Across);
    }
    Path.Add(Centre(Top->Plan));
    return Path;
}

struct FLstWalkResult
{
    FString Failure;
    double MinHeadGap = 1.0e9;
    double MinFeetZ = 1.0e9;
    double EndFeetError = 0.0;
    int32 Frames = 0;
};

FLstWalkResult LstWalk(const FLstFixture& Fx, const bool bUp)
{
    FLstWalkResult R;
    FLstWorld Fixture;
    if (Fixture.World == nullptr) { R.Failure = TEXT("no test world"); return R; }
    for (const FBox& Box : Fx.Collision) { Fixture.Box(Box); }

    TArray<FVector2D> Plan = LstPath(Fx.Layout);
    if (!bUp) { Algo::Reverse(Plan); }
    if (Plan.Num() < 2) { R.Failure = TEXT("no path"); return R; }
    const double StartZ = bUp ? Fx.LowerZ : Fx.UpperZ;
    const double EndZ = bUp ? Fx.UpperZ : Fx.LowerZ;
    const double Half = PWPlayer::CapsuleHalfHeight;
    const double Radius = PWPlayer::CapsuleRadius;
    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Start = Fx.P(Plan[0].X, Plan[0].Y, StartZ + Half + 0.5);
    APerfectWorldPlayerCharacter* Player = Fixture.World->SpawnActor<APerfectWorldPlayerCharacter>(APerfectWorldPlayerCharacter::StaticClass(), FTransform(FRotator::ZeroRotator, Start), Spawn);
    if (Player == nullptr) { R.Failure = TEXT("player did not spawn"); return R; }
    UCharacterMovementComponent* Body = Player->GetCharacterMovement();
    Body->bRunPhysicsWithNoController = true;
    Body->SetMovementMode(MOVE_Walking);

    int32 Next = 1;
    double Best = 1.0e9;
    int32 SinceProgress = 0;
    for (; R.Frames < 20000 && Next < Plan.Num(); ++R.Frames)
    {
        const FVector Target = Fx.P(Plan[Next].X, Plan[Next].Y, 0.0);
        const FVector Here = Player->GetActorLocation();
        FVector To = Target - Here;
        To.Z = 0.0;
        const double Distance = To.Size2D();
        if (Distance < 12.0) { ++Next; Best = 1.0e9; SinceProgress = 0; continue; }
        if (Distance < Best - 2.0) { Best = Distance; SinceProgress = 0; }
        else if (++SinceProgress > 360) { R.Failure = FString::Printf(TEXT("snagged %.0f cm short of waypoint %d"), Distance, Next); break; }
        Player->AddMovementInput(To.GetSafeNormal2D(), 1.0f);
        Body->TickComponent(static_cast<float>(LstFrameSeconds), LEVELTICK_All, &Body->PrimaryComponentTick);

        const FVector Centre = Player->GetActorLocation();
        const double Feet = Centre.Z - Half;
        R.MinFeetZ = FMath::Min(R.MinFeetZ, Feet);
        if (Feet < Fx.LowerZ - 2.0) { R.Failure = TEXT("fell through the floor"); break; }
        const FBox Upper(Centre + FVector(-Radius + 0.5, -Radius + 0.5, -Half + PWPlayer::MaxStepHeight), Centre + FVector(Radius - 0.5, Radius - 0.5, Half - 0.5));
        if (LstHits(Fx.Collision, Upper)) { R.Failure = FString::Printf(TEXT("head or body contact at frame %d"), R.Frames); break; }
        const FVector Eye(Centre.X, Centre.Y, Feet + PWPlayer::EyeHeightAboveFloor);
        if (LstHits(Fx.Collision, FBox(Eye - FVector(PWPlayer::BoomProbeSize), Eye + FVector(PWPlayer::BoomProbeSize)))) { R.Failure = TEXT("eye probe inside geometry"); break; }
        for (const FBox& Box : Fx.Collision)
        {
            if (Box.Min.X < Centre.X + Radius && Box.Max.X > Centre.X - Radius && Box.Min.Y < Centre.Y + Radius && Box.Max.Y > Centre.Y - Radius && Box.Min.Z >= Centre.Z + Half - 0.5)
            {
                R.MinHeadGap = FMath::Min(R.MinHeadGap, Box.Min.Z - (Centre.Z + Half));
            }
        }
    }
    if (R.Failure.IsEmpty() && Next < Plan.Num()) { R.Failure = TEXT("did not reach the end of the stair"); }
    R.EndFeetError = FMath::Abs(Player->GetActorLocation().Z - Half - EndZ);
    return R;
}
} // namespace PWLaneStairTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWLaneStairFixtureTest, "PerfectWorld.LaneStair.Fixture", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWLaneStairFixtureTest::RunTest(const FString& Parameters)
{
    using namespace PWLaneStairTest;
    for (const FLstCase& Case : LstCases())
    {
        for (const FLstFrameCase& FrameCase : LstFrames())
        {
            const FString Label = FString::Printf(TEXT("%s, %s"), Case.Name, FrameCase.Name);
            FLstFixture Fx;
            if (!TestTrue(*(Label + TEXT(": the layout is accepted")), Fx.Build(Case.Input, LstMakeFrame(FrameCase))))
            {
                continue;
            }
            LstCheckFixture(*this, Label, Fx);
        }
    }
    // A footprint that cannot fit is refused, never built.
    FLstFixture Refused;
    LstCore::StairInput Short = LstInput("stair.fixture.short", LstCore::Arrangement::Straight, LstRect{0, 130, 0, 500}, 300);
    TestFalse(TEXT("a 500 cm footprint is refused for a 300 cm storey"), Refused.Build(Short, LstMakeFrame(LstFrames()[0])));
    TestEqual(TEXT("with the footprint-too-short code"), static_cast<int32>(Refused.Layout.Status), static_cast<int32>(LstCore::Refusal::FootprintTooShort));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWLaneStairWalkTest, "PerfectWorld.LaneStair.Walk", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWLaneStairWalkTest::RunTest(const FString& Parameters)
{
    using namespace PWLaneStairTest;
    for (const FLstCase& Case : LstCases())
    {
        for (int32 FrameIndex = 0; FrameIndex < LstFrames().Num(); FrameIndex += 2) // the lane's -Y side and the 90 degree turn
        {
            const FLstFrameCase& FrameCase = LstFrames()[FrameIndex];
            FLstFixture Fx;
            if (!TestTrue(*FString::Printf(TEXT("%s, %s: the layout is accepted"), Case.Name, FrameCase.Name), Fx.Build(Case.Input, LstMakeFrame(FrameCase))))
            {
                continue;
            }
            for (const bool bUp : {true, false})
            {
                const FString Label = FString::Printf(TEXT("%s, %s, walking %s"), Case.Name, FrameCase.Name, bUp ? TEXT("up") : TEXT("down"));
                const FLstWalkResult R = LstWalk(Fx, bUp);
                TestTrue(*FString::Printf(TEXT("%s: %s"), *Label, R.Failure.IsEmpty() ? TEXT("walked it") : *R.Failure), R.Failure.IsEmpty());
                if (R.Failure.IsEmpty())
                {
                    TestTrue(*FString::Printf(TEXT("%s: ends on the floor (error %.2f cm)"), *Label, R.EndFeetError), R.EndFeetError < 3.0);
                    TestTrue(*FString::Printf(TEXT("%s: never closer than 25 cm to the slab above (%.1f cm)"), *Label, R.MinHeadGap), R.MinHeadGap > 25.0);
                    AddInfo(FString::Printf(TEXT("%s: %d frames, least head gap %.1f cm"), *Label, R.Frames, R.MinHeadGap));
                }
            }
        }
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
