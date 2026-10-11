#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "UObject/WeakObjectPtrTemplates.h"

/**
 * What the creation screens read. It caches the subsystem's views so widgets can
 * poll cheaply every frame, and it is refreshed after every change. It holds no
 * rules: every value comes from, and every edit goes through, the subsystem and
 * so the portable core.
 */
class FPWCreationModel
{
public:
    explicit FPWCreationModel(UPerfectWorldCharacterCreatorSubsystem* InSubsystem);

    UPerfectWorldCharacterCreatorSubsystem* GetSubsystem() const { return Subsystem.Get(); }

    /** Re-reads every view from the subsystem. */
    void Refresh();

    /**
     * Call with the result of a subsystem edit. A failure shows the core's own
     * message; a success clears any earlier one. Always refreshes. Returns the
     * argument so callers can chain on it.
     */
    bool Commit(bool bSucceeded);

    /** Runs one subsystem edit and commits its result. A missing subsystem counts as a failed edit. */
    bool Edit(TFunctionRef<bool(UPerfectWorldCharacterCreatorSubsystem&)> Change);

    void ClearMessage();

    TArray<FPWHouseholdView> Households;
    TArray<FPWTraitView> Traits;
    TArray<FPWPresetView> Presets;
    TArray<FPWStreamView> Streams;
    FPWIdentityView Identity;
    FString SchoolReason;
    FPWOpeningHomePlan OpeningHome;
    FPWCreationSummary Summary;

    /** The last refusal from the core, shown on the active page; empty when there is none. */
    FText Message;

private:
    TWeakObjectPtr<UPerfectWorldCharacterCreatorSubsystem> Subsystem;
};
