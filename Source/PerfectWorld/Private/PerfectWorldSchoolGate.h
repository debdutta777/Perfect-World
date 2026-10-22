#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PerfectWorldInteractable.h"

#include "Activities.h"
#include "Attendance.h"
#include "ContentRegistry.h"
#include "DayLoop.h"
#include "SchoolCalendar.h"
#include "WorldReducer.h"

#include "PerfectWorldSchoolGate.generated.h"

class UTextRenderComponent;
class UWorld;

/**
 * R7-03: the school gate's interactions and the seam through which they reach the world (prefix Sc / PWSchool).
 *
 * Attendance is decided ONLY by the portable core (R6-07): WorldSim::Submit(SchoolArrive) over the bound bell schedule and calendar,
 * read back through QuerySchoolArrival (SchoolArrivalView). Nothing here compares minutes, classifies lateness or touches the
 * attendance record; it maps the core's stable codes and reason keys (school.arrival.on_time, school.gate_closed, school.day.holiday ...)
 * to words, and formats times the core reports.
 *
 * The seam: IScGateWorld is what the gate and the timetable board need from the world. The default, MakeLaneLifeGate, runs over
 * UPWLaneLifeSubsystem and the validated content; tests and the acceptance driver can install their own per world (ScGate::Install).
 */
namespace PWSchool
{
/** What signing in at the gate did. The core's answer, unedited, plus its projection afterwards. */
struct FScSignInResult
{
    bool bAccepted = false;
    /** The core's stable rejection code ("school.gate_closed"), or empty when accepted. */
    FString Code;
    /** The core's own wording of the rejection (English); empty when accepted. */
    FString Message;
    /** SchoolArrivalToday after the attempt: Recorded and the outcome's reason key when accepted; the refusal's context otherwise. */
    PerfectWorld::SchoolArrivalView View;
    /** True when the empty day plan the core asks for first (dayloop.plan_first) was submitted before the arrival. */
    bool bPlannedDay = false;
};

/** Everything the gate and the board need from the world. */
class IScGateWorld
{
public:
    virtual ~IScGateWorld() = default;
    /** The core's presentation-free gate projection for the world's current instant. False (with a reason) when there is no world yet. */
    virtual bool QueryArrival(PerfectWorld::SchoolArrivalView& OutView, FString& OutWhy) = 0;
    /** Submit the arrival through the world: the empty plan when the core asks for it, then SchoolArrive. Never decides the outcome. */
    virtual FScSignInResult SignIn() = 0;
    /** The authoritative bell schedule the timetable is read from, or null when content is unavailable. */
    virtual const PerfectWorld::BellSchedule* GetBellSchedule() = 0;
};

/** The validated school content a gate binds to the world. Not movable: Binding points into this object. */
struct FScSchoolContent
{
    PerfectWorld::ActivityCatalogueResult Activities;
    PerfectWorld::BellScheduleResult Bells;
    PerfectWorld::SchoolCalendarResult Calendars;
    const PerfectWorld::BellSchedule* School = nullptr;
    const PerfectWorld::SchoolCalendar* Calendar = nullptr;
    PerfectWorld::DayLoopCatalogues Binding;

    FScSchoolContent() = default;
    FScSchoolContent(const FScSchoolContent&) = delete;
    FScSchoolContent& operator=(const FScSchoolContent&) = delete;

