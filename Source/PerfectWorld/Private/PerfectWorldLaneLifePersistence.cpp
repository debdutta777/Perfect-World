#include "PerfectWorldLaneLifePersistence.h"

#include <filesystem>
#include <system_error>
#include <utility>

namespace PWLaneLifePersistence
{
CaptureResult CaptureWorldSave(const PerfectWorld::CharacterCreationSave& Creation,
    const PerfectWorld::WorldState& World, const PerfectWorld::PlayerPlacement& Player)
{
    CaptureResult Result;
    if (!PerfectWorld::IsWorldQuiescent(World))
    {
        Result.Validation.Error = PerfectWorld::WorldSaveError::InvalidWorld;
        Result.Validation.Message = "The world is still processing events. Please try saving again in a moment.";
        return Result;
    }
    const PerfectWorld::WorldSaveData Snapshot{Creation, World, Player};
    std::string Text = PerfectWorld::SerializeWorldSave(Snapshot);
    if (Text.empty())
    {
        Result.Validation.Error = PerfectWorld::WorldSaveError::InvalidWorld;
        Result.Validation.Message = "The world could not be saved because its doors or conversations are inconsistent. The earlier save is unchanged.";
        return Result;
    }

    // The writer refuses invalid lane authority; the reader also validates creation,
    // placement, clock/day consistency and the remaining world authority.
    PerfectWorld::WorldSaveData Candidate;
    Result.Validation = PerfectWorld::DeserializeWorldSave(Text, Candidate);
    if (!Result.Validation.Succeeded())
    {
        return Result;
    }
    Result.Data = std::move(Candidate);
    Result.Text = std::move(Text);
    return Result;
}

PerfectWorld::SaveOperationResult WriteWorldSlot(const std::string& AbsolutePath, const std::string_view Text)
{
    if (Text.empty())
    {
        return {false, "There is no valid world snapshot to save. The earlier save is unchanged."};
    }
    PerfectWorld::WorldSaveData Candidate;
    const auto Parsed = PerfectWorld::DeserializeWorldSave(Text, Candidate);
    if (!Parsed.Succeeded())
    {
        return {false, Parsed.Message};
    }
    if (Parsed.SourceVersion != PerfectWorld::WorldSaveSchemaVersion || Parsed.Migrated)
    {
        return {false, "Only a current world snapshot can be written. The earlier save is unchanged."};
    }

    const auto Validator = PerfectWorld::WorldSaveValidator();
    const auto Previous = PerfectWorld::ReadSaveFile(AbsolutePath, Validator);
    if (Previous.Status == PerfectWorld::SaveReadStatus::RecoveredFromBackup)
    {
        // Do not let the store rotate a corrupt primary over the only usable .bak.
        // If interruption occurs after this removal, that backup still loads.
        const std::filesystem::path Primary(std::u8string(AbsolutePath.begin(), AbsolutePath.end()));
        std::error_code Error;
        std::filesystem::remove(Primary, Error);
        if (Error)
        {
            return {false, "The damaged latest save could not be set aside. The earlier save is kept."};
        }
    }
    return PerfectWorld::WriteSaveFile(AbsolutePath, Text, Validator);
}

ReadResult ReadWorldSlot(const std::string& AbsolutePath)
{
    ReadResult Result;
    const auto Read = PerfectWorld::ReadSaveFile(AbsolutePath, PerfectWorld::WorldSaveValidator());
    Result.Status = Read.Status;
    Result.Message = Read.Message;
    if (!Read.Succeeded())
    {
        Result.Validation.Error = PerfectWorld::WorldSaveError::Malformed;
        Result.Validation.Message = Read.Message;
        return Result;
    }

    PerfectWorld::WorldSaveData Candidate;
    Result.Validation = PerfectWorld::DeserializeWorldSave(Read.Text, Candidate);
    if (!Result.Validation.Succeeded())
    {
        Result.Message = Result.Validation.Message;
        return Result;
    }
    Result.Data = std::move(Candidate);
    return Result;
}
} // namespace PWLaneLifePersistence
