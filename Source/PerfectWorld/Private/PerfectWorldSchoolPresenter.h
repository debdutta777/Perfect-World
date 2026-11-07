#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include <string>

#include "PerfectWorldSchoolPresenter.generated.h"

class APlayerController;
class SBox;
class SPWSchoolDay;
class SPWStudyDesk;
class SWeakWidget;
class SWidget;

/** What the school presenter is showing. */
enum class EPWSchoolPanel : uint8
{
    None,
    Day,
    Lesson,
    Desk,
    Paper
};

/**
 * S03: opens, hosts and closes the classroom and home study panels (the day panel, the lesson screen, the study desk and the paper), the way
 * UPWBuyPresenter hosts the buy panel. One panel at a time, in one viewport slot. It owns no rules: the bridges are PerfectWorldSchoolLive.h's.
 *
 * Time. Every panel is a paused overlay: the lane-life clock holds while IsOpen() (EPWClockHold::School). Reading costs nothing.
 * Input. Opening switches the local controller to UI-only input and flushes pressed keys; closing restores game-only input unless a menu
 * opened meanwhile. Escape closes the panel before any pause menu (APerfectWorldFrontEndHUD::ToggleInWorldMenu).
 */
UCLASS()
class UPWSchoolPresenter : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    static UPWSchoolPresenter* Get(const UObject* WorldContext);
    /** True when this world's school presenter has a panel open (false with no presenter). */
    static bool IsOpenIn(const UObject* WorldContext);

    virtual void Deinitialize() override;

    bool IsOpen() const { return Kind != EPWSchoolPanel::None; }
    EPWSchoolPanel GetKind() const { return Kind; }
    TSharedPtr<SWidget> GetPanel() const { return Panel; }
    TSharedPtr<SPWSchoolDay> GetDayPanel() const { return DayPanel.Pin(); }

    /** "Take your seat": the school-day panel. False (nothing changes) when another panel, a conversation, a reading, a menu or no screen holds the input. */
    bool OpenDay();
    /** The lesson screen for the period the day panel has chosen (or the binding id given). False when there is no such binding. */
    bool OpenLesson(const FString& BindingId);
    /** The home study desk. False when the bridge says the desk cannot be used or the screen is held. */
    bool OpenDesk();
    /** Closes whatever is open, restores play input, then broadcasts OnClosed. */
    void Close();

    DECLARE_MULTICAST_DELEGATE(FPWSchoolStateChanged);
    FPWSchoolStateChanged OnOpened;
    FPWSchoolStateChanged OnClosed;

private:
    bool BeginHosting();
    void ShowPanel(const TSharedRef<SWidget>& Content, EPWSchoolPanel NewKind);
    void HandOverToLesson();
    void BackToDay();
    void BackToDesk();
    void OpenPaper(const std::string& WorkId);
    FVector2D LogicalSpace() const;

    EPWSchoolPanel Kind = EPWSchoolPanel::None;
    TWeakObjectPtr<APlayerController> Controller;
    TSharedPtr<SBox> Slot;
    TSharedPtr<SWeakWidget> Holder;
    TSharedPtr<SWidget> Panel;
    TWeakPtr<SPWSchoolDay> DayPanel;
    FString LessonBindingId;
};
