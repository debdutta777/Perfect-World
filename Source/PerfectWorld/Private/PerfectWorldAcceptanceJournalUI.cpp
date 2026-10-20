#include "PerfectWorldAcceptanceSubsystem.h"

#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "GameClock.h"
#include "InputCoreTypes.h"
#include "LaneJournal.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceStationery.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "UI/SPWJournalPanel.h"
#include "WorldSave.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

// journal_today (R4-05): the pause menu's Today screen against core truth, in a fresh-process pair like save_restart (AW-02 --restart-pair:
// Start=new, then a FRESH process with Start=continue). Every process lists the same planned steps; the steps of the other phase pass as
// "belongs to the other phase".
//
// New-life phase: Tuesday 08:45, the declared start on the road, then the player WALKS to the north footpath and talks to two people the way
// the E key does (the real interaction component's TryInteract): Arun Paul, then Rekha Bose, each at their pause. In each conversation the
// player chooses the one authored choice ("Ask about his walk" / "Ask about her errands"), so the core records the encounter. The core's
// LaneJournalToday for today must then name exactly those two people, each first met at the minute of the choice (before its time charge).
// Then, in BOTH phases (the continue phase checks the loaded slot's world instead):
//   pause (the Escape key's call) > keyboard Down reaches Today > Enter opens it > the rows the screen DISPLAYS equal LaneJournalToday for
//   today (names, "Met at HH:MM" from the portable calendar, agreement person labels and short text, or the empty states) > the clock
//   did not move while it was read > a shot > D-pad scrolling keeps the panel > the controller's B closes it and focus is back on the Today
//   entry > A reopens it > the rows again > Escape closes it, focus is on Today, the pause menu is still up and time is still held.
// New-life phase only: Save (the pause menu's own call) and the authenticated slot's journal and projection are compared with the live world.
// Both phases end by resuming. Capture at 853x480, 1280x720 and 1920x1080 and at 150% UI scale by running the pair once per setting: the shot
// names are the same, the runner's resolution arguments differ.
//
// What it does NOT claim: no agreement row is produced here (neither person makes one; the agreement text rows are proved by the panel's
// automation test and the journal_state scenario's core truth). The panel has no motion, so reduced motion needs nothing of it.
//
// Integrator hooks (do not add them in this worker):
// PerfectWorldAcceptanceSubsystem.h, namespace PWAcceptance:
//     void AppendJournalTodaySteps(TArray<FStepDef>& OutSteps);       // beside AppendJournalStateSteps
// BuildScenario in PerfectWorldAcceptanceLaneScenario.cpp:
//     if (Name == TEXT("journal_today")) { AppendJournalTodaySteps(OutSteps); return true; }
namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

const FName JpAcArun(TEXT("person.arun_paul"));
const FName JpAcRekha(TEXT("person.rekha_bose"));

struct FJpAcState
{
    bool bContinue = false;
    PerfectWorld::WorldSaveData Saved;
    /** New-life phase: LaneJournalToday after the two conversations; it is the expectation for every later look. */
    PerfectWorld::LaneJournalView Truth;
    PerfectWorld::GameInstant NowAtPause = -1;
};

FStepDef JpAcAction(const FString& Name, const double Timeout, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Def;
    Def.Name = Name;
    Def.TimeoutSeconds = Timeout;
    Def.Run = MoveTemp(Run);
    return Def;
}

FStepDef JpAcPhase(FStepDef Def, const TSharedRef<FJpAcState> State, const bool bContinue)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("belongs to the other fresh-process phase"));
    };
    return Def;
}

bool JpAcReadSlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Out)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
           PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Out).HashVerified;
}

// ---- the Slate tree

void JpAcWalk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
    const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
    Visit(Widget, bShown);
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index) JpAcWalk(Children->GetChildAt(Index), bShown, Visit);
}

TSharedPtr<SWidget> JpAcFindTag(const FName Tag)
{
    TSharedPtr<SWidget> Found;
    if (!FSlateApplication::IsInitialized()) return Found;
    for (const TSharedRef<SWindow>& Window : FSlateApplication::Get().GetTopLevelWindows())
    {
        JpAcWalk(Window, true, [&Found, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
        {
            if (!Found.IsValid() && bShown && Widget->GetTag() == Tag) Found = Widget;
        });
    }
    return Found;
}

TArray<FString> JpAcTexts(const TSharedRef<SWidget>& Root)
{
    TArray<FString> Texts;
    JpAcWalk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock")) Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
    });
    return Texts;
}

