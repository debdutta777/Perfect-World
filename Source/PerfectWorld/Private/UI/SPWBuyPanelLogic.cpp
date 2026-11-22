#include "UI/SPWBuyPanel.h"

#include "HAL/PlatformTime.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCommerceText.h"
#include "UI/PWWidgets.h"
#include "Widgets/Layout/SBox.h"

// The buy panel's behaviour: quotes, the guarded single confirmation, the receipt, keys. Split from SPWBuyPanel.cpp (layout).

namespace PWBuy
{
bool Compact(const float Width, const float Height)
{
    return Height < 760.0f || Width < 760.0f;
}

float PanelWidth(const float Width, const float)
{
    return FMath::Clamp(Width - 32.0f, 280.0f, 560.0f);
}

float PanelMaxHeight(const float, const float Height)
{
    return FMath::Max(120.0f, Height - 32.0f);
}
} // namespace PWBuy

void SPWBuyPanel::SetClockForTest(TFunction<double()> NewClock)
{
    Clock = MoveTemp(NewClock);
    ReArm(); // the arming window is measured on the new clock from now
}

FVector2D SPWBuyPanel::EffectiveSpace() const
{
    // Once laid out, the panel fills the screen; before that (and in tests) the argument stands in.
    const FVector2D Laid(GetCachedGeometry().GetLocalSize());
    return Laid.X > 100.0 && Laid.Y > 100.0 ? Laid : Space.Get();
}

void SPWBuyPanel::ReArm()
{
    ArmedAt = (Clock ? Clock() : FPlatformTime::Seconds()) + PWBuy::ConfirmArmSeconds;
}

bool SPWBuyPanel::IsConfirmArmed() const
{
    return (Clock ? Clock() : FPlatformTime::Seconds()) >= ArmedAt;
}

bool SPWBuyPanel::CanConfirmNow() const
{
    return Stage == PWBuy::EStage::Browsing && !bSubmitting && Quote.CanBuy() && IsConfirmArmed();
}

void SPWBuyPanel::RefreshQuote()
{
    using PerfectWorld::CommerceError;
    if (!Bridge.IsValid() || !Service.Goods.IsValidIndex(Selected))
    {
        Quote = PWCommerce::FCmQuoteView();
        Quote.Quote.Error = CommerceError::InvalidItem;
        Quote.Blocked = CommerceError::InvalidItem;
        return;
    }
    Quote = Bridge->Quote(Service.ShopId, Service.Goods[Selected].ItemId, Quantity);
    if (Quote.Quote.Ok())
    {
        Service.WalletPaise = Quote.Quote.WalletBeforePaise;
    }
}

void SPWBuyPanel::SelectGood(const int32 Index)
{
    if (Stage != PWBuy::EStage::Browsing || !Service.Goods.IsValidIndex(Index) || Index == Selected)
    {
        return;
    }
    Selected = Index;
    Quantity = 1;
    LastRefusal = PerfectWorld::CommerceError::None;
    RefreshQuote();
}

void SPWBuyPanel::StepQuantity(const int32 Delta)
{
    if (Stage != PWBuy::EStage::Browsing)
    {
        return;
    }
    const int32 Next = FMath::Clamp(Quantity + Delta, 1, PWBuy::MaxQuantity);
    if (Next == Quantity)
    {
        return;
    }
    Quantity = Next;
    LastRefusal = PerfectWorld::CommerceError::None;
    RefreshQuote();
}

bool SPWBuyPanel::Confirm()
{
    if (!CanConfirmNow() || !Bridge.IsValid() || !Service.Goods.IsValidIndex(Selected))
    {
        return false;
    }
    // One submission at a time, and once a receipt exists the stage is no longer Browsing, so a repeat press cannot buy again.
    bSubmitting = true;
    ++SubmitCount;
    const PWCommerce::FCmGood& Good = Service.Goods[Selected];
    const PWCommerce::FCmBuyResult Result = Bridge->Buy(Service.ShopId, Good.ItemId, Quantity, Quote.Quote.ExpectedRevision);
    bSubmitting = false;
    if (Result.bAccepted)
    {
        ShowReceipt(Result);
        return true;
    }
    // The core refused: nothing changed. Show why, take a fresh quote (the revision may have moved), and wait before another press.
    LastRefusal = Result.Error != PerfectWorld::CommerceError::None ? Result.Error : PerfectWorld::CommerceError::InvalidState;
    RefreshQuote();
    ReArm();
    PWUI::FocusWhenReady(AsShared(), TWeakPtr<SWidget>(GoodsHolder));
    return true;
}

