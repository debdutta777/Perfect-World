#include "PerfectWorldAwakeningFloor.h"

#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldShopDisplayActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWAwakeningFloor, Log, All);

namespace PWAwakening
{
namespace
{
int32 AwFlSpawnLights(UWorld& World, const TArray<FAwLight>& Lights)
{
    int32 Count = 0;
    for (const FAwLight& Light : Lights)
    {
        APointLight* Actor = World.SpawnActor<APointLight>(Light.Position, FRotator::ZeroRotator);
        UPointLightComponent* Component = Actor != nullptr ? Cast<UPointLightComponent>(Actor->GetLightComponent()) : nullptr;
        if (Component == nullptr)
        {
            continue;
        }
        Actor->Tags.AddUnique(FName(TEXT("awakening.floor.light")));
        Component->SetMobility(EComponentMobility::Movable);
        Component->IntensityUnits = ELightUnits::Lumens;
        Component->SetLightColor(Light.Colour);
        Component->SetIntensity(Light.Lumens);
        Component->SetAttenuationRadius(Light.Radius);
        Component->SetCastShadows(false);
        ++Count;
    }
    return Count;
}
} // namespace

FAwSpawnResult SpawnAwakeningFloor(UWorld& World, const FAwFloorConfig& Config)
{
    FAwSpawnResult Result;
    const FAwFloorSite Site = Config.Site.IsSet() ? Config.Site.GetValue() : AwSiteFromCampus();
    FAwFloorLayout Layout;
    if (!AwBuildLayout(Layout, Site, Config.Cast))
    {
        UE_LOG(LogPWAwakeningFloor, Error, TEXT("Awakening floor: the campus plan has no valid school; the floor is not built."));
        return Result;
    }
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube == nullptr)
    {
        UE_LOG(LogPWAwakeningFloor, Error, TEXT("Awakening floor: the engine cube was not found; the floor is not built."));
        return Result;
    }
    UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Paint == nullptr)
    {
        UE_LOG(LogPWAwakeningFloor, Warning, TEXT("Awakening floor: BasicShapeMaterial not found; the floor will be unpainted."));
    }

    const FAwCommitResult Blocks = AwCommitParts(World, Layout.Parts, *Cube, Paint);
    Result.BlocksActor = Blocks.Actor;
    Result.Instances = Blocks.Instances;
    Result.Components = Blocks.Components;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APWAwakeningFloorActor* Floor = World.SpawnActor<APWAwakeningFloorActor>(Layout.Site.Origin, FRotator::ZeroRotator, Params);
    if (Floor == nullptr)
    {
        return Result;
    }
    Floor->Initialise(Layout, Config);
    Result.Floor = Floor;
    const float Scale = FMath::Clamp(Config.TextScale, 1.0f, 2.0f);

    for (const FAwText& Text : Layout.Texts)
    {
        APWShopHoursSignActor* Sign = World.SpawnActor<APWShopHoursSignActor>(Text.Location, FRotator::ZeroRotator, Params);
        if (Sign != nullptr)
        {
            Sign->Configure(Text.Id, Text.Text, Text.Location, -90.0, Text.SizeCm * Scale);
            Sign->Tags.AddUnique(FName(TEXT("awakening.floor.sign")));
            ++Result.TextSigns;
        }
    }

    for (const FAwPadSpec* Pad : Layout.AllPads())
    {
        APWAwakeningPadActor* Actor = World.SpawnActor<APWAwakeningPadActor>(Pad->Point, FRotator::ZeroRotator, Params);
        if (Actor != nullptr)
        {
            Actor->Configure(Floor, *Pad);
            Floor->RegisterPad(Actor);
            ++Result.Pads;
        }
    }

    for (const FAwStationSpec& Station : Layout.Stations)
    {
        for (const bool bSecondary : {false, true})
        {
            const FAwMoverSpec& Spec = bSecondary ? Station.Secondary : Station.Primary;
            if (Spec.Poses.IsEmpty())
            {
                continue;
            }
            APWAwakeningMoverActor* Mover = World.SpawnActor<APWAwakeningMoverActor>(Spec.Poses[Spec.StartPose].Centre, FRotator::ZeroRotator, Params);
            if (Mover != nullptr)
            {
                Mover->Configure(Spec, Cube, Paint);
                Floor->RegisterMover(Station.Index, bSecondary, Mover);
                ++Result.Movers;
            }
        }
        APWShopHoursSignActor* Status = World.SpawnActor<APWShopHoursSignActor>(Station.StatusAt, FRotator::ZeroRotator, Params);
        if (Status != nullptr)
        {
            Status->Tags.AddUnique(FName(TEXT("awakening.floor.status")));
            Floor->RegisterStatusSign(Station.Index, Status);
            ++Result.TextSigns;
        }
    }

    Result.Lights = AwFlSpawnLights(World, Layout.Lights);
    if (Config.bSpawnFiguresAtOnce)
    {
        Floor->SpawnFigures();
        Result.Figures = Floor->GetFigureCount();
    }
    Result.bBuilt = Result.BlocksActor != nullptr && Result.Pads == Layout.AllPads().Num() && Result.Movers >= SituationCount * 2;
    UE_LOG(LogPWAwakeningFloor, Display, TEXT("Awakening floor: %d block instances in %d components, %d pads, %d movers, %d text signs, %d lights."),
           Result.Instances, Result.Components, Result.Pads, Result.Movers, Result.TextSigns, Result.Lights);
    return Result;
}
} // namespace PWAwakening
