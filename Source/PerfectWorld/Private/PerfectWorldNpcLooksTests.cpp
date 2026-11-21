// The person-looks gate (NR-04): every authored person stands at the height the cast table gives, wears a body that fits the
// capsule and the door, poses consistently, and reads as a different silhouette from every other authored person.
// Run in the editor under "PerfectWorld.NpcLooks" (Session Frontend, or the Unreal MCP AutomationTestToolset).

#include "PerfectWorldNpcLooks.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"

namespace PWNpcLooksTest
{
struct FNlPerson
{
    const TCHAR* Id;
    double HeightCm;
    EPWNpcOutfit Outfit;
    bool bBespoke;
};

// LANE_LIFE_PLAN section B heights, spelled out so a table edit cannot silently change them. Outfit is what the lane passes in today.
const FNlPerson NlPeople[] = {
    {TEXT("person.household_adult"), 168.0, EPWNpcOutfit::ParentHousehold, false},
    {TEXT("person.care_worker"), 162.0, EPWNpcOutfit::HouseholdAdult, false},
    {TEXT("person.farida_qadri"), 158.0, EPWNpcOutfit::ShopProprietor, false},
    {TEXT("person.mira_sen"), 160.0, EPWNpcOutfit::HouseholdAdult, true},
    {TEXT("person.anindita_sen"), 154.0, EPWNpcOutfit::ShopProprietor, true},
    {TEXT("person.subrata_dey"), 181.0, EPWNpcOutfit::HouseholdAdult, true},
    {TEXT("person.rupa_ghosh"), 157.0, EPWNpcOutfit::ParentHousehold, true},
    {TEXT("person.nirmal_bose"), 174.0, EPWNpcOutfit::ParentHousehold, true},
    {TEXT("person.sahana_mitra"), 164.0, EPWNpcOutfit::ParentHousehold, true},
    // Round 4 (R4-06). Heights frozen with the round plan; Mahua is 162 cm like the care-worker prototype, so the distinctness
    // test below accepts an equal height only when the silhouettes differ in at least three attributes.
    {TEXT("person.debashis_pal"), 171.0, EPWNpcOutfit::ParentHousehold, true},
    {TEXT("person.mahua_saha"), 162.0, EPWNpcOutfit::ParentHousehold, true},
    {TEXT("person.purnima_das"), 155.0, EPWNpcOutfit::ShopProprietor, true},
    {TEXT("person.arun_paul"), 176.0, EPWNpcOutfit::ParentHousehold, true},
    {TEXT("person.rekha_bose"), 161.0, EPWNpcOutfit::ParentHousehold, true},
    {TEXT("person.prabir_chatterjee"), 169.0, EPWNpcOutfit::HouseholdAdult, true},
    {TEXT("person.ishita_banerjee"), 165.0, EPWNpcOutfit::ParentHousehold, true},
    {TEXT("person.satyajit_dutta"), 167.0, EPWNpcOutfit::ShopProprietor, true},
    {TEXT("person.madhabi_das"), 156.0, EPWNpcOutfit::HouseholdAdult, true},
    {TEXT("person.arup_basu"), 179.0, EPWNpcOutfit::ParentHousehold, true},
};

constexpr double NlCapsuleRadius = 34.0;
constexpr double NlDoorHeight = 210.0;
constexpr double NlTolerance = 0.01;

TArray<FPWNpcBlock> NlBlocks(const FNlPerson& Person)
{
    TArray<FPWNpcBlock> Blocks;
    PWNpcLooks::BuildBlocks(FName(Person.Id), Person.Outfit, Person.HeightCm, Blocks);
    return Blocks;
}

uint32 NlGeometryHash(const TArray<FPWNpcBlock>& Blocks)
{
    uint32 Hash = 0;
    for (const FPWNpcBlock& Block : Blocks)
    {
        Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt(Block.Centre.X * 2.0), FMath::RoundToInt(Block.Centre.Y * 2.0),
                                                        FMath::RoundToInt(Block.Centre.Z * 2.0))));
        Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt(Block.Size.X * 2.0), FMath::RoundToInt(Block.Size.Y * 2.0),
                                                        FMath::RoundToInt(Block.Size.Z * 2.0))));
    }
    return Hash;
}

uint32 NlPaletteHash(const TArray<FPWNpcBlock>& Blocks)
{
    TSet<uint32> Colours;
    for (const FPWNpcBlock& Block : Blocks) { Colours.Add(Block.Tint.DWColor()); }
    uint32 Sum = 0;
    for (const uint32 Colour : Colours) { Sum += Colour * 2654435761u; } // order independent
    return Sum;
}

