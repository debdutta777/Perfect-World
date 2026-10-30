// NL-04: the lane doors hung on the kit frontages. Every catalogue row builds its front door (and tall_flats a leaf to each flat);
// each door spec must carry the authored id, the authored two-way flag, a hinge on the doorway's jamb, a closed leaf that spans
// the doorway (along U in a front wall, along V in a partition) and an inside sign that points into the dwelling or passage, which
// is the side UPWLaneLifeSubsystem registers as "inside". The flat doors' sweeps must stay clear of collision.
// Run in the editor under "PerfectWorld.KitDoors".

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Doors.h"
#include "LaneFrontageCatalogue.h"
#include "PerfectWorldLaneInteriorKit.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneProps.h"

namespace PWKitDoorTest
{
using namespace PWLane;

/** A first-row building exactly as BuildFirstRows makes it (the frame is the generator's). */
FLaneBuilding KdBuilding(const LI::FrontageEntry& Row)
{
    FLaneBuilding B;
    B.Width = Row.WidthCm;
    B.Depth = Row.DepthCm;
    B.Setback = Row.SetbackCm;
    B.Storeys = Row.Storeys;
    B.Side = Row.Side;
    B.Index = Row.RowIndex;
    B.Seed = Row.Seed;
    B.Detail = ELaneDetail::Full;
    B.Wall = ELaneColour::WallCream;
    B.Frame = MakeLaneFrame(Row.OriginXCm, Row.Side * (FootpathOuter + Row.SetbackCm), FVector(1.0, 0.0, 0.0), FVector(0.0, static_cast<double>(Row.Side), 0.0));
    return B;
}

FBox KdWorldBox(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Rect& R, double H0, double H1)
{
    const double Floor = static_cast<double>(L.FinishedFloorCm);
    FBox Box(ForceInit);
    Box += B.Frame.Pt(R.U0, R.V0, Floor + H0);
    Box += B.Frame.Pt(R.U1, R.V1, Floor + H1);
    return Box;
}

bool KdAnyBlocking(const TArray<FBox>& Blocking, const FBox& Space)
{
    for (const FBox& Box : Blocking)
    {
        if (Box.Intersect(Space.ExpandBy(FVector(-0.5, -0.5, 0.0))))
        {
            return true;
        }
    }
    return false;
}

/** The world direction from the closed leaf's own frame: the actor's local +Y (the closed direction turned +90 degrees of yaw). */
FVector KdLocalY(const FLaneDoorSpec& Spec)
{
    const double Yaw = FMath::DegreesToRadians(Spec.ClosedYaw);
    return FVector(-FMath::Sin(Yaw), FMath::Cos(Yaw), 0.0);
}

FVector KdClosedDir(const FLaneDoorSpec& Spec)
{
    const double Yaw = FMath::DegreesToRadians(Spec.ClosedYaw);
    return FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.0);
}

