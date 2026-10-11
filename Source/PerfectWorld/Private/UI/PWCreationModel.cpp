#include "UI/PWCreationModel.h"

FPWCreationModel::FPWCreationModel(UPerfectWorldCharacterCreatorSubsystem* InSubsystem)
    : Subsystem(InSubsystem)
{
    Refresh();
}

void FPWCreationModel::Refresh()
{
    const UPerfectWorldCharacterCreatorSubsystem* Creator = Subsystem.Get();
    if (Creator == nullptr)
    {
        return;
    }
    Households = Creator->GetHouseholds();
    Traits = Creator->GetTraits();
    Presets = Creator->GetPresets();
    Streams = Creator->GetSchoolStreams();
    Identity = Creator->GetIdentity();
    SchoolReason = Creator->GetSchoolReason();
    OpeningHome = Creator->GetOpeningHome();
    Summary = Creator->GetCreationSummary();
}

bool FPWCreationModel::Commit(bool bSucceeded)
{
    if (bSucceeded)
    {
        Message = FText::GetEmpty();
    }
    else if (const UPerfectWorldCharacterCreatorSubsystem* Creator = Subsystem.Get())
    {
        Message = Creator->GetLastErrorMessage();
    }
    Refresh();
    return bSucceeded;
}

bool FPWCreationModel::Edit(TFunctionRef<bool(UPerfectWorldCharacterCreatorSubsystem&)> Change)
{
    UPerfectWorldCharacterCreatorSubsystem* Creator = Subsystem.Get();
    return Commit(Creator != nullptr && Change(*Creator));
}

void FPWCreationModel::ClearMessage()
{
    Message = FText::GetEmpty();
}
