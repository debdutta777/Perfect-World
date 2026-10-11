// ROADMAP M3-06: the escalation clock and the triggers. Part of PerfectWorldThreadTests.
#include "ThreadsTestSupport.h"

namespace ThrTest
{
namespace
{
const ThreadInstance& Live(const Rig& rig, const std::string& id)
{
    const ThreadInstance* live = ThreadFind(rig.Set, id);
    Expect(live != nullptr, "the thread is live: " + id);
    static const ThreadInstance none;
    return live != nullptr ? *live : none;
}

std::int32_t FiredAt(const std::vector<ThreadEvent>& events, GameInstant at)
{
    for (const ThreadEvent& event : events)
    {
        if (event.Kind == ThreadEventKind::TriggerFired && event.At == at)
            return event.Trigger;
    }
    return -1;
}

void ScheduleOf(const ThreadTemplate& templ)
{
    Rig rig;
    const GameInstant t0 = Days(3) + 17; // not on a day boundary
    const std::string id = rig.Open(templ.Id.c_str(), 0, t0);
    GameInstant entered = t0;
    std::int32_t previousIntensity = 0;
    for (std::size_t i = 0; i < templ.Stages.size(); ++i)
    {
        Expect(Live(rig, id).Stage == static_cast<std::int32_t>(i) && ThreadIntensity(templ, Live(rig, id)) > previousIntensity,
               templ.Id + " is in the expected stage and the intensity has risen");
        previousIntensity = ThreadIntensity(templ, Live(rig, id));
        const GameInstant end = entered + Days(templ.Stages[i].DwellDays);
        const std::vector<ThreadEvent> quiet = rig.Advance(end - 1);
        Expect(quiet.empty() && Live(rig, id).Stage == static_cast<std::int32_t>(i) && Live(rig, id).Now == end - 1,
               templ.Id + " stays put until the dwell is over");
        const std::vector<ThreadEvent> events = rig.Advance(end);
        if (i + 1 < templ.Stages.size())
        {
            Expect(events.size() == 2 && events[0].Kind == ThreadEventKind::Escalated && events[0].At == end &&
                       events[0].Stage == static_cast<std::int32_t>(i + 1) && events[1].Kind == ThreadEventKind::Expressed &&
                       events[1].Channel == templ.Stages[i + 1].Expression.Channel && events[1].Scene == templ.Stages[i + 1].Expression.Scene,
                   templ.Id + " escalates exactly when the dwell ends and shows the next expression");
            Expect(Live(rig, id).StageEnteredAt == end, "the new stage starts at that instant");
            const ThreadExpression* shown = ThreadCurrentExpression(templ, Live(rig, id));
            Expect(shown != nullptr && shown->Observe == templ.Stages[i + 1].Expression.Observe, "the expression follows the stage");
        }
        else
        {
            Expect(events.size() == 1 && events[0].Kind == ThreadEventKind::Resolved && events[0].Detail == templ.DefaultResolution &&
                       events[0].At == end,
                   templ.Id + " resolves with its default resolution when the last dwell ends");
        }
        entered = end;
    }
    Expect(rig.Set.Active.empty() && rig.Set.Resolved.size() == 1 && rig.Set.Resolved[0].Resolution == templ.DefaultResolution &&
               rig.Set.Resolved[0].At == entered,
           "the thread is closed on schedule and recorded");
    Expect(rig.Advance(entered + Days(30)).empty(), "a closed thread produces nothing more");

    // One long advance reaches the same history.
    Rig once;
    once.Open(templ.Id.c_str(), 0, t0);
    const std::vector<ThreadEvent> all = once.Advance(entered + Days(400));
    Expect(CountEvents(all, ThreadEventKind::Escalated) == templ.Stages.size() - 1 && CountEvents(all, ThreadEventKind::Resolved) == 1 &&
               once.Set == rig.Set,
           "a single long advance gives the same set");
}

void TestStaying()
{
    Rig rig;
    rig.Open("thread.hidden_pickup", 0, Days(2));
    const ThreadSet before = rig.Set;
    Expect(rig.Advance(Days(2)).empty() && rig.Set == before, "advancing to the same instant changes nothing");
    const ThreadAdvanceResult back = ThreadAdvance(rig.Set, TheWorld().Templates, rig.MakeFacts(), Days(1));
    Expect(back.Status == ThreadStatus::BadTime && rig.Set == before, "time does not run backwards");
    Expect(ThreadAdvance(rig.Set, TheWorld().Templates, rig.MakeFacts(), -5).Status == ThreadStatus::BadTime, "a negative instant is refused");
    ThreadTemplateSet empty;
    Expect(ThreadAdvance(rig.Set, empty, rig.MakeFacts(), Days(3)).Status == ThreadStatus::BadTemplate && rig.Set == before,
           "a set without its templates is refused");
    rig.Log1(Days(2), "something_happened", {Person(8), Person(9)});
    ThreadSet carrying = rig.Set;
    carrying.Active[0].Cursor = 5;
    Expect(ThreadAdvance(carrying, TheWorld().Templates, rig.MakeFacts(), Days(3)).Status == ThreadStatus::BadFacts,
           "a thread that has read more log than exists is refused");
}
} // namespace

void TestSchedule()
{
    for (const ThreadTemplate& templ : TheWorld().Templates.Templates)
        ScheduleOf(templ);
    TestStaying();
}

namespace
{
void NotesReturned()
{
    Rig rig;
    const GameInstant t0 = DayStart("2042-06-10") + 360;
    // An event before the thread opened is history, not a response.
    rig.Log1(t0 - 5, "notes_returned", {Person(0), Person(1)});
    const std::string id = rig.Open("thread.borrowed_notes", 0, t0);
    rig.Log1(t0 + Days(1), "notes_returned", {Person(0), Person(7)});      // someone else
    rig.Log1(t0 + Days(1), "notes_borrowed_again", {Person(0), Person(1)}); // another kind
    rig.Advance(t0 + Days(12));
    Expect(Live(rig, id).Stage == 1, "the old and unrelated events changed nothing: the clock moved the thread to 'due'");
    const GameInstant act = t0 + Days(12) + 30;
    rig.Log1(act, "notes_returned", {Person(1), Person(0)}); // party order does not matter
    const std::vector<ThreadEvent> events = rig.Advance(t0 + Days(13));
    Expect(rig.Set.Active.empty() && rig.Set.Resolved.size() == 1 && rig.Set.Resolved[0].Resolution == "returned" &&
               rig.Set.Resolved[0].At == act,
           "returning the notes resolves the thread at the instant it happened");
    Expect(CountEvents(events, ThreadEventKind::TriggerFired, "returned_in_time") == 1 && CountEvents(events, ThreadEventKind::Resolved) == 1 &&
               FiredAt(events, act) == 0,
           "the trigger fired exactly once");

    // The same story with the shared explanation.
    Rig second;
    second.Open("thread.borrowed_notes", 0, t0);
    second.Log1(t0 + Days(2), "notes_explained_together", {Person(0), Person(1)});
    second.Advance(t0 + Days(3));
    Expect(second.Set.Resolved.size() == 1 && second.Set.Resolved[0].Resolution == "shared_understanding", "working through the notes together resolves it differently");

    // Untouched, it still reaches its default on schedule (the old event did not count).
    Rig untouched;
    untouched.Log1(t0 - 5, "notes_returned", {Person(0), Person(1)});
    untouched.Open("thread.borrowed_notes", 0, t0);
    untouched.Advance(t0 + Days(42));
    Expect(untouched.Set.Resolved.size() == 1 && untouched.Set.Resolved[0].Resolution == "faded" && untouched.Set.Resolved[0].At == t0 + Days(42),
           "untouched, it fades on day 42");
}

void RefusalCounting()
{
    Rig rig;
    const GameInstant t0 = DayStart("2042-07-01") + 360;
    const std::string id = rig.Open("thread.refused_invitations", 0, t0);
    const std::vector<std::string> pair = {Person(0), Person(1)};
    rig.Log1(t0 + Days(1), "invitation_refused", pair);
    rig.Advance(t0 + Days(2));
    Expect(Live(rig, id).Stage == 0 && Live(rig, id).Counts[0] + Live(rig, id).Counts[1] + Live(rig, id).Counts[2] + Live(rig, id).Counts[3] >= 1,
           "one refusal is counted but is not enough");
    rig.Log1(t0 + Days(3), "invitation_refused", pair);
    const std::vector<ThreadEvent> events = rig.Advance(t0 + Days(4));
    Expect(Live(rig, id).Stage == 1 && Live(rig, id).StageEnteredAt == t0 + Days(3) && CountEvents(events, ThreadEventKind::Escalated) == 1 &&
               FiredAt(events, t0 + Days(3)) == 3,
           "the second refusal escalates the thread at the moment it happens");
    // Exactly once: more refusals do not fire the same trigger again.
    rig.Log1(t0 + Days(5), "invitation_refused", pair);
    rig.Log1(t0 + Days(6), "invitation_refused", pair);
    rig.Log1(t0 + Days(7), "invitation_refused", pair);
    const std::vector<ThreadEvent> more = rig.Advance(t0 + Days(8));
    Expect(Live(rig, id).Stage == 1 && CountEvents(more, ThreadEventKind::TriggerFired) == 0, "a trigger fires once per thread");
    Expect(Live(rig, id).Fired == 8u, "only that trigger is marked fired");

    // Easing: the clock takes it to 'cooling', an explanation brings it back one stage and restarts the dwell.
    rig.Advance(t0 + Days(3) + Days(14) + Days(1));
    Expect(Live(rig, id).Stage == 2, "left alone it moved on to 'cooling'");
    const GameInstant explained = t0 + Days(19) + 100;
    rig.Log1(explained, "reason_explained", pair);
    const std::vector<ThreadEvent> eased = rig.Advance(explained + 10);
    Expect(CountEvents(eased, ThreadEventKind::Eased) == 1 && Live(rig, id).Stage == 1 && Live(rig, id).StageEnteredAt == explained,
           "explaining why eases the thread one stage");
    rig.Advance(explained + Days(14) - 1);
    Expect(Live(rig, id).Stage == 1, "its dwell restarted when it eased");
    rig.Advance(explained + Days(14));
    Expect(Live(rig, id).Stage == 2, "and it escalates again a full dwell later");

    // Saying yes resolves it as mended, even from a late stage.
    rig.Log1(explained + Days(15), "invitation_accepted", pair);
    rig.Advance(explained + Days(16));
    Expect(rig.Set.Resolved.size() == 1 && rig.Set.Resolved[0].Resolution == "mended", "accepting an invitation mends it");
}

void StageWindowAndSameInstant()
{
    Rig rig;
    const GameInstant t0 = DayStart("2042-07-01") + 360;
    const std::string id = rig.Open("thread.evening_rota", 0, t0);
    const std::vector<std::string> pair = {Person(0), Person(1)};
    rig.Log1(t0 + Days(1), "obligation_missed", pair);
    // A second miss at the very instant the first stage ends: the trigger comes before the clock, so one stage, not two.
    rig.Log1(t0 + Days(7), "obligation_missed", pair);
    const std::vector<ThreadEvent> events = rig.Advance(t0 + Days(7));
    Expect(Live(rig, id).Stage == 1 && Live(rig, id).StageEnteredAt == t0 + Days(7) && CountEvents(events, ThreadEventKind::Escalated) == 1,
           "a trigger and the clock at one instant move the thread one stage");
    Expect(FiredAt(events, t0 + Days(7)) == 3, "it was the trigger that moved it");
    // The counter for a stage window restarts in the new stage.
    rig.Log1(t0 + Days(8), "obligation_missed", pair);
    rig.Advance(t0 + Days(9));
    Expect(Live(rig, id).Stage == 1, "one miss in the new stage is not enough, though it is the third since opening");
    // Easing at stage 0 restarts the dwell.
    Rig rival;
    const std::string rid = rival.Open("thread.sports_day_rival", 0, t0);
    rival.Log1(t0 + Days(5), "practice_shared", {Person(0), Person(1)});
    const std::vector<ThreadEvent> eased = rival.Advance(t0 + Days(6));
    Expect(CountEvents(eased, ThreadEventKind::Eased) == 1 && Live(rival, rid).Stage == 0 && Live(rival, rid).StageEnteredAt == t0 + Days(5),
           "easing at the first stage restarts its dwell");
    rival.Advance(t0 + Days(5) + Days(14) - 1);
    Expect(Live(rival, rid).Stage == 0, "so the first escalation is a dwell after the ease");
}

void RelationshipTriggers()
{
    // Owed: the classmate helped the player, so the player owes them (3000), which the thread acknowledges once.
    Rig notes;
    const GameInstant t0 = DayStart("2042-06-10") + 360;
    const std::string nid = notes.Open("thread.borrowed_notes", 0, t0);
    notes.Act(t0 + 60, 1, 0, RelationshipActKind::Help);
    const std::vector<ThreadEvent> events = notes.Advance(t0 + Days(2));
    Expect(CountEvents(events, ThreadEventKind::TriggerFired, "debt_acknowledged") == 1 && FiredAt(events, DayStart("2042-06-11")) == 3,
           "owing a favour is acknowledged at the next midnight");
    Expect(CountEvents(events, ThreadEventKind::Expressed) == 1 && events.back().Trigger == 3 && events.back().Channel == ThreadChannel::Conversation,
           "it expresses itself through a conversation");
    Expect(CountEvents(notes.Advance(t0 + Days(8)), ThreadEventKind::TriggerFired, "debt_acknowledged") == 0 && Live(notes, nid).Stage == 0,
           "and only once");

    // Trust and time: a carer who trusts the player gives room, but only three days into the squeezed stage.
    Rig rota;
    const std::string rid = rota.Open("thread.evening_rota", 0, t0);
    rota.Act(t0 + 10, 1, 0, RelationshipActKind::Help);
    rota.Act(t0 + 20, 1, 0, RelationshipActKind::Confide);
    rota.Act(t0 + 30, 1, 0, RelationshipActKind::Defend);
    rota.Advance(t0 + Days(7) + 5);
    Expect(Live(rota, rid).Stage == 1, "trust alone does nothing before the thread is squeezed");
    rota.Advance(t0 + Days(10) - 1);
    Expect(Live(rota, rid).Stage == 1, "nor before the third day of that stage");
    const std::vector<ThreadEvent> eased = rota.Advance(t0 + Days(10));
    Expect(CountEvents(eased, ThreadEventKind::Eased) == 1 && Live(rota, rid).Stage == 0 && FiredAt(eased, t0 + Days(10)) == 4,
           "on the third day the trust eases the thread, at that exact minute");
}

void ThreeRoles()
{
    const GameInstant t0 = DayStart("2042-08-03") + 360;
    Rig adviser;
    adviser.Open("thread.hidden_pickup", 0, t0);
    adviser.Log1(t0 + Days(11), "adviser_consulted", {Person(1), Person(2)});
    adviser.Advance(t0 + Days(12));
    Expect(adviser.Set.Resolved.size() == 1 && adviser.Set.Resolved[0].Resolution == "support_found", "the classmate asking the adviser resolves it");
    Rig broken;
    broken.Open("thread.hidden_pickup", 0, t0);
    broken.Log1(t0 + Days(3), "confidence_broken", {Person(0), Person(1)});
    broken.Advance(t0 + Days(4));
    Expect(broken.Set.Resolved.size() == 1 && broken.Set.Resolved[0].Resolution == "trust_broken", "betraying the confidence ends it badly");
    Rig confided;
    const std::string id = confided.Open("thread.hidden_pickup", 0, t0);
    confided.Log1(t0 + Days(12), "confidence_shared", {Person(0), Person(1)});
    confided.Advance(t0 + Days(13));
    Expect(Live(confided, id).Stage == 0, "being confided in eases a thread that had begun to slip");
    // The adviser must be in the entry: the peer alone is not enough.
    Rig partial;
    partial.Open("thread.hidden_pickup", 0, t0);
    partial.Log1(t0 + Days(11), "adviser_consulted", {Person(1), Person(5)});
    partial.Advance(t0 + Days(12));
    Expect(partial.Set.Resolved.empty(), "an entry that lacks one of the required parties does not count");
}
} // namespace

void TestTriggers()
{
    NotesReturned();
    RefusalCounting();
    StageWindowAndSameInstant();
    RelationshipTriggers();
    ThreeRoles();
}

namespace
{
std::vector<ThreadEvent> Fired(const std::vector<ThreadEvent>& events)
{
    std::vector<ThreadEvent> out;
    for (const ThreadEvent& event : events)
    {
        if (event.Kind == ThreadEventKind::TriggerFired)
            out.push_back(event);
    }
    return out;
}
} // namespace

void TestCalendarTriggers()
{
    const SchoolCalendar* calendar = TheWorld().Calendar;
    {
        Rig rig;
        rig.Calendar = calendar;
        rig.Open("thread.refused_invitations", 0, DayStart("2042-10-10") + 360);
        const std::vector<ThreadEvent> before = rig.Advance(DayStart("2042-10-20") - 1);
        Expect(Fired(before).empty(), "Puja week has not begun");
        const std::vector<ThreadEvent> during = rig.Advance(DayStart("2042-10-26"));
        const std::vector<ThreadEvent> fired = Fired(during);
        Expect(fired.size() == 1 && fired[0].At == DayStart("2042-10-20") && fired[0].Trigger == 4, "the Puja ask fires at midnight on the first day, once");
        Expect(CountEvents(during, ThreadEventKind::Expressed) >= 1 && during[1].Channel == ThreadChannel::Conversation, "and it is expressed");
    }
    {
        // Without the calendar, or with a different one, a calendar condition never holds.
        Rig none;
        none.Open("thread.refused_invitations", 0, DayStart("2042-10-10") + 360);
        Expect(Fired(none.Advance(DayStart("2042-10-26"))).empty(), "no calendar, no calendar trigger");
        SchoolCalendar other = *calendar;
        other.Id = "calendar.other";
        Rig wrong;
        wrong.Calendar = &other;
        wrong.Open("thread.refused_invitations", 0, DayStart("2042-10-10") + 360);
        Expect(Fired(wrong.Advance(DayStart("2042-10-26"))).empty(), "a different calendar is not the template's calendar");
    }
    {
        // The test week ending brings the borrowed-notes debt due earlier than the clock would.
        Rig rig;
        rig.Calendar = calendar;
        rig.Open("thread.borrowed_notes", 0, DayStart("2042-07-20") + 360);
        const std::vector<ThreadEvent> events = rig.Advance(DayStart("2042-08-05"));
        const std::vector<ThreadEvent> fired = Fired(events);
        Expect(fired.size() == 1 && fired[0].At == DayStart("2042-08-02") && fired[0].Trigger == 2, "unit test 1 is over on 2 August: the debt turns awkward");
        Expect(CountEvents(events, ThreadEventKind::Escalated) == 2, "the clock had already moved it to 'due' on 30 July, then the calendar moved it on");
    }
    {
        Rig rig;
        rig.Calendar = calendar;
        rig.Open("thread.sports_day_rival", 0, DayStart("2042-11-25") + 360);
        const std::vector<ThreadEvent> fired = Fired(rig.Advance(DayStart("2042-12-08")));
        Expect(fired.size() == 1 && fired[0].At == DayStart("2042-12-05") && fired[0].Trigger == 3, "sports day moves the rivalry on, once");
        Expect(ThreadFind(rig.Set, "thr_1") != nullptr && ThreadFind(rig.Set, "thr_1")->Stage == 1, "to the practice gap");
    }
    {
        Rig rig;
        rig.Calendar = calendar;
        rig.Open("thread.evening_rota", 0, DayStart("2042-09-10") + 360);
        const std::vector<ThreadEvent> fired = Fired(rig.Advance(DayStart("2042-09-20")));
        Expect(fired.size() == 1 && fired[0].At == DayStart("2042-09-15") && fired[0].Trigger == 5, "half-yearly week brings the carer's offer");
    }
    {
        Rig rig;
        rig.Calendar = calendar;
        rig.Open("thread.hidden_pickup", 0, DayStart("2043-01-02") + 360);
        const std::vector<ThreadEvent> events = rig.Advance(DayStart("2043-01-13"));
        const std::vector<ThreadEvent> fired = Fired(events);
        Expect(fired.size() == 1 && fired[0].At == DayStart("2043-01-12") && fired[0].Trigger == 4 && CountEvents(events, ThreadEventKind::Escalated) == 1,
               "the class test week pushes the secret on a few hours before the clock would, and the clock then restarts");
        Expect(ThreadFind(rig.Set, "thr_1")->StageEnteredAt == DayStart("2043-01-12"), "the new stage began at midnight");
    }
    {
        Rig rig;
        rig.Calendar = calendar;
        rig.Open("thread.canal_side_stall", 0, DayStart("2042-10-25") + 360);
        const std::vector<ThreadEvent> fired = Fired(rig.Advance(DayStart("2042-11-14")));
        Expect(fired.size() == 1 && fired[0].At == DayStart("2042-11-12") && fired[0].Trigger == 3, "the festival night worries the stall keeper");
    }
}
} // namespace ThrTest
