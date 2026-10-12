#include "UI/SPWHouseholdPage.h"

#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Input/NavigationReply.h"
#include "InputCoreTypes.h"
#include "UI/PWCreationModel.h"
#include "UI/PWStyle.h"
#include "UI/PWWidgets.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "PerfectWorldHouseholdPage"

using PWUI::ETextRole;

void SPWHouseholdPage::Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel)
{
    Model = InModel;
    NarrowLayout = InArgs._NarrowLayout;
    DetailReveal = DetailTransition.AddCurve(0, .16f, ECurveEaseFunction::CubicOut);
    OnEditResources = InArgs._OnEditResources;
    OnFocusDetails = InArgs._OnFocusDetails;
    OnFocusFooter = InArgs._OnFocusFooter;
    SAssignNew(Root, SVerticalBox);
    ChildSlot[Root.ToSharedRef()];
    for (int32 Index = 0; Index < Model->Households.Num(); ++Index) HouseholdRows.Add(BuildHouseholdRow(Index));
    Details = BuildDetails();
    RebuildLayout(false);
}

void SPWHouseholdPage::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    if (LastHousehold != Model->OpeningHome.HouseholdTemplateId || LastResourceRank != Model->OpeningHome.ResourceRank)
    {
        LastHousehold = Model->OpeningHome.HouseholdTemplateId;
        LastResourceRank = Model->OpeningHome.ResourceRank;
        DetailTransition.Play(AsShared());
    }
    Details->SetRenderOpacity(DetailReveal.GetLerp());
    const bool bShouldBeNarrow = NarrowLayout.Get(AllottedGeometry.GetLocalSize().X < 1280.0f);
    if (!bLayoutInitialized || bShouldBeNarrow != bNarrow) RebuildLayout(bShouldBeNarrow);
}

void SPWHouseholdPage::RebuildLayout(bool bUseNarrowLayout)
{
    TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
    bNarrow = bUseNarrowLayout;
    bLayoutInitialized = true;
    Root->ClearChildren();
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    Root->AddSlot().AutoHeight()[PWUI::MakeText(LOCTEXT("Title", "Where did you grow up?"), ETextRole::PageHeading, Style.TextPrimary)];
    Root->AddSlot().AutoHeight().Padding(0, 6, 0, 18)
    [PWUI::MakeText(LOCTEXT("Intro", "Choose your starting household. This choice costs no points. Household Resources can change your opening home."), ETextRole::Body, Style.TextSecondary, true)];
    const TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
    List->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
    [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model->OpeningHome.bGrewUpInCare ? LOCTEXT("RememberedHousehold", "Remembered household") : FText::GetEmpty(); }), ETextRole::Meta, Style.TextMuted)];
    for (const TSharedPtr<SWidget>& Row : HouseholdRows) List->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[Row.ToSharedRef()];
    List->AddSlot().AutoHeight()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return HouseholdProblem; }), ETextRole::Body, Style.Error, true)];
    if (bNarrow)
    {
        Root->AddSlot().AutoHeight()[List];
        Root->AddSlot().AutoHeight().Padding(0, 14, 0, 0)[Details.ToSharedRef()];
    }
    else
    {
        Root->AddSlot().AutoHeight()[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(.35f)[List]
            + SHorizontalBox::Slot().FillWidth(.65f).Padding(24, 0, 0, 0)[Details.ToSharedRef()]];
    }
    if (Focused.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Focused, EFocusCause::SetDirectly);
}

FReply SPWHouseholdPage::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    int32 FocusedIndex = INDEX_NONE;
    for (int32 Index = 0; Index < HouseholdRows.Num(); ++Index)
    {
        if (HouseholdRows[Index]->HasKeyboardFocus() || HouseholdRows[Index]->HasFocusedDescendants()) FocusedIndex = Index;
    }
    if ((Key == EKeys::Up || Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Down) && FocusedIndex != INDEX_NONE)
    {
        const int32 Delta = Key == EKeys::Down || Key == EKeys::Gamepad_DPad_Down ? 1 : -1;
        FSlateApplication::Get().SetKeyboardFocus(HouseholdRows[(FocusedIndex + Delta + HouseholdRows.Num()) % HouseholdRows.Num()], EFocusCause::Navigation);
        return FReply::Handled();
    }
    if (Key == EKeys::F6)
    {
        const bool bInRows = FocusedIndex != INDEX_NONE;
        TSharedPtr<SWidget> Target = bInRows ? EditResourcesWidget : (HouseholdRows.Num() ? HouseholdRows[0] : nullptr);
        if (Target.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Target, EFocusCause::Navigation);
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

TSharedRef<SWidget> SPWHouseholdPage::BuildHouseholdRow(int32 Index)
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    const FPWHouseholdView& Household = Model->Households[Index];
    const FName Id = Household.Id;
    const TSharedRef<SWidget> Content = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(Household.DisplayName, ETextRole::Control, Style.TextPrimary)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)[PWUI::MakeText(Household.GuardianExamples, ETextRole::Meta, Style.TextSecondary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3, 0, 0)
          [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model->OpeningHome.bGrewUpInCare ? LOCTEXT("RememberedForRanks", "Remembered for ranks 1–4") : FText::GetEmpty(); }), ETextRole::Meta, Style.TextMuted, true)];
    return PWUI::MakeContentButton(Content, PWUI::EButtonKind::TextRow,
        FOnClicked::CreateLambda([this, Id]()
        {
            const bool bApplied = Model->Edit([Id](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetHouseholdTemplate(Id); });
            HouseholdProblem = bApplied ? FText::GetEmpty() : Model->Message;
            return FReply::Handled();
        }), TAttribute<bool>::CreateLambda([this, Index]() { return Model->Households.IsValidIndex(Index) && Model->Households[Index].bSelected; }), true, FMargin(16, 12));
}

