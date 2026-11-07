#include "PerfectWorldAwakeningBlocks.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace PWAwakening
{
const FName AwBlocksActorTag(TEXT("awakening.floor.blocks"));

UMaterialInstanceDynamic* AwMakePaint(UMaterialInterface* Base, UObject* Outer, const EAwColour Colour)
{
    if (Base == nullptr)
    {
        return nullptr;
    }
    // BasicShapeMaterial exposes its base colour as the vector parameter "Color"; "BaseColor" is set too (unknown names are ignored).
    UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Base, Outer);
    const FLinearColor Linear = FLinearColor::FromSRGBColor(AwColourSrgb(Colour));
    Paint->SetVectorParameterValue(TEXT("Color"), Linear);
    Paint->SetVectorParameterValue(TEXT("BaseColor"), Linear);
    return Paint;
}

FAwCommitResult AwCommitParts(UWorld& World, const TArray<FAwPart>& Parts, UStaticMesh& Cube, UMaterialInterface* BaseMaterial)
{
    FAwCommitResult Result;
    TArray<FTransform> Lists[AwRoleCount][AwColourCount];
    for (const FAwPart& Part : Parts)
    {
        const FVector Size = Part.Box.GetSize();
        // The engine cube is 100 cm on a side, centred on its pivot.
        Lists[static_cast<int32>(Part.Role)][static_cast<int32>(Part.Colour)].Add(FTransform(FQuat::Identity, Part.Box.GetCenter(), Size / 100.0));
    }
    AActor* Blocks = World.SpawnActor<AActor>();
    if (Blocks == nullptr)
    {
        return Result;
    }
    Blocks->Tags.AddUnique(AwBlocksActorTag);
    USceneComponent* Root = NewObject<USceneComponent>(Blocks, TEXT("AwRoot"));
    Root->SetMobility(EComponentMobility::Static);
    Blocks->SetRootComponent(Root);
    Root->RegisterComponent();

    for (int32 Role = 0; Role < AwRoleCount; ++Role)
    {
        for (int32 Colour = 0; Colour < AwColourCount; ++Colour)
        {
            const TArray<FTransform>& List = Lists[Role][Colour];
            if (List.IsEmpty())
            {
                continue;
            }
            UInstancedStaticMeshComponent* Component =
                NewObject<UInstancedStaticMeshComponent>(Blocks, FName(*FString::Printf(TEXT("AwBlocks_%d_%d"), Role, Colour)));
            Component->SetStaticMesh(&Cube);
            Component->SetMobility(EComponentMobility::Static);
            Component->SetupAttachment(Root);
            Component->SetCanEverAffectNavigation(false);
            const EAwRole BlockRole = static_cast<EAwRole>(Role);
            Component->SetCastShadow(BlockRole != EAwRole::Prop && Colour != static_cast<int32>(EAwColour::GridLine));
            switch (BlockRole)
            {
            case EAwRole::Visual:
                Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                break;
            case EAwRole::Solid:
                Component->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
                Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                break;
            case EAwRole::Prop:
                Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
                Component->SetCollisionObjectType(ECC_WorldStatic);
                Component->SetCollisionResponseToAllChannels(ECR_Ignore);
                Component->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
                Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
                Component->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
                Component->SetVisibility(false);
                Component->SetHiddenInGame(true);
                break;
            }
            if (BlockRole != EAwRole::Prop)
            {
                if (UMaterialInstanceDynamic* Paint = AwMakePaint(BaseMaterial, Blocks, static_cast<EAwColour>(Colour)))
                {
                    Component->SetMaterial(0, Paint);
                }
            }
            Component->RegisterComponent();
            for (const FTransform& Transform : List)
            {
                Component->AddInstance(Transform);
            }
            Result.Instances += List.Num();
            ++Result.Components;
        }
    }
    Result.Actor = Blocks;
    return Result;
}
} // namespace PWAwakening
