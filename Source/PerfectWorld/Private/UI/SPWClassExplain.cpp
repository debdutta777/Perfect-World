#include "UI/SPWClassExplain.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"

// Order A2: the construct-an-explanation screen. Words, bridge and chrome are in SPWClassModel.h / .cpp.


namespace
{
FString A2EStr(std::string_view Value)
{
	return FString(UTF8_TO_TCHAR(std::string(Value).c_str()));
}

std::string A2EUtf8(const FString& Value)
{
	return std::string(TCHAR_TO_UTF8(*Value));
}

FString A2EKey(const TCHAR* Group, std::string_view Name)
{
	return FString(TEXT("phase2.a2.explain.")) + Group + A2EStr(Name);
}

PerfectWorld::SchoolSessionResult A2ERefusal(const char* Code)
{
	PerfectWorld::SchoolSessionResult Result;
	Result.Issue.Code = Code;
	return Result;
}

struct FA2EFail
{
	uint8 Bit;
	const TCHAR* Key;
};

const FA2EFail GA2EFails[] = {
	{PerfectWorld::ExplainFailIndefensible, TEXT("phase2.a2.explain.fail.indefensible")},
	{PerfectWorld::ExplainFailUnsupported, TEXT("phase2.a2.explain.fail.unsupported")},
	{PerfectWorld::ExplainFailOverreach, TEXT("phase2.a2.explain.fail.overreach")},
	{PerfectWorld::ExplainFailPadding, TEXT("phase2.a2.explain.fail.padding")},
};

const PerfectWorld::ExplainGoal GA2EGoals[] = {PerfectWorld::ExplainGoal::ArgueClaim, PerfectWorld::ExplainGoal::WeighEvidence, PerfectWorld::ExplainGoal::StateLimits};

FText A2EEvidenceName(const PerfectWorld::ExplainCaseDef& Def, const FString& Id)
{
	for (const PerfectWorld::ExplainEvidence& Item : Def.Evidence)
	{
		if (A2EStr(Item.Id) == Id)
		{
			return PWClassLesson::FromUtf8(Item.Name);
		}
	}
	return PWClassLesson::Text(TEXT("phase2.a2.explain.item.other"));
}

FText A2EClaimName(const PerfectWorld::ExplainCaseDef& Def, const FString& Id)
{
	for (const PerfectWorld::ExplainClaim& Claim : Def.Claims)
	{
		if (A2EStr(Claim.Id) == Id)
		{
			return PWClassLesson::FromUtf8(Claim.Name);
		}
	}
	return PWClassLesson::Text(TEXT("phase2.a2.explain.item.other"));
}
} // namespace

void SPWClassExplain::Construct(const FArguments& InArgs)
{
	FLessonInit Init;
	Init.Bridge = InArgs._Bridge;
	Init.Space = InArgs._Space;
	Init.ReturnFocus = InArgs._ReturnFocusTarget;
	Init.OnClosed = InArgs._OnClosed;
	InitLesson(Init);
}

void SPWClassExplain::ResetSelections()
{
	SelectedClaim.Reset();
	SelectedEvidence.Reset();
	SelectedLimitation.Reset();
}

PWClassLesson::EMastery SPWClassExplain::MasteryOf(const PerfectWorld::SchoolLessonView& View, const std::string& FamilyId) const
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

const PerfectWorld::ExplainCaseDef* SPWClassExplain::ActiveCase(const PerfectWorld::SchoolLessonView& View) const
{
	if (!Bridge.IsValid() || Bridge->GetCatalogues().Structures.Explain == nullptr)
	{
		return nullptr;
	}
	return Bridge->GetCatalogues().Structures.Explain->FindCase(View.Explain.Exposure.CaseId);
}

void SPWClassExplain::SelectGoal(PerfectWorld::ExplainGoal InGoal)
{
	SelectedGoal = InGoal;
}

void SPWClassExplain::SelectClaim(const FString& ClaimId)
{
	SelectedClaim = ClaimId;
}