void SPWBuyPanel::ShowReceipt(const PWCommerce::FCmBuyResult& Result)
{
    const PWCommerce::FCmGood& Good = Service.Goods[Selected];
    Receipt = PWCommerce::FCmReceiptRow();
    if (Result.bHasReceipt)
    {
        Receipt.Receipt = Result.Receipt;
    }
    else
    {
        // The bridge accepted without a receipt: describe what the accepted quote said, never invent amounts.
        Receipt.Receipt.Kind = PerfectWorld::CommerceReceiptKind::Purchase;
        Receipt.Receipt.Quantity = Quote.Quote.Quantity;
        Receipt.Receipt.UnitPricePaise = Quote.Quote.UnitPricePaise;
        Receipt.Receipt.TotalPaise = Quote.Quote.TotalPaise;
        Receipt.Receipt.BalanceBeforePaise = Quote.Quote.WalletBeforePaise;
        Receipt.Receipt.BalanceAfterPaise = Quote.Quote.WalletAfterPaise;
    }
    Receipt.ItemName = Result.ItemName.IsEmpty() ? Good.Name : Result.ItemName;
    Receipt.ShopName = Result.ShopName.IsEmpty() ? Service.ShopName : Result.ShopName;
    Service.WalletPaise = Receipt.Receipt.BalanceAfterPaise;
    LastRefusal = PerfectWorld::CommerceError::None;
    Stage = PWBuy::EStage::Receipt;
    PWUI::FocusWhenReady(AsShared(), TWeakPtr<SWidget>(DoneHolder));
}

void SPWBuyPanel::BuyMore()
{
    if (Stage != PWBuy::EStage::Receipt)
    {
        return;
    }
    Stage = PWBuy::EStage::Browsing;
    Quantity = 1;
    LastRefusal = PerfectWorld::CommerceError::None;
    RefreshQuote();
    ReArm();
    PWUI::FocusWhenReady(AsShared(), TWeakPtr<SWidget>(GoodsHolder));
}

void SPWBuyPanel::RequestClose()
{
    OnClose.ExecuteIfBound();
}

FText SPWBuyPanel::GetStatusText() const
{
    using PerfectWorld::CommerceError;
    if (Stage == PWBuy::EStage::Receipt)
    {
        return PWCommerce::CmReceiptText(Receipt);
    }
    if (!Quote.Quote.Ok())
    {
        return PWCommerce::CmRefusalText(Quote.Quote.Error, Service.ShopName);
    }
    if (Quote.Blocked == CommerceError::InsufficientFunds)
    {
        return PWCommerce::CmShortfallText(Quote.Quote.WalletAfterPaise);
    }
    if (Quote.Blocked != CommerceError::None)
    {
        return PWCommerce::CmRefusalText(Quote.Blocked, Service.ShopName);
    }
    return LastRefusal != CommerceError::None ? PWCommerce::CmRefusalText(LastRefusal, Service.ShopName) : FText::GetEmpty();
}

bool SPWBuyPanel::IsFooterFocused() const
{
    const auto Focused = [](const TSharedPtr<SWidget>& Widget) { return Widget.IsValid() && Widget->HasKeyboardFocus(); };
    return Focused(ConfirmButton) || Focused(BackButton) || Focused(MoreButton) || Focused(DoneButton);
}

FReply SPWBuyPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
    {
        // A held key auto-repeats; the panel goes back on the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    if (Key == EKeys::Hyphen || Key == EKeys::Subtract || Key == EKeys::Gamepad_LeftShoulder)
    {
        StepQuantity(-1);
        return FReply::Handled();
    }
    if (Key == EKeys::Equals || Key == EKeys::Add || Key == EKeys::Gamepad_RightShoulder)
    {
        StepQuantity(1);
        return FReply::Handled();
    }
    const bool bLeft = Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left;
    const bool bRight = Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right;
    if ((bLeft || bRight) && Stage == PWBuy::EStage::Browsing && !IsFooterFocused())
    {
        // On the goods and the stepper, sideways changes the quantity; on Confirm and Back it moves between them (left to Slate).
        StepQuantity(bRight ? 1 : -1);
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}
