#include "UI/PWQuestText.h"

#include "Conversation.h"
#include "GameClock.h"
#include "PerfectWorldLaneLifeSubsystem.h"

#define LOCTEXT_NAMESPACE "PerfectWorldQuests"

namespace
{
/** Keys are exact; FString's == ignores case. */
bool QwIs(const FString& Value, const TCHAR* Key)
{
    return Value.Equals(Key, ESearchCase::CaseSensitive);
}

FString QwKeyOf(const PerfectWorld::QuestItem& Item)
{
    return PWLaneLife::ToFString(Item.TextKey);
}

constexpr const TCHAR* QwAgreementPrefix = TEXT("journal.agreement.");
constexpr const TCHAR* QwPlanPrefix = TEXT("quest.today.plan.");
constexpr const TCHAR* QwDiscoverPrefix = TEXT("quest.discover.");

bool QwIsAgreement(const FString& Key)
{
    return Key.StartsWith(QwAgreementPrefix, ESearchCase::CaseSensitive) && Key.Len() > FCString::Strlen(QwAgreementPrefix);
}

bool QwIsDiscovery(const FString& Key)
{
    return Key.StartsWith(QwDiscoverPrefix, ESearchCase::CaseSensitive);
}

FText QwActivityName(const std::string& ActivityId, const FText& Fallback)
{
    const FString Id = PWLaneLife::ToFString(ActivityId);
    if (QwIs(Id, TEXT("activity.attend_class"))) return LOCTEXT("ActAttendClass", "Attend class");
    if (QwIs(Id, TEXT("activity.eat_meal"))) return LOCTEXT("ActEatMeal", "Eat a meal");
    if (QwIs(Id, TEXT("activity.exercise_run"))) return LOCTEXT("ActRun", "Go for a run");
    if (QwIs(Id, TEXT("activity.household_chores"))) return LOCTEXT("ActChores", "Household chores");
    if (QwIs(Id, TEXT("activity.rest"))) return LOCTEXT("ActRest", "Rest");
    if (QwIs(Id, TEXT("activity.sleep_night"))) return LOCTEXT("ActSleep", "Sleep");
    if (QwIs(Id, TEXT("activity.study_at_home"))) return LOCTEXT("ActStudy", "Study at home");
    return Fallback;
}

FText QwPlaceName(const std::string& PlaceKey)
{
    const FString Key = PWLaneLife::ToFString(PlaceKey);
    if (Key.IsEmpty()) return LOCTEXT("PlaceNone", "Wherever you are");
    if (QwIs(Key, TEXT("place.school"))) return LOCTEXT("PlaceSchool", "Nabapara Vidyalaya");
    if (QwIs(Key, TEXT("place.home"))) return LOCTEXT("PlaceHome", "Home");
    if (QwIs(Key, TEXT("place.care_home"))) return LOCTEXT("PlaceCareHome", "The children's home");
    if (QwIs(Key, TEXT("place.classroom"))) return LOCTEXT("PlaceClassroom", "The classroom");
    if (Key.StartsWith(TEXT("place."), ESearchCase::CaseSensitive)) return LOCTEXT("PlaceHere", "Where you are");

    FText Name;
    if (QwIs(Key, TEXT("location.nabapara.caretaker_flat"))) Name = LOCTEXT("LocCaretaker", "The caretaker's flat");
    else if (QwIs(Key, TEXT("location.nabapara.tall_flats_unit_a"))) Name = LOCTEXT("LocFlatsA", "Tall flats, unit A");
    else if (QwIs(Key, TEXT("location.nabapara.tall_flats_unit_b"))) Name = LOCTEXT("LocFlatsB", "Tall flats, unit B");
    else if (QwIs(Key, TEXT("location.nabapara.tea_row_house"))) Name = LOCTEXT("LocRowHouse", "The tea row house");
    else if (QwIs(Key, TEXT("location.nabapara.old_house"))) Name = LOCTEXT("LocOldHouse", "The old house");
    else if (QwIs(Key, TEXT("location.nabapara.tall_flats_upper_a"))) Name = LOCTEXT("LocUpperA", "Tall flats, upper flat A");
    else if (QwIs(Key, TEXT("location.nabapara.repair_workshop"))) Name = LOCTEXT("LocWorkshop", "The repair workshop");
    else if (QwIs(Key, TEXT("location.nabapara.hardware_store"))) Name = LOCTEXT("LocHardware", "The hardware store");
    else if (QwIs(Key, TEXT("location.nabapara.stationery"))) Name = LOCTEXT("LocStationery", "The stationery shop");
    else return LOCTEXT("PlaceLane", "Somewhere on the lane");
    return FText::Format(LOCTEXT("PlaceOnLaneFmt", "{0}, Nabapara Lane"), Name);
}

/** The agreement wording, one sentence each; the person is shown separately, so no name appears here. */
FText QwAgreementWhat(const FString& Key)
{
    const FString Suffix = Key.Mid(FCString::Strlen(QwAgreementPrefix));
    if (QwIs(Suffix, TEXT("evening_rota_agreed"))) return LOCTEXT("AgrRotaAgreed", "Share the evening jobs at home. The details are not settled yet.");
    if (QwIs(Suffix, TEXT("evening_rota_dishes_weekdays"))) return LOCTEXT("AgrRotaDishes", "You take the dishes on weekday evenings.");
    if (QwIs(Suffix, TEXT("evening_rota_alternate_days"))) return LOCTEXT("AgrRotaAlternate", "You and your parent alternate days for the evening jobs.");
    if (QwIs(Suffix, TEXT("evening_rota_sweep_and_cook"))) return LOCTEXT("AgrRotaSweepCook", "You sweep and your parent cooks.");
    if (QwIs(Suffix, TEXT("care_jobs_breakfast_weekdays"))) return LOCTEXT("AgrCareBreakfast", "You set out breakfast on weekdays.");
    if (QwIs(Suffix, TEXT("care_jobs_turns"))) return LOCTEXT("AgrCareTurns", "You take turns at the shared jobs with the others, day by day.");
    if (QwIs(Suffix, TEXT("care_jobs_study_room"))) return LOCTEXT("AgrCareStudyRoom", "You tidy the study room after homework hour.");
    if (QwIs(Suffix, TEXT("nirmal_bose.book_evening_agreed"))) return LOCTEXT("AgrNirmalBook", "A book evening, as you agreed.");
    if (QwIs(Suffix, TEXT("rupa_ghosh.visit_promised"))) return LOCTEXT("AgrRupaVisit", "A visit you promised.");
    if (QwIs(Suffix, TEXT("sahana_mitra.sewing_evening_agreed"))) return LOCTEXT("AgrSahanaSewing", "A sewing evening, as you agreed.");
    if (QwIs(Suffix, TEXT("subrata_dey.step_promised"))) return LOCTEXT("AgrSubrataStep", "Help with the step, as you promised.");
    if (QwIs(Suffix, TEXT("debashis_pal.bring_promised"))) return LOCTEXT("AgrDebashisBring", "Bring something in for him to look at.");
    if (QwIs(Suffix, TEXT("mahua_saha.sort_promised"))) return LOCTEXT("AgrMahuaSort", "Help sort loose screws one quiet afternoon.");
    if (QwIs(Suffix, TEXT("prabir_chatterjee.lane_stories_agreed"))) return LOCTEXT("AgrPrabirStories", "An hour of lane stories, as you agreed.");
    if (QwIs(Suffix, TEXT("ishita_banerjee.quiet_evening_agreed"))) return LOCTEXT("AgrIshitaEvening", "A quiet evening, as you agreed.");
    if (QwIs(Suffix, TEXT("madhabi_das.ward_records_agreed"))) return LOCTEXT("AgrMadhabiRecords", "An evening of old ward records, as you agreed.");
    if (QwIs(Suffix, TEXT("arup_basu.parcel_round_agreed"))) return LOCTEXT("AgrArupRound", "An evening hearing about the parcel round, as you agreed.");
    return LOCTEXT("AgrFallback", "An arrangement you made.");
}

struct FQwEntry
{
    FText Title;
    FText What;
};

/** Titles and descriptions of the Today and Discoveries keys. False for a key that is none of them. */
bool QwFindEntry(const FString& Key, FQwEntry& Out)
{
    if (QwIs(Key, TEXT("quest.today.school_bell")))
    {
        Out = {LOCTEXT("SchoolTitle", "Be at school by the first bell"),
               LOCTEXT("SchoolWhat", "School starts with assembly at the first bell. Be there by then to be on time; a little later and you are marked late.")};
    }
    else if (QwIs(Key, TEXT("quest.today.evening_jobs")))
        Out = {LOCTEXT("EveningTitle", "Evening jobs"), LOCTEXT("EveningWhat", "The evening jobs you agreed to share at home. The details are not settled yet.")};
    else if (QwIs(Key, TEXT("quest.today.evening_dishes")))
        Out = {LOCTEXT("DishesTitle", "Do the dishes"), LOCTEXT("DishesWhat", "You take the dishes on weekday evenings, as you agreed.")};
    else if (QwIs(Key, TEXT("quest.today.evening_alternate")))
        Out = {LOCTEXT("AlternateTitle", "Evening jobs, turn by turn"), LOCTEXT("AlternateWhat", "You and your parent alternate days for the evening jobs. Check whose turn it is.")};
    else if (QwIs(Key, TEXT("quest.today.evening_sweep")))
        Out = {LOCTEXT("SweepTitle", "Sweep up for the evening"), LOCTEXT("SweepWhat", "You sweep and your parent cooks, as you agreed.")};
    else if (QwIs(Key, TEXT("quest.today.care_breakfast")))
        Out = {LOCTEXT("BreakfastTitle", "Set out breakfast"), LOCTEXT("BreakfastWhat", "You set out breakfast for the others on weekdays, as you agreed.")};
    else if (QwIs(Key, TEXT("quest.today.care_turns")))
        Out = {LOCTEXT("TurnsTitle", "Take your turn at the shared jobs"), LOCTEXT("TurnsWhat", "You take turns at the shared jobs with the others, day by day.")};
    else if (QwIs(Key, TEXT("quest.today.care_study_room")))
        Out = {LOCTEXT("StudyRoomTitle", "Tidy the study room"), LOCTEXT("StudyRoomWhat", "You tidy the study room after homework hour on weekdays.")};
    else if (QwIs(Key, TEXT("quest.discover.caretaker_flat")))
        Out = {LOCTEXT("DiscCaretakerTitle", "The caretaker's flat"), LOCTEXT("DiscCaretakerWhat", "A home on the lane that receives visitors at set hours. You have not spoken to anyone there yet.")};
    else if (QwIs(Key, TEXT("quest.discover.tall_flats_unit_a")))
        Out = {LOCTEXT("DiscFlatsATitle", "A flat in the tall flats (unit A)"), LOCTEXT("DiscFlatsAWhat", "A home that receives visitors at set hours. You have not spoken to anyone there yet.")};
    else if (QwIs(Key, TEXT("quest.discover.tall_flats_unit_b")))
        Out = {LOCTEXT("DiscFlatsBTitle", "A flat in the tall flats (unit B)"), LOCTEXT("DiscFlatsBWhat", "A home that receives visitors at set hours. You have not spoken to anyone there yet.")};
    else if (QwIs(Key, TEXT("quest.discover.tea_row_house")))
        Out = {LOCTEXT("DiscRowTitle", "The tea row house"), LOCTEXT("DiscRowWhat", "A home that receives visitors at set hours. You have not spoken to anyone there yet.")};
    else if (QwIs(Key, TEXT("quest.discover.old_house")))
        Out = {LOCTEXT("DiscOldTitle", "The old house"), LOCTEXT("DiscOldWhat", "A home that receives visitors at set hours. You have not spoken to anyone there yet.")};
    else if (QwIs(Key, TEXT("quest.discover.tall_flats_upper_a")))
        Out = {LOCTEXT("DiscUpperTitle", "The upper flat in the tall flats"), LOCTEXT("DiscUpperWhat", "A home that receives visitors at set hours. You have not spoken to anyone there yet.")};
    else if (QwIs(Key, TEXT("quest.discover.repair_workshop")))
        Out = {LOCTEXT("DiscWorkshopTitle", "The repair workshop"), LOCTEXT("DiscWorkshopWhat", "A workshop on the lane with set opening hours. You have not spoken to anyone there yet.")};
    else if (QwIs(Key, TEXT("quest.discover.hardware_store")))
        Out = {LOCTEXT("DiscHardwareTitle", "The hardware store"), LOCTEXT("DiscHardwareWhat", "A shop on the lane with set opening hours. You have not spoken to anyone there yet.")};
    else if (QwIs(Key, TEXT("quest.discover.stationery")))
        Out = {LOCTEXT("DiscStationeryTitle", "The stationery shop"), LOCTEXT("DiscStationeryWhat", "A shop on the lane with set opening hours. You have not spoken to anyone there yet.")};
    else
        return false;
    return true;
}

FText QwPromiseWho(const PerfectWorld::QuestItem& Item)
{
    if (!Item.WhoLabel.empty())
    {
        return Item.PersonId == PerfectWorld::HouseholdAdultId ? LOCTEXT("WhoYourParentLower", "your parent") : PWLaneLife::ToText(Item.WhoLabel);
    }
    return LOCTEXT("WhoSomeoneLower", "someone you know");
}
} // namespace

