#include "PerfectWorldLaneLifeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldLaneTeaShop.h"
#include "PerfectWorldNpcCharacter.h"

// Farida's half of the lane-life bridge: her schedule, the anchors and journeys it binds in the built shop, and her intent at the
// world clock's instant. The core resolves the intent; the movement component only presents it.

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
    FaridaResolvedInstant = -1;
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
    // Keyed on the full instant, not the minute of day: a jump of exactly one day lands on the same minute.
    if (!FaridaSchedule || FaridaResolvedInstant == GetNow()) { return; }
    FaridaResolvedInstant = GetNow();
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
    // The schedule is keyed on the world clock's own instant (epoch Monday 06:00), so the weekday and any date exception hold.
    const PerfectWorld::NpcIntentResult Result = PerfectWorld::ResolveNpcIntentAt(
        FaridaSchedule->Person, FaridaSchedule->Travel, GetNow(), false, bFaridaBlocked);
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
