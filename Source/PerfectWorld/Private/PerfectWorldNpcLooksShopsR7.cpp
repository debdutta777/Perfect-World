#include "PerfectWorldNpcLooksShopsR7.h"

namespace PWNpcLooks
{
namespace
{
/** The shared figure: legs, torso, sleeves, forearms, head. Per-person parts are added by the caller. */
struct FR74Figure
{
    FColor Skin, Legs, Sleeves, Top;
    double TorsoX = 20.0, TorsoY = 38.0, LegSpread = 8.0, LegSize = 12.0, ArmOff = 24.0, ArmT = 10.0;
};

void R74AddFigure(const FR74Figure& F, const double H, TArray<FPWNpcBlock>& Out)
{
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(0, Side * F.LegSpread, H * 0.24), FVector(F.LegSize, F.LegSize, H * 0.48), F.Legs,
                 LegGroup(Side), FVector(0, Side * F.LegSpread, H * 0.48));
        const FVector Shoulder(0, Side * F.ArmOff, H * 0.84);
        PutBlock(Out, FVector(0, Side * F.ArmOff, H * 0.74), FVector(F.ArmT, F.ArmT, H * 0.20), F.Sleeves, ArmGroup(Side), Shoulder);
        PutBlock(Out, FVector(0, Side * F.ArmOff, H * 0.57), FVector(F.ArmT - 3.0, F.ArmT - 3.0, H * 0.14), F.Skin, ArmGroup(Side), Shoulder);
    }
    PutBlock(Out, FVector(0, 0, H * 0.66), FVector(F.TorsoX, F.TorsoY, H * 0.36), F.Top);
    PutBlock(Out, FVector(0, 0, H * 0.924), FVector(20, 20, H * 0.152), F.Skin);
}

/** Square glasses: two rims and a bridge on the +X side of the face. */
void R74AddGlasses(const double H, TArray<FPWNpcBlock>& Out)
{
    const FColor Frame(30, 28, 34);
    for (const double Side : {-1.0, 1.0}) { PutBlock(Out, FVector(11, Side * 5, H * 0.925), FVector(1.5, 6, 5), Frame); }
    PutBlock(Out, FVector(11, 0, H * 0.925), FVector(1.5, 3, 1.5), Frame);
}

void R74Bimal(const double H, TArray<FPWNpcBlock>& Out)
{
    FR74Figure F;
    F.Skin = FColor(184, 134, 98); F.Legs = FColor(72, 70, 76); F.Sleeves = FColor(238, 230, 208); F.Top = FColor(238, 230, 208);
    F.TorsoX = 24; F.TorsoY = 44; F.LegSize = 14; F.LegSpread = 9; F.ArmOff = 27; F.ArmT = 11;
    R74AddFigure(F, H, Out);
    const FColor Red(176, 44, 40), Grey(158, 156, 152);
    PutBlock(Out, FVector(13.5, 0, H * 0.58), FVector(2, 36, H * 0.34), Red);   // the red waist apron
    PutBlock(Out, FVector(0, 0, H * 0.50), FVector(26, 46, 4), Red);            // its tie across the hips
    PutBlock(Out, FVector(0, 0, H - 2.5), FVector(21, 21, 5), Grey);            // close-cropped grey hair
    PutBlock(Out, FVector(-9.5, 0, H - 9), FVector(3, 20, 10), Grey);
    R74AddGlasses(H, Out);
    AppendFace(H, 0, Out);
}

void R74Tithi(const double H, TArray<FPWNpcBlock>& Out)
{
    FR74Figure F;
    F.Skin = FColor(196, 148, 110); F.Legs = FColor(46, 46, 58); F.Sleeves = FColor(52, 56, 124); F.Top = FColor(52, 56, 124);
    F.TorsoX = 18; F.TorsoY = 34; F.LegSize = 11; F.LegSpread = 7; F.ArmOff = 22; F.ArmT = 9;
    R74AddFigure(F, H, Out);
    const FColor Pale(240, 234, 214), Hair(30, 24, 26);
    PutBlock(Out, FVector(10, 0, H * 0.60), FVector(2, 26, H * 0.30), Pale);    // the pale bib apron
    PutBlock(Out, FVector(10, 0, H * 0.77), FVector(2, 10, H * 0.08), Pale);    // its bib
    PutBlock(Out, FVector(0, 0, H - 3), FVector(22, 22, 6), Hair);
    PutBlock(Out, FVector(-9, 0, H - 11), FVector(5, 20, 16), Hair);
    PutBlock(Out, FVector(-12, 0, H * 0.64), FVector(5, 7, H * 0.36), Hair);    // the long plait down her back
    AppendFace(H, 0, Out);
}

