#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/** P38/P310: real WorldSim, no fake fallback. New life, home-to-school walk, terminal transfer, real clock commands and pad interactions,
 * three non-combat situations, committed grant and visible result, campus return, 17:59 closed / 18:00 open countdown.
 * Run with -PWAcceptance=awakening_round9 -PWAcceptanceStart=new and an available save slot.
 */
void P38AppendAwakeningSteps(TArray<FStepDef>& Out);
}
