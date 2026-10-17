// The successful-start acknowledgement (LL-04, second submission): it plays once, only for a fresh life that was saved AND
// placed (never for a failed start, a failed placement or a load), expands every sector together by 12% and fades over 450 ms,
// shows nothing under reduced motion, ends at once if reduced motion is switched on while it runs, and is a hit-test
// invisible, non-accessible overlay that delays nothing. Run in the editor under "PerfectWorld.StartAcknowledgement"
// (Session Frontend, or the Unreal MCP AutomationTestToolset). The model tests need no Slate; the widget tests build the
// real sigil and root and drive time by hand.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PerfectWorldPresentationSettings.h"
#include "UI/PWStartAcknowledgement.h"
#include "UI/SPWAwakeningSigil.h"
#include "UI/SPWFrontEndRoot.h"

namespace PWStartAckTest
{
/** Sets reduced motion for one test and puts the player's own setting back afterwards. */
struct FSfScopedMotion
{
    explicit FSfScopedMotion(const bool bReduced)
        : bBefore(UPerfectWorldPresentationSettings::Get().IsReducedMotion())
    {
        UPerfectWorldPresentationSettings::Get().SetReducedMotion(bReduced);
    }
    ~FSfScopedMotion() { UPerfectWorldPresentationSettings::Get().SetReducedMotion(bBefore); }
    FSfScopedMotion(const FSfScopedMotion&) = delete;
    FSfScopedMotion& operator=(const FSfScopedMotion&) = delete;
    bool bBefore = false;
};

/** What the front end does after a start attempt, modelled on APerfectWorldFrontEndHUD::EnterWorld's gate. */
bool SfAttemptStart(PWUI::FStartAcknowledgement& Cue, const bool bSaved, const bool bPlaced, const bool bReduced)
{
    if (!PWUI::FStartAcknowledgement::ShouldPlay(bSaved, bPlaced))
    {
        return false;
    }
    return Cue.Begin(bReduced);
}
} // namespace PWStartAckTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWStartAckGateTest, "PerfectWorld.StartAcknowledgement.PlaysOnlyAfterSaveAndPlacement",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWStartAckGateTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("a saved and placed fresh life earns the cue"), PWUI::FStartAcknowledgement::ShouldPlay(true, true));
    TestFalse(TEXT("a failed save never does"), PWUI::FStartAcknowledgement::ShouldPlay(false, true));
    TestFalse(TEXT("nor a failed placement"), PWUI::FStartAcknowledgement::ShouldPlay(true, false));
    TestFalse(TEXT("nor both failing"), PWUI::FStartAcknowledgement::ShouldPlay(false, false));

    PWUI::FStartAcknowledgement Cue;
    TestFalse(TEXT("a load or Continue (not a fresh start) places the player but never plays it"),
              PWStartAckTest::SfAttemptStart(Cue, false, true, false));
    TestFalse(TEXT("a failed save: no cue"), PWStartAckTest::SfAttemptStart(Cue, false, false, false));
    TestFalse(TEXT("saved but not placed: no cue"), PWStartAckTest::SfAttemptStart(Cue, true, false, false));
    TestEqual(TEXT("so no failure path ever started one"), Cue.GetStartCount(), 0);
    TestFalse(TEXT("and none is showing"), Cue.IsActive());

    // Family and care starts both reach the same gate and get the same single cue.
    TestTrue(TEXT("a family start plays"), PWStartAckTest::SfAttemptStart(Cue, true, true, false));
    TestEqual(TEXT("exactly once"), Cue.GetStartCount(), 1);
    Cue.Cancel();
    TestTrue(TEXT("a care start plays the same"), PWStartAckTest::SfAttemptStart(Cue, true, true, false));
    TestEqual(TEXT("again exactly once more"), Cue.GetStartCount(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWStartAckMotionTest, "PerfectWorld.StartAcknowledgement.ExpandsTwelvePercentAndFadesIn450ms",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWStartAckMotionTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("450 ms"), FMath::IsNearlyEqual(PWUI::FStartAcknowledgement::DurationSeconds, 0.45f));
    TestTrue(TEXT("about 12%"), FMath::IsNearlyEqual(PWUI::FStartAcknowledgement::Expansion, 0.12f));
    TestTrue(TEXT("quiet: never fully opaque"), PWUI::FStartAcknowledgement::PeakOpacity < 1.0f);

    PWUI::FStartAcknowledgement Cue;
    TestTrue(TEXT("begins"), Cue.Begin(false));
    TestTrue(TEXT("starts at the ring's normal size"), FMath::IsNearlyEqual(Cue.GetScale(), 1.0f));

    float LastScale = Cue.GetScale();
    float LastOpacity = Cue.GetOpacity();
    bool bMonotonic = true;
    bool bBounded = LastOpacity <= PWUI::FStartAcknowledgement::PeakOpacity + 0.0001f;
    float Seconds = 0.0f;
    while (Cue.Advance(0.016f))
    {
        Seconds += 0.016f;
        bMonotonic = bMonotonic && Cue.GetScale() >= LastScale - 0.0001f && Cue.GetOpacity() <= LastOpacity + 0.0001f;
        bBounded = bBounded && Cue.GetScale() <= 1.12f + 0.0001f && Cue.GetOpacity() >= 0.0f;
        LastScale = Cue.GetScale();
        LastOpacity = Cue.GetOpacity();
        if (Seconds > 2.0f)
        {
            break;
        }
    }
    TestTrue(TEXT("it grows and fades without ever reversing (no flashing)"), bMonotonic);
    TestTrue(TEXT("and stays within 12% and its peak opacity"), bBounded);
    TestTrue(TEXT("it ends on its own at about 450 ms"), !Cue.IsActive() && Seconds >= 0.40f && Seconds <= 0.50f);
    TestTrue(TEXT("and leaves nothing behind"), FMath::IsNearlyEqual(Cue.GetOpacity(), 0.0f) && FMath::IsNearlyEqual(Cue.GetScale(), 1.0f));

    // Just before the end it is nearly full size and nearly gone.
    Cue.Begin(false);
    Cue.Advance(0.44f);
    TestTrue(TEXT("late in the cue the ring is near 112%"), Cue.GetScale() > 1.115f);
    TestTrue(TEXT("and almost transparent"), Cue.GetOpacity() < 0.02f);
    Cue.Cancel();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWStartAckReducedMotionTest, "PerfectWorld.StartAcknowledgement.ReducedMotionShowsNothingAndStopsAtOnce",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWStartAckReducedMotionTest::RunTest(const FString& Parameters)
{
    // Model: under reduced motion there is no animation, not even a static frame.
    PWUI::FStartAcknowledgement Cue;
    TestFalse(TEXT("reduced motion: nothing begins"), Cue.Begin(true));
    TestFalse(TEXT("so nothing is active"), Cue.IsActive());
    TestEqual(TEXT("and no start is counted"), Cue.GetStartCount(), 0);
    TestTrue(TEXT("scale stays 1 and opacity 0"), FMath::IsNearlyEqual(Cue.GetScale(), 1.0f) && FMath::IsNearlyEqual(Cue.GetOpacity(), 0.0f));

    // A toggle made while it runs ends it on the spot and it stays ended.
    TestTrue(TEXT("ordinary motion begins"), Cue.Begin(false));
    Cue.Advance(0.1f);
    Cue.OnMotionPreference(true);
    TestFalse(TEXT("switching reduced motion on stops it at once"), Cue.IsActive());
    TestFalse(TEXT("time passing does not revive it"), Cue.Advance(0.1f));
    TestFalse(TEXT("and a restart under reduced motion shows nothing"), Cue.Begin(true));
    Cue.OnMotionPreference(false);
    TestFalse(TEXT("switching it off again does not start a cue by itself"), Cue.IsActive());

    // Widget: the real sigil reacts to the player's setting.
    {
        PWStartAckTest::FSfScopedMotion Motion(true);
        const TSharedRef<SPWAwakeningSigil> Sigil = SNew(SPWAwakeningSigil).Acknowledgement(true).FitRing(true);
        TestFalse(TEXT("widget: reduced motion plays nothing"), Sigil->PlayAcknowledgement());
        TestFalse(TEXT("widget: not acknowledging"), Sigil->IsAcknowledging());
        TestTrue(TEXT("widget: and stays collapsed"), Sigil->GetVisibility() == EVisibility::Collapsed);
    }
    {
        PWStartAckTest::FSfScopedMotion Motion(false);
        const TSharedRef<SPWAwakeningSigil> Sigil = SNew(SPWAwakeningSigil).Acknowledgement(true).FitRing(true);
        TestTrue(TEXT("widget: ordinary motion plays"), Sigil->PlayAcknowledgement());
        Sigil->AdvanceAcknowledgement(0.1f);
        TestTrue(TEXT("widget: running"), Sigil->IsAcknowledging());
        UPerfectWorldPresentationSettings::Get().SetReducedMotion(true);
        TestFalse(TEXT("widget: a toggle made while it runs stops it immediately"), Sigil->IsAcknowledging());
        TestTrue(TEXT("widget: and hides it"), Sigil->GetVisibility() == EVisibility::Collapsed);
        Sigil->AdvanceAcknowledgement(0.1f);
        TestFalse(TEXT("widget: it stays stopped"), Sigil->IsAcknowledging());
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWStartAckControlTest, "PerfectWorld.StartAcknowledgement.NeverDelaysOrCapturesControl",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWStartAckControlTest::RunTest(const FString& Parameters)
{
    PWStartAckTest::FSfScopedMotion Motion(false);

    // The sigil is a hit-test invisible decoration outside the accessibility reading order, whether idle or running.
    const TSharedRef<SPWAwakeningSigil> Sigil = SNew(SPWAwakeningSigil).Acknowledgement(true).FitRing(true);
    TestFalse(TEXT("idle: it is collapsed, so nothing can be hit or read"), Sigil->GetVisibility().IsVisible());
    TestFalse(TEXT("it never takes keyboard focus"), Sigil->SupportsKeyboardFocus());
    TestTrue(TEXT("playing returns at once"), Sigil->PlayAcknowledgement());
    TestTrue(TEXT("while it runs it is hit-test invisible"), Sigil->GetVisibility() == EVisibility::HitTestInvisible);
#if WITH_ACCESSIBILITY
    TestTrue(TEXT("and not in the accessibility reading order"),
             Sigil->GetAccessibleBehavior() == EAccessibleBehavior::NotAccessible);
#endif
    Sigil->AdvanceAcknowledgement(PWUI::FStartAcknowledgement::DurationSeconds + 0.01f);
    TestFalse(TEXT("it ends by itself"), Sigil->IsAcknowledging());
    TestFalse(TEXT("and collapses again"), Sigil->GetVisibility().IsVisible());

    // A sigil that is not an acknowledgement ignores the call: the title aperture is never turned into a cue.
    const TSharedRef<SPWAwakeningSigil> Aperture = SNew(SPWAwakeningSigil).Reveal(true).Rotation(true);
    TestFalse(TEXT("an ordinary aperture does not acknowledge"), Aperture->PlayAcknowledgement());

    // The root with the world showing through: it plays the cue yet stays transparent to input.
    const TSharedRef<SPWFrontEndRoot> Root = SNew(SPWFrontEndRoot);
    TestTrue(TEXT("root: a start into the world plays the cue"), Root->PlayStartAcknowledgement());
    TestTrue(TEXT("root: it is running"), Root->IsStartAcknowledgementRunning());
    TestTrue(TEXT("root: the world still receives input through it (self hit-test invisible)"),
             Root->GetVisibility() == EVisibility::SelfHitTestInvisible);
    Root->Hide();
    TestTrue(TEXT("root: hiding (as on any later entry) leaves it harmless"), Root->GetVisibility() == EVisibility::SelfHitTestInvisible);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
