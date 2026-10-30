#pragma once

#include "CoreMinimal.h"

/** Unreal text adapters for the portable core's English creation presentation data. */
namespace PWUI
{
struct FTraitText
{
    /** What the trait is, in a sentence. */
    FText Short;
    /** The starting effect at rank 1, 2, 3 and 4. */
    FText Effect;
};

/** Text for a trait ID; empty text when the ID is not in the table. */
const FTraitText& GetTraitText(FName TraitId);

/** The authored meaning of one rank; empty for an unknown trait or rank. */
FText GetTraitRankMeaning(FName TraitId, int32 Rank);

/** Core-authored effect lines for one rank, formatted for display and joined by newlines. */
FText FormatTraitRankEffects(FName TraitId, int32 Rank);

/** A readable heading for a core group ID, falling back to the ID itself. */
FText GetGroupTitle(FName GroupId);
} // namespace PWUI