TArray<TSharedRef<SWidget>> JpAcFindAll(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
    TArray<TSharedRef<SWidget>> Found;
    JpAcWalk(Root, true, [&Found, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
    {
        if (bShown && Widget->GetTag() == FName(Tag)) Found.Add(Widget);
    });
    return Found;
}

/** The label of the button that has keyboard focus, or empty when focus is elsewhere. */
FString JpAcFocusedButton()
{
    if (!FSlateApplication::IsInitialized()) return FString();
    const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
    if (!Focused.IsValid() || Focused->GetTypeAsString() != TEXT("SButton")) return FString();
    const TArray<FString> Texts = JpAcTexts(Focused.ToSharedRef());
    return Texts.Num() > 0 ? Texts[0] : FString();
}

/** One key press and release through Slate's own routing, so focus navigation, button activation and the screens' key handlers all run. */
bool JpAcPress(const FKey& Key)
{
    if (!FSlateApplication::IsInitialized()) return false;
    FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
    FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key, FModifierKeysState(), 0, false, 0, 0));
    return true;
}

/** Sends the key once, then passes when Reached says the screen answered. */
FStepDef JpAcKeyStep(const FString& Name, const FKey Key, TFunction<bool(UPWAcceptanceSubsystem&, FString&)> Reached)
{
    return JpAcAction(Name, 15.0, [Key, Reached](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        if (Rt.Phase == 0)
        {
            if (!JpAcPress(Key)) return FStepOutcome::Fail(TEXT("Slate is not available to take the key"));
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        FString Said;
        if (Reached(Sub, Said)) return FStepOutcome::Pass(FString::Printf(TEXT("%s: %s"), *Key.ToString(), *Said));
        Rt.Note = FString::Printf(TEXT("%s pressed; focus is on '%s'; %s"), *Key.ToString(), *JpAcFocusedButton(), *Said);
        return FStepOutcome::Wait();
    });
}

// ---- core truth against the displayed rows

/** Expectation for this phase: the live world's projection for today, which must equal the conversations' truth (new) or the loaded slot's (continue). */
bool JpAcExpected(UPWAcceptanceSubsystem& Sub, const FJpAcState& State, PerfectWorld::LaneJournalView& Out, FString& Why)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    if (Life == nullptr) { Why = TEXT("no lane-life subsystem"); return false; }
    const auto Day = PerfectWorld::GameClock::DayNumber(Life->GetWorldState().Clock.Now());
    const PerfectWorld::LaneJournalView Live = PerfectWorld::LaneJournalToday(Life->GetWorldState(), Day);
    Out = State.bContinue ? PerfectWorld::LaneJournalToday(State.Saved.World, Day) : State.Truth;
    if (Out != Live) { Why = TEXT("the live core projection for today differs from the conversations' truth or the saved slot's"); return false; }
    if (Out.PeopleMetToday.size() != 2 || Out.PeopleMetToday[0].PersonId != "person.arun_paul" || Out.PeopleMetToday[1].PersonId != "person.rekha_bose")
    {
        Why = TEXT("the projection does not name exactly Arun Paul and Rekha Bose");
        return false;
    }
    return true;
}

