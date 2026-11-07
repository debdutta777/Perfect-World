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
    PutBlock(Out, FVector(-9.0, 0.0, H - 0.06 * H), FVector(4.0, 20.0, 0.12 * H), Hair);
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
    PutBlock(Out, FVector(-9.0, 0.0, H - 0.07 * H), FVector(4.0, 20.0, 0.14 * H), Hair);
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

// Round 4 (R4-06). The five newcomers. Every hair, wrap and cap block stays at or under H (the head top is the authored height to
// 0.05 cm, which the NpcLooks test enforces), and every block stays inside the 34 cm capsule (arm outer edges 33.75 at most).
void NlR4Debashis(const double H, TArray<FPWNpcBlock>& Out)
{
    // 171 cm; sturdy repairer: close-cropped grey hair, a head loupe flipped up on the forehead, faded shirt with the sleeves
    // rolled, a dark leather work apron with a tool pocket, and sleeve garters on both upper arms.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(178, 126, 90);
    F.Legs = FColor(72, 78, 92);
    F.Top = F.Sleeves = FColor(206, 198, 176);
    F.LegW = 15.0; F.LegOff = 9.5; F.TorsoX = 22.0; F.TorsoY = 44.0; F.ArmOff = 27.5; F.ArmT = 12.0; F.bHands = true;
    NlAddFigure(F, H, Out);
    const FColor Leather(96, 64, 40), LeatherDark(72, 46, 30), Garter(58, 60, 70), Grey(150, 150, 148), Band(40, 40, 44);
    const FColor Brass(190, 150, 60), Red(180, 50, 40), Yellow(210, 170, 50);
    PutBlock(Out, FVector(12.5, 0.0, 0.58 * H), FVector(3.0, 36.0, 0.44 * H), Leather);     // the work apron
    PutBlock(Out, FVector(0.0, 0.0, 0.835 * H), FVector(4.0, 22.0, 2.5), LeatherDark);        // its neck loop
    PutBlock(Out, FVector(-11.5, 0.0, 0.50 * H), FVector(3.0, 40.0, 3.0), LeatherDark);       // and the waist tie at the back
    PutBlock(Out, FVector(14.5, 0.0, 0.47 * H), FVector(2.0, 15.0, 10.0), LeatherDark);       // the tool pocket
    PutBlock(Out, FVector(14.6, -3.5, 0.47 * H + 8.0), FVector(2.0, 2.4, 7.0), Red);          // screwdriver handles
    PutBlock(Out, FVector(14.6, 3.5, 0.47 * H + 8.0), FVector(2.0, 2.4, 6.0), Yellow);
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(0.0, Side * F.ArmOff, 0.84 * H - 0.07 * H), FVector(12.5, 12.5, 3.0), Garter, ArmGroup(Side),
                 FVector(0.0, Side * F.ArmOff, 0.84 * H)); // the sleeve garters
    }
    PutBlock(Out, FVector(0.0, 0.0, H - 2.5), FVector(22.0, 22.0, 5.0), Grey);                // close-cropped grey hair
    PutBlock(Out, FVector(0.0, -10.5, H - 8.0), FVector(20.0, 3.0, 7.0), Grey);
    PutBlock(Out, FVector(0.0, 10.5, H - 8.0), FVector(20.0, 3.0, 7.0), Grey);
    PutBlock(Out, FVector(0.0, 0.0, H - 7.2), FVector(21.5, 21.5, 1.6), Band);                // the loupe's headband
    PutBlock(Out, FVector(12.0, 5.5, H - 5.5), FVector(3.0, 8.0, 4.0), Brass);                // and the loupe, flipped up
    AppendFace(H, 0.0, Out);
}

