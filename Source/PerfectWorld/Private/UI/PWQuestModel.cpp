#include "UI/PWQuestText.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldLaneLifeSubsystem.h"

#include <string_view>

namespace
{
FString QwTrackedId;
uint32 QwTrackedRevision = 0;
TFunction<void()> QwToggleHandler;
bool bQwOpenOnNextPause = false;

/** The school times and calendar, read once per content database and reused (the quest window and the HUD line both ask often). */
struct FQwSchoolCache
{
    const PerfectWorld::ContentDb* Source = nullptr;
    PerfectWorld::BellScheduleSet Bells;
    PerfectWorld::SchoolCalendarSet Calendars;
};
FQwSchoolCache QwSchoolCache;

/**
 * The bell schedule and calendar the school obligation reads. All three Class XI streams share their bell times, so the first schedule
 * stands for them. Without content the projection falls back to the Monday to Friday rule and the shipped bell numbers.
 */
PerfectWorld::QuestLogContext QwContextFor(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    const UGameInstance* Instance = World != nullptr ? World->GetGameInstance() : nullptr;
    const UPWContentSubsystem* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    const PerfectWorld::ContentDb* Db = Content != nullptr ? Content->GetDb() : nullptr;
    PerfectWorld::QuestLogContext Context;
    if (Db == nullptr)
    {
        return Context;
    }
    if (QwSchoolCache.Source != Db)
    {
        QwSchoolCache = FQwSchoolCache();
        QwSchoolCache.Source = Db;
        const PerfectWorld::BellScheduleResult Bells = PerfectWorld::LoadBellSchedules(*Db);
        if (Bells.Ok()) QwSchoolCache.Bells = Bells.Set;
        const PerfectWorld::SchoolCalendarResult Calendars = PerfectWorld::LoadSchoolCalendars(*Db);
        if (Calendars.Ok()) QwSchoolCache.Calendars = Calendars.Set;
    }
    if (!QwSchoolCache.Bells.Schedules.empty())
    {
        Context.School = &QwSchoolCache.Bells.Schedules.front();
        Context.Calendar = QwSchoolCache.Calendars.Find(Context.School->InstitutionId, Context.School->StageId);
    }
    return Context;
}

PerfectWorld::QuestLogView QwReadLog(const UPWLaneLifeSubsystem& Life, const UObject* WorldContext)
{
    return PerfectWorld::MakeQuestLog(Life.GetWorldState(), Life.GetNow(), QwContextFor(WorldContext));
}

void QwMark(PerfectWorld::QuestLogView& Log, const FString& TrackedId)
{
    const FTCHARToUTF8 Utf8(*TrackedId);
    PerfectWorld::QuestLogTrack(Log, std::string_view(Utf8.Get(), static_cast<size_t>(Utf8.Length())));
}

void QwFillSection(FPWQuestSectionModel& Section, const std::vector<PerfectWorld::QuestItem>& Items)
{
    for (const PerfectWorld::QuestItem& Item : Items)
    {
        Section.Rows.Add(PWQuest::MakeRow(Item));
    }
}
} // namespace

int32 FPWQuestModel::RowCount() const
{
    int32 Count = 0;
    for (const FPWQuestSectionModel& Section : Sections) Count += Section.Rows.Num();
    return Count;
}

const FPWQuestRow* FPWQuestModel::FindRow(const FString& Id) const
{
    for (const FPWQuestSectionModel& Section : Sections)
    {
        for (const FPWQuestRow& Row : Section.Rows)
        {
            if (Row.Id == Id) return &Row;
        }
    }
    return nullptr;
}

FPWQuestRow* FPWQuestModel::FindRow(const FString& Id)
{
    return const_cast<FPWQuestRow*>(static_cast<const FPWQuestModel*>(this)->FindRow(Id));
}

const FPWQuestRow* FPWQuestModel::FirstRow() const
{
    for (const FPWQuestSectionModel& Section : Sections)
    {
        if (!Section.Rows.IsEmpty()) return &Section.Rows[0];
    }
    return nullptr;
}