void SPWClassExplain::ToggleEvidence(const FString& EvidenceId)
{
	if (SelectedEvidence.Contains(EvidenceId))
	{
		SelectedEvidence.Remove(EvidenceId);
	}
	else if (SelectedEvidence.Num() < static_cast<int32>(PerfectWorld::ExplainEvidenceSlots))
	{
		SelectedEvidence.Add(EvidenceId);
		SetStatus(FText::GetEmpty(), FString());
	}
	else
	{
		SetStatus(PWClassLesson::Text1(TEXT("phase2.a2.explain.status.slots_full"), PWClassLesson::Number(static_cast<int64>(PerfectWorld::ExplainEvidenceSlots))), FString());
	}
}

void SPWClassExplain::SelectLimitation(const FString& LimitationId)
{
	SelectedLimitation = LimitationId;
}

PerfectWorld::ExplainAction SPWClassExplain::ClaimActionNow() const
{
	PerfectWorld::ExplainAction Action;
	Action.Kind = PerfectWorld::ExplainActionKind::ChooseClaim;
	Action.Claim = A2EUtf8(SelectedClaim);
	return Action;
}

PerfectWorld::ExplainAction SPWClassExplain::EvidenceActionNow() const
{
	PerfectWorld::ExplainAction Action;
	Action.Kind = PerfectWorld::ExplainActionKind::PlaceEvidence;
	for (const FString& Id : SelectedEvidence)
	{
		Action.Evidence.push_back(A2EUtf8(Id));
	}
	return Action;
}

PerfectWorld::ExplainAction SPWClassExplain::LimitationActionNow(bool bNone) const
{
	PerfectWorld::ExplainAction Action;
	Action.Kind = PerfectWorld::ExplainActionKind::PlaceLimitation;
	if (!bNone)
	{
		Action.Limitation = A2EUtf8(SelectedLimitation);
	}
	return Action;
}

PerfectWorld::SchoolSessionResult SPWClassExplain::TriggerModelParagraph()
{
	PerfectWorld::ExplainAction Action;
	Action.Kind = PerfectWorld::ExplainActionKind::ModelParagraph;
	return RunChoose(PerfectWorld::SchoolLessonAction(Action), TEXT("phase2.a2.explain.status.paragraph"));
}

PerfectWorld::SchoolSessionResult SPWClassExplain::TriggerChooseClaim()
{
	if (SelectedClaim.IsEmpty())
	{
		SetStatus(PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_claim")), FString());
		return A2ERefusal("structure.bad_choice");
	}
	return RunChoose(PerfectWorld::SchoolLessonAction(ClaimActionNow()), TEXT("phase2.a2.explain.status.claim"));
}

PerfectWorld::SchoolSessionResult SPWClassExplain::TriggerPlaceEvidence()
{
	if (SelectedEvidence.IsEmpty())
	{
		SetStatus(PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_evidence")), FString());
		return A2ERefusal("structure.bad_choice");
	}
	return RunChoose(PerfectWorld::SchoolLessonAction(EvidenceActionNow()), TEXT("phase2.a2.explain.status.evidence"));
}

PerfectWorld::SchoolSessionResult SPWClassExplain::TriggerPlaceLimitation()
{
	if (SelectedLimitation.IsEmpty())
	{
		SetStatus(PWClassLesson::Text(TEXT("phase2.a2.explain.status.choose_limitation")), FString());
		return A2ERefusal("structure.bad_choice");
	}
	return RunChoose(PerfectWorld::SchoolLessonAction(LimitationActionNow(false)), TEXT("phase2.a2.explain.status.limitation"));
}

PerfectWorld::SchoolSessionResult SPWClassExplain::TriggerStateNoLimitation()
{
	return RunChoose(PerfectWorld::SchoolLessonAction(LimitationActionNow(true)), TEXT("phase2.a2.explain.status.no_limitation"));
}

TSharedRef<SWidget> SPWClassExplain::BuildGoalPicker()
{
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.common.goal_heading")))];
	for (const PerfectWorld::ExplainGoal Goal : GA2EGoals)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			MakeChoiceRow(PWClassLesson::Text(A2EKey(TEXT("goal."), PerfectWorld::ExplainGoalNames[static_cast<size_t>(Goal)])),
				TAttribute<bool>::CreateLambda([this, Goal]() { return SelectedGoal == Goal; }), true,
				FOnClicked::CreateSPLambda(this, [this, Goal]()
				{
					SelectGoal(Goal);
					return FReply::Handled();
				}), FText::GetEmpty())
		];
	}
	return Box;
}

