#include "PerfectWorldNpcLooks.h"

// The bespoke looks (NR-04). Each is proportioned from the standing height H and reads at lane distance through two or more
// silhouette attributes (hair, garment, accessory, build); the cast rows are in PerfectWorldNpcLooks.cpp and the design source
// is LANE_LIFE_PLAN section B. Every block stays inside the 34 cm capsule and under the person's own height.

namespace PWNpcLooks
{
namespace
{
/** The shared skeleton of a bespoke look: two legs, a torso, two arms (sleeve, forearm, optional hand) and a head, all skin or cloth. */
struct FNlFigure
{
    FColor Skin = FColor(190, 140, 104);
    FColor Legs = FColor(60, 60, 66);
    FColor Top = FColor(216, 210, 194);
    FColor Sleeves = FColor(216, 210, 194);
    FColor Forearm = FColor(190, 140, 104);
    double LegW = 15.0;
    double LegOff = 9.5;
    double TorsoX = 20.0;
    double TorsoY = 42.0;
    double ArmOff = 27.0;
    double ArmT = 12.0;
    double HeadForward = 0.0;
    bool bHands = false; // the lower 0.04 H of the forearm is a skin hand and the forearm above it takes the Forearm colour
};

void NlAddFigure(const FNlFigure& F, const double H, TArray<FPWNpcBlock>& Out)
{
    const double LegHeight = 0.48 * H, TorsoHeight = 0.36 * H, SleeveHeight = 0.20 * H, ForearmHeight = 0.14 * H, HeadHeight = 0.152 * H;
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(0.0, Side * F.LegOff, LegHeight * 0.5), FVector(F.LegW, F.LegW, LegHeight), F.Legs, LegGroup(Side),
                 FVector(0.0, Side * F.LegOff, LegHeight));
    }
    PutBlock(Out, FVector(0.0, 0.0, 0.66 * H), FVector(F.TorsoX, F.TorsoY, TorsoHeight), F.Top);
    for (const double Side : {-1.0, 1.0})
    {
        const FVector Shoulder(0.0, Side * F.ArmOff, 0.84 * H);
        PutBlock(Out, FVector(0.0, Side * F.ArmOff, 0.84 * H - SleeveHeight * 0.5), FVector(F.ArmT, F.ArmT, SleeveHeight), F.Sleeves, ArmGroup(Side), Shoulder);
        const double Hand = F.bHands ? 0.04 * H : 0.0;
        const double Forearm = ForearmHeight - Hand;
        PutBlock(Out, FVector(0.0, Side * F.ArmOff, 0.84 * H - SleeveHeight - Forearm * 0.5), FVector(F.ArmT - 3.0, F.ArmT - 3.0, Forearm),
                 F.Forearm, ArmGroup(Side), Shoulder);
        if (F.bHands)
        {
            PutBlock(Out, FVector(0.0, Side * F.ArmOff, 0.84 * H - SleeveHeight - Forearm - Hand * 0.5), FVector(F.ArmT - 3.0, F.ArmT - 3.0, Hand),
                     F.Skin, ArmGroup(Side), Shoulder);
        }
    }
    PutBlock(Out, FVector(F.HeadForward, 0.0, H - HeadHeight * 0.5), FVector(20.0, 20.0, HeadHeight), F.Skin);
}

void NlMira(const double H, TArray<FPWNpcBlock>& Out)
{
    // 160 cm; straight bob, square satchel, navy uniform. A slight frame.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(196, 148, 110);
    F.Legs = FColor(30, 42, 86);
    F.Top = F.Sleeves = FColor(232, 232, 236);
    F.LegW = 13.0; F.LegOff = 8.5; F.TorsoX = 18.0; F.TorsoY = 38.0; F.ArmOff = 25.0; F.ArmT = 11.0;
    NlAddFigure(F, H, Out);
    const FColor Navy(30, 42, 86), Tie(120, 30, 36), Hair(28, 24, 28), Leather(150, 110, 70);
    PutBlock(Out, FVector(0.0, 0.0, 0.60 * H), FVector(21.0, 36.0, 0.34 * H), Navy);       // the pinafore
    PutBlock(Out, FVector(0.0, 0.0, 0.44 * H), FVector(22.0, 40.0, 0.12 * H), Navy);       // its skirt, over the tops of the legs
    PutBlock(Out, FVector(9.6, 0.0, 0.70 * H), FVector(1.5, 4.0, 0.16 * H), Tie);
    PutBlock(Out, FVector(0.0, 0.0, H - 3.0), FVector(22.0, 22.0, 6.0), Hair);
    PutBlock(Out, FVector(-2.0, 0.0, H - 12.0), FVector(24.0, 25.0, 20.0), Hair);          // the straight bob, to the jaw
    PutBlock(Out, FVector(10.8, 0.0, H - 6.0), FVector(2.5, 21.0, 3.0), Hair);             // the fringe
    PutBlock(Out, FVector(-2.0, -29.5, 0.38 * H), FVector(10.0, 7.0, 22.0), Leather);      // the square satchel at her hip
    PutBlock(Out, FVector(9.8, -13.0, 0.64 * H), FVector(1.6, 4.0, 0.34 * H), Leather);     // its strap, front and back
    PutBlock(Out, FVector(-9.8, -13.0, 0.64 * H), FVector(1.6, 4.0, 0.34 * H), Leather);
    AppendFace(H, 0.0, Out);
}

