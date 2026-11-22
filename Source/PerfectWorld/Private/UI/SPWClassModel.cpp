#include "UI/SPWClassModel.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/SBoxPanel.h"

#include <variant>

// The model-a-system lesson (order A2) on the shared shell. Words are in the shared table (phase2.a2.model.*).

namespace
{
FString ClMKey(const TCHAR* Group, std::string_view Name)
{
	return FString(TEXT("phase2.a2.model.")) + Group + PWClassLesson::ToFString(std::string(Name));
}

FText ClMQuantityName(const PerfectWorld::ModelCaseDef& Def, const std::string& Id)
{
	for (const PerfectWorld::ModelQuantity& Quantity : Def.Quantities)
	{
		if (Quantity.Id == Id)
		{
			return PWClassLesson::FromUtf8(Quantity.Name);
		}
	}
	return PWClassLesson::Text(TEXT("phase2.a2.model.quantity.other"));
}

PerfectWorld::StructureGrade ClMGrade(PerfectWorld::ModelGrade Grade)
{
	switch (Grade)
	{
	case PerfectWorld::ModelGrade::Sound: return PerfectWorld::StructureGrade::Sound;
	case PerfectWorld::ModelGrade::Partial: return PerfectWorld::StructureGrade::Partial;
	default: return PerfectWorld::StructureGrade::Unsound;
	}
}

struct FClMFail
{
	uint8 Bit;
	const TCHAR* Key;
};

const FClMFail GClMFails[] = {
	{PerfectWorld::ModelFailWrongModel, TEXT("phase2.a2.model.fail.wrong_model")},
	{PerfectWorld::ModelFailUnsupported, TEXT("phase2.a2.model.fail.unsupported")},
	{PerfectWorld::ModelFailWastedTime, TEXT("phase2.a2.model.fail.wasted_time")},
	{PerfectWorld::ModelFailUnclear, TEXT("phase2.a2.model.fail.unclear")},
};

const PerfectWorld::ModelInvestigation GClMInvestigations[] = {PerfectWorld::ModelInvestigation::AcceptReadings,
	PerfectWorld::ModelInvestigation::CheckTimes, PerfectWorld::ModelInvestigation::LookForFlow, PerfectWorld::ModelInvestigation::CompareMarkings};
const PerfectWorld::ModelClaim GClMClaims[] = {PerfectWorld::ModelClaim::Estimate, PerfectWorld::ModelClaim::BoundedRange,
	PerfectWorld::ModelClaim::NeedMoreMeasurement};
const PerfectWorld::ModelGoal GClMGoals[] = {PerfectWorld::ModelGoal::UnderstandModel, PerfectWorld::ModelGoal::TestMeasurements,
	PerfectWorld::ModelGoal::ExplainToGroup};

PerfectWorld::SchoolSessionResult ClMRefusal(const char* Code)
{
	PerfectWorld::SchoolSessionResult Result;
	Result.Issue.Code = Code;
	return Result;
}
} // namespace

void SPWClassModel::Construct(const FArguments& InArgs)
{
	PWClassLesson::FShellArgs Args;
	Args.Bridge = InArgs._Bridge;
	Args.Space = InArgs._Space;
	Args.OnClose = InArgs._OnClose;
	Args.OnLessonClosed = InArgs._OnLessonClosed;
	Args.ReturnFocus = InArgs._ReturnFocus;
	ConstructShell(Args);
}

void SPWClassModel::ShellSessionChanged()
{
	SelectedLinks.Reset();
	SelectedInvestigation = PerfectWorld::ModelInvestigation::None;
	SelectedClaim = PerfectWorld::ModelClaim::BoundedRange;
	bClaimChosen = false;
	bExplain = false;
	bExplainTouched = false;
}

int32 SPWClassModel::ShellGoalCount() const
{
	return UE_ARRAY_COUNT(GClMGoals);
}

FText SPWClassModel::ShellGoalName(int32 Index) const
{
	const PerfectWorld::ModelGoal Goal = GClMGoals[FMath::Clamp(Index, 0, ShellGoalCount() - 1)];
	return PWClassLesson::Text(ClMKey(TEXT("goal."), PerfectWorld::ModelNameOf(PerfectWorld::ModelGoalNames, Goal)));
}

PerfectWorld::SchoolLessonGoal SPWClassModel::ShellGoalAt(int32 Index) const
{
	return PerfectWorld::SchoolLessonGoal(GClMGoals[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]);
}

void SPWClassModel::ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const
{
	bActive = View.Model.Exposure.Active();
	Left = View.Model.Exposure.UnitsLeft;
	Total = View.Model.Exposure.UnitsTotal;
}

