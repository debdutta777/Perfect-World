#pragma once

#include "CoreMinimal.h"
#include "WorldReducer.h"

// P509: local observation only. No world/save fields, translated text, identity or input capture.
// Calls enqueue bounded records; the worker alone opens files. A full queue drops telemetry, never play.
namespace P509SessionLog
{
void Boundary(const TCHAR* Kind, const PerfectWorld::WorldSim& Sim, const PerfectWorld::WorldState* LoadedSnapshot = nullptr);
void Command(const PerfectWorld::Command& Input, const PerfectWorld::CommandResult& Result,
    const PerfectWorld::WorldSim& Sim, uint64 FirstReceipt);

// Read-only diagnostics for the acceptance scenario. Poll across frames; never wait for disk on the game thread.
FString Path();
bool Idle();
}
