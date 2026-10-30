#include "UI/SPWInventoryPanel.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCommerceText.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "UI/SPWAwakeningSigil.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldInventoryPanel"

namespace PWInventory
{
bool Compact(const float Width, const float Height)
{
    return Height < 760.0f || Width < 760.0f;
}

float PanelWidth(const float Width, const float)
{
    return FMath::Clamp(Width - 32.0f, 280.0f, 640.0f);
}

float PanelMaxHeight(const float, const float Height)
{
    return FMath::Max(120.0f, Height - 32.0f);
}

bool HasUse(const PerfectWorld::CommerceEffect Effect)
{
    return Effect == PerfectWorld::CommerceEffect::Tea || Effect == PerfectWorld::CommerceEffect::Food;
}
} // namespace PWInventory

namespace
{
TSharedRef<SWidget> CmInvHeading(const FText& Text)
{
    return PWUI::MakeText(Text, PWUI::ETextRole::SectionHeading, PWUI::FStyle::Get().Accent, true);
}

/** A quiet inset line that says why a list is empty. */
TSharedRef<SWidget> CmInvNote(const FText& Text, const TCHAR* Tag)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBorder)
        .Tag(FName(Tag))
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(14.0f, 10.0f))
        [
            PWUI::MakeText(Text, 16, Style.TextSecondary, false, true)
        ];
}
} // namespace

FVector2D SPWInventoryPanel::EffectiveSpace() const
{
    return Space.Get();
}

