// Shared helpers for the M2-10 relationship tests.
#pragma once

#include "EventLog.h"
#include "Relationships.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace RelTestSupport
{
inline int Checks = 0;
}

inline void Check(bool condition, const char* message)
{
    ++RelTestSupport::Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

inline std::string RelPerson(std::size_t n)
{
    return "npc_" + std::to_string(n);
}

inline std::uint64_t RelAppend(EventLog& log, GameInstant at, const std::string& type, const std::vector<std::string>& parties)
{
    EventLogDraft draft;
    draft.At = at;
    draft.Type = type;
    draft.Parties = parties;
    const EventLogAppendResult result = log.Append(draft);
    Check(result.Ok(), "the test event is a valid draft");
    return result.Seq;
}

inline RelationshipAct RelAct(std::uint64_t seq, const std::string& actor, const std::string& other, RelationshipActKind kind)
{
    RelationshipAct act;
    act.EventSeq = seq;
    act.Actor = actor;
    act.Other = other;
    act.Kind = kind;
    return act;
}

// Defined in RelationshipsSaveTests.cpp.
void RelTestSaveRefusals();
