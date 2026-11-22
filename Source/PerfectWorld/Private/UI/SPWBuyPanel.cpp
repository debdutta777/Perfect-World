#include "UI/SPWBuyPanel.h"

#include "HAL/PlatformTime.h"
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

#define LOCTEXT_NAMESPACE "PerfectWorldBuyPanel"

namespace
{
/** A label over (or, compact, beside) a value: one figure of the totals block. */
TSharedRef<SWidget> CmBuyFigure(const FText& Label, const TAttribute<FText>& Value, const TAttribute<FSlateColor>& Colour, const TFunction<bool()>& IsCompact)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SBox)
        .Padding_Lambda([IsCompact]() { return IsCompact() ? FMargin(0.0f, 1.0f) : FMargin(0.0f, 4.0f); })
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                PWUI::MakeText(Label, PWUI::ETextRole::Meta, Style.TextSecondary)
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(Value)
                .Font(PWUI::Font(20, PWUI::EFontWeight::SemiBold))
                .ColorAndOpacity(Colour)
                .AutoWrapText(true)
            ]
        ];
}
} // namespace

void SPWBuyPanel::Construct(const FArguments& InArgs)
{
    Bridge = InArgs._Bridge;
    Service = InArgs._Service;
    Space = InArgs._Space;
    OnClose = InArgs._OnClose;
    Clock = []() { return FPlatformTime::Seconds(); };
    Selected = 0;
    Quantity = 1;
    ReArm();
    RefreshQuote();

    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto IsCompact = [this]()
    {
        const FVector2D S = EffectiveSpace();
        return PWBuy::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
    };
    const auto InStage = [this](const PWBuy::EStage Wanted) { return Stage == Wanted; };

    // Browsing: goods, quantity, totals and the explanation. Receipt: the receipt line. Each collapses when it is not the stage.
    const TSharedRef<SWidget> BrowsingBody =
        SNew(SVerticalBox)
        .Visibility_Lambda([InStage]() { return InStage(PWBuy::EStage::Browsing) ? EVisibility::Visible : EVisibility::Collapsed; })
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            BuildGoods()
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            BuildQuantityRow()
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            BuildTotals()
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SNew(SBorder)
            .Tag(FName(PWBuy::StatusTag))
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(12.0f, 6.0f))
            .Visibility_Lambda([this]() { return GetStatusText().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return GetStatusText(); })
                .Font(PWUI::Font(16, PWUI::EFontWeight::Medium))
                .ColorAndOpacity_Lambda([this]()
                {
                    const PWUI::FStyle& Now = PWUI::FStyle::Get();
                    const bool bProblem = !Quote.CanBuy() || LastRefusal != PerfectWorld::CommerceError::None;
                    return FSlateColor(bProblem ? Now.Error : Now.TextPrimary);
                })
                .AutoWrapText(true)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
        [
            SNew(SBox)
            .Visibility_Lambda([IsCompact]() { return IsCompact() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
            [
                PWUI::MakeText(LOCTEXT("Hint", "Up and Down choose. Left and Right change the quantity. Enter or A confirms. Esc or B goes back."),
                               PWUI::ETextRole::Meta, Style.TextSecondary, true)
            ]
        ];

    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(&Style.BrushScrim)
        .Padding(0.0f)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride_Lambda([this]()
            {
                const FVector2D S = EffectiveSpace();
                return FOptionalSize(PWBuy::PanelWidth(static_cast<float>(S.X), static_cast<float>(S.Y)));
            })
            .MaxDesiredHeight_Lambda([this]()
            {
                const FVector2D S = EffectiveSpace();
                return FOptionalSize(PWBuy::PanelMaxHeight(static_cast<float>(S.X), static_cast<float>(S.Y)));
            })
            [
                SNew(SBorder)
                .Tag(FName(PWBuy::PanelTag))
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
                        .Padding_Lambda([IsCompact]() { return IsCompact() ? FMargin(20.0f, 10.0f) : FMargin(32.0f, 24.0f); })
                        [
                            SNew(SVerticalBox)
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                BuildHeader()
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 6.0f)
                            [
                                PWUI::MakeRule()
                            ]
                            + SVerticalBox::Slot().FillHeight(1.0f)
                            [
                                BrowsingBody
                            ]
                            + SVerticalBox::Slot().AutoHeight()
                            [
                                BuildReceipt()
                            ]
                            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
                            [
                                BuildFooter()
                            ]
                        ]
                    ]
                ]
            ]
        ]
    ];

    PWUI::FocusWhenReady(AsShared(), TWeakPtr<SWidget>(GoodsHolder));
}

