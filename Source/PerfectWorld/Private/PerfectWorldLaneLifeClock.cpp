#include "PerfectWorldLaneLifeSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/App.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldReadingPresenter.h"
#include "UI/SPWClockReadout.h"
#include "Widgets/SWeakWidget.h"

#include <variant>

// The clock half of the lane-life bridge: the one WorldSim, free-roam time, the hold policy and the readout. See the class comment.

namespace
{
/** Fixed so a fresh life is the same life every time. TODO(world-save-v8): a saved world carries its own seed. */
constexpr uint64 BrWorldSeed = 0x50575F4C414E45ull;
/** A hitch (a level load, a breakpoint) must not become hours of game time: one tick counts for at most this much. */
constexpr double BrMaxTickMs = 500.0;
/** Above the interaction prompt (40) and notice (41) is crowded; the readout sits under them, far from their corner. */
constexpr int32 BrClockZOrder = 35;
/** Partial advances and settles are resubmitted at most this often; the event cap makes even one round a rarity. */
constexpr int32 BrMaxRounds = 64;

const TCHAR* const BrMonthNames[12] = {TEXT("Jan"), TEXT("Feb"), TEXT("Mar"), TEXT("Apr"), TEXT("May"), TEXT("Jun"),
                                       TEXT("Jul"), TEXT("Aug"), TEXT("Sep"), TEXT("Oct"), TEXT("Nov"), TEXT("Dec")};

// Minimal presentation of what happened (the log). Events without an overload are ignored.
template <typename T>
void BrLogEvent(const T&)
{
}

void BrLogEvent(const PerfectWorld::TimeAdvanced& Event)
{
    if (Event.To - Event.From > 1)
    {
        UE_LOG(LogPWLaneLife, Display, TEXT("Clock: %lld minutes passed (now %lld)."), static_cast<long long>(Event.To - Event.From),
               static_cast<long long>(Event.To));
    }
    else
    {
        UE_LOG(LogPWLaneLife, VeryVerbose, TEXT("Clock: now %lld."), static_cast<long long>(Event.To));
    }
}

void BrLogEvent(const PerfectWorld::DayStarted& Event)
{
    UE_LOG(LogPWLaneLife, Display, TEXT("A new day began (day %lld)."), static_cast<long long>(Event.Day));
}

void BrLogEvent(const PerfectWorld::ScheduledEventFired& Event)
{
    UE_LOG(LogPWLaneLife, Display, TEXT("Scheduled event fired: %s."), *PWLaneLife::ToFString(Event.EventId));
}

void BrLogEvent(const PerfectWorld::ConversationOpened& Event)
{
    UE_LOG(LogPWLaneLife, Display, TEXT("Conversation with %s opened (session %u, node %s)."), *PWLaneLife::ToFString(Event.PersonId),
           Event.Session, *PWLaneLife::ToFString(Event.NodeId));
}

void BrLogEvent(const PerfectWorld::ConversationChoiceAccepted& Event)
{
    UE_LOG(LogPWLaneLife, Display, TEXT("Choice %s with %s accepted (%d minutes)."), *PWLaneLife::ToFString(Event.ChoiceId),
           *PWLaneLife::ToFString(Event.PersonId), Event.CostMinutes);
}

void BrLogEvent(const PerfectWorld::ConversationEnded& Event)
{
    UE_LOG(LogPWLaneLife, Display, TEXT("Conversation with %s ended (session %u)."), *PWLaneLife::ToFString(Event.PersonId), Event.Session);
}

void BrLogEvent(const PerfectWorld::DoorChanged& Event)
{
    UE_LOG(LogPWLaneLife, Verbose, TEXT("Door %s is %s (side %d%s)."), *PWLaneLife::ToFString(Event.DoorId), Event.Open ? TEXT("open") : TEXT("closed"),
           static_cast<int32>(Event.SwingSide), Event.Settlement ? TEXT(", settled") : TEXT(""));
}
} // namespace

int32 UPWLaneLifeSubsystem::GetMinuteOfDay() const
{
    return PerfectWorld::DoorMinuteOfDay(GetNow());
}

