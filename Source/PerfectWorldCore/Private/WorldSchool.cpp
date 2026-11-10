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

/** R9-04: the skill the lesson's problem family trains (its catalogue's SkillId), empty when the family is unknown. */
std::string_view R94FamilySkill(const AssessmentStructures& structures, const SchoolLessonBinding& binding)
{
    switch (binding.Structure)
    {
    case SchoolStructure::Model:
        if (const auto* family = structures.Model != nullptr ? structures.Model->FindFamily(binding.FamilyId) : nullptr)
            return family->SkillId;
        break;
    case SchoolStructure::Explain:
        if (const auto* family = structures.Explain != nullptr ? structures.Explain->FindFamily(binding.FamilyId) : nullptr)
            return family->SkillId;
        break;
    case SchoolStructure::Investigate:
        if (const auto* family = structures.Investigate != nullptr ? structures.Investigate->FindFamily(binding.FamilyId) : nullptr)
            return family->SkillId;
        break;
    case SchoolStructure::Coordinate:
        if (const auto* family = structures.Coordinate != nullptr ? structures.Coordinate->FindFamily(binding.FamilyId) : nullptr)
            return family->SkillId;
        break;
    }
    return {};
}

/**
 * S04 seam, filled by R9-04: the authoritative skill for a lesson is the one growth owner's
 * Awakening.Stats.FindSkill(family skill).LevelMilli, zero when absent (an unseeded life has no skills and reads zero).
 */
std::int32_t S04LessonSkillMilli(const WorldState& world, const AssessmentStructures& structures, const SchoolLessonBinding& binding)
{
    const SkillLevel* skill = world.Awakening.Stats.FindSkill(R94FamilySkill(structures, binding));
    return skill != nullptr ? skill->LevelMilli : 0;
}

struct R94Observed
{
    AptitudeSource Source = AptitudeSource::StructureModel;
    AptitudeFacet Facet = AptitudeFacet::Analysis;
};

/** The classroom structure's own source and facet (AptitudeProjection.h: a structure evidences only its own facet). */
R94Observed R94StructureEvidence(SchoolStructure structure)
{
    switch (structure)
    {
    case SchoolStructure::Explain: return {AptitudeSource::StructureExplain, AptitudeFacet::Interpretation};
    case SchoolStructure::Investigate: return {AptitudeSource::StructureInvestigate, AptitudeFacet::FaultFinding};
    case SchoolStructure::Coordinate: return {AptitudeSource::StructureCoordinate, AptitudeFacet::Teamwork};
    default: return {AptitudeSource::StructureModel, AptitudeFacet::Analysis};
    }
}

/** A resolved lesson outcome's (family, grade index Sound/Partial/Unsound); false for no observation. */
bool R94OutcomeGrade(const SchoolLessonOutcome& outcome, std::string& family, std::size_t& grade)
{
    if (const auto* m = std::get_if<ModelOutcome>(&outcome))
    {
        family = m->FamilyId;
        grade = static_cast<std::size_t>(m->Grade);
        return true;
    }
    const StructureOutcome* o = nullptr;
    if (const auto* e = std::get_if<ExplainOutcome>(&outcome))
        o = e;
    else if (const auto* i = std::get_if<InvestigateOutcome>(&outcome))
        o = i;
    else if (const auto* c = std::get_if<CoordinateOutcome>(&outcome))
        o = c;
    else if (const auto* r = std::get_if<StructureOutcome>(&outcome))
        o = r;
    if (o == nullptr)
        return false;
    family = o->FamilyId;
    grade = static_cast<std::size_t>(o->Grade);
    return true;
}

/**
 * R9-04: the projection evidence a paid close actually observed: a completed foreground or routine lesson with a resolved
 * outcome, id "school.lesson.<activity instance>", its structure's source and facet, the family as subject, the grade as
 * result (a routine exposure is Sound by definition). Attendance-only, cancelled and unresolved lessons observe nothing.
 */
std::optional<AptitudeEvidence> R94LessonEvidence(const SchoolPeriodRecord& record, const SchoolLessonBinding& binding, GameInstant at)
{
    if (!record.Completed || record.Cancelled || record.Mode == SchoolLessonMode::Attend || record.ActivityInstance == 0)
        return std::nullopt;
    std::string family;
    std::size_t grade = 0;
    if (!R94OutcomeGrade(record.Outcome, family, grade))
        return std::nullopt;
    const R94Observed observed = R94StructureEvidence(binding.Structure);
    constexpr AptitudeResult results[] = {AptitudeResult::Sound, AptitudeResult::Partial, AptitudeResult::Unsound};
    AptitudeEvidence item;
    item.Id = "school.lesson." + std::to_string(record.ActivityInstance);
    item.Source = observed.Source;
    item.Facet = observed.Facet;
    item.Subject = std::move(family);
    item.Routine = record.Mode == SchoolLessonMode::Routine;
    item.Result = item.Routine ? AptitudeResult::Sound : grade < std::size(results) ? results[grade] : AptitudeResult::Unsound;
    item.At = at;
    return item;
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
        applied = OpenSchoolLesson(sessions, day, cats, *binding, open->Mode, open->Goal,
                                   S04LessonSkillMilli(State_, Lessons_.Structures, *binding), State_.WorldSeed, {});
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
    // R9-04: the close's completions grow the one owner exactly once, and a paid close adds the evidence it observed.
    if (State_.Awakening.Seeded && std::holds_alternative<SchoolLessonClose>(command.Payload))
    {
        AwakeningState staged = State_.Awakening;
        const AwakeningCreditsResult credited = ApplyActivityCredits(staged, applied.Events);
        if (!credited.Ok())
            return S04Refusal("school.learning_refused", credited.Error);
        for (const DayLoopEvent& event : applied.Events)
        {
            if (event.Kind != DayLoopEventKind::Completed)
                continue;
            for (const SchoolPeriodRecord& record : sessions.Periods)
            {
                if (record.ActivityInstance != event.Instance)
                    continue;
                const SchoolLessonBinding* bound = FindSchoolLessonBinding(*Lessons_.Lessons, record.BindingId);
                const std::optional<AptitudeEvidence> item =
                    bound != nullptr ? R94LessonEvidence(record, *bound, day.Now) : std::nullopt;
                if (!item)
                    continue;
                const AptitudeAddResult added = AptitudeAddEvidence(staged.ProjectionEvidence, *item);
                if (!added.Ok())
                    return S04Refusal("school.learning_refused", std::string(AptitudeErrorCode(added.Error)));
            }
        }
        State_.Awakening = std::move(staged);
    }
    State_.SchoolSessions = std::move(sessions);
    State_.DayLoop = std::move(day);
    // Completion learning leaves the reducer exactly once, as the day loop emitted it (and was credited above).
    for (const DayLoopEvent& event : applied.Events)
        result.Events.push_back({0, event.At, event});
    StagedLesson_ = std::move(applied);
    return std::nullopt;
}
} // namespace PerfectWorld
