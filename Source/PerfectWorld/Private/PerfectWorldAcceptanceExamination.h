#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/** P412: shared real awakening/countdown route to the first examination morning. */
void P412AppendExaminationMorningSteps(TArray<FStepDef>& Out);
/** P412: two fresh lives, declared clock-arrival seam, then real late Sit/floor and missed-window presentation. */
void P412AppendExaminationLateSteps(TArray<FStepDef>& Out);
/** P412: live Defer/year advance/Sit/save, then a separate process's title Continue and actual birth binding. */
void P412AppendExaminationRetakeSteps(TArray<FStepDef>& Out);
/** P406: real walked Awakening, contact-bound ordinary days, automatic 06:00 decision, one Decline. */
void P406AppendExaminationSteps(TArray<FStepDef>& Out);
/** Same real preparation, then Sit/OpenRoute/Choose/Withdraw. Missing runners FAIL with the real refusal. */
void P406AppendExaminationWithdrawSteps(TArray<FStepDef>& Out);
/** P407U: real result, mentor booking, paid campus session and retained attendance. */
void P407UAppendExaminationFollowupSteps(TArray<FStepDef>& Out);
}
