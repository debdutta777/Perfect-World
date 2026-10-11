#pragma once

#include "CoreMinimal.h"

/**
 * Player-facing explanations for the twenty background traits, written from the
 * trait catalogue in Docs/CHARACTER_CREATION.md. The portable core owns each
 * trait's stable ID, display name, group and cost; it carries no description,
 * so this UI-side table holds the plain-language text (string tables come
 * later). Every number here is copied from that catalogue, never recomputed.
 */
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

/** A readable heading for a core group ID, falling back to the ID itself. */
FText GetGroupTitle(FName GroupId);
} // namespace PWUI
