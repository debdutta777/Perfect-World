#include "UI/SPWClassModel.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// Order A2. The shared lesson chrome (SPWClassLessonBase), its words and its interim core bridge, then the model-a-system screen.
// Words: the English table at the end of this file mirrors Data/Text/en/phase2_a2.json key for key (a test keeps them in step).


namespace
{
struct FA2TextEntry
{
	const TCHAR* Key;
	const TCHAR* Value;
};

extern const FA2TextEntry GA2Entries[];
extern const int32 GA2EntryCount;

const TMap<FString, FString>& A2Table()
{
	static const TMap<FString, FString> Table = []()
	{
		TMap<FString, FString> Result;
		for (int32 Index = 0; Index < GA2EntryCount; ++Index)
		{
			Result.Add(FString(GA2Entries[Index].Key), FString(GA2Entries[Index].Value));
		}
		return Result;
	}();
	return Table;
}

FString A2Str(std::string_view Value)
{
	return FString(UTF8_TO_TCHAR(std::string(Value).c_str()));
}

std::string A2Utf8(const FString& Value)
{
	return std::string(TCHAR_TO_UTF8(*Value));
}

/** "physics" -> "Physics", "science_lab" -> "Science lab". */
FString A2Title(std::string_view Token)
{
	FString Result = A2Str(Token).Replace(TEXT("_"), TEXT(" "));
	if (Result.Len() > 0)
	{
		Result[0] = FChar::ToUpper(Result[0]);
	}
	return Result;
}

FString A2FamilyName(const PerfectWorld::SchoolSessionCatalogues& Catalogues, const PerfectWorld::SchoolLessonBinding& Binding)
{
	if (Binding.Structure == PerfectWorld::SchoolStructure::Model && Catalogues.Structures.Model != nullptr)
	{
		if (const PerfectWorld::ModelFamilyDef* Family = Catalogues.Structures.Model->FindFamily(Binding.FamilyId))
		{
			return A2Str(Family->Name);
		}
	}
	if (Binding.Structure == PerfectWorld::SchoolStructure::Explain && Catalogues.Structures.Explain != nullptr)
	{
		if (const auto* Family = Catalogues.Structures.Explain->FindFamily(Binding.FamilyId))
		{
			return A2Str(Family->Name);
		}
	}
	return A2Title(Binding.Subject);
}

FText A2ResolveOr(const std::string& Key, const FText& Fallback)
{
	FText Found;
	return PWClassLesson::FindText(A2Str(Key), Found) ? Found : Fallback;
}

FText A2GradeText(PerfectWorld::StructureGrade Grade)
{
	switch (Grade)
	{
	case PerfectWorld::StructureGrade::Sound: return PWClassLesson::Text(TEXT("phase2.a2.common.grade.sound"));
	case PerfectWorld::StructureGrade::Partial: return PWClassLesson::Text(TEXT("phase2.a2.common.grade.partial"));
	default: return PWClassLesson::Text(TEXT("phase2.a2.common.grade.unsound"));
	}
}

FLinearColor A2GradeColor(PerfectWorld::StructureGrade Grade)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	switch (Grade)
	{
	case PerfectWorld::StructureGrade::Sound: return Style.Good;
	case PerfectWorld::StructureGrade::Partial: return Style.Warn;
	default: return Style.Danger;
	}
}

PerfectWorld::StructureGrade A2ModelGrade(PerfectWorld::ModelGrade Grade)
{
	switch (Grade)
	{
	case PerfectWorld::ModelGrade::Sound: return PerfectWorld::StructureGrade::Sound;
	case PerfectWorld::ModelGrade::Partial: return PerfectWorld::StructureGrade::Partial;
	default: return PerfectWorld::StructureGrade::Unsound;
	}
}
} // namespace

// ---- Words -----------------------------------------------------------------------------------------------------------------

namespace PWClassLesson
{
bool FindText(const FString& Key, FText& OutText)
{
	if (const FString* Found = A2Table().Find(Key))
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
FText FromUtf8(const std::string& Value) { return FText::FromString(A2Str(Value)); }

FText IssueText(const std::string& Code)
{
	if (Code.empty())
	{
		return Text(TEXT("school.session.ok"));
	}
	const FString Key = A2Str(Code);
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

FText QuoteText(const PerfectWorld::SchoolLessonQuote& Quote)
{
	if (!Quote.Issue.Ok())
	{
		return Text1(TEXT("phase2.a2.common.quote.unavailable"), IssueText(Quote.Issue.Code));
	}
	if (Quote.Units == 0)
	{
		return Text1(TEXT("phase2.a2.common.quote.free"), Number(Quote.UnitsAfter));
	}
	return Text2(TEXT("phase2.a2.common.quote.cost"), Number(Quote.Units), Number(Quote.UnitsAfter));
}

TArray<FString> KnownTextKeys()
{
	TArray<FString> Keys;
	A2Table().GetKeys(Keys);
	return Keys;
}

// ---- The interim bridge -------------------------------------------------------------------------------------------------------

FCoreBridge::FCoreBridge(PerfectWorld::SchoolSessionsState& InSessions, PerfectWorld::DayLoopState& InDay, const PerfectWorld::SchoolSessionCatalogues& InCatalogues,
	PerfectWorld::SchoolLessonBinding InBinding, int32 InSkillMilli, uint64 InWorldSeed, std::vector<PerfectWorld::ReceiptCandidate> InKnownAlternatives)
	: Sessions(InSessions)
	, Day(InDay)
	, Catalogues(InCatalogues)
	, Binding(MoveTemp(InBinding))
	, SkillMilli(InSkillMilli)
	, WorldSeed(InWorldSeed)
	, KnownAlternatives(MoveTemp(InKnownAlternatives))
{
}

PerfectWorld::SchoolPeriodView FCoreBridge::QueryPeriod() const { return PerfectWorld::QuerySchoolPeriod(Sessions, Day, Binding, Catalogues); }
PerfectWorld::SchoolLessonView FCoreBridge::QueryLesson() const { return PerfectWorld::QuerySchoolLesson(Sessions); }

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

// ---- Shared builders ---------------------------------------------------------------------------------------------------------

TSharedRef<SWidget> MakeNote(const FText& NoteText, const TCHAR* Tag)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	return SNew(SBorder)
		.Tag(FName(Tag))
		.BorderImage(&Style.BrushInset)
		.Padding(FMargin(14.0f, 10.0f))
		[
			PWUI::MakeText(NoteText, 16, Style.TextSecondary, false, true)
		];
}

TSharedRef<SWidget> MakeHeading(const FText& HeadingText)
{
	return PWUI::MakeText(HeadingText, PWUI::ETextRole::SectionHeading, PWUI::FStyle::Get().Accent, true);
}

TSharedRef<SWidget> MakeUnitsPips(int32 Left, int32 Total)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const TSharedRef<SHorizontalBox> Box = SNew(SHorizontalBox);
	const int32 Count = FMath::Clamp(Total, 0, 16);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Box->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SBox).WidthOverride(18.0f).HeightOverride(8.0f)
			[
				SNew(SBorder).BorderImage(Index < Left ? &Style.BrushPipOn : &Style.BrushPipOff)
			]
		];
	}
	return Box;
}

TSharedRef<SWidget> MakeOutcomeBlock(const FOutcomeFacts& Facts, const TCHAR* Tag)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[PWUI::MakeText(A2GradeText(Facts.Grade), 22, A2GradeColor(Facts.Grade), true, true)];
	for (const FText& Line : Facts.Detail)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[PWUI::MakeText(Line, 17, Style.TextPrimary, false, true)];
	}
	if (Facts.Diagnostics.IsEmpty())
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(Text(TEXT("phase2.a2.common.outcome.no_diagnostics")), 17, Style.TextSecondary, false, true)
		];
	}
	for (const FText& Line : Facts.Diagnostics)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[PWUI::MakeText(Line, 17, Style.Warn, false, true)];
	}
	if (Facts.bForeground)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(Text1(TEXT("phase2.a2.common.units_spent"), Number(Facts.UnitsSpent)), 16, Style.TextSecondary, false, true)
		];
	}
	Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
	[
		PWUI::MakeText(Text1(TEXT("phase2.a2.common.outcome.learning_permille"), Number(Facts.LearningPermille)), 16, Style.TextSecondary, false, true)
	];
	if (Facts.bBecameSecure)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(Text(TEXT("phase2.a2.common.outcome.became_secure")), 17, Style.Good, true, true)
		];
	}
	return SNew(SBorder)
		.Tag(FName(Tag))
		.BorderImage(&Style.BrushInset)
		.Padding(FMargin(14.0f, 10.0f))
		[
			Box
		];
}
} // namespace PWClassLesson

// ---- The shared lesson chrome ----------------------------------------------------------------------------------------------

void SPWClassLessonBase::InitLesson(const FLessonInit& Init)
{
	Bridge = Init.Bridge;
	Space = Init.Space;
	ReturnFocus = Init.ReturnFocus;
	OnClosed = Init.OnClosed;
	bTwoPane = PWClassLesson::TwoPane(Space.Get());
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const auto IsCompact = [this]() { return PWClassLesson::Compact(Space.Get()); };

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride_Lambda([this]() { return FOptionalSize(PWClassLesson::PanelWidth(Space.Get())); })
		.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(PWClassLesson::PanelMaxHeight(Space.Get())); })
		[
			SNew(SBorder)
			.Tag(FName(PWClassLesson::PanelTag))
			.BorderImage(&Style.BrushPanel)
			.Padding(0.0f)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SPWCornerBrackets)
				]
				+ SOverlay::Slot()
				[
					SNew(SBox)
					.Padding_Lambda([IsCompact]() { return IsCompact() ? FMargin(16.0f, 8.0f) : FMargin(36.0f, 24.0f); })
					[
						SAssignNew(BodyHolder, SBox)
					]
				]
			]
		]
	];
	Rebuild();
}

