// Copyright Perfect World. All Rights Reserved.

#include "UI/SPWCountdownPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float kR97CountdownScrollStep = 120.0f;
	constexpr float kR97CountdownStickDeadZone = 0.35f;

	TSharedRef<SWidget> R97MakeDisplacedListWidget(
		const TArray<FText>& DisplacedCommitments,
		float WrapWidth)
	{
		const PWUI::FStyle& Style = PWUI::FStyle::Get();
		TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);

		Box->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 2.0f)
		[
			PWUI::MakeText(
				FText::FromString(TEXT("COMMITMENTS THIS WOULD DISPLACE BEFORE CONFIRMING")),
				PWUI::ETextRole::Meta,
				Style.Warn)
		];

		if (DisplacedCommitments.Num() == 0)
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Tag(PWApCountdown::DisplacedLineTag)
				.Font(PWUI::Font(15))
				.ColorAndOpacity(Style.TextSecondary)
				.WrapTextAt(WrapWidth)
				.Text(FText::FromString(TEXT("No standing household or school obligation overlaps this window.")))
			];
			return Box;
		}

		for (const FText& Line : DisplacedCommitments)
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Tag(PWApCountdown::DisplacedLineTag)
				.Font(PWUI::Font(15))
				.ColorAndOpacity(Style.TextPrimary)
				.WrapTextAt(WrapWidth)
				.Text(Line)
			];
		}

		return Box;
	}
}

float SPWCountdownPanel::PanelWidth(float ViewportWidth, float /*ViewportHeight*/)
{
	const float Available = FMath::Max(360.0f, ViewportWidth - 32.0f);
	return FMath::Clamp(Available, 360.0f, 1140.0f);
}

float SPWCountdownPanel::PanelMaxHeight(float /*ViewportWidth*/, float ViewportHeight)
{
	const float Available = FMath::Max(300.0f, ViewportHeight - 32.0f);
	return FMath::Clamp(Available, 300.0f, 840.0f);
}

void SPWCountdownPanel::Construct(const FArguments& InArgs)
{
	WorldPtr = InArgs._World;
	BridgeOverride = InArgs._BridgeOverride;
	ViewportSize = InArgs._ViewportSize;
	OnClose = InArgs._OnClose;

	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const float Width = PanelWidth(ViewportSize.X, ViewportSize.Y);
	const float MaxHeight = PanelMaxHeight(ViewportSize.X, ViewportSize.Y);
	const bool bCompact = ViewportSize.Y < 560.0f || ViewportSize.X < 960.0f;
	const float BodyScrollMaxH = FMath::Max(120.0f, MaxHeight - (bCompact ? 168.0f : 196.0f));

	OrdinaryDayButton = PWUI::MakeButton(
		FText::FromString(TEXT("Keep an ordinary day")),
		PWUI::EButtonKind::Primary,
		FOnClicked::CreateSP(this, &SPWCountdownPanel::HandleKeepOrdinaryDayClicked),
		true, 20, 0.0f);

	ConfirmButton = PWUI::MakeButton(
		FText::FromString(TEXT("Confirm selected practice")),
		PWUI::EButtonKind::Secondary,
		FOnClicked::CreateSP(this, &SPWCountdownPanel::HandleConfirmClicked),
		true, 20, 0.0f);

	CloseButton = PWUI::MakeButton(
		FText::FromString(TEXT("Back")),
		PWUI::EButtonKind::Secondary,
		FOnClicked::CreateSP(this, &SPWCountdownPanel::HandleCloseClicked),
		true, 20, 0.0f);

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(Width)
		.MaxDesiredHeight(MaxHeight)
		[
			SNew(SBorder)
			.Tag(PWApCountdown::PanelTag)
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
							.Diameter(bCompact ? 24.0f : 30.0f)
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
									FText::FromString(TEXT("EXAMINATION COUNTDOWN · OPTIONAL PRACTICE")),
									22,
									Style.Accent, true)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Tag(PWApCountdown::HeadlineTag)
								.Font(PWUI::Font(17, true))
								.ColorAndOpacity(Style.Good)
								.WrapTextAt(Width - 110.0f)
								.Text_Lambda([this]() { return Model.CountdownHeadlineText; })
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Top)
						[
							PWUI::MakeText(
								TAttribute<FText>::CreateLambda([this]() { return Model.CurrentClockText; }),
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
							SNew(SBox).Tag(PWApCountdown::OrdinaryDayButtonTag)[OrdinaryDayButton.ToSharedRef()]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBox).Tag(PWApCountdown::ConfirmButtonTag)[ConfirmButton.ToSharedRef()]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SBox).Tag(PWApCountdown::CloseButtonTag)[CloseButton.ToSharedRef()]
						]
					]
				]
			]
		]
	];

	RefreshFromBridge();
}