TSharedRef<SWidget> SPWClassExplain::BuildCasePane(const PerfectWorld::SchoolLessonView& View)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::ExplainCaseDef* Def = ActiveCase(View);
	if (Def == nullptr)
	{
		return PWClassLesson::MakeNote(PWClassLesson::Text(TEXT("phase2.a2.common.case_missing")));
	}
	const PerfectWorld::ExplainExposure& Exp = View.Explain.Exposure;
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	Box->AddSlot().AutoHeight()[PWClassLesson::MakeHeading(PWClassLesson::FromUtf8(Def->Name))];
	Box->AddSlot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 8.0f)[PWUI::MakeText(PWClassLesson::FromUtf8(Def->Brief), 17, Style.TextPrimary, false, true)];
	const FText Facts[] = {
		PWClassLesson::Text(A2EKey(TEXT("question."), PerfectWorld::ExplainQuestionNames[static_cast<size_t>(Def->Question)])),
		PWClassLesson::Text(A2EKey(TEXT("role."), PerfectWorld::ExplainRoleNames[static_cast<size_t>(Def->Role)])),
		PWClassLesson::Text(A2EKey(TEXT("scarce."), PerfectWorld::ExplainScarceNames[static_cast<size_t>(Def->Scarce)])),
		PWClassLesson::Text(Exp.MethodKnown ? TEXT("phase2.a2.explain.method.known")
			: Exp.ParagraphTaken ? TEXT("phase2.a2.explain.method.paragraph_taken") : TEXT("phase2.a2.explain.method.unfamiliar")),
	};
	for (const FText& Fact : Facts)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 2.0f)[PWUI::MakeText(Fact, 16, Style.TextSecondary, false, true)];
	}
	if (!Exp.ClaimId.empty())
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.explain.your_claim")))];
		Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			PWUI::MakeText(A2EClaimName(*Def, A2EStr(Exp.ClaimId)), 16, Style.TextPrimary, false, true)
		];
	}
	if (!Exp.EvidenceIds.empty())
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.explain.your_evidence")))];
		for (const std::string& Id : Exp.EvidenceIds)
		{
			Box->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				PWUI::MakeText(A2EEvidenceName(*Def, A2EStr(Id)), 16, Style.TextPrimary, false, true)
			];
		}
	}
	return Box;
}

