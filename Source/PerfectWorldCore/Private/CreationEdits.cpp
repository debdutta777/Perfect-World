// Atomic allocation edits, allocation undo/redo and edit previews for
// CharacterCreator. Rank edits and presets live in CharacterCreation.cpp; both
// commit through CommitRanks here so every way of changing ranks shares one
// history. Only the twenty trait ranks are ever recorded.

#include "CharacterCreation.h"
#include "CreationPresentation.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
using EditRanks = std::array<std::int32_t, TraitCount>;

OperationResult EditLockedFailure()
{
    return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                    "Character creation is locked after life begins.");
}

void EditPushBounded(std::vector<EditRanks>& stack, const EditRanks& ranks)
{
    stack.push_back(ranks);
    if (stack.size() > MaxUndoDepth)
    {
        stack.erase(stack.begin());
    }
}

AllocationPreview FinishPreview(OperationResult result, const CharacterCreator& trial)
{
    AllocationPreview preview;
    preview.Result = std::move(result);
    preview.Review = trial.Review();
    preview.Home = trial.ResolveOpeningHome();
    preview.GroupTotals = GroupAllocationTotals(preview.Review.Allocations);
    preview.StrongestTraits = StrongestTraits(preview.Review.Allocations, StrongestTraitCount);
    return preview;
}
} // namespace

void CharacterCreator::CommitRanks(const EditRanks& proposed)
{
    if (proposed == TraitRanks_)
    {
        return;
    }
    EditPushBounded(UndoStack_, TraitRanks_);
    RedoStack_.clear();
    TraitRanks_ = proposed;
}

void CharacterCreator::ClearHistory() noexcept
{
    UndoStack_.clear();
    RedoStack_.clear();
}

OperationResult CharacterCreator::ClearAllocation()
{
    if (HasStarted_)
    {
        return EditLockedFailure();
    }
    CommitRanks(EditRanks{});
    return OperationResult::Success();
}

OperationResult CharacterCreator::ResetAllocation()
{
    return ApplyPreset(DefaultPresetId);
}

bool CharacterCreator::CanUndo() const noexcept
{
    return !HasStarted_ && !UndoStack_.empty();
}

bool CharacterCreator::CanRedo() const noexcept
{
    return !HasStarted_ && !RedoStack_.empty();
}

OperationResult CharacterCreator::Undo()
{
    if (HasStarted_)
    {
        return EditLockedFailure();
    }
    if (UndoStack_.empty())
    {
        return OperationResult::Failure(CreationError::NothingToConfirm, "There is nothing to undo.");
    }
    EditPushBounded(RedoStack_, TraitRanks_);
    TraitRanks_ = UndoStack_.back();
    UndoStack_.pop_back();
    return OperationResult::Success();
}

OperationResult CharacterCreator::Redo()
{
    if (HasStarted_)
    {
        return EditLockedFailure();
    }
    if (RedoStack_.empty())
    {
        return OperationResult::Failure(CreationError::NothingToConfirm, "There is nothing to redo.");
    }
    EditPushBounded(UndoStack_, TraitRanks_);
    TraitRanks_ = RedoStack_.back();
    RedoStack_.pop_back();
    return OperationResult::Success();
}

// Each preview runs the real edit on a copy, so it cannot disagree with the
// edit it describes: same validation, same refusal, same resulting review.
AllocationPreview CharacterCreator::PreviewPreset(std::string_view presetId) const
{
    CharacterCreator trial(*this);
    OperationResult result = trial.ApplyPreset(presetId);
    return FinishPreview(std::move(result), trial);
}

AllocationPreview CharacterCreator::PreviewClear() const
{
    CharacterCreator trial(*this);
    OperationResult result = trial.ClearAllocation();
    return FinishPreview(std::move(result), trial);
}

AllocationPreview CharacterCreator::PreviewReset() const
{
    return PreviewPreset(DefaultPresetId);
}
} // namespace PerfectWorld