/** Empty when the panel's displayed rows are exactly the projection's; otherwise what differs. */
FString JpAcCompareRows(const TSharedRef<SWidget>& Panel, const PerfectWorld::LaneJournalView& Truth)
{
    const TArray<FString> All = JpAcTexts(Panel);
    if (!All.Contains(TEXT("Today")) || !All.Contains(TEXT("People met today")) || !All.Contains(TEXT("Agreements remembered"))) return TEXT("title or section headings are not shown");
    const TArray<TSharedRef<SWidget>> People = JpAcFindAll(Panel, PWJournal::PersonRowTag);
    if (People.Num() != static_cast<int32>(Truth.PeopleMetToday.size())) return FString::Printf(TEXT("%d person rows shown, core says %d"), People.Num(), static_cast<int32>(Truth.PeopleMetToday.size()));
    for (int32 Index = 0; Index < People.Num(); ++Index)
    {
        const PerfectWorld::LaneJournalPerson& Person = Truth.PeopleMetToday[static_cast<size_t>(Index)];
        const PerfectWorld::CalendarTime When = PerfectWorld::GameClock::ToCalendar(Person.FirstMetMinute);
        const TArray<FString> Row = JpAcTexts(People[Index]);
        const FString WantName = PWLaneLife::ToFString(Person.DisplayName);
        const FString WantTime = FString::Printf(TEXT("Met at %02d:%02d"), When.Hour, When.Minute);
        if (Row.Num() != 2 || !Row[0].Equals(WantName, ESearchCase::CaseSensitive) || !Row[1].Equals(WantTime, ESearchCase::CaseSensitive))
            return FString::Printf(TEXT("person row %d shows '%s' but core says '%s' / '%s'"), Index, *FString::Join(Row, TEXT(" | ")), *WantName, *WantTime);
    }
    const TArray<TSharedRef<SWidget>> Agreements = JpAcFindAll(Panel, PWJournal::AgreementRowTag);
    if (Agreements.Num() != static_cast<int32>(Truth.AgreementsRemembered.size())) return FString::Printf(TEXT("%d agreement rows shown, core says %d"), Agreements.Num(), static_cast<int32>(Truth.AgreementsRemembered.size()));
    for (int32 Index = 0; Index < Agreements.Num(); ++Index)
    {
        const PerfectWorld::LaneJournalAgreement& Agreement = Truth.AgreementsRemembered[static_cast<size_t>(Index)];
        const TArray<FString> Row = JpAcTexts(Agreements[Index]);
        const FString WantWho = PWLaneLife::ToFString(PerfectWorld::LaneJournalDisplayName(Agreement.PersonId));
        const FString WantText = PWJournal::AgreementText(PWLaneLife::ToFString(Agreement.ShortTextKey)).ToString();
        if (Row.Num() != 2 || !Row[0].Equals(WantWho, ESearchCase::CaseSensitive) || !Row[1].Equals(WantText, ESearchCase::CaseSensitive) || Row[1].Contains(TEXT("journal.agreement")))
            return FString::Printf(TEXT("agreement row %d shows '%s' but core says '%s' / '%s'"), Index, *FString::Join(Row, TEXT(" | ")), *WantWho, *WantText);
    }
    const bool bNoPeople = JpAcFindAll(Panel, PWJournal::EmptyPeopleTag).Num() == 1;
    const bool bNoAgreements = JpAcFindAll(Panel, PWJournal::EmptyAgreementsTag).Num() == 1;
    if (bNoPeople != Truth.PeopleMetToday.empty() || bNoAgreements != Truth.AgreementsRemembered.empty()) return TEXT("an empty state is shown (or missing) against the core's lists");
    return FString();
}

FStepOutcome JpAcRowsStep(UPWAcceptanceSubsystem& Sub, const FJpAcState& State)
{
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    const TSharedPtr<SWidget> Panel = JpAcFindTag(FName(PWJournal::PanelTag));
    PerfectWorld::LaneJournalView Truth;
    FString Why;
    if (Life == nullptr || !Panel.IsValid()) return FStepOutcome::Fail(TEXT("the Today panel is not on screen"));
    if (!JpAcExpected(Sub, State, Truth, Why)) return FStepOutcome::Fail(Why);
    Why = JpAcCompareRows(Panel.ToSharedRef(), Truth);
    if (!Why.IsEmpty()) return FStepOutcome::Fail(Why);
    if (Life->GetNow() != State.NowAtPause || Life->GetClockHold() != EPWClockHold::Menu) return FStepOutcome::Fail(TEXT("the world clock moved, or is not held, while the journal was open"));
    return FStepOutcome::Pass(FString::Printf(TEXT("the Today screen shows the %d people met today (with first-met times) and the %d agreements the core projects; clock held at %s"),
                                              static_cast<int32>(Truth.PeopleMetToday.size()), static_cast<int32>(Truth.AgreementsRemembered.size()), *Life->GetClockText().ToString()));
}

