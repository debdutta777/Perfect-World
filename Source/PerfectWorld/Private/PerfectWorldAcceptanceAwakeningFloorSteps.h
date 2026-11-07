#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldAwakeningFloor.h"

// Step builders of the R9-06 awakening_floor scenario (PerfectWorldAcceptanceAwakeningFloor.cpp). File-local names are prefixed AwFl.

namespace PWAcceptance::AwakeningFloor
{
/** What the scenario shares between its steps. */
struct FAwFlRun
{
    /** The fake installed by the scenario when the integrator has installed no real bridge; null when a real one is in use. */
    TSharedPtr<PWAwakening::FAwScriptedBridge> Scripted;
    /** A real bridge cannot replay a completed session, so the second (withdrawal) pass is skipped with a note. */
    bool bSkipSecondPass = false;
};

APWAwakeningFloorActor* AwFlFindFloor(UPWAcceptanceSubsystem& Sub);

/** The floor is in the world (hook 1) and a bridge is installed (the scripted fake when none was). */
FStepDef AwFlBuiltStep(const TSharedRef<FAwFlRun>& Run);
/** The scenario's one declared teleport: the pawn stands at the forecourt terminal. */
FStepDef AwFlStartStep();
FStepDef AwFlCameraStep(const FString& Name, bool bThird);
/** Interacts with the terminal; passes when the pawn stands in the room, the transfer is over and the bridge saw ExpectedEnters entries. */
FStepDef AwFlEnterStep(const FString& Name, const TSharedRef<FAwFlRun>& Run, int32 ExpectedEnters);
FStepDef AwFlWalkToStationStep(const FString& Name, int32 Station);
/** Interacts with pad PadIndex of Station; passes when the bridge saw it, the phase is Complete, the moving parts are at their poses and, for a withdrawal, the pawn is in the safe area. */
FStepDef AwFlChoiceStep(const FString& Name, const TSharedRef<FAwFlRun>& Run, int32 Station, int32 PadIndex, bool bExpectSafeArea);
FStepDef AwFlSessionCompleteStep(const FString& Name, const TSharedRef<FAwFlRun>& Run);
FStepDef AwFlWalkToReturnStep(const FString& Name);
FStepDef AwFlLeaveStep(const FString& Name, const TSharedRef<FAwFlRun>& Run, int32 ExpectedLeaves);
/** A fresh scripted session for the second pass (or, with a real bridge, marks the second pass skipped). */
FStepDef AwFlResetStep(const FString& Name, const TSharedRef<FAwFlRun>& Run);
FStepDef AwFlSummaryStep(const FString& Name, const TSharedRef<FAwFlRun>& Run);
/** Wraps a step so it passes at once with a note when the second pass is skipped. */
FStepDef AwFlSecondPass(FStepDef Step, const TSharedRef<FAwFlRun>& Run);
} // namespace PWAcceptance::AwakeningFloor
