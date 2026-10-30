#include "PerfectWorldCrowdFigures.h"

#include "Components/CapsuleComponent.h"
// ContentRegistry.h includes the core Json.h from its own directory first (as PerfectWorldAcceptanceTests.cpp does).
#include "ContentRegistry.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PerfectWorldInteractionComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace PWCrowd
{
FVector Point(const PerfectWorld::NeighbourhoodPoint P) { return FVector(P.X, P.Y, P.Z); }
bool MayChangePresence(const bool bOldVisible, const bool bNewVisible) { return !bOldVisible && !bNewVisible; }

FLook MakeLook(const PerfectWorld::CrowdPerson& Person)
{
    using PerfectWorld::CrowdCohort;
    using PerfectWorld::CrowdOutfit;
    FLook Result;
    const bool bStudent = Person.Cohort == CrowdCohort::Student;
    const bool bChild = Person.Cohort == CrowdCohort::Child;
    const bool bElder = Person.Cohort == CrowdCohort::Elder;
    Result.Height = (bChild ? 122.0 : bStudent ? 156.0 : bElder ? 157.0 : 162.0) + Person.AppearanceSeed % 13;
    const double H = Result.Height;
    // Start with the existing non-cast shirt/trouser body, including its face and limb pivots.
    PWNpcLooks::BuildBlocks(NAME_None, EPWNpcOutfit::ParentHousehold, H, Result.Blocks);
    const FColor White(242, 242, 234), Blue(38, 65, 124), Basket(166, 115, 64);
    const FColor Skin[] = {FColor(176, 128, 92), FColor(151, 102, 72), FColor(206, 160, 122)};
    const FColor Shirts[] = {FColor(143, 169, 157), FColor(189, 147, 113), FColor(151, 167, 190)};
    const FColor Shirt = bStudent ? White : Shirts[(Person.AppearanceSeed / 7) % 3];
    for (auto& Block : Result.Blocks)
    {
        if (Block.Tint == FColor(216, 210, 194)) Block.Tint = Shirt;
        if (Block.Tint == FColor(58, 60, 66) && bStudent) Block.Tint = Blue;
        if (Block.Tint == FColor(206, 160, 122)) Block.Tint = Skin[Person.AppearanceSeed % 3];
        if (bElder && Block.Tint == FColor(44, 36, 34)) Block.Tint = FColor(176, 176, 173);
    }
    if (bStudent && Person.Outfit == CrowdOutfit::WhiteShirtBlueSkirt)
    {
        // Lower legs retain skin; the skirt moves with the body, not with one leg.
        for (auto& Block : Result.Blocks)
            if (Block.Group == EPWNpcPartGroup::LegLeft || Block.Group == EPWNpcPartGroup::LegRight)
            {
                Block.Size.Z *= .66;
                Block.Centre.Z = Block.Size.Z * .5;
                Block.Tint = Skin[Person.AppearanceSeed % 3];
            }
        PWNpcLooks::PutBlock(Result.Blocks, FVector(0, 0, H * .365), FVector(27, 45, H * .23), Blue);
    }
    if (bStudent)
    {
        PWNpcLooks::PutBlock(Result.Blocks, FVector(-16, 0, H * .66), FVector(12, 29, 35), FColor(76, 62, 47));
        for (double Side : {-1.0, 1.0})
            PWNpcLooks::PutBlock(Result.Blocks, FVector(11, Side * 14, H * .70), FVector(2, 4, 31), FColor(76, 62, 47));
    }
    if (Person.Cohort == CrowdCohort::Vendor)
    {
        PWNpcLooks::PutBlock(Result.Blocks, FVector(0, 0, H * .63), FVector(25, 44, H * .29), FColor(166, 99, 64));
        PWNpcLooks::PutBlock(Result.Blocks, FVector(16, 23, H * .40), FVector(28, 23, 17), Basket);
        PWNpcLooks::PutBlock(Result.Blocks, FVector(16, 23, H * .46), FVector(22, 17, 7), FColor(92, 120, 57));
    }
    if (Person.Cohort == CrowdCohort::OfficeCommuter)
        PWNpcLooks::PutBlock(Result.Blocks, FVector(0, 26, H * .47), FVector(17, 8, 24), FColor(70, 61, 55));
    return Result;
}

// DefaultGame.ini stages ../Data/Text beside Data/Content, so the authored lines load in PIE and the packaged game.
const TMap<FString, FText>& CvExchangeLines()
{
    static const TMap<FString, FText> Lines = []
    {
        TMap<FString, FText> Result;
        FString Text;
        const FString Path = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Text/en/ambient.crowd.json"));
        if (!FFileHelper::LoadFileToString(Text, *Path)) return Result;
        const PerfectWorld::JsonParseResult Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Text));
        if (!Parsed.Value || !Parsed.Value->IsObject()) return Result;
        for (const auto& [Key, Value] : Parsed.Value->Object)
            if (Value.IsString())
                Result.Add(UTF8_TO_TCHAR(Key.c_str()), FText::FromString(UTF8_TO_TCHAR(Value.String.c_str())));
        return Result;
    }();
    return Lines;
}

