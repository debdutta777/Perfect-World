// Copyright Perfect World. All Rights Reserved.

#include "UI/SPWAwakeningResultPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float kR97ResultScrollStep = 120.0f;
	constexpr float kR97ResultStickDeadZone = 0.35f;

	TSharedRef<SWidget> R97MakeEvidenceListWidget(
		const TArray<FText>& EvidenceLines,
		float WrapWidth)
	{
		const PWUI::FStyle& Style = PWUI::FStyle::Get();
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 2.0f)
		[
			PWUI::MakeText(
				FText::FromString(TEXT("NAMED EVIDENCE")),
				PWUI::ETextRole::Meta,
				Style.Accent)
		];

		if (EvidenceLines.Num() == 0)
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Tag(FName(PWApResult::EvidenceLineTag))
				.Font(PWUI::Font(15))
				.ColorAndOpacity(Style.TextSecondary)
				.WrapTextAt(WrapWidth)
				.Text(FText::FromString(TEXT("Recorded from baseline sixteen-year formation and school tutorial conduct.")))
			];
			return Box;
		}

		for (const FText& Line : EvidenceLines)
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Tag(FName(PWApResult::EvidenceLineTag))
				.Font(PWUI::Font(15))
				.ColorAndOpacity(Style.TextPrimary)
				.WrapTextAt(WrapWidth)
				.Text(Line)
			];
		}

		return Box;
	}
}

float SPWAwakeningResultPanel::PanelWidth(float ViewportWidth, float /*ViewportHeight*/)
{
	const float Available = FMath::Max(360.0f, ViewportWidth - 32.0f);
	return FMath::Clamp(Available, 360.0f, 1140.0f);
}

float SPWAwakeningResultPanel::PanelMaxHeight(float /*ViewportWidth*/, float ViewportHeight)
{
	const float Available = FMath::Max(300.0f, ViewportHeight - 32.0f);
	return FMath::Clamp(Available, 300.0f, 840.0f);
}

void SPWAwakeningResultPanel::Construct(const FArguments& InArgs)
{
	WorldPtr = InArgs._World;
	BridgeOverride = InArgs._BridgeOverride;
	ViewportSize = InArgs._ViewportSize;
	OnClose = InArgs._OnClose;

	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const float Width = PanelWidth(ViewportSize.X, ViewportSize.Y);
	const float MaxHeight = PanelMaxHeight(ViewportSize.X, ViewportSize.Y);
	const bool bCompact = ViewportSize.Y < 560.0f || ViewportSize.X < 960.0f;
	const float BodyScrollMaxH = FMath::Max(120.0f, MaxHeight - (bCompact ? 156.0f : 184.0f));

	ContinueButton = PWUI::MakeButton(
		FText::FromString(TEXT("Continue into the day")),
		PWUI::EButtonKind::Primary,
		FOnClicked::CreateSP(this, &SPWAwakeningResultPanel::HandleContinueClicked),
		true, 20, 0.0f);

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(Width)
		.MaxDesiredHeight(MaxHeight)
		[
			SNew(SBorder)
			.Tag(FName(PWApResult::PanelTag))
			.BorderImage(&Style.BrushPanel)
			.Padding(FMargin(bCompact ? 14.0f : 20.0f))
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SNew(SPWCornerBrackets)
				]
				+ SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, bCompact ? 6.0f : 10.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 12.0f, 0.0f)
						[
							SNew(SPWAwakeningSigil)
							.SizeClass(EPWSigilSizeClass::Seal)
							.Diameter(bCompact ? 24.0f : 32.0f)
							.Reveal(!PWUI::IsReducedMotion())
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								PWUI::MakeText(
									FText::FromString(TEXT("AWAKENING RECORD · SCHOOL TUTORIAL")),
									22,
									Style.Accent, true)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Font(PWUI::Font(15))
								.ColorAndOpacity(Style.TextSecondary)
								.WrapTextAt(Width - 110.0f)
								.Text_Lambda([this]() { return Model.SubtitleText; })
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Top)
						[
							PWUI::MakeText(
								TAttribute<FText>::CreateLambda([this]() { return Model.GrantedAtText; }),
								PWUI::ETextRole::Meta,
								Style.TextSecondary)
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, bCompact ? 6.0f : 10.0f)
					[
						PWUI::MakeRule()
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SBox)
						.MaxDesiredHeight(BodyScrollMaxH)
						[
							SAssignNew(DetailScrollBox, SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(BodyContainer, SVerticalBox)
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, bCompact ? 6.0f : 10.0f, 0.0f, 0.0f)
					[
						PWUI::MakeRule()
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, bCompact ? 6.0f : 10.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							PWUI::MakeText(
								FText::FromString(TEXT("Up/Down: inspect sections · PgUp/PgDn: scroll · Enter/Esc: continue")),
								PWUI::ETextRole::Meta,
								Style.TextSecondary)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox).Tag(FName(PWApResult::ContinueButtonTag))[ContinueButton.ToSharedRef()]
						]
					]
				]
			]
		]
	];

	RefreshFromBridge();
}

