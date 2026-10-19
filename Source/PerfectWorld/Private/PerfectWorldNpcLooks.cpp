#include "PerfectWorldNpcLooks.h"

namespace PWNpcLooks
{
namespace
{
/** LANE_LIFE_PLAN section B, "Block identity" column. Ids follow the stable "person.<first>_<surname>" convention. */
const FCastRow NlCast[] = {
    {TEXT("person.farida_qadri"), 158.0, ELook::Outfit},      // sturdy frame, tied-back hair (ShopProprietor)
    {TEXT("person.mira_sen"), 160.0, ELook::Mira},            // straight bob, square satchel, navy uniform
    {TEXT("person.ishan_sheikh"), 173.0, ELook::Outfit},
    {TEXT("person.tara_das"), 168.0, ELook::Outfit},
    {TEXT("person.amrita_basu"), 163.0, ELook::Outfit},
    {TEXT("person.ananya_chatterjee"), 166.0, ELook::Outfit},
    {TEXT("person.amit_roy"), 178.0, ELook::Outfit},
    {TEXT("person.care_worker"), 162.0, ELook::Outfit},       // the staff-caregiver prototype (HouseholdAdult)
    {TEXT("person.household_adult"), 168.0, ELook::Outfit},   // the parent household of the default origin (ParentHousehold)
    {TEXT("person.anindita_sen"), 154.0, ELook::Anindita},    // short wavy hair, plum blouse, paper-coloured apron
    {TEXT("person.debashis_pal"), 171.0, ELook::Outfit},
    {TEXT("person.leela_dutta"), 176.0, ELook::Outfit},
    {TEXT("person.subrata_dey"), 181.0, ELook::Subrata},      // narrow build, thinning grey hair, sage workwear, clipboard
    {TEXT("person.rupa_ghosh"), 157.0, ELook::Rupa},          // NR-01: row-house resident; saree with a mustard border, low bun
    {TEXT("person.nirmal_bose"), 174.0, ELook::Nirmal},       // NR-02: tall-flats unit A; broad build, flat cap, waistcoat
    {TEXT("person.sahana_mitra"), 164.0, ELook::Sahana},      // NR-03: tall-flats unit B; ponytail, round glasses, indigo kurta
};

void NlLegacyHouseholdAdult(const double H, TArray<FPWNpcBlock>& Out)
{
    const double LegHeight = 0.48 * H, TorsoHeight = 0.36 * H, TorsoCentre = 0.66 * H;
    const double SleeveHeight = 0.20 * H, ForearmHeight = 0.14 * H, HeadHeight = 0.152 * H;
    const FColor Skin(176, 128, 92), Trousers(70, 74, 84), Cream(214, 206, 186), Sage(128, 150, 118);
    const FColor Silver(192, 192, 198), Leather(112, 80, 54);
    PutBlock(Out, FVector(0.0, -9.0, LegHeight * 0.5), FVector(14.0, 14.0, LegHeight), Trousers, EPWNpcPartGroup::LegLeft, FVector(0.0, -9.0, LegHeight));
    PutBlock(Out, FVector(0.0, 9.0, LegHeight * 0.5), FVector(14.0, 14.0, LegHeight), Trousers, EPWNpcPartGroup::LegRight, FVector(0.0, 9.0, LegHeight));
    PutBlock(Out, FVector(0.0, 0.0, TorsoCentre), FVector(20.0, 40.0, TorsoHeight), Cream);
    // The sage overshirt, hip length, with the cream shirt showing down the open front.
    PutBlock(Out, FVector(0.0, 0.0, 0.62 * H), FVector(24.0, 44.0, 0.44 * H), Sage);
    PutBlock(Out, FVector(12.5, 0.0, TorsoCentre), FVector(2.0, 9.0, TorsoHeight), Cream);
    for (const double Side : {-1.0, 1.0})
    {
        const FVector Shoulder(0.0, Side * 26.0, 0.84 * H);
        PutBlock(Out, FVector(0.0, Side * 26.0, 0.84 * H - SleeveHeight * 0.5), FVector(12.0, 12.0, SleeveHeight), Sage, ArmGroup(Side), Shoulder);
        PutBlock(Out, FVector(0.0, Side * 26.0, 0.84 * H - SleeveHeight - ForearmHeight * 0.5), FVector(9.0, 9.0, ForearmHeight), Skin, ArmGroup(Side), Shoulder);
    }
    PutBlock(Out, FVector(0.0, 0.0, H - HeadHeight * 0.5), FVector(20.0, 20.0, HeadHeight), Skin);
    PutBlock(Out, FVector(0.0, 0.0, H - 3.5), FVector(22.0, 22.0, 7.0), Silver);      // silver-streaked hair
    PutBlock(Out, FVector(3.0, 25.0, 0.47 * H), FVector(8.0, 11.0, 12.0), Leather);    // the key pouch
}

void NlLegacyShopProprietor(const double H, TArray<FPWNpcBlock>& Out)
{
    const double LegHeight = 0.48 * H, TorsoHeight = 0.36 * H, TorsoCentre = 0.66 * H;
    const double SleeveHeight = 0.20 * H, ForearmHeight = 0.14 * H, HeadHeight = 0.152 * H;
    const FColor Skin(174, 119, 84), Trousers(64, 59, 56), Cream(230, 220, 196), Apron(176, 86, 59), Hair(42, 32, 29);
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(0.0, Side * 10.0, LegHeight * 0.5), FVector(17.0, 17.0, LegHeight), Trousers, LegGroup(Side), FVector(0.0, Side * 10.0, LegHeight));
        // The arm's outer edge stays inside the 34 cm capsule (NR-04: it was 36).
        const FVector Shoulder(0.0, Side * 28.0, 0.84 * H);
        PutBlock(Out, FVector(0.0, Side * 28.0, 0.84 * H - SleeveHeight * 0.5), FVector(12.0, 12.0, SleeveHeight), Cream, ArmGroup(Side), Shoulder);
        PutBlock(Out, FVector(0.0, Side * 28.0, 0.84 * H - SleeveHeight - ForearmHeight * 0.5), FVector(10.0, 10.0, ForearmHeight), Skin, ArmGroup(Side), Shoulder);
    }
    PutBlock(Out, FVector(0.0, 0.0, TorsoCentre), FVector(26.0, 46.0, TorsoHeight), Cream);
    PutBlock(Out, FVector(14.0, 0.0, 0.62 * H), FVector(3.0, 35.0, 0.42 * H), Apron);
    PutBlock(Out, FVector(0.0, 0.0, 0.52 * H), FVector(28.0, 48.0, 4.0), Apron);
    PutBlock(Out, FVector(0.0, 0.0, H - HeadHeight * 0.5), FVector(20.0, 22.0, HeadHeight), Skin);
    PutBlock(Out, FVector(0.0, 0.0, H - 3.5), FVector(22.0, 24.0, 7.0), Hair);
    PutBlock(Out, FVector(-11.0, 0.0, H - 9.0), FVector(9.0, 14.0, 12.0), Hair); // tied-back bun
}