namespace PWQuest
{
FText SectionHeading(const PerfectWorld::QuestSection Section)
{
    switch (Section)
    {
    case PerfectWorld::QuestSection::Today: return LOCTEXT("HeadingToday", "Today");
    case PerfectWorld::QuestSection::Promises: return LOCTEXT("HeadingPromises", "Promises");
    case PerfectWorld::QuestSection::Discoveries: return LOCTEXT("HeadingDiscoveries", "Discoveries");
    }
    return LOCTEXT("HeadingOther", "Other");
}

FText SectionEmptyNote(const PerfectWorld::QuestSection Section)
{
    switch (Section)
    {
    case PerfectWorld::QuestSection::Today: return LOCTEXT("EmptyToday", "Nothing is asked of you today.");
    case PerfectWorld::QuestSection::Promises: return LOCTEXT("EmptyPromises", "No promises yet. Arrangements you agree to will be kept here.");
    case PerfectWorld::QuestSection::Discoveries: return LOCTEXT("EmptyDiscoveries", "Nothing new to find for now.");
    }
    return FText::GetEmpty();
}

FText TitleText(const PerfectWorld::QuestItem& Item)
{
    const FString Key = QwKeyOf(Item);
    FQwEntry Entry;
    if (QwFindEntry(Key, Entry)) return Entry.Title;
    if (QwIs(Key, TEXT("quest.today.commitment")))
    {
        return FText::Format(LOCTEXT("CommitmentTitleFmt", "In progress: {0}"),
                             QwActivityName(Item.ActivityId, LOCTEXT("CommitmentFallback", "Something you decided to do")));
    }
    if (Key.StartsWith(QwPlanPrefix, ESearchCase::CaseSensitive))
    {
        return QwActivityName(Item.ActivityId, LOCTEXT("PlanFallback", "Something you planned"));
    }
    if (QwIsAgreement(Key)) return FText::Format(LOCTEXT("PromiseTitleFmt", "With {0}"), QwPromiseWho(Item));
    return LOCTEXT("TitleFallback", "Something to do");
}

FText WhatText(const PerfectWorld::QuestItem& Item)
{
    const FString Key = QwKeyOf(Item);
    FQwEntry Entry;
    if (QwFindEntry(Key, Entry)) return Entry.What;
    if (QwIs(Key, TEXT("quest.today.commitment"))) return LOCTEXT("CommitmentWhat", "You decided to do this and have not finished it yet.");
    if (Key.StartsWith(QwPlanPrefix, ESearchCase::CaseSensitive)) return LOCTEXT("PlanWhat", "Part of what you planned for today. A plan is an intention, not a duty; you can do something else.");
    if (QwIsAgreement(Key)) return QwAgreementWhat(Key);
    return LOCTEXT("WhatFallback", "Something you have taken on.");
}

FText WhoText(const PerfectWorld::QuestItem& Item)
{
    if (!Item.PersonId.empty())
    {
        return Item.WhoLabel.empty() ? LOCTEXT("WhoSomeone", "Someone you know") : PWLaneLife::ToText(Item.WhoLabel);
    }
    if (QwIsDiscovery(QwKeyOf(Item))) return LOCTEXT("WhoUnmet", "Someone you have not met yet");
    return LOCTEXT("WhoNobody", "No one in particular");
}

FText WhereText(const PerfectWorld::QuestItem& Item)
{
    return QwPlaceName(Item.PlaceKey);
}

FText TimeText(const int32 MinuteOfDay)
{
    const int32 Minute = FMath::Clamp(MinuteOfDay, 0, 24 * 60 - 1);
    return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minute / 60, Minute % 60));
}

