#include "PerfectWorldLaneLifeSubsystem.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFurnitureActor.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeConversation.h"
#include "PerfectWorldLaneTeaShop.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldReadingPresenter.h"

DEFINE_LOG_CATEGORY_STATIC(LogPWLaneLife, Log, All);

#define LOCTEXT_NAMESPACE "PerfectWorldLaneLife"

namespace
{
/** The place the player lives. TODO(care-origin): the resident places come from the chosen creation origin. */
constexpr std::string_view FaOpeningHomeLocationId = "location.nabapara.opening_home";

bool FaIsResidentOf(const PerfectWorld::DoorDef& Door)
{
    return Door.LocationId == FaOpeningHomeLocationId;
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
    Conversations.Reset();
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

    // Readable objects raise one process-wide event; this world's reading presenter shows the text. Reading is free:
    // it costs no world time (LANE_LIFE_PLAN §B). TODO(clock-M1-16): hold the free-roam clock while a reading is open.
    Collection.InitializeDependency<UPWReadingPresenter>();
    APWReadableActor::GlobalReadRequested().RemoveAll(this);
    APWReadableActor::GlobalReadRequested().AddUObject(this, &UPWLaneLifeSubsystem::HandleReadRequested);
}

void UPWLaneLifeSubsystem::Deinitialize()
{
    if (APWNpcCharacter* Person = Farida.Get())
    {
        Person->GetNpcMovement()->OnBlockChanged.RemoveAll(this);
        Person->GetNpcMovement()->SetAnchorResolver({});
        Person->GetNpcMovement()->SetRouteResolver({});
    }
    Farida.Reset();
    APWReadableActor::GlobalReadRequested().RemoveAll(this);
    if (UPWConversationPresenter* Presenter = UPWConversationPresenter::Get(this))
    {
        Presenter->SetSessionFactory(FPWConversationSessionFactory());
    }
    Super::Deinitialize();
}

void UPWLaneLifeSubsystem::HandleReadRequested(const FName ReadableId, const FName ContentId, const FText Title)
{
    // The event is process-wide: only the presenter of the world that owns the readable answers.
    UPWReadingPresenter* Reading = UPWReadingPresenter::Get(this);
    if (Reading == nullptr || !Reading->OpenBuiltIn(ContentId))
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Readable %s could not open %s (\"%s\")."), *ReadableId.ToString(), *ContentId.ToString(),
               *Title.ToString());
    }
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
    const PerfectWorld::DoorResult Result = PerfectWorld::TrySetDoorOpen(Trial, Id, bWantOpen, GetMinuteOfDay(), FaIsResidentOf(*Def));
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
    PerfectWorld::PersonAvailability Availability = PerfectWorld::PersonAvailability::Awake;
    if (PersonId == PWLaneLife::ToName(PerfectWorld::FaridaPersonId))
    {
        RefreshFaridaForClock();
        if (!FaridaIntent)
        {
            return nullptr;
        }
        Availability = FaridaIntent->Availability;
    }
    PerfectWorld::ConversationState& Conversation = MutableConversation(PersonId);
    if (Conversation.Open)
    {
        // A session abandoned without a Leave (the world was torn down mid-talk): close it so a new one can start.
        (void)PerfectWorld::LeaveConversation(Conversation);
    }
    const PerfectWorld::ConversationError Error =
        PerfectWorld::OpenConversation(FaCatalogueForPerson(PersonId), Conversation, PWLaneLife::ToStd(PersonId), GetMinuteOfDay(), Availability);
    if (Error != PerfectWorld::ConversationError::None)
    {
        UE_LOG(LogPWLaneLife, Warning, TEXT("Cannot talk to %s: the core refused (%s)."), *PersonId.ToString(),
               *PWLaneLife::ToFString(PerfectWorld::ConversationErrorCode(Error)));
        return nullptr;
    }
    // The household adult is known to the core by a role label; any other person keeps the name the actor carries.
    const bool bHouseholdAdult = PersonId == PWLaneLife::ToName(PerfectWorld::HouseholdAdultId);
    const FText Speaker = bHouseholdAdult ? PWLaneLife::ToText(PerfectWorld::HouseholdAdultLabel) : DisplayName;
    return MakeShared<FPWCoreConversationSession>(TWeakObjectPtr<UPWLaneLifeSubsystem>(this), PersonId, Speaker);
}

PerfectWorld::ConversationState& UPWLaneLifeSubsystem::MutableConversation(const FName PersonId)
{
    return Conversations.FindOrAdd(PersonId);
}

