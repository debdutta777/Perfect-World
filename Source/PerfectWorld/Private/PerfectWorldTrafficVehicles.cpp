#include "PerfectWorldTrafficVehicles.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
const FLinearColor R72PYellow(0.95f, 0.62f, 0.025f);
const FLinearColor R72PBlue(0.035f, 0.19f, 0.52f);
const FLinearColor R72PGreen(0.025f, 0.35f, 0.12f);
const FLinearColor R72PRed(0.68f, 0.045f, 0.035f);
const FLinearColor R72PGlass(0.12f, 0.25f, 0.32f);
const FLinearColor R72PRubber(0.018f, 0.02f, 0.025f);
const FLinearColor R72PCream(0.88f, 0.83f, 0.65f);
}

APWTrafficVehicle::APWTrafficVehicle()
{
    PrimaryActorTick.bCanEverTick = false;
    Body = CreateDefaultSubobject<UBoxComponent>(TEXT("R72PBody"));
    SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Body->SetCollisionObjectType(ECC_WorldDynamic);
    Body->SetCollisionResponseToAllChannels(ECR_Ignore);
    for (const auto Channel : {ECC_Pawn, ECC_WorldStatic, ECC_WorldDynamic, ECC_Visibility, ECC_Camera})
        Body->SetCollisionResponseToChannel(Channel, ECR_Block);
    Body->CanCharacterStepUpOn = ECB_No;
    Body->SetGenerateOverlapEvents(false);
    Body->SetCanEverAffectNavigation(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> R72PCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> R72PPaint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> R72PGlow(TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"));
    Cube = R72PCube.Object;
    Paint = R72PPaint.Object;
    Glow = R72PGlow.Object;
    SetActorHiddenInGame(true);
}

void APWTrafficVehicle::Block(const FVector& Centre, const FVector& Size, const FLinearColor& Colour, bool bLamp)
{
    auto* Part = NewObject<UStaticMeshComponent>(this);
    AddInstanceComponent(Part);
    Part->SetupAttachment(Body);
    Part->SetStaticMesh(Cube);
    Part->SetRelativeLocation(Centre - FVector(0, 0, Height / 2 + 4));
    Part->SetRelativeScale3D(Size / 100);
    Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Part->SetCanEverAffectNavigation(false);
    // Fall back to the paint material if the emissive engine asset is ever missing.
    auto* Material = UMaterialInstanceDynamic::Create(bLamp && Glow ? Glow.Get() : Paint.Get(), this);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), Colour);
        if (bLamp) Material->SetVectorParameterValue(TEXT("EmissiveColor"), Colour);
        Part->SetMaterial(0, Material);
        if (bLamp) Lamps.Add(Material);
    }
    Part->RegisterComponent();
}

void APWTrafficVehicle::Board(const FVector& Centre, double Yaw, const FString& Text)
{
    auto* Label = NewObject<UTextRenderComponent>(this);
    AddInstanceComponent(Label);
    Label->SetupAttachment(Body);
    Label->SetRelativeLocation(Centre - FVector(0, 0, Height / 2 + 4));
    Label->SetRelativeRotation(FRotator(0, Yaw, 0));
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetVerticalAlignment(EVRTA_TextCenter);
    Label->SetWorldSize(20);
    Label->SetTextRenderColor(FColor::Black);
    Label->SetText(FText::FromString(Text));
    Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label->RegisterComponent();
}

