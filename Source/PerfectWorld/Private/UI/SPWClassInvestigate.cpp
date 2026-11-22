#include "UI/SPWClassInvestigate.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/SBoxPanel.h"

#include <algorithm>
#include <variant>

// The StructureInvestigate lesson (order A3) on the shared shell. Nothing here decides anything: prices are the core's quotes, and the only
// fault-derived value is the reading of a test the player has already run, which is what running it reveals.

namespace
{
const TCHAR* const GClIKindKeys[] = {TEXT("observe"), TEXT("measure"), TEXT("swap")};
const TCHAR* const GClIGoalKeys[] = {TEXT("inspect"), TEXT("measure"), TEXT("swap_parts")};
const PerfectWorld::InvestigateGoal GClIGoals[] = {PerfectWorld::InvestigateGoal::Inspect, PerfectWorld::InvestigateGoal::Measure,
	PerfectWorld::InvestigateGoal::SwapParts};

FString ClIWord(const std::string_view Name)
{
	return FString(static_cast<int32>(Name.size()), Name.data());
}

bool ClIHas(const std::vector<std::string>& Ids, const std::string& Id)
{
	return std::find(Ids.begin(), Ids.end(), Id) != Ids.end();
}

FText ClICauseName(const PerfectWorld::InvestigateCaseDef& Def, const std::string& Id)
{
	for (const PerfectWorld::InvestigateCause& Cause : Def.Causes)
	{
		if (Cause.Id == Id)
		{
			return PWClassLesson::FromUtf8(Cause.Name);
		}
	}
	return FText::GetEmpty();
}

FText ClIJoin(const TArray<FText>& Parts)
{
	return FText::Join(INVTEXT(", "), Parts);
}

bool ClIUnitsShort(const std::string& Code)
{
	return Code == "structure.units_short";
}
} // namespace