void NlR4Mahua(const double H, TArray<FPWNpcBlock>& Out)
{
    // 162 cm; hardware shopkeeper: hair pinned in a grey coil, moss cardigan open over a cream blouse, a yellow tape measure
    // round her neck and hanging at the chest, a maroon ledger held at the hip, a pencil behind her ear.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(186, 136, 100);
    F.Legs = FColor(84, 82, 76);
    F.Top = FColor(232, 224, 206);
    F.Sleeves = FColor(98, 126, 104);
    F.LegW = 14.0; F.LegOff = 9.0; F.TorsoX = 22.0; F.TorsoY = 42.0; F.ArmOff = 26.5; F.ArmT = 12.0; F.bHands = true;
    NlAddFigure(F, H, Out);
    const FColor Moss(98, 126, 104), MossDark(80, 104, 88), Silver(190, 188, 186), Yellow(222, 186, 56), Dark(60, 60, 60);
    const FColor Maroon(110, 34, 38), Paper(238, 232, 214), Pencil(220, 180, 50);
    PutBlock(Out, FVector(-11.5, 0.0, 0.64 * H), FVector(3.0, 44.0, 0.40 * H), Moss);         // the cardigan: back, sides, two fronts
    PutBlock(Out, FVector(0.0, -22.5, 0.64 * H), FVector(22.0, 3.0, 0.40 * H), Moss);
    PutBlock(Out, FVector(0.0, 22.5, 0.64 * H), FVector(22.0, 3.0, 0.40 * H), Moss);
    PutBlock(Out, FVector(12.5, -14.5, 0.64 * H), FVector(3.0, 15.0, 0.40 * H), Moss);        // open at the front, cream blouse showing
    PutBlock(Out, FVector(12.5, 14.5, 0.64 * H), FVector(3.0, 15.0, 0.40 * H), Moss);
    PutBlock(Out, FVector(0.0, 0.0, 0.44 * H), FVector(25.0, 46.0, 3.0), MossDark);           // the ribbed hem
    PutBlock(Out, FVector(0.0, 0.0, 0.835 * H), FVector(24.0, 28.0, 2.2), Yellow);      // the tape measure round the neck
    PutBlock(Out, FVector(14.8, -5.5, 0.72 * H), FVector(1.2, 3.0, 0.20 * H), Yellow);        // and its two hanging lengths
    PutBlock(Out, FVector(14.8, 5.5, 0.72 * H), FVector(1.2, 3.0, 0.20 * H), Yellow);
    PutBlock(Out, FVector(14.8, 5.5, 0.62 * H), FVector(4.0, 5.0, 5.0), Dark);                // the tape's case
    PutBlock(Out, FVector(15.0, -10.0, 0.52 * H), FVector(5.0, 17.0, 23.0), Maroon);          // the ledger at her hip
    PutBlock(Out, FVector(17.8, -10.0, 0.52 * H), FVector(0.6, 14.0, 19.0), Paper);
    PutBlock(Out, FVector(0.0, 0.0, H - 2.5), FVector(22.0, 22.0, 5.0), Silver);              // hair pinned up, grey
    PutBlock(Out, FVector(0.0, -10.5, H - 8.0), FVector(20.0, 3.0, 6.0), Silver);
    PutBlock(Out, FVector(0.0, 10.5, H - 8.0), FVector(20.0, 3.0, 6.0), Silver);
    PutBlock(Out, FVector(-10.5, 0.0, H - 6.0), FVector(3.0, 20.0, 9.0), Silver);
    PutBlock(Out, FVector(-10.5, 0.0, H - 8.5), FVector(9.0, 13.0, 7.0), Silver);              // the coil
    PutBlock(Out, FVector(-1.0, 11.0, H - 11.0), FVector(8.0, 1.6, 1.6), Pencil);             // the pencil behind her ear
    AppendFace(H, 0.0, Out);
}

