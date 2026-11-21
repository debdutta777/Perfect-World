#include "UI/SPWClassExplain.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/SBoxPanel.h"

#include <variant>

// The construct-an-explanation lesson (order A2) on the shared shell. Words are in the shared table (phase2.a2.explain.*).

namespace
{
FString ClEKey(const TCHAR* Group, std::string_view Name)
{
	return FString(TEXT("phase2.a2.explain.")) + Group + PWClassLesson::ToFString(std::string(Name));
}

PerfectWorld::SchoolSessionResult ClERefusal(const char* Code)
{
	PerfectWorld::SchoolSessionResult Result;
	Result.Issue.Code = Code;
	return Result;
}

struct FClEFail
{
	uint8 Bit;
	const TCHAR* Key;
};

const FClEFail GClEFails[] = {
	{PerfectWorld::ExplainFailIndefensible, TEXT("phase2.a2.explain.fail.indefensible")},
	{PerfectWorld::ExplainFailUnsupported, TEXT("phase2.a2.explain.fail.unsupported")},
	{PerfectWorld::ExplainFailOverreach, TEXT("phase2.a2.explain.fail.overreach")},
	{PerfectWorld::ExplainFailPadding, TEXT("phase2.a2.explain.fail.padding")},
};

const PerfectWorld::ExplainGoal GClEGoals[] = {PerfectWorld::ExplainGoal::ArgueClaim, PerfectWorld::ExplainGoal::WeighEvidence,
	PerfectWorld::ExplainGoal::StateLimits};

FText ClEEvidenceName(const PerfectWorld::ExplainCaseDef& Def, const FString& Id)
{
	for (const PerfectWorld::ExplainEvidence& Item : Def.Evidence)
	{
		if (PWClassLesson::ToFString(Item.Id) == Id)
		{
			return PWClassLesson::FromUtf8(Item.Name);
		}
	}
	return PWClassLesson::Text(TEXT("phase2.a2.explain.item.other"));
}

FText ClEClaimName(const PerfectWorld::ExplainCaseDef& Def, const FString& Id)
{
	for (const PerfectWorld::ExplainClaim& Claim : Def.Claims)
	{
		if (PWClassLesson::ToFString(Claim.Id) == Id)
		{
			return PWClassLesson::FromUtf8(Claim.Name);
		}
	}
	return PWClassLesson::Text(TEXT("phase2.a2.explain.item.other"));
}
} // namespace

void SPWClassExplain::Construct(const FArguments& InArgs)
{
	PWClassLesson::FShellArgs Args;
	Args.Bridge = InArgs._Bridge;
	Args.Space = InArgs._Space;
	Args.OnClose = InArgs._OnClose;
	Args.OnLessonClosed = InArgs._OnLessonClosed;
	Args.ReturnFocus = InArgs._ReturnFocus;
	ConstructShell(Args);
}

void SPWClassExplain::ShellSessionChanged()
{
	SelectedClaim.Reset();
	SelectedEvidence.Reset();
	SelectedLimitation.Reset();
}

int32 SPWClassExplain::ShellGoalCount() const
{
	return UE_ARRAY_COUNT(GClEGoals);
}

FText SPWClassExplain::ShellGoalName(int32 Index) const
{
	const PerfectWorld::ExplainGoal Goal = GClEGoals[FMath::Clamp(Index, 0, ShellGoalCount() - 1)];
	return PWClassLesson::Text(ClEKey(TEXT("goal."), PerfectWorld::ExplainGoalNames[static_cast<size_t>(Goal)]));
}

PerfectWorld::SchoolLessonGoal SPWClassExplain::ShellGoalAt(int32 Index) const
{
	return PerfectWorld::SchoolLessonGoal(GClEGoals[FMath::Clamp(Index, 0, ShellGoalCount() - 1)]);
}

void SPWClassExplain::ShellReadExposure(const PerfectWorld::SchoolLessonView& View, bool& bActive, int32& Left, int32& Total) const
{
	bActive = View.Explain.Exposure.Active();
	Left = View.Explain.Exposure.UnitsLeft;
	Total = View.Explain.Exposure.UnitsTotal;
}