FText UPWLaneLifeSubsystem::GetClockText() const
{
    const PerfectWorld::CalendarTime Calendar = PerfectWorld::GameClock::ToCalendar(GetNow());
    const FString Day = PWLaneLife::ToFString(PerfectWorld::WeekdayName(Calendar.DayOfWeek)).Left(3);
    const TCHAR* Month = BrMonthNames[FMath::Clamp(Calendar.Month - 1, 0, 11)];
    return FText::FromString(FString::Printf(TEXT("%s %d %s · %02d:%02d"), *Day, Calendar.Day, Month, Calendar.Hour, Calendar.Minute));
}

void UPWLaneLifeSubsystem::BuildWorld()
{
    Sim = MakeUnique<PerfectWorld::WorldSim>(BrWorldSeed);
    ClockMode = PerfectWorld::TimeModeState();
    RealMsFraction = 0.0;
    LastClockRefusal.Reset();

    // Play opens at the creation start, 2 June 2042 at 07:30; the fresh world's clock stands at the epoch, 06:00.
    // (The character creator carries no start instant of its own yet.)
    const PerfectWorld::GameInstant Start = PerfectWorld::GameClock::FromCalendar(
        PerfectWorld::CampaignStartYear, PerfectWorld::CampaignStartMonth, PerfectWorld::CampaignStartDay,
        PWLaneLife::CampaignStartMinuteOfDay / 60, PWLaneLife::CampaignStartMinuteOfDay % 60);
    PerfectWorld::AdvanceTime Advance;
    Advance.Minutes = Start - Sim->State().Clock.Now();
    PerfectWorld::Command Command;
    Command.Source = PerfectWorld::CommandSource::System;
    Command.Payload = Advance;
    const PerfectWorld::CommandResult Opened = Sim->Submit(Command);
    if (!Opened.Accepted)
    {
        UE_LOG(LogPWLaneLife, Error, TEXT("The world clock could not be set to the creation start: %hs"), Opened.Why.Code.c_str());
    }
    LastBroadcastInstant = GetNow();
    FaridaResolvedInstant = -1;
}