PWClassLesson::EMastery SPWClassModel::ShellMastery(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const
{
	const PerfectWorld::ModelFamilyRecord* Record = View.Model.FindFamily(FamilyId);
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

const PerfectWorld::ModelCaseDef* SPWClassModel::ActiveCase() const
{
	if (!HasBridge() || GetBridge().GetCatalogues().Structures.Model == nullptr)
	{
		return nullptr;
	}
	return GetBridge().GetCatalogues().Structures.Model->FindCase(GetFrame().View.Model.Exposure.CaseId);
}

// ---- Selections and commands ---------------------------------------------------------------------------------------------

void SPWClassModel::ToggleLink(const FString& LinkId)
{
	if (SelectedLinks.Contains(LinkId))
	{
		SelectedLinks.Remove(LinkId);
	}
	else if (SelectedLinks.Num() < static_cast<int32>(PerfectWorld::ModelMaxLinks))
	{
		SelectedLinks.Add(LinkId);
	}
	RefreshModel();
}

void SPWClassModel::SelectInvestigation(PerfectWorld::ModelInvestigation InInvestigation)
{
	SelectedInvestigation = InInvestigation;
	RefreshModel();
}

void SPWClassModel::SelectClaim(PerfectWorld::ModelClaim InClaim)
{
	SelectedClaim = InClaim;
	bClaimChosen = true;
	RefreshModel();
}

void SPWClassModel::SetExplain(bool bInExplain)
{
	bExplain = bInExplain;
	bExplainTouched = true;
	RefreshModel();
}

PerfectWorld::ModelAction SPWClassModel::CommitAction() const
{
	PerfectWorld::ModelAction Action;
	Action.Kind = PerfectWorld::ModelActionKind::CommitModel;
	for (const FString& Link : SelectedLinks)
	{
		Action.Links.push_back(PWClassLesson::ToUtf8(Link));
	}
	return Action;
}

PerfectWorld::ModelAction SPWClassModel::InvestigateActionNow() const
{
	PerfectWorld::ModelAction Action;
	Action.Kind = PerfectWorld::ModelActionKind::Investigate;
	Action.Investigation = SelectedInvestigation;
	return Action;
}

PerfectWorld::ModelAction SPWClassModel::ClaimAction() const
{
	PerfectWorld::ModelAction Action;
	Action.Kind = PerfectWorld::ModelActionKind::PresentClaim;
	Action.Claim = SelectedClaim;
	Action.Explain = bExplain;
	return Action;
}

PerfectWorld::SchoolSessionResult SPWClassModel::ExecuteWorkedExample()
{
	PerfectWorld::ModelAction Action;
	Action.Kind = PerfectWorld::ModelActionKind::WorkedExample;
	return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassLesson::Text(TEXT("phase2.a2.model.status.example")));
}

PerfectWorld::SchoolSessionResult SPWClassModel::ExecuteCommitModel()
{
	return ChooseAction(PerfectWorld::SchoolLessonAction(CommitAction()), PWClassLesson::Text(TEXT("phase2.a2.model.status.model")));
}

PerfectWorld::SchoolSessionResult SPWClassModel::ExecuteCommitInvestigation()
{
	if (SelectedInvestigation == PerfectWorld::ModelInvestigation::None)
	{
		// Nothing is sent: no check has been chosen.
		ShowBanner(PWClassLesson::Text(TEXT("phase2.a2.model.status.choose_investigation")), true);
		return ClMRefusal("model.bad_model");
	}
	return ChooseAction(PerfectWorld::SchoolLessonAction(InvestigateActionNow()), PWClassLesson::Text(TEXT("phase2.a2.model.status.investigation")));
}

PerfectWorld::SchoolSessionResult SPWClassModel::ExecutePresentClaim()
{
	if (!bClaimChosen)
	{
		ShowBanner(PWClassLesson::Text(TEXT("phase2.a2.model.status.choose_claim")), true);
		return ClMRefusal("model.bad_model");
	}
	return ChooseAction(PerfectWorld::SchoolLessonAction(ClaimAction()), PWClassLesson::Text(TEXT("phase2.a2.model.status.claim")));
}

// ---- The case and the steps -------------------------------------------------------------------------------------------------

void SPWClassModel::ShellBuildCase(const TSharedRef<SVerticalBox>& Into)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::ModelCaseDef* Def = ActiveCase();
	if (Def == nullptr)
	{
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a2.common.case_missing")), false);
		return;
	}
	const PerfectWorld::ModelExposure& Exp = GetFrame().View.Model.Exposure;
	AddHeading(Into, PWClassLesson::FromUtf8(Def->Name));
	AddNote(Into, PWClassLesson::FromUtf8(Def->Brief), false);
	AddLine(Into, PWClassLesson::Text(ClMKey(TEXT("unknown."), PerfectWorld::ModelNameOf(PerfectWorld::ModelUnknownNames, Def->Unknown))), Style.TextSecondary);
	AddLine(Into, PWClassLesson::Text(ClMKey(TEXT("role."), PerfectWorld::ModelNameOf(PerfectWorld::ModelRoleNames, Def->Role))), Style.TextSecondary);
	AddLine(Into, PWClassLesson::Text(ClMKey(TEXT("scarce."), PerfectWorld::ModelNameOf(PerfectWorld::ModelScarceNames, Def->Scarce))), Style.TextSecondary);
	AddLine(Into, PWClassLesson::Text(Exp.MethodKnown ? TEXT("phase2.a2.model.method.known")
		: Exp.ExampleTaken ? TEXT("phase2.a2.model.method.example_taken") : TEXT("phase2.a2.model.method.unfamiliar")), Style.TextSecondary);
	AddUnitsMeter(Into);
	AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.model.quantities_heading")));
	for (const PerfectWorld::ModelQuantity& Quantity : Def->Quantities)
	{
		Into->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				PWUI::MakeText(PWClassLesson::FromUtf8(Quantity.Name), TextSize(16, 15), Style.TextPrimary, false, true)
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10.0f, 0.0f, 0.0f, 0.0f)
			[
				PWUI::MakeText(PWClassLesson::Text(Quantity.Given ? TEXT("phase2.a2.model.quantity.given") : TEXT("phase2.a2.model.quantity.unknown")),
					PWUI::ETextRole::Meta, Quantity.Given ? Style.TextSecondary : Style.Accent, true)
			]
		];
	}
}