TSharedRef<SWidget> SPWBuyPanel::BuildHeader()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto IsCompact = [this]()
    {
        const FVector2D S = EffectiveSpace();
        return PWBuy::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
    };
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return Service.ShopName; })
            .Font_Lambda([IsCompact]() { return PWUI::Font(IsCompact() ? 22 : 30, true); })
            .ColorAndOpacity(FSlateColor(Style.TextPrimary))
            .AutoWrapText(true)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return PWCommerce::CmWalletText(TOptional<int64>(Service.WalletPaise)); })
            .Font(PWUI::Font(18, PWUI::EFontWeight::SemiBold))
            .ColorAndOpacity(FSlateColor(Style.Accent))
        ];
}

TSharedRef<SWidget> SPWBuyPanel::BuildGoods()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
    for (int32 Index = 0; Index < Service.Goods.Num(); ++Index)
    {
        const PWCommerce::FCmGood& Good = Service.Goods[Index];
        const TSharedRef<SWidget> Row = PWUI::MakeContentButton(
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
            [
                PWUI::MakeText(PWCommerce::CmItemWithUnit(Good.Name, Good.Unit), 19, Style.TextPrimary, true, true)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
            [
                PWUI::MakeText(FText::FromString(PWCommerce::CmFormatPaise(Good.UnitPricePaise)), 19, Style.Accent, true)
            ],
            PWUI::EButtonKind::Card,
            FOnClicked::CreateSPLambda(this, [this, Index]()
            {
                SelectGood(Index);
                return FReply::Handled();
            }),
            TAttribute<bool>::CreateLambda([this, Index]() { return Selected == Index; }),
            true, FMargin(14.0f, 4.0f));
        Row->SetTag(FName(PWBuy::GoodRowTag));
        List->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[Row];
    }
    return SAssignNew(GoodsHolder, SBox)
        .Tag(FName(PWBuy::GoodsTag))
        [
            SAssignNew(GoodsScroll, SScrollBox)
            .ScrollBarStyle(&Style.ScrollBar)
            .AllowOverscroll(EAllowOverscroll::No)
            .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
            + SScrollBox::Slot().Padding(0.0f, 0.0f, 12.0f, 0.0f)
            [
                List
            ]
        ];
}

TSharedRef<SWidget> SPWBuyPanel::BuildQuantityRow()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const TSharedRef<SWidget> Less = PWUI::MakeButton(LOCTEXT("Less", "−"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]() { StepQuantity(-1); return FReply::Handled(); }),
        TAttribute<bool>::CreateLambda([this]() { return Quantity > 1; }), 20, 36.0f);
    const TSharedRef<SWidget> More = PWUI::MakeButton(LOCTEXT("More", "+"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]() { StepQuantity(1); return FReply::Handled(); }),
        TAttribute<bool>::CreateLambda([this]() { return Quantity < PWBuy::MaxQuantity; }), 20, 36.0f);
    Less->SetTag(FName(PWBuy::LessTag));
    More->SetTag(FName(PWBuy::MoreQuantityTag));
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 12.0f, 0.0f)
        [
            PWUI::MakeText(LOCTEXT("Quantity", "Quantity"), PWUI::ETextRole::Control, Style.TextSecondary)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            Less
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).MinDesiredWidth(44.0f).HAlign(HAlign_Center)
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return FText::AsNumber(Quantity); })
                .Font(PWUI::Font(24, PWUI::EFontWeight::SemiBold))
                .ColorAndOpacity(FSlateColor(Style.TextPrimary))
            ]
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            More
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(12.0f, 0.0f, 0.0f, 0.0f)
        [
            PWUI::MakeText(TAttribute<FText>::CreateLambda([this]()
            {
                return Quote.Quote.Ok() ? PWCommerce::CmUnitPriceText(Quote.Quote.UnitPricePaise, Service.Goods[Selected].Unit) : FText::GetEmpty();
            }), PWUI::ETextRole::Meta, Style.TextSecondary, true)
        ];
}

TSharedRef<SWidget> SPWBuyPanel::BuildTotals()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const auto IsCompact = [this]() -> bool
    {
        const FVector2D S = EffectiveSpace();
        return PWBuy::Compact(static_cast<float>(S.X), static_cast<float>(S.Y));
    };
    const TFunction<bool()> Compact = IsCompact;
    const TAttribute<FSlateColor> Plain = FSlateColor(Style.TextPrimary);
    // The shortfall shows in the error colour: the wallet after a purchase it cannot cover is negative.
    const TAttribute<FSlateColor> AfterColour = TAttribute<FSlateColor>::CreateLambda([this]()
    {
        const PWUI::FStyle& Now = PWUI::FStyle::Get();
        return FSlateColor(Quote.Quote.Ok() && Quote.Quote.WalletAfterPaise < 0 ? Now.Error : Now.TextPrimary);
    });
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            CmBuyFigure(LOCTEXT("Total", "Total"), TAttribute<FText>::CreateLambda([this]() { return TotalText(); }), Plain, Compact)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
        [
            CmBuyFigure(LOCTEXT("WalletNow", "Wallet now"), TAttribute<FText>::CreateLambda([this]() { return WalletNowText(); }), Plain, Compact)
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            CmBuyFigure(LOCTEXT("WalletAfter", "Wallet after"), TAttribute<FText>::CreateLambda([this]() { return WalletAfterText(); }), AfterColour, Compact)
        ];
}