void NlAnindita(const double H, TArray<FPWNpcBlock>& Out)
{
    // 154 cm; short wavy hair, plum blouse, paper-coloured apron.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(180, 128, 92);
    F.Legs = FColor(62, 58, 70);
    F.Top = F.Sleeves = FColor(112, 46, 92);
    F.LegW = 14.0; F.LegOff = 9.0; F.TorsoX = 22.0; F.TorsoY = 40.0; F.ArmOff = 26.0; F.ArmT = 12.0;
    NlAddFigure(F, H, Out);
    const FColor Paper(240, 232, 208), PaperDark(214, 204, 178), Hair(58, 38, 30);
    PutBlock(Out, FVector(12.5, 0.0, 0.61 * H), FVector(3.0, 34.0, 0.44 * H), Paper);       // the bib apron
    PutBlock(Out, FVector(0.0, 0.0, 0.845 * H), FVector(24.0, 4.0, 2.0), Paper);            // its neck strap, over each shoulder
    PutBlock(Out, FVector(-11.5, 0.0, 0.53 * H), FVector(3.0, 30.0, 3.0), Paper);           // the waist tie at the back
    PutBlock(Out, FVector(14.5, 0.0, 0.50 * H), FVector(2.0, 12.0, 9.0), PaperDark);        // the apron pocket
    PutBlock(Out, FVector(0.0, 0.0, H - 3.0), FVector(22.0, 22.0, 6.0), Hair);
    PutBlock(Out, FVector(0.0, -11.5, H - 9.0), FVector(21.0, 5.0, 9.0), Hair);             // short waves, a little different on each side
    PutBlock(Out, FVector(0.0, 11.5, H - 12.0), FVector(21.0, 5.0, 10.0), Hair);
    PutBlock(Out, FVector(-10.5, 0.0, H - 10.0), FVector(4.5, 21.0, 12.0), Hair);
    PutBlock(Out, FVector(-1.0, -13.2, H - 15.0), FVector(16.0, 3.0, 6.0), Hair);
    PutBlock(Out, FVector(-1.0, 13.2, H - 6.0), FVector(16.0, 3.0, 6.0), Hair);
    PutBlock(Out, FVector(10.5, 3.0, H - 6.0), FVector(2.5, 12.0, 4.5), Hair);
    AppendFace(H, 0.0, Out);
}

void NlSubrata(const double H, TArray<FPWNpcBlock>& Out)
{
    // 181 cm; narrow build, thinning grey hair, sage workwear, folding clipboard, head carried a little forward.
    FNlFigure F;
    F.Skin = FColor(188, 140, 104);
    F.Legs = FColor(100, 124, 94);
    F.Top = F.Sleeves = F.Forearm = FColor(112, 136, 102);
    F.LegW = 11.5; F.LegOff = 7.5; F.TorsoX = 17.0; F.TorsoY = 34.0; F.ArmOff = 22.0; F.ArmT = 10.0; F.HeadForward = 3.0; F.bHands = true;
    NlAddFigure(F, H, Out);
    const FColor Dark(84, 106, 78), Belt(60, 64, 52), Grey(176, 176, 172), Wood(166, 126, 82), Paper(236, 234, 224), Clip(150, 150, 156);
    PutBlock(Out, FVector(0.0, 0.0, 0.485 * H), FVector(18.5, 35.0, 3.0), Belt);
    PutBlock(Out, FVector(0.0, 0.0, 0.835 * H), FVector(15.0, 20.0, 3.0), Dark);             // the collar
    PutBlock(Out, FVector(9.2, -7.0, 0.74 * H), FVector(1.5, 9.0, 9.0), Dark);               // the breast pocket
    PutBlock(Out, FVector(F.HeadForward - 7.0, 0.0, H - 11.0), FVector(3.0, 19.0, 12.0), Grey); // thinning grey hair: back and sides only
    PutBlock(Out, FVector(F.HeadForward, -10.5, H - 9.0), FVector(14.0, 3.0, 10.0), Grey);
    PutBlock(Out, FVector(F.HeadForward, 10.5, H - 9.0), FVector(14.0, 3.0, 10.0), Grey);
    PutBlock(Out, FVector(10.5, 4.0, 0.60 * H), FVector(2.2, 22.0, 28.0), Wood);             // the clipboard, held at the chest
    PutBlock(Out, FVector(12.0, 4.0, 0.60 * H), FVector(0.8, 18.0, 24.0), Paper);
    PutBlock(Out, FVector(12.6, 4.0, 0.60 * H + 13.0), FVector(1.0, 8.0, 3.0), Clip);
    AppendFace(H, F.HeadForward, Out);
}

