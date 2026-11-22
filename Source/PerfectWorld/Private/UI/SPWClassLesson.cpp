#include "UI/SPWClassLesson.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// The shared lesson shell: construction, refreshing, focus and keys. The rows and panes are in SPWClassLessonBody.cpp; the commands are in
// SPWClassLessonCommands.cpp.
// Which idea came from which order is in Docs/Phase2/Handoffs/Lessons.md.

namespace
{
bool ClSIsBack(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right;
}

bool ClSModeAvailable(const PerfectWorld::SchoolPeriodView& Period, PerfectWorld::SchoolLessonMode Mode)
{
	switch (Mode)
	{
	case PerfectWorld::SchoolLessonMode::Foreground: return Period.ForegroundAvailable;
	case PerfectWorld::SchoolLessonMode::Routine: return Period.RoutineAvailable;
	case PerfectWorld::SchoolLessonMode::Attend: return Period.AttendAvailable;
	}
	return false;
}
} // namespace

// ---- Construction ---------------------------------------------------------------------------------------------------------

FVector2D SPWClassLesson::GetSpace() const
{
	return Space.Get(FVector2D(1920.0, 1080.0));
}

bool SPWClassLesson::IsCompactNow() const
{
	return PWClassLesson::Compact(GetSpace());
}

bool SPWClassLesson::IsWrongScreen() const
{
	return Bridge.IsValid() && Bridge->GetBinding().Structure != ShellStructure();
}

bool SPWClassLesson::WantsSplit() const
{
	return Bridge.IsValid() && !bLessonClosed && !IsWrongScreen() && Frame.bHasRun && !Frame.bOtherLesson && !Frame.bReadyToClose &&
		PWClassLesson::TwoPane(GetSpace());
}

FText SPWClassLesson::PeriodLine() const
{
	const PerfectWorld::SchoolPeriodView& Period = Frame.Period;
	if (Period.Period < 0 || Period.EndMinute <= Period.StartMinute)
	{
		return FText::GetEmpty();
	}
	FText Line = PWClassLesson::Text3(TEXT("phase2.a3.common.period_line"), PWClassLesson::Number(Period.Period + 1),
		PWClassLesson::ClockText(Period.StartMinute), PWClassLesson::ClockText(Period.EndMinute));
	if (!Period.Room.empty())
	{
		Line = FText::Format(INVTEXT("{0}  -  {1}"), Line,
			PWClassLesson::Text1(TEXT("phase2.a3.common.room_line"), PWClassLesson::FromUtf8(Period.Room)));
	}
	return Line;
}

