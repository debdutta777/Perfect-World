// Copyright Perfect World. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "PerfectWorldAwakeningResultBridge.h"

class SButton;
class SScrollBox;
class SVerticalBox;
class UWorld;

namespace PWApCountdown
{
	inline const FName PanelTag(TEXT("PWApCountdown.Panel"));
	inline const FName HeadlineTag(TEXT("PWApCountdown.Headline"));
	inline const FName OfferRowTag(TEXT("PWApCountdown.OfferRow"));
	inline const FName DisplacedCardTag(TEXT("PWApCountdown.DisplacedCard"));
	inline const FName DisplacedLineTag(TEXT("PWApCountdown.DisplacedLine"));
	inline const FName ObligationRowTag(TEXT("PWApCountdown.ObligationRow"));
	inline const FName StatusBannerTag(TEXT("PWApCountdown.StatusBanner"));
	inline const FName EmptyOffersTag(TEXT("PWApCountdown.EmptyOffers"));
	inline const FName UnavailableTag(TEXT("PWApCountdown.Unavailable"));
	inline const FName ConfirmButtonTag(TEXT("PWApCountdown.ConfirmButton"));
	inline const FName OrdinaryDayButtonTag(TEXT("PWApCountdown.OrdinaryDayButton"));
	inline const FName CloseButtonTag(TEXT("PWApCountdown.CloseButton"));
}

class SPWCountdownPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPWCountdownPanel)
		: _World(nullptr)
		, _ViewportSize(FVector2D(1920.0f, 1080.0f))
	{}
		SLATE_ARGUMENT(UWorld*, World)
		SLATE_ARGUMENT(TSharedPtr<PWAwakening::IApAwakeningWorld>, BridgeOverride)
		SLATE_ARGUMENT(FVector2D, ViewportSize)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void RefreshFromBridge();
	void SetModel(const PWAwakening::FPWCountdownModel& InModel);
	const PWAwakening::FPWCountdownModel& GetModel() const { return Model; }

	TSharedPtr<SWidget> GetInitialFocusTarget() const;
	void SelectOfferIndex(int32 Index);
	int32 GetSelectedOfferIndex() const { return SelectedOfferIndex; }
	int32 GetOfferButtonCount() const { return OfferRowButtons.Num(); }

	bool TriggerConfirmSelectedOffer();
	bool TriggerKeepOrdinaryDay();
	FText GetLastFeedbackText() const { return LastFeedbackText; }

	void ScrollDetailBy(float DeltaOffset);
	float GetDetailScrollOffset() const;

	static float PanelWidth(float ViewportWidth, float ViewportHeight = 1080.0f);
	static float PanelMaxHeight(float ViewportWidth, float ViewportHeight);

	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogEvent) override;

private:
	TSharedPtr<PWAwakening::IApAwakeningWorld> ResolveBridge() const;
	void RebuildBody();
	FReply HandleConfirmClicked();
	FReply HandleKeepOrdinaryDayClicked();
	FReply HandleCloseClicked();

	TWeakObjectPtr<UWorld> WorldPtr;
	TSharedPtr<PWAwakening::IApAwakeningWorld> BridgeOverride;
	FVector2D ViewportSize = FVector2D(1920.0f, 1080.0f);
	FSimpleDelegate OnClose;

	PWAwakening::FPWCountdownModel Model;
	int32 SelectedOfferIndex = 0;
	FText LastFeedbackText;

	TSharedPtr<SScrollBox> DetailScrollBox;
	TSharedPtr<SVerticalBox> BodyContainer;
	TArray<TSharedPtr<SWidget>> OfferRowButtons;
	TSharedPtr<SWidget> ConfirmButton;
	TSharedPtr<SWidget> OrdinaryDayButton;
	TSharedPtr<SWidget> CloseButton;
};
