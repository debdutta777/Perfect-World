// The title viewpoint and the title layout (LL-04, first submission): the composed frame puts the Shanti Chai shopfront in the
// right third and the camera on the road, the drift is bounded and stops under reduced motion, the pawn is hidden and restored
// without being moved, and the responsive layout keeps the aperture in the right third without ever overlapping the menu,
// suppressing ornament before compressing controls. Run in the editor under "PerfectWorld.TitleView" (Session Frontend, or
// the Unreal MCP AutomationTestToolset). Pure parts need no world; the actor tests spawn into a bare game world that never
// begins play.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/DefaultPawn.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldPresentationSettings.h"
#include "PerfectWorldTitleView.h"
#include "UI/SPWTitleScreen.h"

namespace PWTitleViewTest
{
/** A throw-away game world; actors spawn but play never begins. */
struct FTvTestWorld
{
    UWorld* World = nullptr;

    FTvTestWorld() { World = UWorld::CreateWorld(EWorldType::Game, false); }
    ~FTvTestWorld()
    {
        if (World != nullptr)
        {
            World->DestroyWorld(false);
        }
    }
    FTvTestWorld(const FTvTestWorld&) = delete;
    FTvTestWorld& operator=(const FTvTestWorld&) = delete;
};

/** Where a world point lands across the screen (0 left, 1 right) for a camera with a horizontal field of view; 2 when behind. */
double TvScreenX(const PWTitleView::FTvPose& Pose, const FVector& Point)
{
    const FVector Local = Pose.Rotation.UnrotateVector(Point - Pose.Location); // X ahead, Y to the right, Z up
    if (Local.X <= 1.0)
    {
        return 2.0;
    }
    const double HalfTan = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(Pose.FovDegrees) * 0.5));
    return 0.5 + (Local.Y / Local.X) / (2.0 * HalfTan);
}
} // namespace PWTitleViewTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTitleViewFrameTest, "PerfectWorld.TitleView.RestFrameComposition",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWTitleViewFrameTest::RunTest(const FString& Parameters)
{
    const PWTitleView::FTvPose Rest = PWTitleView::RestPose();
    TestTrue(TEXT("the camera stands inside the 5 m road, clear of both footpaths"), FMath::Abs(Rest.Location.Y) < PWLane::RoadHalfWidth - 40.0);
    TestTrue(TEXT("at standing eye height above the road"),
             Rest.Location.Z - PWLane::GroundZ > 140.0 && Rest.Location.Z - PWLane::GroundZ < 200.0);
    TestTrue(TEXT("behind the lane's start, well inside the closing building at X -2400"), Rest.Location.X > PWLane::LaneX0 + 300.0);
    TestTrue(TEXT("a natural lens"), Rest.FovDegrees >= 45.0f && Rest.FovDegrees <= 70.0f);
    TestTrue(TEXT("level enough to read as a street, not a ceiling"), FMath::Abs(Rest.Rotation.Pitch) < 6.0 && FMath::IsNearlyZero(Rest.Rotation.Roll));

    // The Shanti Chai frontage: X 600 to 1300 on the +Y row, front face at Y 520. It must sit in the right third, where
    // the dark menu surface has faded, and not under the aperture's centre alone: both ends in view, not past the edge.
    const double Near = PWTitleViewTest::TvScreenX(Rest, FVector(600.0, 520.0, PWLane::GroundZ + 150.0));
    const double Far = PWTitleViewTest::TvScreenX(Rest, FVector(1300.0, 520.0, PWLane::GroundZ + 150.0));
    TestTrue(TEXT("the shopfront's far end is right of the menu's fade"), Far >= 0.60 && Far <= 0.92);
    TestTrue(TEXT("its near end is in view"), Near >= 0.60 && Near <= 0.92);
    TestTrue(TEXT("and the near end is right of the far end (the facade recedes leftward)"), Near > Far);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTitleViewDriftTest, "PerfectWorld.TitleView.DriftIsSmallPeriodicAndHoldsRest",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWTitleViewDriftTest::RunTest(const FString& Parameters)
{
    const PWTitleView::FTvPose Rest = PWTitleView::RestPose();
    const PWTitleView::FTvPose Start = PWTitleView::DriftPose(0.0f);
    TestTrue(TEXT("drift starts exactly at rest"), Start.Location.Equals(Rest.Location, 0.001) && Start.Rotation.Equals(Rest.Rotation, 0.001));

    bool bMoved = false;
    bool bBounded = true;
    for (float Seconds = 0.0f; Seconds <= 800.0f; Seconds += 1.7f)
    {
        const PWTitleView::FTvPose Pose = PWTitleView::DriftPose(Seconds);
        bBounded = bBounded && FMath::Abs(Pose.Location.X - Rest.Location.X) <= PWTitleView::MaxDriftDistance() + 0.001
                   && FMath::IsNearlyEqual(Pose.Location.Y, Rest.Location.Y) && FMath::IsNearlyEqual(Pose.Location.Z, Rest.Location.Z)
                   && FMath::Abs(Pose.Rotation.Yaw - Rest.Rotation.Yaw) <= PWTitleView::MaxDriftYaw() + 0.001
                   && FMath::IsNearlyEqual(Pose.Rotation.Pitch, Rest.Rotation.Pitch) && Pose.FovDegrees == Rest.FovDegrees;
        bMoved = bMoved || !Pose.Location.Equals(Rest.Location, 1.0);
    }
    TestTrue(TEXT("the drift stays a few degrees and a shallow truck from the composed frame"), bBounded);
    TestTrue(TEXT("and it does move"), bMoved);
    TestTrue(TEXT("a few degrees is few"), PWTitleView::MaxDriftYaw() <= 2.0f && PWTitleView::MaxDriftDistance() <= 200.0);

    const PWTitleView::FTvPose Loop = PWTitleView::DriftPose(400.0f);
    TestTrue(TEXT("the loop closes on the rest frame"), Loop.Location.Equals(Rest.Location, 0.001));

    // The actor: reduced motion snaps to the rest frame and stops ticking; ordinary motion ticks while presenting.
    PWTitleViewTest::FTvTestWorld Test;
    if (!TestNotNull(TEXT("world"), Test.World))
    {
        return false;
    }
    APWTitleView* View = Test.World->SpawnActor<APWTitleView>(Rest.Location, Rest.Rotation);
    if (!TestNotNull(TEXT("title view"), View))
    {
        return false;
    }
    TestTrue(TEXT("the lens fills the window rather than letterboxing to 16:9"),
             View->GetCameraComponent() != nullptr && !View->GetCameraComponent()->bConstrainAspectRatio);
    TestFalse(TEXT("a new view does not tick until presented"), View->IsActorTickEnabled());

    View->SetPresenting(true);
    View->ApplyMotionPreference(false);
    TestTrue(TEXT("ordinary motion ticks while presenting"), View->IsActorTickEnabled());
    if (!UPerfectWorldPresentationSettings::Get().IsReducedMotion())
    {
        View->Tick(10.0f);
        TestFalse(TEXT("after ten seconds the camera has drifted"), View->GetActorLocation().Equals(Rest.Location, 1.0));
    }

    View->ApplyMotionPreference(true);
    TestFalse(TEXT("reduced motion stops the tick at once"), View->IsActorTickEnabled());
    TestTrue(TEXT("and holds the composed frame exactly"), View->GetActorLocation().Equals(Rest.Location, 0.01)
             && View->GetActorRotation().Equals(Rest.Rotation, 0.01));
    TestEqual(TEXT("with no drift time carried"), View->GetDriftSeconds(), 0.0f);

    View->SetPresenting(false);
    View->ApplyMotionPreference(false);
    TestFalse(TEXT("a view that is not presented never ticks"), View->IsActorTickEnabled());
    View->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTitleViewPawnTest, "PerfectWorld.TitleView.PawnIsHiddenAndRestoredNeverMoved",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWTitleViewPawnTest::RunTest(const FString& Parameters)
{
    PWTitleViewTest::FTvTestWorld Test;
    if (!TestNotNull(TEXT("world"), Test.World))
    {
        return false;
    }
    const PWTitleView::FTvPose Rest = PWTitleView::RestPose();
    APWTitleView* View = Test.World->SpawnActor<APWTitleView>(Rest.Location, Rest.Rotation);
    const FVector Feet(100.0, 200.0, 300.0);
    const FRotator Facing(0.0, 77.0, 0.0);
    ADefaultPawn* Pawn = Test.World->SpawnActor<ADefaultPawn>(Feet, Facing);
    ADefaultPawn* Hidden = Test.World->SpawnActor<ADefaultPawn>(Feet, Facing);
    if (!TestNotNull(TEXT("view"), View) || !TestNotNull(TEXT("pawn"), Pawn) || !TestNotNull(TEXT("second pawn"), Hidden))
    {
        return false;
    }

    View->HoldPawn(Pawn);
    TestTrue(TEXT("the title hides the pawn"), Pawn->IsHidden());
    TestTrue(TEXT("without moving it"), Pawn->GetActorLocation().Equals(Feet, 0.001));
    TestTrue(TEXT("or turning it"), Pawn->GetActorRotation().Equals(Facing, 0.001));
    View->ReleasePawn();
    TestFalse(TEXT("entering the world shows it again"), Pawn->IsHidden());
    TestTrue(TEXT("exactly where it stood"), Pawn->GetActorLocation().Equals(Feet, 0.001) && Pawn->GetActorRotation().Equals(Facing, 0.001));

    Hidden->SetActorHiddenInGame(true);
    View->HoldPawn(Hidden);
    View->ReleasePawn();
    TestTrue(TEXT("a pawn that was already hidden stays hidden"), Hidden->IsHidden());

    // Holding the same pawn twice must not forget how it was first found.
    View->HoldPawn(Pawn);
    View->HoldPawn(Pawn);
    View->ReleasePawn();
    TestFalse(TEXT("a repeated hold restores the original visibility"), Pawn->IsHidden());

    View->Destroy();
    Pawn->Destroy();
    Hidden->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWTitleLayoutTest, "PerfectWorld.TitleView.ResponsiveLayoutSuppressesOrnamentBeforeControls",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWTitleLayoutTest::RunTest(const FString& Parameters)
{
    // Where the aperture's box starts, and where the menu's reading column ends (left pad + column + its right padding).
    const auto ApertureLeft = [](const float Width, const SPWTitleScreen::FLayout& L) { return Width - 24.0f - L.ApertureSlot; };
    const auto MenuRight = [](const SPWTitleScreen::FLayout& L) { return L.LeftPad + L.MenuWidth + 24.0f; };

    const SPWTitleScreen::FLayout Hd = SPWTitleScreen::ComputeLayout(1280.0f, 720.0f);
    TestTrue(TEXT("1280x720: the aperture shows"), Hd.bAperture);
    TestTrue(TEXT("1280x720: about 340 units where the right third allows"), Hd.ApertureDiameter >= 330.0f && Hd.ApertureDiameter <= 460.0f);
    TestTrue(TEXT("1280x720: it lives in the right third"), Hd.ApertureSlot <= 1280.0f / 3.0f);
    TestTrue(TEXT("1280x720: the menu stays at its full 560 column"), FMath::IsNearlyEqual(Hd.MenuWidth, 560.0f));
    TestTrue(TEXT("1280x720: no overlap with the menu"), ApertureLeft(1280.0f, Hd) >= MenuRight(Hd));
    TestTrue(TEXT("1280x720: the opaque surface covers the controls"), Hd.SurfaceEdge >= MenuRight(Hd));
    TestFalse(TEXT("1280x720: the narrow slot has no motes"), Hd.bMotes);

    const SPWTitleScreen::FLayout Fhd = SPWTitleScreen::ComputeLayout(1920.0f, 1080.0f);
    TestTrue(TEXT("1920x1080: the aperture shows at 340 to 460"), Fhd.bAperture && Fhd.ApertureDiameter >= 340.0f && Fhd.ApertureDiameter <= 460.0f);
    TestTrue(TEXT("1920x1080: the motes have the room they need"), Fhd.bMotes);
    TestTrue(TEXT("1920x1080: no overlap with the menu"), ApertureLeft(1920.0f, Fhd) >= MenuRight(Fhd));

    // 150% UI scale on those windows is 853x480 and 1280x720 in logical units.
    const SPWTitleScreen::FLayout Scaled = SPWTitleScreen::ComputeLayout(1280.0f / 1.5f, 720.0f / 1.5f);
    TestFalse(TEXT("150% on 1280x720: the aperture is suppressed, not squeezed against the menu"), Scaled.bAperture);
    TestFalse(TEXT("150% on 1280x720: so are the corner brackets"), Scaled.bBrackets);
    TestTrue(TEXT("150% on 1280x720: the controls keep their full column"), FMath::IsNearlyEqual(Scaled.MenuWidth, 560.0f));
    const SPWTitleScreen::FLayout ScaledHd = SPWTitleScreen::ComputeLayout(1920.0f / 1.5f, 1080.0f / 1.5f);
    TestTrue(TEXT("150% on 1920x1080 equals the 1280x720 composition"), ScaledHd.bAperture == Hd.bAperture
             && FMath::IsNearlyEqual(ScaledHd.ApertureDiameter, Hd.ApertureDiameter, 0.01f));

    // Sweeping every width: the aperture never overlaps the menu, and controls are compressed only after ornament is gone.
    bool bNoOverlap = true;
    bool bOrnamentFirst = true;
    bool bBounded = true;
    for (float Width = 320.0f; Width <= 3840.0f; Width += 16.0f)
    {
        for (const float Height : {360.0f, 480.0f, 720.0f, 1080.0f, 1440.0f})
        {
            const SPWTitleScreen::FLayout L = SPWTitleScreen::ComputeLayout(Width, Height);
            bNoOverlap = bNoOverlap && (!L.bAperture || ApertureLeft(Width, L) >= MenuRight(L));
            bOrnamentFirst = bOrnamentFirst && (L.MenuWidth >= 560.0f - 0.01f || (!L.bAperture && !L.bBrackets));
            bBounded = bBounded && L.MenuWidth >= 280.0f - 0.01f && L.MenuWidth <= 560.0f + 0.01f
                       && (!L.bAperture || (L.ApertureDiameter >= 220.0f && L.ApertureDiameter <= 460.0f))
                       && (!L.bMotes || L.bAperture);
        }
    }
    TestTrue(TEXT("the aperture never overlaps the menu at any size"), bNoOverlap);
    TestTrue(TEXT("controls are compressed only after the ornament is suppressed"), bOrnamentFirst);
    TestTrue(TEXT("the menu column and the aperture stay within their bounds"), bBounded);

    const SPWTitleScreen::FLayout Tiny = SPWTitleScreen::ComputeLayout(480.0f, 360.0f);
    TestTrue(TEXT("a very narrow window compresses the column, to no less than 280"), Tiny.MenuWidth < 560.0f && Tiny.MenuWidth >= 280.0f);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