FText WindowsText(const std::vector<PerfectWorld::QuestWindow>& Windows)
{
    TArray<FText> Parts;
    for (const PerfectWorld::QuestWindow& Window : Windows)
    {
        Parts.Add(FText::Format(LOCTEXT("WindowFmt", "{0} to {1}"), TimeText(Window.OpenMinute), TimeText(Window.CloseMinute)));
    }
    return FText::Join(LOCTEXT("WindowJoin", ", "), Parts);
}

namespace
{
int32 QwMinuteOf(const PerfectWorld::GameInstant Instant)
{
    const PerfectWorld::CalendarTime Calendar = PerfectWorld::GameClock::ToCalendar(Instant);
    return Calendar.Hour * 60 + Calendar.Minute;
}
} // namespace

FText WhenText(const PerfectWorld::QuestItem& Item)
{
    const FString Key = QwKeyOf(Item);
    TArray<FText> Parts;
    if (Item.DueAt >= 0)
    {
        const FText Time = TimeText(QwMinuteOf(Item.DueAt));
        if (QwIs(Key, TEXT("quest.today.school_bell"))) Parts.Add(FText::Format(LOCTEXT("WhenSchoolFmt", "By {0}, the first bell."), Time));
        else if (Key.StartsWith(TEXT("quest.today.evening"), ESearchCase::CaseSensitive)) Parts.Add(FText::Format(LOCTEXT("WhenEveningFmt", "From {0}, when evening begins."), Time));
        else if (Key.StartsWith(QwPlanPrefix, ESearchCase::CaseSensitive)) Parts.Add(FText::Format(LOCTEXT("WhenPlanFmt", "Planned for {0}."), Time));
        else Parts.Add(FText::Format(LOCTEXT("WhenAtFmt", "From {0}."), Time));
    }
    else if (QwIs(Key, TEXT("quest.today.commitment")))
    {
        Parts.Add(LOCTEXT("WhenNow", "Now."));
    }
    if (!Item.Windows.empty())
    {
        const FText Hours = WindowsText(Item.Windows);
        Parts.Add(Item.WindowKind == PerfectWorld::QuestWindowKind::Visiting ? FText::Format(LOCTEXT("WhenVisitingFmt", "Visiting hours: {0}."), Hours)
                                                                             : FText::Format(LOCTEXT("WhenOpenFmt", "Open {0}."), Hours));
    }
    if (Item.WeekdaysOnly && Key != TEXT("quest.today.school_bell")) Parts.Add(LOCTEXT("WhenWeekdays", "Weekdays only."));
    if (Parts.IsEmpty()) return LOCTEXT("WhenNone", "No set time.");
    return FText::Join(LOCTEXT("WhenJoin", " "), Parts);
}