namespace PWClassInvestigate
{
FModel BuildModel(const PWClassLesson::IBridge& Bridge, const PWClassLesson::FFrame& Frame)
{
	using namespace PerfectWorld;
	FModel Out;
	const InvestigateCatalogue* Catalogue = Bridge.GetCatalogues().Structures.Investigate;
	const InvestigateExposure& Exposure = Frame.View.Investigate.Exposure;
	if (Catalogue == nullptr || !Frame.bHasRun || Frame.bOtherLesson || Frame.bReadyToClose || !Exposure.Active())
	{
		return Out;
	}
	const InvestigateCaseDef* Def = Catalogue->FindCase(Exposure.CaseId);
	if (Def == nullptr)
	{
		return Out;
	}
	Out.bActive = true;
	Out.CaseId = PWClassLesson::ToFString(Def->Id);
	Out.CaseName = PWClassLesson::FromUtf8(Def->Name);
	Out.CaseBrief = PWClassLesson::FromUtf8(Def->Brief);
	Out.UnitsLeft = Exposure.UnitsLeft;
	Out.UnitsTotal = Exposure.UnitsTotal;
	Out.Symptom = PWClassLesson::Text(TEXT("phase2.a3.investigate.symptom.") + ClIWord(InvestigateSymptomNames[static_cast<size_t>(Def->Symptom)]));
	Out.Role = PWClassLesson::Text(TEXT("phase2.a3.investigate.role.") + ClIWord(InvestigateRoleNames[static_cast<size_t>(Def->Role)]));
	Out.Scarce = PWClassLesson::Text(TEXT("phase2.a3.investigate.scarce.") + ClIWord(InvestigateScarceNames[static_cast<size_t>(Def->Scarce)]));
	Out.Goal = PWClassLesson::Text(TEXT("phase2.a3.investigate.goal.") + ClIWord(InvestigateGoalNames[static_cast<size_t>(Exposure.Goal)]));
	FText MethodName = PWClassLesson::FromUtf8(Def->MethodId);
	if (const auto* Family = Catalogue->FindFamily(Def->FamilyId))
	{
		for (const StructureMethod& Method : Family->Methods)
		{
			if (Method.Id == Def->MethodId)
			{
				MethodName = PWClassLesson::FromUtf8(Method.Name);
			}
		}
	}
	Out.Method = FText::Format(PWClassLesson::Text(Exposure.MethodKnown ? TEXT("phase2.a3.common.method.known") : TEXT("phase2.a3.common.method.unfamiliar")),
		MethodName);

	const std::vector<std::string> Remaining = InvestigateRemaining(*Def, Exposure.TestsRun);
	Out.Standing = static_cast<int32>(Remaining.size());

	for (const InvestigateTest& Test : Def->Tests)
	{
		FTestRow Row;
		Row.Id = PWClassLesson::ToFString(Test.Id);
		Row.Name = PWClassLesson::FromUtf8(Test.Name);
		Row.Kind = PWClassLesson::Text(TEXT("phase2.a3.investigate.kind.") + FString(GClIKindKeys[static_cast<size_t>(Test.Kind)]));
		TArray<FText> Names;
		for (const std::string& CauseId : Test.Implicates)
		{
			Names.Add(ClICauseName(*Def, CauseId));
		}
		if (!Names.IsEmpty())
		{
			Row.Implicates = PWClassLesson::Text1(TEXT("phase2.a3.investigate.test.implicates"), ClIJoin(Names));
		}
		Row.bRun = ClIHas(Exposure.TestsRun, Test.Id);
		// Only a test that has been run shows what it read; before that the model does not know it.
		Row.bPositive = Row.bRun && InvestigateTestPositive(*Def, Test);
		InvestigateAction Action;
		Action.Kind = InvestigateActionKind::RunTest;
		Action.Test = Test.Id;
		const SchoolLessonQuote Quote = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Action));
		Row.Units = Quote.Units;
		Row.UnitsAfter = Quote.UnitsAfter;
		Row.Issue = Quote.Issue;
		Out.Tests.Add(MoveTemp(Row));
	}

	for (const InvestigateCause& Cause : Def->Causes)
	{
		FCauseRow Row;
		Row.Id = PWClassLesson::ToFString(Cause.Id);
		Row.Name = PWClassLesson::FromUtf8(Cause.Name);
		Row.bStanding = ClIHas(Remaining, Cause.Id);
		Row.bGuess = Row.bStanding && Remaining.size() > 1;
		InvestigateAction Action;
		Action.Kind = InvestigateActionKind::Diagnose;
		Action.Cause = Cause.Id;
		Row.Issue = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Action)).Issue;
		Out.Causes.Add(MoveTemp(Row));
	}

	InvestigateAction Report;
	Report.Kind = InvestigateActionKind::ReportInconclusive;
	Out.ReportIssue = Bridge.QuoteAction(Frame.Session, SchoolLessonAction(Report)).Issue;

	std::vector<std::string> Prefix;
	for (const std::string& RunId : Exposure.TestsRun)
	{
		Prefix.push_back(RunId);
		const auto Found = std::find_if(Def->Tests.begin(), Def->Tests.end(), [&RunId](const InvestigateTest& T) { return T.Id == RunId; });
		if (Found == Def->Tests.end())
		{
			continue;
		}
		const bool bPositive = InvestigateTestPositive(*Def, *Found);
		TArray<FText> Names;
		for (const std::string& Id : InvestigateRemaining(*Def, Prefix))
		{
			Names.Add(ClICauseName(*Def, Id));
		}
		const FText Causes = Names.IsEmpty() ? PWClassLesson::Text(TEXT("phase2.a3.investigate.evidence.causes_none"))
			: PWClassLesson::Text1(TEXT("phase2.a3.investigate.evidence.causes"), ClIJoin(Names));
		Out.Evidence.Add(PWClassLesson::Text3(TEXT("phase2.a3.investigate.evidence"), PWClassLesson::FromUtf8(Found->Name),
			PWClassLesson::Text(bPositive ? TEXT("phase2.a3.investigate.evidence.positive") : TEXT("phase2.a3.investigate.evidence.negative")),
			Causes));
	}
	return Out;
}
} // namespace PWClassInvestigate

void SPWClassInvestigate::Construct(const FArguments& InArgs)
{
	PWClassLesson::FShellArgs Args;
	Args.Bridge = InArgs._Bridge;
	Args.Space = InArgs._Space;
	Args.OnClose = InArgs._OnClose;
	Args.OnLessonClosed = InArgs._OnLessonClosed;
	Args.ReturnFocus = InArgs._ReturnFocus;
	ConstructShell(Args);
}

int32 SPWClassInvestigate::ShellGoalCount() const
{
	return UE_ARRAY_COUNT(GClIGoals);
}

FText SPWClassInvestigate::ShellGoalName(int32 Index) const
{
	return PWClassLesson::Text(TEXT("phase2.a3.investigate.goal.") + FString(GClIGoalKeys[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]));
}

FText SPWClassInvestigate::ShellGoalHint(int32 Index) const
{
	return PWClassLesson::Text(TEXT("phase2.a3.investigate.goal.") + FString(GClIGoalKeys[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]) + TEXT(".hint"));
}

