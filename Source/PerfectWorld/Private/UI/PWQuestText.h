#pragma once

#include "CoreMinimal.h"
#include "QuestLog.h"

class UObject;

/** One quest row: the portable QuestItem's words. Text only; nothing here is a rule. */
struct FPWQuestRow
{
    /** The portable item id ("quest.today.school_bell"); stable, used for selection and tracking, never shown. */
    FString Id;
    PerfectWorld::QuestSection Section = PerfectWorld::QuestSection::Today;
    PerfectWorld::QuestStatus Status = PerfectWorld::QuestStatus::Open;
    bool bTracked = false;
    FText Title;
    /** For example "Due now". */
    FText StatusLabel;
    FText What;
    FText Who;
    FText Where;
    FText When;
};

struct FPWQuestSectionModel
{
    PerfectWorld::QuestSection Section = PerfectWorld::QuestSection::Today;
    FText Heading;
    /** Why the section is empty. */
    FText EmptyNote;
    TArray<FPWQuestRow> Rows;
};

/** Everything the quest window shows. Built from the portable QuestLogView; it holds no state of its own. */
struct FPWQuestModel
{
    /** False when the lane world could not be read at all (no game world); the window then says so. */
    bool bAvailable = false;
    /** The world clock's own words, for example "Mon 2 Jun · 07:42". */
    FText Clock;
    /** Always the three sections, in the order Today, Promises, Discoveries. */
    TArray<FPWQuestSectionModel> Sections;

    int32 RowCount() const;
    const FPWQuestRow* FindRow(const FString& Id) const;
    FPWQuestRow* FindRow(const FString& Id);
    /** The first row of the first section that has one, or null. */
    const FPWQuestRow* FirstRow() const;
};

/** What the HUD line under the minimap area says about the tracked quest. */
struct FPWQuestTrackedLine
{
    bool bVisible = false;
    FString Id;
    FText Title;
    /** The short "when" or status, for example "By 10:00, the first bell". */
    FText When;
    PerfectWorld::QuestStatus Status = PerfectWorld::QuestStatus::Open;
};

/**
 * The quest window's words, layout rules and session state, linked by name from the quest window, the HUD tracker and the
 * automation tests. All wording lives here, keyed by the portable projection's text keys (QuestItem::TextKey). An unknown,
 * empty or malformed key reads as neutral text, never as the key.
 */
namespace PWQuest
{
// ---- Words ----

FText SectionHeading(PerfectWorld::QuestSection Section);
FText SectionEmptyNote(PerfectWorld::QuestSection Section);
FText TitleText(const PerfectWorld::QuestItem& Item);
FText WhatText(const PerfectWorld::QuestItem& Item);
FText WhoText(const PerfectWorld::QuestItem& Item);
FText WhereText(const PerfectWorld::QuestItem& Item);
FText WhenText(const PerfectWorld::QuestItem& Item);
FText StatusText(PerfectWorld::QuestSection Section, PerfectWorld::QuestStatus Status);
/** The one-line "when" for the HUD tracker. */
FText ShortWhenText(const PerfectWorld::QuestItem& Item);
/** "HH:MM" from a minute of the day. */
FText TimeText(int32 MinuteOfDay);
/** "16:00 to 19:30, 17:30 to 20:30". */
FText WindowsText(const std::vector<PerfectWorld::QuestWindow>& Windows);

FPWQuestRow MakeRow(const PerfectWorld::QuestItem& Item);

/** Formats the projection. TrackedId marks the row that is tracked (empty or unknown marks none). */
FPWQuestModel BuildModel(const PerfectWorld::QuestLogView& Log, const FText& Clock, const FString& TrackedId);
FPWQuestTrackedLine BuildTrackedLine(const PerfectWorld::QuestLogView& Log, const FString& TrackedId);

/** Reads the one authoritative world (UPWLaneLifeSubsystem) and projects now; bAvailable is false when there is none. */
FPWQuestModel ReadModel(const UObject* WorldContext);
/** The tracked quest from the same world; not visible when nothing is tracked, the item is gone or it is done. */
FPWQuestTrackedLine ReadTrackedLine(const UObject* WorldContext);

// ---- Session state. Never saved: the tracked id lives only until the game closes. ----

FString GetTrackedId();
/** Empty clears. */
void SetTrackedId(const FString& Id);
/** Tracks this id, or clears when it is already the tracked one. Returns whether it is tracked afterwards. */
bool ToggleTracked(const FString& Id);
/** Changes every time the tracked id changes; the HUD line uses it to know when to re-read. */
uint32 TrackedRevision();

// ---- Opening from the world ----

/**
 * J and the controller's left shoulder button (the walking input) call RequestToggle. The front end HUD installs the handler
 * that pauses and opens the quest window (see the HUD hook in the order report); with none installed the request does
 * nothing and returns false.
 */
void SetToggleHandler(TFunction<void()> Handler);
bool RequestToggle();
/** Set by the HUD just before it pauses so the pause menu opens straight onto the quest window; consumed once. */
void SetOpenOnNextPause(bool bOpen);
bool ConsumeOpenOnNextPause();

// ---- Layout: the same density switch as the pause menu (short or narrow windows are compact) ----

bool Compact(float Width, float Height);
/** Wide enough for the list and the detail side by side; otherwise the detail sits under the list. */
bool TwoPane(float Width, float Height);
float PanelWidth(float Width, float Height);
float PanelMaxHeight(float Width, float Height);
float ListWidth(float Width, float Height);

/** Widget tags the tests search for (widgets carry FName(tag)). */
inline const TCHAR* const PanelTag = TEXT("PWQuestPanel");
inline const TCHAR* const RowTag = TEXT("PWQuestRow");
inline const TCHAR* const EmptySectionTag = TEXT("PWQuestEmptySection");
inline const TCHAR* const UnavailableTag = TEXT("PWQuestUnavailable");
inline const TCHAR* const DetailTag = TEXT("PWQuestDetail");
inline const TCHAR* const TrackerTag = TEXT("PWQuestTracker");
} // namespace PWQuest
