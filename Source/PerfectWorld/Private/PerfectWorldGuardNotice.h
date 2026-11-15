#pragma once

#include "CoreMinimal.h"

/** P313: every presentation of a paused-session guard names its reachable exit. */
inline FText P313GuardNotice(const FString& Code)
{
    if (Code == TEXT("school.lesson_open"))
        return NSLOCTEXT("PerfectWorldGuardNotice", "Lesson", "Return to the classroom seat to resume your open lesson, then finish it or choose Leave early.");
    if (Code == TEXT("school.paper_open"))
        return NSLOCTEXT("PerfectWorldGuardNotice", "Paper", "Return to the study desk to resume your open paper, then Hand in or Cancel.");
    if (Code == TEXT("tutorial.active"))
        return NSLOCTEXT("PerfectWorldGuardNotice", "Tutorial", "Return to the Awakening floor through the school forecourt terminal to resume your supervised session. Complete it or choose Withdraw to the safe area.");
    return FText::GetEmpty();
}
