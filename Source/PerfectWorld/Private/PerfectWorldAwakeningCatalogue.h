#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldAwakeningBridge.h"

/**
 * R9-06: what the floor offers at each station, as presentation (prefix Aw / PWAwakening).
 *
 * This table is the ONE place the floor types situation ids, choice ids, words and visible consequences. It is not a rules table:
 * whether a choice is accepted, what it means for conduct and what it is worth are the core's (R9-02/03). A choice's FAwEffect only
 * says which blocks the floor moves to make the core's acceptance visible. When R9-03 authors the real situations, this file is where
 * the ids and wording are aligned (see PerfectWorldAwakeningBridge.h for the ids expected).
 */
namespace PWAwakening
{
/** How a choice is described to the player. Names the action, never a verdict on the person (no "brave", no "good"). */
enum class EAwStance : uint8
{
    Help,
    Observe,
    Wait,
    Protect,
    Improvise,
    Lead,
    Coordinate,
    Investigate,
    Decline,
    Withdraw
};

/** The raised shape on a pad. Every choice has a shape AND words, so a hazard or a choice never relies on colour alone. */
enum class EAwShape : uint8
{
    Plus,
    Frame,
    Bars,
    Tee,
    Arrow,
    Back,
    Dots,
    Ring
};

/** Which block poses a choice's acceptance shows. -1 leaves a mover where it is. */
struct FAwEffect
{
    int32 PrimaryPose = -1;
    int32 SecondaryPose = -1;
    /** Stop the primary mover's own cycling (a braced or protected wall stays put). */
    bool bHoldPrimary = false;
};

struct FAwChoiceDef
{
    FName Id;
    EAwStance Stance = EAwStance::Help;
    /** Short capitals drawn above the pad ("HELP"). */
    FString Label;
    /** The prompt verb ("Help your classmate across"). */
    FText Verb;
    /** What happened, in plain words, when the core accepted it (an evidence statement, not a verdict). */
    FText Caption;
    FAwEffect Effect;
};

struct FAwSituationDef
{
    FName Id;
    FString Title;
    /** One or two lines for the station board. */
    FString Summary;
    /** Station tint name used in the layout's ids ("crossing"). */
    FString Slug;
    /** Who is in the situation with the player: a person id of the NPC kit and the name shown. */
    FName FigurePerson;
    FText FigureName;
    /** The three choices of this situation, in pad order. The withdrawal is separate (AwWithdrawDef) and offered at all of them. */
    TArray<FAwChoiceDef> Choices;
};

/** The three situations in floor order. Built once. */
const TArray<FAwSituationDef>& AwCatalogue();
int32 AwSituationIndex(FName SituationId);
const FAwSituationDef* AwFindSituation(FName SituationId);
/** The universal withdrawal, offered at every station in every phase. */
const FAwChoiceDef& AwWithdrawDef();
/** A choice of this situation, or the withdrawal when ChoiceId is its id. Null when the situation does not offer it. */
const FAwChoiceDef* AwFindChoice(FName SituationId, FName ChoiceId);
/** Every distinct choice id offered anywhere, withdrawal included, once. */
TArray<FName> AwAllChoiceIds();

EAwShape AwStanceShape(EAwStance Stance);
const TCHAR* AwStanceName(EAwStance Stance);

/** The people of the floor (NK = the NPC kit's person ids). Defaults are the established Class XI cast; R9-03 may rename them. */
struct FAwCast
{
    FName Teacher = FName(TEXT("person.amrita_basu"));
    FText TeacherName = NSLOCTEXT("PerfectWorldAwakening", "TeacherName", "Ms Basu");
    FName SafeClassmateA = FName(TEXT("person.classmate_safe_a"));
    FName SafeClassmateB = FName(TEXT("person.classmate_safe_b"));
};

/** The timing of a visible transfer (entering, leaving, withdrawing, a gentle reset). */
struct FAwTransitionPlan
{
    /** A fade to black and back. False under reduced motion: no alpha animation at all. */
    bool bFade = true;
    float FadeOutSeconds = 0.35f;
    /** Held between the two halves (normal) or BEFORE the cut while the caption announces it (reduced). */
    float HoldSeconds = 0.10f;
    float FadeInSeconds = 0.35f;
    /** The caption is shown before the move rather than after it. */
    bool bAnnounceFirst = false;

    float TotalSeconds() const { return FadeOutSeconds + HoldSeconds + FadeInSeconds; }
};

/** Pure: the plan for a transfer. Reduced motion: an announced discrete cut after a still hold, no fade, no motion. */
FAwTransitionPlan AwPlanTransition(bool bReducedMotion);

/** The presentation comfort setting, unless a test or acceptance override says otherwise. */
bool AwIsReducedMotion(const TOptional<bool>& Override);

/** Seconds a reduced-motion mover waits (the caption announcing the step) before it jumps. */
constexpr float AwReducedStepAnnounceSeconds = 1.5f;
/** Seconds a smooth mover takes to reach a pose, and between automatic steps of a cycling mover. */
constexpr float AwMoverGlideSeconds = 0.9f;
constexpr float AwMoverCycleSeconds = 7.0f;
} // namespace PWAwakening
