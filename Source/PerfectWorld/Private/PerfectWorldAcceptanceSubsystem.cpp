#include "PerfectWorldAcceptanceSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldPlayerCharacter.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY(LogPWAcceptance);

namespace
{
/** One scenario may run this long in all; a step's own timeout normally ends it long before. */
constexpr double AwScenarioBudgetSeconds = 900.0;
/** A frame longer than this counts as this much, so a hitch never reads as a long walk. */
constexpr double AwMaxFrameSeconds = 0.25;
/** The pawn is stuck when it moved less than this in each of the last AwStuckProbes one-second probes. */
constexpr double AwStuckMoveCm = 12.0;
constexpr int32 AwStuckProbes = 3;
constexpr int64 AwMinShotBytes = 1024;
constexpr double AwShotWaitSeconds = 15.0;
constexpr int32 AwExitGraceFrames = 3;

FString AwJsonEscape(const FString& Text)
{
    FString Out;
    Out.Reserve(Text.Len() + 8);
    for (const TCHAR Char : Text)
    {
        switch (Char)
        {
        case TEXT('"'): Out += TEXT("\\\""); break;
        case TEXT('\\'): Out += TEXT("\\\\"); break;
        case TEXT('\n'): Out += TEXT("\\n"); break;
        case TEXT('\r'): Out += TEXT("\\r"); break;
        case TEXT('\t'): Out += TEXT("\\t"); break;
        default:
            if (Char < 0x20)
            {
                Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Char));
            }
            else
            {
                Out.AppendChar(Char);
            }
        }
    }
    return Out;
}

FString AwQuoted(const FString& Text)
{
    return FString(TEXT("\"")) + AwJsonEscape(Text) + TEXT("\"");
}

FString AwSanitiseName(const FString& Raw, const int32 MaxLength)
{
    FString Out = Raw.TrimStartAndEnd().ToLower().Left(MaxLength);
    for (TCHAR& Char : Out)
    {
        const bool bOk = (Char >= TEXT('a') && Char <= TEXT('z')) || (Char >= TEXT('0') && Char <= TEXT('9')) || Char == TEXT('_');
        if (!bOk)
        {
            Char = TEXT('_');
        }
    }
    return Out;
}
} // namespace