void NlRupa(const double H, TArray<FPWNpcBlock>& Out)
{
    // 157 cm; saree in teal with a mustard border and a pallu over the left shoulder, maroon blouse, low bun, cloth bag.
    // The saree wraps each leg (the leg blocks are teal and carry the hem border), so a seated pose swings the fabric with the legs
    // instead of sinking a long skirt through the seat; the fixed drape covers only the hips.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(168, 118, 86);
    F.Legs = FColor(36, 98, 110);
    F.Top = F.Sleeves = FColor(124, 38, 52);
    F.LegW = 15.0; F.LegOff = 8.0; F.TorsoY = 40.0; F.ArmOff = 25.0; F.ArmT = 11.0;
    NlAddFigure(F, H, Out);
    const FColor Teal(36, 98, 110), Pleat(60, 124, 136), Mustard(212, 168, 52), Hair(30, 26, 28), Flower(240, 238, 226), Bag(206, 190, 150);
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(0.0, Side * F.LegOff, 6.0), FVector(F.LegW + 1.0, F.LegW + 1.0, 3.0), Mustard, LegGroup(Side),
                 FVector(0.0, Side * F.LegOff, 0.48 * H)); // the border at the ankle
    }
    PutBlock(Out, FVector(0.0, 0.0, 0.40 * H), FVector(26.0, 42.0, 0.24 * H), Teal);        // the drape over the hips and thighs
    PutBlock(Out, FVector(13.5, 2.0, 0.40 * H), FVector(2.0, 14.0, 0.22 * H), Pleat);        // the pleats at the front
    PutBlock(Out, FVector(11.5, -12.0, 0.65 * H), FVector(3.0, 14.0, 0.38 * H), Teal);       // the pallu, front
    PutBlock(Out, FVector(11.6, -17.5, 0.65 * H), FVector(3.2, 3.0, 0.38 * H), Mustard);
    PutBlock(Out, FVector(-11.5, -8.0, 0.58 * H), FVector(3.0, 22.0, 0.52 * H), Teal);       // and back
    PutBlock(Out, FVector(0.0, -14.0, 0.855 * H), FVector(22.0, 14.0, 3.0), Teal);           // and over the shoulder
    PutBlock(Out, FVector(0.0, 0.0, H - 3.0), FVector(22.0, 22.0, 6.0), Hair);
    PutBlock(Out, FVector(-9.0, 0.0, H - 9.0), FVector(4.0, 20.0, 0.12 * H), Hair);
    PutBlock(Out, FVector(-12.0, 0.0, H - 0.115 * H), FVector(7.0, 12.0, 10.0), Hair);       // the low bun, at the nape
    PutBlock(Out, FVector(-15.8, 5.0, H - 0.105 * H), FVector(2.0, 4.0, 4.0), Flower);
    PutBlock(Out, FVector(0.0, 30.0, 0.37 * H), FVector(7.0, 5.0, 0.17 * H), Bag);           // the cloth bag at her right hip
    AppendFace(H, 0.0, Out);
}