// ---- a real conversation: walk (elsewhere), E key, choose, leave

FStepDef JpAcTalkAndChoose(const FString& Name, const FName Person, const FName Choice, const TSharedRef<FJpAcState> State)
{
    return JpAcAction(Name, 90.0, [Person, Choice, State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        UWorld* World = Sub.GetPlayWorld();
        UPWConversationPresenter* Talk = UPWConversationPresenter::Get(World);
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(World);
        UPWInteractionComponent* Interaction = Sub.GetInteraction();
        const UPWNpcPresenceSubsystem* Presence = World != nullptr ? World->GetSubsystem<UPWNpcPresenceSubsystem>() : nullptr;
        if (Talk == nullptr || Life == nullptr || Interaction == nullptr || Presence == nullptr) return FStepOutcome::Fail(TEXT("a conversation, lane-life, interaction or presence service is missing"));
        const auto OpenWith = [Life, Person]() { return PWLaneLife::ToName(PerfectWorld::OpenConversationPersonId(Life->GetWorldState())) == Person; };
        if (Rt.Phase == 0) // face the person and wait for the real prompt, then press E
        {
            AActor* Npc = Presence->FindPerson(Person);
            const IPWInteractable* Interactable = Cast<IPWInteractable>(Npc);
            if (Npc == nullptr || Interactable == nullptr) { Rt.Note = FString::Printf(TEXT("%s is not on the lane"), *Person.ToString()); return FStepOutcome::Wait(); }
            Sub.FaceWorldPoint(Interactable->GetInteractionPoint());
            if (Interaction->GetTarget() != Npc || !Interaction->IsPromptVisible()) { Rt.Note = TEXT("waiting for the interaction prompt"); return FStepOutcome::Wait(); }
            if (UPWInteractionComponent::IsWorldInputBlocked(Sub.GetPawn())) return FStepOutcome::Fail(TEXT("a menu or conversation owns the input"));
            if (!Interaction->TryInteract()) return FStepOutcome::Fail(TEXT("TryInteract refused"));
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 1) // the panel opened: choose the authored choice if this is the greeting that offers it
        {
            const PerfectWorld::ConversationState* Memory = Life->FindConversation(Person);
            if (!Talk->IsOpen() || !OpenWith() || Memory == nullptr)
            {
                if (Rt.InPhase() > 6.0) return FStepOutcome::Fail(TEXT("TryInteract did not open the person's conversation"));
                return FStepOutcome::Wait();
            }
            if (!FString(PWLaneLife::ToFString(Memory->NodeId)).EndsWith(TEXT(".greet")))
            {
                Talk->LeaveConversation(); // walking, not paused: no choice is offered; try again when the prompt allows
                Rt.Phase = 0;
                Rt.Note = TEXT("the person was walking, not paused; left and trying again");
                return FStepOutcome::Wait();
            }
            Rt.LastSize = Life->GetNow(); // the minute of the choice, before its time charge
            Talk->ChooseOption(Choice);
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Rt.Phase == 2) // the core must have recorded this person today, first met at that minute
        {
            const auto Day = PerfectWorld::GameClock::DayNumber(Rt.LastSize);
            bool bRecorded = false;
            for (const PerfectWorld::LaneJournalEncounter& Row : Life->GetWorldState().Journal.Encounters)
                bRecorded = bRecorded || (Row.Day == Day && Row.PersonId == PWLaneLife::ToStd(Person) && Row.FirstMetMinute == Rt.LastSize);
            if (!bRecorded) return FStepOutcome::Fail(FString::Printf(TEXT("the core did not record %s as met at minute %lld"), *Person.ToString(), Rt.LastSize));
            if (Talk->IsOpen()) Talk->LeaveConversation();
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        if (Talk->IsOpen() || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty()) return FStepOutcome::Wait();
        return FStepOutcome::Pass(FString::Printf(TEXT("pressed E, chose '%s', the core recorded the encounter at minute %lld, Leave closed the conversation"), *Choice.ToString(), Rt.LastSize));
    });
}

FStepDef JpAcApproach(const FString& Name, const FName Person)
{
    return PWAcceptance::MakeWalkStep(Name,
        [Person](UPWAcceptanceSubsystem& Sub, FVector& Target, FString&)
        {
            const UPWNpcPresenceSubsystem* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
            const APWNpcCharacter* Npc = Presence != nullptr ? Presence->FindPerson(Person) : nullptr;
            // Until they arrive, wait near their north-alley pause, as the ambient scenario does.
            Target = Npc != nullptr ? Npc->GetActorLocation() + FVector(-110.0, 0.0, 0.0) : FVector(2940.0, 340.0, PWLane::GroundZ + 15.0);
            return true;
        }, 45.0, 50.0);
}

FStepDef JpAcWaitFor(const FString& Name, const FName Person)
{
    return JpAcAction(Name, 55.0, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        Sub.FaceWorldPoint(FVector(2000.0, 340.0, PWLane::GroundZ + 100.0));
        const UPWNpcPresenceSubsystem* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        return Presence != nullptr && Presence->FindPerson(Person) != nullptr ? FStepOutcome::Pass(TEXT("the scheduled passer arrived")) : FStepOutcome::Wait();
    });
}