TSharedPtr<PWAwakening::IApAwakeningWorld> SPWAwakeningResultPanel::ResolveBridge() const
{
	if (BridgeOverride.IsValid())
	{
		return BridgeOverride;
	}
	return PWAwakening::ApBridge::Resolve(WorldPtr.Get());
}

void SPWAwakeningResultPanel::RefreshFromBridge()
{
	if (TSharedPtr<PWAwakening::IApAwakeningWorld> Bridge = ResolveBridge())
	{
		SetModel(Bridge->ReadResultModel());
	}
	else
	{
		PWAwakening::FPWAwakeningResultModel Unavail;
		Unavail.bAvailable = false;
		Unavail.SubtitleText = FText::FromString(TEXT("Awakening record is unavailable outside an active session."));
		SetModel(Unavail);
	}
}

void SPWAwakeningResultPanel::SetModel(const PWAwakening::FPWAwakeningResultModel& InModel)
{
	Model = InModel;
	SelectedSlotIndex = 0;
	RebuildBody();
}

TSharedRef<SWidget> SPWAwakeningResultPanel::AddFocusRow(const TSharedRef<SWidget>& Content, const FName RowTag)
{
	const int32 SlotIdx = FocusableRowButtons.Num();
	const TSharedRef<SWidget> Button = PWUI::MakeContentButton(
		Content,
		PWUI::EButtonKind::TextRow,
		FOnClicked::CreateSPLambda(this, [this, SlotIdx]()
		{
			SelectFocusSlot(SlotIdx);
			return FReply::Handled();
		}),
		false,
		true,
		FMargin(12.0f, 8.0f));
	FocusableRowButtons.Add(Button);
	return SNew(SBox).Tag(RowTag)[Button];
}

