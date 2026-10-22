#include "PerfectWorldSchoolCampus.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldSchoolCampusParts.h"
#include "PerfectWorldSchoolGate.h"
#include "PerfectWorldShopDisplayActor.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWSchoolCampus, Log, All);

namespace PWSchool
{
const FName ScBlocksActorTag(TEXT("school.nabapara.campus_blocks"));

namespace
{
bool ScRectOf(const PerfectWorld::NeighbourhoodPlan& Plan, std::string_view Id, FScRect& Out)
{
    const PerfectWorld::NeighbourhoodFeature* Feature = PerfectWorld::FindNeighbourhoodFeature(Plan, Id);
    if (Feature == nullptr)
    {
        return false;
    }
    Out.X0 = Feature->Bounds.X0;
    Out.Y0 = Feature->Bounds.Y0;
    Out.X1 = Feature->Bounds.X1;
    Out.Y1 = Feature->Bounds.Y1;
    return Out.X1 > Out.X0 && Out.Y1 > Out.Y0;
}

bool ScPointOf(const PerfectWorld::NeighbourhoodPlan& Plan, std::string_view Id, double& OutX, double& OutY, int& OutWidth)
{
    const PerfectWorld::NeighbourhoodFeature* Feature = PerfectWorld::FindNeighbourhoodFeature(Plan, Id);
    if (Feature == nullptr || Feature->Geometry.size() != 1)
    {
        return false;
    }
    OutX = Feature->Geometry[0].X;
    OutY = Feature->Geometry[0].Y;
    OutWidth = Feature->WidthCm;
    return true;
}
} // namespace

FScCampus ScReadCampus(const PerfectWorld::NeighbourhoodPlan& Plan)
{
    FScCampus Campus;
    double GateY = 0.0, OfficeY = 0.0, ClassY = 0.0;
    int Width = 0, Unused = 0;
    if (!ScRectOf(Plan, "site.nabapara.school", Campus.Site) || !ScRectOf(Plan, "school.nabapara.forecourt", Campus.Forecourt) ||
        !ScRectOf(Plan, "building.nabapara.school_teaching", Campus.Teaching) ||
        !ScRectOf(Plan, "school.nabapara.playground", Campus.Playground) ||
        !ScPointOf(Plan, PerfectWorld::NeighbourhoodSchoolGateId, Campus.GateX, GateY, Width) ||
        !ScPointOf(Plan, "entrance.nabapara.school_office", Campus.OfficeDoorX, OfficeY, Unused) ||
        !ScPointOf(Plan, "entrance.nabapara.school_classroom", Campus.ClassroomDoorX, ClassY, Unused))
    {
        return Campus;
    }
    Campus.GateWidth = Width;
    // The gate stands on the site's south boundary; both doors on the teaching block's south facade.
    const bool bGateOnBoundary = FMath::IsNearlyEqual(GateY, Campus.Site.Y0, 0.5);
    const bool bDoorsOnFacade = FMath::IsNearlyEqual(OfficeY, Campus.Teaching.Y0, 0.5) && FMath::IsNearlyEqual(ClassY, Campus.Teaching.Y0, 0.5);
    Campus.bValid = bGateOnBoundary && bDoorsOnFacade && Campus.GateWidth >= 300.0;
    return Campus;
}

FScCampus ScReadCampus()
{
    return ScReadCampus(PerfectWorld::BuildNeighbourhoodPlan(0));
}

bool ScBlocksPlayer(PWLane::ELaneColour Colour)
{
    const int32 Index = static_cast<int32>(Colour);
    return Index < PWLane::WallColourCount || Colour == ELaneColour::Asphalt || Colour == ELaneColour::Footpath ||
           Colour == ELaneColour::Ground || Colour == ELaneColour::PropBlock || Colour == ELaneColour::Barrier;
}

TArray<const FScPart*> FScBuilt::FindParts(const FString& Prefix) const
{
    TArray<const FScPart*> Found;
    for (const FScPart& Part : Parts)
    {
        if (Part.Id.ToString().StartsWith(Prefix))
        {
            Found.Add(&Part);
        }
    }
    return Found;
}

const FScPart* FScBuilt::FindPart(const TCHAR* Id) const
{
    const FName Wanted(Id);
    for (const FScPart& Part : Parts)
    {
        if (Part.Id == Wanted)
        {
            return &Part;
        }
    }
    return nullptr;
}

FBox FScBuilt::UnionOf(const FString& Prefix) const
{
    FBox Union(ForceInit);
    for (const FScPart& Part : Parts)
    {
        if (Part.Id.ToString().StartsWith(Prefix))
        {
            Union += Part.Box;
        }
    }
    return Union;
}

void ScTree(FScBuilt& B, const TCHAR* Id, double Cx, double Cy, double Height)
{
    const double Z = ScGroundZ;
    ScColumn(B, *FString::Printf(TEXT("%s.trunk"), Id), ELaneColour::Trunk, Cx, Cy, 30.0, 30.0, Z, Z + Height);
    ScProp(B, *FString::Printf(TEXT("%s.trunk_collision"), Id), Cx - 17.0, Cy - 17.0, Z, Cx + 17.0, Cy + 17.0, Z + Height);
    ScColumn(B, *FString::Printf(TEXT("%s.crown_low"), Id), ELaneColour::LeafDark, Cx, Cy, 300.0, 300.0, Z + Height - 40.0, Z + Height + 80.0);
    ScColumn(B, *FString::Printf(TEXT("%s.crown_mid"), Id), ELaneColour::LeafLight, Cx, Cy, 220.0, 220.0, Z + Height + 80.0, Z + Height + 160.0);
    ScColumn(B, *FString::Printf(TEXT("%s.crown_top"), Id), ELaneColour::LeafDark, Cx, Cy, 130.0, 130.0, Z + Height + 160.0, Z + Height + 215.0);
}

bool ScBuildCampus(FScBuilt& Out, const FScCampus& Campus)
{
    if (!Campus.bValid)
    {
        return false;
    }
    ScBuildGrounds(Out, Campus);
    ScBuildGate(Out, Campus);
    ScBuildBlock(Out, Campus);
    ScBuildOffice(Out, Campus);
    ScBuildClassroom(Out, Campus);
    return true;
}

namespace
{
/** The ceiling and outdoor lights of the school, in the lane's own warm style. */
int32 ScSpawnLights(UWorld& World, const TArray<PWLane::FLaneRoomLight>& Lights, bool bShadowed)
{
    int32 Count = 0;
    for (const PWLane::FLaneRoomLight& Light : Lights)
    {
        APointLight* Actor = World.SpawnActor<APointLight>(Light.Position, FRotator::ZeroRotator);
        UPointLightComponent* Component = Actor != nullptr ? Cast<UPointLightComponent>(Actor->GetLightComponent()) : nullptr;
        if (Component == nullptr)
        {
            continue;
        }
        Component->SetMobility(EComponentMobility::Movable);
        Component->IntensityUnits = ELightUnits::Lumens;
        Component->SetLightColor(bShadowed ? FLinearColor(1.0f, 0.82f, 0.58f) : FLinearColor(1.0f, 0.74f, 0.44f));
        Component->SetIntensity(Light.Lumens);
        Component->SetAttenuationRadius(Light.Radius);
        Component->SetCastShadows(bShadowed);
        if (bShadowed)
        {
            Component->SetSourceRadius(6.0f);
        }
        ++Count;
    }
    return Count;
}
} // namespace

FScSpawnResult SpawnSchoolCampus(UWorld& World, const PWLane::FLaneScene& /*Scene*/)
{
    FScSpawnResult Result;
    const FScCampus Campus = ScReadCampus();
    if (!Campus.bValid)
    {
        UE_LOG(LogPWSchoolCampus, Error, TEXT("Nabapara Vidyalaya: the neighbourhood plan has no valid school; the campus is not built."));
        return Result;
    }
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cube == nullptr)
    {
        UE_LOG(LogPWSchoolCampus, Error, TEXT("Nabapara Vidyalaya: the engine cube was not found; the campus is not built."));
        return Result;
    }
    UMaterialInterface* Paint = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Paint == nullptr)
    {
        UE_LOG(LogPWSchoolCampus, Warning, TEXT("Nabapara Vidyalaya: BasicShapeMaterial not found; the campus will be unpainted."));
    }

    TUniquePtr<FScBuilt> Built = MakeUnique<FScBuilt>();
    if (!ScBuildCampus(*Built, Campus))
    {
        return Result;
    }

    TSet<const AActor*> Before;
    for (TActorIterator<AActor> It(&World); It; ++It)
    {
        Before.Add(*It);
    }
    Result.Components = Built->Blocks.Commit(World, *Cube, Paint, Result.Instances);
    for (TActorIterator<AActor> It(&World); It; ++It)
    {
        if (!Before.Contains(*It) && It->FindComponentByClass<UInstancedStaticMeshComponent>() != nullptr)
        {
            Result.BlocksActor = *It;
            It->Tags.AddUnique(ScBlocksActorTag);
            break;
        }
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    for (const FScTextSign& Sign : Built->Texts)
    {
        APWShopHoursSignActor* Actor = World.SpawnActor<APWShopHoursSignActor>(Sign.Location, FRotator::ZeroRotator, Params);
        if (Actor != nullptr)
        {
            Actor->Configure(Sign.Id, Sign.Text, Sign.Location, Sign.Yaw, Sign.SizeCm);
            Actor->Tags.AddUnique(FName(TEXT("school.nabapara.sign")));
            ++Result.TextSigns;
        }
    }

    Result.Gate = World.SpawnActor<APWSchoolGateActor>(Built->Points.SignInPoint, FRotator::ZeroRotator, Params);
    if (Result.Gate != nullptr)
    {
        Result.Gate->Configure(Built->Points.SignInPoint);
    }
    Result.Board = World.SpawnActor<APWSchoolBoardActor>(Built->Points.BoardPoint, FRotator::ZeroRotator, Params);
    if (Result.Board != nullptr)
    {
        Result.Board->Configure(Built->Points.BoardPoint, Built->Points.BoardTextLocation, Built->Points.BoardTextYaw,
                                Built->Points.BoardTextSizeCm);
    }
    Result.Lights = ScSpawnLights(World, Built->IndoorLights, true) + ScSpawnLights(World, Built->OutdoorLights, false);
    Result.bBuilt = Result.BlocksActor != nullptr && Result.Gate != nullptr && Result.Board != nullptr;

    UE_LOG(LogPWSchoolCampus, Display, TEXT("Nabapara Vidyalaya: %d block instances in %d components, %d text signs, %d lights, gate %d, board %d."),
           Result.Instances, Result.Components, Result.TextSigns, Result.Lights, Result.Gate != nullptr, Result.Board != nullptr);
    return Result;
}
} // namespace PWSchool
