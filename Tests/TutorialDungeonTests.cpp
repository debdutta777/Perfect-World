#include "TutorialDungeon.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <set>
#include <sstream>
#include <utility>

using namespace PerfectWorld;

namespace
{
int R92Assertions = 0;

void R92Expect(bool condition, const char* message)
{
    ++R92Assertions;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::vector<ContentFile> R92ReadContent(const std::filesystem::path& root)
{
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            paths.push_back(entry.path());
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const auto& path : paths)
    {
        std::ifstream input(path, std::ios::binary);
        std::ostringstream text;
        text << input.rdbuf();
        files.push_back({"/" + std::filesystem::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

AwakeningState R92Seed(const ContentDb& content)
{
    const auto seeded = SeedAwakeningInputs(content, CharacterCreationSave{}, 9202);
    R92Expect(seeded.Ok(), "R9-01 seeds the actual latent owner");
    return seeded.State;
}

void R92Same(const AwakeningState& a, const AwakeningState& b)
{
    R92Expect(a.Seeded == b.Seeded && a.WorldSeed == b.WorldSeed && a.Stats == b.Stats &&
        a.AttributeEvidenceMilli == b.AttributeEvidenceMilli && a.ProjectionEvidence == b.ProjectionEvidence &&
        a.AppliedActivityInstances == b.AppliedActivityInstances && a.Conduct == b.Conduct &&
        a.AppointmentAt == b.AppointmentAt && a.ExaminationAt == b.ExaminationAt &&
        a.MakeUp.Phase == b.MakeUp.Phase && a.MakeUp.At == b.MakeUp.At &&
        a.Granted.has_value() == b.Granted.has_value(), "authoritative inputs, dates and conduct agree");
    if (!a.Granted) return;
    const auto& x = *a.Granted;
    const auto& y = *b.Granted;
    R92Expect(x.At == y.At && x.Input.Conduct == y.Input.Conduct && x.Input.Stats == y.Input.Stats &&
        x.Input.Projection == y.Input.Projection &&
        x.Input.WorldSeed == y.Input.WorldSeed && x.Result == y.Result &&
        x.StartingProfile.Grade == y.StartingProfile.Grade &&
        x.StartingProfile.AffinityPercent == y.StartingProfile.AffinityPercent &&
        x.StartingProfile.Rating == y.StartingProfile.Rating && x.StartingProfile.Peak == y.StartingProfile.Peak &&
        x.StartingProfile.LastPracticeDay == y.StartingProfile.LastPracticeDay &&
        x.StartingProfile.DecaySettledDay == y.StartingProfile.DecaySettledDay &&
        x.StartingProfile.SupervisedPracticeMinutes == y.StartingProfile.SupervisedPracticeMinutes &&
        x.StartingProfile.EarliestRetakeDay == y.StartingProfile.EarliestRetakeDay,
        "committed input, output and progression profile agree");
    for (std::size_t dimension = 0; dimension < PowerDimensionCount; ++dimension)
        for (std::size_t day = 0; day < PracticeDaysInWindow; ++day)
        {
            const auto& left = x.StartingProfile.Window[dimension][day];
            const auto& right = y.StartingProfile.Window[dimension][day];
            R92Expect(left.Day == right.Day && left.Minutes == right.Minutes &&
                left.GainCentipoints == right.GainCentipoints, "practice windows remain exactly equal");
        }
}

void R92Act(TutorialDungeonState& tutorial, AwakeningState& awakening, std::size_t choice)
{
    const auto& situation = TutorialSituations()[tutorial.NextSituation];
    R92Expect(CompleteTutorialChoice(tutorial, awakening, situation.StationId, situation.Choices[choice].Id,
                                    *tutorial.StartedAt).Ok(), "substantive action completes at its station");
    R92Expect(IsTutorialDungeonStateValid(tutorial, awakening), "each action preserves cross-slice invariants");
}

void R92Grant(TutorialDungeonState& tutorial, AwakeningState& awakening, const AbilityRecipeSet& recipes)
{
    R92Expect(!awakening.Granted, "finishing stations does not itself resolve or grant");
    const auto before = awakening;
    const auto input = BuildAwakeningInput(awakening);
    const auto expected = ResolveAwakening(input, recipes);
    R92Expect(GrantTutorialAwakening(tutorial, awakening, recipes, *tutorial.StartedAt + TutorialSessionMinutes).Ok(),
              "a supervised full or withdrawn route grants after the fixed hour");
    R92Expect(tutorial.Phase == TutorialDungeonPhase::Completed && awakening.Granted &&
        IsTutorialDungeonStateValid(tutorial, awakening), "grant and session completion commit together");
    const auto& grant = *awakening.Granted;
    R92Expect(grant.Result == expected && grant.Input.Conduct == input.Conduct &&
        grant.Input.Stats == input.Stats && grant.Input.Projection == input.Projection && grant.Input.WorldSeed == input.WorldSeed &&
        grant.StartingProfile.Grade == AwakenedGrade::F && !grant.Result.Ability.RecipeId.empty() &&
        grant.Result.Ability.OriginTag == AwakeningOriginTag && grant.Result.Ability.Rank == AwakeningAbilityRank &&
        grant.At == *tutorial.StartedAt + TutorialSessionMinutes,
        "actual resolver result and F-grade profile are stored with named input evidence");
    const auto profile = AwakenedStartingProfile(expected, GameClock::DayNumber(grant.At));
    R92Expect(profile && profile->Rating == grant.StartingProfile.Rating &&
        profile->AffinityPercent == grant.StartingProfile.AffinityPercent,
        "starting profile is the existing reader's exact projection");
    R92Expect(awakening.Stats == before.Stats && awakening.ProjectionEvidence == before.ProjectionEvidence &&
        awakening.AttributeEvidenceMilli == before.AttributeEvidenceMilli && awakening.Conduct == before.Conduct &&
        awakening.AppliedActivityInstances == before.AppliedActivityInstances,
        "grant invents no growth, projection, activity or extra conduct");
    R92Expect(std::accumulate(grant.Result.ConductRating.begin(), grant.Result.ConductRating.end(), 0) ==
        AwakeningConductBudgetPoints, "withdrawal and every normal route receive the same capability budget");
    const auto saved = awakening;
    const auto savedTutorial = tutorial;
    R92Expect(GrantTutorialAwakening(tutorial, awakening, {}, grant.At + 1).Error == "tutorial.already_granted",
              "even missing recipes cannot cause a second resolution of a committed grant");
    R92Same(awakening, saved);
    R92Expect(tutorial == savedTutorial, "duplicate grant cannot rewrite completion time");
}

void R92Definitions()
{
    std::set<std::string_view> ids{TutorialSafeStationId, TutorialGrantStationId};
    R92Expect(TutorialSituations().size() == TutorialSituationCount, "three shared situations exposed");
    for (const auto& situation : TutorialSituations())
    {
        R92Expect(ids.insert(situation.Id).second && ids.insert(situation.StationId).second &&
            ids.insert(situation.WithdrawalChoiceId).second, "situation, station and withdrawal IDs are unique");
        R92Expect(IsConductTokenFair(situation.Id) && IsConductTokenFair(situation.WithdrawalChoiceId),
                  "stable situation and withdrawal IDs satisfy conduct fairness");
        for (const auto& choice : situation.Choices)
        {
            R92Expect(ids.insert(choice.Id).second && ids.insert(choice.ConsequenceId).second &&
                IsConductTokenFair(choice.Id), "each substantive choice has a distinct ID and observable consequence");
            const auto points = ConductActionEvidence(choice.Action, TutorialConductStakes);
            R92Expect(std::accumulate(points.begin(), points.end(), 0) == ConductActionTotal(TutorialConductStakes),
                      "helping, observing, shielding, improvising and peaceful routes carry equal evidence");
        }
    }
}

void R92AllRoutes(const ContentDb& content, const AbilityRecipeSet& recipes)
{
    for (std::size_t route = 0; route < 27; ++route)
    {
        auto awakening = R92Seed(content);
        TutorialDungeonState tutorial;
        auto replay = R92Seed(content);
        TutorialDungeonState replayTutorial;
        R92Expect(BeginTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok(), "enter supervised floor");
        R92Expect(BeginTutorialDungeon(replayTutorial, replay, replay.AppointmentAt).Ok(), "independently enter replay");
        R92Expect(awakening.Conduct.Entries.empty() && !awakening.Granted, "sign-in alone adds no conduct or grant");
        std::size_t code = route;
        for (std::size_t station = 0; station < TutorialSituationCount; ++station)
        {
            R92Act(tutorial, awakening, code % 3);
            R92Act(replayTutorial, replay, code % 3);
            R92Same(replay, awakening);
            R92Expect(tutorial == replayTutorial, "same seed and commands produce identical progress");
            code /= 3;
            // Copy/resume at every command boundary; no simulated past choices on restart.
            auto resumed = awakening;
            auto resumedTutorial = tutorial;
            const auto conduct = ParseConductLedger(SerialiseConductLedger(resumed.Conduct));
            R92Expect(conduct.has_value(), "recorded conduct survives its existing canonical codec");
            resumed.Conduct = *conduct;
            R92Same(resumed, awakening);
            R92Expect(IsTutorialDungeonStateValid(resumedTutorial, resumed), "resumed prefix remains valid");
            awakening = std::move(resumed);
            tutorial = resumedTutorial;
        }
        R92Expect(tutorial.Phase == TutorialDungeonPhase::ReadyToGrant && awakening.Conduct.Entries.size() == 3,
                  "all 27 substantive routes finish with exactly three resolved situations");
        auto resumed = awakening;
        auto resumedTutorial = tutorial;
        R92Grant(tutorial, awakening, recipes);
        R92Grant(replayTutorial, replay, recipes);
        R92Grant(resumedTutorial, resumed, recipes);
        R92Same(awakening, replay);
        R92Same(awakening, resumed);
        R92Expect(tutorial == resumedTutorial, "resumed grant has identical deterministic state");
        for (const auto& dimension : awakening.Granted->Input.Conduct.Dimensions)
            for (const auto& evidence : dimension.Evidence)
                R92Expect(FindConductEntry(awakening.Conduct, evidence.TestId)->Record.ChoiceId == evidence.ChoiceId,
                          "grant explanation points to actions actually recorded");
    }
}

void R92StateValidation(const ContentDb& content)
{
    auto awakening = R92Seed(content);
    TutorialDungeonState tutorial;
    R92Expect(IsTutorialDungeonStateValid(tutorial, awakening), "fresh input and tutorial form a valid pair");
    R92Expect(BeginTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok(), "start structural validation");
    R92Act(tutorial, awakening, 0);
    auto malformed = tutorial;
    malformed.NextSituation = 255;
    R92Expect(!IsTutorialDungeonStateValid(malformed, awakening), "invalid situation index is refused without indexing");
    malformed = tutorial;
    malformed.Withdrawn = true;
    malformed.Phase = TutorialDungeonPhase::ReadyToGrant;
    R92Expect(!IsTutorialDungeonStateValid(malformed, awakening), "a flag cannot fabricate early withdrawal conduct");
    malformed = tutorial;
    malformed.StartedAt.reset();
    R92Expect(!IsTutorialDungeonStateValid(malformed, awakening), "session must retain its supervised start");
    auto badAwakening = awakening;
    badAwakening.Conduct.Entries[0].Record.ChoiceId = "tutorial.corridor.brace_wall";
    R92Expect(!IsTutorialDungeonStateValid(tutorial, badAwakening), "choice must belong to its completed situation");
    badAwakening = awakening;
    ++badAwakening.Conduct.Entries[0].Points[0];
    R92Expect(!IsTutorialDungeonStateValid(tutorial, badAwakening), "stored evidence cannot exceed the fixed action table");
    badAwakening = awakening;
    badAwakening.MakeUp = {AwakeningMakeUpPhase::Scheduled, awakening.ExaminationAt - TutorialSessionMinutes};
    R92Expect(!IsTutorialDungeonStateValid(tutorial, badAwakening), "make-up must leave room before the examination");
    badAwakening = awakening;
    badAwakening.Granted = AwakeningGrant{};
    R92Expect(!IsTutorialDungeonStateValid(tutorial, badAwakening), "active session cannot already own a committed grant");
}

void R92Withdrawal(const ContentDb& content, const AbilityRecipeSet& recipes)
{
    std::size_t prefixes = 1;
    for (std::size_t completed = 0; completed <= TutorialSituationCount; ++completed, prefixes *= 3)
    {
        for (std::size_t route = 0; route < prefixes; ++route)
        {
            for (const bool assistance : {false, true})
            {
                if (completed == TutorialSituationCount && assistance) continue;
                auto awakening = R92Seed(content);
                TutorialDungeonState tutorial;
                R92Expect(BeginTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok(), "withdrawal still enters supervision");
                std::size_t code = route;
                for (std::size_t i = 0; i < completed; ++i, code /= 3) R92Act(tutorial, awakening, code % 3);
                if (assistance)
                    R92Expect(RequestTutorialAssistance(tutorial, awakening, awakening.AppointmentAt).Ok(),
                              "mistakes offer safe assistance");
                R92Expect(WithdrawTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok() && tutorial.Withdrawn &&
                    !tutorial.RecoveryPending && IsTutorialDungeonStateValid(tutorial, awakening),
                    "safe withdrawal works after every route prefix, during recovery and after the final action");
                R92Expect(awakening.Conduct.Entries.size() == std::min(completed + 1, TutorialSituationCount),
                          "withdrawal never fabricates participation in unseen situations");
                if (completed < TutorialSituationCount)
                    R92Expect(awakening.Conduct.Entries.back().Record.ChoiceId ==
                        TutorialSituations()[completed].WithdrawalChoiceId &&
                        awakening.Conduct.Entries.back().Record.Action == ConductAction::Withdraw,
                        "withdrawal records its distinct equal-stakes conduct choice");
                const auto withdrawn = awakening;
                const auto withdrawnTutorial = tutorial;
                R92Expect(!WithdrawTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok(),
                          "repeating a safety exit cannot add conduct");
                R92Same(awakening, withdrawn);
                R92Expect(tutorial == withdrawnTutorial, "repeating safety exit leaves both slices unchanged");
                R92Grant(tutorial, awakening, recipes);
            }
        }
    }
}

void R92RefusalsAndRecovery(const ContentDb& content, const AbilityRecipeSet& recipes)
{
    auto awakening = R92Seed(content);
    TutorialDungeonState tutorial;
    const auto initial = awakening;
    R92Expect(!GrantTutorialAwakening(tutorial, awakening, recipes, awakening.AppointmentAt).Ok() &&
        !WithdrawTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok() &&
        !BeginTutorialDungeon(tutorial, awakening, awakening.AppointmentAt - 1).Ok(),
        "zero-conduct fallback cannot bypass supervision or the appointment");
    R92Same(awakening, initial);
    R92Expect(tutorial == TutorialDungeonState{}, "rejected entry leaves no partial session");
    R92Expect(BeginTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok(), "start refusal exercise");
    const auto started = tutorial;
    const auto& crossing = TutorialSituations()[0];
    R92Expect(!BeginTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok() &&
        !MarkTutorialAbsence(tutorial, awakening, awakening.AppointmentAt).Ok() &&
        !GrantTutorialAwakening(tutorial, awakening, recipes, awakening.AppointmentAt + 60).Ok() &&
        !CompleteTutorialChoice(tutorial, awakening, TutorialSafeStationId, crossing.Choices[0].Id, awakening.AppointmentAt).Ok() &&
        !CompleteTutorialChoice(tutorial, awakening, crossing.StationId, "tutorial.unknown", awakening.AppointmentAt).Ok() &&
        !CompleteTutorialChoice(tutorial, awakening, crossing.StationId, crossing.Choices[0].Id, awakening.AppointmentAt - 1).Ok() &&
        !CompleteTutorialChoice(tutorial, awakening, crossing.StationId, crossing.Choices[0].Id,
                                awakening.AppointmentAt, {"p_invalid_identity"}).Ok(), "invalid or premature actions refused");
    R92Expect(tutorial == started, "refusals leave tutorial progress unchanged");
    R92Same(awakening, initial);
    for (int attempt = 0; attempt < 10; ++attempt)
    {
        R92Expect(RequestTutorialAssistance(tutorial, awakening, awakening.AppointmentAt).Ok(), "request assistance repeatedly");
        R92Expect(!CompleteTutorialChoice(tutorial, awakening, crossing.StationId, crossing.Choices[0].Id,
                                         awakening.AppointmentAt).Ok(), "action waits for safe recovery");
        R92Expect(RecoverTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok(), "recover without failure");
    }
    R92Same(awakening, initial);
    R92Expect(tutorial == started, "recovery and paused reading accrue no conduct or scoring state");
    R92Act(tutorial, awakening, 0);
    const auto afterFirst = awakening;
    const auto afterFirstTutorial = tutorial;
    R92Expect(!CompleteTutorialChoice(tutorial, awakening, crossing.StationId, crossing.Choices[0].Id, awakening.AppointmentAt).Ok() &&
        !CompleteTutorialChoice(tutorial, awakening, crossing.StationId, crossing.Choices[1].Id, awakening.AppointmentAt).Ok(),
        "same choice and changed choice cannot resolve the same situation twice");
    R92Same(awakening, afterFirst);
    R92Expect(tutorial == afterFirstTutorial, "duplicate choices do not advance progress");
    R92Act(tutorial, awakening, 1);
    R92Act(tutorial, awakening, 2);
    const auto ready = awakening;
    const auto readyTutorial = tutorial;
    R92Expect(!GrantTutorialAwakening(tutorial, awakening, recipes, awakening.AppointmentAt + 59).Ok() &&
        !GrantTutorialAwakening(tutorial, awakening, recipes, awakening.ExaminationAt).Ok() &&
        GrantTutorialAwakening(tutorial, awakening, {}, awakening.AppointmentAt + 60).Error == "tutorial.resolve_failed",
        "fixed time, fixed deadline and resolver error cannot partially commit");
    R92Same(awakening, ready);
    R92Expect(tutorial == readyTutorial, "failed grant preserves eligibility for corrected recipe data");
    R92Grant(tutorial, awakening, recipes);
    const auto committed = awakening;
    const auto committedTutorial = tutorial;
    R92Expect(!WithdrawTutorialDungeon(tutorial, awakening, awakening.AppointmentAt + 60).Ok() &&
        !BeginTutorialDungeon(tutorial, awakening, awakening.AppointmentAt + 60).Ok() &&
        !RequestTutorialAssistance(tutorial, awakening, awakening.AppointmentAt + 60).Ok(), "committed session cannot restart");
    R92Same(awakening, committed);
    R92Expect(tutorial == committedTutorial, "committed completion is immutable");
}

void R92TimingAndParties(const ContentDb& content, const AbilityRecipeSet& recipes)
{
    auto immediate = R92Seed(content);
    auto paced = immediate;
    TutorialDungeonState immediateTutorial, pacedTutorial;
    const auto at = immediate.AppointmentAt;
    const std::vector<std::string> parties{"p_0000000000000002", "p_0000000000000001"};
    const std::vector<std::string> canonical(parties.rbegin(), parties.rend());
    R92Expect(BeginTutorialDungeon(immediateTutorial, immediate, at).Ok() &&
        BeginTutorialDungeon(pacedTutorial, paced, at).Ok(), "enter comparison sessions with identical latent inputs");
    for (std::size_t i = 0; i < TutorialSituationCount; ++i)
    {
        const auto& situation = TutorialSituations()[i];
        const auto actionAt = at + static_cast<GameInstant>(i + 1) * 10;
        R92Expect(RequestTutorialAssistance(pacedTutorial, paced, actionAt).Ok() &&
            RecoverTutorialDungeon(pacedTutorial, paced, actionAt).Ok(), "assisted route recovers at each situation");
        const auto before = paced;
        const auto beforeTutorial = pacedTutorial;
        R92Expect(!CompleteTutorialChoice(pacedTutorial, paced, situation.StationId, situation.Choices[i].Id,
            actionAt - 1, parties).Ok() &&
            !CompleteTutorialChoice(pacedTutorial, paced, situation.StationId, situation.Choices[i].Id,
            actionAt, {parties[0], parties[0]}).Ok(), "backwards actions and repeated parties are refused atomically");
        R92Same(paced, before);
        R92Expect(pacedTutorial == beforeTutorial, "refused action changes no assistance/progress timestamp");
        R92Expect(CompleteTutorialChoice(immediateTutorial, immediate, situation.StationId, situation.Choices[i].Id,
            at, canonical).Ok() && CompleteTutorialChoice(pacedTutorial, paced, situation.StationId,
            situation.Choices[i].Id, actionAt, parties).Ok(), "action timing and party order are not scoring inputs");
        R92Expect(paced.Conduct.Entries.back().Record.Parties == canonical &&
            SummariseConduct(immediate.Conduct) == SummariseConduct(paced.Conduct),
            "RecordConduct canonicalises actual parties and retains identical evidence regardless of pacing");
    }
    R92Grant(immediateTutorial, immediate, recipes);
    R92Grant(pacedTutorial, paced, recipes);
    R92Expect(immediate.Granted->Result == paced.Granted->Result &&
        immediate.Granted->Input.Conduct == paced.Granted->Input.Conduct,
        "identical conduct with different action times and recovery yields the same Awakening");
}

void R92MakeUp(const ContentDb& content, const AbilityRecipeSet& recipes)
{
    auto awakening = R92Seed(content);
    TutorialDungeonState tutorial;
    R92Expect(MarkTutorialAbsence(tutorial, awakening, awakening.AppointmentAt).Ok() &&
        awakening.MakeUp.Phase == AwakeningMakeUpPhase::Required && !awakening.MakeUp.At && !awakening.Granted,
        "absence requires a supervised make-up, never zero-conduct auto-grant");
    const auto absent = awakening;
    R92Expect(!BeginTutorialDungeon(tutorial, awakening, awakening.AppointmentAt).Ok() &&
        !ScheduleTutorialMakeUp(tutorial, awakening, awakening.ExaminationAt - 60, awakening.AppointmentAt).Ok() &&
        !ScheduleTutorialMakeUp(tutorial, awakening, awakening.AppointmentAt - 1, awakening.AppointmentAt).Ok(),
        "required make-up and whole-session scheduling bounds cannot be bypassed");
    R92Same(awakening, absent);
    const auto slot = awakening.AppointmentAt + MinutesPerDay;
    R92Expect(ScheduleTutorialMakeUp(tutorial, awakening, slot, awakening.AppointmentAt).Ok() &&
        awakening.MakeUp.Phase == AwakeningMakeUpPhase::Scheduled && awakening.MakeUp.At == slot &&
        awakening.AppointmentAt == absent.AppointmentAt && awakening.ExaminationAt == absent.ExaminationAt,
        "make-up preserves original appointment and fixed exam deadline");
    R92Expect(!BeginTutorialDungeon(tutorial, awakening, slot + 1).Ok(), "late arrival needs explicit replacement appointment");
    R92Expect(MarkTutorialAbsence(tutorial, awakening, slot + 1).Ok() &&
        ScheduleTutorialMakeUp(tutorial, awakening, slot + 1, slot + 1).Ok() &&
        BeginTutorialDungeon(tutorial, awakening, slot + 1).Ok(), "missed make-up can be safely rescheduled and entered");
    R92Expect(WithdrawTutorialDungeon(tutorial, awakening, slot + 1).Ok(), "late make-up permits immediate safe withdrawal");
    R92Grant(tutorial, awakening, recipes);
    R92Expect(awakening.MakeUp.Phase == AwakeningMakeUpPhase::Completed && awakening.MakeUp.At == slot + 1 &&
        awakening.Stats == absent.Stats && awakening.Granted->Result.LatentRating ==
        ResolveAwakening(BuildAwakeningInput(absent), recipes).LatentRating,
        "late make-up completes without capability penalty or appointment rewrite");
    auto latest = R92Seed(content);
    TutorialDungeonState latestTutorial;
    const auto lastSlot = latest.ExaminationAt - TutorialSessionMinutes - 1;
    R92Expect(MarkTutorialAbsence(latestTutorial, latest, lastSlot).Ok() &&
        ScheduleTutorialMakeUp(latestTutorial, latest, lastSlot, lastSlot).Ok() &&
        BeginTutorialDungeon(latestTutorial, latest, lastSlot).Ok() &&
        WithdrawTutorialDungeon(latestTutorial, latest, lastSlot).Ok(), "last valid slot permits the full session");
    R92Grant(latestTutorial, latest, recipes);
    R92Expect(latest.Granted->At == latest.ExaminationAt - 1, "last make-up finishes strictly before examination");
}
} // namespace

int main(int argc, char** argv)
{
    R92Expect(argc == 2, "pass Data/Content directory");
    const auto content = LoadContent(R92ReadContent(argv[1]));
    for (const auto& issue : content.Issues) std::cerr << issue.File << ": " << issue.Message << '\n';
    R92Expect(content.Ok(), "real content validates");
    const auto recipes = LoadAbilityRecipes(content.Db);
    R92Expect(recipes.Ok(), "reuse authored ability recipes");
    R92Definitions();
    R92AllRoutes(content.Db, recipes.Set);
    R92Withdrawal(content.Db, recipes.Set);
    R92RefusalsAndRecovery(content.Db, recipes.Set);
    R92TimingAndParties(content.Db, recipes.Set);
    R92StateValidation(content.Db);
    R92MakeUp(content.Db, recipes.Set);
    std::cout << "Tutorial dungeon: " << R92Assertions << " assertions passed\n";
}
