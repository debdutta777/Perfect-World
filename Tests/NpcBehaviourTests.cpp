// LANE_LIFE_IMPLEMENTATION row 4.5: pure schedule-to-intent mapping for NPCs (work, travel, idle, chat, sleep, yield).
#include "NpcBehaviour.h"

#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
int Checks = 0;

void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

constexpr auto Role = ScheduleDoingKind::Role;
constexpr auto Act = ScheduleDoingKind::Activity;

const char* const HomeZone = "zone.home";
const char* const ShopZone = "zone.shop";
const char* const CafeZone = "zone.cafe";

// Day 0 is the Monday of the campaign start; instant 0 is 06:00 that day.
GameInstant At(std::int64_t day, int hour, int minute)
{
    return day * MinutesPerDay + hour * 60 + minute - 6 * 60;
}
int Hm(int hour, int minute)
{
    return hour * 60 + minute;
}

ScheduleEntry Entry(int start, int end, const char* zone, const char* doing, int priority, ScheduleDoingKind kind = Role)
{
    ScheduleEntry e;
    e.StartMinute = start;
    e.EndMinute = end;
    e.Zone = zone;
    e.Kind = kind;
    e.Doing = doing;
    e.Priority = priority;
    return e;
}

// A shopkeeper's day, every day. Gaps between stays leave room for the trips, so nothing is squeezed.
PersonSchedule Shopkeeper()
{
    PersonSchedule p;
    p.PersonId = "person.synthetic_shopkeeper";
    p.HomeZone = HomeZone;
    p.Routine = {
        Entry(Hm(0, 0), Hm(6, 0), HomeZone, "sleeping", 10),   // routine:0
        Entry(Hm(6, 30), Hm(7, 30), HomeZone, "at_home", 10),  // routine:1
        Entry(Hm(8, 0), Hm(12, 0), ShopZone, "sweeping", 10),  // routine:2
        Entry(Hm(12, 15), Hm(12, 55), CafeZone, "break", 10),  // routine:3
        Entry(Hm(13, 0), Hm(17, 0), ShopZone, "serving", 10),  // routine:4
        Entry(Hm(17, 30), Hm(18, 30), ShopZone, "stocking", 10), // routine:5
        Entry(Hm(19, 0), Hm(22, 0), HomeZone, "reading", 10),  // routine:6
        Entry(Hm(22, 0), Hm(24, 0), HomeZone, "sleeping", 10), // routine:7
    };
    return p;
}

ScheduleTravelTable Travel()
{
    ScheduleTravelTable table;
    table.Legs = {{CafeZone, HomeZone, 25}, {CafeZone, ShopZone, 5}, {HomeZone, ShopZone, 20}};
    return table;
}

NpcIntent Intent(const PersonSchedule& p, GameInstant at, bool conversation = false, bool blocked = false)
{
    const NpcIntentResult result = ResolveNpcIntentAt(p, Travel(), at, conversation, blocked);
    Check(result.Ok(), "the synthetic schedule resolves");
    return result.Intent;
}

/** Everything about an intent except how far along its segment it is and which schedule item placed it. */
struct Identity
{
    NpcState State;
    std::string Anchor;
    std::string Route;
    std::string_view Action;
    PersonAvailability Availability;
    bool bTalkable;
    std::string_view Reason;
    bool operator==(const Identity&) const = default;
};
Identity IdentityOf(const NpcIntent& i)
{
    return {i.State, i.AnchorId, i.RouteId, i.WorkActionId, i.Availability, i.bTalkable, i.DeclineReasonKey};
}

