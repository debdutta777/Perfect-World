#include "PerfectWorldExaminationBridge.h"

#include "WorldExamSittings.h"

namespace P406Exam
{
FText P406RefusalText(const FString& Code)
{
    if (Code == TEXT("exam.sitting_used") || Code == TEXT("exam.missed"))
        return FText::Format(FText::FromString(TEXT("Each life has {0} examination sitting. Entering, declining or missing the {1}-minute entry window uses it. Your ordinary life and alternative qualifications remain open.")),
            FText::AsNumber(PerfectWorld::P413SittingsPerLife),
            FText::AsNumber(PerfectWorld::P409LateEntryMinutes));
    if (Code == TEXT("exam.defer_used"))
        return FText::Format(FText::FromString(TEXT("Each life may defer {0} time. Your sitting has already moved to the next annual examination; choose Sit or Decline.")),
            FText::AsNumber(PerfectWorld::P413DeferralsPerLife));
    if (Code == TEXT("exam.age_limit"))
        return FText::Format(FText::FromString(TEXT("You would be older than {0} at this sitting. Annual examination entry ends at age {0}; apprenticeships and adult qualification routes remain open.")),
            FText::AsNumber(PerfectWorld::P409AgeLimitYears));
    struct FP406Words { const TCHAR* Code; const TCHAR* Words; };
    static const FP406Words P406Words[] = {
        {TEXT("p407u.invalid_state"), TEXT("The appointment cannot be read from this life. No booking changed.")},
        {TEXT("p407u.busy"), TEXT("Finish the current activity, sleep or counter service before changing this appointment.")},
        {TEXT("p407u.unavailable"), TEXT("This next step is unavailable for your recorded result.")},
        {TEXT("p407u.attended"), TEXT("This session is already attended and recorded. It will not charge time again.")},
        {TEXT("p407u.stale"), TEXT("The appointment time changed. Read the refreshed next step and choose again.")},
        {TEXT("p407u.booked"), TEXT("This session is already booked, or no slot is available. Read the current appointment.")},
        {TEXT("p407u.not_booked"), TEXT("Book an appointment before attending it.")},
        {TEXT("p407u.not_due"), TEXT("Attend at the booked time. If you missed it, book a new slot.")},
        {TEXT("p407u.charge_interrupted"), TEXT("The session could not finish. Its time and attendance were not committed; try again.")},
        {TEXT("p407u.bad_action"), TEXT("That follow-up action is unavailable. No appointment changed.")},
        {TEXT("p407u.place"), TEXT("Visit Nabapara Vidyalaya, then reopen Examination from pause to attend the session.")},
        {TEXT("world.unsettled"), TEXT("Events at this instant are still being processed. Read the refreshed decision before sitting.")},
        {TEXT("time.conversation_open"), TEXT("Finish your conversation before sitting the examination.")},
        {TEXT("commerce.service_active"), TEXT("Close the shop counter before sitting the examination.")},
        {TEXT("time.service_open"), TEXT("Close the shop counter before sitting the examination.")},
        {TEXT("school.lesson_open"), TEXT("Finish or leave your current lesson before sitting the examination.")},
        {TEXT("school.paper_open"), TEXT("Finish your current school paper before sitting the examination.")},
        {TEXT("time.activity_running"), TEXT("Finish or cancel your current activity before sitting the examination.")},
        {TEXT("time.asleep"), TEXT("Wake up before sitting the examination.")},
        {TEXT("time.beyond_calendar"), TEXT("The remaining calendar cannot hold this examination.")},
        {TEXT("exam.not_open"), TEXT("Your eligible annual examination sitting has not opened.")},
        {TEXT("exam.decision_required"), TEXT("Choose an available examination option before the day continues.")},
        {TEXT("exam.active"), TEXT("Your examination is in progress. Continue it or withdraw first.")},
        {TEXT("exam.not_awakened"), TEXT("Complete your supervised Awakening before sitting the examination.")},
        {TEXT("exam.no_preparation"), TEXT("Your preparation record is not ready. Ask for supervised recovery before sitting.")},
        {TEXT("exam.missing_content"), TEXT("The examination passages are unavailable. You can still defer or decline.")},
        {TEXT("exam.invalid_state"), TEXT("Your examination record cannot be matched to these passages. No action was recorded.")},
        {TEXT("exam.clock_cap"), TEXT("The examination's outside time could not be charged. No action was recorded.")},
        {TEXT("exam.charge_interrupted"), TEXT("Leaving could not finish its time charge. Your examination is unchanged; try again.")},
        {TEXT("exam.not_terminal"), TEXT("Your examination has not ended yet.")},
        {TEXT("tower.session.not_ready"), TEXT("Your examination preparation is not ready for entry.")},
        {TEXT("tower.session.already_entered"), TEXT("Your participation is already recorded. Resume the current screen.")},
        {TEXT("tower.session.busy"), TEXT("Finish the current passage before opening another route.")},
        {TEXT("tower.session.unknown_route"), TEXT("That route is unavailable in these examination passages.")},
        {TEXT("tower.session.floor_order"), TEXT("Choose a route on your current floor.")},
        {TEXT("tower.session.method_not_allowed"), TEXT("That method is unavailable on this route.")},
        {TEXT("tower.session.full"), TEXT("No more passages can be opened in this session. You can withdraw.")},
        {TEXT("tower.session.stale_beat"), TEXT("The passage has moved on. Read the current choices and choose again.")},
        {TEXT("tower.session.invalid_state"), TEXT("The current passage cannot be read. Your record is unchanged.")},
        {TEXT("tower.session.unknown_choice"), TEXT("That choice is no longer available. Read the current passage.")},
        {TEXT("tower.session.not_entered"), TEXT("Sit the examination before making a passage choice.")},
        {TEXT("tower.session.closed"), TEXT("This examination has ended. Your result and next steps remain available.")},
        {TEXT("tower.session.bad_command"), TEXT("That examination action is unavailable. No action was recorded.")},
        {TEXT("tower.session.route_coverage"), TEXT("The examination passages do not cover all routes. Sitting is unavailable.")},
        {TEXT("tower.session.bad_scenario"), TEXT("An examination passage is incomplete. No action was recorded.")},
        {TEXT("tower.session.bad_beat"), TEXT("The passage's choices are incomplete. No action was recorded.")},
        {TEXT("tower.session.bad_clue"), TEXT("A passage clue is unavailable. No action was recorded.")},
        {TEXT("tower.session.performance_bounds"), TEXT("The examination passage has invalid costs or evidence. No action was recorded.")},
        {TEXT("tower.session.ceiling_unreachable"), TEXT("The examination passage has no valid completion. Sitting is unavailable.")},
        {TEXT("tower.session.bad_time"), TEXT("The examination clock is outside its supported dates.")}
    };
    for (const auto& P406Row : P406Words)
        if (Code == P406Row.Code) return FText::FromString(P406Row.Words);
    return FText::FromString(TEXT("That action could not be completed. Nothing was recorded."));
}
}
