// Shared helpers of the R8-05 commerce UI tests (PerfectWorldCommerceText/BuyPanel/Inventory/Counter Tests.cpp). The widgets are the real ones,
// built without a window: SlatePrepass evaluates bound text and visibility, buttons are clicked with SButton::SimulateClick (which does not
// look at the enabled state, so tests read IsEnabled themselves), and layout is checked by arranging the tree into a root geometry.
#pragma once

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/Geometry.h"
#include "PerfectWorldCommerceBridge.h"
#include "PerfectWorldCommerceFake.h"
#include "PerfectWorldCommerceText.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace PWCommerceTest
{
/** The paise of a rupee amount, to keep the cases readable: CmRupees(25) is 2500. */
inline int64 CmRupees(const int64 Rupees) { return Rupees * 100; }

inline FString CmRs(const int64 Paise) { return PWCommerce::CmFormatPaise(Paise); }

/** Visits every reachable widget, telling it whether it and everything above it is shown. */
inline void CmWalk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
    const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
    Visit(Widget, bShown);
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
    {
        CmWalk(Children->GetChildAt(Index), bShown, Visit);
    }
}

inline TArray<FString> CmShownTexts(const TSharedRef<SWidget>& Root)
{
    Root->SlatePrepass(1.0f);
    TArray<FString> Texts;
    CmWalk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock"))
        {
            Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
        }
    });
    return Texts;
}

inline bool CmHas(const TArray<FString>& Texts, const FString& Exact) { return Texts.Contains(Exact); }

inline bool CmAnyContains(const TArray<FString>& Texts, const FString& Part)
{
    for (const FString& Text : Texts)
    {
        if (Text.Contains(Part))
        {
            return true;
        }
    }
    return false;
}

inline int32 CmCountTag(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
    Root->SlatePrepass(1.0f);
    int32 Count = 0;
    CmWalk(Root, true, [&Count, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTag() == FName(Tag)) ++Count;
    });
    return Count;
}

/** The first shown widget with this tag, or null. */
inline TSharedPtr<SWidget> CmFindTag(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
    Root->SlatePrepass(1.0f);
    TSharedPtr<SWidget> Found;
    CmWalk(Root, true, [&Found, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (!Found.IsValid() && bShown && Widget->GetTag() == FName(Tag)) Found = Widget;
    });
    return Found;
}

/** Clicks the first shown button with this tag. False when there is none. */
inline bool CmClickTag(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
    const TSharedPtr<SWidget> Found = CmFindTag(Root, Tag);
    if (!Found.IsValid() || Found->GetTypeAsString() != TEXT("SButton")) return false;
    StaticCastSharedPtr<SButton>(Found)->SimulateClick();
    Root->SlatePrepass(1.0f);
    return true;
}

inline bool CmIsEnabledTag(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
    const TSharedPtr<SWidget> Found = CmFindTag(Root, Tag);
    return Found.IsValid() && Found->IsEnabled();
}

inline FKeyEvent CmKey(const FKey Key, const bool bRepeat = false)
{
    return FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0);
}

/** Arranges the tree into a root geometry of this size and returns the geometry of the first shown widget with the tag. */
inline bool CmFindArranged(const TSharedRef<SWidget>& Widget, const FGeometry& Geometry, const FName Tag, FGeometry& OutGeometry)
{
    if (!Widget->GetVisibility().IsVisible())
    {
        return false;
    }
    if (Widget->GetTag() == Tag)
    {
        OutGeometry = Geometry;
        return true;
    }
    FArrangedChildren Arranged(EVisibility::All);
    Widget->ArrangeChildren(Geometry, Arranged);
    for (int32 Index = 0; Index < Arranged.Num(); ++Index)
    {
        if (CmFindArranged(Arranged[Index].Widget, Arranged[Index].Geometry, Tag, OutGeometry))
        {
            return true;
        }
    }
    return false;
}

inline bool CmArrangedTag(const TSharedRef<SWidget>& Root, const FVector2D& Size, const TCHAR* Tag, FGeometry& OutGeometry)
{
    Root->SlatePrepass(1.0f);
    return CmFindArranged(Root, FGeometry::MakeRoot(Size, FSlateLayoutTransform()), FName(Tag), OutGeometry);
}

inline bool CmInside(const FGeometry& Inner, const FGeometry& Outer, const float Slack = 0.5f)
{
    const FVector2D InnerPos(Inner.GetAbsolutePosition());
    const FVector2D InnerSize(Inner.GetAbsoluteSize());
    const FVector2D OuterPos(Outer.GetAbsolutePosition());
    const FVector2D OuterSize(Outer.GetAbsoluteSize());
    return InnerPos.X >= OuterPos.X - Slack && InnerPos.Y >= OuterPos.Y - Slack
        && InnerPos.X + InnerSize.X <= OuterPos.X + OuterSize.X + Slack && InnerPos.Y + InnerSize.Y <= OuterPos.Y + OuterSize.Y + Slack;
}

/** The standard fake with extra goods in the stationery shop, so a list is longer than a short window can show. */
inline TSharedRef<PWCommerce::FCmFakeCommerce> CmFakeWithLongStationery(const int32 Rank, const int32 Extra)
{
    TSharedRef<PWCommerce::FCmFakeCommerce> Fake = PWCommerce::FCmFakeCommerce::MakeStandard(Rank);
    PWCommerce::FCmFakeCommerce::FShop* Shop = Fake->Shops.FindByPredicate([](const PWCommerce::FCmFakeCommerce::FShop& S) { return S.Id == FName(TEXT("shop.stationery")); });
    for (int32 Index = 0; Shop != nullptr && Index < Extra; ++Index)
    {
        PerfectWorld::CommerceItem Item;
        Item.Id = std::string("item.extra") + std::to_string(Index);
        Item.Name = std::string("Extra Item Number ") + std::to_string(Index + 1);
        Item.Unit = "piece";
        Fake->Items.Add(Item);
        PerfectWorld::CommerceCatalogueEntry Entry;
        Entry.ShopId = "shop.stationery";
        Entry.ItemId = Item.Id;
        Entry.UnitPricePaise = 1000 + 100 * Index;
        Shop->Entries.Add(Entry);
    }
    return Fake;
}
} // namespace PWCommerceTest

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