namespace PWAcceptance
{
bool IsBuildAllowed()
{
#if UE_BUILD_SHIPPING
    return false;
#else
    return true;
#endif
}

FArgs ParseArgs(const TCHAR* CommandLine)
{
    FArgs Result;
    if (CommandLine == nullptr)
    {
        return Result;
    }
    FString Scenario;
    FString Out;
    const bool bHasScenario = FParse::Value(CommandLine, TEXT("PWAcceptance="), Scenario, false);
    const bool bHasOut = FParse::Value(CommandLine, TEXT("PWAcceptanceOut="), Out, false);
    Scenario = Scenario.TrimStartAndEnd();
    Out = Out.TrimStartAndEnd();
    // FParse skips whitespace after '=', so an empty value would swallow the next argument; a value that starts like a flag is empty.
    if (!bHasScenario || !bHasOut || Scenario.IsEmpty() || Out.IsEmpty() || Scenario.StartsWith(TEXT("-")) || Out.StartsWith(TEXT("-")))
    {
        return Result;
    }
    Result.bActive = true;
    Result.Scenario = AwSanitiseName(Scenario, 64);
    Result.OutDir = FPaths::ConvertRelativePathToFull(Out);
    FPaths::NormalizeDirectoryName(Result.OutDir);

    FString Commit;
    if (FParse::Value(CommandLine, TEXT("PWAcceptanceCommit="), Commit, false) && !Commit.TrimStartAndEnd().IsEmpty() &&
        !Commit.TrimStartAndEnd().StartsWith(TEXT("-")))
    {
        Result.Commit = Commit.TrimStartAndEnd().Left(80);
    }
    FString Start;
    if (FParse::Value(CommandLine, TEXT("PWAcceptanceStart="), Start, false))
    {
        Start = Start.TrimStartAndEnd().ToLower();
        if (Start == TEXT("new") || Start == TEXT("continue"))
        {
            Result.Start = Start;
        }
    }
    int32 Slot = -1;
    if (FParse::Value(CommandLine, TEXT("PWAcceptanceSlot="), Slot) && Slot >= 0)
    {
        Result.Slot = Slot;
    }
    return Result;
}

// ---- The result file ----

FResultWriter::FResultWriter(const FString& InOutDir, const FString& InScenario, const FString& InCommit, const TArray<FString>& InPlanned)
    : OutDir(InOutDir), Scenario(InScenario), Commit(InCommit), Planned(InPlanned)
{
    Path = FPaths::Combine(OutDir, Scenario + TEXT(".json"));
    Flush(); // proof that the run started, before anything can go wrong
}

bool FResultWriter::AddStep(const FStepRecord& Step)
{
    Steps.Add(Step);
    return Flush();
}

bool FResultWriter::Complete()
{
    bCompleted = true;
    return Flush();
}

bool FResultWriter::IsPassed() const
{
    if (!bCompleted || Steps.Num() != Planned.Num())
    {
        return false;
    }
    for (int32 Index = 0; Index < Steps.Num(); ++Index)
    {
        if (!Steps[Index].bOk || Steps[Index].Name != Planned[Index])
        {
            return false;
        }
    }
    return true;
}

FString FResultWriter::ToJson() const
{
    FString Json;
    Json += TEXT("{\n");
    Json += FString::Printf(TEXT("  \"schema\": %d,\n"), ResultSchema);
    Json += TEXT("  \"commit\": ") + AwQuoted(Commit) + TEXT(",\n");
    Json += TEXT("  \"scenario\": ") + AwQuoted(Scenario) + TEXT(",\n");
    Json += TEXT("  \"planned_steps\": [");
    for (int32 Index = 0; Index < Planned.Num(); ++Index)
    {
        Json += (Index > 0 ? TEXT(", ") : TEXT("")) + AwQuoted(Planned[Index]);
    }
    Json += TEXT("],\n  \"steps\": [");
    for (int32 Index = 0; Index < Steps.Num(); ++Index)
    {
        const FStepRecord& Step = Steps[Index];
        Json += Index > 0 ? TEXT(",\n    ") : TEXT("\n    ");
        Json += TEXT("{\"name\": ") + AwQuoted(Step.Name) + TEXT(", \"ok\": ") + (Step.bOk ? TEXT("true") : TEXT("false"));
        Json += TEXT(", \"detail\": ") + AwQuoted(Step.Detail) + TEXT(", \"screenshot\": ") + AwQuoted(Step.Screenshot) + TEXT("}");
    }
    Json += Steps.Num() > 0 ? TEXT("\n  ],\n") : TEXT("],\n");
    Json += FString(TEXT("  \"completed\": ")) + (bCompleted ? TEXT("true") : TEXT("false")) + TEXT(",\n");
    Json += FString(TEXT("  \"passed\": ")) + (IsPassed() ? TEXT("true") : TEXT("false")) + TEXT("\n}\n");
    return Json;
}

bool FResultWriter::Flush()
{
    IFileManager& Files = IFileManager::Get();
    Files.MakeDirectory(*OutDir, true);
    const FString Json = ToJson();
    const FString Temp = Path + TEXT(".tmp");
    // Write beside the result, then move over it: a power-off leaves the previous complete file, never half of a new one.
    if (FFileHelper::SaveStringToFile(Json, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
        Files.Move(*Path, *Temp, true, true, false, true))
    {
        return true;
    }
    Files.Delete(*Temp, false, true, true);
    return FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
} // namespace PWAcceptance

// ---- The subsystem ----

bool UPWAcceptanceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    if (!PWAcceptance::IsBuildAllowed() || IsRunningCommandlet() || IsRunningDedicatedServer())
    {
        return false;
    }
    return PWAcceptance::ParseArgs(FCommandLine::Get()).bActive;
}

void UPWAcceptanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Args = PWAcceptance::ParseArgs(FCommandLine::Get());
    if (!Args.bActive || !PWAcceptance::IsBuildAllowed())
    {
        return;
    }
    if (!PWAcceptance::BuildScenario(Args.Scenario, Steps))
    {
        Steps.Reset();
        PWAcceptance::FStepDef Unknown;
        Unknown.Name = TEXT("scenario_known");
        Unknown.TimeoutSeconds = 5.0;
        const FString Name = Args.Scenario;
        Unknown.Run = [Name](UPWAcceptanceSubsystem&, PWAcceptance::FStepRuntime&)
        {
            return PWAcceptance::FStepOutcome::Fail(FString::Printf(TEXT("'%s' is not a scenario this build knows (boot_walk_interact, stationery_shift)."), *Name));
        };
        Steps.Add(Unknown);
    }
    TArray<FString> Planned;
    for (const PWAcceptance::FStepDef& Step : Steps)
    {
        Planned.Add(Step.Name);
    }
    Writer = MakeUnique<PWAcceptance::FResultWriter>(Args.OutDir, Args.Scenario, Args.Commit, Planned);
    UE_LOG(LogPWAcceptance, Display, TEXT("Acceptance: scenario '%s', %d steps, result %s"), *Args.Scenario, Steps.Num(), *Writer->GetPath());
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UPWAcceptanceSubsystem::OnTick), 0.0f);
}

