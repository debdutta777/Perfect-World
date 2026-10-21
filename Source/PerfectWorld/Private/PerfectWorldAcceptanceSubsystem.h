#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PerfectWorldAcceptanceSubsystem.generated.h"

class AActor;
class APerfectWorldFrontEndHUD;
class APerfectWorldPlayerCharacter;
class APlayerController;
class UPWInteractionComponent;
class UPWAcceptanceSubsystem;
class UWorld;

DECLARE_LOG_CATEGORY_EXTERN(LogPWAcceptance, Log, All);

/**
 * The packaged scenario driver (AW-01; Docs/NEXT_STEP_DISCUSSION_2026-10-18.md sections 2 and 3).
 *
 * It lets a Development package walk itself, offscreen, without the owner's keyboard or mouse, and write evidence. It does
 * NOTHING unless the command line names both a scenario and an output directory, and it never exists in a Shipping build:
 *
 *   PerfectWorld.exe -RenderOffScreen -PWAcceptance=boot_walk_interact -PWAcceptanceOut=D:/PWAcc/run1 -PWAcceptanceCommit=<sha>
 *
 * Optional: -PWAcceptanceStart=auto|new|continue (default auto: Continue when a save exists, else a new life) and
 * -PWAcceptanceSlot=<n> (the slot a new life is written to; default the first empty slot).
 *
 * THE CONTRACT (frozen 2026-10-18; AW-02's runner consumes it). One file per scenario, <out>/<scenario>.json:
 *   {"schema":1, "commit":"<sha or unknown>", "scenario":"<name>", "planned_steps":["<name>", ...],
 *    "steps":[{"name":"...", "ok":true|false, "detail":"...", "screenshot":"<file in out dir or empty>"}, ...],
 *    "completed":true|false, "passed":true|false}
 * The file is written at start (no steps, completed false) and rewritten atomically (temp file, then move) after EVERY step, so a
 * power-off leaves an honest partial file. completed is true only when the driver finished its script (a failed step ends the
 * script early but still completes) and asked the engine to exit; passed is completed AND every planned step ran and was ok.
 * Screenshots are <out>/<scenario>__<step>.png. Scenario names: boot_walk_interact, stationery_shift. Reserved, not built yet:
 * origins_seating_cameras, doors_knock, anindita_mira, save_restart.
 *
 * Rules the driver keeps: it drives the REAL possessed pawn with movement input and controller rotation, it uses the real
 * interaction component's TryInteract (the call the E key makes), and it teleports only at a scenario's declared start.
 * It changes no rule and overrides no production decision.
 */
namespace PWAcceptance
{
inline constexpr int32 ResultSchema = 1;

/** What the command line asked for. bActive is true only when BOTH -PWAcceptance= and -PWAcceptanceOut= carry a value. */
struct FArgs
{
    bool bActive = false;
    FString Scenario;         // sanitised to [a-z0-9_]; anything else becomes '_' so it can never leave the output directory
    FString OutDir;           // absolute
    FString Commit = TEXT("unknown");
    FString Start = TEXT("auto"); // auto, new or continue
    int32 Slot = -1;          // -1: first empty slot
};

PERFECTWORLD_API FArgs ParseArgs(const TCHAR* CommandLine);

/** False in a Shipping build, whatever the command line says. */
PERFECTWORLD_API bool IsBuildAllowed();

struct FStepRecord
{
    FString Name;
    bool bOk = false;
    FString Detail;
    FString Screenshot;
};

/** The result file. Pure file work, no engine objects, so tests drive it directly. */
class PERFECTWORLD_API FResultWriter
{
public:
    FResultWriter(const FString& InOutDir, const FString& InScenario, const FString& InCommit, const TArray<FString>& InPlanned);

    /** Appends a step and rewrites the file. Returns false when the file could not be written. */
    bool AddStep(const FStepRecord& Step);
    /** Marks the script finished and rewrites the file. */
    bool Complete();
    bool Flush();

    FString ToJson() const;
    const FString& GetPath() const { return Path; }
    const TArray<FStepRecord>& GetSteps() const { return Steps; }
    bool IsCompleted() const { return bCompleted; }
    bool IsPassed() const;

private:
    FString OutDir;
    FString Scenario;
    FString Commit;
    FString Path;
    TArray<FString> Planned;
    TArray<FStepRecord> Steps;
    bool bCompleted = false;
};

enum class EStepState : uint8
{
    Running,
    Passed,
    Failed
};

struct FStepOutcome
{
    EStepState State = EStepState::Running;
    FString Detail;
    FString Screenshot;

    static FStepOutcome Wait() { return FStepOutcome(); }
    static FStepOutcome Pass(const FString& InDetail, const FString& InShot = FString())
    {
        FStepOutcome Result;
        Result.State = EStepState::Passed;
        Result.Detail = InDetail;
        Result.Screenshot = InShot;
        return Result;
    }
    static FStepOutcome Fail(const FString& InDetail)
    {
        FStepOutcome Result;
        Result.State = EStepState::Failed;
        Result.Detail = InDetail;
        return Result;
    }
};

/** Scratch state of the step that is running; reset when the next step begins. */
struct FStepRuntime
{
    double Elapsed = 0.0;   // real seconds in this step
    double Dt = 0.0;        // the last frame, capped
    int32 Phase = 0;
    double PhaseStart = 0.0; // Elapsed when the phase began
    FVector LastPos = FVector::ZeroVector;
    double StuckSince = 0.0;
    double LastProbe = 0.0;
    FString ShotPath;
    int64 LastSize = -1;     // screenshot file size at the previous frame
    FVector Target = FVector::ZeroVector;
    bool bHaveTarget = false;
    FString Note;            // the latest thing worth saying if the step times out
    FString Memo;            // something a step remembers for its pass message (for example the prompt's verb)

