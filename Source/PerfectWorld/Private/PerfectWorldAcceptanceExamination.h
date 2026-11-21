#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/** P406: real walked Awakening, contact-bound ordinary days, automatic 06:00 decision, one Decline. */
void P406AppendExaminationSteps(TArray<FStepDef>& Out);
/** Same real preparation, then Sit/OpenRoute/Choose/Withdraw. Missing runners FAIL with the real refusal. */
void P406AppendExaminationWithdrawSteps(TArray<FStepDef>& Out);
}
