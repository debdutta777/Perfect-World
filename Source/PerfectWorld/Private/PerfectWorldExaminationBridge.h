#pragma once

#include "CoreMinimal.h"
#include "WorldReducer.h"

class UWorld;

namespace P406Exam
{
enum class EScreen : uint8 { Unavailable, Decision, Routes, Beat, Result, P407USession };
struct FAction
{
    FText Label;
    PerfectWorld::TowerSessionAction Kind = PerfectWorld::TowerSessionAction::Enter;
    FString RouteId;
    PerfectWorld::TowerMethod Method = PerfectWorld::TowerMethod::Analysis;
    FString ChoiceId;
    bool bEnabled = true;
    bool bP407UFollowup = false;
    bool bP407UWait = false;
    PerfectWorld::P407UFollowupCommand P407UCommand;
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
    std::vector<PerfectWorld::P407UFollowupView> P407UFollowups;
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

/** Process-lifetime validated P4-05c runners, borrowed by new-life/Continue sims. */
const PerfectWorld::TowerRunnerCatalogue& P406RunnerCatalogueSeam(const PerfectWorld::ContentDb& Db);
FText P407UFollowupText(const std::string& Key);
bool P407UAtSchool(UWorld* World);
void P407UAppendFollowups(FModel& Model, const PerfectWorld::WorldSim& Sim, bool bAtSchool);

/** Scripted map-free fake: outcomes and projections are test inputs, never live rules. */
class FFakeWorld final : public IWorld
{
public:
    FModel Model;
    FModel AfterAccepted;
    PerfectWorld::CommandResult Reply;
    TArray<PerfectWorld::TowerSessionCommand> Submitted;
    TArray<PerfectWorld::P407UFollowupCommand> P407USubmitted;
    FFakeWorld() { Reply.Accepted = true; }
    virtual FModel Read() const override { return Model; }
    virtual PerfectWorld::CommandResult Submit(const FAction& Action) override;
};
}
