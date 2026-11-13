#pragma once

#include "PerfectWorldNeighbourhoodSink.h"

namespace PWNs
{
/**
 * Block lettering for signboards: every letter is a bitmap of cube instances, a 5 x 7 pixel face drawn on a 6-pixel pitch.
 * Bengali business names are lettered in Latin transliteration, as most Kolkata signboards are. Text reads along +U of the
 * frame, so build the frame with NsFaceFrame (U is the viewer's right).
 */

/** Width in centimetres that Text occupies at pixel size Px (no trailing gap). */
double NsTextWidth(const char* Text, double Px);

/** True when every character of Text has a glyph (A-Z, 0-9, space and . - / & ' ,). */
bool NsCanDraw(const char* Text);

/**
 * Draws block text with its top at WTop and its left at U0, in the lettering slab between frame depths V0 and V1 (negative V
 * is out of the face). Horizontal runs of lit pixels merge into one box. Returns the number of boxes emitted.
 */
int32 NsDrawText(FNsSink& K, const FLaneFrame& F, ELaneColour Colour, double U0, double WTop, double Px, double V0, double V1,
                 const char* Text);
} // namespace PWNs
