#include "UI/SPWClassLesson.h"

// The shared lesson words, the interim core bridge and the frame. The English table is in SPWClassLessonTable.cpp (generated from
// Data/Text/en/phase2_a2.json and phase2_a3.json; a test keeps the two in step). Nothing here decides anything.

namespace
{
const TMap<FString, FString>& ClWTable()
{
	static const TMap<FString, FString> Table = []()
	{
		TMap<FString, FString> Result;
		for (const PWClassLesson::FTextEntry& Entry : PWClassLesson::TextEntries())
		{
			Result.Add(FString(Entry.Key), FString(Entry.Value));
		}
		return Result;
	}();
	return Table;
}

bool ClWIsUnitsShort(const std::string& Code)
{
	return Code == "structure.units_short" || Code == "model.units_short";
}
} // namespace

namespace PWClassLesson
{
bool FindText(const FString& Key, FText& OutText)
{
	if (const FString* Found = ClWTable().Find(Key))
	{
		OutText = FText::FromString(*Found);
		return true;
	}
	return false;
}

FText Text(const FString& Key)
{
	FText Found;
	if (FindText(Key, Found))
	{
		return Found;
	}
	return FText::FromString(TEXT("Text unavailable."));
}

FText Text(const TCHAR* Key) { return Text(FString(Key)); }
FText Text1(const TCHAR* Key, const FText& A) { return FText::Format(Text(Key), A); }
FText Text2(const TCHAR* Key, const FText& A, const FText& B) { return FText::Format(Text(Key), A, B); }
FText Text3(const TCHAR* Key, const FText& A, const FText& B, const FText& C) { return FText::Format(Text(Key), A, B, C); }
FText Number(int64 Value) { return FText::AsNumber(Value); }
FString ToFString(const std::string& Value) { return FString(UTF8_TO_TCHAR(Value.c_str())); }
std::string ToUtf8(const FString& Value) { return std::string(TCHAR_TO_UTF8(*Value)); }
FText FromUtf8(const std::string& Value) { return FText::FromString(ToFString(Value)); }

FText IssueText(const std::string& Code)
{
	if (Code.empty())
	{
		return Text(TEXT("school.session.ok"));
	}
	const FString Key = ToFString(Code);
	FText Found;
	if (FindText(Key, Found))
	{
		return Found;
	}
	if (FindText(FString(TEXT("phase2.a2.issue.")) + Key, Found))
	{
		return Found;
	}
	return Text(TEXT("phase2.a2.issue.unknown"));
}

FText IssueText(const PerfectWorld::SchoolIssue& Issue) { return IssueText(Issue.Code); }

FText ClockText(int32 MinuteOfDay)
{
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), MinuteOfDay / 60, MinuteOfDay % 60));
}

FText CostText(int32 Units)
{
	if (Units <= 0)
	{
		return Text(TEXT("phase2.a3.common.cost.free"));
	}
	if (Units == 1)
	{
		return Text(TEXT("phase2.a3.common.cost.one"));
	}
	return Text1(TEXT("phase2.a3.common.cost.many"), Number(Units));
}

FText PriceText(const PerfectWorld::SchoolLessonQuote& Quote, int32 UnitsLeft)
{
	if (Quote.Issue.Ok())
	{
		return CostText(Quote.Units);
	}
	if (ClWIsUnitsShort(Quote.Issue.Code))
	{
		return Text2(TEXT("phase2.a3.common.cost.short"), Number(Quote.Units), Number(UnitsLeft));
	}
	return FText::GetEmpty();
}

FText UnitsText(int32 Left, int32 Total)
{
	return Text2(TEXT("phase2.a3.common.units"), Number(Left), Number(Total));
}

FText ModeText(PerfectWorld::SchoolLessonMode Mode)
{
	switch (Mode)
	{
	case PerfectWorld::SchoolLessonMode::Foreground: return Text(TEXT("phase2.a3.common.mode.foreground"));
	case PerfectWorld::SchoolLessonMode::Routine: return Text(TEXT("phase2.a3.common.mode.routine"));
	case PerfectWorld::SchoolLessonMode::Attend: break;
	}
	return Text(TEXT("phase2.a3.common.mode.attend"));
}