FStepDef JpAcReach(const FString& Name, const FName Person)
{
    return PWAcceptance::MakeWalkStep(Name,
        [Person](UPWAcceptanceSubsystem& Sub, FVector& Target, FString&)
        {
            const UPWNpcPresenceSubsystem* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
            const APWNpcCharacter* Npc = Presence != nullptr ? Presence->FindPerson(Person) : nullptr;
            if (Npc == nullptr) return false;
            Target = Npc->GetActorLocation() + FVector(-110.0, 0.0, 0.0);
            return true;
        }, 35.0, 25.0);
}

// ---- the pause menu

bool JpAcPausedWithCard() { return JpAcFindTag(FName(TEXT("CpPauseCard"))).IsValid(); }

FStepDef JpAcOpenPause(const TSharedRef<FJpAcState> State)
{
    return JpAcAction(TEXT("jt_open_pause"), 20.0, [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt) -> FStepOutcome
    {
        APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Hud == nullptr || Life == nullptr) return FStepOutcome::Fail(TEXT("no HUD or lane-life subsystem"));
        if (Rt.Phase == 0)
        {
            if (!Hud->IsPaused()) Hud->ToggleInWorldMenu(); // the call Escape makes in the world
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        Rt.Note = FString::Printf(TEXT("paused %d, card %d, clock hold %d"), Hud->IsPaused(), JpAcPausedWithCard(), static_cast<int32>(Life->GetClockHold()));
        if (!Hud->IsPaused() || !JpAcPausedWithCard() || Life->GetClockHold() != EPWClockHold::Menu) return FStepOutcome::Wait();
        State->NowAtPause = Life->GetNow();
        return FStepOutcome::Pass(FString::Printf(TEXT("pause menu up, time held at %s"), *Life->GetClockText().ToString()));
    });
}

bool JpAcToday(UPWAcceptanceSubsystem&, FString& Said)
{
    const bool bPanel = JpAcFindTag(FName(PWJournal::PanelTag)).IsValid();
    Said = FString::Printf(TEXT("panel shown %d"), bPanel);
    return bPanel && JpAcFocusedButton() == TEXT("Back");
}

bool JpAcMenuWithFocusOnToday(UPWAcceptanceSubsystem& Sub, FString& Said)
{
    const APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
    const bool bPanel = JpAcFindTag(FName(PWJournal::PanelTag)).IsValid();
    Said = FString::Printf(TEXT("panel shown %d, still paused %d"), bPanel, Hud != nullptr && Hud->IsPaused());
    return !bPanel && Hud != nullptr && Hud->IsPaused() && JpAcPausedWithCard() && JpAcFocusedButton() == TEXT("Today");
}

