#include "PerfectWorldCharacterCreatorSubsystem.h"

#include "LaneOrigin.h"
#include "CreationSaveSerialization.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/Paths.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldLaneLifePersistence.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldReadingPresenter.h"

namespace
{
std::string Pb2SlotPath(const FString& Path)
{
    return TCHAR_TO_UTF8(*FPaths::ConvertRelativePathToFull(Path));
}

FText Pb2ReplacementRefusal(const UObject* Context)
{
    const auto* Talk = UPWConversationPresenter::Get(Context);
    const auto* Reading = UPWReadingPresenter::Get(Context);
    if ((Talk != nullptr && Talk->IsOpen()) || (Reading != nullptr && Reading->IsOpen()))
        return FText::FromString(TEXT("Close the conversation or reading panel before starting or loading a life."));
    const auto* World = Context->GetWorld();
    const auto* Player = World != nullptr ? World->GetFirstPlayerController() : nullptr;
    const auto* Pawn = Player != nullptr ? Cast<APerfectWorldPlayerCharacter>(Player->GetPawn()) : nullptr;
    if (Pawn != nullptr && Pawn->IsSeated())
        return FText::FromString(TEXT("Stand up before starting or loading a life."));
    return FText::GetEmpty();
}
}

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
    if (!CanReplaceWorld()) return false;
    auto Started = Creator; // Preserve the entire draft and undo history on failure.
    if (!RecordResult(Started.StartLife())) return false;
    const FString Path = GetSaveSlotPath(SlotIndex);
    auto* Life = UPWLaneLifeSubsystem::Get(this);
    if (Path.IsEmpty() || Life == nullptr)
        return RecordFileError(FText::FromString(TEXT("The world or save slot is unavailable.")));
    PerfectWorld::WorldSaveData Fresh;
    const auto Begun = PerfectWorld::BeginWorldFromCreation(Started.CaptureSave(), 0x50575F4C414E45ull, Fresh);
    if (!Begun.Succeeded()) return RecordFileError(FText::FromString(ToFString(Begun.Message)));
    auto Prepared = MakeUnique<PerfectWorld::WorldSim>(Fresh.World);
    if (!Prepared->Submit({PerfectWorld::CommandSource::System,
        PerfectWorld::AdvanceTime{PWLaneLife::CampaignStartMinuteOfDay - 360}}).Accepted)
        return RecordFileError(FText::FromString(TEXT("The new world's clock could not start.")));
    // The core's supported origin-home zero marker is resolved safely on entry, also after a restart.
    const auto Capture = PWLaneLifePersistence::CaptureWorldSave(Started.CaptureSave(), Prepared->State(), Fresh.Player);
    if (!Capture.Succeeded()) return RecordFileError(FText::FromString(ToFString(Capture.Validation.Message)));
    const auto Written = PWLaneLifePersistence::WriteWorldSlot(Pb2SlotPath(Path), Capture.Text);
    if (!Written.Ok) return RecordFileError(FText::FromString(ToFString(Written.Message)));
    Creator = MoveTemp(Started);
    // Fresh entry follows the same origin-marker and explicit camera path as a restart.
    LoadedPlacement = Fresh.Player;
    Life->AdoptSavedWorld(MoveTemp(Prepared));
    LastError = EPWCreationError::None;
    LastErrorMessage = FText::GetEmpty();
    return true;
}