bool NlHasBlock(const TArray<FPWNpcBlock>& Blocks, TFunctionRef<bool(const FPWNpcBlock&)> Match)
{
    for (const FPWNpcBlock& Block : Blocks) { if (Match(Block)) { return true; } }
    return false;
}

FPWNpcPartRest NlRest(const FPWNpcBlock& Block)
{
    FPWNpcPartRest Rest;
    Rest.Group = Block.Group;
    Rest.Rest = Block.Centre;
    Rest.Pivot = Block.Pivot;
    return Rest;
}
} // namespace PWNpcLooksTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcLooksHeightsTest, "PerfectWorld.NpcLooks.AuthoredHeights",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcLooksHeightsTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcLooksTest;
    for (const FNlPerson& Person : NlPeople)
    {
        const FString Label = Person.Id;
        const FName Id(Person.Id);
        TestTrue(*(Label + TEXT(": the cast table has them")), PWNpc::HasAuthoredHeight(Id));
        TestNearlyEqual(*(Label + TEXT(": authored height")), PWNpc::StandingHeightCm(Id, Person.Outfit), Person.HeightCm, 0.001);
        TestNearlyEqual(*(Label + TEXT(": the outfit does not change an authored height")),
                        PWNpc::StandingHeightCm(Id, EPWNpcOutfit::ShopProprietor), Person.HeightCm, 0.001);
        TestNearlyEqual(*(Label + TEXT(": capsule half height")), PWNpc::CapsuleHalfHeightCm(Id, Person.Outfit), Person.HeightCm * 0.5, 0.001);
        TestTrue(*(Label + TEXT(": the head clears a 210 cm door by at least 15 cm")), Person.HeightCm <= NlDoorHeight - 15.0);
        const bool bBespokeInTable = PWNpcLooks::LookOf(Id) != PWNpcLooks::ELook::Outfit;
        TestEqual(*(Label + TEXT(": has a bespoke look exactly when the plan gives one")), bBespokeInTable, Person.bBespoke);
    }
    // Each id appears once in the table, and the coming residents are there with the heights the round plan froze.
    TSet<FString> Seen;
    for (const PWNpcLooks::FCastRow& Row : PWNpcLooks::CastTable())
    {
        bool bAlready = false;
        Seen.Add(FString(Row.PersonId), &bAlready);
        TestFalse(*(FString(Row.PersonId) + TEXT(": listed once")), bAlready);
    }
    TestNearlyEqual(TEXT("Rupa Ghosh is 157 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.rupa_ghosh")), EPWNpcOutfit::ParentHousehold), 157.0, 0.001);
    TestNearlyEqual(TEXT("Nirmal Bose is 174 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.nirmal_bose")), EPWNpcOutfit::ParentHousehold), 174.0, 0.001);
    TestNearlyEqual(TEXT("Sahana Mitra is 164 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.sahana_mitra")), EPWNpcOutfit::ParentHousehold), 164.0, 0.001);
    // Round 4: the five newcomers' authoritative heights.
    TestNearlyEqual(TEXT("Debashis Pal is 171 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.debashis_pal")), EPWNpcOutfit::ParentHousehold), 171.0, 0.001);
    TestNearlyEqual(TEXT("Mahua Saha is 162 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.mahua_saha")), EPWNpcOutfit::ParentHousehold), 162.0, 0.001);
    TestNearlyEqual(TEXT("Purnima Das is 155 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.purnima_das")), EPWNpcOutfit::ShopProprietor), 155.0, 0.001);
    TestNearlyEqual(TEXT("Arun Paul is 176 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.arun_paul")), EPWNpcOutfit::ParentHousehold), 176.0, 0.001);
    TestNearlyEqual(TEXT("Rekha Bose is 161 cm"), PWNpc::StandingHeightCm(FName(TEXT("person.rekha_bose")), EPWNpcOutfit::ParentHousehold), 161.0, 0.001);
    // Nobody who stood in the table before round 4 has moved: these are the heights they had at the round-4 baseline (083b0e2).
    const struct { const TCHAR* Id; double Cm; } Baseline[] = {
        {TEXT("person.farida_qadri"), 158.0}, {TEXT("person.mira_sen"), 160.0}, {TEXT("person.ishan_sheikh"), 173.0},
        {TEXT("person.tara_das"), 168.0}, {TEXT("person.amrita_basu"), 163.0}, {TEXT("person.ananya_chatterjee"), 166.0},
        {TEXT("person.amit_roy"), 178.0}, {TEXT("person.care_worker"), 162.0}, {TEXT("person.household_adult"), 168.0},
        {TEXT("person.anindita_sen"), 154.0}, {TEXT("person.leela_dutta"), 176.0}, {TEXT("person.subrata_dey"), 181.0},
        {TEXT("person.rupa_ghosh"), 157.0}, {TEXT("person.nirmal_bose"), 174.0}, {TEXT("person.sahana_mitra"), 164.0}};
    for (const auto& Row : Baseline)
    {
        TestNearlyEqual(*(FString(Row.Id) + TEXT(": height unchanged since the round-4 baseline")),
                        PWNpc::StandingHeightCm(FName(Row.Id), EPWNpcOutfit::ParentHousehold), Row.Cm, 0.001);
    }
    // The supported player band is 145 to 195 cm (ASSET_PIPELINE, proportions policy: 1.45 to 1.95 m); everyone authored stands inside it.
    for (const PWNpcLooks::FCastRow& Row : PWNpcLooks::CastTable())
    {
        TestTrue(*(FString(Row.PersonId) + TEXT(": inside the supported 145 to 195 cm band")), Row.HeightCm >= 145.0 && Row.HeightCm <= 195.0);
    }
    // A stranger still takes the outfit's default (UP-04), so a new person always gets a body.
    const FName Stranger(TEXT("person.not_in_the_table"));
    TestFalse(TEXT("a stranger has no authored height"), PWNpc::HasAuthoredHeight(Stranger));
    TestTrue(TEXT("a stranger has no bespoke look"), PWNpcLooks::LookOf(Stranger) == PWNpcLooks::ELook::Outfit);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcLooksBodyFitsTest, "PerfectWorld.NpcLooks.BodyFitsCapsuleAndPose",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcLooksBodyFitsTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcLooksTest;
    for (const FNlPerson& Person : NlPeople)
    {
        const FString Label = Person.Id;
        const double H = Person.HeightCm;
        const TArray<FPWNpcBlock> Blocks = NlBlocks(Person);
        if (!TestTrue(*(Label + TEXT(": the body has blocks")), Blocks.Num() > 15)) { continue; }

        double Top = -1e9, Bottom = 1e9;
        int32 LegsL = 0, LegsR = 0, ArmsL = 0, ArmsR = 0;
        bool bInside = true, bPositive = true, bFace = false;
        for (const FPWNpcBlock& Block : Blocks)
        {
            Top = FMath::Max(Top, Block.Centre.Z + Block.Size.Z * 0.5);
            Bottom = FMath::Min(Bottom, Block.Centre.Z - Block.Size.Z * 0.5);
            bInside &= FMath::Abs(Block.Centre.X) + Block.Size.X * 0.5 <= NlCapsuleRadius + NlTolerance;
            bInside &= FMath::Abs(Block.Centre.Y) + Block.Size.Y * 0.5 <= NlCapsuleRadius + NlTolerance;
            bPositive &= Block.Size.X > 0.0 && Block.Size.Y > 0.0 && Block.Size.Z > 0.0;
            bFace |= Block.Centre.X > 10.0 && Block.Centre.Z > 0.85 * H;
            LegsL += Block.Group == EPWNpcPartGroup::LegLeft ? 1 : 0;
            LegsR += Block.Group == EPWNpcPartGroup::LegRight ? 1 : 0;
            ArmsL += Block.Group == EPWNpcPartGroup::ArmLeft ? 1 : 0;
            ArmsR += Block.Group == EPWNpcPartGroup::ArmRight ? 1 : 0;
        }
        TestNearlyEqual(*(Label + TEXT(": the head top is the authored height")), Top, H, 0.05);
        TestNearlyEqual(*(Label + TEXT(": the feet are on the floor")), Bottom, 0.0, 0.05);
        TestTrue(*(Label + TEXT(": every block is inside the 34 cm capsule")), bInside);
        TestTrue(*(Label + TEXT(": every block has a size")), bPositive);
        TestTrue(*(Label + TEXT(": a face looks along +X")), bFace);
        TestTrue(*(Label + TEXT(": two legs and two arms swing")), LegsL > 0 && LegsR > 0 && ArmsL > 0 && ArmsR > 0);

        // Pose: standing leaves every block where it rests; sitting drops the body 0.2 H and swings the legs forward.
        const FPWNpcPose Standing;
        FPWNpcPose Sitting;
        Sitting.Sit = 1.0f;
        double SeatedTop = -1e9, SeatedLowestCentre = 1e9, LegForward = 0.0;
        bool bStandingRests = true, bHeadStays = true;
        for (const FPWNpcBlock& Block : Blocks)
        {
            const FPWNpcPartRest Rest = NlRest(Block);
            FVector Where;
            FQuat Turn;
            PWNpcLooks::PoseBlock(Rest, Standing, H, Where, Turn);
            bStandingRests &= Where.Equals(Block.Centre, 0.001) && Turn.Equals(FQuat::Identity, 0.0001);
            PWNpcLooks::PoseBlock(Rest, Sitting, H, Where, Turn);
            SeatedLowestCentre = FMath::Min(SeatedLowestCentre, Where.Z);
            if (Block.Group == EPWNpcPartGroup::Body)
            {
                SeatedTop = FMath::Max(SeatedTop, Where.Z + Block.Size.Z * 0.5);
                bHeadStays &= FMath::Abs(Where.X - Block.Centre.X) < 0.001;
            }
            else if (Block.Group == EPWNpcPartGroup::LegLeft || Block.Group == EPWNpcPartGroup::LegRight)
            {
                LegForward = FMath::Max(LegForward, Where.X);
            }
        }
        TestTrue(*(Label + TEXT(": the standing pose leaves every block at rest")), bStandingRests);
        TestNearlyEqual(*(Label + TEXT(": seated, the head is 0.2 H lower")), SeatedTop, H * 0.8, 0.05);
        TestTrue(*(Label + TEXT(": seated, the upper body does not slide")), bHeadStays);
        TestTrue(*(Label + TEXT(": seated, the thighs point forward")), LegForward > 0.15 * H);
        TestTrue(*(Label + TEXT(": seated, no block centre sinks through the floor")), SeatedLowestCentre > 0.0);

        // A swung arm moves its sleeve forward and leaves the torso where it was.
        FPWNpcPose Swinging;
        Swinging.ArmPitch[1] = 40.0f;
        double ArmMovedX = 0.0;
        bool bTorsoStill = true;
        for (const FPWNpcBlock& Block : Blocks)
        {
            FVector Where;
            FQuat Turn;
            PWNpcLooks::PoseBlock(NlRest(Block), Swinging, H, Where, Turn);
            if (Block.Group == EPWNpcPartGroup::ArmRight) { ArmMovedX = FMath::Max(ArmMovedX, Where.X - Block.Centre.X); }
            else { bTorsoStill &= Where.Equals(Block.Centre, 0.001); }
        }
        TestTrue(*(Label + TEXT(": a right-arm swing moves the right arm forward")), ArmMovedX > 5.0);
        TestTrue(*(Label + TEXT(": and nothing else")), bTorsoStill);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcLooksSilhouettesTest, "PerfectWorld.NpcLooks.DistinctSilhouettes",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcLooksSilhouettesTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcLooksTest;
    // LANE_LIFE_PLAN section B: every pair differs in at least two silhouette attributes; here also in the generated blocks.
    for (int32 A = 0; A < UE_ARRAY_COUNT(NlPeople); ++A)
    {
        for (int32 B = A + 1; B < UE_ARRAY_COUNT(NlPeople); ++B)
        {
            const FString Pair = FString(NlPeople[A].Id) + TEXT(" / ") + NlPeople[B].Id;
            const PWNpcLooks::FSilhouette SA = PWNpcLooks::SilhouetteOf(FName(NlPeople[A].Id), NlPeople[A].Outfit);
            const PWNpcLooks::FSilhouette SB = PWNpcLooks::SilhouetteOf(FName(NlPeople[B].Id), NlPeople[B].Outfit);
            TestTrue(*(Pair + TEXT(": differ in at least two silhouette attributes")), PWNpcLooks::SilhouetteDifferences(SA, SB) >= 2);
            const TArray<FPWNpcBlock> BlocksA = NlBlocks(NlPeople[A]);
            const TArray<FPWNpcBlock> BlocksB = NlBlocks(NlPeople[B]);
            TestTrue(*(Pair + TEXT(": different block geometry")), NlGeometryHash(BlocksA) != NlGeometryHash(BlocksB));
            TestTrue(*(Pair + TEXT(": different palette")), NlPaletteHash(BlocksA) != NlPaletteHash(BlocksB));
            TestTrue(*(Pair + TEXT(": different height, or at least three silhouette attributes apart")),
                     !FMath::IsNearlyEqual(NlPeople[A].HeightCm, NlPeople[B].HeightCm, 0.5) || PWNpcLooks::SilhouetteDifferences(SA, SB) >= 3);
        }
    }

    // The named features of the cast table rows, found in the blocks.
    const auto Blocks = [](const TCHAR* Id)
    {
        for (const FNlPerson& Person : NlPeople) { if (FString(Person.Id) == Id) { return NlBlocks(Person); } }
        return TArray<FPWNpcBlock>();
    };
    const TArray<FPWNpcBlock> Mira = Blocks(TEXT("person.mira_sen"));
    TestTrue(TEXT("Mira: a square satchel at her left hip"), NlHasBlock(Mira, [](const FPWNpcBlock& B)
        { return B.Centre.Y < -28.0 && B.Centre.Z > 50.0 && B.Centre.Z < 70.0 && FMath::IsNearlyEqual(B.Size.Y, 7.0) && B.Size.X > 8.0; }));
    TestTrue(TEXT("Mira: a navy block over the torso"), NlHasBlock(Mira, [](const FPWNpcBlock& B) { return B.Tint == FColor(30, 42, 86) && B.Group == EPWNpcPartGroup::Body && B.Size.Z > 40.0; }));
    TestTrue(TEXT("Mira: a bob wider than the head"), NlHasBlock(Mira, [](const FPWNpcBlock& B) { return B.Tint == FColor(28, 24, 28) && B.Size.Y > 24.0 && B.Size.Z > 15.0; }));
    const TArray<FPWNpcBlock> Anindita = Blocks(TEXT("person.anindita_sen"));
    TestTrue(TEXT("Anindita: a paper-coloured apron"), NlHasBlock(Anindita, [](const FPWNpcBlock& B) { return B.Tint == FColor(240, 232, 208) && B.Size.Z > 50.0; }));
    TestTrue(TEXT("Anindita: a plum blouse"), NlHasBlock(Anindita, [](const FPWNpcBlock& B) { return B.Tint == FColor(112, 46, 92); }));
    TestFalse(TEXT("Anindita does not share Farida's terracotta apron"), NlHasBlock(Anindita, [](const FPWNpcBlock& B) { return B.Tint == FColor(176, 86, 59); }));
    const TArray<FPWNpcBlock> Subrata = Blocks(TEXT("person.subrata_dey"));
    TestTrue(TEXT("Subrata: a clipboard held at the chest"), NlHasBlock(Subrata, [](const FPWNpcBlock& B)
        { return B.Size.Z > 25.0 && B.Size.X < 3.0 && B.Centre.X > 9.0 && B.Tint == FColor(166, 126, 82); }));
    TestTrue(TEXT("Subrata: narrower than the care worker"), [&]
        {
            double Narrow = 0.0, Wide = 0.0;
            for (const FPWNpcBlock& B : Subrata) { if (B.Group == EPWNpcPartGroup::Body && FMath::Abs(B.Centre.Z - 0.66 * 181.0) < 1.0) { Narrow = FMath::Max(Narrow, B.Size.Y); } }
            for (const FPWNpcBlock& B : Blocks(TEXT("person.care_worker"))) { if (B.Group == EPWNpcPartGroup::Body && FMath::Abs(B.Centre.Z - 0.66 * 162.0) < 1.0) { Wide = FMath::Max(Wide, B.Size.Y); } }
            return Narrow > 0.0 && Narrow < Wide;
        }());
    const TArray<FPWNpcBlock> Rupa = Blocks(TEXT("person.rupa_ghosh"));
    TestTrue(TEXT("Rupa: a saree wrapped to the ankle, with a mustard border at the hem"), NlHasBlock(Rupa, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(212, 168, 52) && B.Centre.Z < 10.0 && B.Group == EPWNpcPartGroup::LegLeft; })
        && NlHasBlock(Rupa, [](const FPWNpcBlock& B) { return B.Tint == FColor(36, 98, 110) && B.Group == EPWNpcPartGroup::LegRight && B.Size.Z > 0.4 * 157.0; }));
    TestTrue(TEXT("Rupa: a drape over the hips wider than the thighs"), NlHasBlock(Rupa, [](const FPWNpcBlock& B)
        { return B.Group == EPWNpcPartGroup::Body && B.Tint == FColor(36, 98, 110) && B.Size.Y >= 42.0 && B.Size.X >= 26.0; }));
    TestTrue(TEXT("Rupa: a pallu over the left shoulder"), NlHasBlock(Rupa, [](const FPWNpcBlock& B) { return B.Centre.Y < -10.0 && B.Centre.Z > 0.84 * 157.0 && B.Tint == FColor(36, 98, 110); }));
    const TArray<FPWNpcBlock> Nirmal = Blocks(TEXT("person.nirmal_bose"));
    TestTrue(TEXT("Nirmal: the broadest torso on the lane"), NlHasBlock(Nirmal, [](const FPWNpcBlock& B) { return B.Group == EPWNpcPartGroup::Body && B.Size.Y >= 48.0 && B.Size.X >= 24.0; }));
    TestTrue(TEXT("Nirmal: a flat cap with a brim"), NlHasBlock(Nirmal, [](const FPWNpcBlock& B) { return B.Tint == FColor(96, 96, 100) && B.Size.X >= 24.0; })
              && NlHasBlock(Nirmal, [](const FPWNpcBlock& B) { return B.Tint == FColor(96, 96, 100) && B.Centre.X >= 13.0; }));
    const TArray<FPWNpcBlock> Sahana = Blocks(TEXT("person.sahana_mitra"));
    TestTrue(TEXT("Sahana: a ponytail down her back"), NlHasBlock(Sahana, [](const FPWNpcBlock& B)
        { return B.Centre.X < -11.0 && B.Size.Z > 30.0 && B.Centre.Z > 164.0 - 45.0 && B.Tint == FColor(46, 32, 30); }));
    TestTrue(TEXT("Sahana: round glasses"), NlHasBlock(Sahana, [](const FPWNpcBlock& B) { return B.Tint == FColor(30, 28, 34) && B.Centre.X > 11.0; }));
    TestTrue(TEXT("Sahana: a knee-length kurta"), NlHasBlock(Sahana, [](const FPWNpcBlock& B) { return B.Tint == FColor(58, 64, 126) && B.Size.Y >= 42.0; }));

    // Round 4 (R4-06): the named features of the five newcomers.
    const TArray<FPWNpcBlock> Debashis = Blocks(TEXT("person.debashis_pal"));
    TestTrue(TEXT("Debashis: a leather apron over the torso"), NlHasBlock(Debashis, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(96, 64, 40) && B.Centre.X > 11.0 && B.Size.Z > 0.4 * 171.0; }));
    TestTrue(TEXT("Debashis: a sleeve garter on each arm"), NlHasBlock(Debashis, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(58, 60, 70) && B.Group == EPWNpcPartGroup::ArmLeft; })
        && NlHasBlock(Debashis, [](const FPWNpcBlock& B) { return B.Tint == FColor(58, 60, 70) && B.Group == EPWNpcPartGroup::ArmRight; }));
    TestTrue(TEXT("Debashis: a brass loupe on the forehead, under the head top"), NlHasBlock(Debashis, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(190, 150, 60) && B.Centre.Z > 171.0 - 10.0 && B.Centre.Z + B.Size.Z * 0.5 <= 171.0 + 0.05; }));
    const TArray<FPWNpcBlock> Mahua = Blocks(TEXT("person.mahua_saha"));
    TestTrue(TEXT("Mahua: a moss cardigan, open at the front"), NlHasBlock(Mahua, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(98, 126, 104) && B.Group == EPWNpcPartGroup::Body && B.Size.Z > 0.35 * 162.0; })
        && !NlHasBlock(Mahua, [](const FPWNpcBlock& B) { return B.Tint == FColor(98, 126, 104) && B.Centre.X > 11.0 && FMath::Abs(B.Centre.Y) < 5.0; }));
    TestTrue(TEXT("Mahua: a yellow tape measure at the chest"), NlHasBlock(Mahua, [](const FPWNpcBlock& B) { return B.Tint == FColor(222, 186, 56) && B.Centre.X > 14.0; }));
    TestTrue(TEXT("Mahua: a maroon ledger held at the hip"), NlHasBlock(Mahua, [](const FPWNpcBlock& B) { return B.Tint == FColor(110, 34, 38) && B.Size.Z > 20.0 && B.Centre.X > 12.0; }));
    TestTrue(TEXT("Mahua: grey hair pinned in a coil at the back"), NlHasBlock(Mahua, [](const FPWNpcBlock& B) { return B.Tint == FColor(190, 188, 186) && B.Centre.X < -9.0 && B.Size.Y > 12.0; }));
    const TArray<FPWNpcBlock> Purnima = Blocks(TEXT("person.purnima_das"));
    TestTrue(TEXT("Purnima: a saffron head-wrap whose top is her height"), [&]
        {
            double WrapTop = 0.0;
            for (const FPWNpcBlock& B : Purnima) { if (B.Tint == FColor(226, 140, 40)) { WrapTop = FMath::Max(WrapTop, B.Centre.Z + B.Size.Z * 0.5); } }
            return FMath::IsNearlyEqual(WrapTop, 155.0, 0.05);
        }());
    TestTrue(TEXT("Purnima: an emerald shawl across the shoulders, wider than the torso"), NlHasBlock(Purnima, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(44, 112, 76) && B.Size.Y >= 50.0 && B.Centre.Z > 0.84 * 155.0; }));
    TestTrue(TEXT("Purnima: a money pouch at her belt"), NlHasBlock(Purnima, [](const FPWNpcBlock& B) { return B.Tint == FColor(88, 58, 40) && B.Centre.X > 12.0; }));
    const TArray<FPWNpcBlock> Arun = Blocks(TEXT("person.arun_paul"));
    TestTrue(TEXT("Arun: a pale blue office shirt with a white collar"), NlHasBlock(Arun, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(150, 178, 208) && B.Group == EPWNpcPartGroup::Body; })
        && NlHasBlock(Arun, [](const FPWNpcBlock& B) { return B.Tint == FColor(236, 238, 242); }));
    TestTrue(TEXT("Arun: a messenger bag at his right hip"), NlHasBlock(Arun, [](const FPWNpcBlock& B) { return B.Tint == FColor(120, 84, 56) && B.Centre.Y > 28.0 && B.Size.Z > 20.0; }));
    TestTrue(TEXT("Arun: a lanyard card on the chest"), NlHasBlock(Arun, [](const FPWNpcBlock& B) { return B.Tint == FColor(240, 240, 236) && B.Centre.X > 10.0 && B.Centre.Z < 0.84 * 176.0; }));
    const TArray<FPWNpcBlock> Rekha = Blocks(TEXT("person.rekha_bose"));
    TestTrue(TEXT("Rekha: a rose saree wrapped to the ankle with a gold border at the hem"), NlHasBlock(Rekha, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(232, 214, 150) && B.Centre.Z < 10.0 && B.Group == EPWNpcPartGroup::LegLeft; })
        && NlHasBlock(Rekha, [](const FPWNpcBlock& B) { return B.Tint == FColor(176, 52, 84) && B.Group == EPWNpcPartGroup::LegRight && B.Size.Z > 0.4 * 161.0; }));
    TestTrue(TEXT("Rekha: the pallu over her right shoulder, where Rupa's is over the left"), NlHasBlock(Rekha, [](const FPWNpcBlock& B)
        { return B.Centre.Y > 10.0 && B.Centre.Z > 0.84 * 161.0 && B.Tint == FColor(176, 52, 84); }));
    TestTrue(TEXT("Rekha: a long plait down her back"), NlHasBlock(Rekha, [](const FPWNpcBlock& B) { return B.Centre.X < -11.0 && B.Size.Z > 0.3 * 161.0 && B.Tint == FColor(26, 22, 26); }));
    TestTrue(TEXT("Rekha: a furled umbrella that swings with her left arm, its tip clear of the floor"), [&]
        {
            bool bSwings = false;
            double TipBottom = 1e9;
            for (const FPWNpcBlock& B : Rekha)
            {
                if (B.Tint == FColor(30, 30, 34) && B.Group == EPWNpcPartGroup::ArmLeft) { bSwings = true; }
                if (B.Tint == FColor(160, 160, 166)) { TipBottom = FMath::Min(TipBottom, B.Centre.Z - B.Size.Z * 0.5); }
            }
            return bSwings && TipBottom > 0.2 * 161.0 - 6.0;
        }());
    return true;
}