void SPWClassLesson::ConstructShell(const PWClassLesson::FShellArgs& Args)
{
	Bridge = Args.Bridge;
	Space = Args.Space;
	OnClose = Args.OnClose;
	OnLessonClosed = Args.OnLessonClosed;
	ReturnFocus = Args.ReturnFocus;
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const FString Prefix(ShellTextPrefix());

	CaseBox = SNew(SVerticalBox);
	PlayBox = SNew(SVerticalBox);

	// Back and Leave early sit outside the scroll area, so neither can leave the screen at 853x480 or 150% text.
	const TSharedRef<SWidget> Back = PWUI::MakeButton(
		PWClassLesson::Text(TEXT("phase2.a3.common.back")), PWUI::EButtonKind::Secondary,
		FOnClicked::CreateSPLambda(this, [this]()
		{
			RequestClose();
			return FReply::Handled();
		}), true, 20, 0.0f);
	BackButton = Back;

	const TSharedRef<SWidget> Leave = PWUI::MakeButton(
		TAttribute<FText>::CreateSPLambda(this, [this]()
		{
			return PWClassLesson::Text(bLeaveArmed ? TEXT("phase2.a2.common.confirm_cancel") : TEXT("phase2.a3.common.leave"));
		}),
		PWUI::EButtonKind::Secondary,
		FOnClicked::CreateSPLambda(this, [this]()
		{
			PressLeaveEarly();
			return FReply::Handled();
		}),
		TAttribute<bool>::CreateSPLambda(this, [this]() { return CanLeaveEarly(); }), 18, 0.0f);

	const TSharedRef<SVerticalBox> Header =
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsCompactNow() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				PWUI::MakeText(PWClassLesson::Text(Prefix + TEXT("eyebrow")), PWUI::ETextRole::Meta, Style.Accent, true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(PWClassLesson::Text(Prefix + TEXT("title")))
			.Font_Lambda([this]() { return PWUI::Font(IsCompactNow() ? 22 : 30, true); })
			.ColorAndOpacity(FSlateColor(Style.TextPrimary))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return IsCompactNow() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
			[
				PWUI::MakeText(PWClassLesson::Text(Prefix + TEXT("subtitle")), 16, Style.TextSecondary, false, true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(TAttribute<FText>::CreateSPLambda(this, [this]() { return PeriodLine(); }), PWUI::ETextRole::Meta, Style.TextSecondary, true)
		];

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride_Lambda([this]() { return FOptionalSize(PWClassLesson::PanelWidth(GetSpace())); })
		.MaxDesiredHeight_Lambda([this]() { return FOptionalSize(PWClassLesson::PanelMaxHeight(GetSpace())); })
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
					.Padding_Lambda([this]() { return IsCompactNow() ? FMargin(16.0f, 8.0f) : FMargin(36.0f, 24.0f); })
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()
						[
							Header
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)
						[
							PWUI::MakeRule()
						]
						+ SVerticalBox::Slot().FillHeight(1.0f)
						[
							SAssignNew(BodyHolder, SBox)
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(SBorder)
							.Tag(FName(PWClassLesson::BannerTag))
							.BorderImage(&Style.BrushInset)
							.Padding(FMargin(12.0f, 6.0f))
							.Visibility_Lambda([this]() { return Banner.IsEmpty() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible; })
							[
								SNew(STextBlock)
								.Text_Lambda([this]() { return Banner; })
								.Font_Lambda([this]() { return PWUI::Font(IsCompactNow() ? 14 : 16, PWUI::EFontWeight::Medium); })
								.ColorAndOpacity_Lambda([this]() { return FSlateColor(bBannerError ? PWUI::FStyle::Get().Danger : PWUI::FStyle::Get().TextPrimary); })
								.AutoWrapText(true)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
						[
							SNew(SBox)
							.Visibility_Lambda([this]() { return IsCompactNow() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
							[
								PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a3.common.hints")), PWUI::ETextRole::Meta, Style.TextSecondary, true)
							]
						]
						+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 12.0f, 0.0f)
							[
								SNew(SBox)
								.Visibility_Lambda([this]() { return CanLeaveEarly() ? EVisibility::Visible : EVisibility::Collapsed; })
								[
									Leave
								]
							]
							+ SHorizontalBox::Slot().FillWidth(1.0f)
							[
								Back
							]
						]
					]
				]
			]
		]
	];

	RefreshModel();
	bStarted = true;
}

// ---- Refreshing and focus ---------------------------------------------------------------------------------------------------

void SPWClassLesson::ClampSelections()
{
	GoalIndex = FMath::Clamp(GoalIndex, 0, FMath::Max(0, ShellGoalCount() - 1));
	if (Frame.bHasRun || ClSModeAvailable(Frame.Period, Mode))
	{
		return;
	}
	for (const PerfectWorld::SchoolLessonMode Candidate : {PerfectWorld::SchoolLessonMode::Foreground, PerfectWorld::SchoolLessonMode::Routine,
		PerfectWorld::SchoolLessonMode::Attend})
	{
		if (ClSModeAvailable(Frame.Period, Candidate))
		{
			Mode = Candidate;
			return;
		}
	}
}