void APWTrafficVehicle::Build(const PerfectWorld::NeighbourhoodVehicle& V)
{
    using PerfectWorld::NeighbourhoodVehicleKind;
    TrafficId = V.Id;
    const double L = V.LengthCm, W = V.WidthCm;
    const bool bBus = V.Kind == NeighbourhoodVehicleKind::Bus;
    const bool bTram = V.Kind == NeighbourhoodVehicleKind::FictionalTram;
    const bool bAuto = V.Kind == NeighbourhoodVehicleKind::SharedAuto;
    const bool bCycle = V.Kind == NeighbourhoodVehicleKind::CycleRickshaw;
    Height = bBus || bTram ? 290 : bAuto || bCycle ? 190 : 160;
    Body->SetBoxExtent(FVector(L / 2, W / 2, Height / 2));
    CarCount = bTram ? 2 : 1;
    // Stable colour variation; no persistent RNG and no dependence on actor names.
    uint32 R72PTint = 0;
    for (const unsigned char C : V.Id) R72PTint = R72PTint * 33 + C;
    const FLinearColor Coach = bBus ? ((R72PTint & 1) ? R72PBlue : R72PRed) : R72PGreen;
    if (bBus || bTram)
    {
        const double CarL = bTram ? (L - 32) / 2 : L;
        for (int32 Car = 0; Car < CarCount; ++Car)
        {
            const double X = bTram ? (Car == 0 ? -(CarL + 32) / 2 : (CarL + 32) / 2) : 0;
            Block({X, 0, 94}, {CarL, W, 120}, Coach);
            Block({X, 0, 183}, {CarL - 8, W - 8, 64}, R72PGlass);
            Block({X, 0, 253}, {CarL, W, 70}, bTram ? R72PCream : R72PYellow);
            for (double Side : {-1.0, 1.0})
            {
                for (int32 Pane = -2; Pane <= 2; ++Pane)
                    Block({X + Pane * CarL / 6, Side * (W / 2 - 4), 185}, {12, 8, 72}, Coach);
                for (double Axle : {-0.32, 0.32})
                    Block({X + Axle * CarL, Side * (W / 2 - 12), 36}, {64, 24, 64}, R72PRubber);
                Block({X, Side * (W / 2 - 2), 117}, {CarL, 4, 15}, R72PYellow);
            }
        }
        if (bTram) Block({0, 0, 75}, {32, 60, 28}, R72PRubber); // visible coupling gap
        Block({L / 2 - 3, 0, 250}, {6, W * 0.86, 32}, R72PCream);
        Board({L / 2 + 1, 0, 250}, 0, bTram ? TEXT("T7  NABAPARA") : TEXT("7B  SCHOOL - PARA"));
        Board({0, -W / 2 - 1, 248}, -90, bTram ? TEXT("T7  MAIN ROAD") : TEXT("7B  NABAPARA"));
    }
    else if (bCycle)
    {
        // A pedal cycle-rickshaw: narrow bicycle front, passenger bench and canopy behind.
        Block({-L * 0.2, 0, 75}, {L * 0.52, W * 0.92, 20}, R72PBlue);
        Block({-L * 0.4, 0, 107}, {16, W * 0.9, 60}, R72PRed);
        Block({-L * 0.22, 0, 182}, {L * 0.56, W, 12}, R72PCream);
        for (double Side : {-1.0, 1.0})
        {
            Block({-L * 0.42, Side * W * 0.43, 134}, {8, 8, 85}, R72PGreen);
            Block({-L * 0.27, Side * (W / 2 - 8), 35}, {64, 12, 64}, R72PRubber);
        }
        Block({L * 0.34, 0, 35}, {64, 12, 64}, R72PRubber);
        Block({L * 0.15, 0, 75}, {L * 0.46, 12, 12}, R72PGreen);
        Block({L * 0.3, 0, 100}, {10, 72, 10}, R72PRubber);
        Block({0, 0, 95}, {35, 28, 10}, R72PRubber);
        // Block rider, visibly pedalling rather than a hand-pulled vehicle.
        Block({0, 0, 132}, {30, 34, 48}, R72PCream);
        Block({3, 0, 167}, {24, 24, 24}, FLinearColor(0.5f, 0.28f, 0.14f));
    }
    else
    {
        const FLinearColor Colour = bAuto ? R72PGreen : R72PYellow;
        Block({0, 0, 59}, {L, W, 62}, Colour);
        Block({-L * 0.08, 0, 109}, {L * 0.57, W - 16, 48}, R72PGlass);
        Block({-L * 0.08, 0, bAuto ? 166.0 : 145.0}, {L * 0.62, W - 8, bAuto ? 44.0 : 22.0}, R72PYellow);
        if (!bAuto)
        {
            // Stepped bonnet and boot give the taxi its Ambassador-style silhouette.
            Block({L * 0.36, 0, 89}, {L * 0.26, W - 8, 24}, R72PYellow);
            Block({-L * 0.4, 0, 88}, {L * 0.18, W - 8, 24}, R72PYellow);
            Block({0, 0, 158}, {52, 32, 4}, R72PCream);
            Board({0, -18, 158}, -90, TEXT("TAXI"));
        }
        for (double Side : {-1.0, 1.0})
        {
            Block({-L * 0.31, Side * (W / 2 - 10), 28}, {50, 20, 50}, R72PRubber);
            if (!bAuto) Block({L * 0.32, Side * (W / 2 - 10), 28}, {50, 20, 50}, R72PRubber);
        }
        if (bAuto) Block({L * 0.36, 0, 28}, {50, 20, 50}, R72PRubber);
    }
    for (double Side : {-1.0, 1.0})
        Block({L / 2 - 4, Side * W * 0.32, 65}, {8, 20, 18}, FLinearColor::White, true);
    SetNight(false);
}

void APWTrafficVehicle::SetNight(bool bNight)
{
    const FLinearColor Light = FLinearColor(1.0f, 0.86f, 0.55f) * (bNight ? 8.0f : 0.35f);
    for (const auto& Lamp : Lamps)
    {
        if (!Lamp) continue;
        Lamp->SetVectorParameterValue(TEXT("Color"), Light);
        Lamp->SetVectorParameterValue(TEXT("EmissiveColor"), Light);
    }
}

FVector APWTrafficVehicle::CentreAt(const FVector& RoadPosition) const
{
    return RoadPosition + FVector(0, 0, Height / 2 + 4);
}

bool APWTrafficVehicle::CanPlace(const FVector& Centre, const FQuat& Heading) const
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(R72PSpawn), false, this);
    // Only people matter: static street furniture is the street builder's concern and must never
    // freeze a vehicle that the portable lane model has already kept inside its carriageway.
    return !GetWorld()->OverlapAnyTestByObjectType(Centre, Heading, FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeBox(Body->GetUnscaledBoxExtent() + FVector(1)), Params);
}

bool APWTrafficVehicle::CanTravel(const FVector& Centre) const
{
    FCollisionQueryParams Params(SCENE_QUERY_STAT(R72PSweep), false, this);
    FHitResult Hit;
    const auto Shape = FCollisionShape::MakeBox(Body->GetUnscaledBoxExtent() + FVector(1));
    // Preflight before moving: a sweep never resolves penetration by displacing a pedestrian.
    const FCollisionObjectQueryParams People(ECC_Pawn);
    return !GetWorld()->OverlapAnyTestByObjectType(GetActorLocation(), GetActorQuat(), People, Shape, Params) &&
        !GetWorld()->SweepSingleByObjectType(Hit, GetActorLocation(), Centre, GetActorQuat(), People, Shape, Params);
}

bool APWTrafficVehicle::Travel(const FVector& Centre)
{
    if (Centre.Equals(GetActorLocation(), 0.001)) return true;
    if (!CanTravel(Centre)) return false;
    // The object-type sweep above already proved the path; a second component sweep would also
    // stop on decorative statics and leave the vehicle half-way. No sweep means no shoving.
    SetActorLocation(Centre, false, nullptr, ETeleportType::None);
    return true;
}
