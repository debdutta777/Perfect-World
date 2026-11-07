// Copyright Perfect World. All Rights Reserved.

#include "PerfectWorldAwakeningResultBridge.h"
#include "UI/SPWAwakeningResultPanel.h"
#include "UI/SPWCountdownPanel.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Geometry.h"
#include "Misc/AutomationTest.h"
#include "PerfectWorldPresentationSettings.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	void R97CollectVisibleTexts(const TSharedRef<SWidget>& Widget, TArray<FString>& OutTexts)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		if (Widget->GetType() == FName(TEXT("STextBlock")))
		{
			const STextBlock& TextBlock = static_cast<const STextBlock&>(Widget.Get());
			const FString Value = TextBlock.GetText().ToString();
			if (!Value.IsEmpty())
			{
				OutTexts.Add(Value);
			}
		}
		FChildren* Children = Widget->GetChildren();
		if (!Children)
		{
			return;
		}
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			R97CollectVisibleTexts(Children->GetChildAt(i), OutTexts);
		}
	}

	void R97CollectTaggedTexts(const TSharedRef<SWidget>& Widget, const FName& Tag, TArray<FString>& OutTexts)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		if (Widget->GetTag() == Tag && Widget->GetType() == FName(TEXT("STextBlock")))
		{
			const STextBlock& TextBlock = static_cast<const STextBlock&>(Widget.Get());
			OutTexts.Add(TextBlock.GetText().ToString());
		}
		FChildren* Children = Widget->GetChildren();
		if (!Children)
		{
			return;
		}
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			R97CollectTaggedTexts(Children->GetChildAt(i), Tag, OutTexts);
		}
	}

	int32 R97CountTaggedWidgets(const TSharedRef<SWidget>& Widget, const FName& Tag)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return 0;
		}
		int32 Count = (Widget->GetTag() == Tag) ? 1 : 0;
		FChildren* Children = Widget->GetChildren();
		if (!Children)
		{
			return Count;
		}
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			Count += R97CountTaggedWidgets(Children->GetChildAt(i), Tag);
		}
		return Count;
	}

	bool R97HasFocusableDescendant(const TSharedRef<SWidget>& Widget)
	{
		FChildren* Children = Widget->GetChildren();
		if (!Children)
		{
			return false;
		}
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			const TSharedRef<SWidget> Child = Children->GetChildAt(i);
			if (Child->SupportsKeyboardFocus() || R97HasFocusableDescendant(Child))
			{
				return true;
			}
		}
		return false;
	}

	void R97CollectFocusableTags(const TSharedRef<SWidget>& Widget, TArray<FName>& OutTags)
	{
		if (!Widget->GetVisibility().IsVisible())
		{
			return;
		}
		// A row is tagged on the box that wraps its focusable button, so a tagged widget counts when it is, or
		// directly wraps, something focusable. The whole-panel tag is not a row.
		const bool bIsPanel = Widget->GetTag() == PWApResult::PanelTag || Widget->GetTag() == PWApCountdown::PanelTag;
		if (!bIsPanel && !Widget->GetTag().IsNone()
			&& (Widget->SupportsKeyboardFocus() || R97HasFocusableDescendant(Widget)))
		{
			OutTags.Add(Widget->GetTag());
			return;
		}
		FChildren* Children = Widget->GetChildren();
		if (!Children)
		{
			return;
		}
		for (int32 i = 0; i < Children->Num(); ++i)
		{
			R97CollectFocusableTags(Children->GetChildAt(i), OutTags);
		}
	}

	FString R97JoinTexts(const TArray<FString>& Texts)
	{
		return FString::Join(Texts, TEXT("\n"));
	}

	bool R97HasRawIdLeak(const FString& AllText, FString& OutOffender)
	{
		static const TCHAR* kForbiddenSubstrings[] = {
			TEXT("attribute."), TEXT("skill."), TEXT("facet."), TEXT("conduct."),
			TEXT("opp_"), TEXT("oblig_"), TEXT("npc_"), TEXT("settled_stance"),
			TEXT("measured_current"), TEXT("kindled_point"), TEXT("paced_crossing"),
			TEXT("clear_interval"), TEXT("unknown_raw_token"), TEXT("_"), TEXT("{"), TEXT("}")
		};
		for (const TCHAR* Token : kForbiddenSubstrings)
		{
			if (AllText.Contains(Token, ESearchCase::IgnoreCase))
			{
				OutOffender = Token;
				return true;
			}
		}
		return false;
	}

	FKeyEvent R97MakeKeyDown(const FKey& Key, bool bRepeat = false)
	{
		return FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPWApVerbatimEvidenceAndNoRawIdsTest,
	"PerfectWorld.AwakeningPanel.VerbatimEvidenceAndNoRawIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPWApVerbatimEvidenceAndNoRawIdsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TSharedRef<PWAwakening::FApFakeAwakeningWorld> Fake = PWAwakening::MakeFakeAwakeningWorld();

	PWAwakening::FPWAwakeningResultModel Model = Fake->ReadResultModel();
	TestTrue(TEXT("Sample Awakening model is available and granted"), Model.bAvailable && Model.bGranted);
	TestEqual(TEXT("Five Tattvas present"), Model.Tattvas.Num(), 5);
	TestEqual(TEXT("Six capabilities present"), Model.Capabilities.Num(), 6);
	TestTrue(TEXT("Starting stage is within F-1..F-10"),
		Model.Stage.StageWithinGrade >= 1 && Model.Stage.StageWithinGrade <= 10);

	const FString VerbatimCrossing = TEXT("You steadied the crossing");
	const FString VerbatimPillar = TEXT("You traced the seam along the tutorial pillar before stepping.");
	Model.Tattvas[0].EvidenceLines.Insert(FText::FromString(VerbatimCrossing), 0);
	Model.Capabilities[0].EvidenceLines.Insert(FText::FromString(VerbatimPillar), 0);

	PerfectWorld::AwakeningCause UnknownRawCause;
	UnknownRawCause.Signal = PerfectWorld::AwakeningSignal::Steadfastness;
	UnknownRawCause.Source = "conduct.unknown_raw_token";
	UnknownRawCause.ChoiceId = "choice_raw_token_99";
	Model.Ability.EvidenceLines.Add(PWAwakening::FormatCauseEvidence(UnknownRawCause));
	Fake->SetResultModel(Model);

	TSharedRef<SPWAwakeningResultPanel> Panel = SNew(SPWAwakeningResultPanel)
		.BridgeOverride(Fake)
		.ViewportSize(FVector2D(1920.0f, 1080.0f));
	Panel->SlatePrepass(1.0f);

	TestEqual(TEXT("Stage card count"), R97CountTaggedWidgets(Panel, PWApResult::StageCardTag), 1);
	TestEqual(TEXT("Five Tattva rows rendered"), R97CountTaggedWidgets(Panel, PWApResult::TattvaRowTag), 5);
	TestEqual(TEXT("Six capability rows rendered"), R97CountTaggedWidgets(Panel, PWApResult::CapabilityRowTag), 6);
	TestEqual(TEXT("One derived ability card rendered"), R97CountTaggedWidgets(Panel, PWApResult::AbilityCardTag), 1);

	TArray<FString> EvidenceTexts;
	R97CollectTaggedTexts(Panel, PWApResult::EvidenceLineTag, EvidenceTexts);
	TestTrue(TEXT("Verbatim crossing evidence line is shown exactly"),
		EvidenceTexts.Contains(VerbatimCrossing));
	TestTrue(TEXT("Verbatim pillar evidence line is shown exactly"),
		EvidenceTexts.Contains(VerbatimPillar));

	TArray<FString> VisibleTexts;
	R97CollectVisibleTexts(Panel, VisibleTexts);
	const FString Joined = R97JoinTexts(VisibleTexts);

	TestTrue(TEXT("Shows Grade F stage"), Joined.Contains(TEXT("Grade F")));
	TestTrue(TEXT("Shows Prithvi"), Joined.Contains(TEXT("Prithvi")));
	TestTrue(TEXT("Shows Akasha"), Joined.Contains(TEXT("Akasha")));
	TestTrue(TEXT("Shows Output"), Joined.Contains(TEXT("Output")));
	TestTrue(TEXT("Shows Coordination"), Joined.Contains(TEXT("Coordination")));
	TestFalse(TEXT("Never invents moral verdicts like 'you are brave'"),
		Joined.Contains(TEXT("you are brave"), ESearchCase::IgnoreCase));

	FString Offender;
	TestFalse(
		FString::Printf(TEXT("No raw IDs leaked into Awakening Result UI (found: %s)"), *Offender),
		R97HasRawIdLeak(Joined, Offender));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPWApCountdownDisplacedAndOrdinaryDayTest,
	"PerfectWorld.AwakeningPanel.CountdownDisplacedAndOrdinaryDay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPWApCountdownDisplacedAndOrdinaryDayTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TSharedRef<PWAwakening::FApFakeAwakeningWorld> Fake = PWAwakening::MakeFakeAwakeningWorld();

	TSharedRef<SPWCountdownPanel> Panel = SNew(SPWCountdownPanel)
		.BridgeOverride(Fake)
		.ViewportSize(FVector2D(1920.0f, 1080.0f));
	Panel->SlatePrepass(1.0f);

	const PWAwakening::FPWCountdownModel& InitialModel = Panel->GetModel();
	TestEqual(TEXT("Days remaining from 24 Jan 08:00 to 2 Feb 06:00 is 8"), InitialModel.DaysRemaining, 8);
	TestEqual(TEXT("Hours remaining from 24 Jan 08:00 to 2 Feb 06:00 is 22"), InitialModel.HoursRemaining, 22);
	TestEqual(TEXT("Two optional practice offers rendered"),
		R97CountTaggedWidgets(Panel, PWApCountdown::OfferRowTag), 2);
	TestEqual(TEXT("One standing obligation rendered"),
		R97CountTaggedWidgets(Panel, PWApCountdown::ObligationRowTag), 1);
	TestEqual(TEXT("Before-confirming displaced card rendered"),
		R97CountTaggedWidgets(Panel, PWApCountdown::DisplacedCardTag), 1);

	TArray<FString> DisplacedLines;
	R97CollectTaggedTexts(Panel, PWApCountdown::DisplacedLineTag, DisplacedLines);
	const FString DisplacedJoined = R97JoinTexts(DisplacedLines);
	TestTrue(TEXT("Shows displaced standing commitment before confirming"),
		DisplacedJoined.Contains(TEXT("Evening tea and market list at home")));
	TestTrue(TEXT("Shows closed same-day option before confirming"),
		DisplacedJoined.Contains(TEXT("Reading room cadence study")));

	TArray<FString> VisibleTexts;
	R97CollectVisibleTexts(Panel, VisibleTexts);
	const FString Joined = R97JoinTexts(VisibleTexts);
	TestTrue(TEXT("Shows examination date 2 February 2043 · 06:00"),
		Joined.Contains(TEXT("2 February 2043 · 06:00")));
	TestTrue(TEXT("Shows 'Keep an ordinary day' option"),
		Joined.Contains(TEXT("Keep an ordinary day")));

	FString Offender;
	TestFalse(
		FString::Printf(TEXT("No raw IDs leaked into Countdown UI (found: %s)"), *Offender),
		R97HasRawIdLeak(Joined, Offender));

	// Verify 'Keep an ordinary day'
	TestTrue(TEXT("TriggerKeepOrdinaryDay succeeds"), Panel->TriggerKeepOrdinaryDay());
	TestEqual(TEXT("KeepOrdinaryDay count incremented"), Fake->GetKeepOrdinaryDayCount(), 1);
	TestTrue(TEXT("Model reflects ordinary day kept"), Panel->GetModel().bOrdinaryDayKept);

	// Re-seed and verify confirming a practice offer
	Fake->SeedSampleCountdownDay();
	Panel->RefreshFromBridge();
	Panel->SelectOfferIndex(0);
	TestTrue(TEXT("TriggerConfirmSelectedOffer succeeds"), Panel->TriggerConfirmSelectedOffer());
	TestEqual(TEXT("ConfirmOfferCount incremented"), Fake->GetConfirmOfferCount(), 1);
	TestEqual(TEXT("Confirmed offer ID recorded on bridge"),
		Fake->GetLastConfirmedOfferId(), FString(TEXT("sample.offer_footing")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPWApEmptyAndUnavailableStatesTest,
	"PerfectWorld.AwakeningPanel.EmptyAndUnavailableStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPWApEmptyAndUnavailableStatesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TSharedRef<PWAwakening::FApFakeAwakeningWorld> Fake = PWAwakening::MakeFakeAwakeningWorld();

	// 1. Unavailable Awakening Result
	PWAwakening::FPWAwakeningResultModel UnavailResult;
	UnavailResult.bAvailable = false;
	Fake->SetResultModel(UnavailResult);

	TSharedRef<SPWAwakeningResultPanel> ResultPanel = SNew(SPWAwakeningResultPanel)
		.BridgeOverride(Fake);
	ResultPanel->SlatePrepass(1.0f);
	TestEqual(TEXT("Unavailable tag shown when result model unavailable"),
		R97CountTaggedWidgets(ResultPanel, PWApResult::UnavailableTag), 1);
	TestEqual(TEXT("Zero Tattva rows when unavailable"),
		R97CountTaggedWidgets(ResultPanel, PWApResult::TattvaRowTag), 0);

	// 2. Available but ungranted Awakening Result
	PWAwakening::FPWAwakeningResultModel UngrantedResult;
	UngrantedResult.bAvailable = true;
	UngrantedResult.bGranted = false;
	Fake->SetResultModel(UngrantedResult);
	ResultPanel->RefreshFromBridge();
	ResultPanel->SlatePrepass(1.0f);
	TestEqual(TEXT("EmptyState tag shown when tutorial not yet granted"),
		R97CountTaggedWidgets(ResultPanel, PWApResult::EmptyStateTag), 1);

	// 3. Unavailable Countdown
	PWAwakening::FPWCountdownModel UnavailCountdown;
	UnavailCountdown.bAvailable = false;
	Fake->SetCountdownModel(UnavailCountdown);

	TSharedRef<SPWCountdownPanel> CountdownPanel = SNew(SPWCountdownPanel)
		.BridgeOverride(Fake);
	CountdownPanel->SlatePrepass(1.0f);
	TestEqual(TEXT("Unavailable tag shown when countdown model unavailable"),
		R97CountTaggedWidgets(CountdownPanel, PWApCountdown::UnavailableTag), 1);

	// 4. Empty offers Countdown
	PWAwakening::FPWCountdownModel EmptyOffersCountdown;
	EmptyOffersCountdown.bAvailable = true;
	EmptyOffersCountdown.ExamDeadlineText = FText::FromString(TEXT("2 February 2043 · 06:00"));
	EmptyOffersCountdown.CountdownHeadlineText = FText::FromString(TEXT("5 days, 0 hours until the examination"));
	EmptyOffersCountdown.FreedomNoticeText = FText::FromString(TEXT("Optional practice is never required."));
	Fake->SetCountdownModel(EmptyOffersCountdown);
	CountdownPanel->RefreshFromBridge();
	CountdownPanel->SlatePrepass(1.0f);
	TestEqual(TEXT("EmptyOffers tag shown when no offers remain today"),
		R97CountTaggedWidgets(CountdownPanel, PWApCountdown::EmptyOffersTag), 1);
	TestEqual(TEXT("OrdinaryDayButton remains present when offers are empty"),
		R97CountTaggedWidgets(CountdownPanel, PWApCountdown::OrdinaryDayButtonTag), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPWApFocusOrderAndNavigationTest,
	"PerfectWorld.AwakeningPanel.FocusOrderAndControllerNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPWApFocusOrderAndNavigationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const bool bPrevReducedMotion = UPerfectWorldPresentationSettings::Get().IsReducedMotion();
	UPerfectWorldPresentationSettings::Get().SetReducedMotion(true);

	TSharedRef<PWAwakening::FApFakeAwakeningWorld> Fake = PWAwakening::MakeFakeAwakeningWorld();
	bool bResultClosed = false;
	TSharedRef<SPWAwakeningResultPanel> ResultPanel = SNew(SPWAwakeningResultPanel)
		.BridgeOverride(Fake)
		.OnClose_Lambda([&bResultClosed]() { bResultClosed = true; });
	ResultPanel->SlatePrepass(1.0f);

	TestTrue(TEXT("Result panel initial focus target is valid"),
		ResultPanel->GetInitialFocusTarget().IsValid());
	TestEqual(TEXT("Result panel has 13 focusable section slots (1 stage + 5 tattvas + 6 capabilities + 1 ability)"),
		ResultPanel->GetFocusSlotCount(), 13);

	TArray<FName> FocusOrder;
	R97CollectFocusableTags(ResultPanel, FocusOrder);
	TestTrue(TEXT("Focusable order starts with StageCard and ends with ContinueButton"),
		FocusOrder.Num() >= 14
		&& FocusOrder[0] == PWApResult::StageCardTag
		&& FocusOrder.Last() == PWApResult::ContinueButtonTag);

	const FGeometry DummyGeo;
	ResultPanel->OnKeyDown(DummyGeo, R97MakeKeyDown(EKeys::Down));
	TestEqual(TEXT("Down key advances focus slot to 1"), ResultPanel->GetSelectedFocusSlot(), 1);
	ResultPanel->OnKeyDown(DummyGeo, R97MakeKeyDown(EKeys::Gamepad_DPad_Down));
	TestEqual(TEXT("Gamepad DPad Down advances focus slot to 2"), ResultPanel->GetSelectedFocusSlot(), 2);
	ResultPanel->OnKeyDown(DummyGeo, R97MakeKeyDown(EKeys::Gamepad_DPad_Up));
	TestEqual(TEXT("Gamepad DPad Up moves focus slot back to 1"), ResultPanel->GetSelectedFocusSlot(), 1);

	ResultPanel->OnKeyDown(DummyGeo, R97MakeKeyDown(EKeys::Gamepad_FaceButton_Right, true));
	TestFalse(TEXT("Repeat B button does not close result panel"), bResultClosed);
	ResultPanel->OnKeyDown(DummyGeo, R97MakeKeyDown(EKeys::Gamepad_FaceButton_Right, false));
	TestTrue(TEXT("Non-repeat B button closes result panel and acknowledges result"),
		bResultClosed && Fake->GetAcknowledgeCount() == 1 && !Fake->HasUnseenResult());

	// Countdown navigation
	bool bCountdownClosed = false;
	TSharedRef<SPWCountdownPanel> CountdownPanel = SNew(SPWCountdownPanel)
		.BridgeOverride(Fake)
		.OnClose_Lambda([&bCountdownClosed]() { bCountdownClosed = true; });
	CountdownPanel->SlatePrepass(1.0f);

	TestTrue(TEXT("Countdown panel initial focus target is valid"),
		CountdownPanel->GetInitialFocusTarget().IsValid());
	TestEqual(TEXT("Countdown starts at offer index 0"), CountdownPanel->GetSelectedOfferIndex(), 0);
	CountdownPanel->OnKeyDown(DummyGeo, R97MakeKeyDown(EKeys::Gamepad_DPad_Down));
	TestEqual(TEXT("Gamepad DPad Down selects offer index 1"), CountdownPanel->GetSelectedOfferIndex(), 1);
	CountdownPanel->OnKeyDown(DummyGeo, R97MakeKeyDown(EKeys::Escape, false));
	TestTrue(TEXT("Escape closes countdown panel"), bCountdownClosed);

	UPerfectWorldPresentationSettings::Get().SetReducedMotion(bPrevReducedMotion);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPWApFitsAtFourSizesTest,
	"PerfectWorld.AwakeningPanel.FitsAtFourSizes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPWApFitsAtFourSizesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TSharedRef<PWAwakening::FApFakeAwakeningWorld> Fake = PWAwakening::MakeFakeAwakeningWorld();

	const FVector2D kSizes[] = {
		FVector2D(853.0f, 480.0f),
		FVector2D(1280.0f, 720.0f),
		FVector2D(1920.0f / 1.5f, 1080.0f / 1.5f),
		FVector2D(1920.0f, 1080.0f)
	};

	for (const FVector2D& Size : kSizes)
	{
		TSharedRef<SPWAwakeningResultPanel> ResultPanel = SNew(SPWAwakeningResultPanel)
			.BridgeOverride(Fake)
			.ViewportSize(Size);
		ResultPanel->SlatePrepass(1.0f);
		const FVector2D ResultDesired = ResultPanel->GetDesiredSize();
		const float ResultMaxW = SPWAwakeningResultPanel::PanelWidth(Size.X, Size.Y);
		const float ResultMaxH = SPWAwakeningResultPanel::PanelMaxHeight(Size.X, Size.Y);

		TestTrue(
			FString::Printf(TEXT("ResultPanel width %.1f fits <= %.1f at %.0fx%.0f"),
				ResultDesired.X, ResultMaxW, Size.X, Size.Y),
			ResultDesired.X > 0.0f && ResultDesired.X <= ResultMaxW + 1.0f);
		TestTrue(
			FString::Printf(TEXT("ResultPanel height %.1f fits <= %.1f at %.0fx%.0f"),
				ResultDesired.Y, ResultMaxH, Size.X, Size.Y),
			ResultDesired.Y > 0.0f && ResultDesired.Y <= ResultMaxH + 1.0f);

		TSharedRef<SPWCountdownPanel> CountdownPanel = SNew(SPWCountdownPanel)
			.BridgeOverride(Fake)
			.ViewportSize(Size);
		CountdownPanel->SlatePrepass(1.0f);
		const FVector2D CountdownDesired = CountdownPanel->GetDesiredSize();
		const float CountdownMaxW = SPWCountdownPanel::PanelWidth(Size.X, Size.Y);
		const float CountdownMaxH = SPWCountdownPanel::PanelMaxHeight(Size.X, Size.Y);

		TestTrue(
			FString::Printf(TEXT("CountdownPanel width %.1f fits <= %.1f at %.0fx%.0f"),
				CountdownDesired.X, CountdownMaxW, Size.X, Size.Y),
			CountdownDesired.X > 0.0f && CountdownDesired.X <= CountdownMaxW + 1.0f);
		TestTrue(
			FString::Printf(TEXT("CountdownPanel height %.1f fits <= %.1f at %.0fx%.0f"),
				CountdownDesired.Y, CountdownMaxH, Size.X, Size.Y),
			CountdownDesired.Y > 0.0f && CountdownDesired.Y <= CountdownMaxH + 1.0f);
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
