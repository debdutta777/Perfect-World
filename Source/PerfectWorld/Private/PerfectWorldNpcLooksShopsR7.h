#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldNpcLooks.h"

/**
 * R7-04: the five shop staff's placeholder bodies. Independent of the look enum so this file compiles before the integrator appends
 * the ELook rows; the integrator's AppendCastLook cases call it (see R7-04_notes.md). Each body stands at the authored height H with
 * the head top at exactly H, the feet on the floor, every block inside the 34 cm capsule, two legs and two arms that swing, and a
 * face on +X.
 */
namespace PWNpcLooks
{
enum class ER74Shopper : uint8 { Bimal, Tithi, Aparna, Gouranga, Koushik };

/** Appends the blocks of one shop staff member. Heights: Bimal 170, Tithi 159, Aparna 172, Gouranga 175, Koushik 177. */
void R74AppendShopLook(ER74Shopper Shopper, double H, TArray<FPWNpcBlock>& Out);
} // namespace PWNpcLooks
