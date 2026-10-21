// The allocation radar on the traits page. Run in the editor under "PerfectWorld.AttributeRadar". The values come from a
// real creator subsystem in a throw-away game world (play never begins, no save slot is touched); the geometry, the morph
// and the fit are exercised on the pure model with exact time steps; the widget and the page are the real ones, built
// without a window and evaluated with SlatePrepass. Window sizes follow the other creation tests: 1280x720, 1920x1080 and
// both at 150% UI scale (853x480 and 1280x720 logical units).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "CharacterCreation.h"
#include "CreationPresentation.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldPresentationSettings.h"
#include "UI/PWAttributeRadarModel.h"
#include "UI/PWAttributeRadarMotion.h"
#include "UI/PWCreationModel.h"
#include "UI/SPWAttributeRadar.h"
#include "UI/SPWTraitsPage.h"

namespace PWAttributeRadarTest
{
/** A throw-away game world with a game instance, so the creator exists. */
struct FPgScene
{
    UWorld* World = nullptr;
    UGameInstance* Instance = nullptr;
    UPerfectWorldCharacterCreatorSubsystem* Creator = nullptr;

    FPgScene()
    {
        World = UWorld::CreateWorld(EWorldType::Game, false);
        Instance = NewObject<UGameInstance>();
        Instance->AddToRoot();
        Instance->Init();
        if (World == nullptr) return;
        World->SetGameInstance(Instance);
        Creator = Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>();
        if (Creator != nullptr) Creator->BeginNewCreation();
    }
    ~FPgScene()
    {
        if (World != nullptr) World->DestroyWorld(false);
        Instance->Shutdown();
        Instance->RemoveFromRoot();
    }
    FPgScene(const FPgScene&) = delete;
};

/** Sets the player's motion preference for one test and puts it back. */
struct FPgMotionScope
{
    explicit FPgMotionScope(const bool bReduced) : bBefore(UPerfectWorldPresentationSettings::Get().IsReducedMotion())
    {
        UPerfectWorldPresentationSettings::Get().SetReducedMotion(bReduced);
    }
    ~FPgMotionScope() { UPerfectWorldPresentationSettings::Get().SetReducedMotion(bBefore); }
    FPgMotionScope(const FPgMotionScope&) = delete;
    bool bBefore = false;
};

int32 PgAxisOf(const PWRadar::FSnapshot& Snapshot, const TCHAR* GroupId)
{
    for (int32 Index = 0; Index < Snapshot.Axes.Num(); ++Index)
        if (Snapshot.Axes[Index].Id == FName(GroupId)) return Index;
    return INDEX_NONE;
}

int32 PgCountType(const TSharedRef<SWidget>& Widget, const TCHAR* Type)
{
    int32 Count = Widget->GetTypeAsString() == Type ? 1 : 0;
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index) Count += PgCountType(Children->GetChildAt(Index), Type);
    return Count;
}

