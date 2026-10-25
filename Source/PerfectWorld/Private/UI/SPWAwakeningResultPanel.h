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

namespace PWApResult
{
	inline const FName PanelTag(TEXT("PWApResult.Panel"));
	inline const FName StageCardTag(TEXT("PWApResult.StageCard"));
	inline const FName TattvaRowTag(TEXT("PWApResult.TattvaRow"));
	inline const FName CapabilityRowTag(TEXT("PWApResult.CapabilityRow"));
	inline const FName AbilityCardTag(TEXT("PWApResult.AbilityCard"));
	inline const FName EvidenceLineTag(TEXT("PWApResult.EvidenceLine"));
	inline const FName EmptyStateTag(TEXT("PWApResult.EmptyState"));
	inline const FName UnavailableTag(TEXT("PWApResult.Unavailable"));
	inline const FName ContinueButtonTag(TEXT("PWApResult.ContinueButton"));
}

class SPWAwakeningResultPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPWAwakeningResultPanel)
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
	void SetModel(const PWAwakening::FPWAwakeningResultModel& InModel);
	const PWAwakening::FPWAwakeningResultModel& GetModel() const { return Model; }

	TSharedPtr<SWidget> GetInitialFocusTarget() const;
	void SelectFocusSlot(int32 Index);
	int32 GetSelectedFocusSlot() const { return SelectedSlotIndex; }
	int32 GetFocusSlotCount() const { return FocusableRowButtons.Num(); }

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
	FReply HandleContinueClicked();

	TWeakObjectPtr<UWorld> WorldPtr;
	TSharedPtr<PWAwakening::IApAwakeningWorld> BridgeOverride;
	FVector2D ViewportSize = FVector2D(1920.0f, 1080.0f);
	FSimpleDelegate OnClose;

	PWAwakening::FPWAwakeningResultModel Model;
	int32 SelectedSlotIndex = 0;

	TSharedPtr<SScrollBox> DetailScrollBox;
	TSharedPtr<SVerticalBox> BodyContainer;
	TArray<TSharedPtr<SButton>> FocusableRowButtons;
	TSharedPtr<SButton> ContinueButton;
};