// Round 4: the hair, wrap and cap envelope. Nothing worn on the head may rise above the authored height, and the gaze sits where
// the face puts it (0.925 H), within a comfortable eyeline of the 145 to 195 cm supported player band.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPWNpcLooksRoundFourEnvelopeTest, "PerfectWorld.NpcLooks.RoundFourEnvelope",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPWNpcLooksRoundFourEnvelopeTest::RunTest(const FString& Parameters)
{
    const TCHAR* Ids[] = {TEXT("person.debashis_pal"), TEXT("person.mahua_saha"), TEXT("person.purnima_das"), TEXT("person.arun_paul"), TEXT("person.rekha_bose")};
    for (const TCHAR* Id : Ids)
    {
        const FName Name(Id);
        const double H = PWNpc::StandingHeightCm(Name, EPWNpcOutfit::ParentHousehold);
        TArray<FPWNpcBlock> Blocks;
        PWNpcLooks::BuildBlocks(Name, EPWNpcOutfit::ParentHousehold, H, Blocks);
        double Highest = 0.0;
        double EyeZ = 0.0;
        for (const FPWNpcBlock& B : Blocks)
        {
            Highest = FMath::Max(Highest, B.Centre.Z + B.Size.Z * 0.5);
            // The eyes are the dark 1.2 x 3.0 x 2.4 blocks PWNpcLooks::AppendFace puts on the +X side.
            if (B.Tint == FColor(30, 26, 26) && FMath::IsNearlyEqual(B.Size.Z, 2.4) && B.Centre.X > 10.0) { EyeZ = B.Centre.Z; }
        }
        TestTrue(*(FString(Id) + TEXT(": nothing, hair or wrap included, rises above the authored height")), Highest <= H + 0.05);
        TestNearlyEqual(*(FString(Id) + TEXT(": gaze height is 0.925 H")), EyeZ, 0.925 * H, 0.05);
        TestTrue(*(FString(Id) + TEXT(": the eyes of a 145 cm and a 195 cm player (0.939 of height) are each within 55 cm of theirs")),
                 FMath::Abs(EyeZ - 145.0 * 0.939) < 55.0 && FMath::Abs(EyeZ - 195.0 * 0.939) < 55.0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR52LooksFeaturesTest, "PerfectWorld.NpcLooks.ResidentsR5Features",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR52LooksFeaturesTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcLooksTest;
    const auto R52PrabirBlocks = NlBlocks({TEXT("person.prabir_chatterjee"), 169, EPWNpcOutfit::HouseholdAdult, true});
    const auto R52IshitaBlocks = NlBlocks({TEXT("person.ishita_banerjee"), 165, EPWNpcOutfit::ParentHousehold, true});
    TestTrue(TEXT("R52 Prabir: narrow slate short kurta"), NlHasBlock(R52PrabirBlocks, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(82, 105, 134) && B.Size.Y == 37 && B.Centre.Z < 100; }));
    TestTrue(TEXT("R52 Prabir: thinning silver side hair under height"), NlHasBlock(R52PrabirBlocks, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(202, 199, 194) && FMath::Abs(B.Centre.Y) == 10 && B.Centre.Z + B.Size.Z * 0.5 <= 169; }));
    TestTrue(TEXT("R52 Prabir: reading glasses"), NlHasBlock(R52PrabirBlocks, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(70, 60, 50) && B.Centre.X == 11 && B.Size.Z == 6; }));
    TestTrue(TEXT("R52 Ishita: teal kurta"), NlHasBlock(R52IshitaBlocks, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(46, 119, 116) && B.Size.Y == 42; }));
    TestTrue(TEXT("R52 Ishita: short curls under height"), NlHasBlock(R52IshitaBlocks, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(38, 30, 32) && B.Size.X == 8 && B.Centre.Z + B.Size.Z * 0.5 <= 165; }));
    TestTrue(TEXT("R52 Ishita: canvas shoulder bag"), NlHasBlock(R52IshitaBlocks, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(182, 166, 125) && B.Centre.Y == 29 && B.Size.Z == 24; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FR64LooksFeaturesTest, "PerfectWorld.NpcLooks.ResidentsR6Features",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FR64LooksFeaturesTest::RunTest(const FString& Parameters)
{
    using namespace PWNpcLooksTest;
    const auto Madhabi = NlBlocks({TEXT("person.madhabi_das"), 156, EPWNpcOutfit::HouseholdAdult, true});
    const auto Arup = NlBlocks({TEXT("person.arup_basu"), 179, EPWNpcOutfit::ParentHousehold, true});
    TestTrue(TEXT("R64 Madhabi: dusty-blue saree wrapped over the legs"), NlHasBlock(Madhabi, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(88, 102, 138) && B.Group == EPWNpcPartGroup::LegLeft && B.Size.Z > 0.4 * 156; }));
    TestTrue(TEXT("R64 Madhabi: grey knot at the back, under her height"), NlHasBlock(Madhabi, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(176, 174, 172) && B.Centre.X < -9 && B.Centre.Z + B.Size.Z * 0.5 <= 156; }));
    TestTrue(TEXT("R64 Madhabi: manila file folder at her left hip"), NlHasBlock(Madhabi, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(208, 176, 108) && B.Centre.Y < -28 && B.Size.Z == 24; }));
    TestTrue(TEXT("R64 Arup: orange dispatch vest with a reflective band"), NlHasBlock(Arup, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(226, 118, 38) && B.Group == EPWNpcPartGroup::Body && B.Size.Y == 44; })
        && NlHasBlock(Arup, [](const FPWNpcBlock& B) { return B.Tint == FColor(222, 226, 170); }));
    TestTrue(TEXT("R64 Arup: close buzz cut whose top is his height"), NlHasBlock(Arup, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(30, 26, 26) && B.Size.Z == 5 && FMath::IsNearlyEqual(B.Centre.Z + B.Size.Z * 0.5, 179.0, 0.05); }));
    TestTrue(TEXT("R64 Arup: handheld scanner at his right hip"), NlHasBlock(Arup, [](const FPWNpcBlock& B)
        { return B.Tint == FColor(36, 38, 44) && B.Centre.Y > 27 && B.Size.Z == 14; }));
    return true;
}

#endif