FText ShortWhenText(const PerfectWorld::QuestItem& Item)
{
    const FString Key = QwKeyOf(Item);
    if (Item.DueAt >= 0)
    {
        const FText Time = TimeText(QwMinuteOf(Item.DueAt));
        return QwIs(Key, TEXT("quest.today.school_bell")) ? FText::Format(LOCTEXT("ShortBeforeFmt", "By {0}"), Time)
                                                           : FText::Format(LOCTEXT("ShortFromFmt", "From {0}"), Time);
    }
    if (!Item.Windows.empty())
    {
        const FText Hours = WindowsText(Item.Windows);
        return Item.WindowKind == PerfectWorld::QuestWindowKind::Visiting ? FText::Format(LOCTEXT("ShortVisitingFmt", "Visiting {0}"), Hours)
                                                                          : FText::Format(LOCTEXT("ShortOpenFmt", "Open {0}"), Hours);
    }
    return QwIs(Key, TEXT("quest.today.commitment")) ? LOCTEXT("ShortNow", "Now") : LOCTEXT("ShortToday", "Today");
}

FText StatusText(const PerfectWorld::QuestSection Section, const PerfectWorld::QuestStatus Status)
{
    using PerfectWorld::QuestStatus;
    switch (Status)
    {
    case QuestStatus::Done: return LOCTEXT("StatusDone", "Done");
    case QuestStatus::Missed: return LOCTEXT("StatusMissed", "Missed");
    case QuestStatus::Due:
        switch (Section)
        {
        case PerfectWorld::QuestSection::Today: return LOCTEXT("StatusDueToday", "Due now");
        case PerfectWorld::QuestSection::Promises: return LOCTEXT("StatusDuePromise", "Can be kept now");
        case PerfectWorld::QuestSection::Discoveries: return LOCTEXT("StatusDueDiscovery", "Open to visit now");
        }
        break;
    case QuestStatus::Open:
        switch (Section)
        {
        case PerfectWorld::QuestSection::Today: return LOCTEXT("StatusOpenToday", "Later today");
        case PerfectWorld::QuestSection::Promises: return LOCTEXT("StatusOpenPromise", "Open");
        case PerfectWorld::QuestSection::Discoveries: return LOCTEXT("StatusOpenDiscovery", "Not visited yet");
        }
        break;
    }
    return LOCTEXT("StatusOpenOther", "Open");
}

FPWQuestRow MakeRow(const PerfectWorld::QuestItem& Item)
{
    FPWQuestRow Row;
    Row.Id = PWLaneLife::ToFString(Item.Id);
    Row.Section = Item.Section;
    Row.Status = Item.Status;
    Row.bTracked = Item.Tracked;
    Row.Title = TitleText(Item);
    Row.StatusLabel = StatusText(Item.Section, Item.Status);
    Row.What = WhatText(Item);
    Row.Who = WhoText(Item);
    Row.Where = WhereText(Item);
    Row.When = WhenText(Item);
    return Row;
}
} // namespace PWQuest

#undef LOCTEXT_NAMESPACE
