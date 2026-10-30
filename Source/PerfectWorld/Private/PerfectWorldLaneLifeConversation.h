#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldConversationPresenter.h"
#include "Conversation.h"

class UPWLaneLifeSubsystem;

/** The catalogue that speaks for a person (the core's ConversationCatalogueForPerson); empty for a person the core does not know. */
PerfectWorld::ConversationCatalogue FaCatalogueForPerson(FName PersonId);

/**
 * The conversation panel's session over the world's conversation commands.
 *
 * Each person's ConversationState lives in the subsystem's WorldSim and is opened by MakeConversationSession (conversation.open),
 * which also gives this session the session number it belongs to. This class only translates: GetView shows the current node's
 * line and the core's visible choices (Leave is the panel's own action, so it is not listed as a choice); Choose submits
 * conversation.choose (a paid choice moves the world clock inside that command, charged once by the core, never re-applied
 * here); Leave submits conversation.leave. A refusal that matters is shown under the line, in the panel, because the interaction
 * notice is hidden while the panel is open. It holds no rules and reaches the subsystem through a weak pointer, so a closing
 * world cannot leave it dangling.
 */
class FPWCoreConversationSession final : public IPWConversationSession
{
public:
    FPWCoreConversationSession(const TWeakObjectPtr<UPWLaneLifeSubsystem>& InOwner, FName InPersonId, const FText& InSpeaker,
                               uint32 InSessionNumber);

    virtual FPWConversationView GetView() const override;
    virtual void Choose(FName ChoiceId) override;
    /** True once the core's session has ended (a choice with no next node, or a newer session), or the world is gone. */
    virtual bool IsFinished() const override;
    virtual void Leave() override;

private:
    /** The person's state when it is still this session; null once the session ended or a newer one began. */
    const PerfectWorld::ConversationState* FindOpenState() const;

    TWeakObjectPtr<UPWLaneLifeSubsystem> Owner;
    FName PersonId;
    FText Speaker;
    uint32 SessionNumber = 0;
    /** Why the last choice was refused, shown under the line until the next choice is accepted. */
    FText RefusalNote;
};