void TestTables()
{
    Check(ValidateNpcRoleTable(), "the role table is well formed");
    Check(FindNpcRole("sleeping") != nullptr && FindNpcRole("sleeping")->State == NpcState::Sleep, "sleeping is a sleep role");
    Check(FindNpcRole("sweeping") != nullptr && FindNpcRole("sweeping")->WorkActionId == "work.sweep", "sweeping names its work action");
    Check(FindNpcRole("flying") == nullptr && FindNpcRole("") == nullptr, "an unknown role is not found");
    bool anyBusy = false;
    bool anyAwake = false;
    for (const NpcRoleDef& role : NpcRoles())
    {
        if (role.State == NpcState::Work)
        {
            (role.WorkAvailability == PersonAvailability::Busy ? anyBusy : anyAwake) = true;
        }
    }
    Check(anyBusy && anyAwake, "work roles come in both Busy and Awake kinds");

    std::set<std::string_view> ids;
    for (std::size_t i = 0; i < NpcStateCount; ++i)
    {
        Check(!NpcStateId(static_cast<NpcState>(i)).empty(), "every state has an id");
        ids.insert(NpcStateId(static_cast<NpcState>(i)));
    }
    Check(ids.size() == NpcStateCount && NpcStateId(NpcState::Yield) == "yield" && NpcStateId(static_cast<NpcState>(99)).empty(), "state ids are unique and stable");
    Check(NpcIntentErrorCode(NpcIntentError::None) == "none" && NpcIntentErrorCode(NpcIntentError::UnknownRole) == "unknown_role" &&
              NpcIntentErrorCode(NpcIntentError::InvalidResolution) == "invalid_resolution" &&
              NpcIntentErrorCode(NpcIntentError::ConversationUnavailable) == "conversation_unavailable",
          "error codes are stable");
}

void TestFixture()
{
    const PersonSchedule p = Shopkeeper();
    Check(ValidatePersonSchedule(p).empty() && ValidateScheduleTravel(Travel()).empty(), "the synthetic schedule validates");
    Check(ValidateScheduleTravelCoverage(p, Travel()).empty() && ValidateScheduleRoutineTravel(p, Travel()).empty(), "no trip is squeezed");
    Check(ValidateNpcSchedule(p).empty(), "every role in the synthetic schedule is known");
}

