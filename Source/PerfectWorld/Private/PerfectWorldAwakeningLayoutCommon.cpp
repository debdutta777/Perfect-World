#include "PerfectWorldAwakeningLayout.h"

#include "PerfectWorldSchoolCampus.h"

// The pieces of the floor layout every builder shares: palette, asset ids, the part and text helpers, the site and the pad shapes.

namespace PWAwakening
{
FColor AwColourSrgb(const EAwColour Colour)
{
    switch (Colour)
    {
    case EAwColour::FloorPanel:
        return FColor(34, 38, 66);
    case EAwColour::GridLine:
        return FColor(40, 140, 140);
    case EAwColour::WallPanel:
        return FColor(52, 58, 96);
    case EAwColour::Ceiling:
        return FColor(24, 26, 44);
    case EAwColour::GateTeal:
        return FColor(47, 209, 196); // gate_teal base (#2FD1C4)
    case EAwColour::GateViolet:
        return FColor(138, 79, 224); // gate_violet base (#8A4FE0)
    case EAwColour::Ink:
        return FColor(38, 38, 51); // ink (#262633)
    case EAwColour::Paper:
        return FColor(239, 232, 216); // paper (#EFE8D8)
    case EAwColour::HazardYellow:
        return FColor(240, 193, 47); // taxi_yellow (#F0C12F)
    case EAwColour::HazardDark:
        return FColor(28, 28, 36);
    case EAwColour::SafeGreen:
        return FColor(78, 142, 58); // monsoon_leaf (#4E8E3A)
    case EAwColour::PadTeal:
        return FColor(30, 150, 142);
    case EAwColour::PadViolet:
        return FColor(100, 60, 170);
    case EAwColour::Count:
        break;
    }
    return FColor::White;
}

TArray<FName> AwAllAssetIds()
{
    TArray<FName> Out;
    for (const TCHAR* Id : {AwAssetIds::FloorGrid, AwAssetIds::WallPanel, AwAssetIds::WallShifting, AwAssetIds::LightBarrier, AwAssetIds::StationCuboid,
                            AwAssetIds::ChoicePad, AwAssetIds::ResetPad, AwAssetIds::SafeArea, AwAssetIds::PlatformCycling, AwAssetIds::HazardStripe,
                            AwAssetIds::EntryKiosk, AwAssetIds::DeliveryCrates, AwAssetIds::DeliveryMarker, AwAssetIds::CorridorBarrier})
    {
        Out.AddUnique(FName(Id));
    }
    return Out;
}

bool AwIsValidAssetId(const FString& Id)
{
    TArray<FString> Segments;
    Id.ParseIntoArray(Segments, TEXT("."), false);
    if (Segments.Num() < 3 || Segments.Num() > 4)
    {
        return false;
    }
    for (int32 S = 0; S < Segments.Num(); ++S)
    {
        const FString& Segment = Segments[S];
        if (Segment.IsEmpty() || Segment.Len() > 32)
        {
            return false;
        }
        for (int32 C = 0; C < Segment.Len(); ++C)
        {
            const TCHAR Ch = Segment[C];
            const bool bLetter = Ch >= TEXT('a') && Ch <= TEXT('z');
            const bool bDigit = Ch >= TEXT('0') && Ch <= TEXT('9');
            if (!(bLetter || bDigit || Ch == TEXT('_')))
            {
                return false;
            }
            if (C == 0 && (S == 0 ? !bLetter : !(bLetter || bDigit)))
            {
                return false;
            }
        }
    }
    return true;
}

const FAwPart* FAwFloorLayout::FindPart(const TCHAR* Id) const
{
    const FName Wanted(Id);
    for (const FAwPart& Part : Parts)
    {
        if (Part.Id == Wanted)
        {
            return &Part;
        }
    }
    return nullptr;
}

TArray<const FAwPart*> FAwFloorLayout::FindParts(const FString& Prefix) const
{
    TArray<const FAwPart*> Found;
    for (const FAwPart& Part : Parts)
    {
        if (Part.Id.ToString().StartsWith(Prefix))
        {
            Found.Add(&Part);
        }
    }
    return Found;
}

TArray<const FAwPadSpec*> FAwFloorLayout::AllPads() const
{
    TArray<const FAwPadSpec*> Out;
    for (const FAwStationSpec& Station : Stations)
    {
        for (const FAwPadSpec& Pad : Station.Pads)
        {
            Out.Add(&Pad);
        }
    }
    Out.Add(&EntryPad);
    Out.Add(&LeavePad);
    return Out;
}

void AwAddPart(FAwFloorLayout& L, const TCHAR* Id, const TCHAR* Asset, const EAwColour Colour, const EAwRole Role, const double X0, const double Y0,
               const double Z0, const double X1, const double Y1, const double Z1)
{
    AwAddWorldPart(L, Id, Asset, Colour, Role, AwWorld(L, X0, Y0, Z0), AwWorld(L, X1, Y1, Z1));
}

void AwAddWorldPart(FAwFloorLayout& L, const TCHAR* Id, const TCHAR* Asset, const EAwColour Colour, const EAwRole Role, const FVector& A, const FVector& B)
{
    const FVector Min(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y), FMath::Min(A.Z, B.Z));
    const FVector Max(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y), FMath::Max(A.Z, B.Z));
    if (Max.X - Min.X < 0.5 || Max.Y - Min.Y < 0.5 || Max.Z - Min.Z < 0.5)
    {
        return;
    }
    FAwPart Part;
    Part.Id = FName(Id);
    Part.AssetId = FName(Asset);
    Part.Colour = Colour;
    Part.Role = Role;
    Part.Box = FBox(Min, Max);
    L.Parts.Add(Part);
}

