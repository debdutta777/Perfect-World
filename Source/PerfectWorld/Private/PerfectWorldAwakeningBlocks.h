#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldAwakeningLayout.h"

class AActor;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UObject;
class UStaticMesh;
class UWorld;

namespace PWAwakening
{
/** The tag the static blocks actor carries so tests and the acceptance driver can find it. */
extern const FName AwBlocksActorTag;

struct FAwCommitResult
{
    AActor* Actor = nullptr;
    int32 Instances = 0;
    int32 Components = 0;
};

/**
 * Turns the layout's parts into one actor with one instanced cube component per (role, colour): the lane's block method, with the
 * floor's own palette (the reserved Gate-kit ramps cannot go through PWLane::ELaneColour, which is the ordinary world's).
 */
FAwCommitResult AwCommitParts(UWorld& World, const TArray<FAwPart>& Parts, UStaticMesh& Cube, UMaterialInterface* BaseMaterial);

/** A dynamic copy of the basic shape material painted in a palette colour (null when Base is null). */
UMaterialInstanceDynamic* AwMakePaint(UMaterialInterface* Base, UObject* Outer, EAwColour Colour);
} // namespace PWAwakening