void TestStates()
{
    const PersonSchedule p = Shopkeeper();

    const NpcIntent asleep = Intent(p, At(1, 3, 0));
    Check(asleep.State == NpcState::Sleep && asleep.AnchorId == HomeZone && asleep.Availability == PersonAvailability::Asleep, "03:00 asleep at home");
    Check(!asleep.bTalkable && asleep.DeclineReasonKey == NpcReasonAsleep && asleep.WorkActionId.empty() && asleep.RouteId.empty(), "a sleeper cannot be talked to and has no work");

    const NpcIntent gap = Intent(p, At(0, 6, 10));
    Check(gap.State == NpcState::Idle && gap.AnchorId == HomeZone && gap.Availability == PersonAvailability::Awake && gap.bTalkable &&
              gap.DeclineReasonKey.empty() && gap.SourceId.empty(),
          "a genuine gap idles in the last zone, awake and talkable");

    const NpcIntent rest = Intent(p, At(0, 7, 0));
    Check(rest.State == NpcState::Idle && rest.AnchorId == HomeZone && rest.SourceId == "routine:1", "an at_home role is idle with its source");

    const NpcIntent walking = Intent(p, At(0, 7, 50));
    Check(walking.State == NpcState::Travel && walking.RouteId == std::string("route:") + HomeZone + "->" + ShopZone && walking.AnchorId == ShopZone, "07:50 walking to the shop");
    Check(walking.ProgressPermille == 500 && walking.Availability == PersonAvailability::Busy && walking.bTalkable && walking.DeclineReasonKey == NpcReasonWalking,
          "mid-travel is Busy but greetable, halfway along");
    Check(walking.WorkActionId.empty(), "a traveller does no work yet");

    const NpcIntent sweeping = Intent(p, At(0, 9, 0));
    Check(sweeping.State == NpcState::Work && sweeping.AnchorId == ShopZone && sweeping.WorkActionId == "work.sweep", "09:00 sweeping at the shop");
    Check(sweeping.Availability == PersonAvailability::Busy && sweeping.bTalkable && sweeping.DeclineReasonKey == NpcReasonWorking && sweeping.RouteId.empty(),
          "sweeping is Busy: a polite decline");

    const NpcIntent serving = Intent(p, At(0, 14, 0));
    Check(serving.State == NpcState::Work && serving.WorkActionId == "work.serve" && serving.Availability == PersonAvailability::Awake &&
              serving.bTalkable && serving.DeclineReasonKey.empty(),
          "serving keeps the person Awake and willing");

    const NpcIntent onBreak = Intent(p, At(0, 12, 30));
    Check(onBreak.State == NpcState::Idle && onBreak.AnchorId == CafeZone, "the break is idle at the cafe");
    const NpcIntent toCafe = Intent(p, At(0, 12, 12));
    Check(toCafe.State == NpcState::Travel && toCafe.RouteId == std::string("route:") + ShopZone + "->" + CafeZone, "the trip to the cafe names its route");

    const NpcIntent late = Intent(p, At(0, 23, 0));
    Check(late.State == NpcState::Sleep && late.SourceId == "routine:7", "23:00 asleep again");

    // Chat and Yield are reached by the flags.
    const NpcIntent chat = Intent(p, At(0, 14, 0), true);
    Check(chat.State == NpcState::Chat && chat.bTalkable && chat.WorkActionId.empty(), "a conversation turns work into Chat");
    const NpcIntent yield = Intent(p, At(0, 7, 50), false, true);
    Check(yield.State == NpcState::Yield && yield.Availability == PersonAvailability::Busy && yield.bTalkable && yield.DeclineReasonKey == NpcReasonBlocked,
          "a blocked walker yields, Busy but greetable");

    std::set<NpcState> seen;
    for (GameInstant t = 0; t < MinutesPerDay; ++t)
    {
        seen.insert(Intent(p, t).State);
        seen.insert(Intent(p, t, false, true).State);
        const NpcIntentResult talk = ResolveNpcIntentAt(p, Travel(), t, true, false);
        if (talk.Ok())
        {
            seen.insert(talk.Intent.State);
        }
    }
    Check(seen.size() == NpcStateCount, "every state is reachable over a day");
}

void TestActivityEntries()
{
    PersonSchedule p = Shopkeeper();
    ScheduleException e;
    e.Id = "exception.class";
    e.From = At(1, 10, 0);
    e.To = At(1, 11, 0);
    e.Zone = ShopZone;
    e.Kind = Act;
    e.Doing = "activity.attend_class";
    e.Priority = 500;
    p.Exceptions.push_back(e);
    Check(ValidateNpcSchedule(p).empty(), "an activity entry only needs an id");
    const NpcIntent work = Intent(p, At(1, 10, 30));
    Check(work.State == NpcState::Work && work.WorkActionId == NpcActivityWorkActionId && work.SourceId == "exception.class" &&
              work.Availability == PersonAvailability::Busy,
          "an activity block is Busy work with the generic action");
}

void TestDeterminism()
{
    const PersonSchedule a = Shopkeeper();
    const PersonSchedule b = Shopkeeper();
    for (GameInstant t = 0; t < 2 * MinutesPerDay; t += 7)
    {
        Check(Intent(a, t) == Intent(a, t), "the same instant gives the same intent");
        Check(Intent(a, t) == Intent(b, t), "an equal schedule gives the same intent");
        const NpcIntentResult chatA = ResolveNpcIntentAt(a, Travel(), t, true, true);
        const NpcIntentResult chatB = ResolveNpcIntentAt(b, Travel(), t, true, true);
        Check(chatA.Error == chatB.Error && chatA.Intent == chatB.Intent, "the flags are deterministic too");
    }
    const ScheduleResolution r = ResolveSchedule(a, Travel(), At(0, 9, 0));
    Check(ResolveNpcIntent(r, false, false).Intent == ResolveNpcIntentAt(a, Travel(), At(0, 9, 0), false, false).Intent, "the two entry points agree");
}

