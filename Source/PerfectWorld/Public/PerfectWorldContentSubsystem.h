#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ContentRegistry.h"
#include "PerfectWorldContentSubsystem.generated.h"

/** One validated content database per game instance, shared by its played worlds. */
UCLASS()
class PERFECTWORLD_API UPWContentSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Null if a file could not be read, the root was empty, or core validation failed. */
    const PerfectWorld::ContentDb* GetDb() const { return bLoaded ? &Db : nullptr; }

private:
    PerfectWorld::ContentDb Db;
    bool bLoaded = false;
};
