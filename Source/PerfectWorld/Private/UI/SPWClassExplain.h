#pragma once

#include "CoreMinimal.h"
#include "UI/SPWClassLesson.h"

#include "StructureExplain.h"

/**
 * The construct-an-explanation lesson on the shared shell: pick a claim, place up to two pieces of evidence, state a limitation or none.
 * More than one reading can be defended, and the screen never says which: it shows the authored text of claims, evidence and limitations and
 * the core's quotes, and prints only the core's own diagnostics after the lesson resolves. It never reads ExplainClaim::Defensible,
 * ExplainEvidence::Supports/Against or ExplainLimitation::Bounds.
 */
class SPWClassExplain : public SPWClassLesson
{
public:
	SLATE_BEGIN_ARGS(SPWClassExplain) : _Space(FVector2D(1920.0, 1080.0)) {}
		SLATE_ARGUMENT(TSharedPtr<PWClassLesson::IBridge>, Bridge)
		/** The logical size of the screen the panel may use. The host passes the size it already measures. */
		SLATE_ATTRIBUTE(FVector2D, Space)
		/** Back was pressed: leave the screen (the lesson stays open). */
		SLATE_EVENT(FSimpleDelegate, OnClose)
		/** The core accepted Record or Leave early. */
		SLATE_EVENT(FPWClassLessonClosed, OnLessonClosed)
		/** The control that opened the lesson; it gets keyboard focus back after OnClose. Held weakly. */
		SLATE_ARGUMENT(TWeakPtr<SWidget>, ReturnFocus)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** The model paragraph, when the method is unfamiliar (step 1). */
	PerfectWorld::SchoolSessionResult ExecuteModelParagraph();
	void SelectClaim(const FString& ClaimId);
	const FString& GetSelectedClaim() const { return SelectedClaim; }
	PerfectWorld::SchoolSessionResult ExecuteChooseClaim();
	void ToggleEvidence(const FString& EvidenceId);
	bool IsEvidenceSelected(const FString& EvidenceId) const { return SelectedEvidence.Contains(EvidenceId); }
	const TArray<FString>& GetSelectedEvidence() const { return SelectedEvidence; }
	PerfectWorld::SchoolSessionResult ExecutePlaceEvidence();
	void SelectLimitation(const FString& LimitationId);
	const FString& GetSelectedLimitation() const { return SelectedLimitation; }
	/** Names the selected limitation and finishes the lesson. */
	PerfectWorld::SchoolSessionResult ExecutePlaceLimitation();
	/** States the claim without a further limitation (free) and finishes the lesson. */
	PerfectWorld::SchoolSessionResult ExecuteStateNoLimitation();

protected:
	virtual PerfectWorld::SchoolStructure ShellStructure() const override { return PerfectWorld::SchoolStructure::Explain; }
	virtual const TCHAR* ShellTextPrefix() const override { return TEXT("phase2.a2.explain."); }
	virtual int32 ShellGoalCount() const override;
	virtual FText ShellGoalName(int32 Index) const override;
	virtual PerfectWorld::SchoolLessonGoal ShellGoalAt(int32 Index) const override;
	virtual void ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const override;
	virtual PWClassLesson::EMastery ShellMastery(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const override;
	virtual void ShellSessionChanged() override;
	virtual void ShellBuildCase(const TSharedRef<SVerticalBox>& Into) override;
	virtual void ShellBuildPlay(const TSharedRef<SVerticalBox>& Into) override;
	virtual bool ShellDescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const override;

private:
	const PerfectWorld::ExplainCaseDef* ActiveCase() const;
	PerfectWorld::ExplainAction ClaimActionNow() const;
	PerfectWorld::ExplainAction EvidenceActionNow() const;
	PerfectWorld::ExplainAction LimitationActionNow(bool bNone) const;

	FString SelectedClaim;
	TArray<FString> SelectedEvidence;
	FString SelectedLimitation;
};
