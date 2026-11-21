#include "UI/TitleR5Model.h"

#include "GameClock.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldLaneLifePersistence.h"
#include "SaveStore.h"
#include "SaveSlotSummary.h"

#define LOCTEXT_NAMESPACE "PerfectWorldTitleR5"

namespace
{
const TCHAR* const Tr5ShortMonths[12] = {TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"),
                                         TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")};
const TCHAR* const Tr5LongMonths[12] = {TEXT("January"), TEXT("February"), TEXT("March"),     TEXT("April"),
                                        TEXT("May"),     TEXT("June"),     TEXT("July"),      TEXT("August"),
                                        TEXT("September"), TEXT("October"), TEXT("November"), TEXT("December")};

FString Tr5FromUtf8(const std::string& Text)
{
    return FString(UTF8_TO_TCHAR(Text.c_str()));
}

FText Tr5SlotLabel(const int32 Slot)
{
    return FText::Format(LOCTEXT("SlotLabel", "Slot {0}"), FText::AsNumber(Slot + 1));
}

FText R62PlaceText(const PerfectWorld::WorldSlotSummary& Facts)
{
    const FString Zone = Tr5FromUtf8(Facts.ZoneDisplayName);
    const FString City = Tr5FromUtf8(Facts.HomeCity);
    if (Zone.IsEmpty())
    {
        return FText::FromString(City);
    }
    return FText::FromString(City.IsEmpty() ? Zone : Zone + TEXT(", ") + City);
}

FText R62CalendarText(const PerfectWorld::CalendarTime& Calendar)
{
    const int32 Month = FMath::Clamp(Calendar.Month, 1, 12) - 1;
    const std::string_view Weekday = PerfectWorld::WeekdayName(Calendar.DayOfWeek);
    return FText::FromString(FString::Printf(TEXT("%s %d %s %d, %02d:%02d"), *Tr5FromUtf8(std::string(Weekday)),
                                             Calendar.Day, Tr5LongMonths[Month], Calendar.Year, Calendar.Hour,
                                             Calendar.Minute));
}

FDateTime Tr5ToLocal(const FDateTime& Utc)
{
    return Utc + (FDateTime::Now() - FDateTime::UtcNow());
}
} // namespace

namespace PWTitleR5
{
FText FSlotSummary::Compose() const
{
    TArray<FString> Parts;
    for (const FText* Part : {&SlotLabel, &Name, &Age, &GameTime, &Place, &SavedAt, &Note})
    {
        if (!Part->IsEmpty())
        {
            Parts.Add(Part->ToString());
        }
    }
    return FText::FromString(FString::Join(Parts, TEXT(" | ")));
}

const FSlotSummary* FTitleSaves::Find(const int32 Slot) const
{
    return Slots.FindByPredicate([Slot](const FSlotSummary& Summary) { return Summary.Slot == Slot; });
}

EContinueState FTitleSaves::GetContinueState() const
{
    const FSlotSummary* Target = ContinueTarget();
    if (Target == nullptr)
    {
        return EContinueState::None;
    }
    return Target->CanLoad() ? EContinueState::Ready : EContinueState::Blocked;
}

int32 FTitleSaves::LoadableCount() const
{
    int32 Count = 0;
    for (const FSlotSummary& Summary : Slots)
    {
        Count += Summary.CanLoad() ? 1 : 0;
    }
    return Count;
}

FText FormatSavedAt(const FDateTime& Local)
{
    const int32 Month = FMath::Clamp(Local.GetMonth(), 1, 12) - 1;
    return FText::FromString(FString::Printf(TEXT("Saved %d %s %d, %02d:%02d"), Local.GetDay(), Tr5ShortMonths[Month],
                                             Local.GetYear(), Local.GetHour(), Local.GetMinute()));
}

FText FormatGameTime(const int64 GameInstantMinutes)
{
    return R62CalendarText(PerfectWorld::GameClock::ToCalendar(GameInstantMinutes));
}

FSlotSummary SummariseWorldSave(const int32 Slot, const PerfectWorld::WorldSaveData& Data, const FDateTime& WrittenUtc,
                                const bool bRecoveredFromBackup, const FString& Notice)
{
    const PerfectWorld::WorldSlotSummary Facts = PerfectWorld::MakeWorldSlotSummary(Data);
    FSlotSummary Summary;
    Summary.Slot = Slot;
    Summary.SlotLabel = Tr5SlotLabel(Slot);
    Summary.WrittenUtc = WrittenUtc;
    Summary.Name = Facts.Name.empty() ? LOCTEXT("Unnamed", "Unnamed") : FText::FromString(Tr5FromUtf8(Facts.Name));
    Summary.Age = FText::Format(LOCTEXT("R62CurrentAge", "Age {0}"), FText::AsNumber(Facts.CurrentAge));
    Summary.SavedAt = FormatSavedAt(Tr5ToLocal(WrittenUtc));
    if (Facts.Draft)
    {
        // A draft has no world to describe: its clock and zone are the fresh-world defaults, so they are not shown.
        Summary.State = ESlotState::Draft;
        Summary.Note = LOCTEXT("DraftNote", "Character creation was not finished. Loading picks up where you stopped.");
        return Summary;
    }
    Summary.State = bRecoveredFromBackup ? ESlotState::Recovered : ESlotState::Ready;
    Summary.GameTime = R62CalendarText(Facts.Calendar);
    Summary.Place = R62PlaceText(Facts);
    if (bRecoveredFromBackup)
    {
        Summary.Note = Notice.IsEmpty() ? LOCTEXT("RecoveredNote", "Recovered from the backup copy of this save.")
                                        : FText::FromString(Notice);
    }
    return Summary;
}

FSlotSummary SummariseUnreadable(const int32 Slot, const FDateTime& WrittenUtc, const FString& Notice)
{
    FSlotSummary Summary;
    Summary.Slot = Slot;
    Summary.State = ESlotState::Unreadable;
    Summary.SlotLabel = Tr5SlotLabel(Slot);
    Summary.WrittenUtc = WrittenUtc;
    Summary.SavedAt = FormatSavedAt(Tr5ToLocal(WrittenUtc));
    Summary.Note = Notice.IsEmpty()
        ? LOCTEXT("UnreadableNote", "This save cannot be read. The file has been left as it is.")
        : FText::Format(LOCTEXT("UnreadableWhy", "This save cannot be read. {0}"), FText::FromString(Notice));
    return Summary;
}

void SortNewestFirst(TArray<FSlotSummary>& Slots, const int32 PinnedSlot)
{
    Slots.StableSort([PinnedSlot](const FSlotSummary& A, const FSlotSummary& B)
    {
        const bool bPinA = A.Slot == PinnedSlot;
        const bool bPinB = B.Slot == PinnedSlot;
        if (bPinA != bPinB)
        {
            return bPinA;
        }
        if (A.WrittenUtc != B.WrittenUtc)
        {
            return A.WrittenUtc > B.WrittenUtc;
        }
        return A.Slot < B.Slot;
    });
}

FSlotSummary ReadSlot(const int32 Slot)
{
    if (!UPerfectWorldCharacterCreatorSubsystem::IsSaveSlotOccupied(Slot))
    {
        return FSlotSummary();
    }
    // The same timestamp rule as FindMostRecentSaveSlot: a surviving backup counts as the slot's time.
    const FString Path = FPaths::ConvertRelativePathToFull(UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot));
    const FDateTime Written = FMath::Max(IFileManager::Get().GetTimeStamp(*Path),
                                         IFileManager::Get().GetTimeStamp(*(Path + TEXT(".bak"))));
    const PWLaneLifePersistence::ReadResult Read = PWLaneLifePersistence::ReadWorldSlot(std::string(TCHAR_TO_UTF8(*Path)));
    if (Read.Status == PerfectWorld::SaveReadStatus::Missing)
    {
        return FSlotSummary();
    }
    if (!Read.Succeeded())
    {
        const std::string& Why = Read.Message.empty() ? Read.Validation.Message : Read.Message;
        return SummariseUnreadable(Slot, Written, Tr5FromUtf8(Why));
    }
    return SummariseWorldSave(Slot, *Read.Data, Written, Read.Status == PerfectWorld::SaveReadStatus::RecoveredFromBackup,
                              Tr5FromUtf8(Read.Message));
}

FTitleSaves ReadTitleSaves()
{
    FTitleSaves Saves;
    Saves.ContinueSlot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    for (int32 Slot = 0; Slot < UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotCount(); ++Slot)
    {
        FSlotSummary Summary = ReadSlot(Slot);
        if (Summary.State != ESlotState::Empty)
        {
            Saves.Slots.Add(MoveTemp(Summary));
        }
    }
    SortNewestFirst(Saves.Slots, Saves.ContinueSlot);
    return Saves;
}
} // namespace PWTitleR5

#undef LOCTEXT_NAMESPACE