    void NextPhase() { ++Phase; PhaseStart = Elapsed; }
    double InPhase() const { return Elapsed - PhaseStart; }
};

struct FStepDef
{
    FString Name;
    double TimeoutSeconds = 20.0;
    TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run;
};

/** Builds the steps of a scenario by name; false for an unknown name (PerfectWorldAcceptanceLaneScenario.cpp). */
bool BuildScenario(const FString& Name, TArray<FStepDef>& OutSteps);
/** stationery_shift (PerfectWorldAcceptanceStationery.cpp). */
void AppendStationeryShiftSteps(TArray<FStepDef>& OutSteps);
void AppendSaveRestartSteps(TArray<FStepDef>& OutSteps);
void AppendDoorsKnockSteps(TArray<FStepDef>& OutSteps);
void AppendAninditaMiraSteps(TArray<FStepDef>& OutSteps);
void AppendOriginsSeatingCamerasSteps(TArray<FStepDef>& OutSteps);
void AppendResidentHomesSteps(TArray<FStepDef>& OutSteps);
/** R4-02: Debashis and Mahua open, work and close their shops (PerfectWorldAcceptanceProprietors.cpp). */
void AppendProprietorsWorkdaySteps(TArray<FStepDef>& OutSteps);
/** R4-04: dated encounters and remembered agreements survive a fresh-process Continue (PerfectWorldAcceptanceJournalState.cpp). */
void AppendJournalStateSteps(TArray<FStepDef>& OutSteps);
/** R4-05: real conversations, then pause > Today shows core truth before and after Continue (PerfectWorldAcceptanceJournalUI.cpp). */
void AppendJournalTodaySteps(TArray<FStepDef>& OutSteps);
/** R5-02: real home visits in both cameras and a fresh-process agreement Continue pair. */
void AppendHomesRound5Steps(TArray<FStepDef>& OutSteps);
/** R5-03: the next three browsing frontages; fresh-process restart pair (PerfectWorldAcceptanceFrontagesR5.cpp). */
void AppendFrontagesRound5Steps(TArray<FStepDef>& OutSteps);
/** R5-05: independent physical visit to Mahua and her working day (PerfectWorldAcceptanceMahua.cpp). */
void AppendMahuaWorkdaySteps(TArray<FStepDef>& OutSteps);
/** R4-01: the three unstaffed browsing frontages (market_stalls_a, market_stalls_b, end_shop); a fresh-process pair (PerfectWorldAcceptanceFrontagesR4.cpp). */
void AppendFrontagesRound4Steps(TArray<FStepDef>& OutSteps);

// Step builders shared by scenarios (PerfectWorldAcceptanceLaneScenario.cpp).
FStepDef MakeWorldReadyStep();
FStepDef MakeEnterWorldStep();
FStepDef MakeShotStep(const FString& StepName);
/** Walks the real pawn to the point FindTarget returns (XY only) until within ToleranceCm. FindTarget may return false to fail the step. */
FStepDef MakeWalkStep(const FString& StepName, TFunction<bool(UPWAcceptanceSubsystem&, FVector&, FString&)> FindTarget,
                      double ToleranceCm, double TimeoutSeconds);
/**
 * Faces the actor FindTarget returns, waits for the real interaction prompt to name it, calls TryInteract (the E key's call) and waits
 * for IsDone. When SkipIf returns true the step passes without interacting (for example a door that is already open).
 */
FStepDef MakeInteractStep(const FString& StepName, TFunction<AActor*(UPWAcceptanceSubsystem&)> FindTarget,
                          TFunction<bool(UPWAcceptanceSubsystem&, FString&)> IsDone,
                          TFunction<bool(UPWAcceptanceSubsystem&)> SkipIf, double TimeoutSeconds);
} // namespace PWAcceptance

/** Runs one scenario of the packaged acceptance driver when the command line asks for it (see the namespace comment). */
UCLASS()
class PERFECTWORLD_API UPWAcceptanceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    const PWAcceptance::FArgs& GetArgs() const { return Args; }
    UWorld* GetPlayWorld() const;
    APlayerController* GetController() const;
    APerfectWorldPlayerCharacter* GetPawn() const;
    APerfectWorldFrontEndHUD* GetHud() const;
    UPWInteractionComponent* GetInteraction() const;

    /** Turns the controller (yaw only) to look at a world point. */
    void FaceWorldPoint(const FVector& Point) const;
    /** One frame of walking toward Target with real movement input; Running until within ToleranceCm (XY) or the pawn is stuck. */
    PWAcceptance::FStepOutcome TickWalk(const FVector& Target, double ToleranceCm, PWAcceptance::FStepRuntime& Rt) const;
    /** Requests a named screenshot; the file's path is returned in Rt.ShotPath. */
    void RequestShot(const FString& StepName, PWAcceptance::FStepRuntime& Rt) const;
    /** Running until the screenshot file exists and is non-empty. */
    PWAcceptance::FStepOutcome TickShot(PWAcceptance::FStepRuntime& Rt) const;

private:
    bool OnTick(float DeltaSeconds);
    void FinishScenario();

    PWAcceptance::FArgs Args;
    TUniquePtr<PWAcceptance::FResultWriter> Writer;
    TArray<PWAcceptance::FStepDef> Steps;
    PWAcceptance::FStepRuntime Runtime;
    int32 StepIndex = 0;
    double TotalSeconds = 0.0;
    int32 ExitGraceFrames = -1;
    FTSTicker::FDelegateHandle TickHandle;
};