bool UPerfectWorldCharacterCreatorSubsystem::SaveToSlot(int32 SlotIndex)
{
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty()) return RecordFileError(FText::FromString(TEXT("That save slot does not exist.")));
    PerfectWorld::SaveOperationResult Written;
    if (!Creator.HasStarted())
    {
        const auto Pb2DraftValidator = [](std::string_view Text, std::string& Error)
        {
            PerfectWorld::CharacterCreationSave Scratch;
            const auto Result = PerfectWorld::DeserializeCreationSave(Text, Scratch);
            Error = Result.Message;
            return Result.Succeeded();
        };
        Written = PerfectWorld::WriteSaveFile(Pb2SlotPath(Path),
            PerfectWorld::SerializeCreationSave(Creator.CaptureSave()), Pb2DraftValidator);
    }
    else
    {
        auto* Life = UPWLaneLifeSubsystem::Get(this);
        const auto* Player = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
        const auto* Pawn = Player != nullptr ? Cast<APerfectWorldPlayerCharacter>(Player->GetPawn()) : nullptr;
        if (Life == nullptr || Pawn == nullptr || Pawn->IsSeated() || Life->HasMovingDoors())
            return RecordFileError(FText::FromString(TEXT("Stand up and let the doors settle before saving. The earlier save is unchanged.")));
        PerfectWorld::PlayerPlacement Placement;
        // The built lane and its interiors are currently one loaded zone.
        Placement.ZoneId = "zone.street.nabapara_lane";
        const FVector Position = Pawn->GetActorLocation();
        for (int32 Axis = 0; Axis < 3; ++Axis)
        {
            if (!FMath::IsFinite(Position[Axis]) || FMath::Abs(Position[Axis]) > PerfectWorld::WorldSavePositionLimitCm)
                return RecordFileError(FText::FromString(TEXT("Your position could not be saved. The earlier save is unchanged.")));
            Placement.PositionCm[Axis] = FMath::RoundToInt64(Position[Axis]);
        }
        const FRotator Look = Player->GetControlRotation();
        if (!FMath::IsFinite(Look.Yaw) || !FMath::IsFinite(Look.Pitch))
            return RecordFileError(FText::FromString(TEXT("Your view could not be saved. The earlier save is unchanged.")));
        Placement.YawCdeg = FMath::RoundToInt(FRotator::ClampAxis(Look.Yaw) * 100.0) % 36000;
        Placement.PitchCdeg = FMath::RoundToInt(FRotator::NormalizeAxis(Look.Pitch) * 100.0);
        Placement.Camera = Pawn->IsFirstPerson() ? PerfectWorld::CameraMode::FirstPerson : PerfectWorld::CameraMode::ThirdPerson;
        Life->SettleForSave();
        const auto Capture = PWLaneLifePersistence::CaptureWorldSave(Creator.CaptureSave(), Life->GetWorldState(), Placement);
        if (!Capture.Succeeded()) return RecordFileError(FText::FromString(ToFString(Capture.Validation.Message)));
        Written = PWLaneLifePersistence::WriteWorldSlot(Pb2SlotPath(Path), Capture.Text);
    }
    if (!Written.Ok) return RecordFileError(FText::FromString(ToFString(Written.Message)));
    LastError = EPWCreationError::None;
    LastErrorMessage = FText::FromString(FString::Printf(TEXT("Saved to slot %d."), SlotIndex + 1));
    return true;
}

bool UPerfectWorldCharacterCreatorSubsystem::LoadFromSlot(int32 SlotIndex)
{
    if (!CanReplaceWorld()) return false;
    const FString Path = GetSaveSlotPath(SlotIndex);
    if (Path.IsEmpty()) return RecordFileError(FText::FromString(TEXT("That save slot does not exist.")));
    auto* Life = UPWLaneLifeSubsystem::Get(this);
    if (Life == nullptr) return RecordFileError(FText::FromString(TEXT("The world is not ready to load.")));
    std::string Message;
    if (!Life->LoadWorldSlot(Pb2SlotPath(Path), Creator, LoadedPlacement, Message))
        return RecordFileError(FText::FromString(ToFString(Message)));
    LastError = EPWCreationError::None;
    LastErrorMessage = FText::FromString(ToFString(Message));
    return true;
}

bool UPerfectWorldCharacterCreatorSubsystem::CanReplaceWorld()
{
    const FText Refusal = Pb2ReplacementRefusal(this);
    return Refusal.IsEmpty() || RecordFileError(Refusal);
}
