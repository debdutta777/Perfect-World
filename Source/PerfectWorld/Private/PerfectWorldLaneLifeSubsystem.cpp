#include "PerfectWorldLaneLifeSubsystem.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeConversation.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWLaneLife, Log, All);

#define LOCTEXT_NAMESPACE "PerfectWorldLaneLife"

namespace
{
/** The place the player lives. TODO(care-origin): the resident places come from the chosen creation origin. */
constexpr std::string_view OpeningHomeLocationId = "location.nabapara.opening_home";

bool IsResidentOf(const PerfectWorld::DoorDef& Door)
{
    return Door.LocationId == OpeningHomeLocationId;
}
} // namespace

namespace PWLaneLife
{
FString ToFString(const std::string_view Text)
{
    return FString(UTF8_TO_TCHAR(std::string(Text).c_str()));
}

FText ToText(const std::string_view Text)
{
    return FText::FromString(ToFString(Text));
}

FName ToName(const std::string_view Text)
{
    return FName(*ToFString(Text));
}

std::string ToStd(const FName& Name)
{
    return std::string(TCHAR_TO_UTF8(*Name.ToString()));
}
} // namespace PWLaneLife

UPWLaneLifeSubsystem* UPWLaneLifeSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    return World != nullptr ? World->GetSubsystem<UPWLaneLifeSubsystem>() : nullptr;
}

bool UPWLaneLifeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Only worlds being played; the editor's own worlds have no doors to keep or people to talk to.
    const UWorld* World = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && World != nullptr && World->IsGameWorld();
}

void UPWLaneLifeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    Doors = PerfectWorld::MakeInitialDoorState();
    Conversation = PerfectWorld::ConversationState();
    PendingMinutes = 0;
    ReportedUnknownDoors.Reset();
    if (!PerfectWorld::IsDoorStateValid(Doors))
    {
        UE_LOG(LogPWLaneLife, Error, TEXT("The core's initial door state is not valid; the doors cannot be driven."));
    }

    // The conversation panel talks to people through whatever session factory it holds: install ours, over the core.
    if (UPWConversationPresenter* Presenter = Collection.InitializeDependency<UPWConversationPresenter>())
    {
        const TWeakObjectPtr<UPWLaneLifeSubsystem> Weak(this);
        Presenter->SetSessionFactory([Weak](const FName PersonId, const FText& DisplayName) -> TSharedPtr<IPWConversationSession>
        {
            UPWLaneLifeSubsystem* Life = Weak.Get();
            if (Life == nullptr)
            {
                return nullptr;
            }
            return Life->MakeConversationSession(PersonId, DisplayName);
        });
    }
    else
    {
        UE_LOG(LogPWLaneLife, Error, TEXT("No conversation presenter in this world; people cannot be talked to."));
    }
}

void UPWLaneLifeSubsystem::Deinitialize()
{
    if (UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this))
    {
        Presenter->SetSessionFactory(FPWConversationSessionFactory());
    }
    Super::Deinitialize();
}

int32 UPWLaneLifeSubsystem::GetMinuteOfDay() const
{
    // The creator subsystem keeps no world clock and no save carries one yet, so the day always opens at 07:30.
    // TODO(clock-M1-16): read the in-world clock (PerfectWorld::DoorMinuteOfDay of its instant) when it exists.
    return PWLaneLife::FixedStartMinuteOfDay;
}

void UPWLaneLifeSubsystem::RegisterDoor(APWDoorActor& Door)
{
    const std::string Id = PWLaneLife::ToStd(Door.GetDoorId());
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(Id);
    if (Def == nullptr)
    {
        if (!ReportedUnknownDoors.Contains(Door.GetDoorId()))
        {
            ReportedUnknownDoors.Add(Door.GetDoorId());
            UE_LOG(LogPWLaneLife, Error, TEXT("Door %s is not in the core's AuthoredDoors(); it cannot be opened. Add it to Doors.cpp or fix the id."),
                   *Door.GetDoorId().ToString());
        }
        return;
    }
    Door.OnSettled.RemoveAll(this);
    Door.OnSettled.AddUObject(this, &UPWLaneLifeSubsystem::HandleDoorSettled);
    // TODO(world-save-v8): a loaded world's DoorState (and swing side) arrives here instead of the authored start.
    int8 Side = 0;
    for (const PerfectWorld::DoorEntry& Entry : Doors.Doors)
    {
        if (Entry.Id == Id)
        {
            Side = static_cast<int8>(Entry.SwingSide);
            break;
        }
    }
    Door.SetOpenImmediate(PerfectWorld::QueryDoorOpen(Doors, Id).value_or(Def->bInitiallyOpen), Side);
}

void UPWLaneLifeSubsystem::HandleDoorSettled(const FName DoorId, const bool bReachedTarget, const bool bIsOpenNow, const int8 SwingSide)
{
    // Record where the leaf really is, including which way a two-way door swung. This is a physical fact, not a
    // permission question, so the core applies it without access rules (an interrupted swing overrides the provisional
    // commit made in RequestDoorToggle; a completed one records its side).
    const PerfectWorld::DoorResult Result =
        PerfectWorld::ApplyDoorSettlement(Doors, PWLaneLife::ToStd(DoorId), bIsOpenNow, static_cast<std::int8_t>(SwingSide));
    if (Result != PerfectWorld::DoorResult::Ok && Result != PerfectWorld::DoorResult::NoChange)
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Door %s settled (open %d, side %d, reached %d) but the core refused it: %hs"),
               *DoorId.ToString(), bIsOpenNow ? 1 : 0, static_cast<int32>(SwingSide), bReachedTarget ? 1 : 0,
               PerfectWorld::DoorResultCode(Result).data());
    }
}