const FLaneDoorSpec* KdFind(const FLaneScene& Scene, const FName Id)
{
    return Scene.Doors.FindByPredicate([&](const FLaneDoorSpec& Spec) { return Spec.Id == Id; });
}
} // namespace PWKitDoorTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitDoorsTest, "PerfectWorld.KitDoors.Specs", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitDoorsTest::RunTest(const FString& Parameters)
{
    using namespace PWKitDoorTest;
    int32 Rows = 0;
    for (const LI::FrontageEntry& Row : LI::KitFrontageEntries())
    {
        ++Rows;
        const FString Name = UTF8_TO_TCHAR(std::string(Row.BuildingId).c_str());
        const FLaneBuilding B = KdBuilding(Row);
        FLaneKitFrontage Frontage;
        if (!IsKitFrontage(B, Frontage))
        {
            AddError(FString::Printf(TEXT("%s: not recognised"), *Name));
            continue;
        }
        const LI::InteriorLayout Layout = MakeKitFrontageLayout(B, Frontage);
        if (!Layout.Ok())
        {
            AddError(FString::Printf(TEXT("%s: layout refused"), *Name));
            continue;
        }
        FLaneScene Scene;
        BuildKitFrontage(Scene, B, Frontage);
        const TArray<FBox>& Blocking = Scene.Blocks.BlockingBoxes();

        // The door set: the front door, plus a leaf to each flat of the shared entry; nothing else.
        TArray<FName> Expected;
        Expected.Add(FName(UTF8_TO_TCHAR(std::string(Row.EntranceDoorId).c_str())));
        if (Row.Kind == LI::Variant::SharedEntry)
        {
            Expected.Add(FName(TEXT("door.nabapara.tall_flats_unit_a")));
            Expected.Add(FName(TEXT("door.nabapara.tall_flats_unit_b")));
        }
        TestEqual(*FString::Printf(TEXT("%s: door spec count"), *Name), Scene.Doors.Num(), Expected.Num());
        TestEqual(*FString::Printf(TEXT("%s: the front door is the planned id"), *Name), Expected[0], Frontage.PlannedDoorId);

        for (const FName Id : Expected)
        {
            const FLaneDoorSpec* Spec = KdFind(Scene, Id);
            if (Spec == nullptr)
            {
                AddError(FString::Printf(TEXT("%s: no door spec %s"), *Name, *Id.ToString()));
                continue;
            }
            const std::string IdText(TCHAR_TO_UTF8(*Id.ToString()));
            const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(IdText);
            TestNotNull(*FString::Printf(TEXT("%s: %s is an authored door"), *Name, *Id.ToString()), Def);
            if (Def == nullptr)
            {
                continue;
            }
            TestEqual(*FString::Printf(TEXT("%s: %s two-way flag is the authored one"), *Name, *Id.ToString()), Spec->bTwoWay, Def->bTwoWay);
            TestTrue(*FString::Printf(TEXT("%s: %s PreferredSwing is a sign"), *Name, *Id.ToString()), Spec->PreferredSwing == 1.0 || Spec->PreferredSwing == -1.0);

            const LI::Opening* Open = nullptr;
            for (const LI::Opening& O : Layout.Openings)
            {
                if (O.DoorId == IdText)
                {
                    Open = &O;
                }
            }
            if (Open == nullptr)
            {
                AddError(FString::Printf(TEXT("%s: no opening carries %s"), *Name, *Id.ToString()));
                continue;
            }
            const bool bAlongU = Open->Plan.Width() > Open->Plan.Depth();
            TestTrue(*FString::Printf(TEXT("%s: %s hinge"), *Name, *Id.ToString()), Spec->Hinge.Equals(KitPoint(B, Layout, Open->HingeU, Open->HingeV, 0.0), 0.01));

            // The closed leaf spans the doorway: from the hinge jamb to the other jamb.
            const FVector Dir = KdClosedDir(*Spec);
            const FVector AcrossAxis = bAlongU ? B.Frame.U : B.Frame.V;
            TestTrue(*FString::Printf(TEXT("%s: %s closed leaf runs along %s"), *Name, *Id.ToString(), bAlongU ? TEXT("U") : TEXT("V")),
                     FMath::Abs(FVector::DotProduct(Dir, AcrossAxis)) > 0.999);
            const double Width = bAlongU ? Open->Plan.Width() : Open->Plan.Depth();
            const double HingeAt = bAlongU ? Open->HingeU : Open->HingeV;
            const double OtherJamb = (bAlongU ? (HingeAt == Open->Plan.U0 ? Open->Plan.U1 : Open->Plan.U0)
                                              : (HingeAt == Open->Plan.V0 ? Open->Plan.V1 : Open->Plan.V0));
            TestTrue(*FString::Printf(TEXT("%s: %s hinge sits on a jamb"), *Name, *Id.ToString()),
                     HingeAt == (bAlongU ? Open->Plan.U0 : Open->Plan.V0) || HingeAt == (bAlongU ? Open->Plan.U1 : Open->Plan.V1));
            const FVector FarJamb = bAlongU ? KitPoint(B, Layout, OtherJamb, Open->HingeV, 0.0) : KitPoint(B, Layout, Open->HingeU, OtherJamb, 0.0);
            TestTrue(*FString::Printf(TEXT("%s: %s closed leaf reaches the far jamb"), *Name, *Id.ToString()),
                     (Spec->Hinge + Dir * Width).Equals(FarJamb, 0.5));

            // The inside sign: the leaf's preferred side is toward the dwelling or passage. Front door: +V (into the building).
            // Flat doors: toward the flat (unit_a lies at lower U than the passage, unit_b at higher U).
            FVector Into = B.Frame.V;
            if (IdText == "door.nabapara.tall_flats_unit_a")
            {
                Into = -B.Frame.U;
            }
            else if (IdText == "door.nabapara.tall_flats_unit_b")
            {
                Into = B.Frame.U;
            }
            TestTrue(*FString::Printf(TEXT("%s: %s inside sign points into the dwelling"), *Name, *Id.ToString()),
                     FVector::DotProduct(Into, KdLocalY(*Spec)) * Spec->PreferredSwing > 0.99);

            if (!bAlongU)
            {
                // The swept leaf area is a sweep zone of the layout, in the flat, and nothing solid stands in it.
                TestTrue(*FString::Printf(TEXT("%s: %s is a flat door (one-way, inward)"), *Name, *Id.ToString()), !Spec->bTwoWay && Def->PreferredSwingSide == 1);
                const LI::Zone* Sweep = nullptr;
                for (const LI::Zone& Z : Layout.Zones)
                {
                    if (Z.Kind == LI::ZoneKind::DoorSweep && Z.OwnerId == Open->Id)
                    {
                        Sweep = &Z;
                    }
                }
                TestNotNull(*FString::Printf(TEXT("%s: %s has a sweep zone"), *Name, *Id.ToString()), Sweep);
                if (Sweep != nullptr)
                {
                    TestFalse(*FString::Printf(TEXT("%s: %s sweep is clear of collision"), *Name, *Id.ToString()),
                              KdAnyBlocking(Blocking, KdWorldBox(B, Layout, Sweep->Area, 2.0, LI::DoorClearHeightCm)));
                    const FVector Mid = KitPoint(B, Layout, 0.5 * (Sweep->Area.U0 + Sweep->Area.U1), 0.5 * (Sweep->Area.V0 + Sweep->Area.V1), 0.0);
                    TestTrue(*FString::Printf(TEXT("%s: %s sweep lies on the inside side"), *Name, *Id.ToString()),
                             FVector::DotProduct(Mid - Spec->Hinge, KdLocalY(*Spec)) * Spec->PreferredSwing > 0.0);
                }
                TestFalse(*FString::Printf(TEXT("%s: %s doorway is clear of collision"), *Name, *Id.ToString()),
                          KdAnyBlocking(Blocking, KdWorldBox(B, Layout, LI::Rect{Open->Plan.U0, Open->Plan.U1, Open->Plan.V0 + 2, Open->Plan.V1 - 2}, 2.0, LI::DoorClearHeightCm)));
            }
        }
    }
    TestEqual(TEXT("three kit rows were checked"), Rows, 3);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
