#include "UI/SPWClassLesson.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"

// The shared lesson shell, part 3: the commands (open, close, choose, leave early) and the action registry. Every choose and close carries
// the session id the screen last displayed (DisplayedSession), so the core refuses a stale screen.

namespace
{
PerfectWorld::SchoolSessionResult ClCmdRefusal(const char* Code)
{
	PerfectWorld::SchoolSessionResult Result;
	Result.Issue.Code = Code;
	return Result;
}
} // namespace

// ---- Commands ---------------------------------------------------------------------------------------------------------------

PerfectWorld::SchoolSessionResult SPWClassLesson::ExecuteOpen()
{
	return ExecuteOpen(Mode);
}

PerfectWorld::SchoolSessionResult SPWClassLesson::ExecuteOpen(PerfectWorld::SchoolLessonMode InMode)
{
	PerfectWorld::SchoolSessionResult Result;
	if (!Bridge.IsValid())
	{
		Result = ClCmdRefusal("school.session.missing_catalogue");
	}
	else if (IsWrongScreen())
	{
		// A screen never drives the wrong kind of lesson.
		Result = ClCmdRefusal("school.session.bad_binding");
	}
	else
	{
		Result = Bridge->OpenLesson(InMode, CurrentGoal());
	}
	LastResult = Result;
	bLeaveArmed = false;
	if (Result.Ok())
	{
		LastIssueCode.Reset();
		const TCHAR* Key = InMode == PerfectWorld::SchoolLessonMode::Foreground ? TEXT("phase2.a2.common.status.opened_foreground")
			: InMode == PerfectWorld::SchoolLessonMode::Routine ? TEXT("phase2.a2.common.status.opened_routine")
			: TEXT("phase2.a2.common.status.opened_attend");
		ShowBanner(PWClassLesson::Text(Key), false);
	}
	else
	{
		LastIssueCode = PWClassLesson::ToFString(Result.Issue.Code);
		ShowBanner(PWClassLesson::IssueText(Result.Issue), true);
	}
	RefreshModel();
	return Result;
}

int32 SPWClassLesson::LeaveMinutes() const
{
	const int32 Spent = Frame.bExposureActive ? FMath::Max(0, Frame.UnitsTotal - Frame.UnitsLeft) : 0;
	return FMath::Clamp(Spent * PerfectWorld::ModelUnitMinutes, 0, FMath::Max(0, Frame.PeriodMinutes - 1));
}

bool SPWClassLesson::CanLeaveEarly() const
{
	return Bridge.IsValid() && Frame.bHasRun && !Frame.bOtherLesson && !bLessonClosed && !IsWrongScreen() &&
		!(Frame.RunMode == PerfectWorld::SchoolLessonMode::Foreground && Frame.bReadyToClose);
}

PerfectWorld::SchoolSessionResult SPWClassLesson::ExecuteClose(bool bCancel)
{
	const TSharedRef<SWidget> KeepAlive = AsShared();
	PerfectWorld::SchoolSessionResult Result;
	bLeaveArmed = false;
	if (!Bridge.IsValid() || !Frame.bHasRun || Frame.bOtherLesson || bLessonClosed)
	{
		Result = ClCmdRefusal("school.session.stale_session");
		LastResult = Result;
		LastIssueCode = PWClassLesson::ToFString(Result.Issue.Code);
		ShowBanner(PWClassLesson::IssueText(Result.Issue), true);
		return Result;
	}
	const PerfectWorld::SchoolLessonOutcome Snapshot = Frame.Outcome;
	const PerfectWorld::SchoolLessonMode SnapshotMode = Frame.RunMode;
	const int32 Elapsed = bCancel ? LeaveMinutes() : 0;
	Result = Bridge->CloseLesson(DisplayedSession, bCancel, Elapsed);
	LastResult = Result;
	if (!Result.Ok())
	{
		LastIssueCode = PWClassLesson::ToFString(Result.Issue.Code);
		ShowBanner(PWClassLesson::IssueText(Result.Issue), true);
		RefreshModel();
		return Result;
	}
	LastIssueCode.Reset();
	bLessonClosed = true;
	bClosedByLeaving = bCancel;
	ClosedMode = SnapshotMode;
	ClosedOutcome = bCancel ? PerfectWorld::SchoolLessonOutcome() : Snapshot;
	ClosedMinutes = 0;
	ClosedMilliXp = 0;
	if (bCancel)
	{
		ClosedMinutes = Elapsed;
	}
	else
	{
		for (const PerfectWorld::DayLoopEvent& Event : Result.Events)
		{
			if (Event.Kind == PerfectWorld::DayLoopEventKind::Completed)
			{
				ClosedMinutes += static_cast<int32>(Event.Minutes);
				for (const PerfectWorld::SkillCredit& Credit : Event.Credited)
				{
					ClosedMilliXp += Credit.MilliXp;
				}
			}
		}
	}
	ShowBanner(PWClassLesson::Text(bCancel ? TEXT("phase2.a2.common.status.cancelled") : TEXT("phase2.a2.common.status.closed")), false);
	RefreshModel();
	OnLessonClosed.ExecuteIfBound(bCancel);
	return Result;
}

