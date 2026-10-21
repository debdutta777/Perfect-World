#pragma once

// Shared by the quest window's automation tests (order F3): widget walking helpers, the fullest world the quest log can project, and a
// throw-away lane world. Tests only; never part of a shipping build.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include <algorithm>
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameClock.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "QuestLog.h"
#include "UI/PWQuestText.h"
#include "WorldState.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace QwTests
{
using namespace PerfectWorld;

inline void QwWalk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
    const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
    Visit(Widget, bShown);
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
    {
        QwWalk(Children->GetChildAt(Index), bShown, Visit);
    }
}

inline TArray<FString> QwShownTexts(const TSharedRef<SWidget>& Root)
{
    Root->SlatePrepass(1.0f);
    TArray<FString> Texts;
    QwWalk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock"))
        {
            Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
        }
    });
    return Texts;
}

inline int32 QwCountTag(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
    Root->SlatePrepass(1.0f);
    int32 Count = 0;
    QwWalk(Root, true, [&Count, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTag() == FName(Tag)) ++Count;
    });
    return Count;
}

inline bool QwHas(const TArray<FString>& Texts, const FString& Exact) { return Texts.Contains(Exact); }

/** Clicks the first shown button whose text is exactly this. */
inline bool QwClick(const TSharedRef<SWidget>& Root, const FString& Label)
{
    Root->SlatePrepass(1.0f);
    TSharedPtr<SButton> Target;
    QwWalk(Root, true, [&Target, &Label](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (!Target.IsValid() && bShown && Widget->GetTypeAsString() == TEXT("SButton") && QwHas(QwShownTexts(Widget), Label))
        {
            Target = StaticCastSharedRef<SButton>(Widget);
        }
    });
    if (!Target.IsValid()) return false;
    Target->SimulateClick();
    Root->SlatePrepass(1.0f);
    return true;
}

inline FKeyEvent QwKey(const FKey Key, const bool bRepeat = false)
{
    return FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0);
}

inline GameInstant QwAt(const int Day, const int Hour, const int Minute)
{
    return GameClock::FromCalendar(2042, 6, Day, Hour, Minute);
}

inline void QwSay(WorldState& World, const char* Person, std::initializer_list<const char*> Facts)
{
    ConversationState& State = World.Conversations[Person];
    for (const char* Fact : Facts) State.Facts.push_back(Fact);
    std::sort(State.Facts.begin(), State.Facts.end());
}

/** A Monday-morning world with a plan, a commitment and every agreement the journal can show: the fullest log there is. */
inline WorldState QwRichWorld()
{
    WorldState World = MakeWorldState(77);
    QwSay(World, "person.household_adult", {"fact.evening_rota_dishes_weekdays"});
    QwSay(World, "person.care_worker", {"fact.care_jobs_breakfast_weekdays", "fact.care_jobs_study_room", "fact.care_jobs_turns"});
    QwSay(World, "person.nirmal_bose", {"fact.nirmal_bose.book_evening_agreed"});
    QwSay(World, "person.rupa_ghosh", {"fact.rupa_ghosh.visit_promised"});
    QwSay(World, "person.sahana_mitra", {"fact.sahana_mitra.sewing_evening_agreed"});
    QwSay(World, "person.subrata_dey", {"fact.subrata_dey.step_promised"});
    QwSay(World, "person.debashis_pal", {"fact.debashis_pal.bring_promised"});
    QwSay(World, "person.mahua_saha", {"fact.mahua_saha.sort_promised"});
    QwSay(World, "person.prabir_chatterjee", {"fact.prabir_chatterjee.lane_stories_agreed"});
    QwSay(World, "person.ishita_banerjee", {"fact.ishita_banerjee.quiet_evening_agreed"});
    World.DayLoop.Planned = true;
    World.DayLoop.Plan = {{540, "activity.study_at_home"}, {720, "activity.eat_meal"}, {900, "activity.exercise_run"}};
    World.DayLoop.HasCommitment = true;
    World.DayLoop.Commitment.ActivityId = "activity.household_chores";
    World.DayLoop.Commitment.PlaceKind = "home";
    return World;
}

inline QuestLogView QwRichLog() { return MakeQuestLog(QwRichWorld(), QwAt(2, 9, 0)); }

inline QuestItem QwItem(const char* Id, const char* TextKey, const QuestSection Section = QuestSection::Today)
{
    QuestItem Item;
    Item.Id = Id;
    Item.TextKey = TextKey;
    Item.Section = Section;
    return Item;
}

/** Text that must never reach a player: any part of a key or id. */
inline bool QwLooksRaw(const FString& Text)
{
    for (const TCHAR* Part : {TEXT("quest."), TEXT("journal."), TEXT("location."), TEXT("place."), TEXT("activity."), TEXT("person."), TEXT("fact."), TEXT("_"), TEXT("{")})
    {
        if (Text.Contains(Part)) return true;
    }
    return false;
}

/** A throw-away game world with a started life, so the lane clock exists; play never begins. */
struct FQwScene
{
    UWorld* World = nullptr;
    UGameInstance* Instance = nullptr;
    APerfectWorldFrontEndHUD* Hud = nullptr;

    FQwScene()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Instance = NewObject<UGameInstance>();
        Instance->AddToRoot();
        Instance->Init();
        if (World == nullptr) return;
        World->SetGameInstance(Instance);
        UPerfectWorldCharacterCreatorSubsystem* Creator = Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>();
        Hud = World->SpawnActor<APerfectWorldFrontEndHUD>();
        if (Creator == nullptr) return;
        Creator->BeginNewCreation();
        if (Creator->ApplyBalancedScholarPreset()) Creator->StartLife();
    }
    ~FQwScene()
    {
        PWQuest::SetTrackedId(FString());
        if (World != nullptr) World->DestroyWorld(false);
        Instance->Shutdown();
        Instance->RemoveFromRoot();
    }
    FQwScene(const FQwScene&) = delete;
};
} // namespace QwTests

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