void R74Aparna(const double H, TArray<FPWNpcBlock>& Out)
{
    FR74Figure F;
    F.Skin = FColor(188, 140, 104); F.Legs = FColor(44, 46, 54); F.Sleeves = FColor(176, 214, 196); F.Top = FColor(176, 214, 196);
    F.TorsoX = 20; F.TorsoY = 38; F.LegSize = 12; F.LegSpread = 8; F.ArmOff = 24; F.ArmT = 10;
    R74AddFigure(F, H, Out);
    const FColor Hair(34, 28, 30), Badge(236, 236, 232), Lanyard(46, 96, 120);
    PutBlock(Out, FVector(0, 0, H - 3), FVector(22, 22, 6), Hair);
    PutBlock(Out, FVector(-9, 0, H - 10), FVector(5, 20, 14), Hair);
    PutBlock(Out, FVector(-12, 0, H - 11), FVector(7, 11, 10), Hair);           // the low bun
    R74AddGlasses(H, Out);
    PutBlock(Out, FVector(10.5, -6, H * 0.72), FVector(1.5, 2, H * 0.12), Lanyard);
    PutBlock(Out, FVector(11.5, -6, H * 0.64), FVector(1.5, 6, 7), Badge);      // the name badge
    AppendFace(H, 0, Out);
}

void R74Gouranga(const double H, TArray<FPWNpcBlock>& Out)
{
    FR74Figure F;
    F.Skin = FColor(168, 118, 84); F.Legs = FColor(74, 72, 64); F.Sleeves = FColor(196, 146, 54); F.Top = FColor(196, 146, 54);
    F.TorsoX = 26; F.TorsoY = 46; F.LegSize = 15; F.LegSpread = 10; F.ArmOff = 28; F.ArmT = 12;
    R74AddFigure(F, H, Out);
    const FColor Blue(46, 82, 138), Hair(52, 46, 44), Silver(176, 176, 178), Cloth(222, 214, 190);
    PutBlock(Out, FVector(14.5, 0, H * 0.56), FVector(2, 38, H * 0.30), Blue);  // the blue waist apron
    PutBlock(Out, FVector(0, 0, H * 0.50), FVector(28, 48, 4), Blue);
    PutBlock(Out, FVector(0, 0, H - 3), FVector(22, 22, 6), Hair);              // wavy hair, greying at the sides
    for (const double Side : {-1.0, 1.0}) { PutBlock(Out, FVector(-2, Side * 10, H - 9), FVector(16, 3, 10), Silver); }
    PutBlock(Out, FVector(-10, 0, H - 8), FVector(3, 19, 8), Silver);
    PutBlock(Out, FVector(-2, 31, H * 0.46), FVector(10, 6, 22), Cloth);        // the cloth shoulder bag
    PutBlock(Out, FVector(12, -12, H * 0.68), FVector(1.5, 4, H * 0.30), Cloth); // its strap across the chest
    PutBlock(Out, FVector(-12, 12, H * 0.68), FVector(1.5, 4, H * 0.30), Cloth);
    AppendFace(H, 0, Out);
}

void R74Koushik(const double H, TArray<FPWNpcBlock>& Out)
{
    FR74Figure F;
    F.Skin = FColor(178, 126, 90); F.Legs = FColor(74, 82, 92); F.Sleeves = FColor(154, 76, 48); F.Top = FColor(154, 76, 48);
    F.TorsoX = 21; F.TorsoY = 40; F.LegSize = 13; F.LegSpread = 8; F.ArmOff = 25; F.ArmT = 10;
    R74AddFigure(F, H, Out);
    const FColor Hair(34, 28, 28), Gamchha(196, 52, 46), Belt(40, 34, 30);
    // Small staggered blocks read as short curls; their upper edges stay at or below the authored crown.
    for (const double X : {-6.0, 0.0, 6.0})
    {
        for (const double Y : {-7.0, 0.0, 7.0}) { PutBlock(Out, FVector(X, Y, H - 3.5 - (X == 0.0 ? 0.0 : 1.0)), FVector(8, 8, 7), Hair); }
    }
    PutBlock(Out, FVector(-9, 0, H - 10), FVector(5, 21, 14), Hair);
    PutBlock(Out, FVector(0, -13, H * 0.82), FVector(14, 8, 5), Gamchha);       // a checked cloth over the shoulder
    PutBlock(Out, FVector(0, 0, H * 0.485), FVector(22, 41, 3), Belt);
    AppendFace(H, 0, Out);
}
} // namespace

void R74AppendShopLook(const ER74Shopper Shopper, const double H, TArray<FPWNpcBlock>& Out)
{
    switch (Shopper)
    {
    case ER74Shopper::Bimal: R74Bimal(H, Out); break;
    case ER74Shopper::Tithi: R74Tithi(H, Out); break;
    case ER74Shopper::Aparna: R74Aparna(H, Out); break;
    case ER74Shopper::Gouranga: R74Gouranga(H, Out); break;
    case ER74Shopper::Koushik: R74Koushik(H, Out); break;
    }
}
} // namespace PWNpcLooks