void NlR4Purnima(const double H, TArray<FPWNpcBlock>& Out)
{
    // 155 cm; the travelling vendor: a saffron head-wrap knotted at the back, an emerald shawl over both shoulders, an ochre
    // wrapped skirt with a dark hem, brass bangles and a leather money pouch on her belt. A broad build for the cart.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(162, 110, 78);
    F.Legs = FColor(176, 128, 70);
    F.Top = F.Sleeves = FColor(192, 150, 96);
    F.LegW = 15.0; F.LegOff = 9.0; F.TorsoX = 24.0; F.TorsoY = 44.0; F.ArmOff = 27.0; F.ArmT = 12.0; F.bHands = true;
    NlAddFigure(F, H, Out);
    const FColor Ochre(176, 128, 70), HemDark(120, 60, 40), Green(44, 112, 76), Saffron(226, 140, 40), Orange(180, 90, 30);
    const FColor Brass(220, 190, 80), Pouch(88, 58, 40), Belt(60, 40, 30);
    PutBlock(Out, FVector(0.0, 0.0, 0.38 * H), FVector(26.0, 44.0, 0.26 * H), Ochre);         // the wrapped skirt
    PutBlock(Out, FVector(0.0, 0.0, 0.25 * H), FVector(27.0, 45.0, 3.0), HemDark);            // and its hem band
    PutBlock(Out, FVector(0.0, 0.0, 0.485 * H), FVector(25.0, 46.0, 3.0), Belt);
    PutBlock(Out, FVector(13.5, 12.0, 0.47 * H), FVector(8.0, 14.0, 10.0), Pouch);            // the money pouch on her belt
    PutBlock(Out, FVector(-12.5, 0.0, 0.58 * H), FVector(3.0, 46.0, 0.52 * H), Green);        // the shawl: a back panel,
    PutBlock(Out, FVector(0.0, 0.0, 0.855 * H), FVector(26.0, 54.0, 4.0), Green);             // a cape over the shoulders,
    PutBlock(Out, FVector(12.5, -16.0, 0.68 * H), FVector(3.0, 10.0, 0.34 * H), Green);       // and two front lengths with the blouse between
    PutBlock(Out, FVector(12.5, 16.0, 0.68 * H), FVector(3.0, 10.0, 0.34 * H), Green);
    PutBlock(Out, FVector(-14.3, 0.0, 0.34 * H), FVector(2.0, 46.0, 3.0), Orange);            // a border along the back hem
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(0.0, Side * F.ArmOff, 0.84 * H - 0.20 * H - 0.09 * H), FVector(9.6, 9.6, 2.0), Brass, ArmGroup(Side),
                 FVector(0.0, Side * F.ArmOff, 0.84 * H)); // bangles
    }
    PutBlock(Out, FVector(0.0, 0.0, H - 3.5), FVector(23.0, 23.0, 7.0), Saffron);             // the head-wrap
    PutBlock(Out, FVector(11.9, 0.0, H - 5.5), FVector(1.0, 23.2, 2.0), Orange);              // its darker band (above the brows)
    PutBlock(Out, FVector(-12.5, 0.0, H - 8.5), FVector(9.0, 12.0, 8.0), Saffron);            // the knot at the back
    PutBlock(Out, FVector(-15.5, 4.0, H - 18.0), FVector(3.0, 8.0, 14.0), Saffron);           // and a tail hanging from it
    AppendFace(H, 0.0, Out);
}

void NlR4Arun(const double H, TArray<FPWNpcBlock>& Out)
{
    // 176 cm; an office worker on his way somewhere: neat side-parted black hair, a pale blue office shirt with a white open
    // collar and a pen in the pocket, charcoal trousers, a lanyard with a card, a leather messenger bag at his right hip.
    FNlFigure F;
    F.Skin = FColor(176, 128, 92);
    F.Legs = FColor(50, 52, 60);
    F.Top = F.Sleeves = F.Forearm = FColor(150, 178, 208);
    F.LegW = 14.0; F.LegOff = 9.0; F.TorsoX = 20.0; F.TorsoY = 40.0; F.ArmOff = 25.5; F.ArmT = 11.0; F.bHands = true;
    NlAddFigure(F, H, Out);
    const FColor Collar(236, 238, 242), Belt(30, 28, 30), Pocket(126, 152, 184), Pen(30, 40, 110), Lanyard(46, 140, 96);
    const FColor Card(240, 240, 236), Hair(24, 22, 24), Leather(120, 84, 56), LeatherDark(92, 62, 40);
    PutBlock(Out, FVector(0.0, 0.0, 0.835 * H), FVector(14.0, 16.0, 3.0), Collar);
    PutBlock(Out, FVector(0.0, 0.0, 0.485 * H), FVector(21.0, 41.0, 3.0), Belt);
    PutBlock(Out, FVector(10.8, -8.0, 0.74 * H), FVector(1.5, 8.0, 8.0), Pocket);              // the shirt pocket and its pen
    PutBlock(Out, FVector(11.2, -8.0, 0.74 * H + 6.0), FVector(1.0, 1.2, 6.0), Pen);
    PutBlock(Out, FVector(10.9, -3.0, 0.745 * H), FVector(1.0, 2.0, 0.12 * H), Lanyard);       // the lanyard, two strands
    PutBlock(Out, FVector(10.9, 3.0, 0.745 * H), FVector(1.0, 2.0, 0.12 * H), Lanyard);
    PutBlock(Out, FVector(11.3, 0.0, 0.64 * H), FVector(1.2, 6.0, 8.0), Card);                 // and the card
    PutBlock(Out, FVector(0.0, 0.0, H - 2.75), FVector(22.0, 22.0, 5.5), Hair);
    PutBlock(Out, FVector(0.0, -10.5, H - 8.5), FVector(20.0, 3.0, 7.0), Hair);
    PutBlock(Out, FVector(0.0, 10.5, H - 8.5), FVector(20.0, 3.0, 7.0), Hair);
    PutBlock(Out, FVector(-10.5, 0.0, H - 7.0), FVector(3.0, 20.0, 10.0), Hair);
    PutBlock(Out, FVector(10.8, 3.0, H - 7.5), FVector(2.5, 14.0, 3.0), Hair);                 // the fringe swept to one side
    PutBlock(Out, FVector(-3.0, 29.5, 0.40 * H), FVector(12.0, 8.0, 24.0), Leather);          // the messenger bag at his right hip
    PutBlock(Out, FVector(-3.0, 29.5, 0.40 * H + 13.0), FVector(12.5, 8.5, 4.0), LeatherDark);  // its flap
    PutBlock(Out, FVector(9.8, 13.0, 0.64 * H), FVector(1.6, 4.0, 0.34 * H), Leather);        // and its strap, front and back
    PutBlock(Out, FVector(-9.8, 13.0, 0.64 * H), FVector(1.6, 4.0, 0.34 * H), Leather);
    AppendFace(H, 0.0, Out);
}

