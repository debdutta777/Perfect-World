#pragma once

#include "CoreMinimal.h"
#include "UI/PWDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UPerfectWorldCharacterCreatorSubsystem;

/** The title screen: the game's name over the darkened block scene, and the five main buttons. */
class SPWTitleScreen : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWTitleScreen) {}
        SLATE_EVENT(FSimpleDelegate, OnNewGame)
        /** Loads the newest save; returns empty text on success or the message to show. */
        SLATE_EVENT(FPWTryDelegate, OnContinue)
        SLATE_EVENT(FSimpleDelegate, OnLoad)
        SLATE_EVENT(FSimpleDelegate, OnSettings)
        SLATE_EVENT(FSimpleDelegate, OnQuit)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator);

private:
    FPWTryDelegate OnContinue;
    int32 ContinueSlot = -1;
    FText ContinueNote;
    FText Status;
};