void JpAcAppendTodayVisit(TArray<FStepDef>& Out, const TSharedRef<FJpAcState> State)
{
    Out.Add(JpAcOpenPause(State));
    Out.Add(JpAcAction(TEXT("jt_focus_starts_on_resume"), 10.0, [](UPWAcceptanceSubsystem&, FStepRuntime& Rt)
    {
        Rt.Note = FString::Printf(TEXT("focus is on '%s'"), *JpAcFocusedButton());
        return JpAcFocusedButton() == TEXT("Resume") ? FStepOutcome::Pass(TEXT("keyboard focus starts on Resume")) : FStepOutcome::Wait();
    }));
    Out.Add(JpAcKeyStep(TEXT("jt_keyboard_down_reaches_today"), EKeys::Down, [](UPWAcceptanceSubsystem&, FString& Said)
    {
        Said = TEXT("waiting for Today");
        return JpAcFocusedButton() == TEXT("Today");
    }));
    Out.Add(JpAcKeyStep(TEXT("jt_keyboard_enter_opens_today"), EKeys::Enter, JpAcToday));
    Out.Add(JpAcAction(TEXT("jt_today_rows_equal_core"), 10.0, [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        // Let a second of real time pass inside the panel: reading must not move the clock.
        return Rt.Elapsed < 1.0 ? FStepOutcome::Wait() : JpAcRowsStep(Sub, *State);
    }));
    Out.Add(JpAcPhase(PWAcceptance::MakeShotStep(TEXT("shot_today_screen_new")), State, false));
    Out.Add(JpAcPhase(PWAcceptance::MakeShotStep(TEXT("shot_today_screen_continue")), State, true));
    Out.Add(JpAcKeyStep(TEXT("jt_dpad_scrolls_panel_stays"), EKeys::Gamepad_DPad_Down, [](UPWAcceptanceSubsystem& Sub, FString& Said)
    {
        return JpAcToday(Sub, Said); // scrolling is handled by the panel; focus and the panel do not change
    }));
    Out.Add(JpAcKeyStep(TEXT("jt_controller_b_closes_focus_on_today"), EKeys::Gamepad_FaceButton_Right, JpAcMenuWithFocusOnToday));
    Out.Add(JpAcKeyStep(TEXT("jt_controller_a_opens_today_again"), EKeys::Gamepad_FaceButton_Bottom, JpAcToday));
    Out.Add(JpAcAction(TEXT("jt_today_rows_equal_core_again"), 10.0, [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&) { return JpAcRowsStep(Sub, *State); }));
    Out.Add(JpAcKeyStep(TEXT("jt_escape_closes_focus_on_today"), EKeys::Escape, JpAcMenuWithFocusOnToday));
    Out.Add(JpAcAction(TEXT("jt_time_held_across_the_visit"), 10.0, [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Life->GetNow() != State->NowAtPause) return FStepOutcome::Fail(TEXT("the world clock moved between pausing and closing the journal"));
        return FStepOutcome::Pass(FString::Printf(TEXT("clock still %s after two visits"), *Life->GetClockText().ToString()));
    }));
}
} // namespace

namespace PWAcceptance
{
void AppendJournalTodaySteps(TArray<FStepDef>& Out)
{
    const auto State = MakeShared<FJpAcState>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(JpAcAction(TEXT("jt_restart_phase_and_slot"), 20.0, [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("journal_today requires AW-02 --restart-pair (explicit new, then continue)"));
        State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (State->bContinue && !JpAcReadSlot(Sub, State->Saved)) return FStepOutcome::Fail(TEXT("the original slot is absent, damaged or lacks current hash certification"));
        return FStepOutcome::Pass(State->bContinue ? TEXT("fresh-process Continue; the original slot was captured") : TEXT("new-life phase"));
    }));
    Out.Add(MakeEnterWorldStep());

