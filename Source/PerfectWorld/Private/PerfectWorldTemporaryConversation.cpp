#include "PerfectWorldTemporaryConversation.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTemporaryConversation"

namespace
{
const FName ChoiceTalkJobs(TEXT("temp.talk_jobs"));
const FName ChoiceBackThisEvening(TEXT("temp.back_this_evening"));
const FName ChoiceTakeTurns(TEXT("temp.take_turns"));
const FName ChoiceDecideLater(TEXT("temp.decide_later"));
const FName ChoiceKeepIt(TEXT("temp.keep_it"));
const FName ChoiceNotYet(TEXT("temp.not_yet"));

FPWConversationChoice MakeChoice(const FName Id, const FText& Label, const int32 CostMinutes)
{
    FPWConversationChoice Choice;
    Choice.Id = Id;
    Choice.Label = Label;
    Choice.CostMinutes = CostMinutes;
    return Choice;
}
} // namespace

FPWTemporaryHouseholdConversation::FPWTemporaryHouseholdConversation(const FText& InSpeaker)
    : Speaker(InSpeaker.IsEmpty() ? LOCTEXT("DefaultSpeaker", "Household adult") : InSpeaker)
{
}

FPWConversationView FPWTemporaryHouseholdConversation::GetView() const
{
    FPWConversationView View;
    View.Speaker = Speaker;
    View.bCanLeave = true;
    switch (Node)
    {
    case ENode::Greeting:
        View.Line = LOCTEXT("LineBreakfast", "Breakfast is ready. Tell me what time you expect to be back.");
        View.Choices.Add(MakeChoice(ChoiceBackThisEvening, LOCTEXT("ChoiceBackThisEvening", "Say you will be back this evening"), 0));
        View.Choices.Add(MakeChoice(ChoiceTalkJobs, LOCTEXT("ChoiceTalkJobs", "Talk about the evening jobs"), 3));
        break;
    case ENode::EveningJobs:
        View.Line = LOCTEXT("LineEveningJobs", "We still need to agree who does the evening jobs.");
        View.Choices.Add(MakeChoice(ChoiceTakeTurns, LOCTEXT("ChoiceTakeTurns", "Suggest taking turns"), 3));
        View.Choices.Add(MakeChoice(ChoiceDecideLater, LOCTEXT("ChoiceDecideLater", "Say you can decide later"), 0));
        break;
    case ENode::Arrangement:
        View.Line = LOCTEXT("LineArrangement", "The arrangement you suggested worked yesterday. Shall we keep it?");
        View.Choices.Add(MakeChoice(ChoiceKeepIt, LOCTEXT("ChoiceKeepIt", "Yes, keep it"), 0));
        View.Choices.Add(MakeChoice(ChoiceNotYet, LOCTEXT("ChoiceNotYet", "Not yet"), 0));
        break;
    }
    return View;
}

void FPWTemporaryHouseholdConversation::Choose(const FName ChoiceId)
{
    if (bFinished)
    {
        return;
    }
    switch (Node)
    {
    case ENode::Greeting:
        if (ChoiceId == ChoiceTalkJobs)
        {
            Node = ENode::EveningJobs;
        }
        else if (ChoiceId == ChoiceBackThisEvening)
        {
            bFinished = true;
        }
        break;
    case ENode::EveningJobs:
        if (ChoiceId == ChoiceTakeTurns)
        {
            Node = ENode::Arrangement;
        }
        else if (ChoiceId == ChoiceDecideLater)
        {
            bFinished = true;
        }
        break;
    case ENode::Arrangement:
        if (ChoiceId == ChoiceKeepIt || ChoiceId == ChoiceNotYet)
        {
            bFinished = true;
        }
        break;
    }
}

#undef LOCTEXT_NAMESPACE