void AwAddText(FAwFloorLayout& L, const TCHAR* Id, const FString& Text, const double X, const double Y, const double Z, const float SizeCm)
{
    FAwText Sign;
    Sign.Id = FName(Id);
    Sign.Text = Text;
    Sign.Location = AwWorld(L, X, Y, Z);
    Sign.SizeCm = SizeCm;
    L.Texts.Add(Sign);
}

FAwFloorSite AwSiteFromCampus()
{
    FAwFloorSite Site;
    const PWSchool::FScCampus Campus = PWSchool::ScReadCampus();
    if (!Campus.bValid)
    {
        return Site;
    }
    // Beneath the playground, 40 m down: sealed on all sides, so nothing of it shows from the campus or the lane.
    Site.Origin = FVector(Campus.Playground.CentreX(), Campus.Playground.CentreY() - 1900.0, PWSchool::ScGroundZ - 4000.0);
    // The terminal stands on the forecourt's north edge in front of the teaching block's east end, clear of the flag plinth (X 7000..7200).
    const double X = Campus.Teaching.X1 - 700.0;
    Site.EntryStand = FVector(X, Campus.Forecourt.Y1 - 180.0, PWSchool::ScGroundZ);
    Site.EntryYaw = 90.0;
    Site.EntryKiosk = FVector(X, Campus.Forecourt.Y1 - 90.0, PWSchool::ScGroundZ);
    Site.bValid = true;
    return Site;
}

bool AwBuildLayout(FAwFloorLayout& Out, const FAwFloorSite& Site, const FAwCast& Cast)
{
    if (!Site.bValid)
    {
        return false;
    }
    FAwFloorLayout L;
    L.Site = Site;
    L.Bounds = FBox(AwWorld(L, -1400.0, 0.0, 0.0), AwWorld(L, 1400.0, 3800.0, AwNumbers::CeilingZ));
    AwBuildShell(L);
    AwBuildStations(L);
    AwBuildTerminal(L);

    FAwFigureSpec Teacher;
    Teacher.PersonId = Cast.Teacher;
    Teacher.Name = Cast.TeacherName.ToString();
    Teacher.Feet = AwWorld(L, -1100.0, 950.0, 0.0);
    Teacher.Yaw = 0.0;
    Teacher.bTeacher = true;
    L.Figures.Add(Teacher);
    FAwFigureSpec A;
    A.PersonId = Cast.SafeClassmateA;
    A.Name = TEXT("A classmate");
    A.Feet = AwWorld(L, -1000.0, 600.0, 0.0);
    A.Yaw = 20.0;
    L.Figures.Add(A);
    FAwFigureSpec B;
    B.PersonId = Cast.SafeClassmateB;
    B.Name = TEXT("A classmate");
    B.Feet = AwWorld(L, -1180.0, 650.0, 0.0);
    B.Yaw = -20.0;
    L.Figures.Add(B);
    for (const FAwStationSpec& Station : L.Stations)
    {
        FAwFigureSpec F;
        F.PersonId = Station.FigurePerson;
        const FAwSituationDef& Def = AwCatalogue()[Station.Index];
        F.Name = Def.FigureName.ToString();
        F.Feet = Station.FigureFeet;
        F.Yaw = Station.FigureYaw;
        L.Figures.Add(F);
    }
    L.bValid = true;
    Out = MoveTemp(L);
    return true;
}

void AwShapeBoxes(const EAwShape Shape, TArray<TPair<FVector, FVector>>& Out)
{
    // (centre offset from the pad centre; Z is the middle of a 2 cm raised bar sitting on the 4 cm slab), size.
    const auto Bar = [&Out](double X, double Y, double W, double D)
    {
        Out.Add(TPair<FVector, FVector>(FVector(X, Y, 5.0), FVector(W, D, 2.0)));
    };
    switch (Shape)
    {
    case EAwShape::Plus:
        Bar(0, 0, 26, 6);
        Bar(0, 0, 6, 26);
        break;
    case EAwShape::Frame:
        Bar(0, 15, 30, 5);
        Bar(0, -15, 30, 5);
        Bar(15, 0, 5, 25);
        Bar(-15, 0, 5, 25);
        break;
    case EAwShape::Bars:
        Bar(-8, 0, 6, 28);
        Bar(8, 0, 6, 28);
        break;
    case EAwShape::Tee:
        Bar(0, 10, 28, 6);
        Bar(0, -2, 6, 22);
        break;
    case EAwShape::Arrow:
        Bar(0, -4, 6, 22);
        Bar(-6, 5, 6, 6);
        Bar(6, 5, 6, 6);
        Bar(0, 11, 6, 6);
        break;
    case EAwShape::Back:
        Bar(4, 0, 22, 6);
        Bar(-5, 6, 6, 6);
        Bar(-5, -6, 6, 6);
        Bar(-11, 0, 6, 6);
        break;
    case EAwShape::Dots:
        Bar(-12, 0, 6, 6);
        Bar(0, 0, 6, 6);
        Bar(12, 0, 6, 6);
        break;
    case EAwShape::Ring:
        Bar(0, 13, 24, 4);
        Bar(0, -13, 24, 4);
        Bar(13, 0, 4, 22);
        Bar(-13, 0, 4, 22);
        Bar(0, 0, 6, 6);
        break;
    }
}
} // namespace PWAwakening