void SPWClassLessonBase::SetBridge(TSharedPtr<PWClassLesson::IBridge> InBridge)
{
	Bridge = InBridge;
	DisplayedSession = 0;
	bShowSummary = false;
	ResetSelections();
	Rebuild();
}

void SPWClassLessonBase::RefreshFromSession()
{
	Rebuild();
}

bool SPWClassLessonBase::RefreshLayout()
{
	const bool bWant = PWClassLesson::TwoPane(Space.Get());
	if (bWant == bTwoPane)
	{
		return false;
	}
	Rebuild();
	return true;
}

void SPWClassLessonBase::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	RefreshLayout();
}

void SPWClassLessonBase::SetStatus(const FText& StatusText, const FString& IssueCode)
{
	LastStatus = StatusText;
	LastIssueCode = IssueCode;
}

void SPWClassLessonBase::RegisterFocus(const TSharedRef<SWidget>& Target)
{
	FocusTarget = Target;
}

FText SPWClassLessonBase::PeriodLine(const PerfectWorld::SchoolPeriodView& Period) const
{
	const PerfectWorld::SchoolLessonBinding& Binding = Bridge->GetBinding();
	const std::string& SubjectToken = Period.Subject.empty() ? Binding.Subject : Period.Subject;
	return PWClassLesson::Text3(TEXT("phase2.a2.common.period_header"), PWClassLesson::Number(Binding.Period + 1),
		FText::FromString(A2Title(SubjectToken)), FText::FromString(Period.Room.empty() ? FString(TEXT("-")) : A2Title(Period.Room)));
}

PerfectWorld::SchoolLessonQuote SPWClassLessonBase::Quote(const PerfectWorld::SchoolLessonAction& Action) const
{
	if (!Bridge.IsValid())
	{
		PerfectWorld::SchoolLessonQuote Missing;
		Missing.Issue.Code = "school.session.missing_catalogue";
		return Missing;
	}
	return Bridge->QuoteAction(DisplayedSession, Action);
}

PerfectWorld::SchoolSessionResult SPWClassLessonBase::RunChoose(const PerfectWorld::SchoolLessonAction& Action, const TCHAR* SuccessKey)
{
	PerfectWorld::SchoolSessionResult Result;
	bCancelArmed = false;
	if (!Bridge.IsValid())
	{
		Result.Issue.Code = "school.session.missing_catalogue";
	}
	else
	{
		Result = Bridge->ChooseAction(DisplayedSession, Action);
	}
	LastResult = Result;
	if (Result.Ok())
	{
		SetStatus(PWClassLesson::Text2(TEXT("phase2.a2.common.status.done"), PWClassLesson::Text(SuccessKey),
			PWClassLesson::Number(Result.ChargedUnits)), FString());
	}
	else
	{
		SetStatus(PWClassLesson::IssueText(Result.Issue.Code), A2Str(Result.Issue.Code));
	}
	Rebuild();
	return Result;
}

PerfectWorld::SchoolSessionResult SPWClassLessonBase::TriggerOpenLesson(PerfectWorld::SchoolLessonMode Mode)
{
	PerfectWorld::SchoolSessionResult Result;
	if (!Bridge.IsValid())
	{
		Result.Issue.Code = "school.session.missing_catalogue";
	}
	else
	{
		Result = Bridge->OpenLesson(Mode, CurrentGoal());
	}
	LastResult = Result;
	if (Result.Ok())
	{
		bShowSummary = false;
		bCancelArmed = false;
		ResetSelections();
		const TCHAR* Key = Mode == PerfectWorld::SchoolLessonMode::Foreground ? TEXT("phase2.a2.common.status.opened_foreground")
			: Mode == PerfectWorld::SchoolLessonMode::Routine ? TEXT("phase2.a2.common.status.opened_routine")
			: TEXT("phase2.a2.common.status.opened_attend");
		SetStatus(PWClassLesson::Text(Key), FString());
	}
	else
	{
		SetStatus(PWClassLesson::IssueText(Result.Issue.Code), A2Str(Result.Issue.Code));
	}
	Rebuild();
	return Result;
}

int32 SPWClassLessonBase::ElapsedSoFar(const PerfectWorld::SchoolLessonView& View) const
{
	const int32 Spent = FMath::Max(0, UnitsTotal(View) - UnitsLeft(View));
	int32 Duration = 45;
	if (Bridge.IsValid())
	{
		const PerfectWorld::SchoolPeriodView Period = Bridge->QueryPeriod();
		if (Period.EndMinute > Period.StartMinute)
		{
			Duration = Period.EndMinute - Period.StartMinute;
		}
	}
	return FMath::Clamp(Spent * PerfectWorld::ModelUnitMinutes, 0, FMath::Max(0, Duration - 1));
}

PerfectWorld::SchoolSessionResult SPWClassLessonBase::TriggerCloseLesson()
{
	PerfectWorld::SchoolSessionResult Result;
	if (!Bridge.IsValid())
	{
		Result.Issue.Code = "school.session.missing_catalogue";
		LastResult = Result;
		SetStatus(PWClassLesson::IssueText(Result.Issue.Code), A2Str(Result.Issue.Code));
		return Result;
	}
	const PerfectWorld::SchoolLessonView View = Bridge->QueryLesson();
	PerfectWorld::SchoolLessonOutcome Snapshot;
	PerfectWorld::SchoolLessonMode Mode = PerfectWorld::SchoolLessonMode::Attend;
	if (View.Run)
	{
		Snapshot = View.Run->Outcome;
		Mode = View.Run->Mode;
	}
	Result = Bridge->CloseLesson(DisplayedSession, false, 0);
	LastResult = Result;
	if (Result.Ok())
	{
		ClosedOutcome = Snapshot;
		ClosedMode = Mode;
		bClosedCancelled = false;
		ClosedMinutes = 0;
		ClosedMilliXp = 0;
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
		bShowSummary = true;
		bCancelArmed = false;
		SetStatus(PWClassLesson::Text(TEXT("phase2.a2.common.status.closed")), FString());
	}
	else
	{
		SetStatus(PWClassLesson::IssueText(Result.Issue.Code), A2Str(Result.Issue.Code));
	}
	Rebuild();
	return Result;
}

PerfectWorld::SchoolSessionResult SPWClassLessonBase::TriggerCancelLesson(int32 ElapsedMinutes)
{
	PerfectWorld::SchoolSessionResult Result;
	if (!Bridge.IsValid())
	{
		Result.Issue.Code = "school.session.missing_catalogue";
		LastResult = Result;
		SetStatus(PWClassLesson::IssueText(Result.Issue.Code), A2Str(Result.Issue.Code));
		return Result;
	}
	const PerfectWorld::SchoolLessonView View = Bridge->QueryLesson();
	PerfectWorld::SchoolLessonMode Mode = PerfectWorld::SchoolLessonMode::Attend;
	int32 Elapsed = FMath::Max(0, ElapsedMinutes);
	if (View.Run)
	{
		Mode = View.Run->Mode;
		if (ElapsedMinutes < 0)
		{
			Elapsed = Mode == PerfectWorld::SchoolLessonMode::Foreground && !View.Run->ReadyToClose ? ElapsedSoFar(View) : 0;
		}
	}
	Result = Bridge->CloseLesson(DisplayedSession, true, Elapsed);
	LastResult = Result;
	if (Result.Ok())
	{
		ClosedOutcome = PerfectWorld::SchoolLessonOutcome();
		ClosedMode = Mode;
		bClosedCancelled = true;
		ClosedMinutes = Elapsed;
		ClosedMilliXp = 0;
		bShowSummary = true;
		bCancelArmed = false;
		SetStatus(PWClassLesson::Text(TEXT("phase2.a2.common.status.cancelled")), FString());
	}
	else
	{
		bCancelArmed = false;
		SetStatus(PWClassLesson::IssueText(Result.Issue.Code), A2Str(Result.Issue.Code));
	}
	Rebuild();
	return Result;
}

void SPWClassLessonBase::RequestLeavePanel()
{
	const TSharedRef<SWidget> KeepAlive = AsShared();
	PWUI::CancelPendingFocus(KeepAlive);
	const TWeakPtr<SWidget> Target = ReturnFocus;
	const FSimpleDelegate Closed = OnClosed;
	Closed.ExecuteIfBound();
	const TSharedPtr<SWidget> Pinned = Target.Pin();
	if (Pinned.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(Pinned, EFocusCause::SetDirectly);
	}
}

void SPWClassLessonBase::ScrollBy(float Delta)
{
	if (MainScroll.IsValid())
	{
		MainScroll->SetScrollOffset(FMath::Clamp(MainScroll->GetScrollOffset() + Delta, 0.0f, MainScroll->GetScrollOffsetOfEnd()));
	}
}

FReply SPWClassLessonBase::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	constexpr float KeyStep = 72.0f;
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		// A held key auto-repeats; leaving happens on the first press only.
		if (!InKeyEvent.IsRepeat())
		{
			RequestLeavePanel();
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_RightStick_Up)
	{
		ScrollBy(-KeyStep);
		return FReply::Handled();
	}
	if (Key == EKeys::Gamepad_RightStick_Down)
	{
		ScrollBy(KeyStep);
		return FReply::Handled();
	}
	if (Key == EKeys::PageUp || Key == EKeys::PageDown)
	{
		const float Page = FMath::Max(KeyStep, static_cast<float>(MyGeometry.GetLocalSize().Y) * 0.5f);
		ScrollBy(Key == EKeys::PageUp ? -Page : Page);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

TSharedRef<SWidget> SPWClassLessonBase::MakeChoiceRow(const FText& Label, TAttribute<bool> Selected, TAttribute<bool> Enabled,
	FOnClicked OnClicked, TAttribute<FText> Price)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	return SNew(SHorizontalBox)
		.Tag(FName(PWClassLesson::ChoiceTag))
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 10.0f, 0.0f)
		[
			PWUI::MakeButton(Label, PWUI::EButtonKind::Chip, OnClicked, Enabled, 18, 0.0f, Selected)
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			PWUI::MakeText(Price, 15, Style.TextSecondary, false, true)
		];
}