void TestSplitAdvance()
{
    const PersonSchedule p = Shopkeeper();
    const GameInstant end = 2 * MinutesPerDay;

    // Minute by minute: the instants at which the identity changes.
    std::vector<GameInstant> minuteChanges;
    Identity last = IdentityOf(Intent(p, 0));
    for (GameInstant t = 1; t <= end; ++t)
    {
        const Identity now = IdentityOf(Intent(p, t));
        if (!(now == last))
        {
            minuteChanges.push_back(t);
            last = now;
        }
    }
    Check(minuteChanges.size() > 16, "the synthetic schedule changes identity many times over two days");

    // Hopping boundary to boundary visits every real change and nothing between two hops differs.
    std::vector<GameInstant> hops{0};
    while (true)
    {
        const std::optional<GameInstant> next = NextNpcIntentBoundary(p, Travel(), hops.back());
        if (!next || *next > end)
        {
            break;
        }
        Check(*next > hops.back(), "boundaries move strictly forward");
        hops.push_back(*next);
    }
    for (const GameInstant change : minuteChanges)
    {
        bool found = false;
        for (const GameInstant hop : hops)
        {
            found = found || hop == change;
        }
        Check(found, "every minute-by-minute change is a boundary");
    }
    std::size_t hopIndex = 0;
    for (GameInstant t = 0; t <= end; ++t)
    {
        while (hopIndex + 1 < hops.size() && hops[hopIndex + 1] <= t)
        {
            ++hopIndex;
        }
        Check(IdentityOf(Intent(p, t)) == IdentityOf(Intent(p, hops[hopIndex])), "between two boundaries the identity is that of the boundary");
    }

    // Advancing the schedule person in any split gives the same events, and each event agrees with the intent at its instant.
    std::vector<ScheduleEvent> whole;
    {
        SchedulePersonState state = StartSchedulePerson(p.PersonId, 0);
        const ScheduleAdvanceResult result = AdvanceSchedulePerson(p, Travel(), state, end);
        Check(result.Ok, "the whole advance is accepted");
        whole = result.Events;
    }
    for (const GameInstant chunk : {GameInstant{1}, GameInstant{7}, GameInstant{60}, GameInstant{333}, MinutesPerDay})
    {
        SchedulePersonState state = StartSchedulePerson(p.PersonId, 0);
        std::vector<ScheduleEvent> parts;
        for (GameInstant to = chunk; to < end + chunk; to += chunk)
        {
            const ScheduleAdvanceResult result = AdvanceSchedulePerson(p, Travel(), state, to < end ? to : end);
            Check(result.Ok, "a chunked advance is accepted");
            parts.insert(parts.end(), result.Events.begin(), result.Events.end());
        }
        Check(parts == whole, "split advances give the same events as one advance");
    }
    std::size_t checked = 0;
    for (const ScheduleEvent& event : whole)
    {
        if (event.Kind == ScheduleEventKind::ActivityEnded)
        {
            continue;
        }
        ++checked;
        const NpcIntent intent = Intent(p, event.At);
        Check(intent.AnchorId == event.Zone && intent.ProgressPermille == 0, "the intent at an event is the segment it begins");
        if (event.Kind == ScheduleEventKind::TravelBegan)
        {
            Check(intent.State == NpcState::Travel && intent.RouteId == "route:" + event.FromZone + "->" + event.Zone, "TravelBegan is Travel on that route");
        }
        else if (event.Kind == ScheduleEventKind::IdleBegan)
        {
            Check(intent.State == NpcState::Idle, "IdleBegan is Idle");
        }
        else
        {
            Check(intent.State == NpcState::Work || intent.State == NpcState::Idle || intent.State == NpcState::Sleep, "StayBegan is a stay state");
        }
    }
    Check(checked > 16, "events were compared with intents");
}