PerfectWorld::SchoolLessonGoal SPWClassInvestigate::ShellGoalAt(int32 Index) const
{
	return PerfectWorld::SchoolLessonGoal(GClIGoals[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]);
}

void SPWClassInvestigate::ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const
{
	const PerfectWorld::InvestigateExposure& Exposure = View.Investigate.Exposure;
	bActive = Exposure.Active();
	Left = Exposure.UnitsLeft;
	Total = Exposure.UnitsTotal;
}

PWClassLesson::EMastery SPWClassInvestigate::ShellMastery(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const
{
	const PerfectWorld::StructureFamilyRecord* Record = View.Investigate.FindFamily(FamilyId);
	if (Record == nullptr)
	{
		return PWClassLesson::EMastery::Unseen;
	}
	if (Record->Secure)
	{
		return PWClassLesson::EMastery::Secure;
	}
	return Record->Exposures > 0 ? PWClassLesson::EMastery::Practised : PWClassLesson::EMastery::Unseen;
}

void SPWClassInvestigate::ShellFrameChanged()
{
	Model = HasBridge() ? PWClassInvestigate::BuildModel(GetBridge(), GetFrame()) : PWClassInvestigate::FModel();
}

void SPWClassInvestigate::ShellBuildCase(const TSharedRef<SVerticalBox>& Into)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	if (!Model.bActive)
	{
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.no_exercise")), false);
		return;
	}
	AddHeading(Into, Model.CaseName);
	AddNote(Into, Model.CaseBrief, false);
	for (const FText& Badge : {Model.Symptom, Model.Role, Model.Scarce, Model.Goal, Model.Method})
	{
		AddLine(Into, Badge, Style.TextSecondary);
	}
	AddUnitsMeter(Into);
	if (!Model.Evidence.IsEmpty())
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.investigate.evidence_heading")));
		for (const FText& Line : Model.Evidence)
		{
			AddNote(Into, Line, false);
		}
	}
}

void SPWClassInvestigate::ShellBuildPlay(const TSharedRef<SVerticalBox>& Into)
{
	if (!Model.bActive)
	{
		return;
	}
	AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.investigate.beat.isolate")));
	for (const PWClassInvestigate::FTestRow& Row : Model.Tests)
	{
		const FString Id = Row.Id;
		FText Detail = FText::Format(PWClassLesson::Text(TEXT("phase2.a3.investigate.test.kind_line")), Row.Kind, Row.Implicates);
		FText Cost = PWClassLesson::CostText(Row.Units);
		ECostTone Tone = Row.Units == 0 ? ECostTone::Free : ECostTone::Plain;
		bool bEnabled = Row.Issue.Ok();
		if (Row.bRun)
		{
			Detail = PWClassLesson::Text(Row.bPositive ? TEXT("phase2.a3.investigate.test.done_positive") : TEXT("phase2.a3.investigate.test.done_negative"));
			Cost = FText::GetEmpty();
			Tone = ECostTone::Plain;
			bEnabled = false;
		}
		else if (ClIUnitsShort(Row.Issue.Code))
		{
			Cost = FText::Format(PWClassLesson::Text(TEXT("phase2.a3.common.cost.short")), PWClassLesson::Number(Row.Units), PWClassLesson::Number(Model.UnitsLeft));
			Tone = ECostTone::Short;
		}
		else if (!Row.Issue.Ok())
		{
			// Any other refusal is the core's code, in words.
			Detail = FText::Format(INVTEXT("{0} {1}"), Detail, PWClassLesson::IssueText(Row.Issue));
			Cost = FText::GetEmpty();
		}
		AddActionRow(Into, TEXT("test.") + Id, Row.Name, Detail, Cost, Tone, bEnabled, [this, Id]() { ExecuteRunTest(Id); });
	}

	AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a3.investigate.beat.conclude")));
	for (const PWClassInvestigate::FCauseRow& Row : Model.Causes)
	{
		const FString Id = Row.Id;
		const FText Detail = !Row.bStanding ? PWClassLesson::Text(TEXT("phase2.a3.investigate.cause.ruled_out"))
			: (Row.bGuess ? PWClassLesson::Text(TEXT("phase2.a3.investigate.cause.guess")) : PWClassLesson::Text(TEXT("phase2.a3.investigate.cause.standing")));
		AddActionRow(Into, TEXT("diagnose.") + Id, PWClassLesson::Text1(TEXT("phase2.a3.investigate.cause.action"), Row.Name), Detail,
			PWClassLesson::CostText(0), ECostTone::Free, Row.Issue.Ok(), [this, Id]() { ExecuteDiagnose(Id); });
	}
	AddActionRow(Into, TEXT("report"), PWClassLesson::Text(TEXT("phase2.a3.investigate.report.action")),
		Model.Standing == 1 ? PWClassLesson::Text(TEXT("phase2.a3.investigate.report.isolated")) : PWClassLesson::Text(TEXT("phase2.a3.investigate.report.hint")),
		PWClassLesson::CostText(0), ECostTone::Free, Model.ReportIssue.Ok(), [this]() { ExecuteReportInconclusive(); });
	AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.leave_note")));
}

