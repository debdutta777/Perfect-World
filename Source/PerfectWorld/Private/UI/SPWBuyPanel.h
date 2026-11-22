#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "PerfectWorldCommerceBridge.h"

class SBox;
class SScrollBox;

/** Layout, timing and widget tags of the buy panel, linked by name from the automation tests. The widget calls these same functions. */
namespace PWBuy
{
/** The most one confirmation buys from this panel: one stack. It is a presentation limit; the core's own bounds decide every refusal. */
inline constexpr int32 MaxQuantity = 10;

/**
 * The panel ignores Confirm for this long after it opens, after a purchase and after a refusal. The interact key that opened the panel
 * (which a player may have rebound to Enter, Space or the controller's A) and a held confirm key can therefore never buy by accident.
 */
inline constexpr double ConfirmArmSeconds = 0.5;

enum class EStage : uint8
{
    /** Goods, quantity, totals and Confirm. */
    Browsing,
    /** The purchase succeeded; the receipt is shown. */
    Receipt
};

/** The same density switch as the pause menu and the journal: short or narrow windows are compact. */
bool Compact(float Width, float Height);
float PanelWidth(float Width, float Height);
float PanelMaxHeight(float Width, float Height);

inline const TCHAR* const PanelTag = TEXT("PWBuyPanel");
inline const TCHAR* const GoodRowTag = TEXT("PWBuyGoodRow");
inline const TCHAR* const GoodsTag = TEXT("PWBuyGoods");
inline const TCHAR* const ConfirmTag = TEXT("PWBuyConfirm");
inline const TCHAR* const BackTag = TEXT("PWBuyBack");
inline const TCHAR* const MoreTag = TEXT("PWBuyMore");
inline const TCHAR* const DoneTag = TEXT("PWBuyDone");
inline const TCHAR* const StatusTag = TEXT("PWBuyStatus");
inline const TCHAR* const ReceiptTag = TEXT("PWBuyReceipt");
inline const TCHAR* const LessTag = TEXT("PWBuyLess");
inline const TCHAR* const MoreQuantityTag = TEXT("PWBuyMoreQuantity");
} // namespace PWBuy

/**
 * The compact paused buy panel (R8-05). It is shown by UPWBuyPresenter after the counter's Buy interaction was accepted by the world.
 *
 * It decides nothing. Every price, total, wallet figure and refusal is what the bridge (ICmCommerceWorld) last answered: the panel asks
 * Quote when the item or the quantity changes, and Confirm submits exactly one Buy with the quote's own revision. It shows the goods
 * (name, unit, unit price in rupees), a quantity stepper, the total, the wallet now and after, and Confirm / Back. A success shows the
 * receipt line; the player may then Buy more or finish.
 *
 * Accidental confirmation. Focus starts on the first item, never on Confirm. Confirm is ignored until PWBuy::ConfirmArmSeconds after the
 * panel opens, after a purchase and after a refusal, so the key that opened it, or a key held down, cannot confirm. A confirmation is
 * accepted once: while it is being submitted, and once it has produced a receipt, further presses do nothing.
 *
 * Keys: Up / Down move between items and controls; Left / Right (D-pad, left stick) change the quantity while focus is on the goods or
 * the stepper, and move between Confirm and Back while focus is on them; - / + and the controller shoulder buttons always change the
 * quantity; Enter, Space or A press the focused button; Esc, B and the controller menu button go back (a held key goes back once).
 * Focus is always visible (the shared focus ring). There is no motion, so reduced motion is respected by construction.
 */
class SPWBuyPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWBuyPanel) : _Space(FVector2D(1920.0, 1080.0)) {}
        SLATE_ARGUMENT(TSharedPtr<PWCommerce::ICmCommerceWorld>, Bridge)
        /** The offer BeginService returned (Ok, with goods). */
        SLATE_ARGUMENT(PWCommerce::FCmServiceView, Service)
        /** The logical size of the screen the panel may use, when it has not been laid out yet. */
        SLATE_ATTRIBUTE(FVector2D, Space)
        SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

    // ---- Actions (the buttons and keys call these; tests call them directly) ----
    void SelectGood(int32 Index);
    void StepQuantity(int32 Delta);
    /** Submits one confirmation when the panel is armed and the last quote can be bought. True when the bridge was called. */
    bool Confirm();
    /** After a receipt: back to the goods, with the wallet and the quote refreshed from the bridge. */
    void BuyMore();
    void RequestClose();

    // ---- State (tests and acceptance read these) ----
    PWBuy::EStage GetStage() const { return Stage; }
    int32 GetSelectedIndex() const { return Selected; }
    int32 GetQuantity() const { return Quantity; }
    const PWCommerce::FCmQuoteView& GetQuote() const { return Quote; }
    const PWCommerce::FCmServiceView& GetService() const { return Service; }
    const PWCommerce::FCmReceiptRow& GetReceipt() const { return Receipt; }
    int32 GetSubmitCount() const { return SubmitCount; }
    /** The explanation under the totals: the refusal, the shortfall or the receipt line; empty when there is nothing to say. */
    FText GetStatusText() const;
    /** False until the arming time has passed (see PWBuy::ConfirmArmSeconds). */
    bool IsConfirmArmed() const;
    /** True when a press of Confirm would submit now (armed, stage Browsing, and the core's last verdict allows it). */
    bool CanConfirmNow() const;
    /** Replaces the time source (seconds), for tests. */
    void SetClockForTest(TFunction<double()> NewClock);

private:
    void RefreshQuote();
    void ReArm();
    void ShowReceipt(const PWCommerce::FCmBuyResult& Result);
    FVector2D EffectiveSpace() const;
    bool IsFooterFocused() const;
    TSharedRef<SWidget> BuildHeader();
    TSharedRef<SWidget> BuildGoods();
    TSharedRef<SWidget> BuildQuantityRow();
    TSharedRef<SWidget> BuildTotals();
    TSharedRef<SWidget> BuildReceipt();
    TSharedRef<SWidget> BuildFooter();
    FText WalletNowText() const;
    FText WalletAfterText() const;
    FText TotalText() const;

    TSharedPtr<PWCommerce::ICmCommerceWorld> Bridge;
    PWCommerce::FCmServiceView Service;
    TAttribute<FVector2D> Space;
    FSimpleDelegate OnClose;
    TFunction<double()> Clock;

    PWBuy::EStage Stage = PWBuy::EStage::Browsing;
    int32 Selected = 0;
    int32 Quantity = 1;
    PWCommerce::FCmQuoteView Quote;
    /** The core's answer to the last confirmation, kept until the player changes something (so StaleRevision is explained). */
    PerfectWorld::CommerceError LastRefusal = PerfectWorld::CommerceError::None;
    PWCommerce::FCmReceiptRow Receipt;
    double ArmedAt = 0.0;
    bool bSubmitting = false;
    int32 SubmitCount = 0;

    TSharedPtr<SBox> GoodsHolder;
    TSharedPtr<SBox> DoneHolder;
    TSharedPtr<SWidget> ConfirmButton;
    TSharedPtr<SWidget> BackButton;
    TSharedPtr<SWidget> MoreButton;
    TSharedPtr<SWidget> DoneButton;
    TSharedPtr<SScrollBox> GoodsScroll;
};
