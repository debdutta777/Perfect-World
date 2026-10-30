#pragma once

#include "CoreMinimal.h"
#include "UI/SPWClassLesson.h"

namespace PWClassInvestigate
{
/** One diagnostic test as the player sees it. The result is only meaningful once the test has been run. */
struct FTestRow
{
	FString Id;
	FText Name;
	FText Kind;
	/** The causes the test would implicate, in words. */
	FText Implicates;
	/** The core's quote for this decision, right now. */
	int32 Units = 0;
	int32 UnitsAfter = 0;
	PerfectWorld::SchoolIssue Issue;
	bool bRun = false;
	bool bPositive = false;
};

/** One candidate cause. The real fault is never part of this row. */
struct FCauseRow
{
	FString Id;
	FText Name;
	bool bStanding = true;
	/** True when naming this cause now would be a guess (other causes still stand). */
	bool bGuess = false;
	PerfectWorld::SchoolIssue Issue;
};

struct FModel
{
	bool bActive = false;
	FString CaseId;
	FText CaseName;
	FText CaseBrief;
	FText Symptom;
	FText Role;
	FText Scarce;
	FText Goal;
	FText Method;
	int32 UnitsLeft = 0;
	int32 UnitsTotal = 0;
	int32 Standing = 0;
	TArray<FTestRow> Tests;
	TArray<FCauseRow> Causes;
	/** What the tests run so far have shown, in the order they were run. */
	TArray<FText> Evidence;
	PerfectWorld::SchoolIssue ReportIssue;
};

/** Reads the running exposure through the bridge's quotes. An idle or foreign run gives an inactive model. */
FModel BuildModel(const PWClassLesson::IBridge& Bridge, const PWClassLesson::FFrame& Frame);
} // namespace PWClassInvestigate

/**
 * StructureInvestigate lesson on the shared shell: a loop of diagnostic tests with different prices and adaptive evidence, then one
 * conclusion. The tests show which causes they implicate and a result only after they were run; a free honest report is always available.
 * Which cause is the real fault is never shown.
 */
class SPWClassInvestigate : public SPWClassLesson
{
public:
	SLATE_BEGIN_ARGS(SPWClassInvestigate) : _Space(FVector2D(1920.0, 1080.0)) {}
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

	const PWClassInvestigate::FModel& GetModel() const { return Model; }
	PerfectWorld::SchoolSessionResult ExecuteRunTest(const FString& TestId);
	PerfectWorld::SchoolSessionResult ExecuteDiagnose(const FString& CauseId);
	PerfectWorld::SchoolSessionResult ExecuteReportInconclusive();

protected:
	virtual PerfectWorld::SchoolStructure ShellStructure() const override { return PerfectWorld::SchoolStructure::Investigate; }
	virtual const TCHAR* ShellTextPrefix() const override { return TEXT("phase2.a3.investigate."); }
	virtual int32 ShellGoalCount() const override;
	virtual FText ShellGoalName(int32 Index) const override;
	virtual FText ShellGoalHint(int32 Index) const override;
	virtual PerfectWorld::SchoolLessonGoal ShellGoalAt(int32 Index) const override;
	virtual void ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const override;
	virtual PWClassLesson::EMastery ShellMastery(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const override;
	virtual void ShellFrameChanged() override;
	virtual void ShellBuildCase(const TSharedRef<SVerticalBox>& Into) override;
	virtual void ShellBuildPlay(const TSharedRef<SVerticalBox>& Into) override;
	virtual bool ShellDescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const override;

private:
	PWClassInvestigate::FModel Model;
};