void SPWInventoryPanel::Construct(const FArguments& InArgs)
{
    Bridge = InArgs._Bridge;
    Space = InArgs._Space;
    OnClose = InArgs._OnClose;
    Model = Bridge.IsValid() ? Bridge->ReadInventory() : PWCommerce::FCmInventoryView();
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto IsCompact = [this]()
    {
        const FVector2D S = EffectiveSpace();
        return PWInventory::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
    };

    BackButton = PWUI::MakeButton(LOCTEXT("Back", "Back"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]()
        {
            RequestClose();
            return FReply::Handled();
        }), true, 20, 0.0f);
    BackButton->SetTag(FName(PWInventory::BackTag));

    const TSharedRef<SWidget> Header =
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("Title", "Inventory"))
            .Font_Lambda([IsCompact]() { return PWUI::Font(IsCompact() ? 22 : 32, true); })
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(16.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]()
            {
                return Model.bAvailable ? PWCommerce::CmWalletText(TOptional<int64>(Model.WalletPaise)) : FText::GetEmpty();
            })
            .Font(PWUI::Font(18, PWUI::EFontWeight::SemiBold))
            .ColorAndOpacity(FSlateColor(Style.Accent))
            .Justification(ETextJustify::Right)
            .AutoWrapText(true)
        ];

    ChildSlot
    [
        SNew(SBox)
        .WidthOverride_Lambda([this]()
        {
            const FVector2D S = EffectiveSpace();
            return FOptionalSize(PWInventory::PanelWidth(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        .MaxDesiredHeight_Lambda([this]()
        {
            const FVector2D S = EffectiveSpace();
            return FOptionalSize(PWInventory::PanelMaxHeight(static_cast<float>(S.X), static_cast<float>(S.Y)));
        })
        [
            SNew(SBorder)
            .Tag(FName(PWInventory::PanelTag))
            .BorderImage(&Style.BrushPanel)
            .Padding(0.0f)
            [
                SNew(SOverlay)
                + SOverlay::Slot()
                [
                    SNew(SPWCornerBrackets)
                ]
                + SOverlay::Slot()
                [
                    SNew(SBox)
                    .Padding_Lambda([IsCompact]() { return IsCompact() ? FMargin(24.0f, 10.0f) : FMargin(40.0f, 28.0f); })
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            Header
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 8.0f)
                        [
                            PWUI::MakeRule()
                        ]
                        + SVerticalBox::Slot().FillHeight(1.0f)
                        [
                            SAssignNew(BodyHolder, SBox)
                            .Tag(FName(PWInventory::BodyTag))
                            [
                                BuildBody()
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
                        [
                            SNew(SBorder)
                            .Tag(FName(PWInventory::MessageTag))
                            .BorderImage(&Style.BrushInset)
                            .Padding(FMargin(12.0f, 6.0f))
                            .Visibility_Lambda([this]() { return Message.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                            [
                                SNew(STextBlock)
                                .Text_Lambda([this]() { return Message; })
                                .Font(PWUI::Font(16, PWUI::EFontWeight::Medium))
                                .ColorAndOpacity_Lambda([this]()
                                {
                                    const PWUI::FStyle& Now = PWUI::FStyle::Get();
                                    return FSlateColor(bMessageIsError ? Now.Error : Now.TextPrimary);
                                })
                                .AutoWrapText(true)
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0.0f, 8.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox).WidthOverride(240.0f)
                            [
                                BackButton.ToSharedRef()
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];

    PWUI::FocusWhenReady(AsShared(), BackButton);
}

TSharedRef<SWidget> SPWInventoryPanel::BuildBody()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    if (!Model.bAvailable)
    {
        return CmInvNote(LOCTEXT("Unavailable", "Your bag cannot be read right now. Go back, resume and try again."), PWInventory::UnavailableTag);
    }
    return SAssignNew(BodyScroll, SScrollBox)
        .ScrollBarStyle(&Style.ScrollBar)
        .AllowOverscroll(EAllowOverscroll::No)
        .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
        + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                BuildBagSection()
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 16.0f, 0.0f, 0.0f)
            [
                BuildReceiptsSection()
            ]
        ];
}

TSharedRef<SWidget> SPWInventoryPanel::BuildBagSection()
{
    const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
    [
        CmInvHeading(FText::Format(LOCTEXT("BagHeading", "In your bag: {0} of {1} spaces"),
                                   FText::AsNumber(Model.Stacks.Num()), FText::AsNumber(Model.SlotCount)))
    ];
    if (Model.Stacks.IsEmpty())
    {
        Box->AddSlot().AutoHeight()
        [
            CmInvNote(LOCTEXT("EmptyBag", "Your bag is empty. Things you buy at a counter will be kept here."), PWInventory::EmptyBagTag)
        ];
    }
    for (const PWCommerce::FCmStackView& Stack : Model.Stacks)
    {
        Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[BuildStackRow(Stack)];
    }
    return Box;
}

TSharedRef<SWidget> SPWInventoryPanel::BuildStackRow(const PWCommerce::FCmStackView& Stack)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const bool bHasUse = PWInventory::HasUse(Stack.Effect);
    // What sits under the name: the core's reason this cannot be used now, or what the item does.
    FText Detail = PWCommerce::CmEffectHint(Stack.Effect);
    if (bHasUse && Stack.UseBlocked != PerfectWorld::CommerceError::None)
    {
        Detail = PWCommerce::CmUseRefusalText(Stack.UseBlocked, Stack.RetryInMinutes);
    }
    const bool bBlocked = bHasUse && Stack.UseBlocked != PerfectWorld::CommerceError::None;
    const FName ItemId = Stack.ItemId;

    const TSharedRef<SVerticalBox> Text = SNew(SVerticalBox);
    Text->AddSlot().AutoHeight()
    [
        PWUI::MakeText(FText::Format(LOCTEXT("StackLine", "{0} × {1}"), PWCommerce::CmItemWithUnit(Stack.Name, Stack.Unit),
                                     FText::AsNumber(Stack.Quantity)), 20, Style.TextPrimary, true, true)
    ];
    if (!Detail.IsEmpty())
    {
        Text->AddSlot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(Detail, PWUI::ETextRole::Meta, bBlocked ? Style.Error : Style.TextSecondary, true)
        ];
    }

    const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center)[Text];
    if (bHasUse)
    {
        const TSharedRef<SWidget> UseButton = PWUI::MakeButton(LOCTEXT("Consume", "Consume"), PWUI::EButtonKind::Secondary,
            FOnClicked::CreateSPLambda(this, [this, ItemId]()
            {
                UseItem(ItemId);
                return FReply::Handled();
            }),
            !bBlocked, 18, 96.0f);
        UseButton->SetTag(FName(PWInventory::UseTag));
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)[UseButton];
    }
    return SNew(SBorder)
        .Tag(FName(PWInventory::StackRowTag))
        .BorderImage(&Style.BrushInset)
        .Padding(FMargin(14.0f, 8.0f))
        [
            Row
        ];
}

TSharedRef<SWidget> SPWInventoryPanel::BuildReceiptsSection()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)[CmInvHeading(LOCTEXT("ReceiptsHeading", "Recent receipts"))];
    if (Model.Receipts.IsEmpty())
    {
        Box->AddSlot().AutoHeight()
        [
            CmInvNote(LOCTEXT("NoReceipts", "Nothing bought yet. Your purchases and your allowance will be listed here."), PWInventory::EmptyReceiptsTag)
        ];
    }
    for (const PWCommerce::FCmReceiptRow& Row : Model.Receipts)
    {
        Box->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
        [
            SNew(SBorder)
            .Tag(FName(PWInventory::ReceiptRowTag))
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(14.0f, 8.0f))
            [
                PWUI::MakeText(PWCommerce::CmReceiptText(Row), 16, Style.TextPrimary, false, true)
            ]
        ];
    }
    return Box;
}

bool SPWInventoryPanel::UseItem(const FName ItemId)
{
    if (!Bridge.IsValid())
    {
        return false;
    }
    // The wait the core reported for this item before the attempt, so a refused cooldown can say how long.
    const PWCommerce::FCmStackView* Stack = Model.Stacks.FindByPredicate([ItemId](const PWCommerce::FCmStackView& Row) { return Row.ItemId == ItemId; });
    const int32 RetryMinutes = Stack != nullptr ? Stack->RetryInMinutes : 0;
    const PWCommerce::FCmUseResult Result = Bridge->Consume(ItemId);
    bMessageIsError = !Result.bAccepted;
    if (Result.bAccepted)
    {
        Message = FText::Format(LOCTEXT("Used", "Used {0}."), Result.ItemName.IsEmpty() ? LOCTEXT("OneItem", "one") : Result.ItemName);
    }
    else
    {
        Message = PWCommerce::CmUseRefusalText(Result.Error, RetryMinutes);
    }
    Reload();
    return Result.bAccepted;
}

void SPWInventoryPanel::Reload()
{
    Model = Bridge.IsValid() ? Bridge->ReadInventory() : PWCommerce::FCmInventoryView();
    if (BodyHolder.IsValid())
    {
        BodyHolder->SetContent(BuildBody());
    }
    // The pressed row may be gone; Back is always there.
    PWUI::FocusWhenReady(AsShared(), BackButton);
}

void SPWInventoryPanel::ScrollBody(const float Delta)
{
    if (BodyScroll.IsValid())
    {
        BodyScroll->SetScrollOffset(FMath::Clamp(BodyScroll->GetScrollOffset() + Delta, 0.0f, BodyScroll->GetScrollOffsetOfEnd()));
    }
}

void SPWInventoryPanel::RequestClose()
{
    OnClose.ExecuteIfBound();
}

FReply SPWInventoryPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    constexpr float StepUnits = 72.0f;
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Escape || Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
    {
        // A held key auto-repeats; this screen goes back on the first press only.
        if (!InKeyEvent.IsRepeat())
        {
            RequestClose();
        }
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_RightStick_Up)
    {
        ScrollBody(-StepUnits);
        return FReply::Handled();
    }
    if (Key == EKeys::Gamepad_RightStick_Down)
    {
        ScrollBody(StepUnits);
        return FReply::Handled();
    }
    if (Key == EKeys::PageUp || Key == EKeys::PageDown)
    {
        const float Page = FMath::Max(StepUnits, static_cast<float>(MyGeometry.GetLocalSize().Y) * 0.5f);
        ScrollBody(Key == EKeys::PageUp ? -Page : Page);
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

#undef LOCTEXT_NAMESPACE