bool SPWClassLesson::PressLeaveEarly()
{
	if (!CanLeaveEarly())
	{
		return false;
	}
	if (!bLeaveArmed)
	{
		bLeaveArmed = true;
		ShowBanner(PWClassLesson::Text(TEXT("phase2.a2.common.cancel_note")), false);
		return false;
	}
	return ExecuteClose(true).Ok();
}

PerfectWorld::SchoolLessonQuote SPWClassLesson::Quote(const PerfectWorld::SchoolLessonAction& Action) const
{
	if (!Bridge.IsValid())
	{
		PerfectWorld::SchoolLessonQuote Missing;
		Missing.Issue.Code = "school.session.missing_catalogue";
		return Missing;
	}
	return Bridge->QuoteAction(DisplayedSession, Action);
}

PerfectWorld::SchoolSessionResult SPWClassLesson::ChooseAction(const PerfectWorld::SchoolLessonAction& Action, const FText& SuccessLine)
{
	PerfectWorld::SchoolSessionResult Result;
	bLeaveArmed = false;
	if (!Bridge.IsValid())
	{
		Result = ClCmdRefusal("school.session.missing_catalogue");
	}
	else
	{
		// The session id is the one this screen displays: a screen the world has moved on from is refused by the core.
		Result = Bridge->ChooseAction(DisplayedSession, Action);
	}
	LastResult = Result;
	if (!Result.Ok())
	{
		LastIssueCode = PWClassLesson::ToFString(Result.Issue.Code);
		ShowBanner(PWClassLesson::IssueText(Result.Issue), true);
	}
	else
	{
		LastIssueCode.Reset();
		if (Result.ReadyToClose)
		{
			ShowBanner(PWClassLesson::Text(TEXT("phase2.a3.common.exercise_done")), false);
		}
		else if (Result.ChargedUnits > 0 && !SuccessLine.IsEmpty())
		{
			ShowBanner(PWClassLesson::Text2(TEXT("phase2.a2.common.status.done"), SuccessLine, PWClassLesson::Number(Result.ChargedUnits)), false);
		}
		else
		{
			ShowBanner(SuccessLine, false);
		}
	}
	RefreshModel();
	return Result;
}

// ---- Actions on screen ----------------------------------------------------------------------------------------------------

TArray<FString> SPWClassLesson::GetActionKeys() const
{
	TArray<FString> Keys;
	for (const FActionSlot& Slot : Actions)
	{
		Keys.Add(Slot.Key);
	}
	return Keys;
}

bool SPWClassLesson::HasAction(const FString& Key) const
{
	return Actions.ContainsByPredicate([&Key](const FActionSlot& Slot) { return Slot.Key == Key; });
}

bool SPWClassLesson::IsActionEnabled(const FString& Key) const
{
	const FActionSlot* Found = Actions.FindByPredicate([&Key](const FActionSlot& Slot) { return Slot.Key == Key; });
	return Found != nullptr && Found->bEnabled;
}

bool SPWClassLesson::PressAction(const FString& Key)
{
	const FActionSlot* Found = Actions.FindByPredicate([&Key](const FActionSlot& Slot) { return Slot.Key == Key; });
	if (Found == nullptr || !Found->bEnabled || !Found->Handler)
	{
		return false;
	}
	// The handler rebuilds the rows, which clears Actions: run a copy. Pressing anything else disarms Leave early.
	const TFunction<void()> Handler = Found->Handler;
	bLeaveArmed = false;
	Handler();
	return true;
}