bool SPWClassInvestigate::ShellDescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const
{
	const PerfectWorld::StructureOutcome* Base = nullptr;
	if (const auto* Result = std::get_if<PerfectWorld::InvestigateOutcome>(&Outcome))
	{
		Base = Result;
		FText Diagnosed = FText::GetEmpty();
		if (HasBridge() && GetBridge().GetCatalogues().Structures.Investigate != nullptr)
		{
			if (const PerfectWorld::InvestigateCaseDef* Def = GetBridge().GetCatalogues().Structures.Investigate->FindCase(Result->CaseId))
			{
				Diagnosed = ClICauseName(*Def, Result->Diagnosed);
			}
		}
		OutFacts.Detail.Add(Result->Diagnosed.empty() ? PWClassLesson::Text(TEXT("phase2.a3.investigate.outcome.reported"))
			: PWClassLesson::Text1(TEXT("phase2.a3.investigate.outcome.diagnosed"), Diagnosed));
		OutFacts.Detail.Add(PWClassLesson::Text3(TEXT("phase2.a3.investigate.outcome.tests"), PWClassLesson::Number(Result->TestsRun),
			PWClassLesson::Number(Result->RedundantTests), PWClassLesson::Number(Result->RemainingCauses)));
	}
	else if (const auto* Plain = std::get_if<PerfectWorld::StructureOutcome>(&Outcome))
	{
		Base = Plain;
	}
	if (Base == nullptr)
	{
		return false;
	}
	OutFacts.Grade = Base->Grade;
	OutFacts.bForeground = Base->Foreground;
	OutFacts.LearningPermille = Base->LearningPermille;
	OutFacts.UnitsSpent = Base->UnitsSpent;
	OutFacts.bBecameSecure = Base->BecameSecure;
	const struct
	{
		uint8 Bit;
		const TCHAR* Key;
	} Failures[] = {
		{PerfectWorld::InvestigateFailWrongCause, TEXT("phase2.a3.investigate.fail.wrong_cause")},
		{PerfectWorld::InvestigateFailUnsupported, TEXT("phase2.a3.investigate.fail.unsupported")},
		{PerfectWorld::InvestigateFailRedundant, TEXT("phase2.a3.investigate.fail.redundant")},
		{PerfectWorld::InvestigateFailAbandoned, TEXT("phase2.a3.investigate.fail.abandoned")}};
	for (const auto& Failure : Failures)
	{
		if ((Base->Failures & Failure.Bit) != 0)
		{
			OutFacts.Diagnostics.Add(PWClassLesson::Text(Failure.Key));
		}
	}
	return true;
}

PerfectWorld::SchoolSessionResult SPWClassInvestigate::ExecuteRunTest(const FString& TestId)
{
	PerfectWorld::InvestigateAction Action;
	Action.Kind = PerfectWorld::InvestigateActionKind::RunTest;
	Action.Test = PWClassLesson::ToUtf8(TestId);
	FText Name = FText::GetEmpty();
	for (const PWClassInvestigate::FTestRow& Row : Model.Tests)
	{
		if (Row.Id == TestId)
		{
			Name = Row.Name;
		}
	}
	return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassLesson::Text1(TEXT("phase2.a3.investigate.test.ran"), Name));
}

PerfectWorld::SchoolSessionResult SPWClassInvestigate::ExecuteDiagnose(const FString& CauseId)
{
	PerfectWorld::InvestigateAction Action;
	Action.Kind = PerfectWorld::InvestigateActionKind::Diagnose;
	Action.Cause = PWClassLesson::ToUtf8(CauseId);
	return ChooseAction(PerfectWorld::SchoolLessonAction(Action), FText::GetEmpty());
}

PerfectWorld::SchoolSessionResult SPWClassInvestigate::ExecuteReportInconclusive()
{
	PerfectWorld::InvestigateAction Action;
	Action.Kind = PerfectWorld::InvestigateActionKind::ReportInconclusive;
	return ChooseAction(PerfectWorld::SchoolLessonAction(Action), FText::GetEmpty());
}
