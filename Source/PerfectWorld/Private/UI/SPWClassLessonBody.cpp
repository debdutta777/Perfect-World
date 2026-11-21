#include "UI/SPWClassLesson.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

// The shared lesson shell, part 2: the rows and the panes (idle, playing, finished, closed). Nothing here decides anything.

namespace
{
bool ClBModeAvailable(const PerfectWorld::SchoolPeriodView& Period, PerfectWorld::SchoolLessonMode Mode)
{
	switch (Mode)
	{
	case PerfectWorld::SchoolLessonMode::Foreground: return Period.ForegroundAvailable;
	case PerfectWorld::SchoolLessonMode::Routine: return Period.RoutineAvailable;
	case PerfectWorld::SchoolLessonMode::Attend: return Period.AttendAvailable;
	}
	return false;
}

bool ClBIsUnitsShort(const std::string& Code)
{
	return Code == "structure.units_short" || Code == "model.units_short";
}

FLinearColor ClBGradeColor(PerfectWorld::StructureGrade Grade)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	switch (Grade)
	{
	case PerfectWorld::StructureGrade::Sound: return Style.Good;
	case PerfectWorld::StructureGrade::Partial: return Style.Warn;
	case PerfectWorld::StructureGrade::Unsound: break;
	}
	return Style.Danger;
}

/** A quiet inset note, tagged for the tests. */
TSharedRef<SWidget> ClBNote(const FText& Text, const TCHAR* Tag, bool bMuted, int32 Size)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	return SNew(SBorder)
		.Tag(FName(Tag))
		.BorderImage(&Style.BrushInset)
		.Padding(FMargin(12.0f, 8.0f))
		[
			PWUI::MakeText(Text, Size, bMuted ? Style.TextSecondary : Style.TextPrimary, false, true)
		];
}

/** The grade, detail, diagnostics, units, learning and mastery lines of one outcome. Every word is the core's typed outcome. */
TSharedRef<SWidget> ClBOutcomeBlock(const PWClassLesson::FOutcomeFacts& Facts, const TCHAR* Tag)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[PWUI::MakeText(PWClassLesson::GradeText(Facts.Grade), 22, ClBGradeColor(Facts.Grade), true, true)];
	for (const FText& Line : Facts.Detail)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)[PWUI::MakeText(Line, 17, Style.TextPrimary, false, true)];
	}
	if (Facts.Diagnostics.IsEmpty())
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a2.common.outcome.no_diagnostics")), 17, Style.TextSecondary, false, true)
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
			PWUI::MakeText(PWClassLesson::Text1(TEXT("phase2.a2.common.units_spent"), PWClassLesson::Number(Facts.UnitsSpent)), 16, Style.TextSecondary, false, true)
		];
	}
	Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
	[
		PWUI::MakeText(PWClassLesson::Text1(TEXT("phase2.a2.common.outcome.learning_permille"), PWClassLesson::Number(Facts.LearningPermille)), 16, Style.TextSecondary, false, true)
	];
	if (Facts.bBecameSecure)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a2.common.outcome.became_secure")), 17, Style.Good, true, true)
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
} // namespace

// ---- Rows -------------------------------------------------------------------------------------------------------------------