void UPWAcceptanceSubsystem::Deinitialize()
{
    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }
    Super::Deinitialize();
}

UWorld* UPWAcceptanceSubsystem::GetPlayWorld() const
{
    const UGameInstance* Instance = GetGameInstance();
    return Instance != nullptr ? Instance->GetWorld() : nullptr;
}

APlayerController* UPWAcceptanceSubsystem::GetController() const
{
    UWorld* World = GetPlayWorld();
    return World != nullptr ? World->GetFirstPlayerController() : nullptr;
}

APerfectWorldPlayerCharacter* UPWAcceptanceSubsystem::GetPawn() const
{
    const APlayerController* Controller = GetController();
    return Controller != nullptr ? Cast<APerfectWorldPlayerCharacter>(Controller->GetPawn()) : nullptr;
}

APerfectWorldFrontEndHUD* UPWAcceptanceSubsystem::GetHud() const
{
    const APlayerController* Controller = GetController();
    return Controller != nullptr ? Cast<APerfectWorldFrontEndHUD>(Controller->GetHUD()) : nullptr;
}

UPWInteractionComponent* UPWAcceptanceSubsystem::GetInteraction() const
{
    const APerfectWorldPlayerCharacter* Pawn = GetPawn();
    return Pawn != nullptr ? Pawn->FindComponentByClass<UPWInteractionComponent>() : nullptr;
}

void UPWAcceptanceSubsystem::FaceWorldPoint(const FVector& Point) const
{
    APlayerController* Controller = GetController();
    const APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;
    if (Pawn == nullptr)
    {
        return;
    }
    FVector Direction = Point - Pawn->GetActorLocation();
    Direction.Z = 0.0;
    if (!Direction.IsNearlyZero())
    {
        Controller->SetControlRotation(FRotator(0.0, Direction.Rotation().Yaw, 0.0));
    }
}

PWAcceptance::FStepOutcome UPWAcceptanceSubsystem::TickWalk(const FVector& Target, const double ToleranceCm, PWAcceptance::FStepRuntime& Rt) const
{
    using PWAcceptance::FStepOutcome;
    APerfectWorldPlayerCharacter* Pawn = GetPawn();
    if (Pawn == nullptr)
    {
        return FStepOutcome::Fail(TEXT("there is no possessed player character to walk"));
    }
    if (UPWInteractionComponent::IsWorldInputBlocked(Pawn))
    {
        return FStepOutcome::Fail(TEXT("a menu or conversation owns the input, so the pawn cannot walk"));
    }
    if (Pawn->IsSeated())
    {
        return FStepOutcome::Fail(TEXT("the pawn is seated"));
    }
    const FVector Here = Pawn->GetActorLocation();
    const FVector2D Delta(Target.X - Here.X, Target.Y - Here.Y);
    const double Distance = Delta.Size();
    Rt.Note = FString::Printf(TEXT("at (%.0f, %.0f), %.0f cm from the target (%.0f, %.0f)"), Here.X, Here.Y, Distance, Target.X, Target.Y);
    if (Distance <= ToleranceCm)
    {
        return FStepOutcome::Pass(FString::Printf(TEXT("reached within %.0f cm of (%.0f, %.0f) at (%.0f, %.0f, %.0f) after %.1f s of walking"),
                                                  Distance, Target.X, Target.Y, Here.X, Here.Y, Here.Z, Rt.Elapsed));
    }
    if (Rt.LastProbe <= 0.0)
    {
        Rt.LastPos = Here;
        Rt.LastProbe = Rt.Elapsed;
    }
    else if (Rt.Elapsed - Rt.LastProbe >= 1.0)
    {
        const double Moved = FVector::Dist2D(Here, Rt.LastPos);
        Rt.StuckSince = Moved < AwStuckMoveCm ? Rt.StuckSince + 1.0 : 0.0;
        Rt.LastPos = Here;
        Rt.LastProbe = Rt.Elapsed;
        if (Rt.StuckSince >= AwStuckProbes)
        {
            return FStepOutcome::Fail(FString::Printf(TEXT("blocked: the pawn made no progress for %d s. %s"), AwStuckProbes, *Rt.Note));
        }
    }
    const FVector Direction(Delta.X / Distance, Delta.Y / Distance, 0.0);
    if (APlayerController* Controller = GetController())
    {
        Controller->SetControlRotation(FRotator(0.0, Direction.Rotation().Yaw, 0.0));
    }
    Pawn->AddMovementInput(Direction, 1.0f);
    return FStepOutcome::Wait();
}