FText ExchangeLine(const std::string& Key)
{
    if (const FText* Line = CvExchangeLines().Find(UTF8_TO_TCHAR(Key.c_str())))
        return *Line;
    return NSLOCTEXT("PerfectWorldCrowd", "ambient.exchange.greeting", "Hello.");
}

FVector RoutePoint(const PerfectWorld::CrowdRouteSlot& Route, double Distance)
{
    for (const auto& Segment : Route.Segments)
    {
        if (Distance <= Segment.LengthCm)
            return FMath::Lerp(Point(Segment.From), Point(Segment.To),
                FMath::Clamp(Distance / FMath::Max(1.0, double(Segment.LengthCm)), 0.0, 1.0));
        Distance -= Segment.LengthCm;
    }
    return Route.Segments.empty() ? FVector::ZeroVector : Point(Route.Segments.back().To);
}

TArray<FVector> Approach(const PerfectWorld::CrowdRouteSlot& Route, const double Target, const bool bReverse)
{
    TArray<FVector> Result;
    double Along = 0;
    if (!bReverse)
    {
        if (!Route.Segments.empty()) Result.Add(Point(Route.Segments.front().From));
        for (const auto& Segment : Route.Segments)
        {
            Along += Segment.LengthCm;
            if (Along >= Target) break;
            Result.Add(Point(Segment.To));
        }
    }
    else
    {
        Along = double(Route.Route.LengthCm);
        if (!Route.Segments.empty()) Result.Add(Point(Route.Segments.back().To));
        for (auto It = Route.Segments.rbegin(); It != Route.Segments.rend(); ++It)
        {
            Along -= It->LengthCm;
            if (Along <= Target) break;
            Result.Add(Point(It->From));
        }
    }
    Result.Add(RoutePoint(Route, Target));
    return Result;
}
}

APWCrowdFigure::APWCrowdFigure()
{
    PrimaryActorTick.bCanEverTick = true;
    // The shared mover honours this tag in AmbientAllowsSnap. Only our presenter may
    // reconcile a crowd pose, after testing both camera envelopes for visibility.
    Tags.Add(FName(TEXT("npc.presentation.no_snap")));
    bUseControllerRotationYaw = false;
    AutoPossessAI = EAutoPossessAI::Disabled;
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
    auto* Motion = GetCharacterMovement();
    Motion->bRunPhysicsWithNoController = true;
    Motion->bOrientRotationToMovement = false;
    Motion->bEnablePhysicsInteraction = false;
    Motion->RepulsionForce = 0;
    Motion->MaxWalkSpeed = 0;
    Motion->MaxAcceleration = 1400;
    Motion->BrakingDecelerationWalking = 1800;
    Motion->MaxStepHeight = 45;
    Mover = CreateDefaultSubobject<UPWNpcMovementComponent>(TEXT("R75PMover"));
    Mover->SetComponentTickEnabled(false);
    Motion->SetComponentTickEnabled(false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> R75PCube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> R75PPaint(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (R75PCube.Succeeded()) Cube = R75PCube.Object;
    if (R75PPaint.Succeeded()) BaseMaterial = R75PPaint.Object;
}

APWCrowdFigure* APWCrowdFigure::Spawn(UWorld& World, const PerfectWorld::CrowdPerson& Person,
    const FVector& Feet, const FRotator& Facing)
{
    const auto Look = PWCrowd::MakeLook(Person);
    const auto Centre = Feet + FVector(0, 0, Look.Height * .5 + 2);
    if (World.OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(PWNpc::CapsuleRadius, float(Look.Height * .5)))) return nullptr;
    const FTransform At(Facing, Centre);
    auto* Figure = World.SpawnActorDeferred<APWCrowdFigure>(StaticClass(), At, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Figure) return nullptr;
    Figure->CrowdPerson = Person;
    Figure->Look = Look;
    Figure->GetCapsuleComponent()->InitCapsuleSize(PWNpc::CapsuleRadius, float(Look.Height * .5));
    Figure->FinishSpawning(At);
    return Figure;
}

void APWCrowdFigure::BeginPlay()
{
    Super::BeginPlay();
    for (const auto& Block : Look.Blocks)
    {
        auto* Part = NewObject<UStaticMeshComponent>(this);
        Part->SetStaticMesh(Cube);
        Part->SetupAttachment(GetCapsuleComponent());
        Part->SetRelativeScale3D(Block.Size / 100);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetCanEverAffectNavigation(false);
        Part->RegisterComponent();
        if (BaseMaterial)
        {
            auto* Paint = UMaterialInstanceDynamic::Create(BaseMaterial, this);
            Paint->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(Block.Tint));
            Paint->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor::FromSRGBColor(Block.Tint));
            Part->SetMaterial(0, Paint);
        }
        Parts.Add(Part);
    }
    Mover->OnPose.AddUObject(this, &APWCrowdFigure::ApplyPose);
    ApplyPose({});
}

