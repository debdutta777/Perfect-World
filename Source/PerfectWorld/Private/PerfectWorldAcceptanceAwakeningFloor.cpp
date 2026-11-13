#include "PerfectWorldAcceptanceAwakeningFloor.h"

#include "PerfectWorldAcceptanceAwakeningFloorSteps.h"

// awakening_floor (see the header). The step builders are in PerfectWorldAcceptanceAwakeningFloorSteps.cpp.

namespace PWAcceptance
{
using namespace AwakeningFloor;

void AppendAwakeningFloorSteps(TArray<FStepDef>& Out)
{
    const TSharedRef<FAwFlRun> Run = MakeShared<FAwFlRun>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(AwFlBuiltStep(Run));
    Out.Add(AwFlStartStep());
    Out.Add(AwFlCameraStep(TEXT("camera_first"), false));
    Out.Add(MakeShotStep(TEXT("shot_terminal_forecourt")));
    Out.Add(AwFlEnterStep(TEXT("enter_the_virtual_floor"), Run, 1));
    Out.Add(MakeShotStep(TEXT("shot_floor_arrival_first")));

    // Pass A, first person: help, protect, coordinate. Pad index 0 is each station's first choice.
    const TCHAR* const Helping[PWAwakening::SituationCount] = {TEXT("help_the_classmate_across"), TEXT("protect_with_the_barrier"), TEXT("coordinate_the_carriers")};
    for (int32 Station = 0; Station < PWAwakening::SituationCount; ++Station)
    {
        const FString Slug = FString::Printf(TEXT("station%d"), Station + 1);
        Out.Add(AwFlWalkToStationStep(TEXT("walk_to_") + Slug, Station));
        Out.Add(MakeShotStep(TEXT("shot_") + Slug + TEXT("_before_first")));
        Out.Add(AwFlChoiceStep(FString(Helping[Station]), Run, Station, 0, false));
        Out.Add(MakeShotStep(TEXT("shot_") + Slug + TEXT("_after_first")));
    }
    Out.Add(AwFlSessionCompleteStep(TEXT("session_complete_after_helping"), Run));
    Out.Add(AwFlWalkToReturnStep(TEXT("walk_to_the_return_pad_first")));
    Out.Add(AwFlLeaveStep(TEXT("return_to_the_campus_first"), Run, 1));
    Out.Add(MakeShotStep(TEXT("shot_back_on_campus_first")));

    // Pass B, third person, a fresh session: withdraw, withdraw, decline the confrontation.
    Out.Add(AwFlResetStep(TEXT("fresh_session_for_the_withdrawal_route"), Run));
    Out.Add(AwFlSecondPass(AwFlCameraStep(TEXT("camera_third"), true), Run));
    Out.Add(AwFlSecondPass(AwFlEnterStep(TEXT("enter_the_virtual_floor_again"), Run, 2), Run));
    Out.Add(AwFlSecondPass(MakeShotStep(TEXT("shot_floor_arrival_third")), Run));

    Out.Add(AwFlSecondPass(AwFlWalkToStationStep(TEXT("walk_to_station1_again"), 0), Run));
    Out.Add(AwFlSecondPass(AwFlChoiceStep(TEXT("withdraw_at_the_crossing"), Run, 0, 3, true), Run));
    Out.Add(AwFlSecondPass(MakeShotStep(TEXT("shot_safe_area_with_teacher")), Run));

    Out.Add(AwFlSecondPass(AwFlWalkToStationStep(TEXT("walk_to_station2_again"), 1), Run));
    Out.Add(AwFlSecondPass(AwFlChoiceStep(TEXT("withdraw_at_the_corridor"), Run, 1, 3, true), Run));
    Out.Add(AwFlSecondPass(MakeShotStep(TEXT("shot_safe_area_second_withdrawal")), Run));

    Out.Add(AwFlSecondPass(AwFlWalkToStationStep(TEXT("walk_to_station3_again"), 2), Run));
    Out.Add(AwFlSecondPass(AwFlChoiceStep(TEXT("decline_the_confrontation"), Run, 2, 2, false), Run));
    Out.Add(AwFlSecondPass(MakeShotStep(TEXT("shot_station3_handover_third")), Run));
    Out.Add(AwFlSecondPass(AwFlSessionCompleteStep(TEXT("session_complete_after_withdrawing"), Run), Run));
    Out.Add(AwFlSecondPass(AwFlWalkToReturnStep(TEXT("walk_to_the_return_pad_second")), Run));
    Out.Add(AwFlSecondPass(AwFlLeaveStep(TEXT("return_to_the_campus_second"), Run, 2), Run));
    Out.Add(AwFlSecondPass(MakeShotStep(TEXT("shot_back_on_campus_third")), Run));
    Out.Add(AwFlSummaryStep(TEXT("bridge_saw_every_choice"), Run));
}
} // namespace PWAcceptance
