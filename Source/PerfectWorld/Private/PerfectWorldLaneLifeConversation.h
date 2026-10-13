#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldConversationPresenter.h"

class UPWLaneLifeSubsystem;

/**
 * The conversation panel's session over the portable Conversation module (Conversation.h).
 *
 * The core's ConversationState lives in UPWLaneLifeSubsystem and was opened by MakeConversationSession (OpenConversation).
 * This class only translates: GetView shows the current node's line and the core's visible choices (Leave is the panel's own
 * action, so it is not listed as a choice); Choose calls ChooseConversationChoice and hands the returned cost to the
 * subsystem's pending-minutes counter (charged once by the core, never re-applied here); Leave calls LeaveConversation.
 * It holds no rules and reaches the subsystem through a weak pointer, so a closing world cannot leave it dangling.
 */
class FPWCoreConversationSession final : public IPWConversationSession
{
public:
    FPWCoreConversationSession(const TWeakObjectPtr<UPWLaneLifeSubsystem>& InOwner, const FText& InSpeaker);

    virtual FPWConversationView GetView() const override;
    virtual void Choose(FName ChoiceId) override;
    /** True once the core's session has ended (a choice with no next node), or the world is gone. */
    virtual bool IsFinished() const override;
    virtual void Leave() override;

private:
    TWeakObjectPtr<UPWLaneLifeSubsystem> Owner;
    FText Speaker;
};
