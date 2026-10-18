#include "PerfectWorldNpcPresenceSubsystem.h"

#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldNpcPresenceInternal.h"

DEFINE_LOG_CATEGORY_STATIC(UaPresenceLog, Log, All);

namespace
{
const FName UaWaitingAnchor(TEXT("anchor.presentation.doorway_wait"));
const FName UaDepartureRoute(TEXT("route.presentation.departure"));
const FName UaArrivalRoute(TEXT("route.presentation.arrival"));

void UaResolveArrival(FNpPresenceRecord& Record, const FNpLaneNpcBindings& Bindings,
                      const PerfectWorld::GameInstant Now)
{
    Record.UaArrivalPoints.Reset();
    if (Record.Actor.IsValid() || Record.Binding.Kind != ENpBindingKind::Local) { return; }
    auto Journey = Record.Intent;
    // A load has no previous presentation intent. Find the journey immediately preceding this stay,
    // using the authoritative resolver (including the previous day), never a guessed source location.
    for (int32 Back = 1; Journey.State != PerfectWorld::NpcState::Travel && Back <= 7 * 1440; ++Back)
    {
        const auto Previous = PerfectWorld::ResolveNpcIntentAt(Record.Schedule.Person, Record.Schedule.Travel, Now - Back, false, false);
        if (!Previous.Ok() || Previous.Intent.AnchorId != Record.Intent.AnchorId) { return; }
        Journey = Previous.Intent;
    }
    if (Journey.State != PerfectWorld::NpcState::Travel) { return; }
    for (const auto& Path : Record.Schedule.Paths)
    {
        const std::string* Source = nullptr;
        if (Journey.RouteId == "route:" + Path.From + "->" + Path.To) { Source = &Path.From; }
        else if (Journey.RouteId == "route:" + Path.To + "->" + Path.From) { Source = &Path.To; }
        if (Source == nullptr) { continue; }
        const auto* Origin = Bindings.FindAnchor(FName(UTF8_TO_TCHAR(Source->c_str())));
        if (Origin == nullptr || Origin->Kind != ENpBindingKind::OffLane) { return; }
        const auto* Threshold = Bindings.FindAnchor(Origin->ExitAnchor);
        const auto Route = Bindings.Resolve(Record.Schedule, Journey);
        if (Threshold == nullptr || Threshold->Kind != ENpBindingKind::Local || Route.Kind == ENpBindingKind::Missing ||
            Route.RoutePoints.Num() < 2 || !Route.RoutePoints[0].Equals(Threshold->Feet.GetLocation(), 1.0))
        {
            Record.Binding.Kind = ENpBindingKind::Missing;
            Record.Binding.Diagnostic = FString::Printf(TEXT("arrival threshold/route unavailable '%hs'"), Journey.RouteId.c_str());
            return;
        }
        Record.UaArrivalPoints = Route.RoutePoints;
        Record.UaArrivalFeet = Threshold->Feet;
        return;
    }
}

void UaResolveDeparture(FNpPresenceRecord& Record, const FNpLaneNpcBindings& Bindings,
                        const PerfectWorld::GameInstant Now)
{
    auto Journey = Record.Intent;
    // Use the first journey crossed by the jump. The final off-lane stay can be days later
    // and need not have a direct path from the old local anchor.
    const auto Limit = FMath::Min<PerfectWorld::GameInstant>(Now, Record.UaIntentAt + 7 * 1440);
    for (auto At = Record.UaIntentAt; At <= Limit; ++At)
    {
        if (At != Record.UaIntentAt)
        {
            const auto Next = PerfectWorld::ResolveNpcIntentAt(Record.Schedule.Person, Record.Schedule.Travel, At, false, false);
            if (!Next.Ok()) { return; }
            Journey = Next.Intent;
        }
        if (Journey.State == PerfectWorld::NpcState::Travel)
        {
            const auto* Destination = Bindings.FindAnchor(FName(UTF8_TO_TCHAR(Journey.AnchorId.c_str())));
            if (Destination != nullptr && Destination->Kind == ENpBindingKind::OffLane)
            {
                Journey.ProgressPermille = 1000;
                const auto Exit = Bindings.Resolve(Record.Schedule, Journey);
                if (Exit.Kind != ENpBindingKind::Missing && Exit.RoutePoints.Num() >= 2)
                {
                    Record.DeparturePoints = Exit.RoutePoints;
                    Record.DepartureAnchor = Destination->ExitAnchor;
                    return;
                }
            }
        }
    }
}

void UaDestroyActor(FNpPresenceRecord& Record)
{
    if (APWNpcCharacter* Actor = Record.Actor.Get()) { Actor->Destroy(); }
    Record.Actor.Reset();
    Record.Yield = {};
    Record.AppliedRoutePoints.Reset();
    Record.UaArrivalPoints.Reset();
}

void UaApplyMovement(FNpPresenceRecord& Record, const FPWNpcIntentView& View, const TArray<FVector>& Points)
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
    for (auto& Pair : People) { UaDestroyActor(*Pair.Value); }
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
        UE_LOG(UaPresenceLog, Error, TEXT("NPC %s: validated content database unavailable; no spawn."), *PersonId.ToString());
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
                UE_LOG(UaPresenceLog, Error, TEXT("NPC %s: multiple authored schedules; no spawn."), *PersonId.ToString());
                return false;
            }
            Match = Candidate;
        }
    }
    if (Match == nullptr)
    {
        UE_LOG(UaPresenceLog, Error, TEXT("NPC %s: no authored npc_schedule record; no spawn."), *PersonId.ToString());
        return false;
    }
    auto Loaded = PerfectWorld::LoadNpcSchedule(Db, Match->Id);
    if (!Loaded.Ok() || Loaded.Value->Person.PersonId != Person)
    {
        UE_LOG(UaPresenceLog, Error, TEXT("NPC %s schedule %hs refused: %hs; no spawn."),
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
    if (const auto* Record = People.Find(PersonId)) { UaDestroyActor(**Record); }
    People.Remove(PersonId);
}

void UPWNpcPresenceSubsystem::ResetPresentation(const PerfectWorld::GameInstant GameInstantNow)
{
    for (auto& Pair : People)
    {
        UaDestroyActor(*Pair.Value);
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
            const auto NewBinding = Bindings().Resolve(Record.Schedule, Resolved.Intent);
            if (Record.Binding.Kind == ENpBindingKind::Local && NewBinding.Kind == ENpBindingKind::OffLane)
            {
                UaResolveDeparture(Record, Bindings(), Now);
            }
            Record.Intent = Resolved.Intent;
            Record.UaIntentAt = Now;
            Record.Binding = NewBinding;
            if (NewBinding.Kind == ENpBindingKind::Local) { Record.DepartureAnchor = NAME_None; }
            UaResolveArrival(Record, Bindings(), Now);
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
        FCollisionQueryParams Params(SCENE_QUERY_STAT(UaPresenceSight), false, Controller->GetPawn());
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
        UE_LOG(UaPresenceLog, Error, TEXT("NPC %s at %lld: %s; no spawn."), *PersonId.ToString(),
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
        if (Record.DepartureAnchor.IsNone()) { Record.DepartureAnchor = Record.Binding.ExitAnchor; }
        const auto* UaThreshold = Bindings().FindAnchor(Record.DepartureAnchor);
        const bool UaAtExit = UaThreshold != nullptr && UaThreshold->Kind == ENpBindingKind::Local &&
            FVector::Dist2D(Feet, UaThreshold->Feet.GetLocation()) <= PWNpcMove::ArrivalToleranceCm &&
            FMath::Abs(Feet.Z - UaThreshold->Feet.GetLocation().Z) <= PWNpcMove::ArrivalToleranceCm;
        if ((Record.Binding.Kind == ENpBindingKind::Missing || UaAtExit) &&
            !Actor->WasRecentlyRendered(0.3f) && !IsPointVisible(Feet)) { UaDestroyActor(Record); return; }
        Record.Yield = {};
        if (Record.Binding.Kind == ENpBindingKind::Missing)
        {
            // Bad bindings never send an already visible person toward a fabricated default destination.
            FPWNpcIntentView Hold;
            Hold.State = EPWNpcState::Yield;
            Actor->GetNpcMovement()->SetIntent(Hold);
            return;
        }
        if (UaThreshold != nullptr && Record.DeparturePoints.Num() >= 2 &&
            Record.DeparturePoints.Last().Equals(UaThreshold->Feet.GetLocation(), 1.0))
        {
            FPWNpcIntentView Exit;
            Exit.State = EPWNpcState::Travel;
            Exit.RouteId = UaDepartureRoute;
            Exit.ProgressPermille = 1000;
            UaApplyMovement(Record, Exit, Record.DeparturePoints);
        }
        else
        {
            FPWNpcIntentView Exit;
            Exit.State = EPWNpcState::Yield; // No authored exit route: hold rather than walk through geometry.
            Actor->GetNpcMovement()->SetIntent(Exit);
        }
        return;
    }
    if (Actor == nullptr)
    {
        const FTransform UaSpawnFeet = Record.UaArrivalPoints.IsEmpty() ? Record.Binding.Feet : Record.UaArrivalFeet;
        if (IsPointVisible(UaSpawnFeet.GetLocation())) { return; } // Retry on tick when out of view.
        // Reject legacy/static duplicates as an integration error, rather than silently owning somebody else's actor.
        for (TActorIterator<APWNpcCharacter> It(GetWorld()); It; ++It)
        {
            if (It->GetPersonId() == PersonId)
            {
                if (Record.LastDiagnostic != TEXT("unmanaged duplicate"))
                {
                    UE_LOG(UaPresenceLog, Error, TEXT("NPC %s: unmanaged duplicate; remove static spawn; no spawn."), *PersonId.ToString());
                    Record.LastDiagnostic = TEXT("unmanaged duplicate");
                }
                return;
            }
        }
        // The same per-person standing height that sizes the body (UP-04) decides whether there is room to appear.
        if (!PWNpc::HasSpawnClearance(*GetWorld(), UaSpawnFeet.GetLocation(), PersonId, Record.Outfit)) { return; }
        Actor = APWNpcCharacter::SpawnAt(*GetWorld(), UaSpawnFeet, PersonId, Record.DisplayName, Record.Outfit);
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
        if (!Record.UaArrivalPoints.IsEmpty() && Record.Intent.State != PerfectWorld::NpcState::Travel)
        {
            FPWNpcIntentView UaArrival = MakeNpcIntentView(Record.Intent);
            UaArrival.State = EPWNpcState::Travel;
            UaArrival.RouteId = UaArrivalRoute;
            UaArrival.ProgressPermille = 1000;
            UaApplyMovement(Record, UaArrival, Record.UaArrivalPoints);
            // The work intent below hands this polyline to the mover's existing FinishRoute.
        }
    }
    if (Record.Yield.IsActive()) { return; } // Yield owns its temporary walking target until released.
    FPWNpcIntentView View = MakeNpcIntentView(Record.Intent, Record.Binding.bPreferSit);
    if (Record.Intent.State == PerfectWorld::NpcState::Travel)
    {
        View.ProgressPermille = Record.Binding.LocalProgress;
    }
    UaApplyMovement(Record, View, Record.Binding.RoutePoints);
}

bool UPWNpcPresenceSubsystem::ResolveActorAnchor(const FName PersonId, const FName AnchorId, FTransform& Out) const
{
    const auto* Record = People.Find(PersonId);
    if (Record == nullptr) { return false; }
    if (AnchorId == UaWaitingAnchor && (*Record)->Yield.IsActive())
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
    Out = RouteId == UaDepartureRoute ? (*Record)->DeparturePoints :
        (RouteId == UaArrivalRoute ? (*Record)->UaArrivalPoints : (*Record)->Binding.RoutePoints);
    return Out.Num() >= 2;
}

void UPWNpcPresenceSubsystem::Tick(const float DeltaTime)
{
    if (!bHasNow) { return; }
    for (auto& Pair : People)
    {
        // Providers may be added after world construction. Missing bindings recover without a new clock minute.
        if (Pair.Value->bIntentValid)
        {
            const bool UaWasMissing = Pair.Value->Binding.Kind == ENpBindingKind::Missing;
            Pair.Value->Binding = Bindings().Resolve(Pair.Value->Schedule, Pair.Value->Intent);
            if (UaWasMissing) { UaResolveArrival(*Pair.Value, Bindings(), Now); }
        }
        Present(Pair.Key, *Pair.Value);
        UpdateYield(Pair.Key, *Pair.Value, DeltaTime);
    }
}
