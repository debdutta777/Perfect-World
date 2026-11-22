#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class SWidget;

namespace PWTitleR5
{
struct FSlotSummary;

/**
 * The text of one save's summary: name, age, in-game date and time, place, the file's real timestamp with its slot, and any
 * note that must be read (backup recovery, unfinished creation, why it cannot be read). Not interactive; the caller puts it
 * in a button or beside one. Every line wraps. bCompact joins lines in pairs for short windows (the full form is used
 * otherwise); both forms hold the same information.
 */
TSharedRef<SWidget> MakeSummaryBody(const FSlotSummary& Summary, const TAttribute<bool>& bCompact);
} // namespace PWTitleR5
