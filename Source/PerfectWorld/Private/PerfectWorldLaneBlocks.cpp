#include "PerfectWorldLaneBlocks.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"

namespace PWLane
{
namespace
{
struct FLaneColourSpec
{
    const TCHAR* Name;
    FColor Colour;     // sRGB
    bool bSolid;       // collision on (walls, ground, road); everything else is walk-through decoration
    bool bCastShadow;  // off for thin or tiny detail where a shadow is only noise
};

// Weathered pastels for the walls, darker trims, and a few saturated accents (shutters, awnings, cloth, bunting).
// The order matches ELaneColour.
const FLaneColourSpec ColourTable[] = {
    {TEXT("WallOchre"), FColor(204, 160, 92), true, true},
    {TEXT("WallTerracotta"), FColor(176, 98, 72), true, true},
    {TEXT("WallTeal"), FColor(96, 148, 146), true, true},
    {TEXT("WallCream"), FColor(226, 212, 176), true, true},
    {TEXT("WallPink"), FColor(212, 148, 152), true, true},
    {TEXT("WallButter"), FColor(224, 196, 112), true, true},
    {TEXT("WallSage"), FColor(150, 168, 130), true, true},
    {TEXT("WallDustyBlue"), FColor(128, 150, 178), true, true},
    {TEXT("TrimLight"), FColor(234, 226, 206), false, true},
    {TEXT("TrimDark"), FColor(74, 62, 56), false, true},
    {TEXT("Plinth"), FColor(98, 86, 74), false, false},
    {TEXT("Stain"), FColor(128, 112, 94), false, false},
    {TEXT("ShutterGreen"), FColor(52, 104, 76), false, true},
    {TEXT("ShutterSteel"), FColor(122, 136, 144), false, true},
    {TEXT("ShutterBlue"), FColor(72, 112, 152), false, true},
    {TEXT("ShutterStripe"), FColor(68, 78, 86), false, false},
    {TEXT("ShopInterior"), FColor(48, 34, 28), false, false},
    {TEXT("WoodLight"), FColor(172, 130, 86), false, true},
    {TEXT("DoorWood"), FColor(92, 56, 38), false, true},
    {TEXT("GlassDusk"), FColor(84, 104, 128), false, false},
    {TEXT("GlassLit"), FColor(255, 200, 108), false, false},
    {TEXT("Concrete"), FColor(152, 150, 144), false, true},
    {TEXT("Asphalt"), FColor(68, 68, 72), true, true},
    {TEXT("Footpath"), FColor(150, 142, 130), true, true},
    {TEXT("Ground"), FColor(152, 134, 106), true, true},
    {TEXT("Drain"), FColor(38, 36, 36), false, false},
    {TEXT("RoadPatch"), FColor(96, 94, 92), false, false},
    {TEXT("Rail"), FColor(56, 78, 66), false, true},
    {TEXT("TankBlack"), FColor(24, 26, 28), false, true},
    {TEXT("TankLid"), FColor(54, 58, 62), false, true},
    {TEXT("PotTerracotta"), FColor(178, 98, 66), false, true},
    {TEXT("LeafDark"), FColor(46, 94, 52), false, true},
    {TEXT("LeafLight"), FColor(100, 146, 64), false, true},
    {TEXT("Trunk"), FColor(94, 72, 52), false, true},
    {TEXT("WireBlack"), FColor(20, 20, 22), false, false},
    {TEXT("AwningRed"), FColor(180, 54, 46), false, true},
    {TEXT("AwningBlue"), FColor(54, 94, 152), false, true},
    {TEXT("AwningGreen"), FColor(60, 130, 94), false, true},
    {TEXT("GoodsYellow"), FColor(238, 198, 54), false, false},
    {TEXT("GoodsOrange"), FColor(222, 122, 42), false, false},
    {TEXT("GoodsRed"), FColor(192, 52, 42), false, false},
    {TEXT("GoodsGreen"), FColor(92, 152, 62), false, false},
    {TEXT("SignBlue"), FColor(46, 86, 150), false, false},
    {TEXT("SignRed"), FColor(176, 48, 44), false, false},
    {TEXT("SignGreen"), FColor(40, 110, 80), false, false},
    {TEXT("ClothMagenta"), FColor(192, 52, 112), false, false},
    {TEXT("ClothMustard"), FColor(224, 172, 52), false, false},
    {TEXT("ClothTeal"), FColor(42, 142, 152), false, false},
    {TEXT("TaxiYellow"), FColor(234, 180, 26), false, true},
    {TEXT("TaxiGlass"), FColor(30, 40, 48), false, false},
    {TEXT("Chrome"), FColor(172, 174, 178), false, true},
    {TEXT("AcUnit"), FColor(208, 210, 208), false, true},
};

static_assert(UE_ARRAY_COUNT(ColourTable) == ColourCount, "ColourTable must have one row per ELaneColour");
} // namespace

double FLaneScene::FrontAbsY(int32 Side, double X) const
{
    for (const FLaneFront& Front : Fronts[Side > 0 ? 1 : 0])
    {
        if (X >= Front.X0 && X <= Front.X1)
        {
            return Front.AbsY;
        }
    }
    return FootpathOuter;
}

void FLaneBlocks::Box(ELaneColour Colour, const FVector& CornerA, const FVector& CornerB)
{
    const FVector Min(FMath::Min(CornerA.X, CornerB.X), FMath::Min(CornerA.Y, CornerB.Y), FMath::Min(CornerA.Z, CornerB.Z));
    const FVector Max(FMath::Max(CornerA.X, CornerB.X), FMath::Max(CornerA.Y, CornerB.Y), FMath::Max(CornerA.Z, CornerB.Z));
    const FVector Size = Max - Min;
    if (Size.X < 0.5 || Size.Y < 0.5 || Size.Z < 0.5)
    {
        return;
    }
    // The engine cube is 100 cm on a side, centred on its pivot.
    Instances[static_cast<int32>(Colour)].Add(FTransform(FQuat::Identity, (Min + Max) * 0.5, Size / 100.0));
}

void FLaneBlocks::Tilted(ELaneColour Colour, const FVector& Centre, const FVector& Size, const FRotator& Rotation)
{
    Instances[static_cast<int32>(Colour)].Add(FTransform(Rotation, Centre, Size / 100.0));
}

void FLaneBlocks::Wire(ELaneColour Colour, const FVector& From, const FVector& To, double Thickness)
{
    const FVector Delta = To - From;
    const double Length = Delta.Size();
    if (Length < 1.0)
    {
        return;
    }
    const FRotator Rotation = FRotationMatrix::MakeFromX(Delta).Rotator();
    Instances[static_cast<int32>(Colour)].Add(
        FTransform(Rotation, (From + To) * 0.5, FVector(Length / 100.0, Thickness / 100.0, Thickness / 100.0)));
}

void FLaneBlocks::SaggingWire(ELaneColour Colour, const FVector& From, const FVector& To, double Sag, double Thickness, int32 Segments)
{
    FVector Previous = From;
    for (int32 I = 1; I <= Segments; ++I)
    {
        const double T = static_cast<double>(I) / static_cast<double>(Segments);
        FVector Point = From + (To - From) * T;
        Point.Z -= 4.0 * Sag * T * (1.0 - T);
        Wire(Colour, Previous, Point, Thickness);
        Previous = Point;
    }
}

int32 FLaneBlocks::Commit(UWorld& World, UStaticMesh& Cube, UMaterialInterface* BaseMaterial, int32& OutInstances)
{
    OutInstances = 0;
    AActor* Lane = World.SpawnActor<AActor>();
    if (Lane == nullptr)
    {
        return 0;
    }
    USceneComponent* Root = NewObject<USceneComponent>(Lane, TEXT("LaneRoot"));
    Root->SetMobility(EComponentMobility::Static);
    Lane->SetRootComponent(Root);
    Root->RegisterComponent();

    int32 Components = 0;
    for (int32 Index = 0; Index < ColourCount; ++Index)
    {
        const TArray<FTransform>& List = Instances[Index];
        if (List.IsEmpty())
        {
            continue;
        }
        const FLaneColourSpec& Spec = ColourTable[Index];

        UInstancedStaticMeshComponent* Blocks =
            NewObject<UInstancedStaticMeshComponent>(Lane, FName(*FString::Printf(TEXT("Lane_%s"), Spec.Name)));
        Blocks->SetStaticMesh(&Cube);
        Blocks->SetMobility(EComponentMobility::Static);
        Blocks->SetupAttachment(Root);
        Blocks->SetCastShadow(Spec.bCastShadow);
        Blocks->SetCollisionEnabled(Spec.bSolid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        Blocks->SetCanEverAffectNavigation(false);
        if (BaseMaterial != nullptr)
        {
            // BasicShapeMaterial exposes its base colour as the vector parameter "Color". "BaseColor" is set too
            // so a differently named parameter would still pick the colour up; unknown names are ignored.
            UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(BaseMaterial, Lane);
            const FLinearColor Linear = FLinearColor::FromSRGBColor(Spec.Colour);
            Paint->SetVectorParameterValue(TEXT("Color"), Linear);
            Paint->SetVectorParameterValue(TEXT("BaseColor"), Linear);
            Blocks->SetMaterial(0, Paint);
        }
        Blocks->RegisterComponent();
        for (const FTransform& Transform : List)
        {
            Blocks->AddInstance(Transform);
        }
        OutInstances += List.Num();
        ++Components;
    }
    return Components;
}
} // namespace PWLane
