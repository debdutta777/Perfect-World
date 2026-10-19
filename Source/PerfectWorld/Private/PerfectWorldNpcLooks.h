#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldNpcCharacter.h"

/**
 * One block of a placeholder person. Centre is X forward, Y right, Z above the floor; Size is the full extent; all in cm.
 * Pivot is the joint a limb swings about (X forward, Y right, Z above the floor); it matters only for limb groups.
 */
struct FPWNpcBlock
{
    FVector Centre = FVector::ZeroVector;
    FVector Size = FVector::ZeroVector;
    FColor Tint = FColor::White;
    EPWNpcPartGroup Group = EPWNpcPartGroup::Body;
    FVector Pivot = FVector::ZeroVector;
};

/**
 * Person looks (NR-04; LANE_LIFE_PLAN section B "Block identity"): what each placeholder body is made of, keyed by person id
 * with the outfit as the fallback. The height of every authored person lives here too, so there is one table. Nothing in the
 * rules reads a look; real art replaces the whole body at M9. The public height query is still PWNpc::StandingHeightCm in
 * PerfectWorldNpcCharacter.h; it reads this table and nothing else.
 */
namespace PWNpcLooks
{
/** The attributes that make a silhouette readable at lane distance. Two people must differ in at least two (LANE_LIFE_PLAN section B). */
enum class EHair : uint8
{
    SilverStreaked, FullDark, TiedBackBun, Bob, ShortWavy, Thinning, LowBun, FlatCap, Ponytail,
    /** Round 4 (R4-06), appended: close-cropped grey, pinned grey coil, saffron head-wrap, neat side parting, long plait. */
    CloseCrop, PinnedCoil, HeadWrap, SidePart, LongPlait
};
enum class EGarment : uint8
{
    Overshirt, BeltedShirt, WaistApron, Pinafore, BibApron, Workwear, Saree, Waistcoat, Kurta,
    /** Round 4 (R4-06), appended: leather work apron over rolled sleeves, open cardigan, heavy shawl, office shirt. */
    LeatherApron, Cardigan, ShawlWrap, OfficeShirt
};
enum class EAccessory : uint8
{
    None, KeyPouch, Satchel, Clipboard, ClothBag, Glasses,
    /** Round 4 (R4-06), appended: head loupe and sleeve garters, tape measure and ledger, waist money pouch, messenger bag, furled umbrella. */
    LoupeGarters, TapeAndLedger, MoneyPouch, MessengerBag, Umbrella
};
enum class EBuild : uint8
{
    Slight, Medium, Sturdy, Broad
};

struct FSilhouette
{
    EHair Hair = EHair::FullDark;
    EGarment Garment = EGarment::BeltedShirt;
    EAccessory Accessory = EAccessory::None;
    EBuild Build = EBuild::Medium;
};

/** How many of the four silhouette attributes differ. */
int32 SilhouetteDifferences(const FSilhouette& A, const FSilhouette& B);

/** Which body builder a person uses. Outfit means "build from the outfit enum", which is every person without a bespoke look. */
enum class ELook : uint8
{
    Outfit, Mira, Anindita, Subrata, Rupa, Nirmal, Sahana,
    /** Round 4 (R4-06): the two proprietors, the vendor and the two passers-by. */
    Debashis, Mahua, Purnima, Arun, Rekha
};

/** One row of the cast table: the stable person id, the standing height (LANE_LIFE_PLAN section B) and the look. */
struct FCastRow
{
    const TCHAR* PersonId;
    double HeightCm;
    ELook Look;
};

TConstArrayView<FCastRow> CastTable();
const FCastRow* FindCast(FName PersonId);

/** The outfit's default height, used only for a person the cast table does not know. */
double OutfitDefaultHeightCm(EPWNpcOutfit Outfit);

/** The look a person wears: their own when the table gives one, else the outfit's. */
ELook LookOf(FName PersonId);
FSilhouette SilhouetteOf(FName PersonId, EPWNpcOutfit Outfit);

/** The blocks for a person standing at height H (their feet at Z 0, centred on X 0, Y 0). Appends to Out. */
void BuildBlocks(FName PersonId, EPWNpcOutfit Outfit, double H, TArray<FPWNpcBlock>& Out);

/** The blocks of a bespoke look (PerfectWorldNpcLooksCast.cpp). False for ELook::Outfit. */
bool AppendCastLook(ELook Look, double H, TArray<FPWNpcBlock>& Out);

/** The face every body shares: eyes, brows and a nose on the +X side. HeadForwardCm shifts it with a stooped head. */
void AppendFace(double H, double HeadForwardCm, TArray<FPWNpcBlock>& Out);

/** Appends one block. */
void PutBlock(TArray<FPWNpcBlock>& Out, const FVector& Centre, const FVector& Size, const FColor& Tint,
              EPWNpcPartGroup Group = EPWNpcPartGroup::Body, const FVector& Pivot = FVector::ZeroVector);

EPWNpcPartGroup ArmGroup(double Side);
EPWNpcPartGroup LegGroup(double Side);

/**
 * Where a block sits and how it is turned for a pose, relative to the capsule's feet (Z above the floor). This is the math
 * APWNpcCharacter::ApplyPose applies to each cube, lifted out so tests can check it without an actor.
 */
void PoseBlock(const FPWNpcPartRest& Rest, const FPWNpcPose& Pose, double H, FVector& OutLocation, FQuat& OutRotation);
} // namespace PWNpcLooks
