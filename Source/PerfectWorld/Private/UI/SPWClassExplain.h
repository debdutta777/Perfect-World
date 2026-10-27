#pragma once

#include "CoreMinimal.h"
#include "UI/SPWClassModel.h"

/**
 * The construct-an-explanation lesson (order A2): pick a claim, place up to two pieces of evidence, state a limitation or none. It
 * shares the chrome, words and bridge of SPWClassModel.h. More than one reading can be defended, and the screen never says which:
 * it shows the authored text of claims, evidence and limitations and the core's quotes, and prints only the core's own diagnostics
 * after the lesson resolves. It never reads ExplainClaim::Defensible, ExplainEvidence::Supports/Against or ExplainLimitation::Bounds.
 */
class SPWClassExplain : public SPWClassLessonBase
{
public:
	SLATE_BEGIN_ARGS(SPWClassExplain) : _Space(FVector2D(1920.0, 1080.0)) {}
		SLATE_ARGUMENT(TSharedPtr<PWClassLesson::IBridge>, Bridge)
		/** The logical size of the screen the panel may use. The host passes the size it already measures. */
		SLATE_ATTRIBUTE(FVector2D, Space)
		/** The control that opened the lesson; it gets keyboard focus back when the screen closes. Held weakly. */
		SLATE_ARGUMENT(TWeakPtr<SWidget>, ReturnFocusTarget)
		SLATE_EVENT(FSimpleDelegate, OnClosed)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SelectGoal(PerfectWorld::ExplainGoal InGoal);
	PerfectWorld::ExplainGoal GetSelectedGoal() const { return SelectedGoal; }

	/** The model paragraph, when the method is unfamiliar (beat 1). */
	PerfectWorld::SchoolSessionResult TriggerModelParagraph();
	void SelectClaim(const FString& ClaimId);
	const FString& GetSelectedClaim() const { return SelectedClaim; }
	PerfectWorld::SchoolSessionResult TriggerChooseClaim();
	void ToggleEvidence(const FString& EvidenceId);
	bool IsEvidenceSelected(const FString& EvidenceId) const { return SelectedEvidence.Contains(EvidenceId); }
	const TArray<FString>& GetSelectedEvidence() const { return SelectedEvidence; }
	PerfectWorld::SchoolSessionResult TriggerPlaceEvidence();
	void SelectLimitation(const FString& LimitationId);
	const FString& GetSelectedLimitation() const { return SelectedLimitation; }
	/** Names the selected limitation and finishes the lesson. */
	PerfectWorld::SchoolSessionResult TriggerPlaceLimitation();
	/** States the claim without an additional limitation (free) and finishes the lesson. */
	PerfectWorld::SchoolSessionResult TriggerStateNoLimitation();

protected:
	virtual PerfectWorld::SchoolLessonGoal CurrentGoal() const override { return SelectedGoal; }
	virtual void ResetSelections() override;
	virtual const TCHAR* TextPrefix() const override { return TEXT("phase2.a2.explain."); }
	virtual PerfectWorld::SchoolStructure Structure() const override { return PerfectWorld::SchoolStructure::Explain; }
	virtual PWClassLesson::EMastery MasteryOf(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const override;
	virtual bool ExposureActive(const PerfectWorld::SchoolLessonView& View) const override { return View.Explain.Exposure.Active(); }
	virtual int32 UnitsLeft(const PerfectWorld::SchoolLessonView& View) const override { return View.Explain.Exposure.UnitsLeft; }
	virtual int32 UnitsTotal(const PerfectWorld::SchoolLessonView& View) const override { return View.Explain.Exposure.UnitsTotal; }
	virtual TSharedRef<SWidget> BuildGoalPicker() override;
	virtual TSharedRef<SWidget> BuildCasePane(const PerfectWorld::SchoolLessonView& View) override;
	virtual TSharedRef<SWidget> BuildBeatPane(const PerfectWorld::SchoolLessonView& View) override;
	virtual bool DescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const override;

private:
	const PerfectWorld::ExplainCaseDef* ActiveCase(const PerfectWorld::SchoolLessonView& View) const;
	PerfectWorld::ExplainAction ClaimActionNow() const;
	PerfectWorld::ExplainAction EvidenceActionNow() const;
	PerfectWorld::ExplainAction LimitationActionNow(bool bNone) const;

	PerfectWorld::ExplainGoal SelectedGoal = PerfectWorld::ExplainGoal::ArgueClaim;
	FString SelectedClaim;
	TArray<FString> SelectedEvidence;
	FString SelectedLimitation;
};
