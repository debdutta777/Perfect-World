#include "PerfectWorldNpcLooks.h"

namespace PWNpcLooks
{
void R66AppendTailorLook(const double H, TArray<FPWNpcBlock>& Out)
{
    const FColor Skin(179, 130, 94), Shirt(220, 216, 198), Burgundy(112, 42, 62), Trousers(48, 48, 58);
    const FColor Hair(38, 30, 28), Tape(238, 212, 110), Chalk(238, 238, 230);
    for (const double Side : {-1.0, 1.0})
    {
        PutBlock(Out, FVector(0, Side * 8, H * 0.24), FVector(12, 12, H * 0.48), Trousers,
            LegGroup(Side), FVector(0, Side * 8, H * 0.48));
        const FVector Shoulder(0, Side * 24, H * 0.84);
        PutBlock(Out, FVector(0, Side * 24, H * 0.74), FVector(10, 10, H * 0.20), Shirt, ArmGroup(Side), Shoulder);
        PutBlock(Out, FVector(0, Side * 24, H * 0.57), FVector(7, 7, H * 0.14), Skin, ArmGroup(Side), Shoulder);
    }
    PutBlock(Out, FVector(0, 0, H * 0.66), FVector(20, 38, H * 0.36), Burgundy);
    PutBlock(Out, FVector(11, 0, H * 0.66), FVector(2, 8, H * 0.36), Shirt);
    PutBlock(Out, FVector(0, 0, H * 0.924), FVector(20, 20, H * 0.152), Skin);
    PutBlock(Out, FVector(0, 0, H - 3), FVector(22, 22, 6), Hair);
    PutBlock(Out, FVector(-9, 0, H - 10), FVector(5, 21, 14), Hair);
    for (const double Side : {-1.0, 1.0})
        PutBlock(Out, FVector(12.5, Side * 11, H * 0.71), FVector(1.5, 3, H * 0.24), Tape);
    PutBlock(Out, FVector(12.5, -12, H * 0.55), FVector(2, 9, 7), Burgundy);
    PutBlock(Out, FVector(14, -12, H * 0.57), FVector(2, 4, 5), Chalk);
    AppendFace(H, 0, Out);
}
}
