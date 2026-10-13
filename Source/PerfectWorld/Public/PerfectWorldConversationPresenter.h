#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PerfectWorldConversationPresenter.generated.h"

class APlayerController;
class SPWConversationPanel;
class SWeakWidget;

/** One thing the player can say. CostMinutes is the time the exchange commits; 0 means a greeting or a question that costs nothing. */
struct FPWConversationChoice
{
    FName Id;
    FText Label;
    int32 CostMinutes = 0;
};

/** Everything the panel shows for one line. The portable core's Conversation module produces the same content; the panel computes nothing. */
struct FPWConversationView
{
    FText Speaker;
    FText Line;
    /** The panel is designed for at most three. */
    TArray<FPWConversationChoice> Choices;
    /** False hides the Leave action's effect (the button stays visible but disabled). Escape and B then do nothing either. */
    bool bCanLeave = true;
};

/**
 * What the presenter talks to. The integrator implements it over the core Conversation module (talk.open, talk.choose,
 * talk.close); until then FPWTemporaryHouseholdConversation serves three authored lines.
 *
 * The presenter calls GetView() when a conversation opens and again after every Choose(). Reading consumes nothing:
 * costs belong to Choose(), and only the session (the core behind it) applies them.
 */
class IPWConversationSession
{
public:
    virtual ~IPWConversationSession() = default;

    /** The line and choices to show right now. */
    virtual FPWConversationView GetView() const = 0;

    /** The player picked a choice that GetView() offered. A stale or unknown id must be ignored. */
    virtual void Choose(FName ChoiceId) = 0;

    /** True once the exchange ended by itself (for example a choice that closes it). The presenter then closes the panel
     *  and does NOT call Leave(). */
    virtual bool IsFinished() const = 0;

    /** The player left (Leave, Escape or B). Called exactly once, after the panel is closed and game input is back. */
    virtual void Leave() = 0;
};

/** Builds the session for a person the player spoke to. The integrator installs one over the core; null means "cannot talk now". */
using FPWConversationSessionFactory = TFunction<TSharedPtr<IPWConversationSession>(FName PersonId, const FText& DisplayName)>;

/**
 * Opens, shows and closes the conversation panel (LANE_LIFE_PLAN section B "Conversation UX").
 *
 * This is a world subsystem rather than a component on the player because the conversation belongs to the world's single
 * local screen, not to a pawn: any actor (a person, later a shop or a notice board) opens it without knowing which pawn
 * the player is, the player's input code only asks the world whether a conversation is open, and its lifetime ends with the
 * world, which removes the Slate panel exactly once. It owns no rules.
 *
 * Input: opening switches the local controller to UI-only input with the cursor shown (what the front end does for its
 * menus), so the keyboard and controller belong to the panel and the walking player stops. Closing restores game-only input
 * and the hidden cursor. The camera is never touched. The engine is not paused; the portable core owns conversation time.
 */
UCLASS()
class PERFECTWORLD_API UPWConversationPresenter : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** The presenter of the world that WorldContext lives in, or null. */
    static UPWConversationPresenter* Get(const UObject* WorldContext);

    virtual void Deinitialize() override;

    bool IsOpen() const { return Session.IsValid(); }

    /** Shows the panel for this session. False (and nothing changes) if one is already open, a menu is up, or there is no screen. */
    bool Open(const TSharedRef<IPWConversationSession>& NewSession);

    /** Asks the session factory for a session for this person and opens it. Until a factory is installed this is the temporary adapter. */
    bool BeginConversationWith(FName PersonId, const FText& DisplayName);

    /** Replaces the session factory. An empty function restores the temporary household adapter. */
    void SetSessionFactory(FPWConversationSessionFactory NewFactory);

    /** From the panel: the player picked a choice. */
    void ChooseOption(FName ChoiceId);

    /** From the panel: the player left. Closes, restores play input, then tells the session. */
    void LeaveConversation();

private:
    void ClosePanelAndRestoreInput();
    void ApplyConversationInput(APlayerController& Controller) const;
    void RestoreGameInput() const;

    TSharedPtr<IPWConversationSession> Session;
    FPWConversationView CurrentView;
    FPWConversationSessionFactory Factory;
    TWeakObjectPtr<APlayerController> Controller;
    TSharedPtr<SPWConversationPanel> Panel;
    TSharedPtr<SWeakWidget> Holder;
};