void TestResumeAfterChat()
{
    const PersonSchedule p = Shopkeeper();
    std::size_t chats = 0;
    for (GameInstant t = 0; t < MinutesPerDay; ++t)
    {
        const NpcIntent base = Intent(p, t);
        const NpcIntentResult chat = ResolveNpcIntentAt(p, Travel(), t, true, false);
        if (base.Availability == PersonAvailability::Asleep)
        {
            Check(!chat.Ok() && chat.Error == NpcIntentError::ConversationUnavailable && chat.Intent == NpcIntent{}, "no conversation with a sleeper");
            continue;
        }
        ++chats;
        Check(chat.Ok() && chat.Intent.State == NpcState::Chat, "a conversation opens with an awake or busy person");
        const NpcIntent& c = chat.Intent;
        Check(c.ResumeState == base.State && c.ResumeWorkActionId == base.WorkActionId, "Chat remembers the suspended state and action");
        Check(c.AnchorId == base.AnchorId && c.RouteId == base.RouteId && c.ProgressPermille == base.ProgressPermille && c.SourceId == base.SourceId,
              "Chat keeps the position context");
        Check(c.WorkActionId.empty() && c.Availability == base.Availability && c.DeclineReasonKey == base.DeclineReasonKey, "the work stops, availability stays");
        Check(base.ResumeState == base.State && base.ResumeWorkActionId == base.WorkActionId, "an unsuspended intent resumes itself");
        // After Leave: ask again without the flag and get exactly the same intent, at the same instant.
        Check(Intent(p, t, false, false) == base, "after Leave the same intent resumes");
        // Chat outranks blocked, and clearing both resumes.
        const NpcIntentResult both = ResolveNpcIntentAt(p, Travel(), t, true, true);
        Check(both.Ok() && both.Intent.State == NpcState::Chat && both.Intent == c, "Chat outranks the blocked flag");
    }
    Check(chats > 600, "most of the day can be chatted through");

    // Chat is stateless: leaving later gives the intent of the later instant, not of the old one.
    const NpcIntent during = Intent(p, At(0, 11, 55), true);
    const NpcIntent after = Intent(p, At(0, 12, 15));
    Check(during.ResumeState == NpcState::Work && after.State == NpcState::Idle && after.AnchorId == CafeZone, "leaving after the boundary resumes the then-current schedule");

    // Yield resumes the same way; Sleep ignores it.
    const NpcIntent blockedWalk = Intent(p, At(0, 7, 50), false, true);
    Check(blockedWalk.State == NpcState::Yield && blockedWalk.ResumeState == NpcState::Travel && blockedWalk.RouteId == Intent(p, At(0, 7, 50)).RouteId,
          "a blocked walker keeps its route");
    Check(Intent(p, At(0, 7, 50), false, false) == Intent(p, At(0, 7, 50)), "unblocked resumes travel");
    const NpcIntent blockedWork = Intent(p, At(0, 9, 0), false, true);
    Check(blockedWork.State == NpcState::Yield && blockedWork.ResumeState == NpcState::Work && blockedWork.ResumeWorkActionId == "work.sweep" &&
              blockedWork.WorkActionId.empty(),
          "a blocked worker holds and remembers the action");
    Check(Intent(p, At(1, 3, 0), false, true).State == NpcState::Sleep, "a sleeper ignores the blocked flag");
}

ScheduleResolution Stay(const char* zone, ScheduleDoingKind kind, const char* doing)
{
    ScheduleResolution r;
    r.Valid = true;
    r.Status = ScheduleStatus::Stay;
    r.Zone = zone;
    r.Kind = kind;
    r.Doing = doing;
    r.SourceId = "routine:0";
    return r;
}

