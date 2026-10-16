#include "Conversation.h"

// The authored care catalogue: the staff member of the children's home, for a player who grew up in institutional care
// (Household Resources rank 0). LANE_LIFE_PLAN section B: "Care must not imply one employee works around the clock";
// gpt-6-astra's critique of 2026-10-14: a care start never inherits a parental label, remembered agreements say WHAT was
// agreed and never that it worked, and the home is shown with the same dignity as a family home.
//
// The person is "person.care_worker", labelled "Care worker" (it matches the LaneOrigin care binding); the owner may give
// the staff member a name later. Nothing here names a second person: the evening colleague is only "the evening staff".
// Plain ASCII, no markup. A choice's cost is shown beside its text by the presenter, so the text never says it.
//
// Tone. The care worker speaks to the player as a capable young adult who takes part in running the home: asks, proposes,
// remembers, never pities and never talks down, and is neither cold nor official. No line addresses the player as a child
// or as family.
//
// Duty. The care worker is on duty 07:00-15:00 and hands over to the evening staff with a written note. Off duty, and in
// the hours between, the person declines politely and points to who is on duty. An Asleep or Busy person declines
// whatever the hour; the Busy lines are split by the same duty hours so that "busy" read from an off-duty schedule never
// claims the person is in the middle of something.
//
// The shared jobs (parallel to the household evening jobs): who sets out breakfast and who tidies the study room. Talking
// about them costs three minutes (paid, may recur, charged once per session by receipt). The talk offers three concrete
// proposals; choosing one records it as the agreed arrangement (a fact), replaces an earlier arrangement and withdraws a
// "kept" that belonged to the old one. Each proposal is hidden while it is the arrangement. Lines say what was agreed and
// never that it worked.
//
// Day shape (minutes since midnight; Awake):
//   07:00-10:59  on duty, breakfast: back-time answers (before anything is agreed) or the remembered arrangement
//   11:00-14:59  on duty: the jobs are open, or settled
//   15:00-21:59  shift over, the evening staff are on duty
//   22:00-06:59  late, off duty until seven
namespace PerfectWorld
{
namespace
{
constexpr std::string_view Person = "person.care_worker";

constexpr std::string_view FactKept = "fact.care_jobs_kept";
constexpr std::string_view FactBreakfast = "fact.care_jobs_breakfast_weekdays";
constexpr std::string_view FactTurns = "fact.care_jobs_turns";
constexpr std::string_view FactStudyRoom = "fact.care_jobs_study_room";

// --- choices (one definition each, so every node that offers one offers it identically and one receipt covers it) ---
constexpr ConversationChoiceDef BackBeforeDark{"choice.care_worker.back_before_dark", "Before dark.", 0, false, "", "", "node.care_worker.reply.back_noted", "", ""};
constexpr ConversationChoiceDef BackUnsure{"choice.care_worker.back_unsure", "I am not sure yet.", 0, false, "", "", "node.care_worker.reply.back_noted", "", ""};
constexpr ConversationChoiceDef TalkJobs{"choice.care_worker.talk_jobs", "Talk about the shared jobs", 3, false, "", "", "node.care_worker.reply.proposals", "", ""};
constexpr ConversationChoiceDef RevisitJobs{"choice.care_worker.revisit_jobs", "Talk it over again", 3, false, "", "", "node.care_worker.reply.revisit", "", ""};
constexpr ConversationChoiceDef KeepJobs{"choice.care_worker.keep_jobs", "Yes, let us keep it.", 0, false, FactKept, "", "node.care_worker.reply.kept", "", ""};
constexpr ConversationChoiceDef ProposeBreakfast{"choice.care_worker.propose_breakfast", "I will set out breakfast on weekdays.", 0, false, FactBreakfast, FactKept,
                                                 "node.care_worker.reply.breakfast_weekdays", "", FactBreakfast};
constexpr ConversationChoiceDef ProposeTurns{"choice.care_worker.propose_turns", "I will take turns with the others, day by day.", 0, false, FactTurns, FactKept,
                                             "node.care_worker.reply.turns", "", FactTurns};
constexpr ConversationChoiceDef ProposeStudyRoom{"choice.care_worker.propose_study_room", "I will tidy the study room after homework hour.", 0, false, FactStudyRoom, FactKept,
                                                 "node.care_worker.reply.study_room", "", FactStudyRoom};

constexpr std::array<ConversationNodeDef, 24> Nodes{{
    // --- on duty, morning (Awake, 07:00-10:59) ---
    {"node.care_worker.breakfast", Person, "Good morning. Breakfast is being set out. Tell me what time you expect to be back, and I will put it in my note for the evening staff.",
     PersonAvailability::Awake, 420, 660, {}, {FactBreakfast, FactTurns, FactStudyRoom}, true, false, {BackBeforeDark, BackUnsure, TalkJobs}, 3},
    // --- the next morning: what was agreed, asked once whether to keep it ---
    {"node.care_worker.remembered_breakfast_weekdays", Person, "Good morning. You suggested setting out breakfast on weekdays. Shall we keep it that way?",
     PersonAvailability::Awake, 420, 660, {FactBreakfast, ""}, {FactKept, ""}, true, false, {KeepJobs, RevisitJobs}, 2},
    {"node.care_worker.remembered_turns", Person, "Good morning. You suggested taking turns with the others, day by day. Shall we keep it that way?",
     PersonAvailability::Awake, 420, 660, {FactTurns, ""}, {FactKept, ""}, true, false, {KeepJobs, RevisitJobs}, 2},
    {"node.care_worker.remembered_study_room", Person, "Good morning. You suggested tidying the study room after homework hour. Shall we keep it that way?",
     PersonAvailability::Awake, 420, 660, {FactStudyRoom, ""}, {FactKept, ""}, true, false, {KeepJobs, RevisitJobs}, 2},
    // --- the mornings after it was kept: it is still what was agreed, and no more is claimed ---
    {"node.care_worker.kept_breakfast_weekdays", Person, "Good morning. We are keeping to what we agreed: you set out breakfast on weekdays.",
     PersonAvailability::Awake, 420, 660, {FactBreakfast, FactKept}, {}, true, false, {RevisitJobs}, 1},
    {"node.care_worker.kept_turns", Person, "Good morning. We are keeping to what we agreed: you take turns with the others, day by day.",
     PersonAvailability::Awake, 420, 660, {FactTurns, FactKept}, {}, true, false, {RevisitJobs}, 1},
    {"node.care_worker.kept_study_room", Person, "Good morning. We are keeping to what we agreed: you tidy the study room after homework hour.",
     PersonAvailability::Awake, 420, 660, {FactStudyRoom, FactKept}, {}, true, false, {RevisitJobs}, 1},
    // --- on duty, the rest of the shift: open, or settled (Awake, 11:00-14:59) ---
    {"node.care_worker.jobs_open", Person, "We still need to settle who does the shared jobs: setting out breakfast and tidying the study room.",
     PersonAvailability::Awake, 660, 900, {}, {FactBreakfast, FactTurns, FactStudyRoom}, true, false, {TalkJobs}, 1},
    {"node.care_worker.jobs_settled_breakfast_weekdays", Person, "That is settled, then: you set out breakfast on weekdays.",
     PersonAvailability::Awake, 660, 900, {FactBreakfast, ""}, {}, true, false, {RevisitJobs}, 1},
    {"node.care_worker.jobs_settled_turns", Person, "That is settled, then: you take turns with the others, day by day.",
     PersonAvailability::Awake, 660, 900, {FactTurns, ""}, {}, true, false, {RevisitJobs}, 1},
    {"node.care_worker.jobs_settled_study_room", Person, "That is settled, then: you tidy the study room after homework hour.",
     PersonAvailability::Awake, 660, 900, {FactStudyRoom, ""}, {}, true, false, {RevisitJobs}, 1},
    // --- off duty (Awake): the shift is over, and who is on duty ---
    {"node.care_worker.off_duty_evening", Person, "My shift ended at three, and the evening staff are on duty now. I have left them my handover note. I am back at seven tomorrow.",
     PersonAvailability::Awake, 900, 1320, {}, {}, true, false, {}, 0},
    {"node.care_worker.off_duty_late", Person, "It is late, and I am off duty until seven. The staff on duty tonight will help if you need anything. Rest well.",
     PersonAvailability::Awake, 1320, 420, {}, {}, true, false, {}, 0},
    // --- polite declines ---
    {"node.care_worker.busy_on_duty", Person, "I am in the middle of something. Can we talk in a little while?", PersonAvailability::Busy, 420, 900,
     {}, {}, true, true, {}, 0},
    {"node.care_worker.busy_evening", Person, "My shift ended at three, so I am off duty. The evening staff are on duty now and have my handover note. I am back at seven tomorrow.",
     PersonAvailability::Busy, 900, 1320, {}, {}, true, true, {}, 0},
    {"node.care_worker.busy_late", Person, "I am off duty until seven. The staff on duty tonight will help if you need anything. Come and find me in the morning.",
     PersonAvailability::Busy, 1320, 420, {}, {}, true, true, {}, 0},
    {"node.care_worker.asleep", Person, "Mmm... not now. I am off duty until seven, and the staff on duty will help you. Come and find me in the morning.", PersonAvailability::Asleep, 0, 1440,
     {}, {}, true, true, {}, 0},
    // --- replies, reached only through a choice ---
    {"node.care_worker.reply.back_noted", Person, "All right. I will write that in my note for the evening staff. Tell them if it changes.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
    {"node.care_worker.reply.proposals", Person, "All right. What would you like to take on?", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {ProposeBreakfast, ProposeTurns, ProposeStudyRoom}, 3},
    {"node.care_worker.reply.revisit", Person, "All right. Tell me what you would change.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {ProposeBreakfast, ProposeTurns, ProposeStudyRoom}, 3},
    {"node.care_worker.reply.breakfast_weekdays", Person, "That is fair. Breakfast on weekdays is yours. We will see how it goes.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.care_worker.reply.turns", Person, "Taking turns with the others, then. We can decide together who starts.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.care_worker.reply.study_room", Person, "The study room after homework hour is yours, then. I will write it down. Let us try it.",
     PersonAvailability::Awake, 0, 1440, {}, {}, false, false, {}, 0},
    {"node.care_worker.reply.kept", Person, "Good. Then it is clear what we agreed.", PersonAvailability::Awake, 0, 1440,
     {}, {}, false, false, {}, 0},
}};

constexpr std::array<ConversationFactDef, 4> Facts{{
    {FactKept, "", ""},
    {FactBreakfast, "care_jobs_arrangement", ""},
    {FactTurns, "care_jobs_arrangement", ""},
    {FactStudyRoom, "care_jobs_arrangement", ""},
}};
} // namespace

ConversationCatalogue CareConversationCatalogue() noexcept
{
    return ConversationCatalogue{Nodes, Facts};
}
} // namespace PerfectWorld
