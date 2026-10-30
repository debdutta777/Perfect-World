#pragma once

#include "CoreMinimal.h"
#include "UI/SPWClassLesson.h"

#include "StructureModel.h"

/**
 * The model-a-system lesson on the shared shell (SPWClassLesson.h): quantities and links, the discrepancy check, the claim. It reads the
 * world only through PWClassLesson::IBridge and never reads ModelLink::Sound or ModelCaseDef::Fault; the only solution-adjacent query is
 * the public ModelExplanationOwed rule, which only pre-ticks the explanation toggle.
 */
class SPWClassModel : public SPWClassLesson
{
public:
	SLATE_BEGIN_ARGS(SPWClassModel) : _Space(FVector2D(1920.0, 1080.0)) {}
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

	/** Step 1, when the method is unfamiliar: the worked example. */
	PerfectWorld::SchoolSessionResult ExecuteWorkedExample();
	/** Selecting only changes what the next command will say; the screen rebuilds. */
	void ToggleLink(const FString& LinkId);
	bool IsLinkSelected(const FString& LinkId) const { return SelectedLinks.Contains(LinkId); }
	const TArray<FString>& GetSelectedLinks() const { return SelectedLinks; }
	PerfectWorld::SchoolSessionResult ExecuteCommitModel();
	/** Step 2. */
	void SelectInvestigation(PerfectWorld::ModelInvestigation InInvestigation);
	PerfectWorld::ModelInvestigation GetSelectedInvestigation() const { return SelectedInvestigation; }
	PerfectWorld::SchoolSessionResult ExecuteCommitInvestigation();
	/** Step 3. */
	void SelectClaim(PerfectWorld::ModelClaim InClaim);
	bool HasSelectedClaim() const { return bClaimChosen; }
	PerfectWorld::ModelClaim GetSelectedClaim() const { return SelectedClaim; }
	void SetExplain(bool bInExplain);
	bool GetExplain() const { return bExplain; }
	PerfectWorld::SchoolSessionResult ExecutePresentClaim();

protected:
	virtual PerfectWorld::SchoolStructure ShellStructure() const override { return PerfectWorld::SchoolStructure::Model; }
	virtual const TCHAR* ShellTextPrefix() const override { return TEXT("phase2.a2.model."); }
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
	const PerfectWorld::ModelCaseDef* ActiveCase() const;
	PerfectWorld::ModelAction CommitAction() const;
	PerfectWorld::ModelAction InvestigateActionNow() const;
	PerfectWorld::ModelAction ClaimAction() const;

	TArray<FString> SelectedLinks;
	PerfectWorld::ModelInvestigation SelectedInvestigation = PerfectWorld::ModelInvestigation::None;
	PerfectWorld::ModelClaim SelectedClaim = PerfectWorld::ModelClaim::BoundedRange;
	bool bClaimChosen = false;
	bool bExplain = false;
	bool bExplainTouched = false;
};