void NlLegacyParentHousehold(const double H, TArray<FPWNpcBlock>& Out)
{
    const double LegHeight = 0.48 * H, TorsoHeight = 0.36 * H, TorsoCentre = 0.66 * H;
    const double SleeveHeight = 0.20 * H, ForearmHeight = 0.14 * H, HeadHeight = 0.152 * H;
    const FColor Skin(206, 160, 122), Trousers(58, 60, 66), Shirt(216, 210, 194), Belt(34, 32, 34), Hair(44, 36, 34);
    PutBlock(Out, FVector(0.0, -9.5, LegHeight * 0.5), FVector(15.0, 15.0, LegHeight), Trousers, EPWNpcPartGroup::LegLeft, FVector(0.0, -9.5, LegHeight));
    PutBlock(Out, FVector(0.0, 9.5, LegHeight * 0.5), FVector(15.0, 15.0, LegHeight), Trousers, EPWNpcPartGroup::LegRight, FVector(0.0, 9.5, LegHeight));
    PutBlock(Out, FVector(0.0, 0.0, TorsoCentre), FVector(20.0, 42.0, TorsoHeight), Shirt);
    PutBlock(Out, FVector(0.0, 0.0, LegHeight + 1.5), FVector(22.0, 43.0, 3.0), Belt);
    for (const double Side : {-1.0, 1.0})
    {
        const FVector Shoulder(0.0, Side * 27.0, 0.84 * H);
        PutBlock(Out, FVector(0.0, Side * 27.0, 0.84 * H - SleeveHeight * 0.5), FVector(12.0, 12.0, SleeveHeight), Shirt, ArmGroup(Side), Shoulder);
        PutBlock(Out, FVector(0.0, Side * 27.0, 0.84 * H - SleeveHeight - ForearmHeight * 0.5), FVector(9.0, 9.0, ForearmHeight), Skin, ArmGroup(Side), Shoulder);
    }
    PutBlock(Out, FVector(0.0, 0.0, H - HeadHeight * 0.5), FVector(20.0, 20.0, HeadHeight), Skin);
    PutBlock(Out, FVector(0.0, 0.0, H - 4.5), FVector(22.0, 22.0, 9.0), Hair);          // fuller, dark hair
    PutBlock(Out, FVector(-8.0, 0.0, H - 0.1 * H), FVector(6.0, 21.0, 0.1 * H), Hair);
}