    /** Loads the Nabapara Vidyalaya schedule for a stream ("science", "commerce", "humanities") and its calendar. False with a reason. */
    bool Load(const PerfectWorld::ContentDb& Db, const FString& Stream, FString& OutWhy);
};

/** Per-world registry of gate seams. A world with none installed gets the default (lane-life) gate on first use. */
namespace ScGate
{
void Install(UWorld* World, TSharedPtr<IScGateWorld> Gate);
void Remove(UWorld* World);
TSharedPtr<IScGateWorld> Resolve(UWorld* World);
} // namespace ScGate

/**
 * The default seam, over UPWLaneLifeSubsystem. It binds the validated school content (UPWContentSubsystem) to a COPY of the live world,
 * submits there and adopts the copy through UPWLaneLifeSubsystem::AdoptSavedWorld only when the core accepted the arrival, so a
 * refusal changes nothing. This is the interim bridge until the lane-life subsystem binds the school content to its own WorldSim
 * (R6-08); the integrator then replaces SignIn's body with the subsystem's own submit (see the order report).
 */
TSharedRef<IScGateWorld> MakeLaneLifeGate(UWorld& World, const FString& Stream = TEXT("science"));

/**
 * The school content for a stream, built once from the world's UPWContentSubsystem and kept for the life of the process (the core
 * borrows it, so it must outlive every WorldSim bound to it). Null with a reason when content is not available.
 */
const FScSchoolContent* ScSharedSchoolContent(UWorld& World, const FString& Stream, FString& OutWhy);

/**
 * Acceptance and test support: moves the lane-life world's clock to Target with the core's own commands (time.advance, then
 * time.settle) on a copy that has the school content bound, and adopts the copy. Fails (touching nothing) when Target is not in the
 * future, the world is not at rest, or the core refuses.
 */
bool ScAdvanceLaneClock(UWorld& World, PerfectWorld::GameInstant Target, FString& OutWhy, const FString& Stream = TEXT("science"));

/**
 * Plans an empty day when the core asks for one, then submits SchoolArrive, on a copy of Live. OutNext is set only when the arrival was
 * accepted (the caller adopts it); otherwise Live is untouched. Shared by the default gate and the tests' world.
 */
FScSignInResult ScSignInOnCopy(const PerfectWorld::WorldState& Live, const PerfectWorld::DayLoopCatalogues& Binding,
                               TUniquePtr<PerfectWorld::WorldSim>& OutNext);

// ---- Words (pure; no world, no decisions) ----

/** "09:35" from a minute of the day. */
FString ScFormatMinute(int32 MinuteOfDay);
/** Words for one of the core's reason keys (school.arrival.*, school.day.*); empty for an unknown key. */
FText ScReasonText(const FString& Key);
/** What the gate says after the core answered: the outcome and its reason, or the refusal. */
FText ScSignInMessage(const FScSignInResult& Result);
/** What the board adds about today from the core's projection, or empty when it is an ordinary school day. */
FText ScTodayNote(const PerfectWorld::SchoolArrivalView& View);
/** The timetable board's lines, from the bell schedule's own blocks: gates, assembly, periods with subjects, break, dismissal. */
TArray<FString> ScBellBoardLines(const PerfectWorld::BellSchedule& Schedule);
} // namespace PWSchool

/**
 * The attendance register at the gate: "Sign in at the gate". Interact submits the arrival through the world (IScGateWorld::SignIn,
 * which is the portable core) and shows the core's result with its reason as a notice. Priority 15. The actor decides nothing.
 */
UCLASS()
class PERFECTWORLD_API APWSchoolGateActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWSchoolGateActor();

    /** PointWorld is the register's interaction point. */
    void Configure(const FVector& PointWorld);

    /** The last result Interact showed (tests and acceptance read it); empty before the first Interact. */
    const PWSchool::FScSignInResult& GetLastResult() const { return LastResult; }
    FText GetLastMessage() const { return LastMessage; }
    int32 GetSignInCount() const { return SignInCount; }

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override { return 15; }
    virtual void Interact(APawn* By) override;

private:
    UPROPERTY()
    TObjectPtr<USceneComponent> Root;

    FVector Point = FVector::ZeroVector;
    PWSchool::FScSignInResult LastResult;
    FText LastMessage;
    int32 SignInCount = 0;
};

/**
 * The timetable on the wall beside the gate: today's bell times, read from the authoritative bell schedule content (ScBellBoardLines
 * over IScGateWorld::GetBellSchedule), drawn as text on the board and shown in full with Interact ("Read the timetable"). The text
 * fills in as soon as the content is available (a one-second retry until then). It shows bell times; it never says whether you are late.
 */
UCLASS()
class PERFECTWORLD_API APWSchoolBoardActor : public AActor, public IPWInteractable
{
    GENERATED_BODY()

public:
    APWSchoolBoardActor();

    /** PointWorld is the interaction point; the text is drawn at TextLocation facing along TextYaw. */
    void Configure(const FVector& PointWorld, const FVector& TextLocation, double TextYaw, float TextSizeCm = 8.0f);

    virtual void Tick(float DeltaSeconds) override;

    /** Reads the schedule from the world's gate seam; true once the board shows it. */
    bool Refresh();
    /** The text on the board's face, lines separated by newlines; empty until the schedule is available. */
    FString GetPostedText() const { return PostedText; }

    // IPWInteractable
    virtual FText GetInteractionVerb(const APawn* By) const override;
    virtual FVector GetInteractionPoint() const override;
    virtual int32 GetInteractionPriority() const override { return 15; }
    virtual void Interact(APawn* By) override;

private:
    UPROPERTY()
    TObjectPtr<UTextRenderComponent> Text;

    FVector Point = FVector::ZeroVector;
    FString PostedText;
};