void UPWAcceptanceSubsystem::RequestShot(const FString& StepName, PWAcceptance::FStepRuntime& Rt) const
{
    Rt.ShotPath = FPaths::Combine(Args.OutDir, Args.Scenario + TEXT("__") + StepName + TEXT(".png"));
    Rt.LastSize = -1;
    IFileManager::Get().MakeDirectory(*Args.OutDir, true);
    IFileManager::Get().Delete(*Rt.ShotPath, false, true, true);
    FScreenshotRequest::RequestScreenshot(Rt.ShotPath, /*bInShowUI=*/true, /*bAddFilenameSuffix=*/false);
}

PWAcceptance::FStepOutcome UPWAcceptanceSubsystem::TickShot(PWAcceptance::FStepRuntime& Rt) const
{
    using PWAcceptance::FStepOutcome;
    const int64 Size = IFileManager::Get().FileSize(*Rt.ShotPath);
    Rt.Note = FString::Printf(TEXT("waiting for %s (size %lld)"), *Rt.ShotPath, Size);
    if (Size >= AwMinShotBytes && Size == Rt.LastSize && !FScreenshotRequest::IsScreenshotRequested())
    {
        FIntPoint View(0, 0);
        if (GEngine != nullptr && GEngine->GameViewport != nullptr && GEngine->GameViewport->Viewport != nullptr)
        {
            View = GEngine->GameViewport->Viewport->GetSizeXY();
        }
        return FStepOutcome::Pass(FString::Printf(TEXT("%lld bytes, viewport %dx%d"), Size, View.X, View.Y), FPaths::GetCleanFilename(Rt.ShotPath));
    }
    Rt.LastSize = Size;
    if (Rt.InPhase() > AwShotWaitSeconds)
    {
        return FStepOutcome::Fail(FString::Printf(TEXT("no usable screenshot file after %.0f s (offscreen capture did not produce %s)"), AwShotWaitSeconds, *Rt.ShotPath));
    }
    return FStepOutcome::Wait();
}

bool UPWAcceptanceSubsystem::OnTick(const float DeltaSeconds)
{
    if (ExitGraceFrames >= 0)
    {
        if (--ExitGraceFrames <= 0)
        {
            UE_LOG(LogPWAcceptance, Display, TEXT("Acceptance: finished (passed=%s); requesting exit."), Writer.IsValid() && Writer->IsPassed() ? TEXT("true") : TEXT("false"));
            FPlatformMisc::RequestExit(false);
            return false;
        }
        return true;
    }
    if (!Writer.IsValid())
    {
        return false;
    }
    const double Dt = FMath::Min(static_cast<double>(DeltaSeconds), AwMaxFrameSeconds);
    TotalSeconds += Dt;
    if (StepIndex >= Steps.Num())
    {
        FinishScenario();
        return true;
    }
    if (TotalSeconds > AwScenarioBudgetSeconds)
    {
        Writer->AddStep({TEXT("scenario_budget"), false, FString::Printf(TEXT("the scenario used its whole %.0f s budget at step '%s'"), AwScenarioBudgetSeconds, *Steps[StepIndex].Name), FString()});
        FinishScenario();
        return true;
    }
    const PWAcceptance::FStepDef& Def = Steps[StepIndex];
    Runtime.Dt = Dt;
    Runtime.Elapsed += Dt;
    PWAcceptance::FStepOutcome Outcome = Def.Run(*this, Runtime);
    if (Outcome.State == PWAcceptance::EStepState::Running && Runtime.Elapsed > Def.TimeoutSeconds)
    {
        Outcome = PWAcceptance::FStepOutcome::Fail(FString::Printf(TEXT("timed out after %.0f s. %s"), Def.TimeoutSeconds, *Runtime.Note));
    }
    if (Outcome.State == PWAcceptance::EStepState::Running)
    {
        return true;
    }
    const bool bOk = Outcome.State == PWAcceptance::EStepState::Passed;
    UE_LOG(LogPWAcceptance, Display, TEXT("Acceptance step %d/%d '%s': %s - %s"), StepIndex + 1, Steps.Num(), *Def.Name, bOk ? TEXT("ok") : TEXT("FAILED"), *Outcome.Detail);
    if (!Writer->AddStep({Def.Name, bOk, Outcome.Detail, Outcome.Screenshot}))
    {
        UE_LOG(LogPWAcceptance, Error, TEXT("Acceptance: the result file %s could not be written."), *Writer->GetPath());
    }
    Runtime = PWAcceptance::FStepRuntime();
    ++StepIndex;
    if (!bOk)
    {
        FinishScenario(); // a failed step ends the script; the result says so
    }
    return true;
}

void UPWAcceptanceSubsystem::FinishScenario()
{
    if (Writer.IsValid() && !Writer->IsCompleted())
    {
        Writer->Complete();
    }
    StepIndex = Steps.Num();
    ExitGraceFrames = AwExitGraceFrames;
}