TSharedRef<SWidget> SPWClassLessonBase::MakeCommitRow(const FText& Label, TAttribute<bool> Enabled, FOnClicked OnClicked, TAttribute<FText> Price)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
		[
			PWUI::MakeButton(Label, PWUI::EButtonKind::Primary, OnClicked, Enabled, 19, 0.0f)
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			PWUI::MakeText(Price, 15, Style.TextSecondary, false, true)
		];
}

TSharedRef<SWidget> SPWClassLessonBase::BuildHeader(const PerfectWorld::SchoolPeriodView& Period)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const FString Prefix(TextPrefix());
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			PWUI::MakeText(PWClassLesson::Text(Prefix + TEXT("eyebrow")), PWUI::ETextRole::Meta, Style.Accent, true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(PWClassLesson::Text(Prefix + TEXT("title")), PWUI::ETextRole::PageHeading, Style.TextPrimary, true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return PWClassLesson::Compact(Space.Get()) ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				PWUI::MakeText(PWClassLesson::Text(Prefix + TEXT("subtitle")), 16, Style.TextSecondary, false, true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(PeriodLine(Period), PWUI::ETextRole::Meta, Style.TextSecondary, true)
		];
}

TSharedRef<SWidget> SPWClassLessonBase::BuildIntro(const PerfectWorld::SchoolPeriodView& Period, const PerfectWorld::SchoolLessonView& View)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::SchoolLessonBinding& Binding = Bridge->GetBinding();
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const FText Title = A2ResolveOr(Binding.TitleTextKey, FText::FromString(A2FamilyName(Bridge->GetCatalogues(), Binding)));
	Box->AddSlot().AutoHeight()[PWUI::MakeText(Title, 22, Style.TextPrimary, true, true)];
	FText Found;
	if (PWClassLesson::FindText(A2Str(Binding.BriefTextKey), Found))
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[PWUI::MakeText(Found, 17, Style.TextPrimary, false, true)];
	}
	if (PWClassLesson::FindText(A2Str(Binding.RoleTextKey), Found))
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)[PWUI::MakeText(Found, 16, Style.TextSecondary, false, true)];
	}
	const PWClassLesson::EMastery Mastery = MasteryOf(View, Binding.FamilyId);
	const TCHAR* MasteryKey = Mastery == PWClassLesson::EMastery::Secure ? TEXT("phase2.a2.common.mastery.secure")
		: Mastery == PWClassLesson::EMastery::Practised ? TEXT("phase2.a2.common.mastery.practised")
		: TEXT("phase2.a2.common.mastery.unseen");
	Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[PWUI::MakeText(PWClassLesson::Text(MasteryKey), 16, Style.Accent, true, true)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
	[
		PWUI::MakeText(PWClassLesson::Text2(TEXT("phase2.a2.common.foreground_used"), PWClassLesson::Number(Period.ForegroundUsed),
			PWClassLesson::Number(PerfectWorld::SchoolForegroundLimit)), 16, Style.TextSecondary, false, true)
	];
	if (!Period.Issue.Ok())
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)[PWClassLesson::MakeNote(PWClassLesson::IssueText(Period.Issue.Code))];
	}
	return Box;
}

TSharedRef<SWidget> SPWClassLessonBase::BuildModeCard(const FText& Title, const FText& Detail, const FText& ButtonLabel, bool bEnabled,
	const FText& WhyNot, PerfectWorld::SchoolLessonMode Mode)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[PWUI::MakeText(Title, 19, Style.TextPrimary, true, true)];
	Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 6.0f)[PWUI::MakeText(Detail, 15, Style.TextSecondary, false, true)];
	if (!bEnabled)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWUI::MakeText(WhyNot, 15, Style.Warn, false, true)];
	}
	Box->AddSlot().AutoHeight().HAlign(HAlign_Left)
	[
		PWUI::MakeButton(ButtonLabel, Mode == PerfectWorld::SchoolLessonMode::Foreground ? PWUI::EButtonKind::Primary : PWUI::EButtonKind::Secondary,
			FOnClicked::CreateSPLambda(this, [this, Mode]()
			{
				TriggerOpenLesson(Mode);
				return FReply::Handled();
			}), bEnabled, 19, 0.0f)
	];
	return SNew(SBorder)
		.Tag(FName(PWClassLesson::ModeCardTag))
		.BorderImage(&Style.BrushInset)
		.Padding(FMargin(14.0f, 10.0f))
		[
			Box
		];
}

TSharedRef<SWidget> SPWClassLessonBase::BuildModeCards(const PerfectWorld::SchoolPeriodView& Period)
{
	const bool bValid = Period.Issue.Ok();
	const FText Blocked = PWClassLesson::IssueText(Period.Issue.Code);
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.common.mode_heading")))];
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[BuildGoalPicker()];
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		BuildModeCard(PWClassLesson::Text(TEXT("phase2.a2.common.mode.foreground")), PWClassLesson::Text(TEXT("phase2.a2.common.mode.foreground.desc")),
			PWClassLesson::Text(TEXT("phase2.a2.common.open_foreground")), bValid && Period.ForegroundAvailable,
			bValid ? PWClassLesson::Text(TEXT("school.session.foreground_limit")) : Blocked, PerfectWorld::SchoolLessonMode::Foreground)
	];
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		BuildModeCard(PWClassLesson::Text(TEXT("phase2.a2.common.mode.routine")), PWClassLesson::Text(TEXT("phase2.a2.common.mode.routine.desc")),
			PWClassLesson::Text(TEXT("phase2.a2.common.open_routine")), bValid && Period.RoutineAvailable,
			bValid ? PWClassLesson::Text(TEXT("school.session.not_secure")) : Blocked, PerfectWorld::SchoolLessonMode::Routine)
	];
	Box->AddSlot().AutoHeight()
	[
		BuildModeCard(PWClassLesson::Text(TEXT("phase2.a2.common.mode.attend")), PWClassLesson::Text(TEXT("phase2.a2.common.mode.attend.desc")),
			PWClassLesson::Text(TEXT("phase2.a2.common.open_attend")), bValid && Period.AttendAvailable, Blocked, PerfectWorld::SchoolLessonMode::Attend)
	];
	return Box;
}

TSharedRef<SWidget> SPWClassLessonBase::BuildOutcomePane(const PerfectWorld::SchoolLessonRun& Run)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	PWClassLesson::FOutcomeFacts Facts;
	if (Run.Mode == PerfectWorld::SchoolLessonMode::Foreground && DescribeOutcome(Run.Outcome, Facts))
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.common.outcome.heading")))];
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[PWClassLesson::MakeOutcomeBlock(Facts, PWClassLesson::OutcomeTag)];
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a2.common.outcome.ready")), 16, Style.TextSecondary, false, true)
		];
	}
	else
	{
		const TCHAR* Key = Run.Mode == PerfectWorld::SchoolLessonMode::Routine ? TEXT("phase2.a2.common.outcome.routine") : TEXT("phase2.a2.common.outcome.attend");
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[PWClassLesson::MakeNote(PWClassLesson::Text(Key), PWClassLesson::OutcomeTag)];
	}
	Box->AddSlot().AutoHeight().HAlign(HAlign_Left)
	[
		PWUI::MakeButton(PWClassLesson::Text(TEXT("phase2.a2.common.complete_lesson")), PWUI::EButtonKind::Primary,
			FOnClicked::CreateSPLambda(this, [this]()
			{
				TriggerCloseLesson();
				return FReply::Handled();
			}), true, 20, 0.0f)
	];
	return Box;
}

TSharedRef<SWidget> SPWClassLessonBase::BuildSummaryPane()
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	const TCHAR* HeadingKey = bClosedCancelled ? TEXT("phase2.a2.common.summary.cancelled") : TEXT("phase2.a2.common.summary.closed");
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(HeadingKey))];
	PWClassLesson::FOutcomeFacts Facts;
	if (!bClosedCancelled && ClosedMode == PerfectWorld::SchoolLessonMode::Foreground && DescribeOutcome(ClosedOutcome, Facts))
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[PWClassLesson::MakeOutcomeBlock(Facts, PWClassLesson::OutcomeTag)];
	}
	else if (!bClosedCancelled)
	{
		const TCHAR* Key = ClosedMode == PerfectWorld::SchoolLessonMode::Routine ? TEXT("phase2.a2.common.summary.routine") : TEXT("phase2.a2.common.summary.attend");
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[PWClassLesson::MakeNote(PWClassLesson::Text(Key), PWClassLesson::OutcomeTag)];
	}
	else
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			PWClassLesson::MakeNote(PWClassLesson::Text(TEXT("phase2.a2.common.summary.cancelled.detail")))
		];
	}
	Box->AddSlot().AutoHeight()
	[
		PWUI::MakeText(PWClassLesson::Text1(TEXT("phase2.a2.common.summary.minutes"), PWClassLesson::Number(ClosedMinutes)), 16, Style.TextSecondary, false, true)
	];
	if (!bClosedCancelled)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(PWClassLesson::Text1(TEXT("phase2.a2.common.summary.learning"), PWClassLesson::Number(ClosedMilliXp)), 16, Style.TextSecondary, false, true)
		];
	}
	return SNew(SBorder)
		.Tag(FName(PWClassLesson::SummaryTag))
		.BorderImage(&Style.BrushInset)
		.Padding(FMargin(14.0f, 10.0f))
		[
			Box
		];
}