void SPWClassModel::ShellBuildPlay(const TSharedRef<SVerticalBox>& Into)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::ModelCaseDef* Def = ActiveCase();
	if (Def == nullptr)
	{
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.no_exercise")), false);
		return;
	}
	const PerfectWorld::ModelExposure& Exp = GetFrame().View.Model.Exposure;
	if (Exp.Beat == PerfectWorld::ModelBeat::BuildModel)
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.model.beat.build_model")));
		if (!Exp.MethodKnown && !Exp.ExampleTaken)
		{
			PerfectWorld::ModelAction Example;
			Example.Kind = PerfectWorld::ModelActionKind::WorkedExample;
			AddQuotedRow(Into, TEXT("example"), PWClassLesson::Text(TEXT("phase2.a2.model.action.worked_example")), FText::GetEmpty(),
				Quote(PerfectWorld::SchoolLessonAction(Example)), [this]() { ExecuteWorkedExample(); });
		}
		AddLine(Into, PWClassLesson::Text(TEXT("phase2.a2.model.links_hint")), Style.TextSecondary);
		for (const PerfectWorld::ModelLink& Link : Def->Links)
		{
			const FString LinkId = PWClassLesson::ToFString(Link.Id);
			AddActionRow(Into, TEXT("link.") + LinkId,
				PWClassLesson::Text2(TEXT("phase2.a2.model.link"), ClMQuantityName(*Def, Link.From), ClMQuantityName(*Def, Link.To)),
				FText::GetEmpty(), FText::GetEmpty(), ECostTone::Plain, true, [this, LinkId]() { ToggleLink(LinkId); }, IsLinkSelected(LinkId));
		}
		AddQuotedRow(Into, TEXT("commit_model"), PWClassLesson::Text(TEXT("phase2.a2.model.action.commit_model")), FText::GetEmpty(),
			Quote(PerfectWorld::SchoolLessonAction(CommitAction())), [this]() { ExecuteCommitModel(); }, false, !SelectedLinks.IsEmpty(),
			PWClassLesson::IssueText(std::string("model.bad_model")));
	}
	else if (Exp.Beat == PerfectWorld::ModelBeat::ResolveDiscrepancy)
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.model.beat.resolve_discrepancy")));
		AddLine(Into, PWClassLesson::Text(TEXT("phase2.a2.model.investigate_hint")), Style.TextSecondary);
		for (const PerfectWorld::ModelInvestigation Investigation : GClMInvestigations)
		{
			PerfectWorld::ModelAction Probe;
			Probe.Kind = PerfectWorld::ModelActionKind::Investigate;
			Probe.Investigation = Investigation;
			const std::string_view Name = PerfectWorld::ModelNameOf(PerfectWorld::ModelInvestigationNames, Investigation);
			AddQuotedRow(Into, TEXT("investigate.") + PWClassLesson::ToFString(std::string(Name)),
				PWClassLesson::Text(ClMKey(TEXT("action.investigate."), Name)), FText::GetEmpty(), Quote(PerfectWorld::SchoolLessonAction(Probe)),
				[this, Investigation]() { SelectInvestigation(Investigation); }, SelectedInvestigation == Investigation);
		}
		AddQuotedRow(Into, TEXT("commit_investigation"), PWClassLesson::Text(TEXT("phase2.a2.model.action.commit_investigation")), FText::GetEmpty(),
			Quote(PerfectWorld::SchoolLessonAction(InvestigateActionNow())), [this]() { ExecuteCommitInvestigation(); }, false,
			SelectedInvestigation != PerfectWorld::ModelInvestigation::None, PWClassLesson::Text(TEXT("phase2.a2.model.status.choose_investigation")));
	}
	else
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.model.beat.present_claim")));
		for (const PerfectWorld::ModelClaim Claim : GClMClaims)
		{
			const std::string_view Name = PerfectWorld::ModelNameOf(PerfectWorld::ModelClaimNames, Claim);
			AddActionRow(Into, TEXT("claim.") + PWClassLesson::ToFString(std::string(Name)),
				PWClassLesson::Text(ClMKey(TEXT("action.claim."), Name)), FText::GetEmpty(), FText::GetEmpty(), ECostTone::Plain, true,
				[this, Claim]() { SelectClaim(Claim); }, bClaimChosen && SelectedClaim == Claim);
		}
		// The group explanation: owed by the seat or the goal, and the screen only says so. The core decides the consequence.
		const bool bOwed = PerfectWorld::ModelExplanationOwed(*Def, Exp.Goal);
		if (!bExplainTouched)
		{
			bExplain = bOwed;
		}
		PerfectWorld::ModelAction WithExplanation = ClaimAction();
		WithExplanation.Explain = true;
		AddActionRow(Into, TEXT("explain"), PWClassLesson::Text(TEXT("phase2.a2.model.action.explain_toggle")),
			PWClassLesson::Text(bOwed ? TEXT("phase2.a2.model.action.explain_owed") : TEXT("phase2.a2.model.action.explain_optional")),
			PWClassLesson::PriceText(Quote(PerfectWorld::SchoolLessonAction(WithExplanation)), GetFrame().UnitsLeft), ECostTone::Plain, true,
			[this]() { SetExplain(!bExplain); }, bExplain);
		AddQuotedRow(Into, TEXT("submit_claim"), PWClassLesson::Text(TEXT("phase2.a2.model.action.submit_claim")), FText::GetEmpty(),
			Quote(PerfectWorld::SchoolLessonAction(ClaimAction())), [this]() { ExecutePresentClaim(); }, false, bClaimChosen,
			PWClassLesson::Text(TEXT("phase2.a2.model.status.choose_claim")));
	}
	AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.leave_note")));
}

