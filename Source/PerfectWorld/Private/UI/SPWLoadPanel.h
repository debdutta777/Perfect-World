#pragma once

#include "CoreMinimal.h"
#include "UI/PWDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class UPerfectWorldCharacterCreatorSubsystem;

/** Load: the three save slots with their descriptions. Picking one loads it and goes to the world. */
class SPWLoadPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWLoadPanel) {}
        /** Loads the named slot; returns empty text on success or the message to show. */
        SLATE_EVENT(FPWSlotDelegate, OnLoadSlot)
        SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UPerfectWorldCharacterCreatorSubsystem* InCreator);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
    TArray<FText> Descriptions;
    TArray<bool> Occupied;
    FPWSlotDelegate OnLoadSlot;
    FSimpleDelegate OnBack;
    FText Status;
};