FText SPWBuyPanel::TotalText() const
{
    return Quote.Quote.Ok() ? FText::FromString(PWCommerce::CmFormatPaise(Quote.Quote.TotalPaise)) : FText::FromString(TEXT("-"));
}

FText SPWBuyPanel::WalletNowText() const
{
    return FText::FromString(PWCommerce::CmFormatPaise(Quote.Quote.Ok() ? Quote.Quote.WalletBeforePaise : Service.WalletPaise));
}

FText SPWBuyPanel::WalletAfterText() const
{
    return Quote.Quote.Ok() ? FText::FromString(PWCommerce::CmFormatPaise(Quote.Quote.WalletAfterPaise)) : FText::FromString(TEXT("-"));
}

TSharedRef<SWidget> SPWBuyPanel::BuildReceipt()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    return SNew(SVerticalBox)
        .Visibility_Lambda([this]() { return Stage == PWBuy::EStage::Receipt ? EVisibility::Visible : EVisibility::Collapsed; })
        + SVerticalBox::Slot().AutoHeight()
        [
            PWUI::MakeText(LOCTEXT("ReceiptHeading", "Purchase complete"), PWUI::ETextRole::SectionHeading, Style.Accent, true)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
        [
            SNew(SBorder)
            .Tag(FName(PWBuy::ReceiptTag))
            .BorderImage(&Style.BrushInset)
            .Padding(FMargin(14.0f, 10.0f))
            [
                SNew(STextBlock)
                .Text_Lambda([this]() { return PWCommerce::CmReceiptText(Receipt); })
                .Font(PWUI::Font(18, PWUI::EFontWeight::Medium))
                .ColorAndOpacity(FSlateColor(Style.TextPrimary))
                .AutoWrapText(true)
            ]
        ];
}

TSharedRef<SWidget> SPWBuyPanel::BuildFooter()
{
    ConfirmButton = PWUI::MakeButton(LOCTEXT("Confirm", "Confirm"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]() { Confirm(); return FReply::Handled(); }),
        TAttribute<bool>::CreateLambda([this]() { return Quote.CanBuy() && !bSubmitting; }), 20, 110.0f);
    BackButton = PWUI::MakeButton(LOCTEXT("Back", "Back"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]() { RequestClose(); return FReply::Handled(); }), true, 20, 110.0f);
    MoreButton = PWUI::MakeButton(LOCTEXT("BuyMore", "Buy more"), PWUI::EButtonKind::Primary,
        FOnClicked::CreateSPLambda(this, [this]() { BuyMore(); return FReply::Handled(); }), true, 20, 110.0f);
    DoneButton = PWUI::MakeButton(LOCTEXT("Done", "Done"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateSPLambda(this, [this]() { RequestClose(); return FReply::Handled(); }), true, 20, 110.0f);
    ConfirmButton->SetTag(FName(PWBuy::ConfirmTag));
    BackButton->SetTag(FName(PWBuy::BackTag));
    MoreButton->SetTag(FName(PWBuy::MoreTag));
    DoneButton->SetTag(FName(PWBuy::DoneTag));

    const auto OnlyIn = [this](const PWBuy::EStage Wanted) -> EVisibility
    {
        return Stage == Wanted ? EVisibility::Visible : EVisibility::Collapsed;
    };
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            SNew(SHorizontalBox)
            .Visibility_Lambda([OnlyIn]() { return OnlyIn(PWBuy::EStage::Browsing); })
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                ConfirmButton.ToSharedRef()
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                BackButton.ToSharedRef()
            ]
        ]
        + SHorizontalBox::Slot().FillWidth(1.0f)
        [
            SNew(SHorizontalBox)
            .Visibility_Lambda([OnlyIn]() { return OnlyIn(PWBuy::EStage::Receipt); })
            + SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
            [
                MoreButton.ToSharedRef()
            ]
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SAssignNew(DoneHolder, SBox)
                [
                    DoneButton.ToSharedRef()
                ]
            ]
        ];
}

#undef LOCTEXT_NAMESPACE