const PerfectWorld::ConversationState& UPWLaneLifeSubsystem::GetConversation(const FName PersonId) const
{
    const PerfectWorld::ConversationState* State = Conversations.Find(PersonId);
    static const PerfectWorld::ConversationState FaEmptyConversation;
    return State != nullptr ? *State : FaEmptyConversation;
}

bool UPWLaneLifeSubsystem::PrepareFarida(const PWLane::FLaneBuilding& Building, FTransform& OutFeet)
{
    if (!FaridaSchedule)
    {
        UGameInstance* Instance = GetWorld()->GetGameInstance();
        const UPWContentSubsystem* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
        if (Content == nullptr || Content->GetDb() == nullptr) { return false; }
        PerfectWorld::FaridaScheduleResult Loaded = PerfectWorld::LoadFaridaSchedule(*Content->GetDb());
        if (!Loaded.Ok())
        {
            UE_LOG(LogPWLaneLife, Error, TEXT("Farida schedule: %s"), UTF8_TO_TCHAR(Loaded.Error.c_str()));
            return false;
        }
        FaridaSchedule = MoveTemp(Loaded.Value);
    }
    FaridaAnchors.Reset();
    FaridaRoutes.Reset();
    TArray<PWLane::FTeaShopAnchor> Anchors;
    PWLane::GetTeaShopAnchors(Building, Anchors);
    for (const PWLane::FTeaShopAnchor& Anchor : Anchors)
    {
        FaridaAnchors.Add(Anchor.Id, FTransform(FRotator(0.0, Anchor.WorldYawDegrees, 0.0), Anchor.World));
    }
    TArray<PWLane::FTeaShopRoute> Segments;
    PWLane::GetTeaShopRoutes(Building, Segments);
    TMap<FName, TArray<FVector>> Lines;
    for (const PWLane::FTeaShopRoute& Segment : Segments) { Lines.Add(Segment.Id, Segment.World); }
    const TArray<FVector>* Footpath = Lines.Find(FName(TEXT("route.shanti_chai.door_to_footpath")));
    if (Footpath != nullptr && !Footpath->IsEmpty())
    {
        FaridaAnchors.Add(FName(TEXT("location.nabapara.shanti_chai_footpath")), FTransform(Footpath->Last()));
    }
    // Compose the core's endpoint journeys, orienting each authored segment by its declared From/To.
    // Add the door connector missing from counter_to_door; trim door_to_footpath's indoor approach at the door anchor.
    for (const PerfectWorld::FaridaTravelPath& Path : FaridaSchedule->Paths)
    {
        std::string Current = Path.From;
        TArray<FVector> Points;
        bool bBound = true;
        for (const std::string& RouteId : Path.Routes)
        {
            const PerfectWorld::FaridaRoute* Def = nullptr;
            for (const PerfectWorld::FaridaRoute& Candidate : FaridaSchedule->Routes)
            {
                if (Candidate.Id == RouteId) { Def = &Candidate; break; }
            }
            const TArray<FVector>* Line = Lines.Find(PWLaneLife::ToName(RouteId));
            if (Def == nullptr || Line == nullptr || Line->Num() < 2) { bBound = false; break; }
            TArray<FVector> Oriented = *Line;
            const FTransform* Start = FaridaAnchors.Find(PWLaneLife::ToName(Def->From));
            const FTransform* End = FaridaAnchors.Find(PWLaneLife::ToName(Def->To));
            if (Start == nullptr || End == nullptr) { bBound = false; break; }
            int32 StartIndex = INDEX_NONE;
            for (int32 Index = 0; Index < Oriented.Num(); ++Index)
            {
                if (Oriented[Index].Equals(Start->GetLocation(), 1.0)) { StartIndex = Index; break; }
            }
            if (StartIndex > 0) { Oriented.RemoveAt(0, StartIndex); }
            else if (StartIndex == INDEX_NONE) { Oriented.Insert(Start->GetLocation(), 0); }
            if (!Oriented.Last().Equals(End->GetLocation(), 1.0)) { Oriented.Add(End->GetLocation()); }
            const bool bReverse = Current == Def->To;
            if (!bReverse && Current != Def->From) { bBound = false; break; }
            for (int32 Index = 0; Index < Oriented.Num(); ++Index)
            {
                const FVector Point = Oriented[bReverse ? Oriented.Num() - 1 - Index : Index];
                if (Points.IsEmpty() || !Points.Last().Equals(Point, 1.0)) { Points.Add(Point); }
            }
            Current = bReverse ? Def->From : Def->To;
        }
        if (!bBound || Current != Path.To || Points.Num() < 2) { continue; }
        FaridaRoutes.Add(PWLaneLife::ToName("route:" + Path.From + "->" + Path.To), Points);
        TArray<FVector> Reverse;
        for (int32 Index = Points.Num() - 1; Index >= 0; --Index) { Reverse.Add(Points[Index]); }
        FaridaRoutes.Add(PWLaneLife::ToName("route:" + Path.To + "->" + Path.From), MoveTemp(Reverse));
    }
    UE_LOG(LogPWLaneLife, Display, TEXT("Farida: %d anchors, %d directed journeys bound; off-lane home geometry is pending."),
           FaridaAnchors.Num(), FaridaRoutes.Num());
    FaridaResolvedMinute = INDEX_NONE;
    RefreshFaridaForClock();
    if (!FaridaIntent) { return false; }
    if (FaridaIntent->State == PerfectWorld::NpcState::Travel)
    {
        const TArray<FVector>* Points = FaridaRoutes.Find(PWLaneLife::ToName(FaridaIntent->RouteId));
        if (Points == nullptr) { return false; }
        FPWNpcPolyline Line;
        Line.Set(*Points);
        const double Distance = Line.Length() * FaridaIntent->ProgressPermille / 1000.0;
        OutFeet = FTransform(Line.DirectionAt(Distance).Rotation(), Line.PointAt(Distance));
        return true;
    }
    const FTransform* Anchor = FaridaAnchors.Find(PWLaneLife::ToName(FaridaIntent->AnchorId));
    if (Anchor == nullptr) { return false; }
    OutFeet = *Anchor;
    return true;
}

