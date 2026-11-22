#pragma once

#include "SchoolFollowupContent.h"
#include "TowerRunnerContent.h"
#include "WorldExam.h"

#include <utility>

namespace P406Exam
{
/** Immutable after publication; the game keeps this owner alive for every borrowing sim. */
struct P406BExaminationContent
{
    PerfectWorld::TowerRunnerCatalogue Runners;
    PerfectWorld::SchoolFollowupContent Followups;
    std::vector<PerfectWorld::ContentIssue> Issues;

    explicit P406BExaminationContent(const PerfectWorld::ContentDb* P406BDb)
    {
        if (P406BDb == nullptr)
        {
            Issues.push_back({{}, {}, "examination content database unavailable"});
            return;
        }
        auto P406BLoaded = PerfectWorld::LoadTowerRunners(*P406BDb);
        auto P406BActions = PerfectWorld::LoadSchoolFollowupContent(*P406BDb);
        Issues = std::move(P406BLoaded.Issues);
        Issues.insert(Issues.end(), P406BActions.Issues.begin(), P406BActions.Issues.end());
        if (P406BLoaded.Catalogue.Scenarios.empty() || P406BActions.Catalogue.Actions.empty())
            Issues.push_back({{}, {}, "examination catalogue is empty"});
        if (!Issues.empty()) return;
        Runners = std::move(P406BLoaded.Catalogue);
        Followups = std::move(P406BActions.Catalogue);
    }

    [[nodiscard]] bool Ok() const { return Issues.empty(); }
    [[nodiscard]] PerfectWorld::P402WorldExamCatalogues P406BBinding() const
    {
        return Ok() ? PerfectWorld::P402WorldExamCatalogues{&Runners, &Followups}
                    : PerfectWorld::P402WorldExamCatalogues{};
    }
};

/** A pending choice holds time even when a menu or another panel owns input. */
template <typename P406BPresent>
bool P406BPresentDecision(bool P406BPending, bool P406BMenuOpen, bool P406BPanelOpen, P406BPresent&& P406BShow)
{
    if (!P406BPending) return false;
    if (!P406BMenuOpen && !P406BPanelOpen) P406BShow();
    return true;
}

/** Release the panel before the pause host acquires input. No participation command is submitted. */
template <typename P406BClose, typename P406BPause>
void P406BLeaveExamination(bool P406BPending, P406BClose&& P406BRelease, P406BPause&& P406BOpenPause)
{
    P406BRelease();
    if (P406BPending) P406BOpenPause();
}
}
