#include "PerfectWorldNpcPresenceSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldNpcPresenceInternal.h"

DEFINE_LOG_CATEGORY_STATIC(NpPresenceLog, Log, All);

namespace
{
const FName NpWaitingAnchor(TEXT("anchor.presentation.doorway_wait"));
const FName NpDepartureRoute(TEXT("route.presentation.departure"));

void NpDestroyActor(FNpPresenceRecord& Record)
{
    if (APWNpcCharacter* Actor = Record.Actor.Get()) { Actor->Destroy(); }
    Record.Actor.Reset();
    Record.Yield = {};
    Record.AppliedRoutePoints.Reset();
}

void NpApplyMovement(FNpPresenceRecord& Record, const FPWNpcIntentView& View, const TArray<FVector>& Points)
{
    UPWNpcMovementComponent* Movement = Record.Actor->GetNpcMovement();
    Movement->SetIntent(View);
    // Do not reset route projection/extrapolation on every presentation tick.
    if (View.State == EPWNpcState::Travel && Record.AppliedRoutePoints != Points)
    {
        Movement->SetRoute(Points);
        Record.AppliedRoutePoints = Points;
    }
}
}

bool UPWNpcPresenceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && World != nullptr && World->IsGameWorld();
}

void UPWNpcPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    SpatialBindings = MakeShared<FNpLaneNpcBindings>();
}

void UPWNpcPresenceSubsystem::Deinitialize()
{
    for (auto& Pair : People) { NpDestroyActor(*Pair.Value); }
    People.Reset();
    SpatialBindings.Reset();
    Super::Deinitialize();
}

TStatId UPWNpcPresenceSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UPWNpcPresenceSubsystem, STATGROUP_Tickables);
}

FNpLaneNpcBindings& UPWNpcPresenceSubsystem::Bindings()
{
    if (!SpatialBindings) { SpatialBindings = MakeShared<FNpLaneNpcBindings>(); }
    return *SpatialBindings;
}

bool UPWNpcPresenceSubsystem::RegisterScheduledPerson(const FName PersonId, const FText& DisplayName, const EPWNpcOutfit Outfit)
{
    const UGameInstance* Instance = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
    const UPWContentSubsystem* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    if (Content == nullptr || Content->GetDb() == nullptr)
    {
        UE_LOG(NpPresenceLog, Error, TEXT("NPC %s: validated content database unavailable; no spawn."), *PersonId.ToString());
        return false;
    }
    return RegisterScheduledPersonFromContent(*Content->GetDb(), PersonId, DisplayName, Outfit);
}

bool UPWNpcPresenceSubsystem::RegisterScheduledPersonFromContent(const PerfectWorld::ContentDb& Db, const FName PersonId,
                                                               const FText& DisplayName, const EPWNpcOutfit Outfit)
{
    if (PersonId.IsNone()) { return false; }
    if (People.Contains(PersonId)) { return true; } // Idempotent: never replace a live actor or duplicate its identity.
    const PerfectWorld::ContentRecord* Match = nullptr;
    const std::string Person(TCHAR_TO_UTF8(*PersonId.ToString()));
    for (const PerfectWorld::ContentRecord* Candidate : Db.OfType("npc_schedule"))
    {
        for (const auto& Field : Candidate->Data.Object)
        {
            if (Field.first != "person" || Field.second.String != Person) { continue; }
            if (Match != nullptr)
            {
                UE_LOG(NpPresenceLog, Error, TEXT("NPC %s: multiple authored schedules; no spawn."), *PersonId.ToString());
                return false;
            }
            Match = Candidate;
        }
    }
    if (Match == nullptr)
    {
        UE_LOG(NpPresenceLog, Error, TEXT("NPC %s: no authored npc_schedule record; no spawn."), *PersonId.ToString());
        return false;
    }
    auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Match->Id);
    if (!Loaded.Ok() || Loaded.Value->Person.PersonId != Person)
    {
        UE_LOG(NpPresenceLog, Error, TEXT("NPC %s schedule %hs refused: %hs; no spawn."),
               *PersonId.ToString(), Match->Id.c_str(), Loaded.Error.c_str());
        return false;
    }
    TSharedPtr<FNpPresenceRecord> Record = MakeShared<FNpPresenceRecord>();
    Record->Schedule = MoveTemp(*Loaded.Value);
    Record->DisplayName = DisplayName;
    Record->Outfit = Outfit;
    People.Add(PersonId, Record);
    if (bHasNow) { Reconcile(Now); }
    return true;
}

void UPWNpcPresenceSubsystem::UnregisterPerson(const FName PersonId)
{
    if (const auto* Record = People.Find(PersonId)) { NpDestroyActor(**Record); }
    People.Remove(PersonId);
}

void UPWNpcPresenceSubsystem::ResetPresentation(const PerfectWorld::GameInstant GameInstantNow)
{
    for (auto& Pair : People)
    {
        NpDestroyActor(*Pair.Value);
        Pair.Value->DeparturePoints.Reset();
        Pair.Value->DepartureAnchor = NAME_None;
    }
    Reconcile(GameInstantNow);
}

