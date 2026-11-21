#pragma once

#include "PerfectWorldAcceptanceSubsystem.h"

namespace PWAcceptance
{
/** R88: real Mahua approach, purchase, wallet, pause Inventory and consumption.
 * Fails explicitly if the merged commerce content does not give Mahua jhalmuri. Never installs a fake.
 * bShantiSmoke selects the existing Farida/biscuits catalogue for the additional smoke.
 */
void R88AppendCommerceSteps(TArray<FStepDef>& Out, bool bShantiSmoke = false);
}