void TestRefusals()
{
    const PersonSchedule p = Shopkeeper();
    const NpcIntent none{};

    auto refused = [&](const ScheduleResolution& r, NpcIntentError error, const char* message) {
        for (const bool conversation : {false, true})
        {
            for (const bool blocked : {false, true})
            {
                const NpcIntentResult result = ResolveNpcIntent(r, conversation, blocked);
                Check(!result.Ok() && result.Error == error && result.Intent == none, message);
            }
        }
    };
    refused(ScheduleResolution{}, NpcIntentError::InvalidResolution, "an invalid resolution is refused");
    Check(ResolveNpcIntentAt(p, Travel(), -1, false, false).Error == NpcIntentError::InvalidResolution, "a negative instant is refused");
    Check(ResolveNpcIntentAt(p, Travel(), ScheduleMaxInstant + 1, false, false).Error == NpcIntentError::InvalidResolution, "an instant past the range is refused");

    refused(Stay("", Role, "sleeping"), NpcIntentError::InvalidResolution, "an empty zone is refused");
    refused(Stay(HomeZone, Role, ""), NpcIntentError::InvalidResolution, "a stay with no doing is refused");
    refused(Stay(HomeZone, Act, ""), NpcIntentError::InvalidResolution, "an activity with no id is refused");
    refused(Stay(HomeZone, Role, "flying"), NpcIntentError::UnknownRole, "an unknown role is refused");

    ScheduleResolution progress = Stay(HomeZone, Role, "sleeping");
    progress.ProgressPermille = 1001;
    refused(progress, NpcIntentError::InvalidResolution, "progress above 1000 is refused");
    progress.ProgressPermille = -1;
    refused(progress, NpcIntentError::InvalidResolution, "negative progress is refused");

    ScheduleResolution badStatus = Stay(HomeZone, Role, "sleeping");
    badStatus.Status = static_cast<ScheduleStatus>(77);
    refused(badStatus, NpcIntentError::InvalidResolution, "an unknown status is refused");

    ScheduleResolution travel = Stay(ShopZone, Role, "sweeping");
    travel.Status = ScheduleStatus::Travel;
    refused(travel, NpcIntentError::InvalidResolution, "a trip with no origin is refused");
    travel.FromZone = ShopZone;
    refused(travel, NpcIntentError::InvalidResolution, "a trip from a zone to itself is refused");
    travel.FromZone = HomeZone;
    travel.Doing = "flying";
    refused(travel, NpcIntentError::UnknownRole, "a trip toward an unknown role is refused");
    travel.Doing = "sweeping";
    Check(ResolveNpcIntent(travel, false, false).Ok(), "the repaired trip is accepted");

    // A conversation with a sleeper is a refusal, not a silent Chat.
    const NpcIntentResult sleeper = ResolveNpcIntent(Stay(HomeZone, Role, "private"), true, false);
    Check(sleeper.Error == NpcIntentError::ConversationUnavailable && sleeper.Intent == none, "a private person cannot be talked to");
    Check(ResolveNpcIntent(Stay(HomeZone, Role, "private"), false, false).Intent.State == NpcState::Sleep, "private is a sleep role");

    PersonSchedule typo = Shopkeeper();
    typo.Routine[2].Doing = "sweepin";
    Check(!ValidateNpcSchedule(typo).empty(), "a typo in a routine role is caught");
    typo = Shopkeeper();
    ScheduleException e;
    e.Id = "exception.odd";
    e.From = 0;
    e.To = 60;
    e.Zone = HomeZone;
    e.Doing = "dancing";
    typo.Exceptions.push_back(e);
    Check(ValidateNpcSchedule(typo).find("exception.odd") != std::string::npos, "a typo in an exception role is caught and named");
}
} // namespace

int main()
{
    TestTables();
    TestFixture();
    TestStates();
    TestActivityEntries();
    TestDeterminism();
    TestSplitAdvance();
    TestResumeAfterChat();
    TestRefusals();
    std::cout << "NpcBehaviourTests passed (" << Checks << " checks)\n";
    return 0;
}
