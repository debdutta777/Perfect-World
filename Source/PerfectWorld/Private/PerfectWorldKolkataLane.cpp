#include "PerfectWorldKolkataLane.h"

#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFurnitureActor.h"
#include "PerfectWorldLaneBarriers.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldLaneBuildings.h"
#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldLaneHomeFurniture.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldLaneNpcBindings.h"
#include "PerfectWorldLaneProps.h"
#include "PerfectWorldLaneRoutes.h"
#include "PerfectWorldLaneStationeryShop.h"
#include "PerfectWorldLaneTeaShop.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldNirmalBindings.h"
#include "PerfectWorldRupaBindings.h"
#include "PerfectWorldSubrataBindings.h"

namespace PWLane
{
namespace
{
constexpr int32 WarmLightCount = 4;
constexpr double WarmLightFocusX = 900.0; // lights go to the stalls nearest this point on the lane

/** The ground, the road, side alleys, drains, patches and manhole covers. The footpaths belong to the buildings. */
void AddGroundAndRoad(FLaneBlocks& K)
{
    constexpr double GZ = GroundZ;

    // Flat packed earth well past the skyline; its top sits 5 cm under the road so the two never fight.
    K.Box(ELaneColour::Ground, FVector(-30000.0, -30000.0, GZ - 400.0), FVector(40000.0, 30000.0, GZ - 5.0));

    K.Box(ELaneColour::Asphalt, FVector(LaneX0, -RoadHalfWidth, GZ - 20.0), FVector(LaneX1, RoadHalfWidth, GZ));
    K.Box(ELaneColour::Asphalt, FVector(AlleyPlusA, RoadHalfWidth, GZ - 20.0), FVector(AlleyPlusB, 1500.0, GZ));
    K.Box(ELaneColour::Asphalt, FVector(AlleyMinusA, -1500.0, GZ - 20.0), FVector(AlleyMinusB, -RoadHalfWidth, GZ));

    // Open drains along both road edges.
    K.Box(ELaneColour::Drain, FVector(LaneX0, -RoadHalfWidth, GZ), FVector(LaneX1, -RoadHalfWidth + 25.0, GZ + 2.0));
    K.Box(ELaneColour::Drain, FVector(LaneX0, RoadHalfWidth - 25.0, GZ), FVector(LaneX1, RoadHalfWidth, GZ + 2.0));

    // Patched tarmac and worn spots, scattered by the hash.
    for (int32 I = 0; I < 8; ++I)
    {
        const uint32 R = LaneHash(55, I);
        const double X = LaneX0 + 300.0 + LaneUnit(R) * (LaneX1 - LaneX0 - 600.0);
        const double Y = (LaneUnit(R >> 8) - 0.5) * 360.0;
        const double Sx = 160.0 + LaneUnit(R >> 14) * 260.0;
        const double Sy = 80.0 + LaneUnit(R >> 18) * 120.0;
        K.Box(ELaneColour::RoadPatch, FVector(X - Sx * 0.5, Y - Sy * 0.5, GZ), FVector(X + Sx * 0.5, Y + Sy * 0.5, GZ + 1.5));
    }
    for (const double X : {1300.0, 3600.0, 5200.0})
    {
        const double Y = (X > 3000.0) ? -40.0 : 30.0;
        K.Box(ELaneColour::Drain, FVector(X - 35.0, Y - 35.0, GZ), FVector(X + 35.0, Y + 35.0, GZ + 2.0));
    }
}

/** A handful of warm point lights at the stalls nearest the starting view; no shadows, so they stay cheap. */
void SpawnWarmLights(UWorld& World, const TArray<FVector>& Spots)
{
    TArray<FVector> Sorted = Spots;
    Sorted.Sort([](const FVector& A, const FVector& B)
                { return FMath::Abs(A.X - WarmLightFocusX) < FMath::Abs(B.X - WarmLightFocusX); });
    const int32 Count = FMath::Min(WarmLightCount, Sorted.Num());
    for (int32 I = 0; I < Count; ++I)
    {
        APointLight* Light = World.SpawnActor<APointLight>(Sorted[I], FRotator::ZeroRotator);
        UPointLightComponent* Component = Light != nullptr ? Cast<UPointLightComponent>(Light->GetLightComponent()) : nullptr;
        if (Component == nullptr)
        {
            continue;
        }
        Component->SetMobility(EComponentMobility::Movable);
        Component->IntensityUnits = ELightUnits::Lumens;
        Component->SetLightColor(FLinearColor(1.0f, 0.72f, 0.42f));
        Component->SetIntensity(600.0f);
        Component->SetAttenuationRadius(650.0f);
        Component->SetCastShadows(false);
    }
}
/** The ceiling lights of the enterable interiors: small, warm, unshadowed, with a short reach so they stay in their room. */
void SpawnRoomLights(UWorld& World, const TArray<FLaneRoomLight>& Lights)
{
    for (const FLaneRoomLight& Light : Lights)
    {
        APointLight* Actor = World.SpawnActor<APointLight>(Light.Position, FRotator::ZeroRotator);
        UPointLightComponent* Component = Actor != nullptr ? Cast<UPointLightComponent>(Actor->GetLightComponent()) : nullptr;
        if (Component == nullptr)
        {
            continue;
        }
        Component->SetMobility(EComponentMobility::Movable);
        Component->IntensityUnits = ELightUnits::Lumens;
        Component->SetLightColor(FLinearColor(1.0f, 0.78f, 0.5f));
        Component->SetIntensity(Light.Lumens);
        Component->SetAttenuationRadius(Light.Radius);
        // Shadowed with a small soft source: an unshadowed sphere shone through the interior walls (LANE_LIFE_PLAN §C;
        // HL's HomeLightSpecs require it). Attenuation stays within the room's shorter side.
        Component->SetCastShadows(true);
        Component->SetSourceRadius(6.0f);
    }
}

/**
 * The hinged doors of the enterable interiors. A door starts closed; at BeginPlay it registers with UPWLaneLifeSubsystem,
 * which sets it to the portable core's state for its id (some household doors start open).
 */
void SpawnDoors(UWorld& World, const TArray<FLaneDoorSpec>& Doors)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FLaneDoorSpec& Spec : Doors)
    {
        APWDoorActor* Door = World.SpawnActor<APWDoorActor>(Spec.Hinge, FRotator(0.0, Spec.ClosedYaw, 0.0), Params);
        if (Door != nullptr)
        {
            Door->Configure(Spec.Id, static_cast<float>(Spec.PreferredSwing), Spec.bTwoWay);
            if (UPWLaneLifeSubsystem* PbLife = UPWLaneLifeSubsystem::Get(&World))
            {
                PbLife->RegisterDoorInsideSide(*Door, Spec.PreferredSwing);
            }
        }
    }
}