PWClassLesson::EMastery SPWClassExplain::ShellMastery(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const
{
	const PerfectWorld::StructureFamilyRecord* Record = View.Explain.FindFamily(FamilyId);
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

const PerfectWorld::ExplainCaseDef* SPWClassExplain::ActiveCase() const
{
	if (!HasBridge() || GetBridge().GetCatalogues().Structures.Explain == nullptr)
	{
		return nullptr;
	}
	return GetBridge().GetCatalogues().Structures.Explain->FindCase(GetFrame().View.Explain.Exposure.CaseId);
}

// ---- Selections and commands ---------------------------------------------------------------------------------------------

void SPWClassExplain::SelectClaim(const FString& ClaimId)
{
	SelectedClaim = ClaimId;
	RefreshModel();
}

void SPWClassExplain::ToggleEvidence(const FString& EvidenceId)
{
	bool bFull = false;
	if (SelectedEvidence.Contains(EvidenceId))
	{
		SelectedEvidence.Remove(EvidenceId);
	}
	else if (SelectedEvidence.Num() < static_cast<int32>(PerfectWorld::ExplainEvidenceSlots))
	{
		SelectedEvidence.Add(EvidenceId);
	}
	else
	{
		bFull = true;
	}
	if (bFull)
	{
		ShowBanner(PWClassLesson::Text1(TEXT("phase2.a2.explain.status.slots_full"),
			PWClassLesson::Number(static_cast<int64>(PerfectWorld::ExplainEvidenceSlots))), true);
	}
	else
	{
		ShowBanner(FText::GetEmpty(), false);
	}
	RefreshModel();
}

void SPWClassExplain::SelectLimitation(const FString& LimitationId)
{
	SelectedLimitation = LimitationId;
	RefreshModel();
}

PerfectWorld::ExplainAction SPWClassExplain::ClaimActionNow() const
{
	PerfectWorld::ExplainAction Action;
	Action.Kind = PerfectWorld::ExplainActionKind::ChooseClaim;
	Action.Claim = PWClassLesson::ToUtf8(SelectedClaim);
	return Action;
}

PerfectWorld::ExplainAction SPWClassExplain::EvidenceActionNow() const
{
	PerfectWorld::ExplainAction Action;
	Action.Kind = PerfectWorld::ExplainActionKind::PlaceEvidence;
	for (const FString& Id : SelectedEvidence)
	{
		Action.Evidence.push_back(PWClassLesson::ToUtf8(Id));
	}
	return Action;
}

PerfectWorld::ExplainAction SPWClassExplain::LimitationActionNow(bool bNone) const
{
	PerfectWorld::ExplainAction Action;
	Action.Kind = PerfectWorld::ExplainActionKind::PlaceLimitation;
	if (!bNone)
	{
		Action.Limitation = PWClassLesson::ToUtf8(SelectedLimitation);
	}
	return Action;
}

PerfectWorld::SchoolSessionResult SPWClassExplain::ExecuteModelParagraph()
{
	PerfectWorld::ExplainAction Action;
	Action.Kind = PerfectWorld::ExplainActionKind::ModelParagraph;
	return ChooseAction(PerfectWorld::SchoolLessonAction(Action), PWClassLesson::Text(TEXT("phase2.a2.explain.status.paragraph")));
}

PerfectWorld::SchoolSessionResult SPWClassExplain::ExecuteChooseClaim()
{
	if (SelectedClaim.IsEmpty())
	{
		ShowBanner(PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_claim")), true);
		return ClERefusal("structure.bad_choice");
	}
	return ChooseAction(PerfectWorld::SchoolLessonAction(ClaimActionNow()), PWClassLesson::Text(TEXT("phase2.a2.explain.status.claim")));
}

PerfectWorld::SchoolSessionResult SPWClassExplain::ExecutePlaceEvidence()
{
	if (SelectedEvidence.IsEmpty())
	{
		ShowBanner(PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_evidence")), true);
		return ClERefusal("structure.bad_choice");
	}
	return ChooseAction(PerfectWorld::SchoolLessonAction(EvidenceActionNow()), PWClassLesson::Text(TEXT("phase2.a2.explain.status.evidence")));
}

PerfectWorld::SchoolSessionResult SPWClassExplain::ExecutePlaceLimitation()
{
	if (SelectedLimitation.IsEmpty())
	{
		ShowBanner(PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_limitation")), true);
		return ClERefusal("structure.bad_choice");
	}
	return ChooseAction(PerfectWorld::SchoolLessonAction(LimitationActionNow(false)), PWClassLesson::Text(TEXT("phase2.a2.explain.status.limitation")));
}

PerfectWorld::SchoolSessionResult SPWClassExplain::ExecuteStateNoLimitation()
{
	return ChooseAction(PerfectWorld::SchoolLessonAction(LimitationActionNow(true)), PWClassLesson::Text(TEXT("phase2.a2.explain.status.no_limitation")));
}

// ---- The case and the steps -------------------------------------------------------------------------------------------------

void SPWClassExplain::ShellBuildCase(const TSharedRef<SVerticalBox>& Into)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::ExplainCaseDef* Def = ActiveCase();
	if (Def == nullptr)
	{
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a2.common.case_missing")), false);
		return;
	}
	const PerfectWorld::ExplainExposure& Exp = GetFrame().View.Explain.Exposure;
	AddHeading(Into, PWClassLesson::FromUtf8(Def->Name));
	AddNote(Into, PWClassLesson::FromUtf8(Def->Brief), false);
	AddLine(Into, PWClassLesson::Text(ClEKey(TEXT("question."), PerfectWorld::ExplainQuestionNames[static_cast<size_t>(Def->Question)])), Style.TextSecondary);
	AddLine(Into, PWClassLesson::Text(ClEKey(TEXT("role."), PerfectWorld::ExplainRoleNames[static_cast<size_t>(Def->Role)])), Style.TextSecondary);
	AddLine(Into, PWClassLesson::Text(ClEKey(TEXT("scarce."), PerfectWorld::ExplainScarceNames[static_cast<size_t>(Def->Scarce)])), Style.TextSecondary);
	AddLine(Into, PWClassLesson::Text(Exp.MethodKnown ? TEXT("phase2.a2.explain.method.known")
		: Exp.ParagraphTaken ? TEXT("phase2.a2.explain.method.paragraph_taken") : TEXT("phase2.a2.explain.method.unfamiliar")), Style.TextSecondary);
	AddUnitsMeter(Into);
	if (!Exp.ClaimId.empty())
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.explain.your_claim")));
		AddLine(Into, ClEClaimName(*Def, PWClassLesson::ToFString(Exp.ClaimId)), Style.TextPrimary);
	}
	if (!Exp.EvidenceIds.empty())
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.explain.your_evidence")));
		for (const std::string& Id : Exp.EvidenceIds)
		{
			AddLine(Into, ClEEvidenceName(*Def, PWClassLesson::ToFString(Id)), Style.TextPrimary);
		}
	}
}

void SPWClassExplain::ShellBuildPlay(const TSharedRef<SVerticalBox>& Into)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::ExplainCaseDef* Def = ActiveCase();
	if (Def == nullptr)
	{
		AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.no_exercise")), false);
		return;
	}
	const PerfectWorld::ExplainExposure& Exp = GetFrame().View.Explain.Exposure;
	if (Exp.Beat == PerfectWorld::ExplainBeat::Claim)
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.explain.beat.claim")));
		if (!Exp.MethodKnown && !Exp.ParagraphTaken)
		{
			PerfectWorld::ExplainAction Paragraph;
			Paragraph.Kind = PerfectWorld::ExplainActionKind::ModelParagraph;
			AddQuotedRow(Into, TEXT("paragraph"), PWClassLesson::Text(TEXT("phase2.a2.explain.action.model_paragraph")), FText::GetEmpty(),
				Quote(PerfectWorld::SchoolLessonAction(Paragraph)), [this]() { ExecuteModelParagraph(); });
		}
		AddLine(Into, PWClassLesson::Text(TEXT("phase2.a2.explain.claims_hint")), Style.TextSecondary);
		for (const PerfectWorld::ExplainClaim& Claim : Def->Claims)
		{
			const FString ClaimId = PWClassLesson::ToFString(Claim.Id);
			AddActionRow(Into, TEXT("claim.") + ClaimId, PWClassLesson::FromUtf8(Claim.Name), FText::GetEmpty(), FText::GetEmpty(),
				ECostTone::Plain, true, [this, ClaimId]() { SelectClaim(ClaimId); }, SelectedClaim == ClaimId);
		}
		AddQuotedRow(Into, TEXT("commit_claim"), PWClassLesson::Text(TEXT("phase2.a2.explain.action.commit_claim")), FText::GetEmpty(),
			Quote(PerfectWorld::SchoolLessonAction(ClaimActionNow())), [this]() { ExecuteChooseClaim(); }, false, !SelectedClaim.IsEmpty(),
			PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_claim")));
	}
	else if (Exp.Beat == PerfectWorld::ExplainBeat::Evidence)
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.explain.beat.evidence")));
		AddLine(Into, PWClassLesson::Text(TEXT("phase2.a2.explain.evidence_hint")), Style.TextSecondary);
		for (const PerfectWorld::ExplainEvidence& Item : Def->Evidence)
		{
			const FString ItemId = PWClassLesson::ToFString(Item.Id);
			AddActionRow(Into, TEXT("evidence.") + ItemId, PWClassLesson::FromUtf8(Item.Name), FText::GetEmpty(), FText::GetEmpty(),
				ECostTone::Plain, true, [this, ItemId]() { ToggleEvidence(ItemId); }, IsEvidenceSelected(ItemId));
		}
		AddQuotedRow(Into, TEXT("commit_evidence"), PWClassLesson::Text(TEXT("phase2.a2.explain.action.commit_evidence")), FText::GetEmpty(),
			Quote(PerfectWorld::SchoolLessonAction(EvidenceActionNow())), [this]() { ExecutePlaceEvidence(); }, false, !SelectedEvidence.IsEmpty(),
			PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_evidence")));
	}
	else
	{
		AddHeading(Into, PWClassLesson::Text(TEXT("phase2.a2.explain.beat.limitation")));
		AddLine(Into, PWClassLesson::Text(TEXT("phase2.a2.explain.limitation_hint")), Style.TextSecondary);
		for (const PerfectWorld::ExplainLimitation& Limit : Def->Limitations)
		{
			const FString LimitId = PWClassLesson::ToFString(Limit.Id);
			AddActionRow(Into, TEXT("limit.") + LimitId, PWClassLesson::FromUtf8(Limit.Name), FText::GetEmpty(), FText::GetEmpty(),
				ECostTone::Plain, true, [this, LimitId]() { SelectLimitation(LimitId); }, SelectedLimitation == LimitId);
		}
		AddQuotedRow(Into, TEXT("commit_limitation"), PWClassLesson::Text(TEXT("phase2.a2.explain.action.commit_limitation")), FText::GetEmpty(),
			Quote(PerfectWorld::SchoolLessonAction(LimitationActionNow(false))), [this]() { ExecutePlaceLimitation(); }, false,
			!SelectedLimitation.IsEmpty(), PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_limitation")));
		AddQuotedRow(Into, TEXT("no_limitation"), PWClassLesson::Text(TEXT("phase2.a2.explain.action.no_limitation")), FText::GetEmpty(),
			Quote(PerfectWorld::SchoolLessonAction(LimitationActionNow(true))), [this]() { ExecuteStateNoLimitation(); });
	}
	AddNote(Into, PWClassLesson::Text(TEXT("phase2.a3.common.leave_note")));
}

bool SPWClassExplain::ShellDescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const
{
	const auto AddFailures = [&OutFacts](uint8 Failures)
	{
		for (const FClEFail& Fail : GClEFails)
		{
			if ((Failures & Fail.Bit) != 0)
			{
				OutFacts.Diagnostics.Add(PWClassLesson::Text(Fail.Key));
			}
		}
	};
	if (const PerfectWorld::ExplainOutcome* Explain = std::get_if<PerfectWorld::ExplainOutcome>(&Outcome))
	{
		OutFacts.Grade = Explain->Grade;
		OutFacts.bForeground = Explain->Foreground;
		OutFacts.LearningPermille = Explain->LearningPermille;
		OutFacts.UnitsSpent = Explain->UnitsSpent;
		OutFacts.bBecameSecure = Explain->BecameSecure;
		OutFacts.Detail.Add(PWClassLesson::Text2(TEXT("phase2.a2.explain.outcome.evidence"), PWClassLesson::Number(Explain->PlacedItems),
			PWClassLesson::Number(Explain->SupportingItems)));
		OutFacts.Detail.Add(PWClassLesson::Text(Explain->Bounded ? TEXT("phase2.a2.explain.outcome.bounded") : TEXT("phase2.a2.explain.outcome.unbounded")));
		AddFailures(Explain->Failures);
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
