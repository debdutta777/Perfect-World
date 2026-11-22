#include "PerfectWorldLegacyRecovery.h"

#include "UI/SPWStudyDesk.h"

#define LOCTEXT_NAMESPACE "PerfectWorldP314U"

namespace PWLegacyRecovery
{
namespace
{
FText P314UJoin(const FText& A, const FText& B)
{
    if (A.IsEmpty()) return B;
    if (B.IsEmpty()) return A;
    return FText::Format(LOCTEXT("Paragraphs", "{0}\n\n{1}"), A, B);
}
}

FText P314UCompatibilityNotice(const PerfectWorld::AwakeningCompatibilityView& View, const bool bIncludeSeed)
{
    FText Notice;
    if (bIncludeSeed && View.SeededOnRecovery)
        Notice = LOCTEXT("Seeded", "This life began before Awakening was recorded. Your Awakening has been prepared from who you were at the start; your earlier school days are kept, but nothing you did not record has been added.");
    if (View.PostDeadline)
        Notice = P314UJoin(Notice, LOCTEXT("PostDeadline", "Legacy compatibility mode: this life was saved before Awakening existed and the last supervised session start has passed. You can keep living it without Awakening, or start a new life from the title screen's New Game. This save is kept."));
    else if (View.MakeUpRequired)
        Notice = P314UJoin(Notice, FText::Format(LOCTEXT("MakeUp", "You missed your Awakening session. A supervised make-up is required. Last session start: {0}. The examination date does not change."),
            PWA5::Deadline(View.LastSessionStart)));
    if (View.CountdownUnavailable)
        Notice = P314UJoin(Notice, LOCTEXT("CountdownUnavailable", "Your countdown began before this save recorded it. The ten days cannot be rebuilt, so countdown opportunities are unavailable; ordinary preparation still counts."));
    return Notice;
}

FText P314URecoverLoadedLife(const PerfectWorld::CharacterCreationSave& Creation, const FP314ULoadActions& Actions)
{
    bool bSeededNow = false;
    FText Failure;
    const auto Submit = [&Actions, &Failure](const PerfectWorld::Command& Command)
    {
        const auto Done = Actions.Submit(Command);
        if (!Done.Accepted)
        {
            UE_LOG(LogTemp, Warning, TEXT("P314U load recovery refused: %hs"), Done.Why.Code.c_str());
            Failure = LOCTEXT("RecoveryRefused", "Compatibility recovery could not finish. This life is loaded; try loading again when its content is available.");
        }
        return Done.Accepted;
    };
    // Saved worlds are normally quiescent. Bound the continuation, just like the live clock.
    for (int32 Round = 0; Round < 64 && !PerfectWorld::IsWorldQuiescent(Actions.Read()); ++Round)
        if (!Submit({PerfectWorld::CommandSource::System, PerfectWorld::SettleDueEvents{}})) break;
    if (PerfectWorld::IsWorldQuiescent(Actions.Read()))
    {
        const auto Plan = PerfectWorld::PlanAwakeningRecovery(Actions.Read());
        if (Plan.Needed())
            bSeededNow = Submit({PerfectWorld::CommandSource::System, PerfectWorld::AwakeningRecover{Creation}}) && Plan.Seed;
        if (!Actions.PendingWork().empty())
            (void)Submit({PerfectWorld::CommandSource::System, PerfectWorld::SchoolWorkReconcile{}});
    }
    else
        Failure = LOCTEXT("StillUnsettled", "The loaded world's events could not settle. Compatibility recovery is still pending.");
    const FText Saved = Actions.Autosave();
    return P314UJoin(P314UJoin(P314UCompatibilityNotice(PerfectWorld::QueryAwakeningCompatibility(Actions.Read()), bSeededNow), Failure), Saved);
}

FText P314URecoveredWorkNotice(const PerfectWorld::SchoolWorkState& Work,
    const PerfectWorld::SchoolWorkBriefCatalogue& Authored, const std::string_view WorkId)
{
    for (const auto& Record : Work.Records)
        if (Record.Brief.Id == WorkId)
            for (const auto& Def : Authored.Briefs)
                if (Def.Brief.Id == WorkId && PerfectWorld::IsRecoveredSchoolWork(Record, Def.Brief))
                    return FText::Format(LOCTEXT("RecoveredHomework", "Recovered homework: set while this save could not receive it. Recovery window: {0} to {1}. Due {1} (at least a seven-day recovery window)."),
                        PWA5::Deadline(Record.IssuedAt), PWA5::Deadline(Record.Brief.DueAt));
    return FText::GetEmpty();
}
}

#undef LOCTEXT_NAMESPACE