TSharedRef<SWidget> SPWClassLessonBase::BuildRunBody(const PerfectWorld::SchoolLessonView& View)
{
	const PerfectWorld::SchoolLessonRun& Run = *View.Run;
	const TSharedRef<SVerticalBox> Single = SNew(SVerticalBox);
	if (Run.Mode == PerfectWorld::SchoolLessonMode::Foreground && !Run.ReadyToClose && ExposureActive(View))
	{
		const PWUI::FStyle& Style = PWUI::FStyle::Get();
		const TSharedRef<SVerticalBox> Units = SNew(SVerticalBox);
		Units->AddSlot().AutoHeight()
		[
			PWUI::MakeText(PWClassLesson::Text2(TEXT("phase2.a2.common.units_remaining"), PWClassLesson::Number(UnitsLeft(View)),
				PWClassLesson::Number(UnitsTotal(View))), 17, Style.TextPrimary, true, true)
		];
		Units->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)[PWClassLesson::MakeUnitsPips(UnitsLeft(View), UnitsTotal(View))];
		if (bTwoPane)
		{
			const TSharedRef<SWidget> Side = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[Units]
				+ SVerticalBox::Slot().AutoHeight()[BuildCasePane(View)];
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(0.42f).Padding(0.0f, 0.0f, 12.0f, 0.0f)
				[
					SAssignNew(SideScroll, SScrollBox)
					.ScrollBarStyle(&Style.ScrollBar)
					.AllowOverscroll(EAllowOverscroll::No)
					+ SScrollBox::Slot().Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						Side
					]
				]
				+ SHorizontalBox::Slot().FillWidth(0.58f)
				[
					SAssignNew(MainScroll, SScrollBox)
					.ScrollBarStyle(&Style.ScrollBar)
					.AllowOverscroll(EAllowOverscroll::No)
					+ SScrollBox::Slot().Padding(0.0f, 0.0f, 10.0f, 0.0f)
					[
						BuildBeatPane(View)
					]
				];
		}
		Single->AddSlot().AutoHeight()[Units];
		Single->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[BuildCasePane(View)];
		Single->AddSlot().AutoHeight()[BuildBeatPane(View)];
	}
	else
	{
		Single->AddSlot().AutoHeight()[BuildOutcomePane(Run)];
	}
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	return SAssignNew(MainScroll, SScrollBox)
		.ScrollBarStyle(&Style.ScrollBar)
		.AllowOverscroll(EAllowOverscroll::No)
		+ SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
		[
			Single
		];
}

TSharedRef<SWidget> SPWClassLessonBase::BuildFooter(const PerfectWorld::SchoolLessonView& View)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const bool bCanCancel = View.Run.has_value() && !bShowSummary &&
		!(View.Run->Mode == PerfectWorld::SchoolLessonMode::Foreground && View.Run->ReadyToClose) &&
		Bridge.IsValid() && View.Run->Binding.Id == Bridge->GetBinding().Id;
	const bool bForegroundRun = View.Run.has_value() && View.Run->Mode == PerfectWorld::SchoolLessonMode::Foreground;
	const TSharedRef<SHorizontalBox> Buttons = SNew(SHorizontalBox);
	if (bCanCancel)
	{
		Buttons->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
		[
			PWUI::MakeButton(
				TAttribute<FText>::CreateLambda([this]()
				{
					return PWClassLesson::Text(bCancelArmed ? TEXT("phase2.a2.common.confirm_cancel") : TEXT("phase2.a2.common.cancel_lesson"));
				}),
				PWUI::EButtonKind::Secondary,
				FOnClicked::CreateSPLambda(this, [this]()
				{
					if (!bCancelArmed)
					{
						bCancelArmed = true;
						SetStatus(PWClassLesson::Text(TEXT("phase2.a2.common.cancel_note")), FString());
					}
					else
					{
						TriggerCancelLesson(-1);
					}
					return FReply::Handled();
				}), true, 17, 0.0f)
		];
	}
	Buttons->AddSlot().AutoWidth().VAlign(VAlign_Center)
	[
		PWUI::MakeButton(PWClassLesson::Text(TEXT("phase2.a2.common.leave_panel")), PWUI::EButtonKind::Secondary,
			FOnClicked::CreateSPLambda(this, [this]()
			{
				RequestLeavePanel();
				return FReply::Handled();
			}), true, 17, 0.0f)
	];
	if (bCanCancel)
	{
		const TCHAR* NoteKey = bForegroundRun ? TEXT("phase2.a2.common.cancel_note.foreground") : TEXT("phase2.a2.common.cancel_note.other");
		Buttons->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(PWClassLesson::Text(NoteKey), 14, Style.TextSecondary, false, true)
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Tag(FName(PWClassLesson::StatusTag))
			.Text_Lambda([this]() { return LastStatus; })
			.Font(PWUI::Font(16, PWUI::EFontWeight::Medium))
			.ColorAndOpacity(FSlateColor(Style.TextPrimary))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			Buttons
		];
}

void SPWClassLessonBase::Rebuild()
{
	if (!BodyHolder.IsValid())
	{
		return;
	}
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	bTwoPane = PWClassLesson::TwoPane(Space.Get());
	FocusTarget.Reset();
	MainScroll.Reset();
	SideScroll.Reset();

	PerfectWorld::SchoolPeriodView Period;
	PerfectWorld::SchoolLessonView View;
	if (Bridge.IsValid())
	{
		Period = Bridge->QueryPeriod();
		View = Bridge->QueryLesson();
	}
	const uint64 Session = View.Run ? View.Run->Session : 0;
	if (Session != DisplayedSession)
	{
		DisplayedSession = Session;
		ResetSelections();
		bCancelArmed = false;
	}

	const auto InScroll = [this, &Style](const TSharedRef<SWidget>& Content) -> TSharedRef<SWidget>
	{
		return SAssignNew(MainScroll, SScrollBox)
			.ScrollBarStyle(&Style.ScrollBar)
			.AllowOverscroll(EAllowOverscroll::No)
			+ SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				Content
			];
	};

	TSharedRef<SWidget> Body = SNullWidget::NullWidget;
	TSharedRef<SWidget> Header = SNullWidget::NullWidget;
	if (!Bridge.IsValid())
	{
		Body = InScroll(PWClassLesson::MakeNote(PWClassLesson::Text(TEXT("phase2.a2.common.no_bridge"))));
		Header = SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(PWClassLesson::Text(FString(TextPrefix()) + TEXT("title")), PWUI::ETextRole::PageHeading, Style.TextPrimary, true)];
	}
	else
	{
		Header = BuildHeader(Period);
		if (Bridge->GetBinding().Structure != Structure())
		{
			Body = InScroll(PWClassLesson::MakeNote(PWClassLesson::Text(TEXT("phase2.a2.common.wrong_screen"))));
		}
		else if (bShowSummary)
		{
			Body = InScroll(BuildSummaryPane());
		}
		else if (View.Run && View.Run->Binding.Id != Bridge->GetBinding().Id)
		{
			Body = InScroll(PWClassLesson::MakeNote(PWClassLesson::Text(TEXT("phase2.a2.common.other_session"))));
		}
		else if (View.Run)
		{
			Body = BuildRunBody(View);
		}
		else
		{
			const TSharedRef<SVerticalBox> Intro = SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)[BuildIntro(Period, View)]
				+ SVerticalBox::Slot().AutoHeight()[BuildModeCards(Period)];
			Body = InScroll(Intro);
		}
	}

	const TSharedRef<SWidget> Footer = BuildFooter(View);
	const TSharedRef<SVerticalBox> Root = SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[Header]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)[PWUI::MakeRule()]
		+ SVerticalBox::Slot().FillHeight(1.0f)[Body]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)[PWUI::MakeRule()]
		+ SVerticalBox::Slot().AutoHeight()[Footer];
	BodyHolder->SetContent(Root);
	RegisterFocus(Root);
	PWUI::FocusWhenReady(AsShared(), FocusTarget);
}

// ---- The model-a-system screen -----------------------------------------------------------------------------------------------

void SPWClassModel::Construct(const FArguments& InArgs)
{
	FLessonInit Init;
	Init.Bridge = InArgs._Bridge;
	Init.Space = InArgs._Space;
	Init.ReturnFocus = InArgs._ReturnFocusTarget;
	Init.OnClosed = InArgs._OnClosed;
	InitLesson(Init);
}

void SPWClassModel::ResetSelections()
{
	SelectedLinks.Reset();
	SelectedInvestigation = PerfectWorld::ModelInvestigation::None;
	SelectedClaim = PerfectWorld::ModelClaim::BoundedRange;
	bClaimChosen = false;
	bExplain = false;
	bExplainTouched = false;
}

namespace
{
FString A2Key(const TCHAR* Prefix, const TCHAR* Group, std::string_view Name)
{
	return FString(Prefix) + Group + A2Str(Name);
}

FText A2QuantityName(const PerfectWorld::ModelCaseDef& Def, const std::string& Id)
{
	for (const PerfectWorld::ModelQuantity& Quantity : Def.Quantities)
	{
		if (Quantity.Id == Id)
		{
			return PWClassLesson::FromUtf8(Quantity.Name);
		}
	}
	return PWClassLesson::Text(TEXT("phase2.a2.model.quantity.other"));
}

struct FA2ModelFail
{
	uint8 Bit;
	const TCHAR* Key;
};

const FA2ModelFail GA2ModelFails[] = {
	{PerfectWorld::ModelFailWrongModel, TEXT("phase2.a2.model.fail.wrong_model")},
	{PerfectWorld::ModelFailUnsupported, TEXT("phase2.a2.model.fail.unsupported")},
	{PerfectWorld::ModelFailWastedTime, TEXT("phase2.a2.model.fail.wasted_time")},
	{PerfectWorld::ModelFailUnclear, TEXT("phase2.a2.model.fail.unclear")},
};

const PerfectWorld::ModelInvestigation GA2Investigations[] = {
	PerfectWorld::ModelInvestigation::AcceptReadings, PerfectWorld::ModelInvestigation::CheckTimes, PerfectWorld::ModelInvestigation::LookForFlow, PerfectWorld::ModelInvestigation::CompareMarkings};
const PerfectWorld::ModelClaim GA2Claims[] = {PerfectWorld::ModelClaim::Estimate, PerfectWorld::ModelClaim::BoundedRange, PerfectWorld::ModelClaim::NeedMoreMeasurement};
const PerfectWorld::ModelGoal GA2Goals[] = {PerfectWorld::ModelGoal::UnderstandModel, PerfectWorld::ModelGoal::TestMeasurements, PerfectWorld::ModelGoal::ExplainToGroup};

PerfectWorld::SchoolSessionResult A2Refusal(const char* Code)
{
	PerfectWorld::SchoolSessionResult Result;
	Result.Issue.Code = Code;
	return Result;
}
} // namespace