void SPWClassLesson::AddActionRow(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Title, const FText& Detail,
	const FText& Cost, ECostTone Tone, bool bEnabled, TFunction<void()> Handler, bool bSelected)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const FLinearColor CostColour = Tone == ECostTone::Free ? Style.Good : (Tone == ECostTone::Short ? Style.Danger : Style.TextSecondary);
	const TSharedRef<SVerticalBox> Words = SNew(SVerticalBox);
	Words->AddSlot().AutoHeight()
	[
		PWUI::MakeText(Title, TextSize(18, 16), Style.TextPrimary, true, true)
	];
	if (!Detail.IsEmpty())
	{
		Words->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(Detail, PWUI::ETextRole::Meta, Style.TextSecondary, true)
		];
	}
	const TSharedRef<SHorizontalBox> Content = SNew(SHorizontalBox);
	Content->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)
	[
		Words
	];
	if (!Cost.IsEmpty())
	{
		Content->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(Cost, PWUI::ETextRole::Control, CostColour, true)
		];
	}
	const TSharedRef<SWidget> Button = PWUI::MakeContentButton(
		Content, PWUI::EButtonKind::Card,
		FOnClicked::CreateSPLambda(this, [this, Key]()
		{
			PressAction(Key);
			return FReply::Handled();
		}),
		bSelected, bEnabled, IsCompactNow() ? FMargin(10.0f, 6.0f) : FMargin(14.0f, 10.0f));
	Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		SNew(SBox).Tag(FName(PWClassLesson::RowTag))[Button]
	];
	FActionSlot Slot;
	Slot.Key = Key;
	Slot.Widget = Button;
	Slot.bEnabled = bEnabled;
	Slot.Handler = MoveTemp(Handler);
	Actions.Add(MoveTemp(Slot));
}

void SPWClassLesson::AddQuotedRow(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Title, const FText& Detail,
	const PerfectWorld::SchoolLessonQuote& Quote, TFunction<void()> Handler, bool bSelected, bool bReady, const FText& NotReadyHint)
{
	if (!bReady)
	{
		AddActionRow(Into, Key, Title, NotReadyHint.IsEmpty() ? Detail : NotReadyHint, FText::GetEmpty(), ECostTone::Plain, false, MoveTemp(Handler), bSelected);
		return;
	}
	const bool bOk = Quote.Issue.Ok();
	FText Shown = Detail;
	if (!bOk)
	{
		// A refusal is shown in words chosen from the core's code, next to the price when there is one.
		const FText Why = PWClassLesson::IssueText(Quote.Issue);
		Shown = Detail.IsEmpty() ? Why : FText::Format(INVTEXT("{0} {1}"), Detail, Why);
	}
	const bool bShort = !bOk && ClBIsUnitsShort(Quote.Issue.Code);
	const ECostTone Tone = bShort ? ECostTone::Short : (bOk && Quote.Units == 0 ? ECostTone::Free : ECostTone::Plain);
	AddActionRow(Into, Key, Title, Shown, PWClassLesson::PriceText(Quote, Frame.UnitsLeft), Tone, bOk, MoveTemp(Handler), bSelected);
}

void SPWClassLesson::AddPrimaryAction(const TSharedRef<SVerticalBox>& Into, const FString& Key, const FText& Label, TFunction<void()> Handler)
{
	const TSharedRef<SWidget> Button = PWUI::MakeButton(
		Label, PWUI::EButtonKind::Primary,
		FOnClicked::CreateSPLambda(this, [this, Key]()
		{
			PressAction(Key);
			return FReply::Handled();
		}), true, TextSize(20, 18), 0.0f);
	Into->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 6.0f)
	[
		Button
	];
	FActionSlot Slot;
	Slot.Key = Key;
	Slot.Widget = Button;
	Slot.bEnabled = true;
	Slot.Handler = MoveTemp(Handler);
	Actions.Add(MoveTemp(Slot));
}

void SPWClassLesson::AddHeading(const TSharedRef<SVerticalBox>& Into, const FText& Text)
{
	Into->AddSlot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 6.0f)
	[
		PWUI::MakeText(Text, PWUI::ETextRole::SectionHeading, PWUI::FStyle::Get().Accent, true)
	];
}

void SPWClassLesson::AddNote(const TSharedRef<SVerticalBox>& Into, const FText& Text, bool bMuted)
{
	if (Text.IsEmpty())
	{
		return;
	}
	Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
	[
		ClBNote(Text, PWClassLesson::NoteTag, bMuted, TextSize(17, 15))
	];
}

void SPWClassLesson::AddLine(const TSharedRef<SVerticalBox>& Into, const FText& Text, const FLinearColor& Color, bool bBold)
{
	if (Text.IsEmpty())
	{
		return;
	}
	Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)
	[
		PWUI::MakeText(Text, TextSize(17, 15), Color, bBold, true)
	];
}