int32 SPWClassLesson::FocusedActionIndex() const
{
	for (int32 Index = 0; Index < Actions.Num(); ++Index)
	{
		const TSharedPtr<SWidget>& Widget = Actions[Index].Widget;
		if (Widget.IsValid() && (Widget->HasKeyboardFocus() || Widget->HasFocusedDescendants()))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

void SPWClassLesson::RefreshModel()
{
	const bool bHadFocus = bWantFocus || HasFocusedDescendants();
	const int32 PreviousIndex = FocusedActionIndex();
	if (Bridge.IsValid())
	{
		Frame = PWClassLesson::BuildFrame(*Bridge);
		bool bActive = false;
		int32 Left = 0;
		int32 Total = 0;
		ShellReadExposure(Frame.View, bActive, Left, Total);
		Frame.bExposureActive = bActive && Frame.bHasRun && !Frame.bOtherLesson;
		Frame.UnitsLeft = Frame.bExposureActive ? Left : 0;
		Frame.UnitsTotal = Frame.bExposureActive ? Total : 0;
	}
	else
	{
		Frame = PWClassLesson::FFrame();
	}
	const uint64 Session = Frame.bHasRun ? Frame.Session : 0;
	if (Session != DisplayedSession)
	{
		// The first sight of a lesson that was already open: this screen resumes it. Its selections start empty; everything chosen so far
		// is in the core's exposure and is shown.
		const bool bResumed = !bStarted && DisplayedSession == 0 && Session != 0 && !Frame.bOtherLesson && !IsWrongScreen();
		DisplayedSession = Session;
		bLeaveArmed = false;
		ShellSessionChanged();
		if (bResumed)
		{
			ShowBanner(PWClassLesson::Text(TEXT("phase2.a2.common.resumed")), false);
		}
	}
	ClampSelections();
	ShellFrameChanged();
	RebuildBody();
	RestoreFocusAfterRebuild(PreviousIndex, bHadFocus);
}

bool SPWClassLesson::RefreshLayout()
{
	if (IsCompactNow() == bBuiltCompact && WantsSplit() == bBuiltSplit)
	{
		return false;
	}
	RefreshModel();
	return true;
}

void SPWClassLesson::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	// A resize or a text-scale change crossing a density or pane line rebuilds the body in the other arrangement.
	RefreshLayout();
}

void SPWClassLesson::RestoreFocusAfterRebuild(int32 PreviousIndex, bool bHadFocus)
{
	if (!bHadFocus)
	{
		return;
	}
	bWantFocus = false;
	// The nearest enabled action at or after the old position, else the nearest before it, else Back.
	TWeakPtr<SWidget> Target = BackButton;
	const int32 Start = FMath::Clamp(PreviousIndex == INDEX_NONE ? 0 : PreviousIndex, 0, FMath::Max(0, Actions.Num() - 1));
	bool bFound = false;
	for (int32 Index = Start; Index < Actions.Num() && !bFound; ++Index)
	{
		if (Actions[Index].bEnabled)
		{
			Target = Actions[Index].Widget;
			bFound = true;
		}
	}
	for (int32 Index = Start - 1; Index >= 0 && !bFound; --Index)
	{
		if (Actions[Index].bEnabled)
		{
			Target = Actions[Index].Widget;
			bFound = true;
		}
	}
	PWUI::FocusWhenReady(AsShared(), Target);
}

void SPWClassLesson::FocusInitial()
{
	bWantFocus = true;
	RestoreFocusAfterRebuild(INDEX_NONE, true);
}

void SPWClassLesson::OnFocusChanging(const FWeakWidgetPath& PreviousFocusPath, const FWidgetPath& NewWidgetPath, const FFocusEvent& InFocusEvent)
{
	SCompoundWidget::OnFocusChanging(PreviousFocusPath, NewWidgetPath, InFocusEvent);
	if (!PlayScroll.IsValid())
	{
		return;
	}
	for (int32 PathIndex = NewWidgetPath.Widgets.Num() - 1; PathIndex >= 0; --PathIndex)
	{
		const TSharedRef<SWidget>& Widget = NewWidgetPath.Widgets[PathIndex].Widget;
		for (const FActionSlot& Slot : Actions)
		{
			if (Slot.Widget.Get() == &Widget.Get())
			{
				PlayScroll->ScrollDescendantIntoView(Slot.Widget, false);
				return;
			}
		}
	}
}

// ---- Keys -------------------------------------------------------------------------------------------------------------------

void SPWClassLesson::ScrollBy(float Delta)
{
	if (PlayScroll.IsValid())
	{
		PlayScroll->SetScrollOffset(FMath::Clamp(PlayScroll->GetScrollOffset() + Delta, 0.0f, PlayScroll->GetScrollOffsetOfEnd()));
	}
}

FReply SPWClassLesson::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	constexpr float KeyStep = 72.0f;
	const FKey Key = InKeyEvent.GetKey();
	if (ClSIsBack(Key))
	{
		// A held key auto-repeats; this screen goes back on the first press only.
		if (!InKeyEvent.IsRepeat())
		{
			RequestClose();
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
	const bool bIdle = Bridge.IsValid() && !Frame.bHasRun && !bLessonClosed;
	if (bIdle && (Key == EKeys::G || Key == EKeys::Gamepad_RightShoulder))
	{
		if (!InKeyEvent.IsRepeat())
		{
			CycleGoal(1);
		}
		return FReply::Handled();
	}
	if (bIdle && (Key == EKeys::M || Key == EKeys::Gamepad_LeftShoulder))
	{
		if (!InKeyEvent.IsRepeat())
		{
			CycleMode(1);
		}
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SPWClassLesson::RequestClose()
{
	// Leaving only leaves the screen: the lesson stays open in the core and a new screen on the same world resumes it. The host may drop
	// its last reference to this screen from inside OnClose.
	const TSharedRef<SWidget> KeepAlive = AsShared();
	PWUI::CancelPendingFocus(KeepAlive);
	const TWeakPtr<SWidget> Return = ReturnFocus;
	const FSimpleDelegate Closed = OnClose;
	Closed.ExecuteIfBound();
	const TSharedPtr<SWidget> Target = Return.Pin();
	if (Target.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetKeyboardFocus(Target, EFocusCause::SetDirectly);
	}
}

void SPWClassLesson::ShowBanner(const FText& Text, bool bError)
{
	Banner = Text;
	bBannerError = bError;
}

// ---- Mode and goal --------------------------------------------------------------------------------------------------------

bool SPWClassLesson::SetMode(PerfectWorld::SchoolLessonMode InMode)
{
	if (Frame.bHasRun || bLessonClosed || InMode == Mode || !ClSModeAvailable(Frame.Period, InMode))
	{
		return false;
	}
	Mode = InMode;
	RefreshModel();
	return true;
}

bool SPWClassLesson::SetGoalIndex(int32 Index)
{
	if (Frame.bHasRun || bLessonClosed || Index == GoalIndex || Index < 0 || Index >= ShellGoalCount())
	{
		return false;
	}
	GoalIndex = Index;
	RefreshModel();
	return true;
}

void SPWClassLesson::CycleMode(int32 Delta)
{
	const PerfectWorld::SchoolLessonMode Order[3] = {PerfectWorld::SchoolLessonMode::Foreground, PerfectWorld::SchoolLessonMode::Routine,
		PerfectWorld::SchoolLessonMode::Attend};
	int32 Position = 0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (Order[Index] == Mode)
		{
			Position = Index;
		}
	}
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		const int32 Next = ((Position + Delta * Step) % 3 + 3) % 3;
		if (SetMode(Order[Next]) || Order[Next] == Mode)
		{
			return;
		}
	}
}

void SPWClassLesson::CycleGoal(int32 Delta)
{
	const int32 Count = ShellGoalCount();
	if (Count > 0 && Mode == PerfectWorld::SchoolLessonMode::Foreground)
	{
		SetGoalIndex(((GoalIndex + Delta) % Count + Count) % Count);
	}
}