void UPWLaneLifeSubsystem::ResetWorld()
{
    BuildWorld();
    for (const TWeakObjectPtr<APWDoorActor>& Weak : RegisteredDoors)
    {
        APWDoorActor* Door = Weak.Get();
        if (Door == nullptr)
        {
            continue;
        }
        const std::string Id = PWLaneLife::ToStd(Door->GetDoorId());
        for (const PerfectWorld::DoorEntry& Entry : Sim->State().Doors.Doors)
        {
            if (Entry.Id == Id)
            {
                Door->SetOpenImmediate(Entry.Open, static_cast<int8>(Entry.SwingSide));
                break;
            }
        }
    }
    RefreshFaridaForClock();
    // Scheduled people on the generic presenter (LL-02) drop their actors and detours and are re-presented at the new instant.
    if (UPWNpcPresenceSubsystem* NpPresence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
    {
        NpPresence->ResetPresentation(GetNow());
    }
    OnClockChanged.Broadcast(GetNow());
}

PerfectWorld::CommandResult UPWLaneLifeSubsystem::SubmitCommand(const PerfectWorld::Command& Command)
{
    PerfectWorld::CommandResult Result = Sim->Submit(Command);
    if (Result.Accepted)
    {
        PresentEvents(Result.Events);
        // Every accepted command, even one that leaves the time unchanged (leaving a conversation can release a person to walk
        // on), reconciles the scheduled people on the presenter before the clock is broadcast.
        if (UPWNpcPresenceSubsystem* NpPresence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>())
        {
            NpPresence->Reconcile(GetNow());
        }
        HandleClockMoved();
    }
    return Result;
}

void UPWLaneLifeSubsystem::PresentEvents(const std::vector<PerfectWorld::DomainEvent>& Events) const
{
    for (const PerfectWorld::DomainEvent& Event : Events)
    {
        std::visit([](const auto& Payload) { BrLogEvent(Payload); }, Event.Payload);
    }
}

void UPWLaneLifeSubsystem::HandleClockMoved()
{
    const PerfectWorld::GameInstant Now = GetNow();
    if (Now == LastBroadcastInstant)
    {
        return;
    }
    LastBroadcastInstant = Now;
    // Farida still has her own path (LL-02 submission 2 moves her onto the presenter); everyone registered with the presenter
    // was reconciled by SubmitCommand already.
    RefreshFaridaForClock();
    OnClockChanged.Broadcast(Now);
}

void UPWLaneLifeSubsystem::SettleUntilQuiescent()
{
    for (int32 Round = 0; Round < BrMaxRounds && !PerfectWorld::IsWorldQuiescent(Sim->State()); ++Round)
    {
        PerfectWorld::Command Command;
        Command.Source = PerfectWorld::CommandSource::System;
        Command.Payload = PerfectWorld::SettleDueEvents{};
        const PerfectWorld::CommandResult Result = SubmitCommand(Command);
        if (!Result.Accepted)
        {
            UE_LOG(LogPWLaneLife, Warning, TEXT("The world could not settle: %hs"), Result.Why.Code.c_str());
            return;
        }
    }
}

void UPWLaneLifeSubsystem::AdvanceClock(const int64 Minutes)
{
    SettleUntilQuiescent();
    int64 Remaining = Minutes;
    for (int32 Round = 0; Remaining > 0 && Round < BrMaxRounds; ++Round)
    {
        PerfectWorld::AdvanceTime Advance;
        Advance.Minutes = Remaining;
        PerfectWorld::Command Command;
        Command.Source = PerfectWorld::CommandSource::System;
        Command.Payload = Advance;
        const PerfectWorld::CommandResult Result = SubmitCommand(Command);
        if (!Result.Accepted)
        {
            // Asleep, an activity running, a conversation the panel no longer shows: time stands still until it clears. A
            // standing refusal is reported once, not every second.
            const FString Code = PWLaneLife::ToFString(Result.Why.Code);
            if (Code != LastClockRefusal)
            {
                LastClockRefusal = Code;
                UE_LOG(LogPWLaneLife, Display, TEXT("The clock cannot advance: %s."), *Code);
            }
            return;
        }
        LastClockRefusal.Reset();
        Remaining -= Result.To - Result.From;
        if (!Result.Partial)
        {
            return;
        }
        // The event cap stopped the advance at an event: fire what is still due there, then carry on with the rest.
        SettleUntilQuiescent();
    }
    if (Remaining > 0)
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Dropped %lld minutes of free-roam time the world could not take."), static_cast<long long>(Remaining));
    }
}

bool UPWLaneLifeSubsystem::UpdateClockHold()
{
    const UWorld* World = GetWorld();
    const APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
    const APerfectWorldFrontEndHUD* Hud = Controller != nullptr ? Cast<APerfectWorldFrontEndHUD>(Controller->GetHUD()) : nullptr;
    const UPWConversationPresenter* Talk = UPWConversationPresenter::Get(this);
    const UPWReadingPresenter* Reading = UPWReadingPresenter::Get(this);

    EPWClockHold Hold = EPWClockHold::None;
    if (Controller == nullptr || (Hud != nullptr && Hud->IsMenuOpen()))
    {
        Hold = EPWClockHold::Menu; // title, creation, pause; or no screen yet
    }
    else if (Talk != nullptr && Talk->IsOpen())
    {
        Hold = EPWClockHold::Conversation; // only its paid choices spend time
    }
    else if (Reading != nullptr && Reading->IsOpen())
    {
        Hold = EPWClockHold::Reading; // reading is free
    }
    else if (!FApp::HasFocus())
    {
        Hold = EPWClockHold::Unfocused;
    }
    ClockHold = Hold;

    // The core's mode controller turns real time into minutes only in free roam with no overlay. Menus, the conversation and the
    // reading are its ui_paused overlay; the window losing focus is handled here, before real time is offered to it. The overlay is
    // set only when it changes: setting it drops the sub-minute carry.
    const bool bOverlay = Hold == EPWClockHold::Menu || Hold == EPWClockHold::Conversation || Hold == EPWClockHold::Reading;
    if (bOverlay != ClockMode.UiOpen)
    {
        PerfectWorld::SetUiPause(ClockMode, bOverlay);
    }
    return Hold != EPWClockHold::None;
}

