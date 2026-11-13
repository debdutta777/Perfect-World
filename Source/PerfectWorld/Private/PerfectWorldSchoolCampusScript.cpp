#include "PerfectWorldSchoolCampusParts.h"

// The Bengali line of the gate signboard, "Nabapara Vidyalaya" in the block-text style the project uses until real art arrives (M9).
//
// This is a STYLISED PLACEHOLDER: each glyph is a 5 x 7 block pattern with the continuous headline (matra) that marks the script. It is
// recognisably Bengali-shaped lettering, not a typographically correct font; the exact Unicode string is ScBengaliSchoolName so the
// real font can replace these blocks without changing the sign's meaning. The engine's default font has no Bengali glyphs, which is why
// the line is built from cubes and not drawn as text.

namespace PWSchool
{
namespace
{
/** The sign's Bengali text: Nabapara Vidyalaya. Kept for the M9 font pass and for the tests. */
const TCHAR* const ScBengaliSchoolName = TEXT("নবপাড়া বিদ্যালয়");

struct FScGlyph
{
    int32 Width;
    bool bHeadline;
    const char* Rows[7]; // top row first; '#' is a filled block
};

// The letters, in the order they are drawn. The i-kar comes before its consonant, as written.
const FScGlyph ScNa = {5, true, {"#####", "..#.#", ".##.#", "#.#.#", "#..##", "#..#.", ".###."}};
const FScGlyph ScBa = {5, true, {"#####", "#...#", "#...#", "#.###", "#...#", "#...#", ".###."}};
const FScGlyph ScPa = {5, true, {"#####", "#...#", "#.#.#", "#.#.#", "#.###", "#...#", "....."}};
const FScGlyph ScAa = {2, true, {"##", ".#", ".#", ".#", ".#", ".#", ".#"}};
const FScGlyph ScRra = {5, true, {"#####", "#..##", "#..#.", "###..", "#..#.", "#..##", "..#.."}};
const FScGlyph ScIkar = {3, true, {"###", "#..", "#..", "#..", "#..", "#..", "#.."}};
const FScGlyph ScDa = {5, true, {"#####", "#...#", "#...#", ".#..#", "..#.#", "..#.#", "...#."}};
const FScGlyph ScYaPhala = {3, false, {"...", "...", "...", ".#.", "#.#", "#.#", ".#."}};
const FScGlyph ScLa = {5, true, {"#####", "..#.#", ".#..#", "#.#.#", "#.#.#", ".##.#", "..#.."}};
const FScGlyph ScYya = {5, true, {"#####", "#...#", "#.#.#", "#.#.#", "#.###", ".#..#", "..#.."}};

constexpr int32 ScWordGap = 4;

/** One word as a list of glyph pointers. */
void ScWordColumns(const TArray<const FScGlyph*>& Word, int32& OutColumns)
{
    OutColumns = 0;
    for (int32 I = 0; I < Word.Num(); ++I)
    {
        OutColumns += Word[I]->Width + (I + 1 < Word.Num() ? 1 : 0);
    }
}

/** Draws one word left to right starting at column Start; runs of filled blocks in a row become one box. */
void ScDrawWord(FScBuilt& B, const TArray<const FScGlyph*>& Word, int32 Start, double X0, double Y, double ZBottom, double Pixel, int32& Run)
{
    int32 Column = Start;
    for (int32 G = 0; G < Word.Num(); ++G)
    {
        const FScGlyph& Glyph = *Word[G];
        for (int32 Row = 0; Row < 7; ++Row)
        {
            const double Z0 = ZBottom + static_cast<double>(6 - Row) * Pixel;
            int32 RunStart = -1;
            // One column past the glyph closes the last run; the headline reaches across the gap to the next glyph.
            const bool bJoinsNext = Row == 0 && Glyph.bHeadline && G + 1 < Word.Num() && Word[G + 1]->bHeadline;
            const int32 Limit = Glyph.Width + (bJoinsNext ? 1 : 0);
            for (int32 C = 0; C <= Limit; ++C)
            {
                const bool bOn = C < Glyph.Width ? Glyph.Rows[Row][C] == '#' : (C == Glyph.Width && bJoinsNext);
                if (bOn && RunStart < 0)
                {
                    RunStart = C;
                }
                if (!bOn && RunStart >= 0)
                {
                    const double Xa = X0 + static_cast<double>(Column + RunStart) * Pixel;
                    const double Xb = X0 + static_cast<double>(Column + C) * Pixel;
                    ScBox(B, *FString::Printf(TEXT("gate.sign_bengali.run%d"), Run++), ELaneColour::TrimLight, Xa, Y - 3.0, Z0, Xb, Y, Z0 + Pixel);
                    RunStart = -1;
                }
            }
        }
        Column += Glyph.Width + 1;
    }
}
} // namespace

void ScBuildBengaliLine(FScBuilt& B, double CentreX, double Y, double ZBottom, double PixelCm)
{
    TArray<const FScGlyph*> First;
    First.Add(&ScNa);
    First.Add(&ScBa);
    First.Add(&ScPa);
    First.Add(&ScAa);
    First.Add(&ScRra);
    First.Add(&ScAa);
    TArray<const FScGlyph*> Second;
    Second.Add(&ScIkar);
    Second.Add(&ScBa);
    Second.Add(&ScDa);
    Second.Add(&ScYaPhala);
    Second.Add(&ScAa);
    Second.Add(&ScLa);
    Second.Add(&ScYya);
    int32 FirstColumns = 0;
    int32 SecondColumns = 0;
    ScWordColumns(First, FirstColumns);
    ScWordColumns(Second, SecondColumns);
    const int32 Total = FirstColumns + ScWordGap + SecondColumns;
    const double X0 = CentreX - 0.5 * static_cast<double>(Total) * PixelCm;
    int32 Run = 0;
    ScDrawWord(B, First, 0, X0, Y, ZBottom, PixelCm, Run);
    ScDrawWord(B, Second, FirstColumns + ScWordGap, X0, Y, ZBottom, PixelCm, Run);
    (void)ScBengaliSchoolName;
}
} // namespace PWSchool