PWClassLesson::EMastery SPWClassModel::MasteryOf(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const
{
	const PerfectWorld::ModelFamilyRecord* Record = View.Model.FindFamily(FamilyId);
	if (Record == nullptr)
	{
		return PWClassLesson::EMastery::Unseen;
	}
	if (Record->Secure)
	{
		return PWClassLesson::EMastery::Secure;
	}
	return Record->Exposures > 0 ? PWClassLesson::EMastery::Practised : PWClassLesson::EMastery::Unseen;
}

const PerfectWorld::ModelCaseDef* SPWClassModel::ActiveCase(const PerfectWorld::SchoolLessonView& View) const
{
	if (!Bridge.IsValid() || Bridge->GetCatalogues().Structures.Model == nullptr)
	{
		return nullptr;
	}
	return Bridge->GetCatalogues().Structures.Model->FindCase(View.Model.Exposure.CaseId);
}

void SPWClassModel::SelectGoal(PerfectWorld::ModelGoal InGoal)
{
	SelectedGoal = InGoal;
}

void SPWClassModel::ToggleLink(const FString& LinkId)
{
	if (SelectedLinks.Contains(LinkId))
	{
		SelectedLinks.Remove(LinkId);
	}
	else if (SelectedLinks.Num() < static_cast<int32>(PerfectWorld::ModelMaxLinks))
	{
		SelectedLinks.Add(LinkId);
	}
}

void SPWClassModel::SelectInvestigation(PerfectWorld::ModelInvestigation InInvestigation)
{
	SelectedInvestigation = InInvestigation;
}

void SPWClassModel::SelectClaim(PerfectWorld::ModelClaim InClaim)
{
	SelectedClaim = InClaim;
	bClaimChosen = true;
}

void SPWClassModel::SetExplain(bool bInExplain)
{
	bExplain = bInExplain;
	bExplainTouched = true;
}

PerfectWorld::ModelAction SPWClassModel::CommitAction() const
{
	PerfectWorld::ModelAction Action;
	Action.Kind = PerfectWorld::ModelActionKind::CommitModel;
	for (const FString& Link : SelectedLinks)
	{
		Action.Links.push_back(A2Utf8(Link));
	}
	return Action;
}

PerfectWorld::ModelAction SPWClassModel::InvestigateActionNow() const
{
	PerfectWorld::ModelAction Action;
	Action.Kind = PerfectWorld::ModelActionKind::Investigate;
	Action.Investigation = SelectedInvestigation;
	return Action;
}

PerfectWorld::ModelAction SPWClassModel::ClaimAction() const
{
	PerfectWorld::ModelAction Action;
	Action.Kind = PerfectWorld::ModelActionKind::PresentClaim;
	Action.Claim = SelectedClaim;
	Action.Explain = bExplain;
	return Action;
}

PerfectWorld::SchoolSessionResult SPWClassModel::TriggerWorkedExample()
{
	PerfectWorld::ModelAction Action;
	Action.Kind = PerfectWorld::ModelActionKind::WorkedExample;
	return RunChoose(PerfectWorld::SchoolLessonAction(Action), TEXT("phase2.a2.model.status.example"));
}

PerfectWorld::SchoolSessionResult SPWClassModel::TriggerCommitModel()
{
	return RunChoose(PerfectWorld::SchoolLessonAction(CommitAction()), TEXT("phase2.a2.model.status.model"));
}

PerfectWorld::SchoolSessionResult SPWClassModel::TriggerCommitInvestigation()
{
	if (SelectedInvestigation == PerfectWorld::ModelInvestigation::None)
	{
		SetStatus(PWClassLesson::Text(TEXT("phase2.a2.model.status.choose_investigation")), FString());
		return A2Refusal("model.bad_model");
	}
	return RunChoose(PerfectWorld::SchoolLessonAction(InvestigateActionNow()), TEXT("phase2.a2.model.status.investigation"));
}

PerfectWorld::SchoolSessionResult SPWClassModel::TriggerPresentClaim()
{
	if (!bClaimChosen)
	{
		SetStatus(PWClassLesson::Text(TEXT("phase2.a2.model.status.choose_claim")), FString());
		return A2Refusal("model.bad_model");
	}
	return RunChoose(PerfectWorld::SchoolLessonAction(ClaimAction()), TEXT("phase2.a2.model.status.claim"));
}

TSharedRef<SWidget> SPWClassModel::BuildGoalPicker()
{
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.common.goal_heading")))];
	for (const PerfectWorld::ModelGoal Goal : GA2Goals)
	{
		const FString Key = A2Key(TEXT("phase2.a2.model."), TEXT("goal."), PerfectWorld::ModelNameOf(PerfectWorld::ModelGoalNames, Goal));
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeChoiceRow(PWClassLesson::Text(Key),
				TAttribute<bool>::CreateLambda([this, Goal]() { return SelectedGoal == Goal; }), true,
				FOnClicked::CreateSPLambda(this, [this, Goal]()
				{
					SelectGoal(Goal);
					return FReply::Handled();
				}), FText::GetEmpty())
		];
	}
	return Box;
}

TSharedRef<SWidget> SPWClassModel::BuildCasePane(const PerfectWorld::SchoolLessonView& View)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::ModelCaseDef* Def = ActiveCase(View);
	if (Def == nullptr)
	{
		return PWClassLesson::MakeNote(PWClassLesson::Text(TEXT("phase2.a2.common.case_missing")));
	}
	const PerfectWorld::ModelExposure& Exp = View.Model.Exposure;
	const TCHAR* Prefix = TEXT("phase2.a2.model.");
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[PWClassLesson::MakeHeading(PWClassLesson::FromUtf8(Def->Name))];
	Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)[PWUI::MakeText(PWClassLesson::FromUtf8(Def->Brief), 17, Style.TextPrimary, false, true)];
	const FText Facts[] = {
		PWClassLesson::Text(A2Key(Prefix, TEXT("unknown."), PerfectWorld::ModelNameOf(PerfectWorld::ModelUnknownNames, Def->Unknown))),
		PWClassLesson::Text(A2Key(Prefix, TEXT("role."), PerfectWorld::ModelNameOf(PerfectWorld::ModelRoleNames, Def->Role))),
		PWClassLesson::Text(A2Key(Prefix, TEXT("scarce."), PerfectWorld::ModelNameOf(PerfectWorld::ModelScarceNames, Def->Scarce))),
		PWClassLesson::Text(Exp.MethodKnown ? TEXT("phase2.a2.model.method.known")
			: Exp.ExampleTaken ? TEXT("phase2.a2.model.method.example_taken") : TEXT("phase2.a2.model.method.unfamiliar")),
	};
	for (const FText& Fact : Facts)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)[PWUI::MakeText(Fact, 16, Style.TextSecondary, false, true)];
	}
	Box->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 4.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.model.quantities_heading")))];
	for (const PerfectWorld::ModelQuantity& Quantity : Def->Quantities)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				PWUI::MakeText(PWClassLesson::FromUtf8(Quantity.Name), 16, Style.TextPrimary, false, true)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				PWUI::MakeText(PWClassLesson::Text(Quantity.Given ? TEXT("phase2.a2.model.quantity.given") : TEXT("phase2.a2.model.quantity.unknown")),
					PWUI::ETextRole::Meta, Quantity.Given ? Style.TextSecondary : Style.Accent, true)
			]
		];
	}
	return Box;
}