bool SPWClassModel::ShellDescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const
{
	const auto AddFailures = [&OutFacts](uint8 Failures)
	{
		for (const FClMFail& Fail : GClMFails)
		{
			if ((Failures & Fail.Bit) != 0)
			{
				OutFacts.Diagnostics.Add(PWClassLesson::Text(Fail.Key));
			}
		}
	};
	if (const PerfectWorld::ModelOutcome* Model = std::get_if<PerfectWorld::ModelOutcome>(&Outcome))
	{
		OutFacts.Grade = ClMGrade(Model->Grade);
		OutFacts.bForeground = Model->Foreground;
		OutFacts.LearningPermille = Model->LearningPermille;
		OutFacts.UnitsSpent = Model->UnitsSpent;
		OutFacts.bBecameSecure = Model->BecameSecure;
		if (Model->MissingLinks > 0)
		{
			OutFacts.Detail.Add(PWClassLesson::Text1(TEXT("phase2.a2.model.outcome.missing_links"), PWClassLesson::Number(Model->MissingLinks)));
		}
		if (Model->ExtraLinks > 0)
		{
			OutFacts.Detail.Add(PWClassLesson::Text1(TEXT("phase2.a2.model.outcome.extra_links"), PWClassLesson::Number(Model->ExtraLinks)));
		}
		AddFailures(Model->Failures);
		return true;
	}
	if (const PerfectWorld::StructureOutcome* Generic = std::get_if<PerfectWorld::StructureOutcome>(&Outcome))
	{
		OutFacts.Grade = Generic->Grade;
		OutFacts.bForeground = Generic->Foreground;
		OutFacts.LearningPermille = Generic->LearningPermille;
		OutFacts.UnitsSpent = Generic->UnitsSpent;
		OutFacts.bBecameSecure = Generic->BecameSecure;
		AddFailures(Generic->Failures);
		return true;
	}
	return false;
}
