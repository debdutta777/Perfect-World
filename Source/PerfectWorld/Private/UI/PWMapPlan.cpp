#include "UI/PWMapPlan.h"

namespace
{
namespace MpLm = PerfectWorld::LaneMap;

/** A rectangle polygon (counter-clockwise, four points) as the centre, size and angle of its image on the widget. */
bool MpProjectRectangle(const MpLm::View& View, const MpLm::Size& Widget, const MpLm::Polygon& Outline, FVector2f& OutCentre, FVector2f& OutSize, float& OutAngle)
{
    if (Outline.size() != 4)
    {
        return false;
    }
    const MpLm::Point P0 = MpLm::WorldToWidget(View, Widget, Outline[0]);
    const MpLm::Point P1 = MpLm::WorldToWidget(View, Widget, Outline[1]);
    const MpLm::Point P2 = MpLm::WorldToWidget(View, Widget, Outline[2]);
    const MpLm::Point P3 = MpLm::WorldToWidget(View, Widget, Outline[3]);
    const double E1X = P1.X - P0.X;
    const double E1Y = P1.Y - P0.Y;
    const double E2X = P3.X - P0.X;
    const double E2Y = P3.Y - P0.Y;
    OutCentre = FVector2f(static_cast<float>((P0.X + P2.X) * 0.5), static_cast<float>((P0.Y + P2.Y) * 0.5));
    OutSize = FVector2f(static_cast<float>(FMath::Sqrt(E1X * E1X + E1Y * E1Y)), static_cast<float>(FMath::Sqrt(E2X * E2X + E2Y * E2Y)));
    OutAngle = static_cast<float>(FMath::Atan2(E1Y, E1X));
    return true;
}
} // namespace

namespace PWMap
{
EQuadRole QuadRoleOf(PerfectWorld::LaneMap::SurfaceKind Kind)
{
    switch (Kind)
    {
    case MpLm::SurfaceKind::Road: return EQuadRole::Road;
    case MpLm::SurfaceKind::Alley: return EQuadRole::Alley;
    case MpLm::SurfaceKind::Footpath: return EQuadRole::Footpath;
    }
    return EQuadRole::Road;
}

EQuadRole QuadRoleOf(PerfectWorld::LaneMap::BuildingKind Kind)
{
    switch (Kind)
    {
    case MpLm::BuildingKind::Home: return EQuadRole::Home;
    case MpLm::BuildingKind::Shop: return EQuadRole::Shop;
    case MpLm::BuildingKind::School: return EQuadRole::School;
    case MpLm::BuildingKind::Workshop: return EQuadRole::Workshop;
    case MpLm::BuildingKind::Sealed: return EQuadRole::Sealed;
    case MpLm::BuildingKind::Civic: return EQuadRole::School; // institutional colour; no new palette entry (R6-05)
    }
    return EQuadRole::Sealed;
}

EDotRole DotRoleOf(PerfectWorld::LaneMap::DoorAvailability Availability)
{
    switch (Availability)
    {
    case MpLm::DoorAvailability::OpenToAll: return EDotRole::DoorOpen;
    case MpLm::DoorAvailability::ClosedForNow: return EDotRole::DoorClosed;
    case MpLm::DoorAvailability::PrivateKnock: return EDotRole::DoorPrivate;
    case MpLm::DoorAvailability::Locked: return EDotRole::DoorLocked;
    }
    return EDotRole::DoorLocked;
}

FMapPlan BuildPlan(const FPWMapScene& Scene, const FPWMapSnapshot& Snapshot, const MpLm::View& View, const FVector2f& WidgetSize, const MpLm::Rect& Clip,
                   const FPlanOptions& Options)
{
    FMapPlan Plan;
    const MpLm::Size Widget{WidgetSize.X, WidgetSize.Y};

    // Surfaces first, then buildings, so a footpath never paints over a wall.
    const auto AddQuad = [&](const MpLm::Polygon& Outline, EQuadRole Role, int32 SourceIndex, bool bBuilding)
    {
        if (!MpLm::PolygonVisible(View, Widget, Outline, Clip))
        {
            ++Plan.CulledQuads;
            return;
        }
        FPlanQuad Quad;
        if (!MpProjectRectangle(View, Widget, Outline, Quad.Centre, Quad.Size, Quad.AngleRadians))
        {
            return;
        }
        Quad.Role = Role;
        Quad.SourceIndex = SourceIndex;
        Quad.bBuilding = bBuilding;
        Plan.Quads.Add(Quad);
    };
    for (std::size_t Index = 0; Index < Scene.Map.Surfaces.size(); ++Index)
    {
        AddQuad(Scene.Map.Surfaces[Index].Outline, QuadRoleOf(Scene.Map.Surfaces[Index].Kind), static_cast<int32>(Index), false);
    }
    for (std::size_t Index = 0; Index < Scene.Map.Buildings.size(); ++Index)
    {
        AddQuad(Scene.Map.Buildings[Index].Footprint, QuadRoleOf(Scene.Map.Buildings[Index].Kind), static_cast<int32>(Index), true);
    }

    const auto AddDot = [&](EDotRole Role, const MpLm::Point& World, int32 SourceIndex)
    {
        if (!MpLm::PointVisible(View, Widget, World, Clip, Options.DotInsetPx))
        {
            return;
        }
        const MpLm::Point Pixel = MpLm::WorldToWidget(View, Widget, World);
        FPlanDot Dot;
        Dot.Role = Role;
        Dot.Position = FVector2f(static_cast<float>(Pixel.X), static_cast<float>(Pixel.Y));
        Dot.SourceIndex = SourceIndex;
        Plan.Dots.Add(Dot);
    };
    if (Options.bDoors)
    {
        for (std::size_t Index = 0; Index < Scene.Map.Doors.size(); ++Index)
        {
            const MpLm::DoorAvailability Availability = Snapshot.Doors.IsValidIndex(static_cast<int32>(Index))
                                                          ? Snapshot.Doors[static_cast<int32>(Index)].Availability
                                                          : MpLm::DoorAvailability::Locked;
            AddDot(DotRoleOf(Availability), Scene.Map.Doors[Index].Position, static_cast<int32>(Index));
        }
    }
    if (Options.bPeople)
    {
        for (int32 Index = 0; Index < Snapshot.People.Num(); ++Index)
        {
            AddDot(EDotRole::Person, Snapshot.People[Index].WorldCm, Index);
        }
    }

    if (Snapshot.bHasPlayer && MpLm::PointVisible(View, Widget, Snapshot.PlayerCm, Clip, 0.0))
    {
        const MpLm::Point Pixel = MpLm::WorldToWidget(View, Widget, Snapshot.PlayerCm);
        Plan.Player.bVisible = true;
        Plan.Player.Position = FVector2f(static_cast<float>(Pixel.X), static_cast<float>(Pixel.Y));
        Plan.Player.AngleDegrees = static_cast<float>(MpLm::ScreenAngleOfYaw(View, Snapshot.PlayerYawDegrees));
    }
    return Plan;
}

MpLm::View MakeMinimapView(const FPWMapSnapshot& Snapshot, const FPWMapPreferences& Preferences, float SidePx, bool bReducedMotion)
{
    const MpLm::Point Player = Snapshot.bHasPlayer ? Snapshot.PlayerCm : MpLm::Point{0.0, 0.0};
    return MpLm::MinimapView(Player, Snapshot.PlayerYawDegrees, SidePx, Preferences.Orientation(bReducedMotion));
}
} // namespace PWMap
