// Checks for the production face of the three unstaffed browsing shops (R4-01; PerfectWorldLaneKitStalls.h and ...KitDisplays.h): the
// stalls leave the doorway's approach and a continuous 120 cm footpath route clear and never touch the interior's collision, and the
// browse actors carry the tags, verb and notice the acceptance scenario looks for. They run the real builders on bare scenes and read back
// the collision boxes; the last test spawns the actors in a throw-away world (play never begins).
// Run in the editor under "PerfectWorld.KitStalls" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EngineUtils.h"
#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneKitStallsTestSupport.h"
#include "PerfectWorldLaneProps.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldShopDisplayActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitStallsCollisionTest, "PerfectWorld.KitStalls.CollisionAndRoute",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitStallsCollisionTest::RunTest(const FString& Parameters)
{
    using namespace PWKitStallTest;
    FLaneScene Props; // the kerb-edge poles and trees of the real lane
    BuildLaneProps(Props);
    const TArray<FBox>& PropBoxes = Props.Blocks.BlockingBoxes();

    for (const TCHAR* Shop : StShops)
    {
        const FString Name = Shop;
        FLaneBuilding B;
        FLaneKitFrontage Kit;
        if (!StBuild(Shop, B, Kit))
        {
            AddError(FString::Printf(TEXT("%s: not a recognised kit frontage"), *Name));
            continue;
        }
        const LI::InteriorLayout Layout = MakeKitFrontageLayout(B, Kit);
        const LI::Opening* Entrance = Layout.Ok() ? KitFrontEntrance(Layout) : nullptr;
        if (Entrance == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: no layout or no entrance"), *Name));
            continue;
        }
        const double DoorU = 0.5 * (Entrance->Plan.U0 + Entrance->Plan.U1);
        FLaneScene KitScene;
        BuildKitFrontage(KitScene, B, Kit);
        FLaneScene Face;
        BuildKitPublicFace(Face, B, Kit);
        const TArray<FBox>& KitBoxes = KitScene.Blocks.BlockingBoxes();
        const TArray<FBox>& FaceBoxes = Face.Blocks.BlockingBoxes();

        TArray<FKitStallBay> Plan;
        PlanKitStalls(B, DoorU, Plan);
        int32 Expected = 0;
        for (const FKitStallBay& Bay : Plan)
        {
            Expected += KitStallCollisionBoxCount(Bay);
        }
        TestEqual(*FString::Printf(TEXT("%s: the public face adds exactly each stall's counter and crates as collision (the board and awnings add none)"), *Name),
                  FaceBoxes.Num(), Expected);

        const double FrontAbsY = FMath::Abs(B.Frame.Origin.Y);
        const double FloorAbs = GroundZ + Layout.FinishedFloorCm;
        for (const FBox& Box : FaceBoxes)
        {
            const double AbsMin = B.Side > 0 ? Box.Min.Y : -Box.Max.Y;
            const double AbsMax = B.Side > 0 ? Box.Max.Y : -Box.Min.Y;
            TestTrue(*FString::Printf(TEXT("%s: stall collision stands wholly outside the wall line (never in the sales room)"), *Name), AbsMax <= FrontAbsY + 0.01);
            TestTrue(*FString::Printf(TEXT("%s: stall collision keeps 120 cm of footpath between it and the kerb"), *Name),
                     AbsMin >= 250.0 + MinFootpathRoute - 0.01);
            TestFalse(*FString::Printf(TEXT("%s: stall collision touches no interior or wall collision"), *Name), StAnyOverlap(KitBoxes, Box));
        }

        // The entrance: the 120 cm in front of the doorway is free of the stalls, the kit and the lane's props.
        const FBox Approach = StBox(B, Layout, Entrance->Plan.U0, Entrance->Plan.U1, -LI::CirculationCm, 0, 2.0, LI::DoorClearHeightCm);
        TestFalse(*FString::Printf(TEXT("%s: the doorway's approach is clear of the stalls"), *Name), StAnyOverlap(FaceBoxes, Approach));
        TestFalse(*FString::Printf(TEXT("%s: the doorway's approach is clear of the kit's own collision"), *Name), StAnyOverlap(KitBoxes, Approach));
        TestFalse(*FString::Printf(TEXT("%s: the doorway's approach is clear of lane props"), *Name), StAnyOverlap(PropBoxes, Approach));

        // A continuous route of at least 120 cm along the whole frontage, between the kerb and the wall, past stalls, poles and trunks.
        TArray<const TArray<FBox>*> Sets;
        Sets.Add(&KitBoxes);
        Sets.Add(&FaceBoxes);
        Sets.Add(&PropBoxes);
        TArray<FStGap> Reachable;
        double Narrowest = 1.0e9;
        bool bBroken = false;
        for (double U = 0.0; U <= B.Width && !bBroken; U += 5.0)
        {
            const double X = B.Frame.Pt(U, 0.0, 0.0).X;
            TArray<FStGap> Gaps;
            StFreeGaps(Sets, B.Side, X, FrontAbsY, FloorAbs, MinFootpathRoute, Gaps);
            TArray<FStGap> Next;
            double Widest = 0.0;
            for (const FStGap& Gap : Gaps)
            {
                const bool bLinked = Reachable.IsEmpty() ||
                    Reachable.ContainsByPredicate([&](const FStGap& Old) { return FMath::Min(Old.Hi, Gap.Hi) - FMath::Max(Old.Lo, Gap.Lo) >= StCapsuleWidth; });
                if (bLinked)
                {
                    Next.Add(Gap);
                    Widest = FMath::Max(Widest, Gap.Hi - Gap.Lo);
                }
            }
            if (Next.IsEmpty())
            {
                AddError(FString::Printf(TEXT("%s: the 120 cm footpath route breaks at U %.0f (X %.0f)"), *Name, U, X));
                bBroken = true;
            }
            Narrowest = FMath::Min(Narrowest, Widest);
            Reachable = Next;
        }
        TestFalse(*FString::Printf(TEXT("%s: a continuous 120 cm route (narrowest free strip %.0f cm)"), *Name, Narrowest), bBroken);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWKitStallsBrowseTest, "PerfectWorld.KitStalls.BrowseDisplays",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPWKitStallsBrowseTest::RunTest(const FString& Parameters)
{
    using namespace PWKitStallTest;

    // The acceptance binding contract, spelled out.
    TestEqual(TEXT("tag, market_stalls_a"), BrowseActorTag(FName(TEXT("building.nabapara.market_stalls_a"))), FName(TEXT("interaction.nabapara.market_stalls_a.browse")));
    TestEqual(TEXT("tag, market_stalls_b"), BrowseActorTag(FName(TEXT("building.nabapara.market_stalls_b"))), FName(TEXT("interaction.nabapara.market_stalls_b.browse")));
    for (const TCHAR* R53Name : {TEXT("cloth_merchant"), TEXT("narrow_shop"), TEXT("tailor_shop")})
    {
        TestEqual(TEXT("round5 browse tag"), BrowseActorTag(FName(*(FString(TEXT("building.nabapara.")) + R53Name))),
                  FName(*(FString(TEXT("interaction.nabapara.")) + R53Name + TEXT(".browse"))));
    }
    TestEqual(TEXT("tag, end_shop"), BrowseActorTag(FName(TEXT("building.nabapara.end_shop"))), FName(TEXT("interaction.nabapara.end_shop.browse")));
    TestTrue(TEXT("an id that is not a building has no tag"), BrowseActorTag(FName(TEXT("door.nabapara.end_shop_front"))).IsNone());

    // The scenario is registered and well formed.
    TArray<PWAcceptance::FStepDef> Steps;
    TestTrue(TEXT("frontages_round4 is built"), PWAcceptance::BuildScenario(TEXT("frontages_round4"), Steps));
    TestTrue(TEXT("frontages_round4 starts by waiting for the world"), Steps.Num() >= 4 && Steps[0].Name == TEXT("world_ready"));
    TSet<FString> Seen;
    for (const PWAcceptance::FStepDef& Step : Steps)
    {
        TestFalse(*FString::Printf(TEXT("frontages_round4: step '%s' has a unique name"), *Step.Name), Seen.Contains(Step.Name));
        Seen.Add(Step.Name);
        TestTrue(*FString::Printf(TEXT("frontages_round4: step '%s' has a body and a timeout"), *Step.Name), static_cast<bool>(Step.Run) && Step.TimeoutSeconds > 0.0);
    }

    // The real spawn, from a scene the kit built (the doors it hung are what the spawn looks for).
    FLaneScene Scene;
    for (const TCHAR* Shop : StShops)
    {
        FLaneBuilding B;
        FLaneKitFrontage Kit;
        if (StBuild(Shop, B, Kit))
        {
            BuildKitFrontage(Scene, B, Kit);
        }
        else
        {
            AddError(FString::Printf(TEXT("%s: not a recognised kit frontage"), Shop));
        }
    }
    FStWorld Holder;
    if (Holder.World == nullptr)
    {
        AddError(TEXT("no test world"));
        return false;
    }
    TestEqual(TEXT("all six shops are served"), SpawnKitShopDisplays(*Holder.World, Scene), 6);

    int32 BrowseActors = 0;
    for (TActorIterator<APWShopBrowseActor> It(Holder.World); It; ++It)
    {
        ++BrowseActors;
    }
    TestEqual(TEXT("exactly six browse actors (the staffed shops get none)"), BrowseActors, 6);
    int32 SignActors = 0;
    for (TActorIterator<APWShopHoursSignActor> It(Holder.World); It; ++It)
    {
        ++SignActors;
    }
    TestEqual(TEXT("exactly six hours signs"), SignActors, 6);

    FActorSpawnParameters Spawn;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APerfectWorldPlayerCharacter* Player = Holder.World->SpawnActor<APerfectWorldPlayerCharacter>(
        APerfectWorldPlayerCharacter::StaticClass(), FTransform(FVector(0.0, 0.0, 500.0)), Spawn);
    UPWInteractionComponent* Interaction = Player != nullptr ? Player->FindComponentByClass<UPWInteractionComponent>() : nullptr;
    TestNotNull(TEXT("a player with an interaction component"), Interaction);

    for (const TCHAR* Shop : StShops)
    {
        const FString Name = Shop;
        FLaneBuilding B;
        FLaneKitFrontage Kit;
        if (!StBuild(Shop, B, Kit))
        {
            continue;
        }
        const LI::InteriorLayout Layout = MakeKitFrontageLayout(B, Kit);
        const LI::Anchor* Anchor = FindKitBrowseAnchor(Layout);
        const LI::Opening* Entrance = Layout.Ok() ? KitFrontEntrance(Layout) : nullptr;
        if (Anchor == nullptr || Entrance == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: no browse anchor or entrance in the layout"), *Name));
            continue;
        }
        const PerfectWorld::DoorDef* R53Door = PerfectWorld::FindDoor(Kit.Entry->EntranceDoorId);
        if (R53Door == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: no front door definition"), *Name));
            continue;
        }
        const FName Tag = BrowseActorTag(FName(Shop));
        APWShopBrowseActor* Found = nullptr;
        int32 Matches = 0;
        for (TActorIterator<APWShopBrowseActor> It(Holder.World); It; ++It)
        {
            if (It->ActorHasTag(Tag))
            {
                Found = *It;
                ++Matches;
            }
        }
        TestEqual(*FString::Printf(TEXT("%s: exactly one browse actor carries %s"), *Name, *Tag.ToString()), Matches, 1);
        if (Found == nullptr)
        {
            continue;
        }
        TestEqual(*FString::Printf(TEXT("%s: the verb"), *Name), Found->GetInteractionVerb(Player).ToString(), FString(TEXT("Browse goods")));
        TestEqual(*FString::Printf(TEXT("%s: priority above doors, below people"), *Name), Found->GetInteractionPriority(), 15);
        TestEqual(*FString::Printf(TEXT("%s: the building"), *Name), Found->GetBuildingId(), FName(Shop));

        // Position: in the sales room, at the anchor's reach, within the interaction reach, not inside any solid piece.
        const FVector Point = Found->GetInteractionPoint();
        TestTrue(*FString::Printf(TEXT("%s: the point is where the layout puts it"), *Name), Point.Equals(KitBrowsePoint(B, Layout, *Anchor), 0.1));
        const FVector Stand = KitPoint(B, Layout, Anchor->U, Anchor->V, 0.0);
        TestTrue(*FString::Printf(TEXT("%s: the point is within reach of the stand anchor"), *Name),
                 FVector::Dist2D(Point, Stand) <= KitPublic::BrowseReachCm + 0.5 && FVector::Dist2D(Point, Stand) <= PWInteraction::ReachCm);
        const LI::Room* Sales = LI::FindRoom(Layout, Layout.BuildingId + ".room.sales");
        const double LocalU = FVector::DotProduct(Point - B.Frame.Origin, B.Frame.U);
        const double LocalV = FVector::DotProduct(Point - B.Frame.Origin, B.Frame.V);
        TestTrue(*FString::Printf(TEXT("%s: the point is in the sales room"), *Name),
                 Sales != nullptr && Sales->Area.ContainsPoint(FMath::RoundToInt(LocalU), FMath::RoundToInt(LocalV)));
        const double Height = Point.Z - (GroundZ + Layout.FinishedFloorCm);
        bool bInside = false;
        for (const LI::Furniture& Piece : Layout.Pieces)
        {
            bInside |= Piece.Solid && LocalU > Piece.Footprint.U0 && LocalU < Piece.Footprint.U1 && LocalV > Piece.Footprint.V0 &&
                       LocalV < Piece.Footprint.V1 && Height > Piece.H0 && Height < Piece.H1;
        }
        TestFalse(*FString::Printf(TEXT("%s: the point is not inside a solid piece"), *Name), bInside);

        if (Player != nullptr && Interaction != nullptr)
        {
            Found->Interact(Player);
            const FString Notice = Interaction->GetNotice().ToString();
            TestTrue(*FString::Printf(TEXT("%s: Interact shows a notice with the hours, Unstaffed and Purchasing unavailable"), *Name), StHasEveryRequiredPhrase(Notice, *R53Door));
            TestFalse(*FString::Printf(TEXT("%s: the notice implies no buying or paying"), *Name), StImpliesCommerce(Notice));
        }

        // The sign: the same hours, facing the footpath.
        APWShopHoursSignActor* Sign = nullptr;
        for (TActorIterator<APWShopHoursSignActor> It(Holder.World); It; ++It)
        {
            if (It->GetBuildingId() == FName(Shop))
            {
                Sign = *It;
            }
        }
        if (Sign == nullptr)
        {
            AddError(FString::Printf(TEXT("%s: no hours sign"), *Name));
            continue;
        }
        TestTrue(*FString::Printf(TEXT("%s: the sign's text is the door's board text"), *Name), Sign->GetPostedText() == PostedSignText(*R53Door));
        TestTrue(*FString::Printf(TEXT("%s: the sign faces the footpath"), *Name), FVector::DotProduct(Sign->GetActorForwardVector(), -B.Frame.V) > 0.99);
        const double DoorU = 0.5 * (Entrance->Plan.U0 + Entrance->Plan.U1);
        TestTrue(*FString::Printf(TEXT("%s: the sign is over the door"), *Name), Sign->GetActorLocation().Equals(KitSignLocation(B, DoorU), 0.1));
    }
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