void NlNirmal(const double H, TArray<FPWNpcBlock>& Out)
{
    // 174 cm; broad build, flat cap, moustache, brown waistcoat over a cream shirt with the sleeves rolled.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(150, 106, 76);
    F.Legs = FColor(142, 124, 92);
    F.Top = F.Sleeves = FColor(228, 224, 206);
    F.LegW = 17.0; F.LegOff = 11.0; F.TorsoX = 24.0; F.TorsoY = 48.0; F.ArmOff = 28.5; F.ArmT = 11.0;
    NlAddFigure(F, H, Out);
    const FColor Vest(84, 62, 46), Belt(40, 32, 28), Cap(96, 96, 100), Side(150, 148, 146), Moustache(70, 64, 60);
    PutBlock(Out, FVector(0.0, 0.0, 0.485 * H), FVector(26.0, 49.0, 3.0), Belt);
    PutBlock(Out, FVector(-12.5, 0.0, 0.64 * H), FVector(3.0, 46.0, 0.34 * H), Vest);        // the waistcoat: back panel and two fronts
    PutBlock(Out, FVector(12.5, -12.0, 0.64 * H), FVector(3.0, 17.0, 0.34 * H), Vest);
    PutBlock(Out, FVector(12.5, 12.0, 0.64 * H), FVector(3.0, 17.0, 0.34 * H), Vest);
    PutBlock(Out, FVector(0.0, -14.0, 0.845 * H), FVector(20.0, 8.0, 2.5), Vest);            // its shoulder straps
    PutBlock(Out, FVector(0.0, 14.0, 0.845 * H), FVector(20.0, 8.0, 2.5), Vest);
    PutBlock(Out, FVector(0.0, 0.0, H - 2.75), FVector(24.0, 24.0, 5.5), Cap);               // the flat cap
    PutBlock(Out, FVector(14.0, 0.0, H - 6.2), FVector(7.0, 18.0, 1.5), Cap);                // and its brim
    PutBlock(Out, FVector(0.0, -11.2, H - 8.5), FVector(20.0, 2.5, 5.0), Side);              // grey hair at the sides
    PutBlock(Out, FVector(0.0, 11.2, H - 8.5), FVector(20.0, 2.5, 5.0), Side);
    AppendFace(H, 0.0, Out);
    PutBlock(Out, FVector(11.0, 0.0, H - 0.075 * H - 7.5), FVector(1.6, 9.0, 2.4), Moustache);
}

void NlSahana(const double H, TArray<FPWNpcBlock>& Out)
{
    // 164 cm; high ponytail, round glasses, indigo knee-length kurta over pale churidar.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(190, 142, 104);
    F.Legs = FColor(226, 218, 200);
    F.Top = F.Sleeves = FColor(58, 64, 126);
    F.LegW = 13.0; F.LegOff = 8.0; F.TorsoY = 38.0; F.ArmOff = 25.0; F.ArmT = 11.0;
    NlAddFigure(F, H, Out);
    const FColor Indigo(58, 64, 126), Hem(222, 170, 150), Yoke(230, 196, 120), Hair(46, 32, 30), Tie(200, 90, 70), Frame(30, 28, 34);
    PutBlock(Out, FVector(0.0, 0.0, 0.41 * H), FVector(24.0, 42.0, 0.22 * H), Indigo);       // the kurta skirt, to the knee
    PutBlock(Out, FVector(0.0, 0.0, 0.30 * H + 1.2), FVector(25.0, 43.0, 2.5), Hem);
    PutBlock(Out, FVector(10.8, 0.0, 0.76 * H), FVector(1.5, 20.0, 8.0), Yoke);              // the embroidered yoke
    PutBlock(Out, FVector(0.0, 0.0, H - 3.0), FVector(22.0, 22.0, 6.0), Hair);
    PutBlock(Out, FVector(-9.0, 0.0, H - 11.0), FVector(4.0, 20.0, 0.14 * H), Hair);
    PutBlock(Out, FVector(-11.0, 0.0, H - 6.0), FVector(5.0, 6.0, 5.0), Tie);                // the hair tie
    PutBlock(Out, FVector(-12.5, 0.0, H - 26.0), FVector(5.0, 7.0, 36.0), Hair);             // and the ponytail down her back
    AppendFace(H, 0.0, Out);
    const double EyeZ = H - 0.075 * H;
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(11.4, Side * 4.5, EyeZ + 3.2), FVector(1.0, 7.0, 1.0), Frame); // round glasses, drawn as four bars a lens
        PutBlock(Out, FVector(11.4, Side * 4.5, EyeZ - 3.0), FVector(1.0, 7.0, 1.0), Frame);
        PutBlock(Out, FVector(11.4, Side * 8.0, EyeZ), FVector(1.0, 1.0, 6.0), Frame);
        PutBlock(Out, FVector(11.4, Side * 1.0, EyeZ), FVector(1.0, 1.0, 6.0), Frame);
        PutBlock(Out, FVector(6.0, Side * 10.4, EyeZ + 2.0), FVector(10.0, 1.0, 1.0), Frame); // the arms over the ears
    }
}
} // namespace

bool AppendCastLook(const ELook Look, const double H, TArray<FPWNpcBlock>& Out)
{
    switch (Look)
    {
    case ELook::Mira: NlMira(H, Out); return true;
    case ELook::Anindita: NlAnindita(H, Out); return true;
    case ELook::Subrata: NlSubrata(H, Out); return true;
    case ELook::Rupa: NlRupa(H, Out); return true;
    case ELook::Nirmal: NlNirmal(H, Out); return true;
    case ELook::Sahana: NlSahana(H, Out); return true;
    default: return false;
    }
}
} // namespace PWNpcLooks