bool UPWLaneLifeSubsystem::IsDoorOpen(const FName DoorId, const bool bFallback) const
{
    return PerfectWorld::QueryDoorOpen(Doors, PWLaneLife::ToStd(DoorId)).value_or(bFallback);
}

void UPWLaneLifeSubsystem::RequestDoorToggle(APWDoorActor& Door, APawn* By)
{
    const std::string Id = PWLaneLife::ToStd(Door.GetDoorId());
    const PerfectWorld::DoorDef* Def = PerfectWorld::FindDoor(Id);
    const std::optional<bool> Current = Def != nullptr ? PerfectWorld::QueryDoorOpen(Doors, Id) : std::nullopt;
    if (!Current.has_value())
    {
        RegisterDoor(Door); // reports the unknown id once
        ShowDoorRefusal(By, PerfectWorld::DoorResult::UnknownDoor, nullptr);
        return;
    }

    // Ask the core on a copy: the real state changes only once the leaf has really started to swing.
    const bool bWantOpen = !*Current;
    PerfectWorld::DoorState Trial = Doors;
    const PerfectWorld::DoorResult Result = PerfectWorld::TrySetDoorOpen(Trial, Id, bWantOpen, GetMinuteOfDay(), IsResidentOf(*Def));
    if (Result != PerfectWorld::DoorResult::Ok)
    {
        ShowDoorRefusal(By, Result, Def);
        return;
    }
    if (!Door.TryMoveTo(bWantOpen, By))
    {
        // Allowed by the core, but a person or a piece of furniture is in the leaf's way: nothing changed anywhere.
        if (UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr)
        {
            Interaction->ShowNotice(LOCTEXT("InTheWay", "Something is in the way."));
        }
        return;
    }
    Doors = MoveTemp(Trial); // provisional: HandleDoorSettled corrects it if the swing is interrupted
}

void UPWLaneLifeSubsystem::ShowDoorRefusal(const APawn* By, const PerfectWorld::DoorResult Result, const PerfectWorld::DoorDef* Def) const
{
    UPWInteractionComponent* Interaction = By != nullptr ? By->FindComponentByClass<UPWInteractionComponent>() : nullptr;
    if (Interaction == nullptr)
    {
        return;
    }
    // The core gives the reason as a key (DoorResultReasonKey); the wording lives here.
    const std::string_view Key = PerfectWorld::DoorResultReasonKey(Result);
    FText Message;
    if (Key == "door.reason.private")
    {
        Message = LOCTEXT("ReasonPrivate", "It's someone's home. Knock first.");
    }
    else if (Key == "door.reason.locked")
    {
        Message = LOCTEXT("ReasonLocked", "It's locked.");
    }
    else if (Key == "door.reason.closed_hours")
    {
        const int32 Opens = Def != nullptr ? PerfectWorld::DoorOpensAtMinute(*Def) : -1;
        Message = Opens >= 0 ? FText::Format(LOCTEXT("ReasonClosedUntil", "Closed until {0}."),
                                             FText::FromString(FString::Printf(TEXT("%02d:%02d"), Opens / 60, Opens % 60)))
                             : LOCTEXT("ReasonClosed", "It's closed.");
    }
    else
    {
        Message = LOCTEXT("ReasonOther", "It won't open.");
    }
    Interaction->ShowNotice(Message);
}

TSharedPtr<IPWConversationSession> UPWLaneLifeSubsystem::MakeConversationSession(const FName PersonId, const FText& DisplayName)
{
    if (Conversation.Open)
    {
        // A session abandoned without a Leave (the world was torn down mid-talk): close it so a new one can start.
        (void)PerfectWorld::LeaveConversation(Conversation);
    }
    const PerfectWorld::ConversationError Error =
        PerfectWorld::OpenConversation(Conversation, PWLaneLife::ToStd(PersonId), GetMinuteOfDay(), PerfectWorld::PersonAvailability::Awake);
    if (Error != PerfectWorld::ConversationError::None)
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Cannot talk to %s: the core refused (%s)."), *PersonId.ToString(),
               *PWLaneLife::ToFString(PerfectWorld::ConversationErrorCode(Error)));
        return nullptr;
    }
    // The household adult is known to the core by a role label; any other person keeps the name the actor carries.
    const bool bHouseholdAdult = PersonId == PWLaneLife::ToName(PerfectWorld::HouseholdAdultId);
    const FText Speaker = bHouseholdAdult ? PWLaneLife::ToText(PerfectWorld::HouseholdAdultLabel) : DisplayName;
    return MakeShared<FPWCoreConversationSession>(TWeakObjectPtr<UPWLaneLifeSubsystem>(this), Speaker);
}

#undef LOCTEXT_NAMESPACE