void NlR4Rekha(const double H, TArray<FPWNpcBlock>& Out)
{
    // 161 cm; a slight woman in a rose saree with a cream border and the pallu over her RIGHT shoulder, cream blouse, a long
    // black plait tied with a red ribbon, and a furled black umbrella carried in her LEFT hand (it swings with that arm).
    FNlFigure F;
    F.Skin = F.Forearm = FColor(198, 150, 112);
    F.Legs = FColor(176, 52, 84);
    F.Top = F.Sleeves = FColor(236, 226, 206);
    F.LegW = 14.5; F.LegOff = 8.0; F.TorsoX = 18.0; F.TorsoY = 38.0; F.ArmOff = 24.5; F.ArmT = 11.0; F.bHands = true;
    NlAddFigure(F, H, Out);
    const FColor Rose(176, 52, 84), RoseDark(140, 36, 66), Gold(232, 214, 150), Hair(26, 22, 26), Ribbon(200, 60, 60);
    const FColor Black(30, 30, 34), Wood(130, 90, 58), Metal(160, 160, 166);
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(0.0, Side * F.LegOff, 6.0), FVector(F.LegW + 1.0, F.LegW + 1.0, 3.0), Gold, LegGroup(Side),
                 FVector(0.0, Side * F.LegOff, 0.48 * H)); // the border at the ankle
    }
    PutBlock(Out, FVector(0.0, 0.0, 0.40 * H), FVector(24.0, 40.0, 0.24 * H), Rose);           // the drape over the hips and thighs
    PutBlock(Out, FVector(12.5, 0.0, 0.40 * H), FVector(2.0, 13.0, 0.22 * H), RoseDark);       // the pleats
    PutBlock(Out, FVector(10.5, 12.0, 0.65 * H), FVector(3.0, 14.0, 0.38 * H), Rose);          // the pallu: front,
    PutBlock(Out, FVector(10.6, 17.5, 0.65 * H), FVector(3.2, 3.0, 0.38 * H), Gold);
    PutBlock(Out, FVector(-10.5, 10.0, 0.58 * H), FVector(3.0, 18.0, 0.52 * H), Rose);         // back,
    PutBlock(Out, FVector(0.0, 14.0, 0.855 * H), FVector(22.0, 14.0, 3.0), Rose);              // and over the right shoulder
    PutBlock(Out, FVector(0.0, 0.0, H - 3.0), FVector(22.0, 22.0, 6.0), Hair);
    PutBlock(Out, FVector(-9.0, 0.0, H - 0.06 * H), FVector(4.0, 20.0, 0.12 * H), Hair);
    PutBlock(Out, FVector(-12.5, 0.0, H - 0.12 * H - 0.17 * H), FVector(5.0, 6.0, 0.34 * H), Hair); // the long plait down her back
    PutBlock(Out, FVector(-12.5, 0.0, H - 0.46 * H + 1.5), FVector(6.0, 7.0, 2.5), Ribbon);
    const FVector Shoulder(0.0, -F.ArmOff, 0.84 * H);
    PutBlock(Out, FVector(0.0, -31.5, 0.35 * H), FVector(3.0, 3.0, 0.22 * H), Black, EPWNpcPartGroup::ArmLeft, Shoulder);   // the compact furled umbrella (its tip stays above 0.2 H so a seated pose never sinks it)
    PutBlock(Out, FVector(0.0, -31.5, 0.51 * H), FVector(3.4, 3.4, 0.10 * H), Wood, EPWNpcPartGroup::ArmLeft, Shoulder);    // gripped beside her hand
    PutBlock(Out, FVector(0.0, -31.5, 0.56 * H + 1.5), FVector(3.8, 3.8, 3.0), Wood, EPWNpcPartGroup::ArmLeft, Shoulder);   // with a cap on the handle
    PutBlock(Out, FVector(0.0, -31.5, 0.24 * H - 2.0), FVector(1.6, 1.6, 4.0), Metal, EPWNpcPartGroup::ArmLeft, Shoulder);  // and a metal tip
    AppendFace(H, 0.0, Out);
}
void R52PrabirLook(const double H, TArray<FPWNpcBlock>& Out)
{
    FNlFigure F;
    F.Skin = F.Forearm = FColor(172, 126, 94);
    F.Legs = FColor(226, 216, 187); F.Top = F.Sleeves = FColor(82, 105, 134);
    F.LegW = 12.5; F.LegOff = 8; F.TorsoX = 18; F.TorsoY = 36; F.ArmOff = 23; F.ArmT = 10;
    NlAddFigure(F, H, Out);
    const FColor Slate(82, 105, 134), Silver(202, 199, 194), Frame(70, 60, 50);
    PutBlock(Out, FVector(0, 0, 0.50 * H), FVector(21, 37, 0.20 * H), Slate); // short kurta hem
    PutBlock(Out, FVector(10, 0, 0.73 * H), FVector(2, 3, 0.12 * H), FColor(214, 210, 197));
    // Receding crown: silver side strips, no solid cap covering the forehead.
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(-2, Side * 10, H - 7), FVector(18, 3, 12), Silver);
        PutBlock(Out, FVector(11, Side * 5, H - 0.075 * H), FVector(1.5, 8, 6), Frame);
    }
    PutBlock(Out, FVector(-10, 0, H - 5), FVector(3, 19, 8), Silver);
    PutBlock(Out, FVector(11, 0, H - 0.075 * H), FVector(1.5, 3, 1.5), Frame);
    AppendFace(H, 0, Out);
}

