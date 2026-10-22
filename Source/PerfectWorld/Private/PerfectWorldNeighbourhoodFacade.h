#pragma once

#include "PerfectWorldNeighbourhoodSink.h"

namespace PWNs
{
/** What a frontage building looks like, in its own face frame (NsFaceFrame): U along the face, V into the building, W up. */
struct FNsFacade
{
    double Width = 600.0;
    double Depth = 1000.0;
    int32 Storeys = 3;
    double StoreyHeight = NsStorey;
    uint32 Seed = 0;
    ELaneColour Wall = ELaneColour::WallCream;
    ELaneColour Shutter = ELaneColour::ShutterBlue; // blue and red shutters are the commonest in the para
    bool bBalconies = true;                         // off where a signboard fills the first-floor wall (the new shop lots)

    /** Roof slab height above GroundZ, as FLaneBuilding::RoofZ: the floor level one storey above the top storey. */
    double RoofZ() const { return PWLane::FloorZ(Storeys, StoreyHeight); }
    /** Finished-floor height of storey N above GroundZ. */
    double FloorZ(int32 N) const { return PWLane::FloorZ(N, StoreyHeight); }
};

/** The shutter colour of a seed: blue about two in five, red one in three, then green and steel. */
ELaneColour NsShutterColour(uint32 Seed);
/** A weathered wall colour of a seed, different from Avoid (the neighbour's). */
ELaneColour NsWallColour(uint32 Seed, ELaneColour Avoid);

// Ground floors. Each emits the whole ground storey (walls, openings) and the signboard where there is one. All sealed: no door opens.
/** A closed roller-shutter shop under a lettered signboard; returns the signboard's boxes (0 when the face is too narrow). */
int32 NsGroundShutterShop(FNsSink& K, const FLaneFrame& F, const FNsFacade& S);
/** A house front: a wooden door, a barred window, a lamp and a number plate. */
void NsGroundHouse(FNsSink& K, const FLaneFrame& F, const FNsFacade& S);

/** The solid volume above the ground storey (everything from the shutter lintel to the roof slab) for the full face. */
void NsUpperVolume(FNsSink& K, const FLaneFrame& F, const FNsFacade& S, double FromW);

/** Window and balcony storeys above the ground floor, floor bands, cornice and damp stains. */
void NsUpperFloors(FNsSink& K, const FLaneFrame& F, const FNsFacade& S);
/** Parapet, water tanks, a stair room and an aerial. */
void NsRoof(FNsSink& K, const FLaneFrame& F, const FNsFacade& S, bool bClutter);
/** A second-row block seen over the roofs: window frames and glass, floor bands, parapet. */
void NsMediumBlock(FNsSink& K, const FLaneFrame& F, const FNsFacade& S);
/** A side wall that shows on a street: a few windows per floor and weathering. */
void NsFlankFace(FNsSink& K, const FLaneFrame& F, const FNsFacade& S);

/** A hoarding: a 240 cm painted wall with poster panels and block lettering. Returns the boxes of its lettering. */
int32 NsHoarding(FNsSink& K, const FLaneFrame& F, double Width, double Depth, uint32 Seed);

// Signboards (PerfectWorldNeighbourhoodSigns.cpp).
/** A board of Board colour over [U0, U1] x [W0, W1] with Text lettered in Ink, picking the largest pixel size that fits. Returns lettering boxes. */
int32 NsSignboard(FNsSink& K, const FLaneFrame& F, double U0, double U1, double W0, double W1, ELaneColour Board, ELaneColour Ink,
                  const char* Text);
/** A fictional shop name of a seed that is at most MaxChars long. */
const char* NsShopName(uint32 Seed, int32 MaxChars);
/** A fictional wall-advertisement line of a seed. */
const char* NsAdName(uint32 Seed);
} // namespace PWNs