    // ---- new-life phase: two real conversations on the north footpath
    Out.Add(JpAcPhase(Stationery::JumpClockStep(TEXT("jt_clock_tuesday_0845"), 8, 45), State, false));
    Out.Add(JpAcPhase(JpAcAction(TEXT("jt_declared_start"), 10.0, [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        APerfectWorldPlayerCharacter* Pawn = Sub.GetPawn();
        if (Pawn == nullptr || Sub.GetController() == nullptr) return FStepOutcome::Fail(TEXT("no possessed pawn"));
        Pawn->SetActorLocation(FVector(2200.0, -150.0, PWLane::GroundZ + PWPlayer::CapsuleHalfHeight + 2.0), false); // the scenario's one teleport
        Sub.GetController()->SetControlRotation(FRotator(0.0, 180.0, 0.0));
        return FStepOutcome::Pass(TEXT("declared start on the road, facing west"));
    }), State, false));
    Out.Add(JpAcPhase(PWAcceptance::MakeWalkStep(TEXT("jt_walk_north_footpath"),
        [](UPWAcceptanceSubsystem&, FVector& Target, FString&) { Target = FVector(2500.0, 340.0, PWLane::GroundZ + 15.0); return true; }, 25.0, 40.0), State, false));
    Out.Add(JpAcPhase(JpAcApproach(TEXT("jt_approach_arun_paul"), JpAcArun), State, false));
    Out.Add(JpAcPhase(JpAcWaitFor(TEXT("jt_wait_for_arun_paul"), JpAcArun), State, false));
    Out.Add(JpAcPhase(JpAcReach(TEXT("jt_reach_arun_paul"), JpAcArun), State, false));
    Out.Add(JpAcPhase(JpAcTalkAndChoose(TEXT("jt_talk_to_arun_paul"), JpAcArun, FName(TEXT("choice.arun_paul.round")), State), State, false));
    Out.Add(JpAcPhase(JpAcApproach(TEXT("jt_approach_rekha_bose"), JpAcRekha), State, false));
    Out.Add(JpAcPhase(JpAcWaitFor(TEXT("jt_wait_for_rekha_bose"), JpAcRekha), State, false));
    Out.Add(JpAcPhase(JpAcReach(TEXT("jt_reach_rekha_bose"), JpAcRekha), State, false));
    Out.Add(JpAcPhase(JpAcTalkAndChoose(TEXT("jt_talk_to_rekha_bose"), JpAcRekha, FName(TEXT("choice.rekha_bose.round")), State), State, false));
    Out.Add(JpAcPhase(JpAcAction(TEXT("jt_core_truth_after_conversations"), 10.0, [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr) return FStepOutcome::Fail(TEXT("no lane-life subsystem"));
        State->Truth = PerfectWorld::LaneJournalToday(Life->GetWorldState(), PerfectWorld::GameClock::DayNumber(Life->GetNow()));
        PerfectWorld::LaneJournalView Same;
        FString Why;
        if (!JpAcExpected(Sub, *State, Same, Why)) return FStepOutcome::Fail(Why);
        return FStepOutcome::Pass(TEXT("LaneJournalToday names Arun Paul and Rekha Bose, each first met at the minute of the choice"));
    }), State, false));

    // ---- both phases: pause > Today, keyboard, controller, Escape (the continue phase checks the loaded slot's world)
    JpAcAppendTodayVisit(Out, State);

    // ---- new-life phase: save through the pause menu's own call, compare the authenticated slot
    Out.Add(JpAcPhase(JpAcAction(TEXT("jt_save_and_compare_slot"), 20.0, [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        bool bSaved = false;
        const FText Why = Sub.GetHud() != nullptr ? Sub.GetHud()->SaveActiveGame(bSaved) : FText::GetEmpty();
        const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!bSaved || Life == nullptr || !JpAcReadSlot(Sub, State->Saved)) return FStepOutcome::Fail(Why.ToString());
        const auto Day = PerfectWorld::GameClock::DayNumber(Life->GetWorldState().Clock.Now());
        if (State->Saved.World.Journal != Life->GetWorldState().Journal || PerfectWorld::LaneJournalToday(State->Saved.World, Day) != State->Truth)
            return FStepOutcome::Fail(TEXT("the saved slot's journal or today's projection differs from the live world"));
        return FStepOutcome::Pass(TEXT("saved; the authenticated slot's journal and today's projection equal the live world's"));
    }), State, false));
    Out.Add(JpAcAction(TEXT("jt_resume"), 15.0, [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        APerfectWorldFrontEndHUD* Hud = Sub.GetHud();
        if (Hud == nullptr) return FStepOutcome::Fail(TEXT("no HUD"));
        if (Rt.Phase == 0)
        {
            Hud->ResumeGame();
            Rt.NextPhase();
            return FStepOutcome::Wait();
        }
        return Hud->IsPaused() ? FStepOutcome::Wait() : FStepOutcome::Pass(TEXT("resumed"));
    }));
}
} // namespace PWAcceptance