FVector APWCrowdFigure::Feet() const
{
    return GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
}

void APWCrowdFigure::ApplyPose(const FPWNpcPose& Pose)
{
    CurrentPose = Pose;
    for (int32 I = 0; I < Parts.Num(); ++I)
    {
        const auto& Block = Look.Blocks[I];
        const FPWNpcPartRest Rest{Block.Group, Block.Centre, Block.Pivot};
        FVector At;
        FQuat Rotation;
        PWNpcLooks::PoseBlock(Rest, Pose, Look.Height, At, Rotation);
        Parts[I]->SetRelativeLocationAndRotation(At - FVector(0, 0, Look.Height * .5), Rotation);
    }
}

void APWCrowdFigure::Present(const PerfectWorld::CrowdPerson& Person, const PerfectWorld::CrowdRouteSlot& Route)
{
    if (Route.Segments.empty()) return;
    bool bReverse = Person.RouteDistanceCm < CrowdPerson.RouteDistanceCm;
    double Along = 0;
    for (const auto& Segment : Route.Segments)
    {
        Along += Segment.LengthCm;
        if (Person.RouteDistanceCm > Along) continue;
        if (Person.SpeedCmPerGameMinute > 0)
            bReverse = FVector::DotProduct(PWCrowd::Point(Person.Heading),
                PWCrowd::Point(Segment.To) - PWCrowd::Point(Segment.From)) < 0;
        break;
    }
    CrowdPerson = Person;
    // No target beyond this authoritative sample: an idle/paused pupil cannot drift past a gate.
    Mover->SetRoute(PWCrowd::Approach(Route, double(Person.RouteDistanceCm), bReverse));
    FPWNpcIntentView Intent;
    Intent.State = EPWNpcState::Travel;
    Intent.RouteId = FName(TEXT("route.ambient.presentation"));
    Intent.ProgressPermille = 1000;
    Intent.bTalkable = Person.ReceivesAvoidanceAndInteraction;
    Mover->SetIntent(Intent);
}

void APWCrowdFigure::SetClockRunning(const bool bRunning)
{
    bClockRunning = bRunning;
    const bool bMove = bRunning && TalkRemaining <= 0;
    Mover->SetComponentTickEnabled(bMove);
    GetCharacterMovement()->SetComponentTickEnabled(bMove);
    if (!bMove) GetCharacterMovement()->StopMovementImmediately();
}

void APWCrowdFigure::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (TalkRemaining > 0)
    {
        TalkRemaining = FMath::Max(0.0f, TalkRemaining - DeltaSeconds);
        SetClockRunning(bClockRunning);
    }
}

FText APWCrowdFigure::GetInteractionVerb(const APawn* By) const
{
    return CrowdPerson.ReceivesAvoidanceAndInteraction && !UPWInteractionComponent::IsWorldInputBlocked(By)
        ? NSLOCTEXT("PerfectWorldCrowd", "Talk", "Talk") : FText::GetEmpty();
}
FVector APWCrowdFigure::GetInteractionPoint() const { return Feet() + FVector(0, 0, Look.Height * .65); }
int32 APWCrowdFigure::GetInteractionPriority() const { return PWNpc::InteractionPriority; }

void APWCrowdFigure::Interact(APawn* By)
{
    if (!By || GetInteractionVerb(By).IsEmpty() || FVector::DistSquared2D(By->GetActorLocation(), Feet()) > FMath::Square(230.0)) return;
    if (auto* Interaction = By->FindComponentByClass<UPWInteractionComponent>())
    {
        Interaction->ShowNotice(PWCrowd::ExchangeLine(CrowdPerson.ShortExchangeTextKey));
        TalkRemaining = 2.5f;
        SetClockRunning(bClockRunning);
    }
}