TSharedRef<SWidget> SPWClassModel::BuildBeatPane(const PerfectWorld::SchoolLessonView& View)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::ModelCaseDef* Def = ActiveCase(View);
	if (Def == nullptr)
	{
		return PWClassLesson::MakeNote(PWClassLesson::Text(TEXT("phase2.a2.common.case_missing")));
	}
	const PerfectWorld::ModelExposure& Exp = View.Model.Exposure;
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (Exp.Beat == PerfectWorld::ModelBeat::BuildModel)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.model.beat.build_model")))];
		if (!Exp.MethodKnown && !Exp.ExampleTaken)
		{
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.model.action.worked_example")), true,
					FOnClicked::CreateSPLambda(this, [this]()
					{
						TriggerWorkedExample();
						return FReply::Handled();
					}),
					TAttribute<FText>::CreateLambda([this]()
					{
						PerfectWorld::ModelAction Action;
						Action.Kind = PerfectWorld::ModelActionKind::WorkedExample;
						return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(Action)));
					}))
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a2.model.links_hint")), 15, Style.TextSecondary, false, true)];
		for (const PerfectWorld::ModelLink& Link : Def->Links)
		{
			const FString LinkId = A2Str(Link.Id);
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeChoiceRow(PWClassLesson::Text2(TEXT("phase2.a2.model.link"), A2QuantityName(*Def, Link.From), A2QuantityName(*Def, Link.To)),
					TAttribute<bool>::CreateLambda([this, LinkId]() { return IsLinkSelected(LinkId); }), true,
					FOnClicked::CreateSPLambda(this, [this, LinkId]()
					{
						ToggleLink(LinkId);
						return FReply::Handled();
					}), FText::GetEmpty())
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.model.action.commit_model")),
				TAttribute<bool>::CreateLambda([this]() { return !SelectedLinks.IsEmpty(); }),
				FOnClicked::CreateSPLambda(this, [this]()
				{
					TriggerCommitModel();
					return FReply::Handled();
				}),
				TAttribute<FText>::CreateLambda([this]() { return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(CommitAction()))); }))
		];
	}
	else if (Exp.Beat == PerfectWorld::ModelBeat::ResolveDiscrepancy)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.model.beat.resolve_discrepancy")))];
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a2.model.investigate_hint")), 15, Style.TextSecondary, false, true)];
		for (const PerfectWorld::ModelInvestigation Investigation : GA2Investigations)
		{
			const FString Key = A2Key(TEXT("phase2.a2.model.action.investigate."), TEXT(""), PerfectWorld::ModelNameOf(PerfectWorld::ModelInvestigationNames, Investigation));
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeChoiceRow(PWClassLesson::Text(Key),
					TAttribute<bool>::CreateLambda([this, Investigation]() { return SelectedInvestigation == Investigation; }), true,
					FOnClicked::CreateSPLambda(this, [this, Investigation]()
					{
						SelectInvestigation(Investigation);
						return FReply::Handled();
					}),
					TAttribute<FText>::CreateLambda([this, Investigation]()
					{
						PerfectWorld::ModelAction Action;
						Action.Kind = PerfectWorld::ModelActionKind::Investigate;
						Action.Investigation = Investigation;
						return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(Action)));
					}))
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.model.action.commit_investigation")),
				TAttribute<bool>::CreateLambda([this]() { return SelectedInvestigation != PerfectWorld::ModelInvestigation::None; }),
				FOnClicked::CreateSPLambda(this, [this]()
				{
					TriggerCommitInvestigation();
					return FReply::Handled();
				}),
				TAttribute<FText>::CreateLambda([this]()
				{
					return SelectedInvestigation == PerfectWorld::ModelInvestigation::None ? FText::GetEmpty()
						: PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(InvestigateActionNow())));
				}))
		];
	}
	else
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.model.beat.present_claim")))];
		for (const PerfectWorld::ModelClaim Claim : GA2Claims)
		{
			const FString Key = A2Key(TEXT("phase2.a2.model.action.claim."), TEXT(""), PerfectWorld::ModelNameOf(PerfectWorld::ModelClaimNames, Claim));
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeChoiceRow(PWClassLesson::Text(Key),
					TAttribute<bool>::CreateLambda([this, Claim]() { return bClaimChosen && SelectedClaim == Claim; }), true,
					FOnClicked::CreateSPLambda(this, [this, Claim]()
					{
						SelectClaim(Claim);
						return FReply::Handled();
					}), FText::GetEmpty())
			];
		}
		// The group explanation: owed by the role or the goal, and the screen only says so. The core decides the consequence.
		const bool bOwed = PerfectWorld::ModelExplanationOwed(*Def, Exp.Goal);
		if (!bExplainTouched)
		{
			bExplain = bOwed;
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 4.0f)
		[
			MakeChoiceRow(PWClassLesson::Text(TEXT("phase2.a2.model.action.explain_toggle")),
				TAttribute<bool>::CreateLambda([this]() { return bExplain; }), true,
				FOnClicked::CreateSPLambda(this, [this]()
				{
					SetExplain(!bExplain);
					return FReply::Handled();
				}),
				TAttribute<FText>::CreateLambda([this]()
				{
					PerfectWorld::ModelAction Action = ClaimAction();
					Action.Explain = true;
					return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(Action)));
				}))
		];
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			PWUI::MakeText(PWClassLesson::Text(bOwed ? TEXT("phase2.a2.model.action.explain_owed") : TEXT("phase2.a2.model.action.explain_optional")),
				15, bOwed ? Style.Warn : Style.TextSecondary, false, true)
		];
		Box->AddSlot().AutoHeight()
		[
			MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.model.action.submit_claim")),
				TAttribute<bool>::CreateLambda([this]() { return bClaimChosen; }),
				FOnClicked::CreateSPLambda(this, [this]()
				{
					TriggerPresentClaim();
					return FReply::Handled();
				}),
				TAttribute<FText>::CreateLambda([this]() { return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(ClaimAction()))); }))
		];
	}
	return Box;
}

bool SPWClassModel::DescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const
{
	const auto AddFailures = [&OutFacts](uint8 Failures)
	{
		for (const FA2ModelFail& Fail : GA2ModelFails)
		{
			if ((Failures & Fail.Bit) != 0)
			{
				OutFacts.Diagnostics.Add(PWClassLesson::Text(Fail.Key));
			}
		}
	};
	if (const PerfectWorld::ModelOutcome* Model = std::get_if<PerfectWorld::ModelOutcome>(&Outcome))
	{
		OutFacts.Grade = A2ModelGrade(Model->Grade);
		OutFacts.bForeground = Model->Foreground;
		OutFacts.LearningPermille = Model->LearningPermille;
		OutFacts.UnitsSpent = Model->UnitsSpent;
		OutFacts.bBecameSecure = Model->BecameSecure;
		if (Model->MissingLinks > 0)
		{
			OutFacts.Detail.Add(PWClassLesson::Text1(TEXT("phase2.a2.model.outcome.missing_links"), PWClassLesson::Number(Model->MissingLinks)));
		}
		if (Model->ExtraLinks > 0)
		{
			OutFacts.Detail.Add(PWClassLesson::Text1(TEXT("phase2.a2.model.outcome.extra_links"), PWClassLesson::Number(Model->ExtraLinks)));
		}
		AddFailures(Model->Failures);
		return true;
	}
	if (const PerfectWorld::StructureOutcome* Generic = std::get_if<PerfectWorld::StructureOutcome>(&Outcome))
	{
		OutFacts.Grade = Generic->Grade;
		OutFacts.bForeground = Generic->Foreground;
		OutFacts.LearningPermille = Generic->LearningPermille;
		OutFacts.UnitsSpent = Generic->UnitsSpent;
		OutFacts.bBecameSecure = Generic->BecameSecure;
		AddFailures(Generic->Failures);
		return true;
	}
	return false;
}