void SPWClassLesson::AddUnitsMeter(const TSharedRef<SVerticalBox>& Into)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const TSharedRef<SHorizontalBox> Pips = SNew(SHorizontalBox);
	for (int32 Pip = 0; Pip < 16; ++Pip)
	{
		Pips->AddSlot().AutoWidth().Padding(0.0f, 0.0f, 4.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(18.0f)
			.HeightOverride(8.0f)
			.Visibility_Lambda([this, Pip]() { return Pip < Frame.UnitsTotal ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SImage)
				.Image_Lambda([this, Pip]() -> const FSlateBrush*
				{
					return Pip < Frame.UnitsLeft ? &PWUI::FStyle::Get().BrushPipOn : &PWUI::FStyle::Get().BrushPipOff;
				})
			]
		];
	}
	Into->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 6.0f)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			PWUI::MakeText(PWClassLesson::UnitsText(Frame.UnitsLeft, Frame.UnitsTotal), TextSize(17, 15), Style.TextPrimary, true, true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			Pips
		]
	];
}

// ---- Panes ------------------------------------------------------------------------------------------------------------------

FText SPWClassLesson::FamilyName() const
{
	if (Bridge.IsValid())
	{
		const PerfectWorld::SchoolLessonBinding& Binding = Bridge->GetBinding();
		const PerfectWorld::AssessmentStructures& Structures = Bridge->GetCatalogues().Structures;
		if (Binding.Structure == PerfectWorld::SchoolStructure::Model && Structures.Model != nullptr)
		{
			if (const auto* Family = Structures.Model->FindFamily(Binding.FamilyId))
			{
				return PWClassLesson::FromUtf8(Family->Name);
			}
		}
		if (Binding.Structure == PerfectWorld::SchoolStructure::Explain && Structures.Explain != nullptr)
		{
			if (const auto* Family = Structures.Explain->FindFamily(Binding.FamilyId))
			{
				return PWClassLesson::FromUtf8(Family->Name);
			}
		}
		if (Binding.Structure == PerfectWorld::SchoolStructure::Investigate && Structures.Investigate != nullptr)
		{
			if (const auto* Family = Structures.Investigate->FindFamily(Binding.FamilyId))
			{
				return PWClassLesson::FromUtf8(Family->Name);
			}
		}
		if (Binding.Structure == PerfectWorld::SchoolStructure::Coordinate && Structures.Coordinate != nullptr)
		{
			if (const auto* Family = Structures.Coordinate->FindFamily(Binding.FamilyId))
			{
				return PWClassLesson::FromUtf8(Family->Name);
			}
		}
	}
	return PWClassLesson::Text(TEXT("phase2.a3.common.lesson"));
}

void SPWClassLesson::BuildIntro(const TSharedRef<SVerticalBox>& Into)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	// The binding's title (A1's text key), else the family's own name.
	AddLine(Into, Frame.Title.IsEmpty() ? FamilyName() : Frame.Title, Style.TextPrimary, true);
	AddNote(Into, Frame.Brief, false);
	if (!Frame.Role.IsEmpty())
	{
		AddLine(Into, PWClassLesson::Text1(TEXT("phase2.a3.common.role_line"), Frame.Role), Style.TextSecondary);
	}
	const PWClassLesson::EMastery Mastery = ShellMastery(Frame.View, Frame.Binding.FamilyId);
	const TCHAR* MasteryKey = Mastery == PWClassLesson::EMastery::Secure ? TEXT("phase2.a2.common.mastery.secure")
		: Mastery == PWClassLesson::EMastery::Practised ? TEXT("phase2.a2.common.mastery.practised")
		: TEXT("phase2.a2.common.mastery.unseen");
	AddLine(Into, PWClassLesson::Text(MasteryKey), Style.Accent, true);
	AddLine(Into, PWClassLesson::Text2(TEXT("phase2.a2.common.foreground_used"), PWClassLesson::Number(Frame.Period.ForegroundUsed),
		PWClassLesson::Number(PerfectWorld::SchoolForegroundLimit)), Style.TextSecondary);
	if (!Frame.Period.Issue.Ok())
	{
		// Why this period cannot be taken: the core's own code, in words.
		AddNote(Into, PWClassLesson::IssueText(Frame.Period.Issue), false);
	}
}

