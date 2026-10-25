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
#include "Widgets/Text/STextBlock.h"

namespace
{
	constexpr float kApCountdownScrollStep = 120.0f;
	constexpr float kApCountdownStickDeadZone = 0.35f;

	TSharedRef<SWidget> ApMakeDisplacedListWidget(
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
				Style.FontMonoSmall,
				Style.AmberWarn)
		];

		if (DisplacedCommitments.Num() == 0)
		{
			Box->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Tag(PWApCountdown::DisplacedLineTag)
				.Font(Style.FontBodySmall)
				.ColorAndOpacity(Style.PaperMuted)
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
				.Font(Style.FontBodySmall)
				.ColorAndOpacity(Style.PaperWarm)
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
	SetTag(PWApCountdown::PanelTag);

	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const float Width = PanelWidth(ViewportSize.X, ViewportSize.Y);
	const float MaxHeight = PanelMaxHeight(ViewportSize.X, ViewportSize.Y);
	const bool bCompact = ViewportSize.Y < 560.0f || ViewportSize.X < 960.0f;
	const float BodyScrollMaxH = FMath::Max(120.0f, MaxHeight - (bCompact ? 168.0f : 196.0f));

	OrdinaryDayButton = PWUI::MakeButton(
		FText::FromString(TEXT("Keep an ordinary day")),
		FOnClicked::CreateSP(this, &SPWCountdownPanel::HandleKeepOrdinaryDayClicked),
		PWUI::EButtonKind::Primary);
	OrdinaryDayButton->SetTag(PWApCountdown::OrdinaryDayButtonTag);

	ConfirmButton = PWUI::MakeButton(
		FText::FromString(TEXT("Confirm selected practice")),
		FOnClicked::CreateSP(this, &SPWCountdownPanel::HandleConfirmClicked),
		PWUI::EButtonKind::Secondary);
	ConfirmButton->SetTag(PWApCountdown::ConfirmButtonTag);

	CloseButton = PWUI::MakeButton(
		FText::FromString(TEXT("Back")),
		FOnClicked::CreateSP(this, &SPWCountdownPanel::HandleCloseClicked),
		PWUI::EButtonKind::Ghost);
	CloseButton->SetTag(PWApCountdown::CloseButtonTag);

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
							.Size(bCompact ? 24.0f : 30.0f)
							.Phase(0.75f)
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
									Style.FontTitleSmall,
									Style.GoldBright)
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(0.0f, 2.0f, 0.0f, 0.0f)
							[
								SNew(STextBlock)
								.Tag(PWApCountdown::HeadlineTag)
								.Font(Style.FontBodyBold)
								.ColorAndOpacity(Style.CyanSignal)
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
							OrdinaryDayButton.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							ConfirmButton.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(8.0f, 0.0f, 0.0f, 0.0f)
						[
							CloseButton.ToSharedRef()
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
				.Font(Style.FontBody)
				.ColorAndOpacity(Style.PaperMuted)
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
						Style.FontMonoSmall,
						Style.GoldBrass)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					PWUI::MakeText(Model.ExamDeadlineText, Style.FontMono, Style.GoldBright)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Font(Style.FontBodySmall)
				.ColorAndOpacity(Style.PaperWarm)
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
				.Font(Style.FontBodyBold)
				.ColorAndOpacity(Style.JadeGood)
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
			Style.FontMono,
			Style.GoldBright)
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
				.Font(Style.FontBody)
				.ColorAndOpacity(Style.PaperMuted)
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

			TSharedRef<SVerticalBox> OfferContent = SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						PWUI::MakeText(
							Offer.Title,
							Style.FontBodyBold,
							bSelected ? Style.GoldBright : Style.PaperWarm)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						PWUI::MakeText(Offer.WindowText, Style.FontMono, Style.CyanSignal)
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
						PWUI::MakeText(Offer.ContactText, Style.FontBodySmall, Style.PaperMuted)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						PWUI::MakeText(Offer.AvailabilityText, Style.FontMonoSmall, Style.GoldBrass)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 2.0f)
				[
					PWUI::MakeText(Offer.SummaryText, Style.FontBodySmall, Style.PaperWarm)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					ApMakeDisplacedListWidget(Offer.DisplacedCommitments, InnerWrap)
				];

			TSharedRef<SButton> Btn = PWUI::MakeContentButton(
				OfferContent,
				FOnClicked::CreateLambda([this, i]()
				{
					SelectOfferIndex(i);
					return FReply::Handled();
				}),
				PWUI::EButtonKind::Ghost,
				FMargin(12.0f, 8.0f),
				Offer.Title);
			Btn->SetTag(PWApCountdown::OfferRowTag);
			OfferRowButtons.Add(Btn);

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
							Style.FontBodyBold,
							Style.GoldBright)
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						ApMakeDisplacedListWidget(Sel.DisplacedCommitments, InnerWrap)
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
				Style.FontMono,
				Style.GoldBright)
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
							PWUI::MakeText(Obl.Title, Style.FontBodyBold, Style.PaperWarm)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							PWUI::MakeText(Obl.PersonText, Style.FontBodySmall, Style.PaperMuted)
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
							PWUI::MakeText(Obl.WindowText, Style.FontMonoSmall, Style.CyanSignal)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Right)
						[
							PWUI::MakeText(Obl.StatusText, Style.FontMonoSmall, Style.PaperMuted)
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
		const TSharedPtr<SButton>& Target = OfferRowButtons[SelectedOfferIndex];
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
			ScrollDetailBy(kApCountdownScrollStep);
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
			ScrollDetailBy(-kApCountdownScrollStep);
		}
		return FReply::Handled();
	}
	if (Key == EKeys::PageDown || Key == EKeys::Gamepad_RightStick_Down)
	{
		ScrollDetailBy(kApCountdownScrollStep);
		return FReply::Handled();
	}
	if (Key == EKeys::PageUp || Key == EKeys::Gamepad_RightStick_Up)
	{
		ScrollDetailBy(-kApCountdownScrollStep);
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
	if (Key == EKeys::Gamepad_RightY && FMath::Abs(Val) > kApCountdownStickDeadZone)
	{
		ScrollDetailBy(-Val * 28.0f);
		return FReply::Handled();
	}
	return SCompoundWidget::OnAnalogValueChanged(MyGeometry, InAnalogEvent);
}