TSharedPtr<PWAwakening::IApAwakeningWorld> SPWCountdownPanel::ResolveBridge() const
{
	if (BridgeOverride.IsValid())
	{
		return BridgeOverride;
	}
	return PWAwakening::ApBridge::Resolve(WorldPtr.Get());
}

void SPWCountdownPanel::RefreshFromBridge()
{
	if (TSharedPtr<PWAwakening::IApAwakeningWorld> Bridge = ResolveBridge())
	{
		SetModel(Bridge->ReadCountdownModel());
	}
	else
	{
		PWAwakening::FPWCountdownModel Unavail;
		Unavail.bAvailable = false;
		Unavail.CountdownHeadlineText = FText::FromString(
			TEXT("Examination countdown is unavailable outside an active session."));
		SetModel(Unavail);
	}
}

void SPWCountdownPanel::SetModel(const PWAwakening::FPWCountdownModel& InModel)
{
	Model = InModel;
	SelectedOfferIndex = FMath::Clamp(SelectedOfferIndex, 0, FMath::Max(0, Model.Offers.Num() - 1));
	RebuildBody();
}

void SPWCountdownPanel::RebuildBody()
{
	if (!BodyContainer.IsValid())
	{
		return;
	}
	BodyContainer->ClearChildren();
	OfferRowButtons.Reset();

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
			.Tag(PWApCountdown::UnavailableTag)
			.BorderImage(&Style.BrushInset)
			.Padding(FMargin(14.0f))
			[
				SNew(STextBlock)
				.Font(PWUI::Font(17))
				.ColorAndOpacity(Style.TextSecondary)
				.WrapTextAt(InnerWrap)
				.Text(FText::FromString(TEXT("No active world session is available to read the examination countdown.")))
			]
		];
		return;
	}

	// Freedom & deadline notice card
	BodyContainer->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 0.0f, 0.0f, 8.0f)
	[
		SNew(SBorder)
		.BorderImage(&Style.BrushInset)
		.Padding(FMargin(12.0f, 8.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					PWUI::MakeText(
						FText::FromString(TEXT("NATIONAL VIRTUAL-TOWER EXAMINATION DEADLINE")),
						PWUI::ETextRole::Meta,
						Style.Accent)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Model.ExamDeadlineText, PWUI::ETextRole::Meta, Style.Accent)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(PWUI::Font(15))
				.ColorAndOpacity(Style.TextPrimary)
				.WrapTextAt(InnerWrap)
				.Text(Model.FreedomNoticeText)
			]
		]
	];

	if (!Model.StatusBannerText.IsEmpty())
	{
		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SBorder)
			.Tag(PWApCountdown::StatusBannerTag)
			.BorderImage(&Style.BrushInset)
			.Padding(FMargin(12.0f, 8.0f))
			[
				SNew(STextBlock)
				.Font(PWUI::Font(17, true))
				.ColorAndOpacity(Style.Good)
				.WrapTextAt(InnerWrap)
				.Text(Model.StatusBannerText)
			]
		];
	}

	// Today's optional practice offers
	BodyContainer->AddSlot()
	.AutoHeight()
	.Padding(0.0f, 4.0f, 0.0f, 4.0f)
	[
		PWUI::MakeText(
			FText::FromString(TEXT("TODAY'S OPTIONAL PRACTICE OFFERS")),
			PWUI::ETextRole::Meta,
			Style.Accent)
	];

	if (Model.Offers.Num() == 0)
	{
		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 8.0f)
		[
			SNew(SBorder)
			.Tag(PWApCountdown::EmptyOffersTag)
			.BorderImage(&Style.BrushInset)
			.Padding(FMargin(12.0f, 10.0f))
			[
				SNew(STextBlock)
				.Font(PWUI::Font(17))
				.ColorAndOpacity(Style.TextSecondary)
				.WrapTextAt(InnerWrap)
				.Text(FText::FromString(
					TEXT("No optional practice windows are open for the remainder of today. Your ordinary day continues uninterrupted.")))
			]
		];
	}
	else
	{
		for (int32 i = 0; i < Model.Offers.Num(); ++i)
		{
			const PWAwakening::FPWCountdownOfferRow& Offer = Model.Offers[i];
			const bool bSelected = (i == SelectedOfferIndex);

			const TSharedRef<SWidget> OfferContent = SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						PWUI::MakeText(
							Offer.Title,
							17,
							bSelected ? Style.Accent : Style.TextPrimary,
							true)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						PWUI::MakeText(Offer.WindowText, PWUI::ETextRole::Meta, Style.Good)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						PWUI::MakeText(Offer.ContactText, 15, Style.TextSecondary)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						PWUI::MakeText(Offer.AvailabilityText, PWUI::ETextRole::Meta, Style.Accent)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					PWUI::MakeText(Offer.SummaryText, 15, Style.TextPrimary)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					R97MakeDisplacedListWidget(Offer.DisplacedCommitments, InnerWrap)
				];

			const TSharedRef<SWidget> OfferButton = PWUI::MakeContentButton(
				OfferContent,
				PWUI::EButtonKind::TextRow,
				FOnClicked::CreateSPLambda(this, [this, i]()
				{
					SelectOfferIndex(i);
					return FReply::Handled();
				}),
				bSelected,
				true,
				FMargin(12.0f, 8.0f));
			OfferRowButtons.Add(OfferButton);
			const TSharedRef<SWidget> Btn = SNew(SBox).Tag(PWApCountdown::OfferRowTag)[OfferButton];

			BodyContainer->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				Btn
			];
		}

		if (Model.Offers.IsValidIndex(SelectedOfferIndex))
		{
			const PWAwakening::FPWCountdownOfferRow& Sel = Model.Offers[SelectedOfferIndex];
			BodyContainer->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.Tag(PWApCountdown::DisplacedCardTag)
				.BorderImage(&Style.BrushInset)
				.Padding(FMargin(12.0f, 8.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						PWUI::MakeText(
							FText::FromString(FString::Printf(
								TEXT("BEFORE CONFIRMING: %s (%s)"),
								*Sel.Title.ToString(),
								*Sel.WindowText.ToString())),
							17,
							Style.Accent, true)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						R97MakeDisplacedListWidget(Sel.DisplacedCommitments, InnerWrap)
					]
				]
			];
		}
	}

	// Standing obligations section
	if (Model.Obligations.Num() > 0)
	{
		BodyContainer->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 4.0f)
		[
			PWUI::MakeText(
				FText::FromString(TEXT("STANDING HOUSEHOLD & SCHOOL COMMITMENTS")),
				PWUI::ETextRole::Meta,
				Style.Accent)
		];

		for (const PWAwakening::FPWCountdownObligationRow& Obl : Model.Obligations)
		{
			BodyContainer->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(SBorder)
				.Tag(PWApCountdown::ObligationRowTag)
				.BorderImage(&Style.BrushInset)
				.Padding(FMargin(12.0f, 6.0f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							PWUI::MakeText(Obl.Title, 17, Style.TextPrimary, true)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							PWUI::MakeText(Obl.PersonText, 15, Style.TextSecondary)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						[
							PWUI::MakeText(Obl.WindowText, PWUI::ETextRole::Meta, Style.Good)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						[
							PWUI::MakeText(Obl.StatusText, PWUI::ETextRole::Meta, Style.TextSecondary)
						]
					]
				]
			];
		}
	}
}

