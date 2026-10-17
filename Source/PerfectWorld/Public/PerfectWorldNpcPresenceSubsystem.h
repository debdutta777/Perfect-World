#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameClock.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.generated.h"

namespace PerfectWorld { class ContentDb; struct NpcIntent; }
class FNpLaneNpcBindings;
struct FNpPresenceRecord;

/** One schedule-driven presentation per stable person ID. Never changes core outcomes. */
UCLASS()
class PERFECTWORLD_API UPWNpcPresenceSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    bool RegisterScheduledPerson(FName PersonId, const FText& DisplayName, EPWNpcOutfit Outfit);
    /** Same production validation, with an explicit content database (also useful for isolated worlds). */
    bool RegisterScheduledPersonFromContent(const PerfectWorld::ContentDb& Db, FName PersonId,
                                           const FText& DisplayName, EPWNpcOutfit Outfit);
    void Reconcile(PerfectWorld::GameInstant GameInstantNow);
    void UnregisterPerson(FName PersonId);
    /** Reset/load discards transient detours and presentation actors, retaining content and providers. */
    void ResetPresentation(PerfectWorld::GameInstant GameInstantNow);
    APWNpcCharacter* FindPerson(FName PersonId) const;
    const PerfectWorld::NpcIntent* FindIntent(FName PersonId) const;
    int32 RegisteredPersonCount() const { return People.Num(); }
    bool IsRegistered(FName PersonId) const { return People.Contains(PersonId); }
    FNpLaneNpcBindings& Bindings();

private:
    void Present(FName PersonId, FNpPresenceRecord& Record);
    void UpdateYield(FName PersonId, FNpPresenceRecord& Record, float Dt);
    bool ResolveActorAnchor(FName PersonId, FName AnchorId, FTransform& Out) const;
    bool ResolveActorRoute(FName PersonId, FName RouteId, TArray<FVector>& Out) const;
    bool IsPointVisible(const FVector& Feet) const;

    TMap<FName, TSharedPtr<FNpPresenceRecord>> People;
    TSharedPtr<FNpLaneNpcBindings> SpatialBindings;
    PerfectWorld::GameInstant Now = 0;
    bool bHasNow = false;
};