TSharedRef<SWidget> SPWHouseholdPage::BuildDetails()
{
    const PWUI::FStyle& Style = PWUI::FStyle::Get();
    EditResourcesWidget = PWUI::MakeButton(LOCTEXT("EditResources", "Edit Household Resources"), PWUI::EButtonKind::Secondary,
        FOnClicked::CreateLambda([this]() { OnEditResources.ExecuteIfBound(); return FReply::Handled(); }),
        true, 16);
    return PWUI::MakePanel(SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model->OpeningHome.bGrewUpInCare ? LOCTEXT("CareTitle", "Growing up in a children's home") : LOCTEXT("HomeTitle", "Your opening home"); }), ETextRole::SectionHeading, Style.TextPrimary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return FText::Format(LOCTEXT("HomeFmt", "{0} · {1}"), Model->OpeningHome.HomeKitName, Model->OpeningHome.DressingTierName); }), ETextRole::Control, Style.Accent, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 18)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model->OpeningHome.DressingDescription; }), ETextRole::Body, Style.TextSecondary, true)]
        + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(LOCTEXT("EverydayHeading", "Everyday life"), ETextRole::SectionHeading, Style.TextPrimary)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 18)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return EverydayText(); }), ETextRole::Body, Style.TextSecondary, true)]
        + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(LOCTEXT("CareHeading", "Care and support"), ETextRole::SectionHeading, Style.TextPrimary)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 8)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model->OpeningHome.bGrewUpInCare ? LOCTEXT("StaffCaregiver", "A staff caregiver") : Model->Summary.GuardianText; }), ETextRole::Control, Style.TextPrimary, true)]
        + SVerticalBox::Slot().AutoHeight()[PWUI::MakeText(LOCTEXT("CommonCare", "Resources, support and housing security are separate choices. Every starting home has a place to sleep, meals, storage and access to school."), ETextRole::Body, Style.TextSecondary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 0)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model->OpeningHome.bGrewUpInCare ? LOCTEXT("RememberedStoryHeading", "Remembered household: story emphasis") : LOCTEXT("StoryHeading", "Story emphasis"); }), ETextRole::SectionHeading, Style.TextPrimary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)[PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { for (const FPWHouseholdView& Household : Model->Households) if (Household.bSelected) return Household.StoryEmphasis; return FText::GetEmpty(); }), ETextRole::Body, Style.TextSecondary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 0)
          [PWUI::MakeText(TAttribute<FText>::CreateLambda([this]() { return Model->OpeningHome.bGrewUpInCare ? LOCTEXT("CareExplanation", "Household Resources is rank 0. Your opening home is a children's home. Your household choice is remembered for ranks 1–4.") : FText::GetEmpty(); }), ETextRole::Body, Style.TextSecondary, true)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
          [EditResourcesWidget.ToSharedRef()]);
}

FText SPWHouseholdPage::EverydayText() const
{
    if (Model->OpeningHome.bGrewUpInCare) return LOCTEXT("CareEveryday", "School mornings begin alongside other residents, with staff helping the day get started.");
    for (const FPWHouseholdView& Household : Model->Households)
    {
        if (!Household.bSelected) continue;
        return Household.EverydayText;
    }
    return FText::GetEmpty();
}

FNavigationReply SPWHouseholdPage::OnNavigation(const FGeometry& Geometry, const FNavigationEvent& Event)
{
    const auto Direction = Event.GetNavigationType();
    if (Direction == EUINavigation::Up || Direction == EUINavigation::Down)
        for (int32 Index = 0; Index < HouseholdRows.Num(); ++Index)
            if (HouseholdRows[Index]->HasKeyboardFocus() || HouseholdRows[Index]->HasFocusedDescendants())
                return FNavigationReply::Explicit(HouseholdRows[(Index + (Direction == EUINavigation::Down ? 1 : HouseholdRows.Num() - 1)) % HouseholdRows.Num()]);
    return SCompoundWidget::OnNavigation(Geometry, Event);
}

void SPWHouseholdPage::FocusRegion(bool bReverse, bool bFromFooter)
{
    for (const auto& Row : HouseholdRows) if (Row->HasKeyboardFocus() || Row->HasFocusedDescendants()) Region = 0;
    if (EditResourcesWidget->HasKeyboardFocus() || EditResourcesWidget->HasFocusedDescendants()) Region = 1;
    if (bFromFooter) Region = 2;
    Region = (Region + (bReverse ? 2 : 1)) % 3;
    if (Region == 1) OnFocusDetails.ExecuteIfBound(Details);
    else if (Region == 2) OnFocusFooter.ExecuteIfBound();
    else if (!HouseholdRows.IsEmpty()) PWUI::FocusWhenReady(AsShared(), HouseholdRows[0]);
}

#undef LOCTEXT_NAMESPACE