FSilhouette NlSilhouetteOfLook(const ELook Look, const EPWNpcOutfit Outfit)
{
    switch (Look)
    {
    case ELook::Mira: return {EHair::Bob, EGarment::Pinafore, EAccessory::Satchel, EBuild::Slight};
    case ELook::Anindita: return {EHair::ShortWavy, EGarment::BibApron, EAccessory::None, EBuild::Medium};
    case ELook::Subrata: return {EHair::Thinning, EGarment::Workwear, EAccessory::Clipboard, EBuild::Slight};
    case ELook::Rupa: return {EHair::LowBun, EGarment::Saree, EAccessory::ClothBag, EBuild::Medium};
    case ELook::Nirmal: return {EHair::FlatCap, EGarment::Waistcoat, EAccessory::None, EBuild::Broad};
    case ELook::Sahana: return {EHair::Ponytail, EGarment::Kurta, EAccessory::Glasses, EBuild::Medium};
    default: break;
    }
    switch (Outfit)
    {
    case EPWNpcOutfit::HouseholdAdult: return {EHair::SilverStreaked, EGarment::Overshirt, EAccessory::KeyPouch, EBuild::Medium};
    case EPWNpcOutfit::ShopProprietor: return {EHair::TiedBackBun, EGarment::WaistApron, EAccessory::None, EBuild::Sturdy};
    default: return {EHair::FullDark, EGarment::BeltedShirt, EAccessory::None, EBuild::Medium};
    }
}
} // namespace

int32 SilhouetteDifferences(const FSilhouette& A, const FSilhouette& B)
{
    return (A.Hair != B.Hair ? 1 : 0) + (A.Garment != B.Garment ? 1 : 0) + (A.Accessory != B.Accessory ? 1 : 0) + (A.Build != B.Build ? 1 : 0);
}

TConstArrayView<FCastRow> CastTable()
{
    return MakeArrayView(NlCast);
}

const FCastRow* FindCast(const FName PersonId)
{
    if (PersonId.IsNone()) { return nullptr; }
    const FString Id = PersonId.ToString();
    for (const FCastRow& Row : NlCast)
    {
        if (Id.Equals(Row.PersonId, ESearchCase::CaseSensitive)) { return &Row; }
    }
    return nullptr;
}

double OutfitDefaultHeightCm(const EPWNpcOutfit Outfit)
{
    if (Outfit == EPWNpcOutfit::ShopProprietor) { return 158.0; }
    return Outfit == EPWNpcOutfit::HouseholdAdult ? 162.0 : 168.0;
}

ELook LookOf(const FName PersonId)
{
    const FCastRow* Row = FindCast(PersonId);
    return Row != nullptr ? Row->Look : ELook::Outfit;
}

FSilhouette SilhouetteOf(const FName PersonId, const EPWNpcOutfit Outfit)
{
    return NlSilhouetteOfLook(LookOf(PersonId), Outfit);
}

void PutBlock(TArray<FPWNpcBlock>& Out, const FVector& Centre, const FVector& Size, const FColor& Tint, const EPWNpcPartGroup Group, const FVector& Pivot)
{
    FPWNpcBlock Block;
    Block.Centre = Centre;
    Block.Size = Size;
    Block.Tint = Tint;
    Block.Group = Group;
    Block.Pivot = Pivot;
    Out.Add(Block);
}

EPWNpcPartGroup ArmGroup(const double Side) { return Side < 0.0 ? EPWNpcPartGroup::ArmLeft : EPWNpcPartGroup::ArmRight; }
EPWNpcPartGroup LegGroup(const double Side) { return Side < 0.0 ? EPWNpcPartGroup::LegLeft : EPWNpcPartGroup::LegRight; }