FText ModeHint(PerfectWorld::SchoolLessonMode Mode)
{
	switch (Mode)
	{
	case PerfectWorld::SchoolLessonMode::Foreground: return Text(TEXT("phase2.a3.common.mode.foreground.hint"));
	case PerfectWorld::SchoolLessonMode::Routine: return Text(TEXT("phase2.a3.common.mode.routine.hint"));
	case PerfectWorld::SchoolLessonMode::Attend: break;
	}
	return Text(TEXT("phase2.a3.common.mode.attend.hint"));
}

FText GradeText(PerfectWorld::StructureGrade Grade)
{
	switch (Grade)
	{
	case PerfectWorld::StructureGrade::Sound: return Text(TEXT("phase2.a2.common.grade.sound"));
	case PerfectWorld::StructureGrade::Partial: return Text(TEXT("phase2.a2.common.grade.partial"));
	case PerfectWorld::StructureGrade::Unsound: break;
	}
	return Text(TEXT("phase2.a2.common.grade.unsound"));
}

TArray<FString> KnownTextKeys()
{
	TArray<FString> Keys;
	ClWTable().GetKeys(Keys);
	return Keys;
}

// ---- The interim bridge --------------------------------------------------------------------------------------------------

FCoreBridge::FCoreBridge(PerfectWorld::SchoolSessionsState& InSessions, PerfectWorld::DayLoopState& InDay,
	const PerfectWorld::SchoolSessionCatalogues& InCatalogues, PerfectWorld::SchoolLessonBinding InBinding, int32 InSkillMilli,
	uint64 InWorldSeed, std::vector<PerfectWorld::ReceiptCandidate> InKnownAlternatives)
	: Sessions(InSessions)
	, Day(InDay)
	, Catalogues(InCatalogues)
	, Binding(MoveTemp(InBinding))
	, SkillMilli(InSkillMilli)
	, WorldSeed(InWorldSeed)
	, KnownAlternatives(MoveTemp(InKnownAlternatives))
{
}

PerfectWorld::SchoolPeriodView FCoreBridge::QueryPeriod() const
{
	return PerfectWorld::QuerySchoolPeriod(Sessions, Day, Binding, Catalogues);
}

PerfectWorld::SchoolLessonView FCoreBridge::QueryLesson() const
{
	return PerfectWorld::QuerySchoolLesson(Sessions);
}

PerfectWorld::SchoolSessionResult FCoreBridge::OpenLesson(PerfectWorld::SchoolLessonMode Mode, const PerfectWorld::SchoolLessonGoal& Goal)
{
	return PerfectWorld::OpenSchoolLesson(Sessions, Day, Catalogues, Binding, Mode, Goal, SkillMilli, WorldSeed, KnownAlternatives);
}

PerfectWorld::SchoolLessonQuote FCoreBridge::QuoteAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) const
{
	return PerfectWorld::QuoteSchoolLessonAction(Sessions, Catalogues, Session, Action);
}

PerfectWorld::SchoolSessionResult FCoreBridge::ChooseAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action)
{
	return PerfectWorld::ChooseSchoolLesson(Sessions, Catalogues, Session, Action);
}

PerfectWorld::SchoolSessionResult FCoreBridge::CloseLesson(uint64 Session, bool bCancel, int32 ElapsedMinutes)
{
	return PerfectWorld::CloseSchoolLesson(Sessions, Day, Catalogues, Session, bCancel, ElapsedMinutes);
}

// ---- The frame -------------------------------------------------------------------------------------------------------------

FFrame BuildFrame(const IBridge& Bridge)
{
	FFrame Out;
	Out.Binding = Bridge.GetBinding();
	Out.Period = Bridge.QueryPeriod();
	Out.View = Bridge.QueryLesson();
	if (Out.Period.EndMinute > Out.Period.StartMinute)
	{
		Out.PeriodMinutes = Out.Period.EndMinute - Out.Period.StartMinute;
	}
	Out.Title = Bridge.ResolveText(ToFString(Out.Binding.TitleTextKey));
	Out.Brief = Bridge.ResolveText(ToFString(Out.Binding.BriefTextKey));
	Out.Role = Bridge.ResolveText(ToFString(Out.Binding.RoleTextKey));
	if (Out.View.Run.has_value())
	{
		const PerfectWorld::SchoolLessonRun& Run = *Out.View.Run;
		Out.bHasRun = true;
		Out.bOtherLesson = !(Run.Binding == Out.Binding);
		Out.bReadyToClose = Run.ReadyToClose;
		Out.Session = Run.Session;
		Out.RunMode = Run.Mode;
		Out.Outcome = Run.Outcome;
	}
	return Out;
}
} // namespace PWClassLesson
