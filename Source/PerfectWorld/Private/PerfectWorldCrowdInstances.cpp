#include "PerfectWorldCrowdInstances.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UInstancedStaticMeshComponent* APWCrowdPresenter::Batch(const FColor Tint)
{
    const uint32 Colour = Tint.DWColor();
    if (const auto* Found = Batches.Find(Colour)) return Found->Get();
    auto* Component = NewObject<UInstancedStaticMeshComponent>(this);
    Component->SetupAttachment(RootComponent);
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetStaticMesh(Cube);
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetCanEverAffectNavigation(false);
    Component->SetCastShadow(false);
    if (BaseMaterial)
    {
        auto* Paint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
        Paint->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(Tint));
        Paint->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor::FromSRGBColor(Tint));
        Component->SetMaterial(0, Paint);
    }
    Component->RegisterComponent();
    Batches.Add(Colour, Component);
    return Component;
}

void APWCrowdPresenter::MakeFar(FR75PCrowdRecord& Record)
{
    if (!Record.Instances.IsEmpty()) return;
    for (const auto& Block : Record.Look.Blocks)
    {
        auto* Component = Batch(Block.Tint);
        auto& Free = FreeInstances.FindOrAdd(Block.Tint.DWColor());
        const int32 Index = Free.IsEmpty() ? Component->AddInstance(FTransform(FQuat::Identity,
            FVector::ZeroVector, FVector::ZeroVector), true) : Free.Pop(EAllowShrinking::No);
        Record.Instances.Add({Block.Tint.DWColor(), Index});
    }
    DrawFar(Record);
}

void APWCrowdPresenter::ReleaseFar(FR75PCrowdRecord& Record)
{
    for (const auto& Handle : Record.Instances)
    {
        if (const auto* Found = Batches.Find(Handle.Colour))
            (*Found)->UpdateInstanceTransform(Handle.Index, FTransform(FQuat::Identity,
                FVector::ZeroVector, FVector::ZeroVector), true, false, true);
        FreeInstances.FindOrAdd(Handle.Colour).Add(Handle.Index);
    }
    Record.Instances.Reset();
}

void APWCrowdPresenter::DrawFar(FR75PCrowdRecord& Record)
{
    const FTransform Body(Record.Facing, Record.Feet);
    for (int32 I = 0; I < Record.Instances.Num(); ++I)
    {
        const auto& Block = Record.Look.Blocks[I];
        FVector At;
        FQuat Rotation;
        PWNpcLooks::PoseBlock({Block.Group, Block.Centre, Block.Pivot}, Record.Pose, Record.Look.Height, At, Rotation);
        const FTransform Local(Rotation, At, Block.Size / 100);
        const auto& Handle = Record.Instances[I];
        Batches[Handle.Colour]->UpdateInstanceTransform(Handle.Index, Local * Body, true, false, true);
    }
}

void APWCrowdPresenter::Demote(FR75PCrowdRecord& Record)
{
    if (!Record.Actor.IsValid()) return;
    Record.Feet = Record.Actor->Feet();
    Record.Facing = Record.Actor->GetActorRotation();
    Record.Pose = Record.Actor->Pose();
    // Project its actual avoided position onto its graph before normal far movement resumes.
    if (const auto* Path = Route(Record.Target.Slot))
    {
        double Along = 0, Best = TNumericLimits<double>::Max();
        for (const auto& Segment : Path->Segments)
        {
            const FVector A = PWCrowd::Point(Segment.From), B = PWCrowd::Point(Segment.To);
            const FVector AB = B - A;
            const double T = FMath::Clamp(FVector::DotProduct(Record.Feet - A, AB) / FMath::Max(1.0, AB.SizeSquared()), 0.0, 1.0);
            const double D = FVector::DistSquared(Record.Feet, A + AB * T);
            if (D < Best) { Best = D; Record.Distance = Along + T * Segment.LengthCm; }
            Along += Segment.LengthCm;
        }
    }
    MakeFar(Record); // fill instances at precisely the pose being removed
    Record.Actor->Destroy();
    Record.Actor.Reset();
}

void APWCrowdPresenter::AdvanceFar(const float DeltaSeconds, const bool bRunning)
{
    for (auto& Pair : People)
    {
        auto& Record = *Pair.Value;
        if (Record.Actor.IsValid() || Record.Instances.IsEmpty()) continue;
        const auto* Path = Route(Record.Target.Slot);
        if (Path && bRunning && !Record.bFrozen && !Record.bRetiring)
        {
            const double Before = Record.Distance;
            const FVector Previous = Record.Feet;
            const FVector OnGraph = PWCrowd::RoutePoint(*Path, Record.Distance);
            if (!Record.Feet.Equals(OnGraph, .1))
                Record.Feet = FMath::VInterpConstantTo(Previous, OnGraph, FMath::Min(DeltaSeconds, .1f), 140.0f);
            else
            {
                Record.Distance = FMath::FInterpConstantTo(Record.Distance, double(Record.Target.RouteDistanceCm),
                    double(FMath::Min(DeltaSeconds, .1f)), 140.0);
                Record.Feet = PWCrowd::RoutePoint(*Path, Record.Distance);
            }
            if (!Record.Feet.Equals(Previous, .01))
                Record.Facing = (Record.Feet - Previous).Rotation();
            const float SpeedFraction = DeltaSeconds > 0 ? float(FMath::Abs(Record.Distance - Before) / (140 * DeltaSeconds)) : 0;
            const auto TargetPose = PWNpcGesture::Walk(float(Record.Distance), SpeedFraction);
            Record.Pose.BlendToward(TargetPose, FMath::Min(1.0f, DeltaSeconds * 8), FMath::Min(1.0f, DeltaSeconds * 8));
        }
        DrawFar(Record);
    }
    // One render-state update per colour batch, not per cube. Slots are recycled, never compacted under visible IDs.
    for (const auto& Pair : Batches) Pair.Value->MarkRenderStateDirty();
}
