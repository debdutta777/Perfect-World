#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldSchoolCampus.h"
#include "PerfectWorldSchoolGate.h"

// Step builders of the R7-03 school scenarios (PerfectWorldAcceptanceSchoolR7.cpp).

namespace PWAcceptance::SchoolR7
{
constexpr double ScAFloor = PWSchool::ScGroundZ;

bool ScAView(UPWAcceptanceSubsystem& Sub, PerfectWorld::SchoolArrivalView& OutView, FString& OutWhy);
FStepDef ScACamera(const FString& Name, bool bThird);
/** Walks a list of points; FaceAfter (optional) is looked at on arrival. */
FStepDef ScAPointsWalk(const FString& Name, const TArray<FVector>& Points, double Timeout, const FVector* FaceAfter = nullptr);
FStepDef ScAReadBoardStep();
FStepDef ScABaselineStep(const FString& Name, const TSharedRef<int32>& ExpectedCalls);
FStepDef ScASignInStep(const FString& Name, const TSharedRef<int32>& ExpectedCalls, const FString& WantCode, TFunction<bool(UPWAcceptanceSubsystem&)> SkipIf);
FStepDef ScAClockStep(const FString& Name, int32 Minute, int32 MaxMinute);
FStepDef ScARecordStep(const FString& Name, PerfectWorld::AttendanceOutcome Want, const TCHAR* Phrase);
/** world_ready, enter_world, campus check, home door, the plan's route to the gate. */
void ScAWalkToGate(TArray<FStepDef>& Out);
PWSchool::FScPoints ScAPoints();
} // namespace PWAcceptance::SchoolR7