TSharedRef<SWidget> SPWClassExplain::BuildBeatPane(const PerfectWorld::SchoolLessonView& View)
{
	const PWUI::FStyle& Style = PWUI::FStyle::Get();
	const PerfectWorld::ExplainCaseDef* Def = ActiveCase(View);
	if (Def == nullptr)
	{
		return PWClassLesson::MakeNote(PWClassLesson::Text(TEXT("phase2.a2.common.case_missing")));
	}
	const PerfectWorld::ExplainExposure& Exp = View.Explain.Exposure;
	const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	if (Exp.Beat == PerfectWorld::ExplainBeat::Claim)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.explain.beat.claim")))];
		if (!Exp.MethodKnown && !Exp.ParagraphTaken)
		{
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.explain.action.model_paragraph")), true,
					FOnClicked::CreateSPLambda(this, [this]()
					{
						TriggerModelParagraph();
						return FReply::Handled();
					}),
					TAttribute<FText>::CreateLambda([this]()
					{
						PerfectWorld::ExplainAction Action;
						Action.Kind = PerfectWorld::ExplainActionKind::ModelParagraph;
						return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(Action)));
					}))
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a2.explain.claims_hint")), 15, Style.TextSecondary, false, true)];
		for (const PerfectWorld::ExplainClaim& Claim : Def->Claims)
		{
			const FString ClaimId = A2EStr(Claim.Id);
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeChoiceRow(PWClassLesson::FromUtf8(Claim.Name),
					TAttribute<bool>::CreateLambda([this, ClaimId]() { return SelectedClaim == ClaimId; }), true,
					FOnClicked::CreateSPLambda(this, [this, ClaimId]()
					{
						SelectClaim(ClaimId);
						return FReply::Handled();
					}), FText::GetEmpty())
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.explain.action.commit_claim")),
				TAttribute<bool>::CreateLambda([this]() { return !SelectedClaim.IsEmpty(); }),
				FOnClicked::CreateSPLambda(this, [this]()
				{
					TriggerChooseClaim();
					return FReply::Handled();
				}),
				TAttribute<FText>::CreateLambda([this]() { return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(ClaimActionNow()))); }))
		];
	}
	else if (Exp.Beat == PerfectWorld::ExplainBeat::Evidence)
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.explain.beat.evidence")))];
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a2.explain.evidence_hint")), 15, Style.TextSecondary, false, true)];
		for (const PerfectWorld::ExplainEvidence& Item : Def->Evidence)
		{
			const FString ItemId = A2EStr(Item.Id);
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeChoiceRow(PWClassLesson::FromUtf8(Item.Name),
					TAttribute<bool>::CreateLambda([this, ItemId]() { return IsEvidenceSelected(ItemId); }), true,
					FOnClicked::CreateSPLambda(this, [this, ItemId]()
					{
						ToggleEvidence(ItemId);
						return FReply::Handled();
					}), FText::GetEmpty())
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.explain.action.commit_evidence")),
				TAttribute<bool>::CreateLambda([this]() { return !SelectedEvidence.IsEmpty(); }),
				FOnClicked::CreateSPLambda(this, [this]()
				{
					TriggerPlaceEvidence();
					return FReply::Handled();
				}),
				TAttribute<FText>::CreateLambda([this]() { return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(EvidenceActionNow()))); }))
		];
	}
	else
	{
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[PWClassLesson::MakeHeading(PWClassLesson::Text(TEXT("phase2.a2.explain.beat.limitation")))];
		Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[PWUI::MakeText(PWClassLesson::Text(TEXT("phase2.a2.explain.limitation_hint")), 15, Style.TextSecondary, false, true)];
		for (const PerfectWorld::ExplainLimitation& Limit : Def->Limitations)
		{
			const FString LimitId = A2EStr(Limit.Id);
			Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				MakeChoiceRow(PWClassLesson::FromUtf8(Limit.Name),
					TAttribute<bool>::CreateLambda([this, LimitId]() { return SelectedLimitation == LimitId; }), true,
					FOnClicked::CreateSPLambda(this, [this, LimitId]()
					{
						SelectLimitation(LimitId);
						return FReply::Handled();
					}), FText::GetEmpty())
			];
		}
		Box->AddSlot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 6.0f)
		[
			MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.explain.action.commit_limitation")),
				TAttribute<bool>::CreateLambda([this]() { return !SelectedLimitation.IsEmpty(); }),
				FOnClicked::CreateSPLambda(this, [this]()
				{
					TriggerPlaceLimitation();
					return FReply::Handled();
				}),
				TAttribute<FText>::CreateLambda([this]() { return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(LimitationActionNow(false)))); }))
		];
		Box->AddSlot().AutoHeight()
		[
			MakeCommitRow(PWClassLesson::Text(TEXT("phase2.a2.explain.action.no_limitation")), true,
				FOnClicked::CreateSPLambda(this, [this]()
				{
					TriggerStateNoLimitation();
					return FReply::Handled();
				}),
				TAttribute<FText>::CreateLambda([this]() { return PWClassLesson::QuoteText(Quote(PerfectWorld::SchoolLessonAction(LimitationActionNow(true)))); }))
		];
	}
	return Box;
}

bool SPWClassExplain::DescribeOutcome(const PerfectWorld::SchoolLessonOutcome& Outcome, PWClassLesson::FOutcomeFacts& OutFacts) const
{
	const auto AddFailures = [&OutFacts](uint8 Failures)
	{
		for (const FA2EFail& Fail : GA2EFails)
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