void UPWLaneLifeSubsystem::RegisterFarida(APWNpcCharacter& Person)
{
    if (Person.GetPersonId() != PWLaneLife::ToName(PerfectWorld::FaridaPersonId) || !FaridaIntent) { return; }
    Farida = &Person;
    UPWNpcMovementComponent* Movement = Person.GetNpcMovement();
    const TWeakObjectPtr<UPWLaneLifeSubsystem> Weak(this);
    Movement->SetAnchorResolver([Weak](const FName Id, FTransform& Out)
    {
        const UPWLaneLifeSubsystem* Life = Weak.Get();
        const FTransform* Anchor = Life != nullptr ? Life->FaridaAnchors.Find(Id) : nullptr;
        if (Anchor == nullptr) { return false; }
        Out = *Anchor;
        return true;
    });
    Movement->SetRouteResolver([Weak](const FName Id, TArray<FVector>& Out)
    {
        const UPWLaneLifeSubsystem* Life = Weak.Get();
        const TArray<FVector>* Points = Life != nullptr ? Life->FaridaRoutes.Find(Id) : nullptr;
        if (Points == nullptr) { return false; }
        Out = *Points;
        return true;
    });
    Movement->OnBlockChanged.RemoveAll(this);
    Movement->OnBlockChanged.AddUObject(this, &UPWLaneLifeSubsystem::HandleFaridaBlock);
    ResolveFaridaIntent();
}

void UPWLaneLifeSubsystem::RefreshFaridaForClock()
{
    if (!FaridaSchedule || FaridaResolvedMinute == GetMinuteOfDay()) { return; }
    FaridaResolvedMinute = GetMinuteOfDay();
    ResolveFaridaIntent();
}

void UPWLaneLifeSubsystem::HandleFaridaBlock(const bool bBlocked, const FName RouteId)
{
    // TODO(clock-M1-16): persist a bounded delayed/replanned journey command; this flag only presents the wait.
    bFaridaBlocked = bBlocked;
    ResolveFaridaIntent();
}

void UPWLaneLifeSubsystem::ResolveFaridaIntent()
{
    if (!FaridaSchedule) { return; }
    // TODO(clock-M1-16): use the full authoritative GameInstant (epoch Monday 06:00), preserving weekday/load.
    const PerfectWorld::GameInstant At = GetMinuteOfDay() - PerfectWorld::CampaignStartHour * 60;
    const PerfectWorld::NpcIntentResult Result = PerfectWorld::ResolveNpcIntentAt(
        FaridaSchedule->Person, FaridaSchedule->Travel, At, false, bFaridaBlocked);
    if (!Result.Ok())
    {
        FaridaIntent.reset();
        UE_LOG(LogPWLaneLife, Error, TEXT("Farida intent refused: %s"),
               *PWLaneLife::ToFString(PerfectWorld::NpcIntentErrorCode(Result.Error)));
        return;
    }
    FaridaIntent = Result.Intent;
    if (APWNpcCharacter* Person = Farida.Get())
    {
        // The component owns the local Chat hold and clears it when the presenter closes, resuming this intent.
        Person->GetNpcMovement()->SetIntent(MakeNpcIntentView(*FaridaIntent,
            FaridaIntent->AnchorId == "anchor.shanti_chai.rest"));
    }
}

#undef LOCTEXT_NAMESPACE