APWNpcCharacter* UPWNpcPresenceSubsystem::FindPerson(const FName PersonId) const
{
    const auto* Record = People.Find(PersonId);
    return Record != nullptr ? (*Record)->Actor.Get() : nullptr;
}

const PerfectWorld::NpcIntent* UPWNpcPresenceSubsystem::FindIntent(const FName PersonId) const
{
    const auto* Record = People.Find(PersonId);
    return Record != nullptr && (*Record)->bIntentValid ? &(*Record)->Intent : nullptr;
}

void UPWNpcPresenceSubsystem::Reconcile(const PerfectWorld::GameInstant GameInstantNow)
{
    Now = GameInstantNow;
    bHasNow = true;
    for (auto& Pair : People)
    {
        FNpPresenceRecord& Record = *Pair.Value;
        // Local Chat is held by the existing movement component; paid time still resolves the underlying schedule.
        const auto Resolved = PerfectWorld::ResolveNpcIntentAt(Record.Schedule.Person, Record.Schedule.Travel, Now, false, false);
        Record.bIntentValid = Resolved.Ok();
        if (!Resolved.Ok())
        {
            Record.Binding = {};
            Record.Binding.Diagnostic = FString::Printf(TEXT("intent refused '%hs'"),
                std::string(PerfectWorld::NpcIntentErrorCode(Resolved.Error)).c_str());
        }
        else
        {
            // On a paid skip from a stay directly to home, reconstruct the authored exit journey, not a straight wall crossing.
            const auto NewBinding = Bindings().Resolve(Record.Schedule, Resolved.Intent);
            if (Record.Binding.Kind == ENpBindingKind::Local && NewBinding.Kind == ENpBindingKind::OffLane)
            {
                PerfectWorld::NpcIntent ExitIntent = Resolved.Intent;
                ExitIntent.State = PerfectWorld::NpcState::Travel;
                ExitIntent.RouteId = "route:" + Record.Intent.AnchorId + "->" + Resolved.Intent.AnchorId;
                ExitIntent.ProgressPermille = 1000;
                const auto Exit = Bindings().Resolve(Record.Schedule, ExitIntent);
                if (Exit.Kind == ENpBindingKind::OffLane && Exit.RoutePoints.Num() >= 2)
                {
                    Record.DeparturePoints = Exit.RoutePoints;
                }
            }
            Record.Intent = Resolved.Intent;
            Record.Binding = NewBinding;
            if (Record.Binding.Kind == ENpBindingKind::Local && !Record.Binding.RoutePoints.IsEmpty())
            {
                Record.DeparturePoints = Record.Binding.RoutePoints;
            }
        }
        Present(Pair.Key, Record);
    }
}

bool UPWNpcPresenceSubsystem::IsPointVisible(const FVector& Feet) const
{
    const APlayerController* Controller = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (Controller == nullptr) { return false; } // Initial construction and isolated automation worlds.
    FVector Eye;
    FRotator View;
    Controller->GetPlayerViewPoint(Eye, View);
    for (const double Height : {30.0, 145.0})
    {
        const FVector Target = Feet + FVector(0.0, 0.0, Height);
        // Conservatively wider than the gameplay frustum: delaying an arrival is preferable to a visible pop.
        if (FVector::DotProduct((Target - Eye).GetSafeNormal(), View.Vector()) < 0.1) { continue; }
        FCollisionQueryParams Params(SCENE_QUERY_STAT(NpPresenceSight), false, Controller->GetPawn());
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByChannel(Hit, Eye, Target, ECC_Visibility, Params)) { return true; }
        if (FVector::DistSquared(Hit.ImpactPoint, Target) < FMath::Square(40.0)) { return true; }
    }
    return false;
}