struct FPgSize { float Width; float Height; };
const FPgSize PgSizes[] = {{1280.0f, 720.0f}, {1920.0f, 1080.0f}, {1280.0f / 1.5f, 720.0f / 1.5f}, {1920.0f / 1.5f, 1080.0f / 1.5f}};
} // namespace PWAttributeRadarTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPgRadarCoreValuesTest, "PerfectWorld.AttributeRadar.ValuesAreTheCoresAndScaledByItsCaps",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPgRadarCoreValuesTest::RunTest(const FString& Parameters)
{
    using namespace PWAttributeRadarTest;
    FPgScene Scene;
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;
    TestTrue(TEXT("the preset applies"), Scene.Creator->ApplyBalancedScholarPreset());
    const TSharedRef<FPWCreationModel> Model = MakeShared<FPWCreationModel>(Scene.Creator);
    const PWRadar::FSnapshot Snapshot = PWRadar::ReadSnapshot(*Model);

    TestEqual(TEXT("one axis for each domain the core publishes"), Snapshot.Axes.Num(), static_cast<int32>(PerfectWorld::TraitGroupCount));
    TestEqual(TEXT("the budget is the core's"), Snapshot.Budget, Model->Summary.TotalBudget);
    TestEqual(TEXT("the remaining points are the core's"), Snapshot.Remaining, Model->Summary.Remaining);
    const TArray<FPWTraitGroupView> Groups = Scene.Creator->GetTraitGroups();
    int32 Total = 0;
    const int32 TopRank = PerfectWorld::CumulativeCostAtRank(PerfectWorld::MaxRank);
    for (int32 Index = 0; Index < Snapshot.Axes.Num(); ++Index)
    {
        const PWRadar::FAxis& Axis = Snapshot.Axes[Index];
        TestTrue(TEXT("axes follow the core's order"), Axis.Id == Groups[Index].Id);
        TestEqual(TEXT("points are the core's group total"), Axis.Points, Groups[Index].AllocatedPoints);
        int32 Traits = 0;
        for (const FPWTraitView& Trait : Model->Traits) Traits += Trait.Group == Axis.Id ? 1 : 0;
        TestEqual(TEXT("the cap is the core's top-rank cost for each trait, held to the budget"), Axis.Cap, FMath::Min(Traits * TopRank, Snapshot.Budget));
        const float Fraction = PWRadar::Fraction(Axis.Points, Axis.Cap);
        TestTrue(TEXT("a fraction is 0 to 1"), Fraction >= 0.0f && Fraction <= 1.0f);
        TestEqual(TEXT("a fraction is points over cap"), Fraction, static_cast<float>(Axis.Points) / static_cast<float>(Axis.Cap), 0.0001f);
        Total += Axis.Points;
    }
    TestEqual(TEXT("the axes together hold everything spent"), Total, Model->Summary.Spent);

    // Clear, then raise one trait: only its own domain moves, by exactly its cost, and the points left fall by the same.
    TestTrue(TEXT("clear"), Model->Edit([](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.ClearAllocation(); }));
    PWRadar::FSnapshot Cleared = PWRadar::ReadSnapshot(*Model);
    for (const PWRadar::FAxis& Axis : Cleared.Axes) TestEqual(TEXT("cleared axes are empty"), Axis.Points, 0);
    TestEqual(TEXT("cleared: everything is left"), Cleared.Remaining, Cleared.Budget);
    TestTrue(TEXT("raise vitality"), Model->Edit([](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetTraitRank(FName(TEXT("vitality")), 4); }));
    const PWRadar::FSnapshot Raised = PWRadar::ReadSnapshot(*Model);
    const int32 Physical = PgAxisOf(Raised, TEXT("physical"));
    TestTrue(TEXT("the physical axis exists"), Physical != INDEX_NONE);
    if (Physical != INDEX_NONE) TestEqual(TEXT("the physical axis holds the rank's cost"), Raised.Axes[Physical].Points, TopRank);
    TestEqual(TEXT("the points left fall by the same"), Raised.Remaining, Cleared.Remaining - TopRank);
    for (int32 Index = 0; Index < Raised.Axes.Num(); ++Index)
        if (Index != Physical) TestEqual(TEXT("no other axis moves"), Raised.Axes[Index].Points, 0);

    // The pure builder holds every cap to the budget and gives 0 for a domain with no capacity.
    TArray<FPWTraitGroupView> Fake;
    FPWTraitGroupView& Group = Fake.AddDefaulted_GetRef();
    Group.Id = FName(TEXT("fake"));
    Group.AllocatedPoints = 30;
    TArray<FPWTraitView> FakeTraits;
    for (int32 Index = 0; Index < 8; ++Index)
    {
        FPWTraitView& Trait = FakeTraits.AddDefaulted_GetRef();
        Trait.Id = FName(*FString::Printf(TEXT("t%d"), Index));
        Trait.Group = Group.Id;
    }
    const PWRadar::FSnapshot Built = PWRadar::BuildSnapshot(Fake, FakeTraits, [](FName) { return 180; }, 1000, 970);
    TestEqual(TEXT("eight traits at 180 are held to the budget"), Built.Axes[0].Cap, 1000);
    TestEqual(TEXT("no capacity gives no fraction"), PWRadar::Fraction(5, 0), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPgRadarGeometryTest, "PerfectWorld.AttributeRadar.PolygonVerticesSitWhereTheValuesPutThem",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPgRadarGeometryTest::RunTest(const FString& Parameters)
{
    const FVector2f Centre(100.0f, 100.0f);
    // Four axes: up, right, down, left, clockwise from the top.
    const TArray<float> Full = {1.0f, 1.0f, 1.0f, 1.0f};
    const TArray<FVector2f> Square = PWRadar::Polygon(Centre, 50.0f, Full);
    TestEqual(TEXT("four vertices"), Square.Num(), 4);
    TestTrue(TEXT("up"), Square[0].Equals(FVector2f(100.0f, 50.0f), 0.01f));
    TestTrue(TEXT("right"), Square[1].Equals(FVector2f(150.0f, 100.0f), 0.01f));
    TestTrue(TEXT("down"), Square[2].Equals(FVector2f(100.0f, 150.0f), 0.01f));
    TestTrue(TEXT("left"), Square[3].Equals(FVector2f(50.0f, 100.0f), 0.01f));
    TestTrue(TEXT("an empty axis is at the centre"), PWRadar::Vertex(Centre, 50.0f, 2, 4, 0.0f).Equals(Centre, 0.001f));
    TestEqual(TEXT("the display curve is 0 at 0"), PWRadar::DisplayRadius(0.0f), 0.0f);
    TestEqual(TEXT("and 1 at 1"), PWRadar::DisplayRadius(1.0f), 1.0f);
    float Previous = -1.0f;
    bool bMonotonic = true;
    for (int32 Step = 0; Step <= 20; ++Step)
    {
        const float Value = PWRadar::DisplayRadius(static_cast<float>(Step) / 20.0f);
        bMonotonic = bMonotonic && Value > Previous;
        Previous = Value;
    }
    TestTrue(TEXT("the display curve rises with the value"), bMonotonic);
    TestTrue(TEXT("a small value is still visible"), PWRadar::DisplayRadius(0.05f) > 0.15f);

    // Seven axes: all on the outer ring, evenly spaced, the first straight up, the vertices summing to the centre.
    const int32 Count = static_cast<int32>(PerfectWorld::TraitGroupCount);
    FVector2f Sum = FVector2f::ZeroVector;
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FVector2f Vertex = PWRadar::Vertex(FVector2f::ZeroVector, 80.0f, Index, Count, 1.0f);
        TestEqual(TEXT("on the outer ring"), Vertex.Size(), 80.0f, 0.01f);
        Sum += Vertex;
        const FVector2f Next = PWRadar::Vertex(FVector2f::ZeroVector, 80.0f, (Index + 1) % Count, Count, 1.0f);
        TestEqual(TEXT("evenly spaced"), FVector2f::DotProduct(Vertex, Next) / (80.0f * 80.0f), FMath::Cos(2.0f * PI / Count), 0.001f);
    }
    TestTrue(TEXT("the first axis points up"), PWRadar::Direction(0, Count).Equals(FVector2f(0.0f, -1.0f), 0.0001f));
    TestTrue(TEXT("a regular polygon is balanced about its centre"), Sum.Size() < 0.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPgRadarMorphTest, "PerfectWorld.AttributeRadar.MorphGoesFromOneShapeToTheNextInTwoHundredMilliseconds",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPgRadarMorphTest::RunTest(const FString& Parameters)
{
    PWRadar::FMotion Motion;
    Motion.Snap({0.2f, 0.4f, 0.6f}, {100, 200, 300});
    TestFalse(TEXT("a snap does not morph"), Motion.IsMorphing());
    TestFalse(TEXT("and highlights nothing"), Motion.IsActive());

    Motion.Retarget({0.5f, 0.4f, 0.1f}, {250, 200, 50}, false);
    TestTrue(TEXT("a change morphs"), Motion.IsMorphing());
    TestEqual(TEXT("the start is the old shape (0)"), Motion.Displayed(0), 0.2f, 0.0001f);
    TestEqual(TEXT("the start is the old shape (2)"), Motion.Displayed(2), 0.6f, 0.0001f);
    TestTrue(TEXT("the axis that rose is marked"), Motion.Signal(0) == PWRadar::EAxisSignal::Rose);
    TestEqual(TEXT("with its change"), Motion.SignalDelta(0), 150);
    TestTrue(TEXT("the axis that held is not"), Motion.Signal(1) == PWRadar::EAxisSignal::None);
    TestTrue(TEXT("the axis that fell is marked"), Motion.Signal(2) == PWRadar::EAxisSignal::Fell);
    TestEqual(TEXT("with its change"), Motion.SignalDelta(2), -250);

    Motion.Advance(0.05f);
    const float Early = Motion.Displayed(0);
    TestTrue(TEXT("part-way is between the two shapes"), Early > 0.2f && Early < 0.5f);
    TestTrue(TEXT("ease-out leads: a quarter of the time is more than a quarter of the way"), Early > 0.2f + 0.25f * 0.3f);
    Motion.Advance(0.05f);
    const float Middle = Motion.Displayed(0);
    TestTrue(TEXT("and keeps going"), Middle > Early && Middle < 0.5f);
    Motion.Advance(0.1f);
    TestFalse(TEXT("200 ms lands it"), Motion.IsMorphing());
    TestEqual(TEXT("the end is the new shape (0)"), Motion.Displayed(0), 0.5f, 0.0001f);
    TestEqual(TEXT("the end is the new shape (2)"), Motion.Displayed(2), 0.1f, 0.0001f);
    TestEqual(TEXT("the morph length is about 200 ms"), PWRadar::FMotion::MorphSeconds, 0.2f, 0.0001f);

    // A change that arrives mid-morph continues from what is on screen: no jump.
    Motion.Retarget({0.9f, 0.4f, 0.1f}, {450, 200, 50}, false);
    Motion.Advance(0.05f);
    const float Mid = Motion.Displayed(0);
    Motion.Retarget({0.0f, 0.4f, 0.1f}, {0, 200, 50}, false);
    TestEqual(TEXT("a retarget begins where the picture is"), Motion.Displayed(0), Mid, 0.0001f);

    // The highlight outlasts the morph and then goes.
    Motion.Advance(0.5f);
    TestTrue(TEXT("still highlighted after half a second"), Motion.Signal(0) == PWRadar::EAxisSignal::Fell);
    TestTrue(TEXT("and fading"), Motion.SignalStrength(0) < 1.0f && Motion.SignalStrength(0) > 0.0f);
    Motion.Advance(PWRadar::FMotion::SignalSeconds);
    TestTrue(TEXT("gone after its time"), Motion.Signal(0) == PWRadar::EAxisSignal::None);
    TestFalse(TEXT("nothing left running"), Motion.IsActive());

    // A refused edit shakes, damped, and comes to rest.
    Motion.Reject(false);
    bool bMoved = false;
    for (int32 Step = 0; Step < 18; ++Step)
    {
        Motion.Advance(0.02f);
        bMoved = bMoved || FMath::Abs(Motion.ShakeOffset()) > 0.5f;
        TestTrue(TEXT("the shake stays small"), FMath::Abs(Motion.ShakeOffset()) <= PWRadar::FMotion::ShakeAmplitude + 0.001f);
    }
    TestTrue(TEXT("a refusal shakes"), bMoved);
    Motion.Advance(0.1f);
    TestEqual(TEXT("and rests"), Motion.ShakeOffset(), 0.0f);
    TestEqual(TEXT("the flash ends"), Motion.RejectFlash(), 0.0f);
    TestEqual(TEXT("ease-out ends at 1"), PWRadar::EaseOut(1.0f), 1.0f);
    TestEqual(TEXT("and starts at 0"), PWRadar::EaseOut(0.0f), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPgRadarReducedMotionTest, "PerfectWorld.AttributeRadar.ReducedMotionLandsAtOnceAndNothingMoves",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPgRadarReducedMotionTest::RunTest(const FString& Parameters)
{
    using namespace PWAttributeRadarTest;
    PWRadar::FMotion Motion;
    Motion.Snap({0.2f, 0.4f}, {100, 200});
    Motion.Retarget({0.7f, 0.1f}, {350, 50}, true);
    TestFalse(TEXT("no morph under reduced motion"), Motion.IsMorphing());
    TestEqual(TEXT("the new shape at once (0)"), Motion.Displayed(0), 0.7f, 0.0001f);
    TestEqual(TEXT("the new shape at once (1)"), Motion.Displayed(1), 0.1f, 0.0001f);
    TestTrue(TEXT("the highlight is still given"), Motion.Signal(0) == PWRadar::EAxisSignal::Rose && Motion.Signal(1) == PWRadar::EAxisSignal::Fell);
    Motion.Advance(0.6f);
    TestEqual(TEXT("held steady, not faded"), Motion.SignalStrength(0), 1.0f);
    Motion.Advance(1.0f);
    TestTrue(TEXT("and removed when its time is up"), Motion.Signal(0) == PWRadar::EAxisSignal::None);
    Motion.Reject(true);
    for (int32 Step = 0; Step < 10; ++Step)
    {
        Motion.Advance(0.02f);
        TestEqual(TEXT("a refusal never shakes"), Motion.ShakeOffset(), 0.0f);
    }
    TestEqual(TEXT("but it is flashed steadily"), Motion.RejectFlash(), 1.0f);
    Motion.Advance(1.0f);
    TestEqual(TEXT("until it ends"), Motion.RejectFlash(), 0.0f);

    // The same through the widget: the player's own setting decides.
    FPgScene Scene;
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;
    Scene.Creator->ApplyBalancedScholarPreset();
    const TSharedRef<FPWCreationModel> Model = MakeShared<FPWCreationModel>(Scene.Creator);
    for (const bool bReduced : {true, false})
    {
        FPgMotionScope Scope(bReduced);
        TestTrue(TEXT("restore"), Model->Edit([](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.ApplyBalancedScholarPreset(); }));
        const TSharedRef<SPWAttributeRadar> Radar = SNew(SPWAttributeRadar, Model);
        // Lower the first trait that holds any points; its domain's axis must fall.
        FName Lowered, LoweredGroup;
        for (const FPWTraitView& Trait : Model->Traits)
            if (Trait.Rank > 0) { Lowered = Trait.Id; LoweredGroup = Trait.Group; break; }
        if (!TestFalse(TEXT("the preset gives some trait a rank"), Lowered.IsNone())) return false;
        const int32 Axis = PgAxisOf(Radar->GetSnapshot(), *LoweredGroup.ToString());
        if (!TestTrue(TEXT("its domain has an axis"), Axis != INDEX_NONE)) return false;
        const float Before = Radar->GetMotion().Displayed(Axis);
        TestTrue(TEXT("lower the trait to rank 0"), Model->Edit([Lowered](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetTraitRank(Lowered, 0); }));
        Radar->Sync();
        const float Target = Radar->GetMotion().Target(Axis);
        TestTrue(TEXT("the target fell"), Target < Before);
        if (bReduced) TestEqual(TEXT("reduced motion: displayed at the target at once"), Radar->GetMotion().Displayed(Axis), Target, 0.0001f);
        else
        {
            TestEqual(TEXT("normal motion: still at the old shape"), Radar->GetMotion().Displayed(Axis), Before, 0.0001f);
            Radar->Advance(0.2f);
            TestEqual(TEXT("normal motion: lands after 200 ms"), Radar->GetMotion().Displayed(Axis), Target, 0.0001f);
        }
        TestTrue(TEXT("the domain that fell is marked either way"), Radar->GetMotion().Signal(Axis) == PWRadar::EAxisSignal::Fell);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPgRadarRejectionTest, "PerfectWorld.AttributeRadar.ARefusedEditShakesTheBudgetAndMovesNothing",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPgRadarRejectionTest::RunTest(const FString& Parameters)
{
    using namespace PWAttributeRadarTest;
    FPgMotionScope Scope(false);
    FPgScene Scene;
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;
    Scene.Creator->ApplyBalancedScholarPreset();
    const TSharedRef<FPWCreationModel> Model = MakeShared<FPWCreationModel>(Scene.Creator);
    const TSharedRef<SPWAttributeRadar> Radar = SNew(SPWAttributeRadar, Model);
    const PWRadar::FSnapshot Before = Radar->GetSnapshot();
    const int32 Serial = Model->RejectionSerial;

    // Every point is spent, so raising a trait that has room to rise is refused by the core's own ledger.
    bool bRefused = false;
    for (const FPWTraitView& Trait : Model->Traits)
    {
        if (Trait.Rank >= PerfectWorld::MaxRank - 1) continue;
        const FName Id = Trait.Id;
        bRefused = !Model->Edit([Id](UPerfectWorldCharacterCreatorSubsystem& Creator) { return Creator.SetTraitRank(Id, PerfectWorld::MaxRank); });
        if (bRefused) break;
    }
    TestTrue(TEXT("the core refuses an edit it cannot afford"), bRefused);
    TestEqual(TEXT("the refusal is counted once"), Model->RejectionSerial, Serial + 1);
    Radar->Sync();
    TestTrue(TEXT("nothing moved"), Radar->GetSnapshot().SameValues(Before));
    TestFalse(TEXT("and the polygon does not morph"), Radar->GetMotion().IsMorphing());
    TestTrue(TEXT("the budget shakes and flashes"), Radar->GetMotion().RejectFlash() > 0.9f);
    Radar->Advance(0.04f);
    TestTrue(TEXT("sideways"), FMath::Abs(Radar->GetShakeOffset()) > 0.0f);
    Radar->Advance(1.0f);
    TestEqual(TEXT("it comes to rest"), Radar->GetShakeOffset(), 0.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPgRadarFitTest, "PerfectWorld.AttributeRadar.FitsAtEveryWindowAndScale",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPgRadarFitTest::RunTest(const FString& Parameters)
{
    using namespace PWAttributeRadarTest;
    const int32 Count = static_cast<int32>(PerfectWorld::TraitGroupCount);
    for (const FPgSize& Window : PgSizes)
    {
        // The creation flow is 90% of the window from 1440 up, else the window less 48 by 32; the page is what is left
        // of the flow after its padding (up to 64) and its scroll gutter (12).
        const FVector2f Flow = Window.Width >= 1440.0f ? FVector2f(Window.Width * 0.9f, Window.Height * 0.9f) : FVector2f(Window.Width - 48.0f, Window.Height - 32.0f);
        const float PageWidth = Flow.X - 76.0f;
        const bool bNarrow = Flow.X < 1320.0f;
        const FString Name = FString::Printf(TEXT("%.0fx%.0f"), Window.Width, Window.Height);
        const PWRadar::FPlan Plan = PWRadar::PlanForPage(PageWidth, bNarrow, Count);
        const PWRadar::FPageSlot Slot = PWRadar::PageSlot(PageWidth, bNarrow);
        TestTrue(*(Name + TEXT(": the chart is not wider than the room it is given")), Plan.Size.X <= Slot.AvailableWidth + 0.01f);
        TestTrue(*(Name + TEXT(": nor wider than the page")), Plan.Size.X <= PageWidth);
        TestTrue(*(Name + TEXT(": it stays readable")), Plan.Radius >= (bNarrow ? 60.0f : 80.0f));
        TestTrue(*(Name + TEXT(": and does not take over the screen")), Plan.Size.Y <= Flow.Y * 0.6f);
        TestEqual(*(Name + TEXT(": narrow pages use the compact labels")), Plan.bCompact, bNarrow);
        for (int32 Index = 0; Index < Count; ++Index)
            TestTrue(*FString::Printf(TEXT("%s: label %d is inside the widget"), *Name, Index), Plan.LabelBox(Index, Count).IsInside(Plan.Size));
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const FVector2f Tip = PWRadar::Vertex(Plan.Centre(), Plan.Radius, Index, Count, 1.0f);
            TestTrue(*FString::Printf(TEXT("%s: vertex %d is inside the widget"), *Name, Index),
                     Tip.X >= 0.0f && Tip.Y >= 0.0f && Tip.X <= Plan.Size.X && Tip.Y <= Plan.Size.Y);
        }
        // Labels sit outside the outer ring, so none can overlap another's polygon vertex.
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const PWRadar::FRect Box = Plan.LabelBox(Index, Count);
            const FVector2f Tip = PWRadar::Vertex(Plan.Centre(), Plan.Radius, Index, Count, 1.0f);
            const bool bOverlaps = Tip.X > Box.Min.X && Tip.X < Box.Max.X && Tip.Y > Box.Min.Y && Tip.Y < Box.Max.Y;
            TestFalse(*FString::Printf(TEXT("%s: label %d does not cover its own vertex"), *Name, Index), bOverlaps);
        }
    }

    // The real widget reports the same size, and shrinks with the room it is given.
    FPgScene Scene;
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;
    Scene.Creator->ApplyBalancedScholarPreset();
    const TSharedRef<FPWCreationModel> Model = MakeShared<FPWCreationModel>(Scene.Creator);
    float Available = 600.0f;
    const TSharedRef<SPWAttributeRadar> Radar = SNew(SPWAttributeRadar, Model)
        .AvailableWidth_Lambda([&Available]() { return Available; })
        .MaxRadius(150.0f);
    float LastWidth = 1.0e9f;
    for (const float Room : {600.0f, 420.0f, 340.0f, 300.0f})
    {
        Available = Room;
        const FVector2D Desired = Radar->ComputeDesiredSize(1.0f);
        TestTrue(*FString::Printf(TEXT("widget at %.0f wide fits"), Room), Desired.X <= Room + 0.01);
        TestTrue(TEXT("a smaller room never gives a larger chart"), Desired.X <= LastWidth + 0.01f);
        LastWidth = static_cast<float>(Desired.X);
    }
    TestFalse(TEXT("the chart is display only: it takes no focus"), Radar->SupportsKeyboardFocus());
    TestTrue(TEXT("and is invisible to hit tests"), Radar->GetVisibility() == EVisibility::HitTestInvisible);
    TestTrue(TEXT("a screen reader is told every domain and the points left"),
             Radar->GetSummaryText().ToString().Contains(TEXT("points remaining")) && Radar->GetSummaryText().ToString().Contains(TEXT("Physical")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPgRadarPageTest, "PerfectWorld.AttributeRadar.TheTraitsPageHostsTheChartAtEverySize",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPgRadarPageTest::RunTest(const FString& Parameters)
{
    using namespace PWAttributeRadarTest;
    FPgScene Scene;
    if (!TestNotNull(TEXT("creator"), Scene.Creator)) return false;
    Scene.Creator->ApplyBalancedScholarPreset();
    for (const bool bNarrow : {false, true})
    {
        const TSharedRef<FPWCreationModel> Model = MakeShared<FPWCreationModel>(Scene.Creator);
        const TSharedRef<SPWTraitsPage> Page = SNew(SPWTraitsPage, Model).NarrowLayout(bNarrow);
        Page->SlatePrepass(1.0f);
        TestEqual(*FString::Printf(TEXT("narrow=%d: one radar is on the page"), bNarrow ? 1 : 0), PgCountType(Page, TEXT("SPWAttributeRadar")), 1);
        TestTrue(*FString::Printf(TEXT("narrow=%d: and there are rank selectors to focus"), bNarrow ? 1 : 0), PgCountType(Page, TEXT("SPWRankSelector")) > 0);
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
