#include "UI/SPWCountdownPanel.h"

#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"

void SPWCountdownPanel::P312AddPlanControls()
{
    const auto& Style = PWUI::FStyle::Get();
    for (const auto& Contact : Model.P312Contacts)
        BodyContainer->AddSlot().AutoHeight().Padding(0, 2)
            [PWUI::MakeText(Contact, 15, Style.TextSecondary)];
    if (!Model.P312NewsText.IsEmpty())
        BodyContainer->AddSlot().AutoHeight().Padding(0, 6)
            [PWUI::MakeText(Model.P312NewsText, 17, Style.Warn, true, true)];
    if (!Model.P312PlanText.IsEmpty())
        BodyContainer->AddSlot().AutoHeight().Padding(0, 6)
            [PWUI::MakeText(Model.P312PlanText, 15, Style.TextPrimary, false, true)];
    if (Model.bP312CanResolve)
        BodyContainer->AddSlot().AutoHeight().Padding(0, 6)
        [
            SNew(SBox).Tag(PWApCountdown::P312ResolveTag)
            [PWUI::MakeButton(FText::FromString(TEXT("Resolve this day / honour standing promises")), PWUI::EButtonKind::Primary,
                FOnClicked::CreateSPLambda(this, [this]() { P312TriggerResolveDays(); return FReply::Handled(); }))]
        ];
}

void SPWCountdownPanel::P312AddObligationControls(const PWAwakening::FPWCountdownObligationRow& Obligation)
{
    const auto& Style = PWUI::FStyle::Get();
    for (const auto& Proposal : Obligation.Proposals)
    {
        BodyContainer->AddSlot().AutoHeight().Padding(0, 2)
            [PWUI::MakeText(Proposal.Disclosure, 15, Style.TextSecondary, false, true)];
        // Honour remains a standing block. Only the resolver executes it; Honour is never a negotiation payload.
        if (Proposal.Proposal == PerfectWorld::CountdownProposal::Honour) continue;
        FString Label;
        switch (Proposal.Proposal)
        {
        case PerfectWorld::CountdownProposal::Share: Label = TEXT("Ask to share"); break;
        case PerfectWorld::CountdownProposal::Defer: Label = TEXT("Ask to defer"); break;
        case PerfectWorld::CountdownProposal::Decline: Label = TEXT("Decline this promise"); break;
        default: continue;
        }
        BodyContainer->AddSlot().AutoHeight().Padding(0, 2)
        [
            SNew(SBox).Tag(PWApCountdown::P312ProposalTag)
            [PWUI::MakeButton(FText::FromString(Label), PWUI::EButtonKind::Secondary,
                FOnClicked::CreateSPLambda(this, [this, Id = Obligation.Id, Kind = Proposal.Proposal]()
                    { P312TriggerProposal(Id, Kind); return FReply::Handled(); }), Proposal.bAvailable, 16)]
        ];
    }
}

void SPWCountdownPanel::P312RefreshAfterAction(const FText& Notice)
{
    LastFeedbackText = Notice;
    RefreshFromBridge();
    // Rebuilt rows may have owned keyboard focus. Schedule focus on the current, attached controls.
    PWUI::FocusWhenReady(AsShared(), GetInitialFocusTarget());
}

bool SPWCountdownPanel::P312TriggerResolveDays()
{
    const auto Bridge = ResolveBridge();
    if (!Bridge.IsValid()) return false;
    const auto Days = Model.P312Days;
    FText Notice;
    const bool Accepted = Bridge->P312ResolveDays(Days, Model.P312WorldSeq, Notice);
    P312RefreshAfterAction(Notice);
    return Accepted;
}

bool SPWCountdownPanel::P312TriggerProposal(const FString& Id, PerfectWorld::CountdownProposal Proposal)
{
    const auto Bridge = ResolveBridge();
    if (!Bridge.IsValid()) return false;
    FText Notice;
    const bool Accepted = Bridge->P312NegotiateObligation(Id, Proposal, Model.P312WorldSeq, Notice);
    P312RefreshAfterAction(Notice);
    return Accepted;
}
