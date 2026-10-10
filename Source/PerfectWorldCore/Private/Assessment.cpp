#include "Assessment.h"

#include "StructureInternal.h"

#include <algorithm>
#include <span>
#include <tuple>

// The assessment catalogue, the state's validity rule, beginning, cancelling, handing in
// and the survey (ROADMAP M3-07). The rules are documented in Public/Assessment.h; answering
// and marking are in AssessmentRun.cpp and the save text in AssessmentText.cpp.
namespace PerfectWorld
{
using namespace StructureInternal;

namespace
{
// ---- Native failure bits to the four kinds -----------------------------------------

struct AssessmentKindRow
{
    std::uint8_t Native;
    std::uint8_t Kind;
};

constexpr std::array<AssessmentKindRow, 4> AssessmentModelRows{{{ModelFailWrongModel, AssessmentFailWrongModel},
                                                                {ModelFailUnsupported, AssessmentFailUnsupported},
                                                                {ModelFailWastedTime, AssessmentFailWastedTime},
                                                                {ModelFailUnclear, AssessmentFailUnclear}}};
constexpr std::array<AssessmentKindRow, 4> AssessmentExplainRows{{{ExplainFailIndefensible, AssessmentFailWrongModel},
                                                                  {ExplainFailUnsupported, AssessmentFailUnsupported},
                                                                  {ExplainFailPadding, AssessmentFailWastedTime},
                                                                  {ExplainFailOverreach, AssessmentFailUnclear}}};
constexpr std::array<AssessmentKindRow, 4> AssessmentInvestigateRows{{{InvestigateFailWrongCause, AssessmentFailWrongModel},
                                                                      {InvestigateFailUnsupported, AssessmentFailUnsupported},
                                                                      {InvestigateFailRedundant, AssessmentFailWastedTime},
                                                                      {InvestigateFailAbandoned, AssessmentFailWastedTime}}};
constexpr std::array<AssessmentKindRow, 6> AssessmentCoordinateRows{{{CoordinateFailOverloaded, AssessmentFailWrongModel},
                                                                     {CoordinateFailMismatched, AssessmentFailWrongModel},
                                                                     {CoordinateFailUninformed, AssessmentFailUnsupported},
                                                                     {CoordinateFailUnsettled, AssessmentFailUnsupported},
                                                                     {CoordinateFailWasted, AssessmentFailWastedTime},
                                                                     {CoordinateFailSeam, AssessmentFailUnclear}}};

std::uint8_t AssessmentMapRows(std::span<const AssessmentKindRow> rows, std::uint8_t native) noexcept
{
    std::uint8_t kinds = 0;
    for (const AssessmentKindRow& row : rows)
    {
        if ((native & row.Native) != 0)
            kinds = static_cast<std::uint8_t>(kinds | row.Kind);
    }
    return kinds;
}

// ---- Loading --------------------------------------------------------------------------

/** What every structure's case def offers marking: the family, the name and the method threshold. */
template <typename CaseDef>
void AssessmentFillFromCase(AssessmentQuestion& question, const CaseDef& def, std::string_view skillId)
{
    question.Name = def.Name;
    question.FamilyId = def.FamilyId;
    question.SkillId = std::string(skillId);
    question.MethodKnownAtMilli = def.MethodKnownAtMilli;
}

std::string AssessmentResolveQuestion(const AssessmentStructures& structures, AssessmentQuestion& question)
{
    const std::string missing = "the case " + question.CaseId + " is not a " + std::string(AssessmentStructureNames[static_cast<std::size_t>(question.Structure)]) +
                                " case in its catalogue";
    switch (question.Structure)
    {
    case AssessmentStructure::Model:
    {
        const ModelCaseDef* def = structures.Model != nullptr ? structures.Model->FindCase(question.CaseId) : nullptr;
        const ModelFamilyDef* family = def != nullptr ? structures.Model->FindFamily(def->FamilyId) : nullptr;
        if (def == nullptr || family == nullptr)
            return missing;
        AssessmentFillFromCase(question, *def, family->SkillId);
        question.Trap = def->Fault != ModelFault::None;
        question.Critic = def->Role == ModelRole::Critic;
        return {};
    }
    case AssessmentStructure::Explain:
    {
        const ExplainCaseDef* def = structures.Explain != nullptr ? structures.Explain->FindCase(question.CaseId) : nullptr;
        const ExplainFamilyDef* family = def != nullptr ? structures.Explain->FindFamily(def->FamilyId) : nullptr;
        if (def == nullptr || family == nullptr)
            return missing;
        AssessmentFillFromCase(question, *def, family->SkillId);
        question.Trap = true;
        question.Critic = def->Role == ExplainRole::Critic;
        return {};
    }
    case AssessmentStructure::Investigate:
    {
        const InvestigateCaseDef* def = structures.Investigate != nullptr ? structures.Investigate->FindCase(question.CaseId) : nullptr;
        const InvestigateFamilyDef* family = def != nullptr ? structures.Investigate->FindFamily(def->FamilyId) : nullptr;
        if (def == nullptr || family == nullptr)
            return missing;
        AssessmentFillFromCase(question, *def, family->SkillId);
        question.Trap = def->Symptom == InvestigateSymptom::Intermittent;
        question.Critic = def->Role == InvestigateRole::Critic;
        return {};
    }
    case AssessmentStructure::Coordinate:
    {
        const CoordinateCaseDef* def = structures.Coordinate != nullptr ? structures.Coordinate->FindCase(question.CaseId) : nullptr;
        const CoordinateFamilyDef* family = def != nullptr ? structures.Coordinate->FindFamily(def->FamilyId) : nullptr;
        if (def == nullptr || family == nullptr)
            return missing;
        AssessmentFillFromCase(question, *def, family->SkillId);
        question.Trap = def->Seam;
        question.Critic = false;
        return {};
    }
    }
    return missing;
}

std::string AssessmentLoadPaper(const ContentRecord& record, const AssessmentStructures& structures, AssessmentPaperDef& paper)
{
    paper.Id = record.Id;
    paper.Name = JsonString(&record.Data, "name");
    paper.CalendarId = JsonString(&record.Data, "calendar");
    paper.WindowId = JsonString(&record.Data, "window");
    std::int32_t minutes = 0;
    if (!JsonInt32(&record.Data, "units", paper.Units) || !JsonInt32(&record.Data, "minutes", minutes))
        return "units and minutes must be whole numbers";
    if (paper.Units != AssessmentPaperUnits || minutes != AssessmentPaperMinutes)
        return "a paper is " + std::to_string(AssessmentPaperMinutes) + " minutes: " + std::to_string(AssessmentPaperUnits) + " units of " +
               std::to_string(AssessmentUnitMinutes) + " minutes";
    const std::vector<JsonValue>& groups = JsonItems(&record.Data, "groups");
    if (groups.size() != AssessmentGroupCount)
        return "a paper has " + std::to_string(AssessmentGroupCount) + " question groups, found " + std::to_string(groups.size());
    for (const JsonValue& entry : groups)
    {
        AssessmentQuestion question;
        question.GroupId = JsonString(&entry, "id");
        question.CaseId = JsonString(&entry, "case");
        if (!ModelIdWellFormed(question.GroupId) || !ModelIdWellFormed(question.CaseId) || !JsonInt32(&entry, "marks", question.Marks) ||
            !ModelParseName(AssessmentStructureNames, JsonString(&entry, "structure"), question.Structure))
            return "a group needs a well-formed id and case, marks and a known structure";
        if (question.Marks != AssessmentGroupMarks)
            return "group " + question.GroupId + " is worth " + std::to_string(question.Marks) + " marks, not " + std::to_string(AssessmentGroupMarks);
        if (std::any_of(paper.Questions.begin(), paper.Questions.end(), [&](const AssessmentQuestion& q) { return q.GroupId == question.GroupId; }))
            return "the group id " + question.GroupId + " is used twice";
        if (const std::string error = AssessmentResolveQuestion(structures, question); !error.empty())
            return error;
        paper.Questions.push_back(std::move(question));
    }
    return {};
}

std::int32_t AssessmentSkillOf(const AssessmentSkills& skills, std::string_view skillId) noexcept
{
    for (const AssessmentSkillLevel& level : skills)
    {
        if (level.SkillId == skillId)
            return level.Milli;
    }
    return 0;
}

bool AssessmentSkillsValid(const AssessmentSkills& skills) noexcept
{
    for (std::size_t i = 0; i < skills.size(); ++i)
    {
        if (skills[i].Milli < 0 || skills[i].SkillId.empty())
            return false;
        for (std::size_t j = 0; j < i; ++j)
        {
            if (skills[j].SkillId == skills[i].SkillId)
                return false;
        }
    }
    return true;
}

bool AssessmentPaperFitsState(const AssessmentPaperDef& paper, const AssessmentState& state) noexcept
{
    return !state.PaperId.empty() && paper.Id == state.PaperId && paper.Questions.size() == state.Groups.size();
}

/** None while the paper can be worked on, else why not. */
AssessmentError AssessmentActiveError(const AssessmentState& state) noexcept
{
    if (!IsAssessmentStateValid(state))
        return AssessmentError::InvalidState;
    if (state.PaperId.empty())
        return AssessmentError::NotActive;
    return state.Finished ? AssessmentError::Finished : AssessmentError::None;
}
} // namespace

std::uint8_t AssessmentKindsOf(AssessmentStructure structure, std::uint8_t nativeFailures) noexcept
{
    switch (structure)
    {
    case AssessmentStructure::Model:
        return AssessmentMapRows(AssessmentModelRows, nativeFailures);
    case AssessmentStructure::Explain:
        return AssessmentMapRows(AssessmentExplainRows, nativeFailures);
    case AssessmentStructure::Investigate:
        return AssessmentMapRows(AssessmentInvestigateRows, nativeFailures);
    case AssessmentStructure::Coordinate:
        return AssessmentMapRows(AssessmentCoordinateRows, nativeFailures);
    }
    return 0;
}

std::string_view AssessmentErrorCode(AssessmentError error) noexcept
{
    switch (error)
    {
    case AssessmentError::None:
        return {};
    case AssessmentError::UnknownPaper:
        return "assessment.unknown_paper";
    case AssessmentError::Busy:
        return "assessment.busy";
    case AssessmentError::NotActive:
        return "assessment.not_active";
    case AssessmentError::Finished:
        return "assessment.finished";
    case AssessmentError::NotFinished:
        return "assessment.not_finished";
    case AssessmentError::AlreadySurveyed:
        return "assessment.already_surveyed";
    case AssessmentError::UnknownGroup:
        return "assessment.unknown_group";
    case AssessmentError::AlreadyAnswered:
        return "assessment.already_answered";
    case AssessmentError::BadChoice:
        return "assessment.bad_choice";
    case AssessmentError::UnitsShort:
        return "assessment.units_short";
    case AssessmentError::AllocationShort:
        return "assessment.allocation_short";
    case AssessmentError::BadAllocation:
        return "assessment.bad_allocation";
    case AssessmentError::InvalidSkill:
        return "assessment.invalid_skill";
    case AssessmentError::InvalidState:
        return "assessment.invalid_state";
    }
    return "assessment.invalid_state";
}

const AssessmentPaperDef* AssessmentCatalogue::FindPaper(std::string_view id) const noexcept
{
    const auto it = std::lower_bound(Papers.begin(), Papers.end(), id,
                                     [](const AssessmentPaperDef& paper, std::string_view key) { return paper.Id < key; });
    return it != Papers.end() && it->Id == id ? &*it : nullptr;
}

AssessmentCatalogueResult LoadAssessmentCatalogue(const ContentDb& content, const AssessmentStructures& structures)
{
    AssessmentCatalogueResult result;
    for (const ContentRecord* record : content.OfType("assessment_paper"))
    {
        AssessmentPaperDef paper;
        if (const std::string error = AssessmentLoadPaper(*record, structures, paper); !error.empty())
        {
            result.Error = record->Id + ": " + error;
            result.Catalogue.Papers.clear();
            return result;
        }
        result.Catalogue.Papers.push_back(std::move(paper));
    }
    std::sort(result.Catalogue.Papers.begin(), result.Catalogue.Papers.end(),
              [](const AssessmentPaperDef& a, const AssessmentPaperDef& b) { return a.Id < b.Id; });
    return result;
}

// ---- State ----------------------------------------------------------------------------

std::int32_t AssessmentAnswerUnits(bool methodKnown, AssessmentMode mode, bool confirm) noexcept
{
    const std::int32_t clearing = mode == AssessmentMode::Guess ? AssessmentGuessUnits
                                                                : (methodKnown ? AssessmentClearKnownUnits : AssessmentClearDeriveUnits);
    return clearing + (confirm ? AssessmentVerifyUnits : 0);
}

std::int32_t AssessmentUnallocated(const AssessmentState& state) noexcept
{
    std::int32_t free = state.UnitsLeft;
    for (const AssessmentGroupState& group : state.Groups)
    {
        if (!group.Answered)
            free -= group.Allocated;
    }
    return free;
}

AssessmentFamiliarity AssessmentFamiliarityOf(const AssessmentGroupState& group) noexcept
{
    if (group.Known)
        return AssessmentFamiliarity::Practised;
    return group.Shaky ? AssessmentFamiliarity::Shaky : AssessmentFamiliarity::Unfamiliar;
}

std::pair<std::int32_t, std::int32_t> AssessmentEstimate(AssessmentFamiliarity familiarity) noexcept
{
    switch (familiarity)
    {
    case AssessmentFamiliarity::Practised:
        return {AssessmentClearKnownUnits, AssessmentClearKnownUnits};
    case AssessmentFamiliarity::Shaky:
        return {AssessmentClearKnownUnits, AssessmentClearDeriveUnits};
    case AssessmentFamiliarity::Unfamiliar:
        break;
    }
    return {AssessmentClearDeriveUnits, AssessmentClearDeriveUnits};
}

bool IsAssessmentStateValid(const AssessmentState& state)
{
    if (state.PaperId.empty())
        return state == AssessmentState{};
    if (!ModelIdWellFormed(state.PaperId) || state.Groups.size() != AssessmentGroupCount)
        return false;
    std::int32_t spent = state.Surveyed ? AssessmentSurveyUnits : 0;
    std::int32_t promised = 0;
    std::size_t answered = 0;
    for (const AssessmentGroupState& group : state.Groups)
    {
        if ((group.Known && group.Shaky) || group.Allocated < 0 || group.Allocated > AssessmentPaperUnits ||
            static_cast<std::size_t>(group.Mode) >= AssessmentModeNames.size())
            return false;
        if (group.Answered)
        {
            if (group.Spent != AssessmentAnswerUnits(group.Known, group.Mode, group.Confirm))
                return false;
            ++answered;
        }
        else
        {
            if (group.Spent != 0 || group.Mode != AssessmentMode::Work || group.Confirm || group.Hedge)
                return false;
            promised += group.Allocated;
        }
        spent += group.Spent;
    }
    if (state.UnitsLeft != AssessmentPaperUnits - spent || state.UnitsLeft < 0 || promised > state.UnitsLeft)
        return false;
    // The last answer hands the paper in, so a paper with every group answered is always finished.
    return answered < AssessmentGroupCount || state.Finished;
}

AssessmentError BeginAssessment(AssessmentState& state, const AssessmentPaperDef& paper, const AssessmentSkills& skills)
{
    if (state.Active())
        return AssessmentError::Busy;
    if (!IsAssessmentStateValid(state))
        return AssessmentError::InvalidState;
    if (!AssessmentSkillsValid(skills))
        return AssessmentError::InvalidSkill;
    if (!ModelIdWellFormed(paper.Id) || paper.Units != AssessmentPaperUnits || paper.Questions.size() != AssessmentGroupCount)
        return AssessmentError::UnknownPaper;

    AssessmentState next;
    next.PaperId = paper.Id;
    next.UnitsLeft = paper.Units;
    for (const AssessmentQuestion& question : paper.Questions)
    {
        const std::int32_t skill = AssessmentSkillOf(skills, question.SkillId);
        AssessmentGroupState group;
        group.Known = skill >= question.MethodKnownAtMilli;
        group.Shaky = !group.Known && skill + AssessmentShakyMilli >= question.MethodKnownAtMilli;
        next.Groups.push_back(group);
    }
    state = std::move(next);
    return AssessmentError::None;
}

AssessmentError CancelAssessment(AssessmentState& state)
{
    if (state.PaperId.empty())
        return IsAssessmentStateValid(state) ? AssessmentError::NotActive : AssessmentError::InvalidState;
    state = AssessmentState{};
    return AssessmentError::None;
}

AssessmentError FinishAssessment(AssessmentState& state)
{
    const AssessmentError error = AssessmentActiveError(state);
    if (error != AssessmentError::None)
        return error;
    state.Finished = true;
    return AssessmentError::None;
}

// ---- Surveying ------------------------------------------------------------------------

AssessmentQuote QuoteAssessmentSurvey(const AssessmentState& state)
{
    AssessmentQuote quote;
    quote.Error = AssessmentActiveError(state);
    quote.Units = AssessmentSurveyUnits;
    quote.UnitsAfter = state.UnitsLeft - quote.Units;
    if (quote.Error == AssessmentError::None && state.Surveyed)
        quote.Error = AssessmentError::AlreadySurveyed;
    if (quote.Error == AssessmentError::None && AssessmentUnallocated(state) < quote.Units)
        quote.Error = AssessmentError::UnitsShort;
    return quote;
}

std::vector<AssessmentSurveyLine> AssessmentSurveyLines(const AssessmentState& state, const AssessmentPaperDef& paper)
{
    std::vector<AssessmentSurveyLine> lines;
    if (!state.Surveyed || !AssessmentPaperFitsState(paper, state))
        return lines;
    for (std::size_t i = 0; i < paper.Questions.size(); ++i)
    {
        const AssessmentQuestion& question = paper.Questions[i];
        AssessmentSurveyLine line;
        line.Group = i;
        line.GroupId = question.GroupId;
        line.Marks = question.Marks;
        line.Structure = question.Structure;
        line.Familiarity = AssessmentFamiliarityOf(state.Groups[i]);
        std::tie(line.EstimateLow, line.EstimateHigh) = AssessmentEstimate(line.Familiarity);
        lines.push_back(std::move(line));
    }
    return lines;
}

AssessmentSurveyResult SurveyAssessment(AssessmentState& state, const AssessmentPaperDef& paper)
{
    AssessmentSurveyResult result;
    const AssessmentQuote quote = QuoteAssessmentSurvey(state);
    if (!quote.Ok())
    {
        result.Error = quote.Error;
        return result;
    }
    if (!AssessmentPaperFitsState(paper, state))
    {
        result.Error = AssessmentError::UnknownPaper;
        return result;
    }
    state.UnitsLeft = quote.UnitsAfter;
    state.Surveyed = true;
    result.Charged = quote.Units;
    result.Lines = AssessmentSurveyLines(state, paper);
    return result;
}
} // namespace PerfectWorld
