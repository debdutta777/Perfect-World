#include "PerfectWorldAwakeningCatalogue.h"

#include "PerfectWorldPresentationSettings.h"

#define LOCTEXT_NAMESPACE "PerfectWorldAwakeningCatalogue"

namespace PWAwakening
{
namespace
{
FAwChoiceDef AwMakeChoice(const TCHAR* Id, EAwStance Stance, const TCHAR* Label, const FText& Verb, const FText& Caption, int32 Primary,
                          int32 Secondary, bool bHold)
{
    FAwChoiceDef Def;
    Def.Id = FName(Id);
    Def.Stance = Stance;
    Def.Label = Label;
    Def.Verb = Verb;
    Def.Caption = Caption;
    Def.Effect.PrimaryPose = Primary;
    Def.Effect.SecondaryPose = Secondary;
    Def.Effect.bHoldPrimary = bHold;
    return Def;
}

TArray<FAwSituationDef> AwBuildCatalogue()
{
    TArray<FAwSituationDef> Out;

    {
        FAwSituationDef S;
        S.Id = FName(Ids::StalledCrossing);
        S.Title = TEXT("STATION 1 - STALLED CROSSING");
        S.Summary = TEXT("The platform has stopped half way. A classmate is waiting\non the far side and is frightened.");
        S.Slug = TEXT("crossing");
        S.FigurePerson = FName(TEXT("person.mira_sen"));
        S.FigureName = LOCTEXT("FigureMira", "Mira");
        S.Choices.Add(AwMakeChoice(Ids::HelpClassmate, EAwStance::Help, TEXT("HELP"), LOCTEXT("VerbHelp", "Help your classmate across"),
                                   LOCTEXT("CapHelp", "You helped your classmate across; the platform now meets the near side."), 0, -1, false));
        S.Choices.Add(AwMakeChoice(Ids::ObserveCycle, EAwStance::Observe, TEXT("OBSERVE"),
                                   LOCTEXT("VerbObserve", "Watch the platform cycle and explain a route"),
                                   LOCTEXT("CapObserve", "You watched the platform cycle and marked the safe moment for the group."), 0, 1, false));
        S.Choices.Add(AwMakeChoice(Ids::WaitTogether, EAwStance::Wait, TEXT("WAIT"), LOCTEXT("VerbWait", "Wait together for assistance"),
                                   LOCTEXT("CapWait", "You waited with your classmate; the supervisor's signal levelled the platform."), 0, 1, false));
        Out.Add(MoveTemp(S));
    }
    {
        FAwSituationDef S;
        S.Id = FName(Ids::ShiftingCorridor);
        S.Title = TEXT("STATION 2 - SHIFTING CORRIDOR");
        S.Summary = TEXT("The end wall of the corridor slides between two places.\nA classmate is inside it.");
        S.Slug = TEXT("corridor");
        S.FigurePerson = FName(TEXT("person.ishan_sheikh"));
        S.FigureName = LOCTEXT("FigureIshan", "Ishan");
        S.Choices.Add(AwMakeChoice(Ids::ProtectBarrier, EAwStance::Protect, TEXT("PROTECT"),
                                   LOCTEXT("VerbProtect", "Slide the barrier in front of your classmate"),
                                   LOCTEXT("CapProtect", "You slid the barrier between your classmate and the moving wall."), -1, 1, true));
        S.Choices.Add(AwMakeChoice(Ids::BraceWall, EAwStance::Improvise, TEXT("BRACE"), LOCTEXT("VerbBrace", "Improvise a brace to hold the wall"),
                                   LOCTEXT("CapBrace", "You set a brace under the wall and it stopped shifting."), -1, 2, true));
        S.Choices.Add(AwMakeChoice(Ids::LeadLongerRoute, EAwStance::Lead, TEXT("LEAD"), LOCTEXT("VerbLead", "Lead the group along the longer route"),
                                   LOCTEXT("CapLead", "You led the group by the longer route; the east way is open."), 1, -1, true));
        Out.Add(MoveTemp(S));
    }
    {
        FAwSituationDef S;
        S.Id = FName(Ids::SharedDelivery);
        S.Title = TEXT("STATION 3 - SHARED DELIVERY");
        S.Summary = TEXT("Supplies must reach the handover desk. The marker is\nflickering and two carriers disagree.");
        S.Slug = TEXT("delivery");
        S.FigurePerson = FName(TEXT("person.tara_das"));
        S.FigureName = LOCTEXT("FigureTara", "Tara");
        S.Choices.Add(AwMakeChoice(Ids::CoordinateCarriers, EAwStance::Coordinate, TEXT("COORDINATE"),
                                   LOCTEXT("VerbCoordinate", "Coordinate the carriers and the supplies"),
                                   LOCTEXT("CapCoordinate", "You lined up the carriers and the supplies."), 1, -1, false));
        S.Choices.Add(AwMakeChoice(Ids::InvestigateMarker, EAwStance::Investigate, TEXT("CHECK"),
                                   LOCTEXT("VerbInvestigate", "Investigate the faulty marker"),
                                   LOCTEXT("CapInvestigate", "You checked the marker; it now shows a steady light."), -1, 1, false));
        S.Choices.Add(AwMakeChoice(Ids::DeclineConfrontation, EAwStance::Decline, TEXT("DECLINE"),
                                   LOCTEXT("VerbDecline", "Decline the argument and arrange a supervised handover"),
                                   LOCTEXT("CapDecline", "You declined the argument and arranged a supervised handover."), -1, 2, false));
        Out.Add(MoveTemp(S));
    }
    return Out;
}
} // namespace

const TArray<FAwSituationDef>& AwCatalogue()
{
    static const TArray<FAwSituationDef> Catalogue = AwBuildCatalogue();
    return Catalogue;
}

int32 AwSituationIndex(const FName SituationId)
{
    const TArray<FAwSituationDef>& All = AwCatalogue();
    for (int32 Index = 0; Index < All.Num(); ++Index)
    {
        if (All[Index].Id == SituationId)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

const FAwSituationDef* AwFindSituation(const FName SituationId)
{
    const int32 Index = AwSituationIndex(SituationId);
    return Index == INDEX_NONE ? nullptr : &AwCatalogue()[Index];
}

const FAwChoiceDef& AwWithdrawDef()
{
    static const FAwChoiceDef Withdraw = AwMakeChoice(Ids::WithdrawToSafety, EAwStance::Withdraw, TEXT("WITHDRAW"),
                                                      LOCTEXT("VerbWithdraw", "Withdraw to the safe area"),
                                                      LOCTEXT("CapWithdraw", "You moved to the safe area. Withdrawing is a complete choice."), -1, -1, false);
    return Withdraw;
}

const FAwChoiceDef* AwFindChoice(const FName SituationId, const FName ChoiceId)
{
    const FAwSituationDef* Situation = AwFindSituation(SituationId);
    if (Situation == nullptr)
    {
        return nullptr;
    }
    if (ChoiceId == AwWithdrawDef().Id)
    {
        return &AwWithdrawDef();
    }
    for (const FAwChoiceDef& Choice : Situation->Choices)
    {
        if (Choice.Id == ChoiceId)
        {
            return &Choice;
        }
    }
    return nullptr;
}

TArray<FName> AwAllChoiceIds()
{
    TArray<FName> Out;
    for (const FAwSituationDef& Situation : AwCatalogue())
    {
        for (const FAwChoiceDef& Choice : Situation.Choices)
        {
            Out.AddUnique(Choice.Id);
        }
    }
    Out.AddUnique(AwWithdrawDef().Id);
    return Out;
}

EAwShape AwStanceShape(const EAwStance Stance)
{
    switch (Stance)
    {
    case EAwStance::Help:
        return EAwShape::Plus;
    case EAwStance::Observe:
    case EAwStance::Investigate:
        return EAwShape::Frame;
    case EAwStance::Wait:
    case EAwStance::Decline:
        return EAwShape::Bars;
    case EAwStance::Protect:
    case EAwStance::Improvise:
        return EAwShape::Tee;
    case EAwStance::Lead:
    case EAwStance::Coordinate:
        return EAwShape::Arrow;
    case EAwStance::Withdraw:
        return EAwShape::Back;
    }
    return EAwShape::Frame;
}

const TCHAR* AwStanceName(const EAwStance Stance)
{
    switch (Stance)
    {
    case EAwStance::Help:
        return TEXT("help");
    case EAwStance::Observe:
        return TEXT("observe");
    case EAwStance::Wait:
        return TEXT("wait");
    case EAwStance::Protect:
        return TEXT("protect");
    case EAwStance::Improvise:
        return TEXT("improvise");
    case EAwStance::Lead:
        return TEXT("lead");
    case EAwStance::Coordinate:
        return TEXT("coordinate");
    case EAwStance::Investigate:
        return TEXT("investigate");
    case EAwStance::Decline:
        return TEXT("decline");
    case EAwStance::Withdraw:
        return TEXT("withdraw");
    }
    return TEXT("unknown");
}

FAwTransitionPlan AwPlanTransition(const bool bReducedMotion)
{
    FAwTransitionPlan Plan;
    if (bReducedMotion)
    {
        // No fade, no flash: the caption announces the move, the view holds still, then it cuts once.
        Plan.bFade = false;
        Plan.FadeOutSeconds = 0.0f;
        Plan.HoldSeconds = 1.2f;
        Plan.FadeInSeconds = 0.0f;
        Plan.bAnnounceFirst = true;
    }
    return Plan;
}

bool AwIsReducedMotion(const TOptional<bool>& Override)
{
    return Override.IsSet() ? Override.GetValue() : UPerfectWorldPresentationSettings::Get().IsReducedMotion();
}
} // namespace PWAwakening

#undef LOCTEXT_NAMESPACE
