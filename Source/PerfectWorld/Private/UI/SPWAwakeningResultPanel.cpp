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
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float kApResultScrollStep = 120.0f;
	constexpr float kApResultStickDeadZone = 0.35f;

	TSharedRef<SWidget> ApMakeEvidenceListWidget(
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
				Style.FontMonoSmall,
				Style.GoldBrass)
		];

		if (EvidenceLines.Num() == 0)
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Tag(PWApResult::EvidenceLineTag)
				.Font(Style.FontBodySmall)
				.ColorAndOpacity(Style.PaperMuted)
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
				.Tag(PWApResult::EvidenceLineTag)
				.Font(Style.FontBodySmall)
				.ColorAndOpacity(Style.PaperWarm)
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
	SetTag(PWApResult::PanelTag);

	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const float Width = PanelWidth(ViewportSize.X, ViewportSize.Y);
	const float MaxHeight = PanelMaxHeight(ViewportSize.X, ViewportSize.Y);
	const bool bCompact = ViewportSize.Y < 560.0f || ViewportSize.X < 960.0f;
	const float BodyScrollMaxH = FMath::Max(120.0f, MaxHeight - (bCompact ? 156.0f : 184.0f));

	ContinueButton = PWUI::MakeButton(
		FText::FromString(TEXT("Continue into the day")),
		FOnClicked::CreateSP(this, &SPWAwakeningResultPanel::HandleContinueClicked),
		PWUI::EButtonKind::Primary);
	ContinueButton->SetTag(PWApResult::ContinueButtonTag);

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(Width)
		.MaxDesiredHeight(MaxHeight)
		[
			SNew(SBorder)
			.BorderImage(&Style.BrushPanel)
			.Padding(FMargin(bCompact ? 14.0f : 20.0f))
			[
				SNew(PWUI::SPWCornerBrackets)
				.Arm(14.0f)
				.Thickness(1.5f)
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
							SNew(PWUI::SPWAwakeningSigil)
							.Size(bCompact ? 24.0f : 32.0f)
							.Phase(1.0f)
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
									Style.FontTitleSmall,
									Style.GoldBright)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Font(Style.FontBodySmall)
								.ColorAndOpacity(Style.PaperMuted)
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
								Style.FontMonoSmall,
								Style.PaperMuted)
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
								Style.FontMonoSmall,
								Style.PaperMuted)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							ContinueButton.ToSharedRef()
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
			.Tag(PWApResult::UnavailableTag)
			.BorderImage(&Style.BrushInset)
			.Padding(FMargin(14.0f))
			[
				SNew(STextBlock)
				.Font(Style.FontBody)
				.ColorAndOpacity(Style.PaperMuted)
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
			.Tag(PWApResult::EmptyStateTag)
			.BorderImage(&Style.BrushInset)
			.Padding(FMargin(14.0f))
			[
				SNew(STextBlock)
				.Font(Style.FontBody)
				.ColorAndOpacity(Style.PaperWarm)
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
		const int32 SlotIdx = FocusableRowButtons.Num();
		TSharedRef<SVerticalBox> StageContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(
						FText::FromString(TEXT("STARTING STAGE WITHIN GRADE F")),
						Style.FontMonoSmall,
						Style.GoldBrass)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Model.Stage.StageCodeText, Style.FontTitleSmall, Style.GoldBright)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Model.Stage.StageTitleText, Style.FontBodyBold, Style.PaperWarm)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Font(Style.FontBodySmall)
				.ColorAndOpacity(Style.PaperMuted)
				.WrapTextAt(InnerWrap)
				.Text(Model.Stage.RuleNoticeText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ApMakeEvidenceListWidget(Model.Stage.EvidenceLines, InnerWrap)
			];

		TSharedRef<SButton> Btn = PWUI::MakeContentButton(
			StageContent,
			FOnClicked::CreateLambda([this, SlotIdx]()
			{
				SelectFocusSlot(SlotIdx);
				return FReply::Handled();
			}),
			PWUI::EButtonKind::Ghost,
			FMargin(12.0f, 8.0f),
			Model.Stage.StageTitleText);
		Btn->SetTag(PWApResult::StageCardTag);
		FocusableRowButtons.Add(Btn);

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
			Style.FontMono,
			Style.GoldBright)
	];

	for (int32 i = 0; i < Model.Tattvas.Num(); ++i)
	{
		const PWAwakening::FPWAwakeningTattvaRow& Row = Model.Tattvas[i];
		const int32 SlotIdx = FocusableRowButtons.Num();

		TSharedRef<SVerticalBox> RowContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(Row.Name, Style.FontBodyBold, Style.PaperWarm)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Row.ShareText, Style.FontMono, Style.CyanSignal)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Row.Domain, Style.FontBodySmall, Style.PaperMuted)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ApMakeEvidenceListWidget(Row.EvidenceLines, InnerWrap)
			];

		TSharedRef<SButton> Btn = PWUI::MakeContentButton(
			RowContent,
			FOnClicked::CreateLambda([this, SlotIdx]()
			{
				SelectFocusSlot(SlotIdx);
				return FReply::Handled();
			}),
			PWUI::EButtonKind::Ghost,
			FMargin(12.0f, 8.0f),
			Row.Name);
		Btn->SetTag(PWApResult::TattvaRowTag);
		FocusableRowButtons.Add(Btn);

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
			Style.FontMono,
			Style.GoldBright)
	];

	for (int32 i = 0; i < Model.Capabilities.Num(); ++i)
	{
		const PWAwakening::FPWAwakeningCapabilityRow& Row = Model.Capabilities[i];
		const int32 SlotIdx = FocusableRowButtons.Num();

		TSharedRef<SVerticalBox> RowContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(Row.Name, Style.FontBodyBold, Style.PaperWarm)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Row.ScoreText, Style.FontMono, Style.GoldBright)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Row.Domain, Style.FontBodySmall, Style.PaperMuted)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Row.BreakdownText, Style.FontMonoSmall, Style.PaperMuted)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ApMakeEvidenceListWidget(Row.EvidenceLines, InnerWrap)
			];

		TSharedRef<SButton> Btn = PWUI::MakeContentButton(
			RowContent,
			FOnClicked::CreateLambda([this, SlotIdx]()
			{
				SelectFocusSlot(SlotIdx);
				return FReply::Handled();
			}),
			PWUI::EButtonKind::Ghost,
			FMargin(12.0f, 8.0f),
			Row.Name);
		Btn->SetTag(PWApResult::CapabilityRowTag);
		FocusableRowButtons.Add(Btn);

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
			Style.FontMono,
			Style.GoldBright)
	];

	{
		const int32 SlotIdx = FocusableRowButtons.Num();
		TSharedRef<SVerticalBox> AbilityContent = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(Model.Ability.Name, Style.FontTitleSmall, Style.GoldBright)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Model.Ability.AffinityText, Style.FontMono, Style.CyanSignal)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				PWUI::MakeText(Model.Ability.OriginBadgeText, Style.FontMonoSmall, Style.GoldBrass)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Font(Style.FontBody)
				.ColorAndOpacity(Style.PaperWarm)
				.WrapTextAt(InnerWrap)
				.Text(Model.Ability.EffectText)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				ApMakeEvidenceListWidget(Model.Ability.EvidenceLines, InnerWrap)
			];

		TSharedRef<SButton> Btn = PWUI::MakeContentButton(
			AbilityContent,
			FOnClicked::CreateLambda([this, SlotIdx]()
			{
				SelectFocusSlot(SlotIdx);
				return FReply::Handled();
			}),
			PWUI::EButtonKind::Ghost,
			FMargin(12.0f, 8.0f),
			Model.Ability.Name);
		Btn->SetTag(PWApResult::AbilityCardTag);
		FocusableRowButtons.Add(Btn);

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
	const TSharedPtr<SButton>& Target = FocusableRowButtons[SelectedSlotIndex];
	if (Target.IsValid())
	{
		if (DetailScrollBox.IsValid())
		{
			DetailScrollBox->ScrollDescendantIntoView(Target, true);
		}
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().SetKeyboardFocus(Target, EFocusCause::Navigation);
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
			ScrollDetailBy(kApResultScrollStep);
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
			ScrollDetailBy(-kApResultScrollStep);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::PageDown || Key == EKeys::Gamepad_RightStick_Down)
	{
		ScrollDetailBy(kApResultScrollStep);
		return FReply::Handled();
	}
	if (Key == EKeys::PageUp || Key == EKeys::Gamepad_RightStick_Up)
	{
		ScrollDetailBy(-kApResultScrollStep);
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
	if (Key == EKeys::Gamepad_RightY && FMath::Abs(Val) > kApResultStickDeadZone)
	{
		ScrollDetailBy(-Val * 28.0f);
		return FReply::Handled();
	}
	return SCompoundWidget::OnAnalogValueChanged(MyGeometry, InAnalogEvent);
}