void R52IshitaLook(const double H, TArray<FPWNpcBlock>& Out)
{
    FNlFigure F;
    F.Skin = F.Forearm = FColor(191, 143, 105);
    F.Legs = FColor(52, 55, 61); F.Top = F.Sleeves = FColor(46, 119, 116);
    F.TorsoY = 41; F.ArmOff = 26; F.ArmT = 11;
    NlAddFigure(F, H, Out);
    const FColor Teal(46, 119, 116), Hair(38, 30, 32), Canvas(182, 166, 125);
    PutBlock(Out, FVector(0, 0, 0.48 * H), FVector(24, 42, 0.30 * H), Teal);
    // Small staggered blocks read as short curls; their upper edges stay below the authored crown.
    for (const double X : {-7.0, 0.0, 7.0})
        for (const double Y : {-8.0, 0.0, 8.0})
            PutBlock(Out, FVector(X, Y, H - 3.5 - (X == 0 ? 0 : 1)), FVector(8, 9, 7), Hair);
    PutBlock(Out, FVector(-9, 0, H - 11), FVector(6, 23, 16), Hair);
    PutBlock(Out, FVector(-2, 29, 0.40 * H), FVector(11, 8, 24), Canvas);
    PutBlock(Out, FVector(11, 12, 0.64 * H), FVector(1.5, 4, 0.34 * H), Canvas);
    PutBlock(Out, FVector(-11, 12, 0.64 * H), FVector(1.5, 4, 0.34 * H), Canvas);
    AppendFace(H, 0, Out);
}