TSharedPtr<SWidget> SPWCountdownPanel::GetInitialFocusTarget() const
{
	if (OfferRowButtons.IsValidIndex(SelectedOfferIndex) && OfferRowButtons[SelectedOfferIndex].IsValid())
	{
		return OfferRowButtons[SelectedOfferIndex];
	}
	if (OrdinaryDayButton.IsValid())
	{
		return OrdinaryDayButton;
	}
	return CloseButton;
}

void SPWCountdownPanel::SelectOfferIndex(int32 Index)
{
	if (Model.Offers.Num() == 0)
	{
		SelectedOfferIndex = 0;
		return;
	}
	SelectedOfferIndex = FMath::Clamp(Index, 0, Model.Offers.Num() - 1);
	RebuildBody();
	if (OfferRowButtons.IsValidIndex(SelectedOfferIndex) && OfferRowButtons[SelectedOfferIndex].IsValid())
	{
		const TSharedPtr<SWidget>& Target = OfferRowButtons[SelectedOfferIndex];
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

bool SPWCountdownPanel::TriggerConfirmSelectedOffer()
{
	TSharedPtr<PWAwakening::IApAwakeningWorld> Bridge = ResolveBridge();
	if (!Bridge.IsValid() || !Model.Offers.IsValidIndex(SelectedOfferIndex))
	{
		LastFeedbackText = FText::FromString(TEXT("Select an available optional practice window first."));
		return false;
	}
	const FString OfferId = Model.Offers[SelectedOfferIndex].Id;
	FText Notice;
	const bool bConfirmed = Bridge->ConfirmPracticeOffer(OfferId, Notice);
	LastFeedbackText = Notice;
	RefreshFromBridge();
	return bConfirmed;
}

bool SPWCountdownPanel::TriggerKeepOrdinaryDay()
{
	TSharedPtr<PWAwakening::IApAwakeningWorld> Bridge = ResolveBridge();
	if (!Bridge.IsValid())
	{
		LastFeedbackText = FText::FromString(TEXT("No active session is available."));
		return false;
	}
	FText Notice;
	const bool bKept = Bridge->KeepOrdinaryDay(Notice);
	LastFeedbackText = Notice;
	RefreshFromBridge();
	return bKept;
}

FReply SPWCountdownPanel::HandleConfirmClicked()
{
	TriggerConfirmSelectedOffer();
	return FReply::Handled();
}

FReply SPWCountdownPanel::HandleKeepOrdinaryDayClicked()
{
	TriggerKeepOrdinaryDay();
	return FReply::Handled();
}

FReply SPWCountdownPanel::HandleCloseClicked()
{
	OnClose.ExecuteIfBound();
	return FReply::Handled();
}

void SPWCountdownPanel::ScrollDetailBy(float DeltaOffset)
{
	if (DetailScrollBox.IsValid())
	{
		const float Next = FMath::Max(0.0f, DetailScrollBox->GetScrollOffset() + DeltaOffset);
		DetailScrollBox->SetScrollOffset(Next);
	}
}

float SPWCountdownPanel::GetDetailScrollOffset() const
{
	return DetailScrollBox.IsValid() ? DetailScrollBox->GetScrollOffset() : 0.0f;
}

FReply SPWCountdownPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
	{
		if (InKeyEvent.IsRepeat())
		{
			return FReply::Handled();
		}
		OnClose.ExecuteIfBound();
		return FReply::Handled();
	}
	if (Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_LeftStick_Down)
	{
		if (OfferRowButtons.Num() > 0)
		{
			SelectOfferIndex(SelectedOfferIndex + 1);
		}
		else
		{
			ScrollDetailBy(kR97CountdownScrollStep);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::Up || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_LeftStick_Up)
	{
		if (OfferRowButtons.Num() > 0)
		{
			SelectOfferIndex(SelectedOfferIndex - 1);
		}
		else
		{
			ScrollDetailBy(-kR97CountdownScrollStep);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::PageDown || Key == EKeys::Gamepad_RightStick_Down)
	{
		ScrollDetailBy(kR97CountdownScrollStep);
		return FReply::Handled();
	}
	if (Key == EKeys::PageUp || Key == EKeys::Gamepad_RightStick_Up)
	{
		ScrollDetailBy(-kR97CountdownScrollStep);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

FReply SPWCountdownPanel::OnAnalogValueChanged(
	const FGeometry& MyGeometry,
	const FAnalogInputEvent& InAnalogEvent)
{
	const FKey Key = InAnalogEvent.GetKey();
	const float Val = InAnalogEvent.GetAnalogValue();
	if (Key == EKeys::Gamepad_RightY && FMath::Abs(Val) > kR97CountdownStickDeadZone)
	{
		ScrollDetailBy(-Val * 28.0f);
		return FReply::Handled();
	}
	return SCompoundWidget::OnAnalogValueChanged(MyGeometry, InAnalogEvent);
}