void UPWNpcPresenceSubsystem::Present(const FName PersonId, FNpPresenceRecord& Record)
{
    if (Record.Binding.Kind == ENpBindingKind::Missing && Record.LastDiagnostic != Record.Binding.Diagnostic)
    {
        UE_LOG(NpPresenceLog, Error, TEXT("NPC %s at %lld: %s; no spawn."), *PersonId.ToString(),
               static_cast<long long>(Now), *Record.Binding.Diagnostic);
        Record.LastDiagnostic = Record.Binding.Diagnostic;
    }
    else if (Record.Binding.Kind != ENpBindingKind::Missing && Record.LastDiagnostic != TEXT("unmanaged duplicate"))
    {
        Record.LastDiagnostic.Reset();
    }
    APWNpcCharacter* Actor = Record.Actor.Get();
    if (Record.Binding.Kind != ENpBindingKind::Local)
    {
        if (Actor == nullptr) { return; }
        if (Actor->GetNpcMovement()->GetPresentedState() == EPWNpcState::Chat) { return; }
        const FVector Feet = Actor->GetActorLocation() - FVector(0.0, 0.0, Actor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        if (!Actor->WasRecentlyRendered(0.3f) && !IsPointVisible(Feet)) { NpDestroyActor(Record); return; }
        Record.Yield = {};
        if (Record.Binding.Kind == ENpBindingKind::Missing)
        {
            // Bad bindings never send an already visible person toward a fabricated default destination.
            FPWNpcIntentView Hold;
            Hold.State = EPWNpcState::Yield;
            Actor->GetNpcMovement()->SetIntent(Hold);
            return;
        }
        Record.DepartureAnchor = Record.Binding.ExitAnchor;
        if (Record.DeparturePoints.Num() >= 2)
        {
            FPWNpcIntentView Exit;
            Exit.State = EPWNpcState::Travel;
            Exit.RouteId = NpDepartureRoute;
            Exit.ProgressPermille = 1000;
            NpApplyMovement(Record, Exit, Record.DeparturePoints);
        }
        else
        {
            FPWNpcIntentView Exit;
            Exit.AnchorId = Record.DepartureAnchor;
            Actor->GetNpcMovement()->SetIntent(Exit);
        }
        return;
    }
    if (Actor == nullptr)
    {
        if (IsPointVisible(Record.Binding.Feet.GetLocation())) { return; } // Retry on tick when out of view.
        // Reject legacy/static duplicates as an integration error, rather than silently owning somebody else's actor.
        for (TActorIterator<APWNpcCharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetPersonId() == PersonId)
            {
                if (Record.LastDiagnostic != TEXT("unmanaged duplicate"))
                {
                    UE_LOG(NpPresenceLog, Error, TEXT("NPC %s: unmanaged duplicate; remove static spawn; no spawn."), *PersonId.ToString());
                    Record.LastDiagnostic = TEXT("unmanaged duplicate");
                }
                return;
            }
        }
        const double Half = Record.Outfit == EPWNpcOutfit::ShopProprietor ? 79.0
            : Record.Outfit == EPWNpcOutfit::ParentHousehold ? 84.0 : 81.0;
        const FVector Centre = Record.Binding.Feet.GetLocation() + FVector(0.0, 0.0, Half + 2.0);
        if (GetWorld()->OverlapBlockingTestByChannel(Centre, FQuat::Identity, ECC_Pawn,
            FCollisionShape::MakeCapsule(PWNpc::CapsuleRadius, static_cast<float>(Half)))) { return; }
        Actor = APWNpcCharacter::SpawnAt(*GetWorld(), Record.Binding.Feet, PersonId, Record.DisplayName, Record.Outfit);
        if (Actor == nullptr) { return; }
        Record.Actor = Actor;
        Record.LastDiagnostic.Reset();
        const TWeakObjectPtr<UPWNpcPresenceSubsystem> Weak(this);
        Actor->GetNpcMovement()->SetAnchorResolver([Weak, PersonId](const FName Id, FTransform& Out)
        {
            const auto* Presence = Weak.Get();
            return Presence != nullptr && Presence->ResolveActorAnchor(PersonId, Id, Out);
        });
        Actor->GetNpcMovement()->SetRouteResolver([Weak, PersonId](const FName Id, TArray<FVector>& Out)
        {
            const auto* Presence = Weak.Get();
            return Presence != nullptr && Presence->ResolveActorRoute(PersonId, Id, Out);
        });
    }
    if (Record.Yield.IsActive()) { return; } // Yield owns its temporary walking target until released.
    FPWNpcIntentView View = MakeNpcIntentView(Record.Intent, Record.Binding.bPreferSit);
    if (Record.Intent.State == PerfectWorld::NpcState::Travel)
    {
        View.ProgressPermille = Record.Binding.LocalProgress;
    }
    NpApplyMovement(Record, View, Record.Binding.RoutePoints);
}

bool UPWNpcPresenceSubsystem::ResolveActorAnchor(const FName PersonId, const FName AnchorId, FTransform& Out) const
{
    const auto* Record = People.Find(PersonId);
    if (Record == nullptr) { return false; }
    if (AnchorId == NpWaitingAnchor && (*Record)->Yield.IsActive())
    {
        Out = FTransform((*Record)->Yield.WaitingFeet);
        return true;
    }
    const FNpAnchorBinding* Anchor = SpatialBindings->FindAnchor(AnchorId);
    if (Anchor == nullptr || Anchor->Kind != ENpBindingKind::Local) { return false; }
    Out = Anchor->Feet;
    return true;
}

bool UPWNpcPresenceSubsystem::ResolveActorRoute(const FName PersonId, const FName RouteId, TArray<FVector>& Out) const
{
    const auto* Record = People.Find(PersonId);
    if (Record == nullptr) { return false; }
    Out = RouteId == NpDepartureRoute ? (*Record)->DeparturePoints : (*Record)->Binding.RoutePoints;
    return Out.Num() >= 2;
}

void UPWNpcPresenceSubsystem::Tick(const float DeltaTime)
{
    if (!bHasNow) { return; }
    for (auto& Pair : People)
    {
        // Providers may be added after world construction. Missing bindings recover without a new clock minute.
        if (Pair.Value->bIntentValid) { Pair.Value->Binding = Bindings().Resolve(Pair.Value->Schedule, Pair.Value->Intent); }
        Present(Pair.Key, *Pair.Value);
        UpdateYield(Pair.Key, *Pair.Value, DeltaTime);
    }
}
