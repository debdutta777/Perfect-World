#include "PerfectWorldLaneNpcBindings.h"

#include "PerfectWorldLaneCareInterior.h"
#include "PerfectWorldNpcMovementComponent.h"

namespace
{
FName NpBindingName(const std::string_view Id)
{
    return FName(UTF8_TO_TCHAR(std::string(Id).c_str()));
}

FNpResolvedBinding NpMissingBinding(const FName Id)
{
    FNpResolvedBinding Result;
    Result.Diagnostic = FString::Printf(TEXT("missing binding '%s'"), *Id.ToString());
    return Result;
}
}

bool FNpLaneNpcBindings::RegisterProvider(const FName ProviderId, FNpBindingProvider Provider)
{
    if (ProviderId.IsNone()) { return false; }
    // Refuse ambiguous ownership; lookup order must never decide a person's position.
    for (const auto& Existing : Providers)
    {
        if (Existing.Key == ProviderId) { continue; }
        for (const auto& Anchor : Provider.Anchors)
        {
            if (Existing.Value.Anchors.Contains(Anchor.Key)) { return false; }
        }
        for (const auto& Route : Provider.Routes)
        {
            if (Existing.Value.Routes.Contains(Route.Key)) { return false; }
        }
        for (const FNpDoorwayZone& Door : Provider.Doorways)
        {
            for (const FNpDoorwayZone& Other : Existing.Value.Doorways)
            {
                if (Door.Id == Other.Id) { return false; }
            }
        }
    }
    Providers.Add(ProviderId, MoveTemp(Provider));
    return true;
}

const FNpAnchorBinding* FNpLaneNpcBindings::FindAnchor(const FName Id) const
{
    for (const auto& Provider : Providers)
    {
        if (const FNpAnchorBinding* Found = Provider.Value.Anchors.Find(Id)) { return Found; }
    }
    return nullptr;
}

const FNpRouteBinding* FNpLaneNpcBindings::FindRoute(const FName Id) const
{
    for (const auto& Provider : Providers)
    {
        if (const FNpRouteBinding* Found = Provider.Value.Routes.Find(Id)) { return Found; }
    }
    return nullptr;
}

TArray<FNpDoorwayZone> FNpLaneNpcBindings::Doorways() const
{
    TArray<FNpDoorwayZone> Result;
    for (const auto& Provider : Providers) { Result.Append(Provider.Value.Doorways); }
    Result.Sort([](const FNpDoorwayZone& A, const FNpDoorwayZone& B) { return A.Id.LexicalLess(B.Id); });
    return Result;
}

FNpResolvedBinding FNpLaneNpcBindings::Resolve(const PerfectWorld::NpcSchedule& Schedule,
                                             const PerfectWorld::NpcIntent& Intent) const
{
    const FName AnchorId = NpBindingName(Intent.AnchorId);
    const FNpAnchorBinding* Anchor = FindAnchor(AnchorId);
    if (Anchor == nullptr) { return NpMissingBinding(AnchorId); }
    FNpResolvedBinding Result;
    Result.Kind = Anchor->Kind;
    Result.Feet = Anchor->Feet;
    Result.ExitAnchor = Anchor->ExitAnchor;
    Result.bPreferSit = Anchor->bPreferSit;
    const auto Under = Intent.State == PerfectWorld::NpcState::Chat || Intent.State == PerfectWorld::NpcState::Yield
        ? Intent.ResumeState : Intent.State;
    if (Under != PerfectWorld::NpcState::Travel) { return Result; }

    // Journey IDs are the core's directed endpoints. Segment definitions remain generic content.
    const PerfectWorld::FaridaTravelPath* Path = nullptr;
    bool bReverse = false;
    for (const auto& Candidate : Schedule.Paths)
    {
        if (Intent.RouteId == "route:" + Candidate.From + "->" + Candidate.To) { Path = &Candidate; break; }
        if (Intent.RouteId == "route:" + Candidate.To + "->" + Candidate.From) { Path = &Candidate; bReverse = true; break; }
    }
    if (Path == nullptr) { return NpMissingBinding(NpBindingName(Intent.RouteId)); }
    int64 Total = 0;
    int64 LocalStart = -1;
    int64 LocalEnd = 0;
    std::string Current = bReverse ? Path->To : Path->From;
    bool bLocalEnded = false;
    for (size_t Index = 0; Index < Path->Routes.size(); ++Index)
    {
        const std::string& Id = Path->Routes[bReverse ? Path->Routes.size() - 1 - Index : Index];
        const PerfectWorld::FaridaRoute* Def = nullptr;
        for (const auto& Candidate : Schedule.Routes) { if (Candidate.Id == Id) { Def = &Candidate; break; } }
        const FNpRouteBinding* Segment = FindRoute(NpBindingName(Id));
        if (Def == nullptr || Segment == nullptr) { return NpMissingBinding(NpBindingName(Id)); }
        const bool bForward = Current == Def->From;
        if (!bForward && Current != Def->To) { return NpMissingBinding(NpBindingName(Id)); }
        Current = bForward ? Def->To : Def->From;
        if (!Segment->bOffLane)
        {
            if (bLocalEnded || Segment->Points.Num() < 2) { return NpMissingBinding(NpBindingName(Id)); }
            if (LocalStart < 0) { LocalStart = Total; }
            const FVector Start = Segment->Points[bForward ? 0 : Segment->Points.Num() - 1];
            if (!Result.RoutePoints.IsEmpty() && !Result.RoutePoints.Last().Equals(Start, 1.0))
            {
                auto Missing = NpMissingBinding(NpBindingName(Id));
                Missing.Diagnostic += TEXT(" (disconnected route segments)");
                return Missing;
            }
            for (int32 PointIndex = 0; PointIndex < Segment->Points.Num(); ++PointIndex)
            {
                const FVector Point = Segment->Points[bForward ? PointIndex : Segment->Points.Num() - 1 - PointIndex];
                if (Result.RoutePoints.IsEmpty() || !Result.RoutePoints.Last().Equals(Point, 1.0)) { Result.RoutePoints.Add(Point); }
            }
            LocalEnd = Total + Def->LengthCm;
        }
        else if (LocalStart >= 0) { bLocalEnded = true; }
        Total += Def->LengthCm;
    }
    // Local progress is weighted by authored travel lengths, so off-lane time is never compressed into the visible walk.
    const double Distance = Total * FMath::Clamp(Intent.ProgressPermille, 0, 1000) / 1000.0;
    if (LocalStart < 0 || Distance < LocalStart || Distance > LocalEnd)
    {
        Result.Kind = ENpBindingKind::OffLane;
        return Result;
    }
    Result.Kind = ENpBindingKind::Local;
    Result.LocalProgress = FMath::Clamp(FMath::RoundToInt(1000.0 * (Distance - LocalStart) / FMath::Max<int64>(1, LocalEnd - LocalStart)), 0, 1000);
    FPWNpcPolyline Line;
    Line.Set(Result.RoutePoints);
    if (!Line.IsValid()) { return NpMissingBinding(NpBindingName(Intent.RouteId)); }
    const double Along = Line.Length() * Result.LocalProgress / 1000.0;
    Result.Feet = FTransform(Line.DirectionAt(Along).Rotation(), Line.PointAt(Along));
    return Result;
}