void SPWAwakeningResultPanel::RebuildBody()
{
	if (!BodyContainer.IsValid())
	{
		return;
	}
	BodyContainer->ClearChildren();
	FocusableRowButtons.Reset();

	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const float Width = PanelWidth(ViewportSize.X, ViewportSize.Y);
	const float InnerWrap = FMath::Max(260.0f, Width - 88.0f);

	if (!Model.bAvailable)
	{
		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f)
		[
			SNew(SBorder)
			.Tag(FName(PWApResult::UnavailableTag))
			.BorderImage(&Style.BrushInset)
			.Padding(FMargin(14.0f))
			[
				SNew(STextBlock)
				.Font(PWUI::Font(17))
				.ColorAndOpacity(Style.TextSecondary)
				.WrapTextAt(InnerWrap)
				.Text(FText::FromString(TEXT("No active world session is available to read the Awakening record.")))
			]
		];
		return;
	}

	if (!Model.bGranted)
	{
		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f)
		[
			SNew(SBorder)
			.Tag(FName(PWApResult::EmptyStateTag))
			.BorderImage(&Style.BrushInset)
			.Padding(FMargin(14.0f))
			[
				SNew(STextBlock)
				.Font(PWUI::Font(17))
				.ColorAndOpacity(Style.TextPrimary)
				.WrapTextAt(InnerWrap)
				.Text(Model.SubtitleText.IsEmpty()
					? FText::FromString(TEXT("The school tutorial dungeon has not yet granted an Awakening record."))
					: Model.SubtitleText)
			]
		];
		return;
	}

	// 1. Starting F-grade stage card
	{
		const TSharedRef<SWidget> StageContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(
						FText::FromString(TEXT("STARTING STAGE WITHIN GRADE F")),
						PWUI::ETextRole::Meta,
						Style.Accent)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Model.Stage.StageCodeText, 22, Style.Accent, true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Model.Stage.StageTitleText, 17, Style.TextPrimary, true)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Font(PWUI::Font(15))
				.ColorAndOpacity(Style.TextSecondary)
				.WrapTextAt(InnerWrap)
				.Text(Model.Stage.RuleNoticeText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				R97MakeEvidenceListWidget(Model.Stage.EvidenceLines, InnerWrap)
			];

		const TSharedRef<SWidget> Btn = AddFocusRow(StageContent, PWApResult::StageCardTag);

		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			Btn
		];
	}

	// 2. Five-Tattva distribution
	BodyContainer->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 4.0f, 0.0f, 4.0f)
	[
		PWUI::MakeText(
			FText::FromString(TEXT("FIVE-TATTVA AFFINITY DISTRIBUTION")),
			PWUI::ETextRole::Meta,
			Style.Accent)
	];

	for (int32 i = 0; i < Model.Tattvas.Num(); ++i)
	{
		const PWAwakening::FPWAwakeningTattvaRow& Row = Model.Tattvas[i];

		const TSharedRef<SWidget> RowContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(Row.Name, 17, Style.TextPrimary, true)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Row.ShareText, PWUI::ETextRole::Meta, Style.Good)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Row.Domain, 15, Style.TextSecondary)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				R97MakeEvidenceListWidget(Row.EvidenceLines, InnerWrap)
			];

		const TSharedRef<SWidget> Btn = AddFocusRow(RowContent, PWApResult::TattvaRowTag);

		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			Btn
		];
	}

	// 3. Six capability dimensions
	BodyContainer->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 8.0f, 0.0f, 4.0f)
	[
		PWUI::MakeText(
			FText::FromString(TEXT("SIX CAPABILITY DIMENSIONS")),
			PWUI::ETextRole::Meta,
			Style.Accent)
	];

	for (int32 i = 0; i < Model.Capabilities.Num(); ++i)
	{
		const PWAwakening::FPWAwakeningCapabilityRow& Row = Model.Capabilities[i];

		const TSharedRef<SWidget> RowContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(Row.Name, 17, Style.TextPrimary, true)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Row.ScoreText, PWUI::ETextRole::Meta, Style.Accent)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Row.Domain, 15, Style.TextSecondary)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Row.BreakdownText, PWUI::ETextRole::Meta, Style.TextSecondary)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				R97MakeEvidenceListWidget(Row.EvidenceLines, InnerWrap)
			];

		const TSharedRef<SWidget> Btn = AddFocusRow(RowContent, PWApResult::CapabilityRowTag);

		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			Btn
		];
	}

	// 4. Derived ability card
	BodyContainer->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 8.0f, 0.0f, 4.0f)
	[
		PWUI::MakeText(
			FText::FromString(TEXT("DERIVED AWAKENED ABILITY")),
			PWUI::ETextRole::Meta,
			Style.Accent)
	];

	{
		const TSharedRef<SWidget> AbilityContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(Model.Ability.Name, 22, Style.Accent, true)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Model.Ability.AffinityText, PWUI::ETextRole::Meta, Style.Good)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Model.Ability.OriginBadgeText, PWUI::ETextRole::Meta, Style.Accent)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Font(PWUI::Font(17))
				.ColorAndOpacity(Style.TextPrimary)
				.WrapTextAt(InnerWrap)
				.Text(Model.Ability.EffectText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				R97MakeEvidenceListWidget(Model.Ability.EvidenceLines, InnerWrap)
			];

		const TSharedRef<SWidget> Btn = AddFocusRow(AbilityContent, PWApResult::AbilityCardTag);

		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			Btn
		];
	}
}

