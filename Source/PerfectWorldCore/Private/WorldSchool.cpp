#include "WorldReducer.h"

#include "SchoolLessonContent.h"

#include <utility>

// S04: the school.lesson.* commands through WorldSim, plus the guards every Phase 2 school command shares. Each command
// stages copies of the school slice and the day, runs the frozen SchoolSessions contract on them, charges any movement of
// the day through ChargeTime (so scheduled events, the day loop and allowance settlement stay in step) and commits only
// when everything succeeded; Submit's snapshot restores a late refusal. Work and the week are in WorldSchoolWeek.cpp.
namespace PerfectWorld
{
namespace
{
Rejection S04Refusal(std::string code, std::string message)
{
    return Rejection{std::move(code), std::move(message)};
}

Rejection S04FromIssue(const SchoolIssue& issue)
{
    return Rejection{issue.Code, issue.Detail.empty() ? std::string("The school rules refused that action.") : issue.Detail};
}

bool S04HasStructures(const AssessmentStructures& structures)
{
    return structures.Model != nullptr && structures.Explain != nullptr && structures.Investigate != nullptr &&
           structures.Coordinate != nullptr;
}

/**
 * S04 seam (R9 not routable): the authoritative skill for a lesson is R9's Awakening.Stats.FindSkill(family skill).LevelMilli,
 * zero when absent. WorldState holds no AwakeningState yet, so every skill is absent and reads zero, exactly as
 * CompressSchoolWeek already opens its lessons. Replace this body (and S04WorkSkills) once R9-04 adds the owner.
 */
std::int32_t S04LessonSkillMilli(const WorldState& /*world*/, const SchoolLessonBinding& /*binding*/)
{
    return 0;
}
} // namespace

bool IsSchoolPhase2Command(const CommandPayload& payload) noexcept
{
    return std::holds_alternative<SchoolLessonOpen>(payload) || std::holds_alternative<SchoolLessonChoose>(payload) ||
           std::holds_alternative<SchoolLessonClose>(payload) || std::holds_alternative<SchoolWorkCommand>(payload) ||
           std::holds_alternative<SchoolWeekOrder>(payload) || std::holds_alternative<SchoolWeekResolve>(payload);
}

std::optional<Rejection> SchoolPlayOpenRefusal(const WorldState& state)
{
    if (state.SchoolSessions.Active)
        return S04Refusal("school.lesson_open", "A lesson is open; record it or leave early first.");
    for (const SchoolWorkRecord& record : state.SchoolWork.Records)
    {
        if (record.StartedAt && !record.SubmittedAt)
            return S04Refusal("school.paper_open", "A paper is open; hand it in or set it aside first.");
    }
    return std::nullopt;
}

std::optional<Rejection> WorldSim::ValidateSchoolPhase2(const Command& command) const
{
    if (!IsWorldDayInStep(State_))
        return S04Refusal("school.invalid_state", "The world and day must be valid and share one clock.");
    if (!IsWorldQuiescent(State_))
        return S04Refusal("world.unsettled", "Events are still due; settle them first.");
    if (!OpenConversationPersonId(State_).empty())
        return S04Refusal("school.conversation_open", "Finish the conversation first.");
    if (!ServiceShop_.empty())
        return S04Refusal("commerce.service_active", "Close the counter service first.");
    const bool dayContent = School_.School != nullptr && School_.Calendar != nullptr && School_.Activities != nullptr;
    const auto missing = [] { return S04Refusal("school.missing_content", "The school's lessons and papers are not bound to the world."); };
    if (std::holds_alternative<SchoolWeekOrder>(command.Payload))
        return std::nullopt; // Planning is free and needs no content.
    if (const auto* work = std::get_if<SchoolWorkCommand>(&command.Payload))
    {
        (void)work;
        if (Lessons_.Papers == nullptr)
            return missing();
        if (State_.SchoolSessions.Active)
            return S04Refusal("school.lesson_open", "A lesson is open; record it or leave early first.");
        return std::nullopt;
    }
    if (!dayContent || !S04HasStructures(Lessons_.Structures))
        return missing();
    if (std::holds_alternative<SchoolWeekResolve>(command.Payload) || std::holds_alternative<SchoolLessonOpen>(command.Payload))
    {
        if (Lessons_.Lessons == nullptr)
            return missing();
    }
    if (std::holds_alternative<SchoolLessonOpen>(command.Payload))
    {
        if (std::optional<Rejection> open = SchoolPlayOpenRefusal(State_))
            return open;
    }
    return std::nullopt;
}

std::optional<Rejection> WorldSim::ApplySchoolLessonCommand(const Command& command, CommandResult& result)
{
    const SchoolSessionCatalogues cats = SchoolCatalogues();
    SchoolSessionsState sessions = State_.SchoolSessions;
    DayLoopState day = State_.DayLoop;
    const GameInstant now = State_.Clock.Now();
    SchoolSessionResult applied;
    if (const auto* open = std::get_if<SchoolLessonOpen>(&command.Payload))
    {
        // BindingId is resolved here from the bound A1 catalogue; ValidateSchoolLessonBinding refuses another stream's lesson.
        const SchoolLessonBinding* binding = FindSchoolLessonBinding(*Lessons_.Lessons, open->BindingId);
        if (binding == nullptr)
            return S04Refusal("school.session.bad_binding", "No lesson of this school has that binding.");
        applied = OpenSchoolLesson(sessions, day, cats, *binding, open->Mode, open->Goal, S04LessonSkillMilli(State_, *binding),
                                   State_.WorldSeed, {});
    }
    else if (const auto* choose = std::get_if<SchoolLessonChoose>(&command.Payload))
        applied = ChooseSchoolLesson(sessions, cats, choose->Session, choose->Action);
    else if (const auto* close = std::get_if<SchoolLessonClose>(&command.Payload))
        applied = CloseSchoolLesson(sessions, day, cats, close->Session, close->Cancel, close->ElapsedMinutes);
    else
        return S04Refusal("command.unknown", "Unknown command.");
    if (!applied.Ok())
        return S04FromIssue(applied.Issue);
    if (day.Now < now)
        return S04Refusal("school.invalid_state", "The lesson moved the day behind the world clock.");
    if (day.Now != now)
    {
        // Close charged the bell period (or the minutes before leaving early) in the day loop: the world follows it once.
        ChargeTime(day.Now - now, result);
        if (result.Partial || State_.Clock.Now() != day.Now)
            return S04Refusal("school.lesson.charge_interrupted", "Settle the scheduled time before recording the lesson.");
    }
    State_.SchoolSessions = std::move(sessions);
    State_.DayLoop = std::move(day);
    // Completion learning leaves the reducer exactly once, as the day loop emitted it (R9's ApplyActivityCredits input).
    for (const DayLoopEvent& event : applied.Events)
        result.Events.push_back({0, event.At, event});
    StagedLesson_ = std::move(applied);
    return std::nullopt;
}
} // namespace PerfectWorld