void SPWClassLesson::BuildIdle(const TSharedRef<SVerticalBox>& Into)
{
	BuildIntro(Into);
	if (!Frame.Period.AttendAvailable)
	{
		return;
	}
	AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.common.mode_heading")));
	const struct
	{
		PerfectWorld::SchoolLessonMode Mode;
		const TCHAR* Key;
		const char* Reason;
	} Modes[] = {
		{PerfectWorld::SchoolLessonMode::Foreground, TEXT("mode.foreground"), "school.session.foreground_limit"},
		{PerfectWorld::SchoolLessonMode::Routine, TEXT("mode.routine"), "school.session.not_secure"},
		{PerfectWorld::SchoolLessonMode::Attend, TEXT("mode.attend"), "school.session.ok"}};
	for (const auto& Entry : Modes)
	{
		const bool bAvailable = ClBModeAvailable(Frame.Period, Entry.Mode);
		const PerfectWorld::SchoolLessonMode Picked = Entry.Mode;
		AddActionRow(Into, Entry.Key, PWClassLesson::ModeText(Entry.Mode),
			bAvailable ? PWClassLesson::ModeHint(Entry.Mode) : PWClassLesson::IssueText(std::string(Entry.Reason)), FText::GetEmpty(),
			ECostTone::Plain, bAvailable, [this, Picked]() { SetMode(Picked); }, Mode == Entry.Mode);
	}
	if (Mode == PerfectWorld::SchoolLessonMode::Foreground)
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.common.goal_heading")));
		for (int32 Index = 0; Index < ShellGoalCount(); ++Index)
		{
			AddActionRow(Into, FString::Printf(TEXT("goal.%d"), Index), ShellGoalName(Index), ShellGoalHint(Index), FText::GetEmpty(),
				ECostTone::Plain, true, [this, Index]() { SetGoalIndex(Index); }, GoalIndex == Index);
		}
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.quotes_note")));
	}
	AddPrimaryAction(Into, TEXT("open"), PWClassLesson::Text(TEXT("phase2.a3.common.start")), [this]() { ExecuteOpen(); });
}

void SPWClassLesson::BuildFinished(const TSharedRef<SVerticalBox>& Into)
{
	AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.common.outcome.heading")));
	PWClassLesson::FOutcomeFacts Facts;
	if (Frame.RunMode == PerfectWorld::SchoolLessonMode::Foreground)
	{
		if (ShellDescribeOutcome(Frame.Outcome, Facts))
		{
			Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[ClBOutcomeBlock(Facts, PWClassLesson::OutcomeTag)];
			AddNote(Into, PWClassLesson::Text(TEXT("phase2.a2.common.outcome.ready")));
		}
		else
		{
			AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.outcome.pending")));
		}
	}
	else
	{
		const TCHAR* Key = Frame.RunMode == PerfectWorld::SchoolLessonMode::Routine ? TEXT("phase2.a2.common.outcome.routine")
			: TEXT("phase2.a2.common.outcome.attend");
		Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[ClBNote(PWClassLesson::Text(Key), PWClassLesson::OutcomeTag, true, TextSize(17, 15))];
	}
	AddPrimaryAction(Into, TEXT("record"), PWClassLesson::Text(TEXT("phase2.a2.common.complete_lesson")), [this]() { ExecuteClose(false); });
}