/**
 * The household adult of the default (Parent) creation origin, standing in the living and eating room just inside the
 * living door, so the player who walks in from the entry meets them. TODO(care-origin): a care-origin start has the
 * institutional staff counterpart in this place instead.
 *
 * The first packaged walk-in found the old spot at the edge of reach and behind the living door's open leaf; the home
 * layout now publishes the spot (PerfectWorldLaneHomeFurniture.h).
 */
void SpawnHouseholdAdult(UWorld& World)
{
    // The spot and facing come from the home's own layout (HomeAdultSpot, order HL): within reach of the living doorway,
    // clear of furniture and of every door swing. The opening home's frame: origin (-300, -400), U = +X, V = -Y.
    const FLaneFrame HomeFrame = MakeLaneFrame(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
    const FTransform At(FRotator(0.0, HomeAdultSpotYaw(HomeFrame), 0.0), HomeAdultSpotWorld(HomeFrame));
    APWNpcCharacter* Parent = APWNpcCharacter::SpawnAt(World, At, PWLaneLife::ToName(PerfectWorld::HouseholdAdultId),
                                                       PWLaneLife::ToText(PerfectWorld::HouseholdAdultLabel), EPWNpcOutfit::ParentHousehold);
    if (Parent == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Kolkata lane: the household adult could not be placed in the opening home."));
    }
}

/**
 * The children's home: its seats are usable, and its day-shift care worker works to npc_schedule.care_worker through the
 * NPC presenter (LL-02): at the staff desk and in the common room on duty, handing over and walking home after it, and
 * stepping out of a doorway the player approaches. She works in the lane whatever the player's origin.
 */
void SpawnCareHome(UWorld& World)
{
    const FLaneFrame CareFrame = MakeLaneFrame(-1400.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FHomeSeatSpec& Spec : CareSeatSpecs())
    {
        if (APWSeatActor* Seat = World.SpawnActor<APWSeatActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
        {
            Seat->ConfigureFromSpec(Spec, CareFrame);
        }
    }

    // The building's anchors, routes and doorways go to the presenter first; then the person, who is placed (or not, when
    // off duty) by her schedule at the world clock's instant. A missing binding is a diagnostic, never a default spawn.
    UPWNpcPresenceSubsystem* NpPresence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
    if (NpPresence == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("Kolkata lane: no NPC presenter; the care worker is not placed."));
        return;
    }
    if (!NpPresence->Bindings().RegisterProvider(FName(TEXT("building.nabapara.care_home")), NpCareHomeBindings(CareFrame)))
    {
        UE_LOG(LogTemp, Error, TEXT("Kolkata lane: the children's home NPC bindings conflict with another building's."));
        return;
    }
    NpPresence->RegisterScheduledPerson(FName(TEXT("person.care_worker")), NSLOCTEXT("PerfectWorldLane", "CareWorker", "Care worker"),
                                        EPWNpcOutfit::HouseholdAdult);
    if (const UPWLaneLifeSubsystem* NpLife = UPWLaneLifeSubsystem::Get(&World))
    {
        NpPresence->Reconcile(NpLife->GetNow());
    }
}

/** The opening home's usable furniture: the seats and the newspaper, from the layout's own anchor specs (order HL/FI). */
void SpawnHomeFurniture(UWorld& World)
{
    const FLaneFrame HomeFrame = MakeLaneFrame(-300.0, -400.0, FVector(1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FHomeSeatSpec& Spec : HomeSeatSpecs())
    {
        if (APWSeatActor* Seat = World.SpawnActor<APWSeatActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
        {
            Seat->ConfigureFromSpec(Spec, HomeFrame);
        }
    }
    if (APWReadableActor* Paper = World.SpawnActor<APWReadableActor>(FVector::ZeroVector, FRotator::ZeroRotator, Params))
    {
        Paper->ConfigureFromSpec(HomeReadableSpec(), HomeFrame, NSLOCTEXT("PerfectWorldLane", "HomeNewspaper", "Today's newspaper"));
    }
}
void FaSpawnFarida(UWorld& World, const FLaneScene& Scene)
{
    if (!HasTeaShop(Scene)) { return; }
    // HasTeaShop means TS accepted the complete footprint. Recover its built frame from the recorded frontage;
    // the remaining dimensions are TS's published binding, not a second shop generator.
    for (const FLaneFront& Front : Scene.Fronts[1])
    {
        FLaneBuilding Building;
        Building.Frame = MakeLaneFrame(Front.X0, Front.AbsY, FVector::ForwardVector, FVector::RightVector);
        Building.Width = Front.X1 - Front.X0;
        Building.Depth = 900.0;
        Building.Setback = 120.0;
        Building.Storeys = 3;
        Building.Side = 1;
        Building.Index = 3;
        if (!IsTeaShopBuilding(Building)) { continue; }
        UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&World);
        if (Life == nullptr || !Life->RegisterTeaShopPresence(Building))
        {
            UE_LOG(LogTemp, Error, TEXT("Shanti Chai NPC provider/schedule registration failed; no fallback spawn."));
        }
        return;
    }
    UE_LOG(LogTemp, Error, TEXT("Shanti Chai was built but its recorded frontage did not resolve."));
}

void PbSpawnAnindita(UWorld& World, const FLaneScene& Scene)
{
    if (!HasStationeryShop(Scene)) { return; }
    for (const FLaneFront& PbFront : Scene.Fronts[1])
    {
        FLaneBuilding PbBuilding;
        PbBuilding.Frame = MakeLaneFrame(PbFront.X0, PbFront.AbsY, FVector::ForwardVector, FVector::RightVector);
        PbBuilding.Width = PbFront.X1 - PbFront.X0;
        PbBuilding.Depth = 700.0;
        PbBuilding.Setback = 60.0;
        PbBuilding.Storeys = 3;
        PbBuilding.Side = 1;
        PbBuilding.Index = 2;
        if (!IsStationeryBuilding(PbBuilding)) { continue; }
        auto* PbPresence = World.GetSubsystem<UPWNpcPresenceSubsystem>();
        if (PbPresence == nullptr || !PbPresence->Bindings().RegisterProvider(
                FName(TEXT("building.nabapara.stationery")), NpStationeryBindings(PbBuilding)))
        {
            UE_LOG(LogTemp, Error, TEXT("Stationery NPC provider registration failed; no fallback spawn."));
            return;
        }
        if (!PbPresence->RegisterScheduledPerson(FName(TEXT("person.anindita_sen")),
                NSLOCTEXT("PerfectWorldLane", "AninditaName", "Anindita"), EPWNpcOutfit::ShopProprietor))
        {
            UE_LOG(LogTemp, Error, TEXT("Anindita's schedule registration failed; no fallback spawn."));
        }
        // Mira helps at the packing desk on Tuesday and Thursday afternoons; school and her own afternoons are off-lane
        // behind the shop threshold (UP-01), so she always enters and leaves the lane through this door.
        // TODO(outfit): her own look (navy uniform, square satchel) replaces the placeholder outfit.
        if (!PbPresence->RegisterScheduledPerson(FName(TEXT("person.mira_sen")),
                NSLOCTEXT("PerfectWorldLane", "MiraName", "Mira"), EPWNpcOutfit::HouseholdAdult))
        {
            UE_LOG(LogTemp, Error, TEXT("Mira's schedule registration failed; no fallback spawn."));
        }
        if (const auto* PbLife = UPWLaneLifeSubsystem::Get(&World)) { PbPresence->Reconcile(PbLife->GetNow()); }
        return;
    }
    UE_LOG(LogTemp, Error, TEXT("Stationery shop was built but its recorded frontage did not resolve."));
}
} // namespace

void BuildKolkataLane(UWorld& World, UStaticMesh& Cube, UMaterialInterface* BaseMaterial)
{
    FLaneScene Scene;
    AddGroundAndRoad(Scene.Blocks);
    BuildLaneRows(Scene);
    BuildLaneProps(Scene);
    BuildLaneBarriers(Scene);
    ReportFootpathRoutes(Scene); // development builds only: logs the narrowest clear footpath width
    if (!Scene.bOpeningHomeBuilt)
    {
        UE_LOG(LogTemp, Warning, TEXT("Kolkata lane: the opening home was not built (the -Y building at index 2 changed shape); it stays a solid block."));
    }

    int32 Instances = 0;
    const int32 Components = Scene.Blocks.Commit(World, Cube, BaseMaterial, Instances);
    SpawnWarmLights(World, Scene.LightSpots);
    SpawnRoomLights(World, Scene.RoomLights);
    SpawnDoors(World, Scene.Doors);
    FaSpawnFarida(World, Scene);
    PbSpawnAnindita(World, Scene);
    SpawnSubrata(World, Scene);
    SpawnNirmal(World, Scene);
    SpawnRupa(World, Scene);
    if (Scene.bOpeningHomeBuilt)
    {
        SpawnHouseholdAdult(World);
        SpawnHomeFurniture(World);
    }
    if (HasCareHome(Scene))
    {
        SpawnCareHome(World);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Kolkata lane: the children's home was not built (the -Y building at index 1 changed shape)."));
    }

    UE_LOG(LogTemp, Display, TEXT("Kolkata lane: %d block instances in %d components, %d stall lights placed."), Instances, Components,
           FMath::Min(WarmLightCount, Scene.LightSpots.Num()));
}
} // namespace PWLane
