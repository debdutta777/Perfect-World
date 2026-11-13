#pragma once

#include "CoreMinimal.h"
#include "CreationPresentation.h"

/** Shared formatting for subsystem views and legacy review text. No rules live here. */
namespace PWCreationPresentation
{
FText EffectText(const PerfectWorld::EffectLine& Line);
TArray<FText> RankEffectTexts(const PerfectWorld::TraitRankEffect& Rank);
FText HouseholdEverydayText(FName HouseholdId);
}