void AppendFace(const double H, const double HeadForwardCm, TArray<FPWNpcBlock>& Out)
{
    // A minimal face on the +X side (the actor's forward), so a player can read which way a person faces: the first
    // packaged walk-in (2026-10-14) could not tell a symmetric block head's front from its back.
    const FColor Eye(30, 26, 26), Brow(52, 40, 36), Nose(150, 104, 78);
    const double EyeZ = H - 0.075 * H;
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(10.6 + HeadForwardCm, Side * 4.5, EyeZ), FVector(1.2, 3.0, 2.4), Eye);
        PutBlock(Out, FVector(10.6 + HeadForwardCm, Side * 4.5, EyeZ + 3.2), FVector(1.2, 4.2, 1.0), Brow);
    }
    PutBlock(Out, FVector(11.2 + HeadForwardCm, 0.0, EyeZ - 4.0), FVector(2.4, 2.4, 3.6), Nose);
}

void BuildBlocks(const FName PersonId, const EPWNpcOutfit Outfit, const double H, TArray<FPWNpcBlock>& Out)
{
    const ELook Look = LookOf(PersonId);
    if (AppendCastLook(Look, H, Out)) { return; } // the bespoke looks add their own face
    if (Outfit == EPWNpcOutfit::HouseholdAdult) { NlLegacyHouseholdAdult(H, Out); }
    else if (Outfit == EPWNpcOutfit::ShopProprietor) { NlLegacyShopProprietor(H, Out); }
    else { NlLegacyParentHousehold(H, Out); }
    AppendFace(H, 0.0, Out);
}

void PoseBlock(const FPWNpcPartRest& Rest, const FPWNpcPose& Pose, const double H, FVector& OutLocation, FQuat& OutRotation)
{
    const double Drop = 0.2 * H * Pose.Sit; // hips sink onto a seat about 45 cm up
    const FVector Hip(0.0, 0.0, 0.48 * H - Drop);
    const FQuat Lean(FVector::RightVector, FMath::DegreesToRadians(static_cast<double>(Pose.LeanPitch)));
    const FQuat Roll(FVector::ForwardVector, FMath::DegreesToRadians(static_cast<double>(Pose.SwayRoll)));
    const FQuat Lie(FVector::RightVector, FMath::DegreesToRadians(-90.0 * Pose.Lie)); // onto the back, head towards -X
    FVector P = Rest.Rest;
    FQuat Q = FQuat::Identity;
    double Swing = 0.0; // degrees, positive swings a hanging limb forward (+X)
    switch (Rest.Group)
    {
    case EPWNpcPartGroup::LegLeft: Swing = Pose.LegPitch[0] + 90.0 * Pose.Sit; break;
    case EPWNpcPartGroup::LegRight: Swing = Pose.LegPitch[1] + 90.0 * Pose.Sit; break;
    case EPWNpcPartGroup::ArmLeft: Swing = Pose.ArmPitch[0]; break;
    case EPWNpcPartGroup::ArmRight: Swing = Pose.ArmPitch[1]; break;
    default: break;
    }
    if (Rest.Group != EPWNpcPartGroup::Body)
    {
        const FQuat Limb(FVector::RightVector, FMath::DegreesToRadians(-Swing));
        P = Rest.Pivot + Limb.RotateVector(P - Rest.Pivot);
        Q = Limb;
    }
    P.Z -= Drop;
    if (Rest.Group != EPWNpcPartGroup::LegLeft && Rest.Group != EPWNpcPartGroup::LegRight)
    {
        P = Hip + Lean.RotateVector(P - Hip); // everything above the hips leans together
        Q = Lean * Q;
    }
    P = Roll.RotateVector(P);
    Q = Roll * Q;
    if (Pose.Lie > 0.0f)
    {
        P = Lie.RotateVector(P) + FVector(0.5 * H * Pose.Lie, 0.0, 14.0 * Pose.Lie);
        Q = Lie * Q;
    }
    P.Z += Pose.BobCm;
    OutLocation = P;
    OutRotation = Q;
}
} // namespace PWNpcLooks

namespace PWNpc
{
double StandingHeightCm(const FName PersonId, const EPWNpcOutfit Outfit)
{
    const PWNpcLooks::FCastRow* Row = PWNpcLooks::FindCast(PersonId);
    return Row != nullptr ? Row->HeightCm : PWNpcLooks::OutfitDefaultHeightCm(Outfit);
}

bool HasAuthoredHeight(const FName PersonId)
{
    return PWNpcLooks::FindCast(PersonId) != nullptr;
}

double CapsuleHalfHeightCm(const FName PersonId, const EPWNpcOutfit Outfit)
{
    return StandingHeightCm(PersonId, Outfit) * 0.5;
}
} // namespace PWNpc