namespace
{
const FA2TextEntry GA2Entries[] = {
	{TEXT("phase2.a2.common.cancel_lesson"), TEXT("Cancel lesson")},
	{TEXT("phase2.a2.common.cancel_note"), TEXT("Press again to cancel. A cancelled lesson earns no learning and the period cannot be retaken.")},
	{TEXT("phase2.a2.common.cancel_note.foreground"), TEXT("Cancelling earns no learning credit, charges the time already worked, and still counts as one of today's two foreground lessons.")},
	{TEXT("phase2.a2.common.cancel_note.other"), TEXT("Cancelling earns no learning credit.")},
	{TEXT("phase2.a2.common.case_missing"), TEXT("This lesson's material cannot be found.")},
	{TEXT("phase2.a2.common.complete_lesson"), TEXT("Complete and close the period")},
	{TEXT("phase2.a2.common.confirm_cancel"), TEXT("Confirm cancel")},
	{TEXT("phase2.a2.common.foreground_used"), TEXT("Foreground lessons today: {0} of {1}")},
	{TEXT("phase2.a2.common.goal_heading"), TEXT("Choose your goal for this lesson")},
	{TEXT("phase2.a2.common.grade.partial"), TEXT("Result: partly sound")},
	{TEXT("phase2.a2.common.grade.sound"), TEXT("Result: sound")},
	{TEXT("phase2.a2.common.grade.unsound"), TEXT("Result: not yet sound")},
	{TEXT("phase2.a2.common.leave_panel"), TEXT("Return to classroom")},
	{TEXT("phase2.a2.common.mastery.practised"), TEXT("Mastery: practised")},
	{TEXT("phase2.a2.common.mastery.secure"), TEXT("Mastery: secure (routine lessons available)")},
	{TEXT("phase2.a2.common.mastery.unseen"), TEXT("Mastery: not yet practised")},
	{TEXT("phase2.a2.common.mode.attend"), TEXT("Attend")},
	{TEXT("phase2.a2.common.mode.attend.desc"), TEXT("Sit the period as supervised instruction. Normal attendance credit, but nothing new is observed.")},
	{TEXT("phase2.a2.common.mode.foreground"), TEXT("Foreground lesson")},
	{TEXT("phase2.a2.common.mode.foreground.desc"), TEXT("Work the lesson by hand. Every choice shows its cost before you commit. At most two foreground lessons a day.")},
	{TEXT("phase2.a2.common.mode.routine"), TEXT("Routine lesson")},
	{TEXT("phase2.a2.common.mode.routine.desc"), TEXT("Use the method you have mastered. Normal attendance and normal learning, no manual play.")},
	{TEXT("phase2.a2.common.mode_heading"), TEXT("How will you take this period?")},
	{TEXT("phase2.a2.common.no_bridge"), TEXT("This lesson cannot be reached right now. Return to the classroom and try again.")},
	{TEXT("phase2.a2.common.open_attend"), TEXT("Attend period")},
	{TEXT("phase2.a2.common.open_foreground"), TEXT("Begin foreground lesson")},
	{TEXT("phase2.a2.common.open_routine"), TEXT("Run routine lesson")},
	{TEXT("phase2.a2.common.other_session"), TEXT("Another lesson is already open. Finish or cancel it first.")},
	{TEXT("phase2.a2.common.outcome.attend"), TEXT("Attendance recorded for this period. Close it to commit the period time.")},
	{TEXT("phase2.a2.common.outcome.became_secure"), TEXT("This method is now secure: routine lessons are available.")},
	{TEXT("phase2.a2.common.outcome.heading"), TEXT("How it went")},
	{TEXT("phase2.a2.common.outcome.learning_permille"), TEXT("Learning credit: {0} of 1000")},
	{TEXT("phase2.a2.common.outcome.no_diagnostics"), TEXT("Nothing to correct.")},
	{TEXT("phase2.a2.common.outcome.ready"), TEXT("The lesson is resolved. Close the period to record the time, your condition and the learning credit.")},
	{TEXT("phase2.a2.common.outcome.routine"), TEXT("Routine lesson ready. Close it to commit the period time and learning credit.")},
	{TEXT("phase2.a2.common.period_header"), TEXT("Period {0} - {1} ({2})")},
	{TEXT("phase2.a2.common.quote.cost"), TEXT("Cost: {0} units (leaves {1})")},
	{TEXT("phase2.a2.common.quote.free"), TEXT("Free (leaves {0})")},
	{TEXT("phase2.a2.common.quote.unavailable"), TEXT("Unavailable: {0}")},
	{TEXT("phase2.a2.common.status.cancelled"), TEXT("Lesson cancelled.")},
	{TEXT("phase2.a2.common.status.closed"), TEXT("Period recorded.")},
	{TEXT("phase2.a2.common.status.done"), TEXT("{0} Charged: {1} units.")},
	{TEXT("phase2.a2.common.status.opened_attend"), TEXT("Attendance begun.")},
	{TEXT("phase2.a2.common.status.opened_foreground"), TEXT("Foreground lesson begun.")},
	{TEXT("phase2.a2.common.status.opened_routine"), TEXT("Routine lesson begun.")},
	{TEXT("phase2.a2.common.summary.attend"), TEXT("Attendance recorded.")},
	{TEXT("phase2.a2.common.summary.cancelled"), TEXT("Lesson cancelled")},
	{TEXT("phase2.a2.common.summary.cancelled.detail"), TEXT("No learning was credited. The time already worked has been charged.")},
	{TEXT("phase2.a2.common.summary.closed"), TEXT("Period recorded")},
	{TEXT("phase2.a2.common.summary.learning"), TEXT("Learning credited: {0} milli-XP")},
	{TEXT("phase2.a2.common.summary.minutes"), TEXT("Time recorded: {0} minutes")},
	{TEXT("phase2.a2.common.summary.routine"), TEXT("Routine lesson recorded with normal learning.")},
	{TEXT("phase2.a2.common.units_remaining"), TEXT("Units remaining: {0} of {1}")},
	{TEXT("phase2.a2.common.units_spent"), TEXT("Units spent: {0}")},
	{TEXT("phase2.a2.common.wrong_screen"), TEXT("This period belongs to a different kind of lesson.")},
	{TEXT("phase2.a2.explain.action.commit_claim"), TEXT("Commit the selected claim")},
	{TEXT("phase2.a2.explain.action.commit_evidence"), TEXT("Place the selected evidence")},
	{TEXT("phase2.a2.explain.action.commit_limitation"), TEXT("Commit the selected limitation")},
	{TEXT("phase2.a2.explain.action.model_paragraph"), TEXT("Read a model paragraph")},
	{TEXT("phase2.a2.explain.action.no_limitation"), TEXT("State it without a further limitation")},
	{TEXT("phase2.a2.explain.beat.claim"), TEXT("Step 1: choose a claim")},
	{TEXT("phase2.a2.explain.beat.evidence"), TEXT("Step 2: place your evidence")},
	{TEXT("phase2.a2.explain.beat.limitation"), TEXT("Step 3: state a limitation")},
	{TEXT("phase2.a2.explain.claims_hint"), TEXT("Choose the claim you will argue. Several may be defensible.")},
	{TEXT("phase2.a2.explain.evidence_hint"), TEXT("Place the evidence you will rely on (at most two items). Not everything on the table helps.")},
	{TEXT("phase2.a2.explain.eyebrow"), TEXT("CLASS XI - INTERPRETATION AND ARGUMENT")},
	{TEXT("phase2.a2.explain.fail.indefensible"), TEXT("The claim could not be defended from the sources.")},
	{TEXT("phase2.a2.explain.fail.overreach"), TEXT("The claim needed a limitation that fits it, and did not have one.")},
	{TEXT("phase2.a2.explain.fail.padding"), TEXT("Evidence that did not support the claim diluted the argument.")},
	{TEXT("phase2.a2.explain.fail.unsupported"), TEXT("No placed evidence supported the claim.")},
	{TEXT("phase2.a2.explain.goal.argue_claim"), TEXT("Goal: argue a clear claim")},
	{TEXT("phase2.a2.explain.goal.state_limits"), TEXT("Goal: state the limits")},
	{TEXT("phase2.a2.explain.goal.weigh_evidence"), TEXT("Goal: weigh the evidence")},
	{TEXT("phase2.a2.explain.item.other"), TEXT("An item")},
	{TEXT("phase2.a2.explain.limitation_hint"), TEXT("Say where your claim stops, or state it without a further limitation.")},
	{TEXT("phase2.a2.explain.method.known"), TEXT("You know how to read these sources closely.")},
	{TEXT("phase2.a2.explain.method.paragraph_taken"), TEXT("You have read a model paragraph.")},
	{TEXT("phase2.a2.explain.method.unfamiliar"), TEXT("Close reading is unfamiliar here. A model paragraph costs time, as does reading unaided.")},
	{TEXT("phase2.a2.explain.outcome.bounded"), TEXT("Your limitation bounded the claim.")},
	{TEXT("phase2.a2.explain.outcome.evidence"), TEXT("Evidence placed: {0}. Of these, supporting your claim: {1}.")},
	{TEXT("phase2.a2.explain.outcome.unbounded"), TEXT("Your claim was left without a limitation that bounds it.")},
	{TEXT("phase2.a2.explain.question.cause"), TEXT("The question asks: what caused it?")},
	{TEXT("phase2.a2.explain.question.judgement"), TEXT("The question asks: how should it be judged?")},
	{TEXT("phase2.a2.explain.question.meaning"), TEXT("The question asks: what does it mean?")},
	{TEXT("phase2.a2.explain.role.author"), TEXT("Your seat: author")},
	{TEXT("phase2.a2.explain.role.critic"), TEXT("Your seat: critic")},
	{TEXT("phase2.a2.explain.role.recorder"), TEXT("Your seat: recorder")},
	{TEXT("phase2.a2.explain.role.researcher"), TEXT("Your seat: researcher")},
	{TEXT("phase2.a2.explain.scarce.counterpoint"), TEXT("Short of: counterpoint notes")},
	{TEXT("phase2.a2.explain.scarce.help"), TEXT("Short of: teacher feedback")},
	{TEXT("phase2.a2.explain.scarce.sources"), TEXT("Short of: source extracts")},
	{TEXT("phase2.a2.explain.scarce.time"), TEXT("Short of: drafting time")},
	{TEXT("phase2.a2.explain.status.choose_claim"), TEXT("Choose a claim first.")},
	{TEXT("phase2.a2.explain.status.choose_evidence"), TEXT("Choose at least one piece of evidence first.")},
	{TEXT("phase2.a2.explain.status.choose_limitation"), TEXT("Choose a limitation, or state the claim without one.")},
	{TEXT("phase2.a2.explain.status.claim"), TEXT("Claim committed.")},
	{TEXT("phase2.a2.explain.status.evidence"), TEXT("Evidence placed.")},
	{TEXT("phase2.a2.explain.status.limitation"), TEXT("Limitation stated.")},
	{TEXT("phase2.a2.explain.status.no_limitation"), TEXT("Claim stated without a further limitation.")},
	{TEXT("phase2.a2.explain.status.paragraph"), TEXT("You read the model paragraph.")},
	{TEXT("phase2.a2.explain.status.slots_full"), TEXT("Only {0} items fit. Remove one first.")},
	{TEXT("phase2.a2.explain.subtitle"), TEXT("Advance an interpretation you can defend, back it with evidence, and say where it stops. More than one reading can be defended.")},
	{TEXT("phase2.a2.explain.title"), TEXT("Claim, evidence and limitation lesson")},
	{TEXT("phase2.a2.explain.your_claim"), TEXT("Your claim")},
	{TEXT("phase2.a2.explain.your_evidence"), TEXT("Evidence placed")},
	{TEXT("phase2.a2.issue.dayloop.already_committed"), TEXT("Something is already lined up.")},
	{TEXT("phase2.a2.issue.dayloop.asleep"), TEXT("You are asleep.")},
	{TEXT("phase2.a2.issue.dayloop.busy"), TEXT("You are busy with something else right now.")},
	{TEXT("phase2.a2.issue.dayloop.invalid_command"), TEXT("That could not be done.")},
	{TEXT("phase2.a2.issue.dayloop.invalid_state"), TEXT("Your day is not in a usable state.")},
	{TEXT("phase2.a2.issue.dayloop.missing_catalogue"), TEXT("The school timetable could not be read.")},
	{TEXT("phase2.a2.issue.dayloop.not_asleep"), TEXT("You are not asleep.")},
	{TEXT("phase2.a2.issue.dayloop.not_committed"), TEXT("Nothing is lined up to start.")},
	{TEXT("phase2.a2.issue.dayloop.not_running"), TEXT("Nothing is running.")},
	{TEXT("phase2.a2.issue.dayloop.plan_first"), TEXT("Plan the day first.")},
	{TEXT("phase2.a2.issue.dayloop.unknown_activity"), TEXT("That activity is not known.")},
	{TEXT("phase2.a2.issue.dayloop.wrong_place"), TEXT("You are not in the right place for that.")},
	{TEXT("phase2.a2.issue.model.bad_model"), TEXT("Choose at least one link, and no link twice.")},
	{TEXT("phase2.a2.issue.model.busy"), TEXT("A problem is already open.")},
	{TEXT("phase2.a2.issue.model.example_not_needed"), TEXT("You do not need a worked example.")},
	{TEXT("phase2.a2.issue.model.invalid_skill"), TEXT("Your skill could not be read.")},
	{TEXT("phase2.a2.issue.model.invalid_state"), TEXT("The lesson record is not in a usable state.")},
	{TEXT("phase2.a2.issue.model.not_active"), TEXT("There is no problem open.")},
	{TEXT("phase2.a2.issue.model.not_secure"), TEXT("This method is not secure yet.")},
	{TEXT("phase2.a2.issue.model.units_short"), TEXT("There is not enough working time left for that.")},
	{TEXT("phase2.a2.issue.model.unknown_case"), TEXT("This lesson's problem cannot be found.")},
	{TEXT("phase2.a2.issue.model.unknown_family"), TEXT("This lesson's material cannot be found.")},
	{TEXT("phase2.a2.issue.model.unknown_link"), TEXT("That link is not part of this problem.")},
	{TEXT("phase2.a2.issue.model.wrong_beat"), TEXT("That choice belongs to a different step.")},
	{TEXT("phase2.a2.issue.structure.already_done"), TEXT("That has already been done.")},
	{TEXT("phase2.a2.issue.structure.bad_choice"), TEXT("That is not a valid choice.")},
	{TEXT("phase2.a2.issue.structure.busy"), TEXT("A case is already open.")},
	{TEXT("phase2.a2.issue.structure.invalid_skill"), TEXT("Your skill could not be read.")},
	{TEXT("phase2.a2.issue.structure.invalid_state"), TEXT("The lesson record is not in a usable state.")},
	{TEXT("phase2.a2.issue.structure.not_active"), TEXT("There is no case open.")},
	{TEXT("phase2.a2.issue.structure.not_needed"), TEXT("You do not need that.")},
	{TEXT("phase2.a2.issue.structure.not_secure"), TEXT("This method is not secure yet.")},
	{TEXT("phase2.a2.issue.structure.units_short"), TEXT("There is not enough working time left for that.")},
	{TEXT("phase2.a2.issue.structure.unknown_case"), TEXT("This lesson's case cannot be found.")},
	{TEXT("phase2.a2.issue.structure.unknown_family"), TEXT("This lesson's material cannot be found.")},
	{TEXT("phase2.a2.issue.structure.unknown_item"), TEXT("That item is not part of this case.")},
	{TEXT("phase2.a2.issue.structure.wrong_beat"), TEXT("That choice belongs to a different step.")},
	{TEXT("phase2.a2.issue.unknown"), TEXT("That could not be done right now.")},
	{TEXT("phase2.a2.model.action.claim.bounded_range"), TEXT("State a bounded range")},
	{TEXT("phase2.a2.model.action.claim.estimate"), TEXT("State a single estimate")},
	{TEXT("phase2.a2.model.action.claim.need_more_measurement"), TEXT("Conclude that more measurement is needed")},
	{TEXT("phase2.a2.model.action.commit_investigation"), TEXT("Commit this check")},
	{TEXT("phase2.a2.model.action.commit_model"), TEXT("Commit the selected links")},
	{TEXT("phase2.a2.model.action.explain_optional"), TEXT("An explanation is optional for your seat and goal.")},
	{TEXT("phase2.a2.model.action.explain_owed"), TEXT("Your seat or goal owes the group an explanation.")},
	{TEXT("phase2.a2.model.action.explain_toggle"), TEXT("Explain it to the group as well")},
	{TEXT("phase2.a2.model.action.investigate.accept_readings"), TEXT("Accept the readings as recorded")},
	{TEXT("phase2.a2.model.action.investigate.check_times"), TEXT("Check the timing and intervals")},
	{TEXT("phase2.a2.model.action.investigate.compare_markings"), TEXT("Compare the gauge scale with the vessel markings")},
	{TEXT("phase2.a2.model.action.investigate.look_for_flow"), TEXT("Look for pipes, valves or flow nobody counted")},
	{TEXT("phase2.a2.model.action.submit_claim"), TEXT("Present the claim to the class")},
	{TEXT("phase2.a2.model.action.worked_example"), TEXT("Study a worked example")},
	{TEXT("phase2.a2.model.beat.build_model"), TEXT("Step 1: build the model")},
	{TEXT("phase2.a2.model.beat.present_claim"), TEXT("Step 3: present your claim")},
	{TEXT("phase2.a2.model.beat.resolve_discrepancy"), TEXT("Step 2: settle the discrepancy")},
	{TEXT("phase2.a2.model.eyebrow"), TEXT("CLASS XI - MODELLING A SYSTEM")},
	{TEXT("phase2.a2.model.fail.unclear"), TEXT("An explanation was owed to the group and was not given.")},
	{TEXT("phase2.a2.model.fail.unsupported"), TEXT("The claim was not supported by the discrepancy check.")},
	{TEXT("phase2.a2.model.fail.wasted_time"), TEXT("Working time went on a check or a hedge the situation did not need.")},
	{TEXT("phase2.a2.model.fail.wrong_model"), TEXT("The set of links was not the right model.")},
	{TEXT("phase2.a2.model.goal.explain_to_group"), TEXT("Goal: explain it to the group")},
	{TEXT("phase2.a2.model.goal.test_measurements"), TEXT("Goal: test the measurements")},
	{TEXT("phase2.a2.model.goal.understand_model"), TEXT("Goal: understand the model")},
	{TEXT("phase2.a2.model.investigate_hint"), TEXT("Only one check fits before the group must report. Each shows its cost.")},
	{TEXT("phase2.a2.model.link"), TEXT("{0} \u2192 {1}")},
	{TEXT("phase2.a2.model.links_hint"), TEXT("Choose the links that belong in your model. The lesson will say afterwards how the set did.")},
	{TEXT("phase2.a2.model.method.example_taken"), TEXT("You have seen a worked example.")},
	{TEXT("phase2.a2.model.method.known"), TEXT("You know this method.")},
	{TEXT("phase2.a2.model.method.unfamiliar"), TEXT("This method is unfamiliar. A worked example costs time, as does working it out yourself.")},
	{TEXT("phase2.a2.model.outcome.extra_links"), TEXT("Links you chose that did not belong: {0}")},
	{TEXT("phase2.a2.model.outcome.missing_links"), TEXT("Links that belonged in the model and were left out: {0}")},
	{TEXT("phase2.a2.model.quantities_heading"), TEXT("Quantities in this problem")},
	{TEXT("phase2.a2.model.quantity.given"), TEXT("Given")},
	{TEXT("phase2.a2.model.quantity.other"), TEXT("A quantity")},
	{TEXT("phase2.a2.model.quantity.unknown"), TEXT("To find")},
	{TEXT("phase2.a2.model.role.critic"), TEXT("Your seat: critic")},
	{TEXT("phase2.a2.model.role.experimenter"), TEXT("Your seat: experimenter")},
	{TEXT("phase2.a2.model.role.presenter"), TEXT("Your seat: presenter")},
	{TEXT("phase2.a2.model.role.recorder"), TEXT("Your seat: recorder")},
	{TEXT("phase2.a2.model.role.solver"), TEXT("Your seat: solver")},
	{TEXT("phase2.a2.model.scarce.equipment"), TEXT("Short of: shared apparatus")},
	{TEXT("phase2.a2.model.scarce.help"), TEXT("Short of: teacher guidance")},
	{TEXT("phase2.a2.model.scarce.readings"), TEXT("Short of: repeat readings")},
	{TEXT("phase2.a2.model.scarce.time"), TEXT("Short of: working time")},
	{TEXT("phase2.a2.model.status.choose_claim"), TEXT("Choose a claim first.")},
	{TEXT("phase2.a2.model.status.choose_investigation"), TEXT("Choose a check first.")},
	{TEXT("phase2.a2.model.status.claim"), TEXT("Claim presented.")},
	{TEXT("phase2.a2.model.status.example"), TEXT("You studied the worked example.")},
	{TEXT("phase2.a2.model.status.investigation"), TEXT("Check made.")},
	{TEXT("phase2.a2.model.status.model"), TEXT("Model committed.")},
	{TEXT("phase2.a2.model.subtitle"), TEXT("Relate the quantities, settle a measurement discrepancy, then present a claim your group can follow.")},
	{TEXT("phase2.a2.model.title"), TEXT("Model and discrepancy lesson")},
	{TEXT("phase2.a2.model.unknown.inflow"), TEXT("Unknown to find: the rate in")},
	{TEXT("phase2.a2.model.unknown.outflow"), TEXT("Unknown to find: the rate out")},
	{TEXT("phase2.a2.model.unknown.stock"), TEXT("Unknown to find: the amount held (stock)")},
	{TEXT("phase2.a2.model.unknown.time"), TEXT("Unknown to find: the time taken")},
	{TEXT("school.session.already_resolved"), TEXT("The lesson is already resolved and can only be closed.")},
	{TEXT("school.session.bad_binding"), TEXT("This lesson is not set up correctly for this period.")},
	{TEXT("school.session.bad_elapsed"), TEXT("The time reported for closing the period is not valid.")},
	{TEXT("school.session.bad_mode"), TEXT("That way of taking the period is not available.")},
	{TEXT("school.session.busy"), TEXT("You are busy with something else right now.")},
	{TEXT("school.session.closed_day"), TEXT("There is no school today.")},
	{TEXT("school.session.foreground_limit"), TEXT("You have already played two foreground lessons today.")},
	{TEXT("school.session.history_full"), TEXT("The school record cannot take another period.")},
	{TEXT("school.session.invalid_state"), TEXT("The school record is not in a usable state.")},
	{TEXT("school.session.missing_catalogue"), TEXT("The school timetable could not be read.")},
	{TEXT("school.session.not_at_bell"), TEXT("Lessons begin at the bell for the period.")},
	{TEXT("school.session.not_attending"), TEXT("You were not recorded as attending this period.")},
	{TEXT("school.session.not_secure"), TEXT("Routine lessons need a secure method in this subject.")},
	{TEXT("school.session.ok"), TEXT("Ready.")},
	{TEXT("school.session.period_content_mismatch"), TEXT("This lesson does not match the scheduled period.")},
	{TEXT("school.session.period_used"), TEXT("This period has already been taken today.")},
	{TEXT("school.session.ready_to_close"), TEXT("The lesson is finished. Close the period.")},
	{TEXT("school.session.stale_session"), TEXT("This lesson screen is out of date. Return to the classroom and reopen it.")},
	{TEXT("school.session.unfinished"), TEXT("Finish every step of the lesson before closing it, or cancel it.")},
	{TEXT("school.session.unknown_family"), TEXT("This lesson's material cannot be found.")},
	{TEXT("school.session.wrong_action"), TEXT("That choice does not belong to this kind of lesson.")},
	{TEXT("school.session.wrong_goal"), TEXT("That goal does not belong to this kind of lesson.")},
};
const int32 GA2EntryCount = UE_ARRAY_COUNT(GA2Entries);
} // namespace
