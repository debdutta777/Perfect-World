#pragma once

#include "CoreMinimal.h"
#include "LegacyRecovery.h"
#include "SchoolWorkIssuance.h"
#include "WorldReducer.h"

namespace PWLegacyRecovery
{
/** P314U: load-only seam. Production submits through the subsystem so events are presented;
 * automation supplies fakes without a map, viewport, slot or game instance. */
struct FP314ULoadActions
{
    TFunction<const PerfectWorld::WorldState&()> Read;
    TFunction<std::vector<std::string>()> PendingWork;
    TFunction<PerfectWorld::CommandResult(const PerfectWorld::Command&)> Submit;
    /** Exactly one attempt after recovery. Empty means saved; otherwise return player-facing words. */
    TFunction<FText()> Autosave;
};

FText P314URecoverLoadedLife(const PerfectWorld::CharacterCreationSave& Creation, const FP314ULoadActions& Actions);
/** Include the seed disclosure only for the first load which actually seeds this life. */
FText P314UCompatibilityNotice(const PerfectWorld::AwakeningCompatibilityView& View, bool bIncludeSeed);
/** Empty for ordinary work or missing authored content. Never issues homework. */
FText P314URecoveredWorkNotice(const PerfectWorld::SchoolWorkState& Work,
    const PerfectWorld::SchoolWorkBriefCatalogue& Authored, std::string_view WorkId);
}
