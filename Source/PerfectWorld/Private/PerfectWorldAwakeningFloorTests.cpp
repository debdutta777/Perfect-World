// R9-06 Awakening floor, pure parts: the catalogue, the authored layout (stations at their authored positions, reach, safe area,
// hazard cues, asset coverage), the transition plan and the scripted bridge. No world is spawned here.
// Run in the editor under "PerfectWorld.AwakeningFloor" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldAwakeningTestSupport.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldSchoolCampus.h"

using namespace PWAwakeningTest;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwCatalogueTest, "PerfectWorld.AwakeningFloor.Catalogue",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwCatalogueTest::RunTest(const FString& Parameters)
{
    const TArray<FAwSituationDef>& All = AwCatalogue();
    TestEqual(TEXT("three situations"), All.Num(), SituationCount);
    TestEqual(TEXT("situation 0 id"), All[0].Id.ToString(), FString(Ids::StalledCrossing));
    TestEqual(TEXT("situation 1 id"), All[1].Id.ToString(), FString(Ids::ShiftingCorridor));
    TestEqual(TEXT("situation 2 id"), All[2].Id.ToString(), FString(Ids::SharedDelivery));

    TSet<FName> Seen;
    TSet<int32> Stances;
    for (const FAwSituationDef& Situation : All)
    {
        TestEqual(*FString::Printf(TEXT("%s has three choices"), *Situation.Id.ToString()), Situation.Choices.Num(), 3);
        TestTrue(TEXT("a station title and a summary"), !Situation.Title.IsEmpty() && !Situation.Summary.IsEmpty());
        for (const FAwChoiceDef& Choice : Situation.Choices)
        {
            TestFalse(*FString::Printf(TEXT("choice id %s is unique"), *Choice.Id.ToString()), Seen.Contains(Choice.Id));
            Seen.Add(Choice.Id);
            Stances.Add(static_cast<int32>(Choice.Stance));
            TestTrue(TEXT("a label, a verb and a caption"), !Choice.Label.IsEmpty() && !Choice.Verb.IsEmpty() && !Choice.Caption.IsEmpty());
            TestEqual(TEXT("labels are capitals"), Choice.Label, Choice.Label.ToUpper());
            TestTrue(TEXT("the choice id is namespaced"), Choice.Id.ToString().StartsWith(TEXT("awakening.choice.")));
            // A caption describes evidence ("You steadied ..."), never a verdict on the person.
            const FString Caption = Choice.Caption.ToString().ToLower();
            for (const TCHAR* Verdict : {TEXT("brave"), TEXT("coward"), TEXT("fail"), TEXT("score"), TEXT("wrong"), TEXT("weak")})
            {
                TestFalse(*FString::Printf(TEXT("caption of %s has no verdict word '%s'"), *Choice.Id.ToString(), Verdict), Caption.Contains(Verdict));
            }
            TestTrue(TEXT("the choice's effect is within its mover's poses"), Choice.Effect.PrimaryPose <= 1 && Choice.Effect.SecondaryPose <= 2);
        }
        TestNotNull(TEXT("the withdrawal is offered at every situation"), AwFindChoice(Situation.Id, FName(Ids::WithdrawToSafety)));
    }
    // The plan's substantive choices all exist: help, observe, protect, lead, coordinate, refuse to fight, withdraw.
    for (const EAwStance Needed : {EAwStance::Help, EAwStance::Observe, EAwStance::Protect, EAwStance::Lead, EAwStance::Coordinate, EAwStance::Decline})
    {
        TestTrue(*FString::Printf(TEXT("stance %s is offered"), AwStanceName(Needed)), Stances.Contains(static_cast<int32>(Needed)));
    }
    TestTrue(TEXT("the withdrawal stance"), AwWithdrawDef().Stance == EAwStance::Withdraw);
    TestEqual(TEXT("withdrawal has no block effect"), AwWithdrawDef().Effect.PrimaryPose, -1);
    TestEqual(TEXT("ten distinct choice ids with the withdrawal"), AwAllChoiceIds().Num(), 10);
    TestNull(TEXT("a choice of another situation is not offered"), AwFindChoice(FName(Ids::StalledCrossing), FName(Ids::ProtectBarrier)));
    TestEqual(TEXT("index of an unknown situation"), AwSituationIndex(FName(TEXT("awakening.situation.nope"))), static_cast<int32>(INDEX_NONE));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwTransitionPlanTest, "PerfectWorld.AwakeningFloor.TransitionPlan",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwTransitionPlanTest::RunTest(const FString& Parameters)
{
    const FAwTransitionPlan Normal = AwPlanTransition(false);
    TestTrue(TEXT("normal motion fades"), Normal.bFade && Normal.FadeOutSeconds > 0.0f && Normal.FadeInSeconds > 0.0f);
    TestFalse(TEXT("normal motion announces after the move"), Normal.bAnnounceFirst);
    TestTrue(TEXT("a normal transfer is under a second"), Normal.TotalSeconds() < 1.0f);

    const FAwTransitionPlan Reduced = AwPlanTransition(true);
    TestFalse(TEXT("reduced motion never fades"), Reduced.bFade);
    TestEqual(TEXT("no fade-out time"), Reduced.FadeOutSeconds, 0.0f);
    TestEqual(TEXT("no fade-in time"), Reduced.FadeInSeconds, 0.0f);
    TestTrue(TEXT("the cut is announced first and the view holds still"), Reduced.bAnnounceFirst && Reduced.HoldSeconds >= 1.0f);

    TestTrue(TEXT("the override wins: reduced"), AwIsReducedMotion(TOptional<bool>(true)));
    TestFalse(TEXT("the override wins: normal"), AwIsReducedMotion(TOptional<bool>(false)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwLayoutTest, "PerfectWorld.AwakeningFloor.LayoutAuthoredPositions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwLayoutTest::RunTest(const FString& Parameters)
{
    FAwFloorLayout L;
    const FAwFloorSite Site = AwTSite();
    if (!TestTrue(TEXT("the layout builds"), AwBuildLayout(L, Site, FAwCast())))
    {
        return false;
    }
    TestFalse(TEXT("an invalid site builds nothing"), AwBuildLayout(L, FAwFloorSite(), FAwCast()));
    TestEqual(TEXT("three stations"), L.Stations.Num(), SituationCount);

    TSet<FName> PadIds;
    for (int32 I = 0; I < L.Stations.Num(); ++I)
    {
        const FAwStationSpec& S = L.Stations[I];
        const double Yb = AwNumbers::BayBaseY + AwNumbers::BayStride * I;
        const FVector Stand = Site.Origin + FVector(AwNumbers::StandX, Yb + AwNumbers::StandBack, 0.0);
        TestTrue(TEXT("the station's situation"), S.Situation == AwCatalogue()[I].Id);
        TestTrue(TEXT("the stand point is where it is authored"), S.Stand.Equals(Stand, 0.01));
        TestTrue(TEXT("the reset pad is the stand point"), S.ResetPad.Equals(Stand, 0.01));
        TestTrue(TEXT("the console is where it is authored"), S.Console.Equals(Site.Origin + FVector(1270.0, Yb + 696.0, 120.0), 0.01));
        TestEqual(TEXT("six pads"), S.Pads.Num(), AwNumbers::PadsPerStation);
        const EAwPadKind Kinds[] = {EAwPadKind::Choice, EAwPadKind::Choice, EAwPadKind::Choice, EAwPadKind::Withdraw, EAwPadKind::Advance, EAwPadKind::Reset};
        for (int32 K = 0; K < S.Pads.Num(); ++K)
        {
            const FAwPadSpec& Pad = S.Pads[K];
            TestTrue(TEXT("pad kind order"), Pad.Kind == Kinds[K]);
            TestFalse(*FString::Printf(TEXT("pad id %s is unique"), *Pad.Id.ToString()), PadIds.Contains(Pad.Id));
            PadIds.Add(Pad.Id);
            const double Angle = FMath::DegreesToRadians(AwNumbers::PadAngles[K]);
            const FVector Want = Stand + FVector(AwNumbers::PadRadiusCm * FMath::Sin(Angle), AwNumbers::PadRadiusCm * FMath::Cos(Angle), 0.0);
            TestTrue(*FString::Printf(TEXT("pad %s stands on the arc"), *Pad.Id.ToString()), Pad.Centre.Equals(Want, 0.01));
            TestTrue(TEXT("the interaction point is over the pad"), Pad.Point.Equals(Pad.Centre + FVector(0.0, 0.0, AwNumbers::PadPointZ), 0.01));
            // Reach is measured from the capsule axis (never from a camera), so first and third person reach identically; every pad is
            // inside the near zone, where a target counts whichever way the player faces.
            TestTrue(TEXT("every pad is in the near zone of the stand point"), FVector::Dist2D(Pad.Point, Stand) <= PWInteraction::NearZoneCm);
            TestTrue(TEXT("every pad is within reach of the capsule axis"),
                     FVector::Dist(Pad.Point, FVector(Stand.X, Stand.Y, Pad.Point.Z)) <= PWInteraction::ReachCm);
            TestTrue(TEXT("a label and a shape on every pad"), !Pad.Label.IsEmpty());
            TArray<TPair<FVector, FVector>> Boxes;
            AwShapeBoxes(Pad.Shape, Boxes);
            TestTrue(TEXT("the shape has raised bars"), Boxes.Num() > 0);
            for (int32 J = K + 1; J < S.Pads.Num(); ++J)
            {
                TestTrue(TEXT("pad slabs do not touch"), FVector::Dist2D(Pad.Centre, S.Pads[J].Centre) > AwNumbers::PadSizeCm + 2.0);
            }
        }
        TestTrue(TEXT("the fourth pad is the withdrawal"), S.Pads[3].Choice == AwWithdrawDef().Id);
        TestTrue(TEXT("the withdrawal wears the back arrow"), S.Pads[3].Shape == EAwShape::Back);
        TestNotNull(TEXT("a reset plate"), L.FindPart(*FString::Printf(TEXT("s%d.reset_plate"), I + 1)));
        TestTrue(TEXT("hazard stripes (shape) for the station"), L.FindParts(FString::Printf(TEXT("s%d.hazard.stripe"), I + 1)).Num() > 0);
        TestTrue(TEXT("two movers with poses"), S.Primary.Poses.Num() >= 2 && S.Secondary.Poses.Num() >= 2);
        TestTrue(TEXT("the figure stands inside the room"), L.Bounds.IsInside(S.FigureFeet + FVector(0.0, 0.0, 1.0)));
    }
    TestTrue(TEXT("crossing and corridor mark a hazard"), L.Stations[0].Hazard.IsValid && L.Stations[1].Hazard.IsValid);
    TestTrue(TEXT("a text sign names the gap"), L.FindPart(TEXT("s1.hazard.surface")) != nullptr);
    bool bGapText = false;
    for (const FAwText& T : L.Texts)
    {
        bGapText |= T.Text.StartsWith(TEXT("GAP"));
    }
    TestTrue(TEXT("hazard cues are shape and words"), bGapText);

    // The safe area: the teacher and classmates are in it, the player's withdrawal lands in it, and its barriers are soft.
    TestTrue(TEXT("the safe stand is inside the safe area"), L.SafeArea.IsInside(L.SafeStand + FVector(0.0, 0.0, 1.0)));
    int32 InSafe = 0;
    for (const FAwFigureSpec& F : L.Figures)
    {
        const bool bIn = L.SafeArea.IsInside(F.Feet + FVector(0.0, 0.0, 1.0));
        InSafe += bIn ? 1 : 0;
        TestTrue(TEXT("every figure is inside the room"), L.Bounds.IsInside(F.Feet + FVector(0.0, 0.0, 1.0)));
        if (F.bTeacher)
        {
            TestTrue(TEXT("the teacher is in the safe area"), bIn);
        }
    }
    TestEqual(TEXT("the teacher and two classmates are in the safe area"), InSafe, 3);
    TestEqual(TEXT("six people on the floor"), L.Figures.Num(), 6);
    const TArray<const FAwPart*> Barriers = L.FindParts(TEXT("safe.barrier"));
    TestTrue(TEXT("the safe area has light barriers"), Barriers.Num() > 10);
    for (const FAwPart* Barrier : Barriers)
    {
        TestTrue(TEXT("light barriers are soft: no collision"), Barrier->Role == EAwRole::Visual);
    }

    // Nothing the player must reach stands inside a solid part.
    const auto InsideSolid = [&L](const FVector& Feet)
    {
        for (const FAwPart& Part : L.Parts)
        {
            if (Part.Role != EAwRole::Visual && Part.Id != FName(TEXT("floor.panel")) && Part.Box.IsInside(Feet + FVector(0.0, 0.0, 20.0)))
            {
                return true;
            }
        }
        return false;
    };
    TestFalse(TEXT("the arrival is clear"), InsideSolid(L.Arrival));
    TestFalse(TEXT("the safe stand is clear"), InsideSolid(L.SafeStand));
    for (const FAwStationSpec& S : L.Stations)
    {
        TestFalse(TEXT("the stand is clear"), InsideSolid(S.Stand));
        TestFalse(TEXT("the figure is clear"), InsideSolid(S.FigureFeet));
    }
    // Everything except the campus terminal lies inside the sealed room.
    const FBox Room = L.Bounds.ExpandBy(FVector(40.0, 40.0, 60.0));
    for (const FAwPart& Part : L.Parts)
    {
        if (!Part.Id.ToString().StartsWith(TEXT("terminal")))
        {
            TestTrue(*FString::Printf(TEXT("part %s is inside the room"), *Part.Id.ToString()), Room.IsInside(Part.Box.Min) && Room.IsInside(Part.Box.Max));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwAssetCoverageTest, "PerfectWorld.AwakeningFloor.AssetCoverage",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwAssetCoverageTest::RunTest(const FString& Parameters)
{
    FAwFloorLayout L;
    if (!TestTrue(TEXT("the layout builds"), AwBuildLayout(L, AwTSite(), FAwCast())))
    {
        return false;
    }
    const TArray<FName> Known = AwAllAssetIds();
    TSet<FName> Used;
    bool bTeal = false, bViolet = false;
    for (const FAwPart& Part : L.Parts)
    {
        TestTrue(*FString::Printf(TEXT("part %s names a Gate-kit asset"), *Part.Id.ToString()), Known.Contains(Part.AssetId));
        Used.Add(Part.AssetId);
        bTeal |= Part.Colour == EAwColour::GateTeal;
        bViolet |= Part.Colour == EAwColour::GateViolet;
    }
    for (const FAwStationSpec& S : L.Stations)
    {
        for (const FAwMoverSpec* Mover : {&S.Primary, &S.Secondary})
        {
            TestTrue(*FString::Printf(TEXT("mover %s names a Gate-kit asset"), *Mover->Id.ToString()), Known.Contains(Mover->AssetId));
            Used.Add(Mover->AssetId);
        }
    }
    for (const FName Id : Known)
    {
        TestTrue(*FString::Printf(TEXT("asset id %s follows the registry's id rule"), *Id.ToString()), AwIsValidAssetId(Id.ToString()));
        TestTrue(*FString::Printf(TEXT("asset id %s is category arch, subcategory gatekit"), *Id.ToString()), Id.ToString().StartsWith(TEXT("arch.gatekit.")));
        TestTrue(*FString::Printf(TEXT("asset id %s is used by the floor"), *Id.ToString()), Used.Contains(Id));
    }
    TestTrue(TEXT("the reserved Gate ramps (teal and violet) are used"), bTeal && bViolet);
    TestFalse(TEXT("an uppercase id is refused"), AwIsValidAssetId(TEXT("Arch.gatekit.floor")));
    TestFalse(TEXT("two segments are refused"), AwIsValidAssetId(TEXT("arch.floor")));
    TestFalse(TEXT("five segments are refused"), AwIsValidAssetId(TEXT("a.b.c.d.e")));
    TestTrue(TEXT("a four segment id is fine"), AwIsValidAssetId(TEXT("arch.gatekit.floor_grid.variant")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwSiteTest, "PerfectWorld.AwakeningFloor.CampusSite",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwSiteTest::RunTest(const FString& Parameters)
{
    const FAwFloorSite Site = AwSiteFromCampus();
    if (!TestTrue(TEXT("the campus gives a valid site"), Site.bValid))
    {
        return false;
    }
    const PWSchool::FScCampus Campus = PWSchool::ScReadCampus();
    TestTrue(TEXT("the terminal stand is on the forecourt"), Campus.Forecourt.ContainsXY(Site.EntryStand.X, Site.EntryStand.Y));
    TestTrue(TEXT("the terminal is in front of the teaching block's east end"), Site.EntryKiosk.X > Campus.Teaching.X1 - 1000.0 && Site.EntryKiosk.X < Campus.Teaching.X1);
    TestEqual(TEXT("the terminal stands at the pedestrian datum"), Site.EntryKiosk.Z, PWSchool::ScGroundZ);
    TestTrue(TEXT("the floor is far under the campus"), Site.Origin.Z < PWSchool::ScGroundZ - 3000.0);
    TestTrue(TEXT("the floor lies under the site"), Campus.Site.ContainsXY(Site.Origin.X, Site.Origin.Y + 1900.0));

    FAwFloorLayout L;
    TestTrue(TEXT("the layout builds"), AwBuildLayout(L, Site, FAwCast()));
    TestTrue(TEXT("the entry pad is in reach of the stand"), FVector::Dist2D(L.EntryPad.Point, Site.EntryStand) <= PWInteraction::NearZoneCm);
    TestNotNull(TEXT("the terminal's collision"), L.FindPart(TEXT("terminal.collision")));
    const FAwPart* Plate = L.FindPart(TEXT("terminal.plate"));
    TestTrue(TEXT("the stand is on the terminal's plate"), Plate != nullptr && Plate->Box.IsInside(Site.EntryStand + FVector(0.0, 0.0, 0.5)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwScriptedBridgeTest, "PerfectWorld.AwakeningFloor.ScriptedBridge",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwScriptedBridgeTest::RunTest(const FString& Parameters)
{
    FAwScriptedBridge Bridge;
    FString Why;
    TestTrue(TEXT("entry is open by default"), Bridge.CanEnter(Why));
    FAwChoiceResult Early = Bridge.SubmitChoice(FName(Ids::StalledCrossing), FName(Ids::HelpClassmate));
    TestFalse(TEXT("a choice before the session is refused"), Early.bAccepted);
    TestTrue(TEXT("entering is accepted"), Bridge.Enter().bAccepted);

    FAwChoiceResult First = Bridge.SubmitChoice(FName(Ids::StalledCrossing), FName(Ids::HelpClassmate));
    TestTrue(TEXT("a catalogue choice is accepted"), First.bAccepted);
    TestTrue(TEXT("the situation is complete"), First.Phase == EAwPhase::Complete);
    TestFalse(TEXT("a repeat is refused"), Bridge.SubmitChoice(FName(Ids::StalledCrossing), FName(Ids::ObserveCycle)).bAccepted);
    TestFalse(TEXT("another situation's choice is refused"), Bridge.SubmitChoice(FName(Ids::ShiftingCorridor), FName(Ids::HelpClassmate)).bAccepted);

    Bridge.QueueAssistance(FName(Ids::ShiftingCorridor));
    const FAwChoiceResult Assisted = Bridge.SubmitChoice(FName(Ids::ShiftingCorridor), FName(Ids::BraceWall));
    TestTrue(TEXT("an assisted choice is accepted"), Assisted.bAccepted && Assisted.bAssistance);
    TestTrue(TEXT("it stays open"), Assisted.Phase == EAwPhase::Recovering);
    TestTrue(TEXT("the retry completes it"), Bridge.SubmitChoice(FName(Ids::ShiftingCorridor), FName(Ids::BraceWall)).Phase == EAwPhase::Complete);
    TestFalse(TEXT("the session is not complete yet"), Bridge.GetState().bSessionComplete);
    const FAwChoiceResult Last = Bridge.SubmitChoice(FName(Ids::SharedDelivery), FName(Ids::WithdrawToSafety));
    TestTrue(TEXT("withdrawal is accepted and completes the session"), Last.bAccepted && Last.bSessionComplete);
    TestEqual(TEXT("calls are recorded"), Bridge.CountSubmits(FName(Ids::ShiftingCorridor), FName(Ids::BraceWall)), 2);

    Bridge.ResetSession();
    TestFalse(TEXT("a reset clears completion"), Bridge.GetState().bSessionComplete);
    Bridge.SetEntryRefusal(TEXT("Not due yet."));
    TestFalse(TEXT("an entry refusal is honoured"), Bridge.CanEnter(Why));
    TestEqual(TEXT("with its reason"), Why, FString(TEXT("Not due yet.")));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
