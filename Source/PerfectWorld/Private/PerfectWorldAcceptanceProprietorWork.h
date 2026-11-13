#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"
#include "NpcBehaviour.h"

PWAcceptance::FStepDef R55SafeTalkStep(const TCHAR* Name, FName Person);
PWAcceptance::FStepDef R55LeaveResumeStep(const TCHAR* Name, FName Person);
PWAcceptance::FStepDef R55PilotTimeStep(const TCHAR* Name, int32 Hour, int32 Minute);
PWAcceptance::FStepDef R55DecisionStep(const TCHAR* Name, FName Person, PerfectWorld::NpcDecisionReason Reason);
PWAcceptance::FStepDef R55ObstructionStep(const TCHAR* Name, FName Person);

namespace PWAcceptance
{
/** Independent physical visit with one declared start outside Mahua's shop. */
void AppendMahuaWorkdaySteps(TArray<FStepDef>& Out);
}
