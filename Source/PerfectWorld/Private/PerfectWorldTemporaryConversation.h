#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldConversationPresenter.h"

/**
 * TEMPORARY ADAPTER. Delete when the portable Conversation module is wired in.
 *
 * Serves astra's three household-adult lines (LANE_LIFE_PLAN.md section B, "Three sample lines each") so the panel can be
 * seen and tried before the core exists. It holds no rules and persists nothing: every conversation starts again at the
 * first line, costs are declared but never charged, and the "remembered relationship" line is shown here without the
 * remembered event that would gate it in the real game.
 *
 * Replace it by calling UPWConversationPresenter::SetSessionFactory with a factory that builds a session over the core.
 * It is only ever created by PerfectWorldConversationPresenter.cpp, as the default factory.
 */
class FPWTemporaryHouseholdConversation final : public IPWConversationSession
{
public:
    explicit FPWTemporaryHouseholdConversation(const FText& InSpeaker);

    virtual FPWConversationView GetView() const override;
    virtual void Choose(FName ChoiceId) override;
    virtual bool IsFinished() const override { return bFinished; }
    virtual void Leave() override {}

private:
    enum class ENode : uint8
    {
        Greeting,
        EveningJobs,
        Arrangement
    };

    FText Speaker;
    ENode Node = ENode::Greeting;
    bool bFinished = false;
};
