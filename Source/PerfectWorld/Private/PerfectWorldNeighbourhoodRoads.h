#pragma once

#include "PerfectWorldNeighbourhoodStreets.h"

namespace PWNs
{
constexpr double NsTramHalfWidth = 120.0; // the fictional tram corridor, centred on the main road
constexpr double NsMarkingHeight = 1.5;

/** A plan crossing with the carriageway it crosses. */
struct FNsCrossing
{
    FNsRect Cross;
    FNsRect Carriage;
    bool bStreetAlongX = true; // pedestrians cross along Y when true, along X when false
};

/** Every plan crossing that has a carriageway. Stable plan order. */
TArray<FNsCrossing> NsCollectCrossings(const FNsContext& Ctx);

/** The XY bounds of a street's carriageway feature by street id; empty when the plan has none. */
FNsRect NsCarriageOf(const FNsContext& Ctx, const char* StreetId);

/** Centre X of the tram corridor on the main road. */
double NsTramCentreX(const FNsContext& Ctx);

/** Paves the part of R that no earlier surface or ramp covers with footpath-coloured World boxes (top NsPath), and records it. */
void NsEmitPaving(FNsSink& K, FNsContext& Ctx, const FNsRect& R);

/** Zebras, lane dividers, direction arrows, stop lines, bus bays and the tram strip. Called by NsBuildRoads after the surfaces. */
void NsBuildMarkings(FNsSink& K, FNsContext& Ctx);

/** True when any rectangle in the list overlaps R. */
bool NsAnyOverlap(const TArray<FNsRect>& Rects, const FNsRect& R);
} // namespace PWNs
