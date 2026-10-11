#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FPWCreationModel;
class SEditableTextBox;

/** Step 3: names, date of birth inside the valid window, pronouns and primary language. */
class SPWIdentityPage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPWIdentityPage) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<FPWCreationModel>& InModel);

private:
    /** One part of the date of birth: 0 day, 1 month, 2 year. Direction is +1 or -1. */
    void StepBirth(int32 Part, int32 Direction);
    TSharedRef<SWidget> BuildDatePart(const FText& Label, TAttribute<FText> Value, int32 Part);
    TSharedRef<SWidget> BuildDatePanel();
    TSharedRef<SWidget> BuildChipRow(const TArray<FString>& Choices, bool bPronouns);

    TSharedPtr<FPWCreationModel> Model;
    TSharedPtr<SEditableTextBox> PronounsBox;
    TSharedPtr<SEditableTextBox> LanguageBox;
};
