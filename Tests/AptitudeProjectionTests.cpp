// ROADMAP M3-05: Aptitude Projection (evidence per path, naming what has not been observed, nothing to farm).
#include "AptitudeTestSupport.h"

#include <algorithm>
#include <set>

using namespace AptitudeTests;

namespace AptitudeTests
{
void RunAptitudeTextTests();
}

namespace
{
constexpr AptitudeSource Model = AptitudeSource::StructureModel;
constexpr AptitudeSource Explain = AptitudeSource::StructureExplain;
constexpr AptitudeSource Investigate = AptitudeSource::StructureInvestigate;
constexpr AptitudeSource Coordinate = AptitudeSource::StructureCoordinate;
constexpr AptitudeSource Practice = AptitudeSource::PracticeConsolidation;
constexpr AptitudeSource Activity = AptitudeSource::ActivityCompletion;
constexpr AptitudeSource Attend = AptitudeSource::Attendance;
constexpr AptitudeResult Sound = AptitudeResult::Sound;
constexpr AptitudeResult Partial = AptitudeResult::Partial;
constexpr AptitudeResult Unsound = AptitudeResult::Unsound;

bool HasDigit(const std::string& text)
{
    return std::any_of(text.begin(), text.end(), [](char c) { return c >= '0' && c <= '9'; });
}

void TestTables()
{
    std::set<std::string_view> pathIds;
    std::set<std::string_view> facetIds;
    std::set<AptitudeFacet> covered;
    for (std::size_t p = 0; p < AptitudePathCount; ++p)
    {
        const AptitudePath path = static_cast<AptitudePath>(p);
        Check(!AptitudePathId(path).empty() && !AptitudePathName(path).empty(), "a path has an ID and a name");
        pathIds.insert(AptitudePathId(path));
        const auto& facets = AptitudeFacetsForPath(path);
        const std::set<AptitudeFacet> distinct(facets.begin(), facets.end());
        Check(distinct.size() == AptitudeFacetsPerPath, "a path documents five distinct facets");
        Check(distinct.count(AptitudeFacet::Teamwork) == 1, "every path documents teamwork");
        covered.insert(facets.begin(), facets.end());
    }
    Check(pathIds.size() == AptitudePathCount, "path IDs are unique");
    for (std::size_t f = 0; f < AptitudeFacetCount; ++f)
    {
        const AptitudeFacet facet = static_cast<AptitudeFacet>(f);
        facetIds.insert(AptitudeFacetId(facet));
        Check(!AptitudeFacetName(facet).empty(), "a facet has a name");
        Check(covered.count(facet) == 1 && !AptitudePathsForFacet(facet).empty(), "every facet bears on some path");
    }
    Check(facetIds.size() == AptitudeFacetCount, "facet IDs are unique");
    Check(AptitudePathsForFacet(AptitudeFacet::Teamwork).size() == AptitudePathCount, "teamwork bears on all ten paths");
    Check(AptitudePathsForFacet(AptitudeFacet::Accounts).size() == 1, "accounts bears on enterprise alone");

    const char* const skills[] = {"skill.academics",      "skill.commerce", "skill.communication", "skill.computing",
                                  "skill.creative",       "skill.fitness",  "skill.household_tasks", "skill.logistics",
                                  "skill.practical_repair", "skill.self_management"};
    for (const char* skill : skills)
    {
        AptitudeFacet facet = AptitudeFacet::Analysis;
        Check(AptitudeFacetForSkill(skill, facet), "every skill maps to a facet");
    }
    AptitudeFacet facet = AptitudeFacet::Analysis;
    Check(!AptitudeFacetForSkill("skill.unknown", facet), "an unknown skill maps to nothing");
    Check(AptitudeFacetForSkill("skill.computing", facet) && facet == AptitudeFacet::FaultFinding, "computing is fault finding");
}

void TestEveryPathNamesWhatIsUnobserved()
{
    const AptitudeProjection projection = AptitudeProject(AptitudeLedger{});
    Check(projection.Paths.size() == AptitudePathCount, "one projection per path");
    for (std::size_t p = 0; p < AptitudePathCount; ++p)
    {
        const AptitudePathProjection& path = projection.Paths[p];
        Check(path.Path == static_cast<AptitudePath>(p), "paths are in path order");
        Check(path.Band == AptitudeBand::InsufficientObservation && path.Coverage == AptitudeCoverage::Unknown,
              "no evidence is insufficient observation, never weakness");
        Check(path.Unobserved.size() == AptitudeFacetsPerPath && path.Facets.size() == AptitudeFacetsPerPath,
              "every documented evidence kind is named as unobserved");
        Check(path.Strongest.empty(), "no evidence, no references");
        for (std::size_t i = 0; i < path.Facets.size(); ++i)
            Check(path.Facets[i].State == AptitudeFacetState::Unobserved && path.Facets[i].Facet == path.Unobserved[i],
                  "the unobserved list matches the facet states, in order");
        const std::string line = AptitudePrintoutLine(path);
        Check(line.find("This means unknown, not unsuitable.") != std::string::npos, "the printout says unknown, not unsuitable");
        Check(line.find("Unobserved:") != std::string::npos, "the printout names what is unobserved");
    }
}

void TestFarmingTheSameEvidence()
{
    // 10,000 repeats of one problem family: four slots, then nothing.
    AptitudeLedger ledger;
    AptitudeProjection afterFour;
    for (int i = 0; i < 10'000; ++i)
    {
        const AptitudeAddResult result =
            AptitudeAddEvidence(ledger, Structure("rep." + std::to_string(i), Model, "family.flow", Sound, i));
        Check(result.Ok(), "a repeat is accepted");
        if (i >= static_cast<int>(AptitudeGroupKeep))
            Check(result.Outcome == AptitudeAddOutcome::Saturated, "a repeat past saturation changes nothing");
        if (i == static_cast<int>(AptitudeGroupKeep) - 1)
            afterFour = AptitudeProject(ledger);
    }
    Check(ledger.Items.size() == AptitudeGroupKeep, "10,000 repeats occupy four slots");
    Check(AptitudeProject(ledger) == afterFour, "the projection after 10,000 repeats equals the one after four");
    const AptitudePathProjection& scholar = PathOf(afterFour, AptitudePath::Scholar);
    Check(StateOf(scholar, AptitudeFacet::Analysis) == AptitudeFacetState::Shown, "one family, however often, is shown and no more");
    Check(scholar.Band == AptitudeBand::InsufficientObservation, "one facet alone never leaves insufficient observation");

    // The same on every route: 10,000 near-identical activities spread over 300 barely-different subjects.
    AptitudeLedger spread;
    for (int i = 0; i < 10'000; ++i)
        Add(spread, Item("act." + std::to_string(i), Activity, AptitudeFacet::HandsOn, "activity.s" + std::to_string(i % 300), Sound, i));
    const AptitudeProjection spreadProjection = AptitudeProject(spread);
    for (const AptitudePathProjection& path : spreadProjection.Paths)
    {
        for (const AptitudeFacetStatement& statement : path.Facets)
            Check(statement.State != AptitudeFacetState::Established, "one source kind never establishes a facet");
        Check(path.Band != AptitudeBand::Strong, "farming one source kind never reaches strong evidence");
    }

    // Every structure, 200 subjects each, ten repeats each: still nothing established.
    AptitudeLedger structures;
    const AptitudeSource all[] = {Model, Explain, Investigate, Coordinate};
    for (int round = 0; round < 10; ++round)
    {
        for (const AptitudeSource source : all)
        {
            for (int family = 0; family < 200; ++family)
                Add(structures, Structure("s" + std::to_string(static_cast<int>(source)) + "." + std::to_string(family) + "." +
                                              std::to_string(round),
                                          source, "family.f" + std::to_string(family), Sound, round));
        }
    }
    for (const AptitudePathProjection& path : AptitudeProject(structures).Paths)
    {
        for (const AptitudeFacetStatement& statement : path.Facets)
            Check(statement.State != AptitudeFacetState::Established, "structures alone never establish a facet");
        Check(path.Band != AptitudeBand::Strong, "structures alone never reach strong evidence");
    }
}

void TestDifferentKindsMoveBands()
{
    AptitudeLedger ledger;
    Add(ledger, Structure("m1", Model, "family.flow", Sound, 1));
    AptitudeProjection projection = AptitudeProject(ledger);
    AptitudePathProjection scholar = PathOf(projection, AptitudePath::Scholar);
    Check(scholar.Band == AptitudeBand::InsufficientObservation && scholar.Coverage == AptitudeCoverage::Sparse && scholar.Unobserved.size() == 4,
          "one facet seen: insufficient observation, sparse, four unobserved");
    Check(std::find(scholar.Unobserved.begin(), scholar.Unobserved.end(), AptitudeFacet::Analysis) == scholar.Unobserved.end(),
          "analysis is no longer unobserved");

    Add(ledger, Structure("e1", Explain, "family.claim", Sound, 2));
    scholar = PathOf(AptitudeProject(ledger), AptitudePath::Scholar);
    Check(scholar.Band == AptitudeBand::Plausible && scholar.Coverage == AptitudeCoverage::Partial, "a second facet shown is plausible");
    Add(ledger, Structure("i1", Investigate, "family.fault", Sound, 3));
    Check(BandOf(ledger, AptitudePath::Scholar) == AptitudeBand::Plausible, "three facets shown, none established, is still plausible");

    // Analysis established needs two kinds of evidence: the structure (capped) and a consolidated practice.
    Add(ledger, Structure("m2", Model, "family.ratio", Sound, 4));
    Check(StateOf(PathOf(AptitudeProject(ledger), AptitudePath::Scholar), AptitudeFacet::Analysis) == AptitudeFacetState::Shown,
          "two families of one kind are still only shown");
    Add(ledger, Item("p1", Practice, AptitudeFacet::Analysis, "skill.academics", Sound, 5));
    scholar = PathOf(AptitudeProject(ledger), AptitudePath::Scholar);
    Check(StateOf(scholar, AptitudeFacet::Analysis) == AptitudeFacetState::Established && scholar.Band == AptitudeBand::Plausible,
          "a second kind of evidence establishes analysis; one established facet is not yet strong");
    Add(ledger, Structure("e2", Explain, "family.argument", Sound, 6));
    Add(ledger, Item("a1", Activity, AptitudeFacet::Interpretation, "activity.compare_reports", Sound, 7));
    scholar = PathOf(AptitudeProject(ledger), AptitudePath::Scholar);
    Check(scholar.Band == AptitudeBand::Strong, "two core facets established and a third shown is strong evidence");
    Check(scholar.Unobserved.size() == 2 && AptitudePrintoutLine(scholar).find("Unobserved: teamwork, hands-on work.") != std::string::npos,
          "strong evidence still names teamwork and hands-on work as unobserved");
    Check(scholar.Strongest.size() == AptitudeMaxReferences && scholar.Strongest[0] == "m1" &&
              scholar.Strongest[1] == "e1" && scholar.Strongest[2] == "i1",
          "the strongest evidence is named, best and earliest first");

    // Hands-on and physical evidence bear on other paths, not on research.
    Check(BandOf(ledger, AptitudePath::Athlete) == AptitudeBand::InsufficientObservation, "classroom evidence says nothing about athletics");
    AptitudeLedger repair;
    Add(repair, Item("r1", Activity, AptitudeFacet::HandsOn, "activity.bicycle", Sound, 1));
    Add(repair, Item("r2", Practice, AptitudeFacet::Analysis, "skill.academics", Sound, 2));
    Check(PathOf(AptitudeProject(repair), AptitudePath::Engineer).Band == AptitudeBand::InsufficientObservation,
          "an engineer whose fault finding was never seen is unknown, however much else is shown");
    Add(repair, Item("r3", Activity, AptitudeFacet::FaultFinding, "activity.debug", Sound, 3));
    Check(PathOf(AptitudeProject(repair), AptitudePath::Engineer).Band == AptitudeBand::Plausible,
          "fault finding and hands-on work shown give a plausible engineer");
    Check(PathOf(AptitudeProject(repair), AptitudePath::Athlete).Band == AptitudeBand::InsufficientObservation, "and nothing for the athlete");
}

void TestRoutineAttendanceAndUnsound()
{
    // A routine exposure counts as documented evidence of an established method: shown, not established, and weaker than foreground.
    AptitudeLedger routine;
    Add(routine, Structure("m1", Model, "family.flow", Sound, 1, true));
    Check(StateOf(PathOf(AptitudeProject(routine), AptitudePath::Scholar), AptitudeFacet::Analysis) == AptitudeFacetState::Shown,
          "one routine exposure is documented and shown");
    for (int i = 2; i < 200; ++i)
        Add(routine, Structure("m" + std::to_string(i), Model, "family.flow", Sound, i, true));
    Check(StateOf(PathOf(AptitudeProject(routine), AptitudePath::Scholar), AptitudeFacet::Analysis) == AptitudeFacetState::Shown &&
              routine.Items.size() == AptitudeGroupKeep,
          "routine repeats saturate like any others");

    // Attendance is steadiness and nothing else; one class repeated saturates below shown, two classes show it.
    AptitudeLedger attendance;
    for (int i = 0; i < 500; ++i)
        Add(attendance, Item("att." + std::to_string(i), Attend, AptitudeFacet::Steadiness, "class.maths", Sound, i));
    Check(StateOf(PathOf(AptitudeProject(attendance), AptitudePath::Engineer), AptitudeFacet::Steadiness) == AptitudeFacetState::Observed,
          "one class attended 500 times is observed and not yet shown");
    Add(attendance, Item("att.b", Attend, AptitudeFacet::Steadiness, "class.physics", Sound, 600));
    Add(attendance, Item("att.c", Attend, AptitudeFacet::Steadiness, "class.history", Sound, 601));
    Check(StateOf(PathOf(AptitudeProject(attendance), AptitudePath::Engineer), AptitudeFacet::Steadiness) == AptitudeFacetState::Shown,
          "three classes attended show steadiness");
    Check(BandOf(attendance, AptitudePath::Hunter) == AptitudeBand::InsufficientObservation, "attendance alone is a single facet");

    // Unsound results are observed and not demonstrated: visibly different from unobserved.
    AptitudeLedger unsound;
    Add(unsound, Structure("m1", Model, "family.flow", Unsound, 1));
    Add(unsound, Structure("e1", Explain, "family.claim", Unsound, 2));
    const AptitudePathProjection developing = PathOf(AptitudeProject(unsound), AptitudePath::Scholar);
    Check(developing.Band == AptitudeBand::Developing && AptitudeBandText(developing.Band) == "Early evidence",
          "observed but not demonstrated is its own band");
    Check(developing.Band != PathOf(AptitudeProject(AptitudeLedger{}), AptitudePath::Scholar).Band, "unobserved and weak look different");
    Check(StateOf(developing, AptitudeFacet::Analysis) == AptitudeFacetState::Observed &&
              StateOf(developing, AptitudeFacet::FaultFinding) == AptitudeFacetState::Unobserved,
          "per facet, observed and unobserved are different states");
    Check(developing.Strongest.empty(), "an unsound result is never named as support");
    Check(AptitudePrintoutLine(developing).find("unknown, not unsuitable") == std::string::npos, "an observed path is not called unknown");
}

void TestGroupActivityNarrowsUncertainty()
{
    AptitudeLedger ledger;
    Add(ledger, Structure("m1", Model, "family.flow", Sound, 1));
    Add(ledger, Structure("e1", Explain, "family.claim", Sound, 2));
    const AptitudePathProjection before = PathOf(AptitudeProject(ledger), AptitudePath::Scholar);
    Check(before.Band == AptitudeBand::Plausible && before.Unobserved.size() == 3, "the baseline is plausible with three kinds unobserved");

    Add(ledger, Structure("c1", Coordinate, "family.group", Unsound, 3));
    const AptitudePathProjection after = PathOf(AptitudeProject(ledger), AptitudePath::Scholar);
    Check(after.Band == before.Band, "a group activity that went badly does not change the band");
    Check(after.Strongest == before.Strongest, "nor the evidence behind it");
    Check(after.Unobserved.size() == 2 &&
              std::find(after.Unobserved.begin(), after.Unobserved.end(), AptitudeFacet::Teamwork) == after.Unobserved.end(),
          "but teamwork is no longer unobserved");
    Check(StateOf(after, AptitudeFacet::Teamwork) == AptitudeFacetState::Observed, "teamwork is observed, not demonstrated");
    Check(StateOf(after, AptitudeFacet::Analysis) == StateOf(before, AptitudeFacet::Analysis) &&
              StateOf(after, AptitudeFacet::Interpretation) == StateOf(before, AptitudeFacet::Interpretation),
          "no demonstrated facet moved");
    Check(after.Coverage >= before.Coverage, "coverage never narrows the wrong way");

    Add(ledger, Structure("i1", Investigate, "family.fault", Unsound, 4));
    const AptitudePathProjection again = PathOf(AptitudeProject(ledger), AptitudePath::Scholar);
    Check(again.Coverage == AptitudeCoverage::Broad && again.Coverage > before.Coverage && again.Band == before.Band,
          "a second unsound observation widens coverage and still leaves the band alone");

    // The same item, observed again as Sound, is what moves a facet from observed to shown.
    Add(ledger, Structure("c2", Coordinate, "family.group2", Sound, 5));
    Check(StateOf(PathOf(AptitudeProject(ledger), AptitudePath::Scholar), AptitudeFacet::Teamwork) == AptitudeFacetState::Shown,
          "a sound group result shows teamwork");
}

// Eight weeks, five school days a week, of a study-heavy and a repair-heavy life.
AptitudeLedger Persona(bool study)
{
    AptitudeLedger ledger;
    int serial = 0;
    const char* const classes[] = {"class.maths", "class.physics", "class.language", "class.history", "class.workshop"};
    for (int week = 0; week < 8; ++week)
    {
        for (int day = 0; day < 5; ++day)
        {
            const GameInstant at = (week * 7 + day) * MinutesPerDay;
            const std::string tag = "." + std::to_string(serial++);
            Add(ledger, Item("att" + tag, Attend, AptitudeFacet::Steadiness, classes[day], Sound, at));
            if (study)
            {
                Add(ledger, Structure("m" + tag, Model, day % 2 == 0 ? "family.flow" : "family.ratio", day == 4 ? Partial : Sound, at));
                Add(ledger, Structure("e" + tag, Explain, day % 2 == 0 ? "family.claim" : "family.argument", Partial, at));
                Add(ledger, Item("a" + tag, Activity, AptitudeFacet::Interpretation, "activity.compare_reports", Sound, at));
            }
            else
            {
                Add(ledger, Structure("i" + tag, Investigate, day % 2 == 0 ? "family.fault" : "family.circuit", Sound, at));
                Add(ledger, Item("a" + tag, Activity, AptitudeFacet::HandsOn, "activity.s" + std::to_string(day % 3), Sound, at));
                Add(ledger, Item("d" + tag, Activity, AptitudeFacet::FaultFinding, "activity.debug", Sound, at));
            }
        }
        if (study)
            Add(ledger, Item("pa." + std::to_string(week), Practice, AptitudeFacet::Analysis, "skill.academics", Sound, week * 7 * MinutesPerDay));
        else
            Add(ledger, Item("pr." + std::to_string(week), Practice, AptitudeFacet::HandsOn, "skill.practical_repair", Sound, week * 7 * MinutesPerDay));
    }
    Add(ledger, Structure("c.week3", Coordinate, "family.project", Partial, 3 * 7 * MinutesPerDay));
    return ledger;
}

void TestPersonasDiverge()
{
    const AptitudeProjection study = AptitudeProject(Persona(true));
    const AptitudeProjection repair = AptitudeProject(Persona(false));
    Check(!(study == repair), "the two lives project differently");
    Check(PathOf(study, AptitudePath::Scholar).Band == AptitudeBand::Strong, "the study-heavy life is strong evidence for research");
    Check(PathOf(repair, AptitudePath::Scholar).Band == AptitudeBand::InsufficientObservation,
          "the repair-heavy life never touched analysis: research is unknown, not weak");
    Check(PathOf(repair, AptitudePath::Engineer).Band == AptitudeBand::Strong, "the repair-heavy life is strong evidence for engineering");
    Check(PathOf(study, AptitudePath::Engineer).Band == AptitudeBand::InsufficientObservation,
          "the study-heavy life never touched fault finding: engineering is unknown, not weak");
    Check(PathOf(study, AptitudePath::Clinician).Unobserved != PathOf(repair, AptitudePath::Clinician).Unobserved,
          "what is unobserved differs between the two lives");
    Check(PathOf(study, AptitudePath::Athlete).Band == AptitudeBand::InsufficientObservation &&
              PathOf(repair, AptitudePath::Athlete).Band == AptitudeBand::InsufficientObservation,
          "athletics is unknown for both");
    Check(AptitudeProject(Persona(true)) == study && AptitudeProject(Persona(false)) == repair, "the projection is deterministic");
}

void TestLedgerRules()
{
    AptitudeLedger ledger;
    const AptitudeEvidence good = Structure("g1", Model, "family.flow", Sound, 5);
    Check(AptitudeAddEvidence(ledger, good).Outcome == AptitudeAddOutcome::Added, "a first item is added");
    Check(AptitudeAddEvidence(ledger, good).Outcome == AptitudeAddOutcome::AlreadyKept && ledger.Items.size() == 1,
          "the same item twice is one item");

    const AptitudeEvidence bad[] = {
        Item("", Practice, AptitudeFacet::HandsOn, "skill.x"),
        Item("Upper", Practice, AptitudeFacet::HandsOn, "skill.x"),
        Item("ok", Practice, AptitudeFacet::HandsOn, ""),
        Item("ok", Practice, AptitudeFacet::HandsOn, "has space"),
        Item("ok", Model, AptitudeFacet::HandsOn, "family.flow"),
        Item("ok", Explain, AptitudeFacet::Analysis, "family.flow"),
        Item("ok", Model, AptitudeFacet::Analysis, "family.flow", Partial, 0, true),
        Item("ok", Practice, AptitudeFacet::HandsOn, "skill.x", Sound, 0, true),
        Item("ok", Attend, AptitudeFacet::HandsOn, "class.x"),
        Item("ok", Attend, AptitudeFacet::Steadiness, "class.x", Unsound),
        Item("ok", Practice, AptitudeFacet::HandsOn, "skill.x", Unsound),
        Item("ok", Activity, AptitudeFacet::HandsOn, "activity.x", Sound, -1),
        Item(std::string(AptitudeMaxIdLength + 1, 'a'), Activity, AptitudeFacet::HandsOn, "activity.x"),
    };
    for (const AptitudeEvidence& item : bad)
    {
        const AptitudeLedger before = ledger;
        const AptitudeAddResult result = AptitudeAddEvidence(ledger, item);
        Check(result.Error == AptitudeError::InvalidEvidence && !result.Ok() && ledger == before, "an invalid item is refused and changes nothing");
        Check(AptitudeErrorCode(result.Error) == "aptitude.invalid_evidence", "the refusal has a stable code");
    }
    Check(AptitudeErrorCode(AptitudeError::None).empty() && AptitudeErrorCode(AptitudeError::TooMany) == "aptitude.too_many" &&
              AptitudeErrorCode(AptitudeError::InvalidLedger) == "aptitude.invalid_ledger",
          "every error has a code");
    Check(IsAptitudeEvidenceValid(Item("ok", Activity, AptitudeFacet::Creative, "activity.sketch", Unsound)), "an activity may be unsound");

    AptitudeLedger broken;
    broken.Items.push_back(Item("b", Activity, AptitudeFacet::HandsOn, "activity.x"));
    broken.Items.push_back(Item("a", Activity, AptitudeFacet::HandsOn, "activity.y"));
    Check(!IsAptitudeLedgerValid(broken) && AptitudeAddEvidence(broken, good).Error == AptitudeError::InvalidLedger,
          "an unsorted ledger is refused");
    Check(AptitudeProject(broken).Paths.empty() && SerialiseAptitude(broken).empty(), "an inconsistent ledger projects and saves as nothing");

    // Quality wins within a group, and an ID reused with other content keeps the better copy whatever the order.
    AptitudeLedger forward;
    AptitudeLedger backward;
    const AptitudeEvidence weaker = Item("same", Activity, AptitudeFacet::HandsOn, "activity.x", Partial, 9);
    const AptitudeEvidence stronger = Item("same", Activity, AptitudeFacet::HandsOn, "activity.x", Sound, 9);
    Add(forward, weaker);
    Add(forward, stronger);
    Add(backward, stronger);
    Check(AptitudeAddEvidence(backward, weaker).Outcome == AptitudeAddOutcome::AlreadyKept, "the weaker copy of an ID is not kept");
    Check(forward == backward && forward.Items.size() == 1 && forward.Items[0].Result == Sound, "an ID reused keeps the better copy in either order");

    // A better item displaces the group's worst once the group is full.
    AptitudeLedger full;
    for (int i = 0; i < 4; ++i)
        Add(full, Item("w" + std::to_string(i), Activity, AptitudeFacet::HandsOn, "activity.x", Partial, i));
    Check(AptitudeAddEvidence(full, Item("w9", Activity, AptitudeFacet::HandsOn, "activity.x", Sound, 9)).Outcome == AptitudeAddOutcome::Added &&
              full.Items.size() == AptitudeGroupKeep,
          "a better item replaces the worst of a full group");
    Check(AptitudeAddEvidence(full, Item("w8", Activity, AptitudeFacet::HandsOn, "activity.x", Partial, 99)).Outcome == AptitudeAddOutcome::Saturated,
          "a worse item is saturated");

    // The ledger is bounded: distinct groups cannot grow it without limit.
    AptitudeLedger large;
    for (std::size_t i = 0; i < AptitudeMaxItems; ++i)
        Add(large, Item("l." + std::to_string(i), Activity, AptitudeFacet::HandsOn, "activity.s" + std::to_string(i), Sound, 0));
    const AptitudeAddResult overflow = AptitudeAddEvidence(large, Item("l.over", Activity, AptitudeFacet::HandsOn, "activity.over"));
    Check(overflow.Error == AptitudeError::TooMany && large.Items.size() == AptitudeMaxItems, "past the bound a new group is refused");
    Check(AptitudeAddEvidence(large, Item("l.same", Activity, AptitudeFacet::HandsOn, "activity.s0", Sound, 1)).Error == AptitudeError::TooMany,
          "a full ledger takes nothing that would grow it");
    Check(AptitudeAddEvidence(large, large.Items[5]).Outcome == AptitudeAddOutcome::AlreadyKept, "but a repeat is still recognised");
}

void TestNoRawNumbersInPublicText()
{
    Rng rng{20260310};
    for (int round = 0; round < 40; ++round)
    {
        AptitudeLedger ledger;
        for (int i = 0; i < 60; ++i)
            AptitudeAddEvidence(ledger, RandomItem(rng, static_cast<std::uint64_t>(round * 100 + i)));
        for (const AptitudePathProjection& path : AptitudeProject(ledger).Paths)
        {
            Check(!HasDigit(AptitudePrintoutLine(path)), "the printout line contains no digit");
            Check(!HasDigit(std::string(AptitudeBandText(path.Band))) && !HasDigit(std::string(AptitudeCoverageText(path.Coverage))),
                  "band and coverage text contain no digit");
            for (const AptitudeFacetStatement& statement : path.Facets)
                Check(!HasDigit(std::string(AptitudeFacetStateText(statement.State))) && !HasDigit(std::string(AptitudeFacetName(statement.Facet))),
                      "facet text contains no digit");
        }
    }
    // The public types carry no integer or floating member; this fails to compile if one is added.
    static_assert(sizeof(AptitudePathProjection::Band) == 1 && sizeof(AptitudePathProjection::Coverage) == 1,
                  "the band and coverage are one-byte enums, not scores");
}
} // namespace

int main()
{
    TestTables();
    TestEveryPathNamesWhatIsUnobserved();
    TestFarmingTheSameEvidence();
    TestDifferentKindsMoveBands();
    TestRoutineAttendanceAndUnsound();
    TestGroupActivityNarrowsUncertainty();
    TestPersonasDiverge();
    TestLedgerRules();
    TestNoRawNumbersInPublicText();
    RunAptitudeTextTests();
    std::cout << "AptitudeProjectionTests passed (" << Checks << " checks)\n";
    return 0;
}