void UPWLaneLifeSubsystem::CloseOrphanedConversation()
{
    const std::string_view Open = PerfectWorld::OpenConversationPersonId(Sim->State());
    if (Open.empty())
    {
        return;
    }
    const UPWConversationPresenter* Talk = UPWConversationPresenter::Get(this);
    if (Talk != nullptr && Talk->IsOpen())
    {
        return;
    }
    // The core opened a session the panel never showed (or lost): time cannot move while it stands.
    const FName Person = PWLaneLife::ToName(Open);
    UE_LOG(LogPWLaneLife, Warning, TEXT("The core has a conversation with %s open but no panel shows it; leaving it."), *Person.ToString());
    LeaveConversation(Person);
}

void UPWLaneLifeSubsystem::Tick(const float DeltaTime)
{
    Super::Tick(DeltaTime);
    const UWorld* World = GetWorld();
    if (!Sim.IsValid() || World == nullptr || !World->HasBegunPlay())
    {
        return;
    }
    EnsureClockReadout();
    CloseOrphanedConversation();
    if (UpdateClockHold())
    {
        RealMsFraction = 0.0;
        return;
    }

    // Free roam: 60 real seconds are 60 game minutes. Real milliseconds are summed exactly (the fraction of a millisecond is
    // carried), then the core's AdvanceRealTime keeps the sub-minute remainder and returns whole minutes.
    const double Offered = FMath::Min(static_cast<double>(DeltaTime) * 1000.0, BrMaxTickMs) + RealMsFraction;
    const int64 WholeMs = static_cast<int64>(FMath::FloorToDouble(Offered));
    RealMsFraction = Offered - static_cast<double>(WholeMs);
    const int64 Minutes = PerfectWorld::AdvanceRealTime(ClockMode, WholeMs);
    if (Minutes > 0)
    {
        AdvanceClock(Minutes);
    }
}

void UPWLaneLifeSubsystem::EnsureClockReadout()
{
    if (ClockHolder.IsValid() || GEngine == nullptr || GEngine->GameViewport == nullptr || IsRunningDedicatedServer())
    {
        return;
    }
    const UWorld* World = GetWorld();
    if (World == nullptr || World->GetFirstPlayerController() == nullptr)
    {
        return;
    }
    // The widget reads this subsystem through a weak pointer only; it never keeps the subsystem (or any widget) alive.
    const TWeakObjectPtr<UPWLaneLifeSubsystem> Weak(this);
    ClockReadout = SNew(SPWClockReadout)
        .TimeText(TAttribute<FText>::CreateLambda([Weak]()
        {
            const UPWLaneLifeSubsystem* Life = Weak.Get();
            return Life != nullptr && Life->Sim.IsValid() ? Life->GetClockText() : FText::GetEmpty();
        }))
        .ShowReadout(TAttribute<bool>::CreateLambda([Weak]()
        {
            // Hidden under the title, creation and pause menus; shown in the world, over a conversation and over a reading.
            const UPWLaneLifeSubsystem* Life = Weak.Get();
            return Life != nullptr && Life->ClockHold != EPWClockHold::Menu;
        }));
    ClockHolder = SNew(SWeakWidget).PossiblyNullContent(ClockReadout);
    ClockHolder->SetVisibility(EVisibility::HitTestInvisible);
    GEngine->GameViewport->AddViewportWidgetContent(ClockHolder.ToSharedRef(), BrClockZOrder);
}

void UPWLaneLifeSubsystem::RemoveClockReadout()
{
    if (GEngine != nullptr && GEngine->GameViewport != nullptr && ClockHolder.IsValid())
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(ClockHolder.ToSharedRef());
    }
    ClockHolder.Reset();
    ClockReadout.Reset();
}
