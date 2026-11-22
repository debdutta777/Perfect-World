#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
// P39: live Phase 3 scenarios. Continue requires the driver's two fresh-process phases.
void P39AppendContinueSteps(TArray<FStepDef>& Out);
void P39AppendWithdrawalSteps(TArray<FStepDef>& Out);
void P39AppendOrdinarySteps(TArray<FStepDef>& Out);
void P39AppendInterruptedSteps(TArray<FStepDef>& Out);
} // namespace PWAcceptance