namespace PWQuest
{
FPWQuestModel BuildModel(const PerfectWorld::QuestLogView& Log, const FText& Clock, const FString& TrackedId)
{
    PerfectWorld::QuestLogView Marked = Log;
    QwMark(Marked, TrackedId);
    FPWQuestModel Model;
    Model.bAvailable = true;
    Model.Clock = Clock;
    const std::pair<PerfectWorld::QuestSection, const std::vector<PerfectWorld::QuestItem>*> Parts[] = {
        {PerfectWorld::QuestSection::Today, &Marked.Today},
        {PerfectWorld::QuestSection::Promises, &Marked.Promises},
        {PerfectWorld::QuestSection::Discoveries, &Marked.Discoveries}};
    for (const auto& Part : Parts)
    {
        FPWQuestSectionModel& Section = Model.Sections.AddDefaulted_GetRef();
        Section.Section = Part.first;
        Section.Heading = SectionHeading(Part.first);
        Section.EmptyNote = SectionEmptyNote(Part.first);
        QwFillSection(Section, *Part.second);
    }
    return Model;
}

FPWQuestTrackedLine BuildTrackedLine(const PerfectWorld::QuestLogView& Log, const FString& TrackedId)
{
    FPWQuestTrackedLine Line;
    if (TrackedId.IsEmpty())
    {
        return Line;
    }
    const FTCHARToUTF8 Utf8(*TrackedId);
    const PerfectWorld::QuestItem* Item = PerfectWorld::QuestLogFind(Log, std::string_view(Utf8.Get(), static_cast<size_t>(Utf8.Length())));
    // A finished or vanished objective leaves the HUD; the id is kept, so a promise that is made again is picked up again.
    if (Item == nullptr || Item->Status == PerfectWorld::QuestStatus::Done)
    {
        return Line;
    }
    Line.bVisible = true;
    Line.Id = TrackedId;
    Line.Title = TitleText(*Item);
    Line.When = ShortWhenText(*Item);
    Line.Status = Item->Status;
    return Line;
}

FPWQuestModel ReadModel(const UObject* WorldContext)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(WorldContext);
    if (Life == nullptr)
    {
        return FPWQuestModel();
    }
    return BuildModel(QwReadLog(*Life, WorldContext), Life->GetClockText(), GetTrackedId());
}

FPWQuestTrackedLine ReadTrackedLine(const UObject* WorldContext)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(WorldContext);
    if (Life == nullptr || QwTrackedId.IsEmpty())
    {
        return FPWQuestTrackedLine();
    }
    return BuildTrackedLine(QwReadLog(*Life, WorldContext), QwTrackedId);
}

FString GetTrackedId()
{
    return QwTrackedId;
}

void SetTrackedId(const FString& Id)
{
    if (QwTrackedId != Id)
    {
        QwTrackedId = Id;
        ++QwTrackedRevision;
    }
}

bool ToggleTracked(const FString& Id)
{
    const bool bWasTracked = !Id.IsEmpty() && QwTrackedId == Id;
    SetTrackedId(bWasTracked ? FString() : Id);
    return !QwTrackedId.IsEmpty();
}

uint32 TrackedRevision()
{
    return QwTrackedRevision;
}

void SetToggleHandler(TFunction<void()> Handler)
{
    QwToggleHandler = MoveTemp(Handler);
}

bool RequestToggle()
{
    if (!QwToggleHandler)
    {
        return false;
    }
    QwToggleHandler();
    return true;
}

void SetOpenOnNextPause(const bool bOpen)
{
    bQwOpenOnNextPause = bOpen;
}

bool ConsumeOpenOnNextPause()
{
    const bool bOpen = bQwOpenOnNextPause;
    bQwOpenOnNextPause = false;
    return bOpen;
}

bool Compact(const float Width, const float Height)
{
    return Height < 760.0f || Width < 760.0f;
}

bool TwoPane(const float Width, const float)
{
    return Width >= 700.0f;
}

float PanelWidth(const float Width, const float)
{
    return FMath::Clamp(Width - 32.0f, 280.0f, 1100.0f);
}

float PanelMaxHeight(const float, const float Height)
{
    return FMath::Max(120.0f, Height - 32.0f);
}

float ListWidth(const float Width, const float Height)
{
    return FMath::Clamp(PanelWidth(Width, Height) * 0.36f, 220.0f, 380.0f);
}
} // namespace PWQuest
