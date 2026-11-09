#pragma once

// P3-05: what the live school bridges may honestly say about the player's promises and about practice.
// Pure readers over a WorldState and an ActivityCatalogue (no UObject, no command): the planner's known commitments and the study
// desk's practice offer are both built here, so a test can run them over a hand-made world without a map.
//
// Known commitments are only what WorldState records with an authored window: the morning plan's items that are not yet done (read
// through the core's own MakeQuestLog, which matches plan items to receipts), and the activity the player has committed to or is
// running. Nothing else has both a start and a length in the world (see P3-05_notes.md for the gaps), so nothing else is invented.

#include "CoreMinimal.h"

#include "Activities.h"
#include "Assessment.h"
#include "DayLoop.h"
#include "OpportunityReceipts.h"
#include "SchoolSessions.h"
#include "StructureModelCatalogue.h"
#include "UI/SPWStudyDesk.h"
#include "WorldState.h"

namespace PWSchoolLive
{
/** The study-desk practice the core permits: the one activity the A5 handoff names. */
inline constexpr std::string_view P35StudyActivityId = "activity.study_at_home";
inline constexpr std::string_view P35StudyOfferPlace = "home";

/** Receipt candidates for the planner (and for a practice commit), and a readable line for each by candidate id. */
struct FP35Known
{
    std::vector<PerfectWorld::ReceiptCandidate> Candidates;
    /** Candidate id to "Name (what it is; what it is worth and costs)". */
    TMap<FString, FText> Lines;
};

/** The player's agreed, still-open commitments with a window. Empty when there are none; never a guess. */
FP35Known P35KnownCommitments(const PerfectWorld::WorldState& State, const PerfectWorld::ActivityCatalogue& Activities);

/** Why practice cannot start now, in words; empty text when it can. */
struct FP35PracticeStatus
{
    /** Null when the catalogue has no study activity: then there is no offer at all. */
    const PerfectWorld::ActivityDef* Def = nullptr;
    FText BlockedReason;
    /** The core's own forecast at this instant (learning, condition cost, label). Meaningful only when Ok. */
    PerfectWorld::DayActivityForecast Forecast;
    bool CanStart() const { return Def != nullptr && BlockedReason.IsEmpty(); }
};

FP35PracticeStatus P35ReadPractice(const PerfectWorld::WorldState& State, const PerfectWorld::ActivityCatalogue* Activities);

/** Zero offers when the catalogue cannot supply the study activity; otherwise one offer with its costs, available or blocked with the reason. */
TArray<PWA5::FPracticeOffer> P35PracticeOffers(const PerfectWorld::WorldState& State, const PerfectWorld::ActivityCatalogue* Activities);

/** The known candidates that overlap a practice window starting now; what a commit passes as its alternatives (at most the day loop's cap). */
std::vector<PerfectWorld::ReceiptCandidate> P35PracticeAlternatives(const PerfectWorld::WorldState& State,
    const PerfectWorld::ActivityCatalogue& Activities, const PerfectWorld::ActivityDef& Def);

/** The public context of one question group of a paper: its name, and the authored brief when the core's case has one. Never Trap or marking. Empty when unknown. */
FText P35GroupContext(const PerfectWorld::AssessmentCatalogue& Papers, const PerfectWorld::ModelCatalogue& Models, std::string_view PaperId, std::size_t Group);
} // namespace PWSchoolLive
