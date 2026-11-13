#include "PerfectWorldNeighbourhoodStreets.h"

#include <string_view>

namespace PWNs
{
namespace
{
using NsKind = PerfectWorld::NeighbourhoodFeatureKind;

bool NsStartsWith(const std::string& Text, std::string_view Prefix)
{
    return Text.size() >= Prefix.size() && std::string_view(Text).substr(0, Prefix.size()) == Prefix;
}
} // namespace

const PerfectWorld::NeighbourhoodFeature* FNsContext::Feature(const char* Id) const
{
    return PerfectWorld::FindNeighbourhoodFeature(Plan, Id);
}

FNsRect FNsContext::Bounds(const char* Id) const
{
    const PerfectWorld::NeighbourhoodFeature* F = Feature(Id);
    return F == nullptr ? FNsRect() : FNsRect(F->Bounds.X0, F->Bounds.Y0, F->Bounds.X1, F->Bounds.Y1);
}

TArray<FNsRect> FNsContext::PlanSolids() const
{
    TArray<FNsRect> Out;
    for (const PerfectWorld::NeighbourhoodFeature& F : Plan.Features)
    {
        bool bSolid = false;
        switch (F.Kind)
        {
        case NsKind::Footprint:
        case NsKind::Site:
        case NsKind::Carriageway:
        case NsKind::Footpath:
        case NsKind::Crossing:
            bSolid = true;
            break;
        case NsKind::Reservation:
            // The street reservations are covered by their carriageway and footpath features; the original lane's massing,
            // the campus and the club/pandal are the reservations that stand alone.
            bSolid = NsStartsWith(F.Id, "reserve.nabapara.original_lane") || NsStartsWith(F.Id, "reserve.nabapara.school") ||
                     NsStartsWith(F.Id, "reserve.nabapara.club_pandal");
            break;
        default:
            break;
        }
        if (bSolid)
        {
            Out.Add(FNsRect(F.Bounds.X0, F.Bounds.Y0, F.Bounds.X1, F.Bounds.Y1));
        }
    }
    return Out;
}

bool FNsContext::BlockedByPlan(const FNsRect& R) const
{
    for (const FNsRect& Solid : PlanSolids())
    {
        if (Solid.Overlaps(R))
        {
            return true;
        }
    }
    return false;
}

bool FNsContext::TouchesStreet(const FNsRect& R) const
{
    for (const PerfectWorld::NeighbourhoodFeature& F : Plan.Features)
    {
        if ((F.Kind == NsKind::Footpath || F.Kind == NsKind::Carriageway) &&
            FNsRect(F.Bounds.X0, F.Bounds.Y0, F.Bounds.X1, F.Bounds.Y1).Overlaps(R))
        {
            return true;
        }
    }
    return false;
}

bool FNsContext::Claimed(const FNsRect& R) const
{
    for (const FNsLot& Lot : Lots)
    {
        if (Lot.R.Overlaps(R))
        {
            return true;
        }
    }
    for (const FNsWall& Wall : Walls)
    {
        if (Wall.R.Overlaps(R))
        {
            return true;
        }
    }
    return false;
}

void NsBuildAll(FNsSink& K, FNsContext& Ctx)
{
    NsBuildRoads(K, Ctx);
    NsBuildShopLots(K, Ctx);
    NsBuildClubAndPandal(K, Ctx);
    NsBuildFrontages(K, Ctx);
    NsBuildStreetFurniture(K, Ctx);
    NsBuildBoundaries(K, Ctx);
}
} // namespace PWNs
