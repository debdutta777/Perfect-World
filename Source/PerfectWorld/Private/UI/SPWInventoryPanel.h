#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "PerfectWorldCommerceBridge.h"

class SBox;
class SScrollBox;

/** Layout rules and widget tags of the Inventory screen, linked by name from the automation tests. The widget calls these same functions. */
namespace PWInventory
{
/** The same density switch as the pause menu and the journal. */
bool Compact(float Width, float Height);
float PanelWidth(float Width, float Height);
float PanelMaxHeight(float Width, float Height);

/** Items shown by the Use button: the core's effect classes that have a use this round (tea, food). Presentation only. */
bool HasUse(PerfectWorld::CommerceEffect Effect);

inline const TCHAR* const PanelTag = TEXT("PWInventoryPanel");
inline const TCHAR* const BodyTag = TEXT("PWInventoryBody");
inline const TCHAR* const StackRowTag = TEXT("PWInventoryStackRow");
inline const TCHAR* const UseTag = TEXT("PWInventoryUse");
inline const TCHAR* const ReceiptRowTag = TEXT("PWInventoryReceiptRow");
inline const TCHAR* const EmptyBagTag = TEXT("PWInventoryEmptyBag");
inline const TCHAR* const EmptyReceiptsTag = TEXT("PWInventoryEmptyReceipts");
inline const TCHAR* const UnavailableTag = TEXT("PWInventoryUnavailable");
inline const TCHAR* const MessageTag = TEXT("PWInventoryMessage");
inline const TCHAR* const BackTag = TEXT("PWInventoryBack");
} // namespace PWInventory

/**
 * The pause menu's Inventory screen (R8-05): the wallet, the bag's items with a Consume button for the ones that have a use, and the
 * recent receipts. It is built from the bridge's FCmInventoryView and decides nothing: Consume asks the bridge, which asks the core,
 * and the screen then shows the core's answer (the refusal in words, or "Used Tea.") and reads the inventory again.
 *
 * It is screen aligned and scales with the viewport like the journal and the pause card; the body scrolls inside the card so Back never
 * leaves the screen. Keys: Up / Down move between the Consume buttons and Back; Page Up / Page Down and the right stick scroll; Esc, B
 * and the controller menu button go back (a held key goes back once); Enter or A press the focused button. No motion is used, so
 * reduced motion is respected by construction.
 */
class SPWInventoryPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWInventoryPanel) : _Space(FVector2D(1920.0, 1080.0)) {}
        /** Null reads as an unavailable inventory. */
        SLATE_ARGUMENT(TSharedPtr<PWCommerce::ICmCommerceWorld>, Bridge)
        /** The logical size of the screen the panel may use. The pause menu passes the size it already measures. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    /** Asks the bridge to use one unit, shows the answer and refreshes the screen. True when the bridge accepted. */
    bool UseItem(FName ItemId);
    void RequestClose();

    const PWCommerce::FCmInventoryView& GetModel() const { return Model; }
    /** The line under the heading after the last Use; empty before the first. */
    FText GetMessage() const { return Message; }

private:
    TSharedRef<SWidget> BuildBody();
    TSharedRef<SWidget> BuildBagSection();
    TSharedRef<SWidget> BuildReceiptsSection();
    TSharedRef<SWidget> BuildStackRow(const PWCommerce::FCmStackView& Stack);
    void Reload();
    void ScrollBody(float Delta);
    FVector2D EffectiveSpace() const;

    TSharedPtr<PWCommerce::ICmCommerceWorld> Bridge;
    PWCommerce::FCmInventoryView Model;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    FText Message;
    bool bMessageIsError = false;

    TSharedPtr<SBox> BodyHolder;
    TSharedPtr<SWidget> BackButton;
    TSharedPtr<SScrollBox> BodyScroll;
};