void SPWClassLesson::BuildSummary(const TSharedRef<SVerticalBox>& Into)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		PWUI::MakeText(PWClassLesson::Text(bClosedByLeaving ? TEXT("phase2.a2.common.summary.cancelled") : TEXT("phase2.a2.common.summary.closed")),
			PWUI::ETextRole::SectionHeading, Style.Accent, true)
	];
	PWClassLesson::FOutcomeFacts Facts;
	if (!bClosedByLeaving && ClosedMode == PerfectWorld::SchoolLessonMode::Foreground && ShellDescribeOutcome(ClosedOutcome, Facts))
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[ClBOutcomeBlock(Facts, PWClassLesson::OutcomeTag)];
	}
	else if (bClosedByLeaving)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			ClBNote(PWClassLesson::Text(TEXT("phase2.a2.common.summary.cancelled.detail")), PWClassLesson::NoteTag, true, 16)
		];
	}
	else
	{
		const TCHAR* Key = ClosedMode == PerfectWorld::SchoolLessonMode::Routine ? TEXT("phase2.a2.common.summary.routine")
			: TEXT("phase2.a2.common.summary.attend");
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)[ClBNote(PWClassLesson::Text(Key), PWClassLesson::OutcomeTag, true, 16)];
	}
	Box->AddSlot().AutoHeight()
	[
		PWUI::MakeText(PWClassLesson::Text1(TEXT("phase2.a2.common.summary.minutes"), PWClassLesson::Number(ClosedMinutes)), 16, Style.TextSecondary, false, true)
	];
	if (!bClosedByLeaving)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(PWClassLesson::Text1(TEXT("phase2.a2.common.summary.learning"), PWClassLesson::Number(ClosedMilliXp)), 16, Style.TextSecondary, false, true)
		];
	}
	Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		SNew(SBorder)
		.Tag(FName(PWClassLesson::SummaryTag))
		.BorderImage(&Style.BrushInset)
		.Padding(FMargin(14.0f, 10.0f))
		[
			Box
		]
	];
	AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.closed_note")));
}

void SPWClassLesson::SetLayout(ELayout InLayout)
{
	if (PlayScroll.IsValid() && BuiltLayout == InLayout)
	{
		return;
	}
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	// Release the old tree first, so the persistent boxes are free to move into the new one.
	BodyHolder->SetContent(SNullWidget::NullWidget);
	BuiltLayout = InLayout;
	if (InLayout == ELayout::Split)
	{
		BodyHolder->SetContent(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.42f).Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SAssignNew(SideScroll, SScrollBox)
				.ScrollBarStyle(&Style.ScrollBar)
				.AllowOverscroll(EAllowOverscroll::No)
				+ SScrollBox::Slot().Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					CaseBox.ToSharedRef()
				]
			]
			+ SHorizontalBox::Slot().FillWidth(0.58f)
			[
				SAssignNew(PlayScroll, SScrollBox)
				.ScrollBarStyle(&Style.ScrollBar)
				.AllowOverscroll(EAllowOverscroll::No)
				+ SScrollBox::Slot().Padding(0.0f, 0.0f, 10.0f, 0.0f)
				[
					PlayBox.ToSharedRef()
				]
			]);
		return;
	}
	SideScroll.Reset();
	BodyHolder->SetContent(
		SAssignNew(PlayScroll, SScrollBox)
		.ScrollBarStyle(&Style.ScrollBar)
		.AllowOverscroll(EAllowOverscroll::No)
		+ SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				CaseBox.ToSharedRef()
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				PlayBox.ToSharedRef()
			]
		]);
}

void SPWClassLesson::RebuildBody()
{
	Actions.Reset();
	CaseBox->ClearChildren();
	PlayBox->ClearChildren();
	bBuiltCompact = IsCompactNow();
	bBuiltSplit = WantsSplit();
	SetLayout(bBuiltSplit ? ELayout::Split : ELayout::Single);

	const TSharedRef<SVerticalBox> Into = PlayBox.ToSharedRef();
	if (!Bridge.IsValid())
	{
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.unavailable")), false);
		return;
	}
	if (IsWrongScreen())
	{
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a2.common.wrong_screen")), false);
		return;
	}
	if (bLessonClosed)
	{
		BuildSummary(Into);
		return;
	}
	if (Frame.bOtherLesson)
	{
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.other_lesson")), false);
		return;
	}
	if (!Frame.bHasRun)
	{
		BuildIdle(Into);
		return;
	}
	if (Frame.bReadyToClose)
	{
		BuildFinished(Into);
		return;
	}
	ShellBuildCase(CaseBox.ToSharedRef());
	ShellBuildPlay(Into);
}
