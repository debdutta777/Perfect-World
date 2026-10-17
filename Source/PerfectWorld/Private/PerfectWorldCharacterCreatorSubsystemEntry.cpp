#include "PerfectWorldCharacterCreatorSubsystem.h"

#include "LaneOrigin.h"

FPWLaneOriginView UPerfectWorldCharacterCreatorSubsystem::GetLaneOrigin() const
{
    const PerfectWorld::LaneOriginResult EnOrigin = PerfectWorld::ResolveLaneOrigin(Creator.ResolveOpeningHome());
    FPWLaneOriginView EnView;
    if (EnOrigin.Ok())
    {
        EnView.bResolved = true;
        EnView.HomeLocationId = FName(ToFString(EnOrigin.Binding.LocationId));
        EnView.CounterpartPersonId = FName(ToFString(EnOrigin.Binding.CounterpartPersonId));
        EnView.CounterpartLabel = FText::FromString(ToFString(EnOrigin.Binding.RelationshipLabel));
        EnView.bGrewUpInCare = EnOrigin.Binding.GrewUpInCare;
    }
    return EnView;
}

bool UPerfectWorldCharacterCreatorSubsystem::HasLaneOriginResidency(FName LocationId) const
{
    const PerfectWorld::LaneOriginResult EnOrigin = PerfectWorld::ResolveLaneOrigin(Creator.ResolveOpeningHome());
    const std::string EnLocation(TCHAR_TO_UTF8(*LocationId.ToString()));
    return EnOrigin.Ok() && PerfectWorld::HasLaneOriginResidency(EnOrigin.Binding, EnLocation);
}

bool UPerfectWorldCharacterCreatorSubsystem::StartLifeAndSaveToSlot(int32 SlotIndex)
{
    // Copy the creator itself: CaptureSave/RestoreSave would discard allocation undo/redo history.
    PerfectWorld::CharacterCreator EnDraft = Creator;
    if (!StartLife())
    {
        return false;
    }
    if (!SaveToSlot(SlotIndex))
    {
        Creator = MoveTemp(EnDraft);
        // Keep the slot writer's error and message for the existing creator confirmation flow.
        return false;
    }
    return true;
}
