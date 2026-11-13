#pragma once

#include "PerfectWorldNeighbourhoodSink.h"

namespace PWNs
{
/** Small standing props shared by the club yard, the bus stops and the street furniture. Each carries PropBlock collision. */

/**
 * A slatted bench of Length cm along X (bAlongX) or Y, 42 cm deep and 47 cm to the seat, with a backrest on the side away
 * from the way the sitter faces. Standing on the footpath level.
 */
void NsBench(FNsSink& K, double CentreX, double CentreY, double Length, bool bAlongX, ENsFace SitterFaces);

/** A street tree: a 330 cm trunk with stacked canopy blocks above head height; only the trunk is solid. Base at footpath level. */
void NsTree(FNsSink& K, double X, double Y, double BaseZ);

/** A street lamp: a 600 cm steel post with a 90 cm arm toward ArmToward ending in a lit head. Base at BaseZ. */
void NsLampPost(FNsSink& K, double X, double Y, double BaseZ, ENsFace ArmToward);

/** A concrete utility pole of 900 cm above the road with a cross-arm along X or Y; returns the cross-arm tip height. */
double NsPowerPole(FNsSink& K, double X, double Y, bool bArmAlongX, bool bTransformer);

/** A painted compound wall (World collision) with a light coping. */
void NsCompoundWall(FNsSink& K, const FNsRect& R, double Height, ELaneColour Colour);

/** A freestanding notice board: two posts and a board with paper notices, facing Face. Width cm wide, its panel 100 cm tall at 110 cm. */
void NsNoticeBoard(FNsSink& K, double CentreX, double CentreY, double Width, ENsFace Face, uint32 Seed);
} // namespace PWNs
