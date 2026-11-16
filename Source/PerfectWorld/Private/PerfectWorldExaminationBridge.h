#pragma once

#include "CoreMinimal.h"
#include "WorldReducer.h"

class UWorld;

namespace P406Exam
{
enum class EScreen : uint8 { Unavailable, Decision, Routes, Beat, Result };
struct FAction
{
    FText Label;
    PerfectWorld::TowerSessionAction Kind = PerfectWorld::TowerSessionAction::Enter;
    FString RouteId;
    PerfectWorld::TowerMethod Method = PerfectWorld::TowerMethod::Analysis;
    FString ChoiceId;
    bool bEnabled = true;
};
struct FModel
{
    EScreen Screen = EScreen::Unavailable;
    PerfectWorld::P402ExamWorldView World;
    PerfectWorld::TowerSessionView Session;
    FText Title;
    TArray<FText> Lines;
    TArray<FAction> Actions;
    TArray<FText> Offers;
};

/** Presentation only. The bridge supplies the current runner nonce and beat for EVERY action. */
class IWorld
{
public:
    virtual ~IWorld() = default;
    virtual FModel Read() const = 0;
    virtual PerfectWorld::CommandResult Submit(const FAction& Action) = 0;
};

TSharedRef<IWorld> P406MakeLiveWorld(UWorld& World);
FText P406RefusalText(const FString& Code);
/** Compiled lesson-style table generated from the existing phase2_a7 English bundle. Unknown keys never leak IDs. */
FText P406RunnerText(const std::string& Key, const TCHAR* Fallback);
bool P406FindExpandedRunnerText(const FString& Key, FText& Out);
bool P406PauseEntryVisible(const UWorld* World);
/** Retry hosting while another overlay owns input; never advances the clock or resubmits Enter. */
bool P406PresentRequired(UWorld* World);

/** P406 / P4-05c SEAM: replace ONLY this implementation with validated LoadTowerRunners(content).
 * The returned catalogue must outlive every new-life/Continue sim. Empty means honest exam.missing_content. */
const PerfectWorld::TowerRunnerCatalogue& P406RunnerCatalogueSeam();

/** Scripted map-free fake: outcomes and projections are test inputs, never live rules. */
class FFakeWorld final : public IWorld
{
public:
    FModel Model;
    FModel AfterAccepted;
    PerfectWorld::CommandResult Reply;
    TArray<PerfectWorld::TowerSessionCommand> Submitted;
    FFakeWorld() { Reply.Accepted = true; }
    virtual FModel Read() const override { return Model; }
    virtual PerfectWorld::CommandResult Submit(const FAction& Action) override;
};
}