TSharedPtr<SWidget> SPWAwakeningResultPanel::GetInitialFocusTarget() const
{
	if (FocusableRowButtons.IsValidIndex(SelectedSlotIndex) && FocusableRowButtons[SelectedSlotIndex].IsValid())
	{
		return FocusableRowButtons[SelectedSlotIndex];
	}
	if (ContinueButton.IsValid())
	{
		return ContinueButton;
	}
	return nullptr;
}

void SPWAwakeningResultPanel::SelectFocusSlot(int32 Index)
{
	if (FocusableRowButtons.Num() == 0)
	{
		SelectedSlotIndex = 0;
		return;
	}
	SelectedSlotIndex = FMath::Clamp(Index, 0, FocusableRowButtons.Num() - 1);
	const TSharedPtr<SWidget>& Target = FocusableRowButtons[SelectedSlotIndex];
	if (Target.IsValid())
	{
		if (DetailScrollBox.IsValid())
		{
			DetailScrollBox->ScrollDescendantIntoView(Target, true);
		}
		if (FSlateApplication::IsInitialized())
		{
			const TSharedPtr<SWidget> Focusable = PWUI::FindFirstFocusable(Target.ToSharedRef());
			FSlateApplication::Get().SetKeyboardFocus(Focusable.IsValid() ? Focusable : Target, EFocusCause::Navigation);
		}
	}
}

void SPWAwakeningResultPanel::ScrollDetailBy(float DeltaOffset)
{
	if (DetailScrollBox.IsValid())
	{
		const float Next = FMath::Max(0.0f, DetailScrollBox->GetScrollOffset() + DeltaOffset);
		DetailScrollBox->SetScrollOffset(Next);
	}
}

float SPWAwakeningResultPanel::GetDetailScrollOffset() const
{
	return DetailScrollBox.IsValid() ? DetailScrollBox->GetScrollOffset() : 0.0f;
}

FReply SPWAwakeningResultPanel::HandleContinueClicked()
{
	if (TSharedPtr<PWAwakening::IApAwakeningWorld> Bridge = ResolveBridge())
	{
		Bridge->AcknowledgeResult();
	}
	Model.bUnseen = false;
	OnClose.ExecuteIfBound();
	return FReply::Handled();
}

FReply SPWAwakeningResultPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		if (InKeyEvent.IsRepeat())
		{
			return FReply::Handled();
		}
		return HandleContinueClicked();
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		if (FocusableRowButtons.Num() > 0)
		{
			SelectFocusSlot(SelectedSlotIndex + 1);
		}
		else
		{
			ScrollDetailBy(kR97ResultScrollStep);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		if (FocusableRowButtons.Num() > 0)
		{
			SelectFocusSlot(SelectedSlotIndex - 1);
		}
		else
		{
			ScrollDetailBy(-kR97ResultScrollStep);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::PageDown || Key == EKeys::Gamepad_RightStick_Down)
	{
		ScrollDetailBy(kR97ResultScrollStep);
		return FReply::Handled();
	}
	if (Key == EKeys::PageUp || Key == EKeys::Gamepad_RightStick_Up)
	{
		ScrollDetailBy(-kR97ResultScrollStep);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SPWAwakeningResultPanel::OnAnalogValueChanged(
	const FGeometry& MyGeometry,
	const FAnalogInputEvent& InAnalogEvent)
{
	const FKey Key = InAnalogEvent.GetKey();
	const float Val = InAnalogEvent.GetAnalogValue();
	if (Key == EKeys::Gamepad_RightY && FMath::Abs(Val) > kR97ResultStickDeadZone)
	{
		ScrollDetailBy(-Val * 28.0f);
		return FReply::Handled();
	}
	return SCompoundWidget::OnAnalogValueChanged(MyGeometry, InAnalogEvent);
}