FNpBindingProvider NpCareHomeBindings(const PWLane::FLaneFrame& Frame)
{
    FNpBindingProvider Result;
    const auto NpPoint = [&Frame](const double U, const double V) { return PWLane::CareWorldPoint(Frame, U, V); };
    const auto NpAdd = [&Result](const TCHAR* Id, const FVector& Point, const double Yaw)
    {
        FNpAnchorBinding Anchor;
        Anchor.Feet = FTransform(FRotator(0.0, Yaw, 0.0), Point);
        Result.Anchors.Add(FName(Id), Anchor);
    };
    // Stand beside the authored chair rather than put a standing capsule inside its solid seat/back.
    NpAdd(TEXT("anchor.care_home.staff_desk"), NpPoint(180.0, 314.0), (NpPoint(114.0, 314.0) - NpPoint(180.0, 314.0)).Rotation().Yaw);
    NpAdd(TEXT("anchor.care_home.common_room"), PWLane::CareStaffSpot(Frame), PWLane::CareStaffSpotYaw(Frame));
    NpAdd(TEXT("anchor.care_home.front_door"), NpPoint(235.0, -45.0), (NpPoint(235.0, 90.0) - NpPoint(235.0, -45.0)).Rotation().Yaw);
    NpAdd(TEXT("location.nabapara.care_home_footpath"), NpPoint(235.0, -85.0), 0.0);
    FNpAnchorBinding Home;
    Home.Kind = ENpBindingKind::OffLane;
    Home.ExitAnchor = FName(TEXT("location.nabapara.care_home_footpath"));
    Result.Anchors.Add(FName(TEXT("location.nabapara.care_worker_home")), Home);
    Result.Routes.Add(FName(TEXT("route.care_home.desk_to_common")),
        FNpRouteBinding{{NpPoint(180.0, 314.0), NpPoint(180.0, 425.0), NpPoint(506.0, 425.0), NpPoint(560.0, 425.0), PWLane::CareStaffSpot(Frame)}, false});
    Result.Routes.Add(FName(TEXT("route.care_home.desk_to_door")),
        FNpRouteBinding{{NpPoint(180.0, 314.0), NpPoint(235.0, 314.0), NpPoint(235.0, -45.0)}, false});
    Result.Routes.Add(FName(TEXT("route.care_home.door_to_footpath")),
        FNpRouteBinding{{NpPoint(235.0, -45.0), NpPoint(235.0, -85.0)}, false});
    Result.Routes.Add(FName(TEXT("route.care_worker_home.footpath_to_home")), FNpRouteBinding{{}, true});
    FNpDoorwayZone Front;
    Front.Id = FName(PWLane::CareDoorFrontId);
    // FBox's two-corner constructor does not reorder mirrored axes.
    Front.Bounds = FBox(ForceInit);
    Front.Bounds += NpPoint(180.0, -65.0);
    Front.Bounds += NpPoint(290.0, 140.0) + FVector(0.0, 0.0, 212.0);
    Front.WaitingFeet = {NpPoint(180.0, 220.0), NpPoint(375.0, -85.0)};
    Result.Doorways.Add(Front);
    FNpDoorwayZone Common;
    Common.Id = FName(PWLane::CareDoorCommonId);
    Common.Bounds += NpPoint(400.0, 388.0);
    Common.Bounds += NpPoint(610.0, 482.0) + FVector(0.0, 0.0, 212.0);
    Common.WaitingFeet = {NpPoint(650.0, 330.0), NpPoint(320.0, 390.0)};
    Result.Doorways.Add(Common);
    return Result;
}
