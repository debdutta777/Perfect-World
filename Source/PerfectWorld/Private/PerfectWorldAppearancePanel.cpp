#include "PerfectWorldAppearancePanel.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Styling/CoreStyle.h"

namespace
{
/** Finds an option's display name, or "Unknown" if the id is not among the options. */
FString FindOptionName(const TArray<FPWAppearanceOption>& Options, FName Id)
{
    for (const FPWAppearanceOption& Option : Options)
    {
        if (Option.Id == Id)
        {
            return Option.DisplayName.ToString();
        }
    }
    return TEXT("Unknown");
}

int32 ToByte(float LinearChannel)
{
    return FMath::Clamp(FMath::RoundToInt(LinearChannel * 255.f), 0, 255);
}
} // namespace

TSharedRef<SWidget> BuildAppearancePanel(TWeakObjectPtr<UPerfectWorldCharacterCreatorSubsystem> Creator,
                                          FSimpleDelegate OnContinue, FSimpleDelegate OnBack)
{
    const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24);
    const FSlateFontInfo SectionFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 17);
    const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 17);
    const FSlateFontInfo ButtonFont = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 16);

    // One row of buttons per discrete picker. Selection is re-read from the
    // subsystem on every redraw, never cached, and marked [x]/[ ] the same
    // way the household picker does in PerfectWorldCreatorPanel.cpp.
    auto MakeOptionRow = [Creator, BodyFont](TFunction<TArray<FPWAppearanceOption>()> GetOptions,
                                              TFunction<void(FName)> SetOption)
    {
        TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
        if (Creator.IsValid())
        {
            for (const FPWAppearanceOption& Option : GetOptions())
            {
                const FName Id = Option.Id;
                Row->AddSlot().AutoWidth().Padding(0, 0, 6, 6)[
                    SNew(SButton)
                    .ContentPadding(FMargin(10, 6))
                    .OnClicked_Lambda([SetOption, Id]
                    {
                        SetOption(Id);
                        return FReply::Handled();
                    })
                    [SNew(STextBlock)
                        .Font(BodyFont)
                        .Text_Lambda([GetOptions, Id]
                        {
                            for (const FPWAppearanceOption& Current : GetOptions())
                            {
                                if (Current.Id == Id)
                                {
                                    return FText::FromString(FString::Printf(TEXT("%s %s"),
                                        Current.bSelected ? TEXT("[x]") : TEXT("[ ]"), *Current.DisplayName.ToString()));
                                }
                            }
                            return FText::GetEmpty();
                        })]
                ];
            }
        }
        return Row;
    };

    // One R/G/B slider row. Sliders stay in Slate's native 0-1 range (matching
    // the FLinearColor channel they edit) and show the 0-255 value alongside,
    // since the sliders-vs-swatches choice landed on sliders: see the report
    // for why (precise numeric control matters more here than quick picks).
    auto MakeChannelRow = [BodyFont](const TCHAR* ChannelLabel, TFunction<float()> GetChannel,
                                      TFunction<void(float)> SetChannel)
    {
        return SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0).VAlign(VAlign_Center)[SNew(STextBlock)
                .Font(BodyFont)
                .Text(FText::FromString(ChannelLabel))]
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SSlider)
                .Value_Lambda([GetChannel] { return GetChannel(); })
                .OnValueChanged_Lambda([SetChannel](float NewValue) { SetChannel(NewValue); })]
            + SHorizontalBox::Slot().AutoWidth().Padding(8, 0, 0, 0).VAlign(VAlign_Center)[SNew(SBox).MinDesiredWidth(34)[
                SNew(STextBlock)
                .Font(BodyFont)
                .Text_Lambda([GetChannel] { return FText::AsNumber(ToByte(GetChannel())); })]];
    };

    auto MakeColorBlock = [MakeChannelRow, SectionFont](const TCHAR* Label, TFunction<FLinearColor()> GetColor,
                                                          TFunction<void(FLinearColor)> SetColor)
    {
        TFunction<float()> GetR = [GetColor] { return GetColor().R; };
        TFunction<float()> GetG = [GetColor] { return GetColor().G; };
        TFunction<float()> GetB = [GetColor] { return GetColor().B; };
        TFunction<void(float)> SetR = [GetColor, SetColor](float V) { FLinearColor C = GetColor(); C.R = V; SetColor(C); };
        TFunction<void(float)> SetG = [GetColor, SetColor](float V) { FLinearColor C = GetColor(); C.G = V; SetColor(C); };
        TFunction<void(float)> SetB = [GetColor, SetColor](float V) { FLinearColor C = GetColor(); C.B = V; SetColor(C); };

        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[SNew(STextBlock).Font(SectionFont).Text(FText::FromString(Label))]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[MakeChannelRow(TEXT("R"), GetR, SetR)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[MakeChannelRow(TEXT("G"), GetG, SetG)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[MakeChannelRow(TEXT("B"), GetB, SetB)];
    };

    TFunction<TArray<FPWAppearanceOption>()> GetFaceShapes = [Creator]
    { return Creator.IsValid() ? Creator->GetFaceShapeOptions() : TArray<FPWAppearanceOption>(); };
    TFunction<TArray<FPWAppearanceOption>()> GetHairStyles = [Creator]
    { return Creator.IsValid() ? Creator->GetHairStyleOptions() : TArray<FPWAppearanceOption>(); };
    TFunction<TArray<FPWAppearanceOption>()> GetBuilds = [Creator]
    { return Creator.IsValid() ? Creator->GetBuildOptions() : TArray<FPWAppearanceOption>(); };
    TFunction<TArray<FPWAppearanceOption>()> GetUniformVariants = [Creator]
    { return Creator.IsValid() ? Creator->GetUniformVariantOptions() : TArray<FPWAppearanceOption>(); };

    TFunction<FLinearColor()> GetSkinTone = [Creator]
    { return Creator.IsValid() ? Creator->GetAppearance().SkinTone : FLinearColor::White; };
    TFunction<FLinearColor()> GetHairColor = [Creator]
    { return Creator.IsValid() ? Creator->GetAppearance().HairColor : FLinearColor::White; };
    TFunction<FLinearColor()> GetEyeColor = [Creator]
    { return Creator.IsValid() ? Creator->GetAppearance().EyeColor : FLinearColor::White; };

    return SNew(SBorder).Padding(28).BorderBackgroundColor(FLinearColor(0.025f, 0.045f, 0.07f, 1))[
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)[SNew(STextBlock)
            .Font(TitleFont)
            .Text(FText::FromString(TEXT("Appearance")))]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[SNew(STextBlock)
            .Font(BodyFont)
            .AutoWrapText(true)
            .Text(FText::FromString(TEXT("There is no character art yet. This screen edits the saved appearance record only; a portrait or model will render it once the art pipeline exists.")))]
        + SVerticalBox::Slot().FillHeight(1).Padding(0, 0, 0, 10)[SNew(SScrollBox)
            + SScrollBox::Slot()[
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[SNew(STextBlock).Font(SectionFont).Text(FText::FromString(TEXT("Face shape")))]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[MakeOptionRow(GetFaceShapes,
                    [Creator](FName Id) { if (Creator.IsValid()) Creator->SetFaceShape(Id); })]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[SNew(STextBlock).Font(SectionFont).Text(FText::FromString(TEXT("Hair style")))]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[MakeOptionRow(GetHairStyles,
                    [Creator](FName Id) { if (Creator.IsValid()) Creator->SetHairStyle(Id); })]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[SNew(STextBlock).Font(SectionFont).Text(FText::FromString(TEXT("Build")))]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)[MakeOptionRow(GetBuilds,
                    [Creator](FName Id) { if (Creator.IsValid()) Creator->SetBuild(Id); })]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 2)[SNew(STextBlock).Font(SectionFont).Text(FText::FromString(TEXT("Uniform variant")))]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[MakeOptionRow(GetUniformVariants,
                    [Creator](FName Id) { if (Creator.IsValid()) Creator->SetUniformVariant(Id); })]
                + SVerticalBox::Slot().AutoHeight()[MakeColorBlock(TEXT("Skin tone"), GetSkinTone,
                    [Creator](FLinearColor C) { if (Creator.IsValid()) Creator->SetSkinColor(C); })]
                + SVerticalBox::Slot().AutoHeight()[MakeColorBlock(TEXT("Hair colour"), GetHairColor,
                    [Creator](FLinearColor C) { if (Creator.IsValid()) Creator->SetHairColor(C); })]
                + SVerticalBox::Slot().AutoHeight()[MakeColorBlock(TEXT("Eye colour"), GetEyeColor,
                    [Creator](FLinearColor C) { if (Creator.IsValid()) Creator->SetEyeColor(C); })]
            ]]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 14)[SNew(STextBlock)
            .Font(BodyFont)
            .AutoWrapText(true)
            .Text_Lambda([Creator]
            {
                if (!Creator.IsValid()) return FText::GetEmpty();
                const FPWAppearanceRecord Appearance = Creator->GetAppearance();
                const FString Face = FindOptionName(Creator->GetFaceShapeOptions(), Appearance.FaceShapeId);
                const FString Hair = FindOptionName(Creator->GetHairStyleOptions(), Appearance.HairStyleId);
                const FString BuildName = FindOptionName(Creator->GetBuildOptions(), Appearance.BuildId);
                const FString Uniform = FindOptionName(Creator->GetUniformVariantOptions(), Appearance.UniformVariantId);
                return FText::FromString(FString::Printf(
                    TEXT("%s face, %s hair, %s build, %s; skin %d,%d,%d; hair %d,%d,%d; eyes %d,%d,%d"),
                    *Face, *Hair, *BuildName, *Uniform,
                    ToByte(Appearance.SkinTone.R), ToByte(Appearance.SkinTone.G), ToByte(Appearance.SkinTone.B),
                    ToByte(Appearance.HairColor.R), ToByte(Appearance.HairColor.G), ToByte(Appearance.HairColor.B),
                    ToByte(Appearance.EyeColor.R), ToByte(Appearance.EyeColor.G), ToByte(Appearance.EyeColor.B)));
            })]
        + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)[SNew(SBox).WidthOverride(160)[SNew(SButton)
                .ContentPadding(FMargin(14, 8))
                .OnClicked_Lambda([OnBack]
                {
                    OnBack.ExecuteIfBound();
                    return FReply::Handled();
                })
                [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Back")))]]]
            + SHorizontalBox::Slot().FillWidth(1)[SNew(SBox)]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(160)[SNew(SButton)
                .ContentPadding(FMargin(14, 8))
                .OnClicked_Lambda([OnContinue]
                {
                    OnContinue.ExecuteIfBound();
                    return FReply::Handled();
                })
                [SNew(STextBlock).Font(ButtonFont).Justification(ETextJustify::Center).Text(FText::FromString(TEXT("Continue")))]]]
        ]
    ];
}