void R64MadhabiLook(const double H, TArray<FPWNpcBlock>& Out)
{
    // 156 cm; a slight 58-year-old records clerk: dusty-blue cotton saree with a cream border tucked at the waist, cream blouse,
    // grey hair in a low knot, a manila file folder held at her left hip.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(176, 128, 96);
    F.Legs = FColor(88, 102, 138); F.Top = F.Sleeves = FColor(234, 228, 214);
    F.LegW = 14; F.LegOff = 8; F.TorsoX = 19; F.TorsoY = 39; F.ArmOff = 24.5; F.ArmT = 11; F.bHands = true;
    NlAddFigure(F, H, Out);
    const FColor Saree(88, 102, 138), Border(226, 214, 176), Grey(176, 174, 172), Folder(208, 176, 108);
    for (const double Side : {-1.0, 1.0})
        PutBlock(Out, FVector(0, Side * F.LegOff, 6), FVector(F.LegW + 1, F.LegW + 1, 3), Border, LegGroup(Side), FVector(0, Side * F.LegOff, 0.48 * H));
    PutBlock(Out, FVector(0, 0, 0.40 * H), FVector(24, 40, 0.24 * H), Saree);    // the drape over the hips and thighs
    PutBlock(Out, FVector(12.5, 0, 0.40 * H), FVector(2, 12, 0.22 * H), Border); // the pleats' cream edge
    PutBlock(Out, FVector(0, 0, H - 3.5), FVector(22, 22, 7), Grey);            // grey hair over the crown
    PutBlock(Out, FVector(-11, 0, H - 11), FVector(8, 12, 12), Grey);           // in a low knot
    PutBlock(Out, FVector(2, -29, 0.42 * H), FVector(10, 4, 24), Folder);       // the manila folder
    AppendFace(H, 0, Out);
}

void R64ArupLook(const double H, TArray<FPWNpcBlock>& Out)
{
    // 179 cm; a medium build in navy trousers, a pale shirt under an orange dispatch vest with reflective bands, a close
    // buzz cut and a handheld parcel scanner at his right hip.
    FNlFigure F;
    F.Skin = F.Forearm = FColor(186, 138, 102);
    F.Legs = FColor(54, 60, 72); F.Top = F.Sleeves = FColor(226, 226, 220);
    F.TorsoY = 42; F.ArmOff = 27; F.ArmT = 12;
    NlAddFigure(F, H, Out);
    const FColor Vest(226, 118, 38), Band(222, 226, 170), Hair(30, 26, 26), Scanner(36, 38, 44), Screen(120, 190, 210);
    PutBlock(Out, FVector(0, 0, 0.66 * H), FVector(23, 44, 0.34 * H), Vest);
    PutBlock(Out, FVector(0, 0, 0.60 * H), FVector(23.4, 44.4, 3), Band);
    PutBlock(Out, FVector(0, 0, 0.74 * H), FVector(23.4, 44.4, 3), Band);
    PutBlock(Out, FVector(0, 0, H - 2.5), FVector(21, 21, 5), Hair);            // the close crop, top at his height
    PutBlock(Out, FVector(6, 28, 0.50 * H), FVector(6, 6, 14), Scanner);        // the handheld scanner
    PutBlock(Out, FVector(9.2, 28, 0.50 * H + 3), FVector(0.6, 4, 6), Screen);  // and its screen
    AppendFace(H, 0, Out);
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
    case ELook::Debashis: NlR4Debashis(H, Out); return true;
    case ELook::Mahua: NlR4Mahua(H, Out); return true;
    case ELook::Purnima: NlR4Purnima(H, Out); return true;
    case ELook::Arun: NlR4Arun(H, Out); return true;
    case ELook::Rekha: NlR4Rekha(H, Out); return true;
    case ELook::Prabir: R52PrabirLook(H, Out); return true;
    case ELook::Ishita: R52IshitaLook(H, Out); return true;
    case ELook::Satyajit: R66AppendTailorLook(H, Out); return true;
    case ELook::Madhabi: R64MadhabiLook(H, Out); return true;
    case ELook::Arup: R64ArupLook(H, Out); return true;
    default: return false;
    }
}
} // namespace PWNpcLooks
