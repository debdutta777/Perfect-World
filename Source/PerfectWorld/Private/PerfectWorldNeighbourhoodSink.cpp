#include "PerfectWorldNeighbourhoodSink.h"

namespace PWNs
{
TArray<FNsRect> NsSubtract(const FNsRect& Rect, const TArray<FNsRect>& Cuts)
{
    TArray<FNsRect> Pieces;
    Pieces.Add(Rect);
    for (const FNsRect& Cut : Cuts)
    {
        TArray<FNsRect> Next;
        for (const FNsRect& P : Pieces)
        {
            if (!P.Overlaps(Cut))
            {
                Next.Add(P);
                continue;
            }
            // Left and right strips take the full height; the strips above and below take only the cut's width.
            if (Cut.X0 > P.X0)
            {
                Next.Add(FNsRect(P.X0, P.Y0, Cut.X0, P.Y1));
            }
            if (Cut.X1 < P.X1)
            {
                Next.Add(FNsRect(Cut.X1, P.Y0, P.X1, P.Y1));
            }
            const double MidX0 = FMath::Max(P.X0, Cut.X0);
            const double MidX1 = FMath::Min(P.X1, Cut.X1);
            if (Cut.Y0 > P.Y0)
            {
                Next.Add(FNsRect(MidX0, P.Y0, MidX1, Cut.Y0));
            }
            if (Cut.Y1 < P.Y1)
            {
                Next.Add(FNsRect(MidX0, Cut.Y1, MidX1, P.Y1));
            }
        }
        Pieces.Reset();
        for (const FNsRect& P : Next)
        {
            if (!P.IsEmpty())
            {
                Pieces.Add(P);
            }
        }
    }
    return Pieces;
}

FVector NsNormal(ENsFace Face)
{
    switch (Face)
    {
    case ENsFace::PlusX:
        return FVector(1.0, 0.0, 0.0);
    case ENsFace::MinusX:
        return FVector(-1.0, 0.0, 0.0);
    case ENsFace::PlusY:
        return FVector(0.0, 1.0, 0.0);
    case ENsFace::MinusY:
        return FVector(0.0, -1.0, 0.0);
    }
    return FVector(0.0, 1.0, 0.0);
}

FLaneFrame NsFaceFrame(const FNsRect& R, ENsFace Face, double& OutWidth, double& OutDepth)
{
    // V = -normal (back into the building); U = V x Z = (V.Y, -V.X), the viewer's right. The origin is the viewer's left end of the face.
    switch (Face)
    {
    case ENsFace::PlusY:
        OutWidth = R.Width();
        OutDepth = R.Height();
        return PWLane::MakeLaneFrame(R.X1, R.Y1, FVector(-1.0, 0.0, 0.0), FVector(0.0, -1.0, 0.0));
    case ENsFace::MinusY:
        OutWidth = R.Width();
        OutDepth = R.Height();
        return PWLane::MakeLaneFrame(R.X0, R.Y0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
    case ENsFace::PlusX:
        OutWidth = R.Height();
        OutDepth = R.Width();
        return PWLane::MakeLaneFrame(R.X1, R.Y0, FVector(0.0, 1.0, 0.0), FVector(-1.0, 0.0, 0.0));
    case ENsFace::MinusX:
        OutWidth = R.Height();
        OutDepth = R.Width();
        return PWLane::MakeLaneFrame(R.X0, R.Y1, FVector(0.0, -1.0, 0.0), FVector(1.0, 0.0, 0.0));
    }
    OutWidth = R.Width();
    OutDepth = R.Height();
    return PWLane::MakeLaneFrame(R.X0, R.Y0, FVector(1.0, 0.0, 0.0), FVector(0.0, 1.0, 0.0));
}
} // namespace PWNs
