#pragma once

#include "CoreMinimal.h"
#include "PerfectWorldAwakeningCatalogue.h"

/**
 * R9-06: the geometry of the supervised tutorial floor (prefix Aw / PWAwakening). Pure data: built without a world so tests read
 * the authored positions back. Everything is world centimetres.
 *
 * The floor is one compact sealed block room (2860 x 3860 cm, 520 cm high) UNDER the school campus (FAwFloorSite::Origin), entered
 * from a "virtual floor" terminal on the forecourt. Local frame inside the room: X east, Y north (the way the player first faces),
 * Z up from the floor top; world = Origin + local.
 *
 *   safe area (teacher)   X -1400..-700  Y 150..1350      west
 *   arrival pad           (0, 260)       return pad (350, 260)
 *   three bays, stride 950, base Y 700 / 1650 / 2600, stand point (300, base+120), six pads on a 95 cm arc around it
 *
 * Palette: the reserved Gate-kit ramps (gate_teal, gate_violet) with ink and paper (ASSET_PIPELINE section 1.4); the ordinary world
 * never uses them, so teal-violet on this floor reads as "virtual tower". Every visible part carries a Gate-kit asset id
 * (arch.gatekit.*, see AwAssetIds); the floor needs no mesh.
 */
namespace PWAwakening
{
enum class EAwColour : uint8
{
    FloorPanel,
    GridLine,
    WallPanel,
    Ceiling,
    GateTeal,
    GateViolet,
    Ink,
    Paper,
    HazardYellow,
    HazardDark,
    SafeGreen,
    PadTeal,
    PadViolet,
    Count
};

/** Who a part blocks. Visual parts have no collision. */
enum class EAwRole : uint8
{
    Visual,
    Solid, // blocks the pawn, camera and visibility
    Prop   // invisible box hugging a visible solid prop
};

constexpr int32 AwColourCount = static_cast<int32>(EAwColour::Count);
constexpr int32 AwRoleCount = 3;

/** sRGB colour of a palette entry. */
FColor AwColourSrgb(EAwColour Colour);

/** The Gate-kit asset ids the floor's parts reference (category arch, subcategory gatekit; ASSET_PIPELINE section 3.1). */
namespace AwAssetIds
{
inline constexpr const TCHAR* FloorGrid = TEXT("arch.gatekit.floor_grid");
inline constexpr const TCHAR* WallPanel = TEXT("arch.gatekit.wall_panel");
inline constexpr const TCHAR* WallShifting = TEXT("arch.gatekit.wall_shifting");
inline constexpr const TCHAR* LightBarrier = TEXT("arch.gatekit.light_barrier");
inline constexpr const TCHAR* StationCuboid = TEXT("arch.gatekit.station_cuboid");
inline constexpr const TCHAR* ChoicePad = TEXT("arch.gatekit.choice_pad");
inline constexpr const TCHAR* ResetPad = TEXT("arch.gatekit.reset_pad");
inline constexpr const TCHAR* SafeArea = TEXT("arch.gatekit.safe_area_marker");
inline constexpr const TCHAR* PlatformCycling = TEXT("arch.gatekit.platform_cycling");
inline constexpr const TCHAR* HazardStripe = TEXT("arch.gatekit.hazard_stripe");
inline constexpr const TCHAR* EntryKiosk = TEXT("arch.gatekit.entry_kiosk");
inline constexpr const TCHAR* DeliveryCrates = TEXT("arch.gatekit.delivery_crates");
inline constexpr const TCHAR* DeliveryMarker = TEXT("arch.gatekit.delivery_marker");
inline constexpr const TCHAR* CorridorBarrier = TEXT("arch.gatekit.corridor_barrier");
} // namespace AwAssetIds

/** The ids above, once each, for coverage checks. */
TArray<FName> AwAllAssetIds();
/** True when the id satisfies the registry's id regex ^[a-z][a-z0-9_]*(\.[a-z0-9][a-z0-9_]*){2,3}$ (segments of at most 32 characters). */
bool AwIsValidAssetId(const FString& Id);

/** One named box, kept so tests can read the geometry back (as PWSchool::FScPart). */
struct FAwPart
{
    FName Id;
    FName AssetId;
    EAwColour Colour = EAwColour::FloorPanel;
    EAwRole Role = EAwRole::Visual;
    FBox Box = FBox(ForceInit);
};

/** Text drawn on a sign (APWShopHoursSignActor). Faces -Y. */
struct FAwText
{
    FName Id;
    FString Text;
    FVector Location = FVector::ZeroVector;
    float SizeCm = 8.0f;
};

struct FAwLight
{
    FVector Position = FVector::ZeroVector;
    float Radius = 900.0f;
    float Lumens = 2500.0f;
    FLinearColor Colour = FLinearColor(0.80f, 0.95f, 1.0f);
};

enum class EAwPadKind : uint8
{
    Choice,   // forwards (situation, choice) to the bridge
    Withdraw, // forwards the withdrawal choice, then moves the player to the safe area
    Advance,  // steps the station's moving parts (announced); presentation only, never reaches the bridge
    Reset,    // returns the station's moving parts to their start and the player to the reset pad; never reaches the bridge
    Enter,    // the campus terminal: step onto the floor
    Leave     // the return pad: step back onto the campus
};

struct FAwPadSpec
{
    FName Id;
    EAwPadKind Kind = EAwPadKind::Choice;
    int32 StationIndex = INDEX_NONE;
    FName Situation;
    FName Choice;
    EAwStance Stance = EAwStance::Help;
    EAwShape Shape = EAwShape::Plus;
    FString Label;
    /** Floor-level centre of the pad. */
    FVector Centre = FVector::ZeroVector;
    /** The interaction point (45 cm over the centre, on the capsule axis of someone standing at the stand point). */
    FVector Point = FVector::ZeroVector;
};

struct FAwPose
{
    FVector Centre = FVector::ZeroVector;
    EAwColour Colour = EAwColour::GateTeal;
};

/** A block that moves between poses (platform, wall, barrier, crates, marker). */
struct FAwMoverSpec
{
    FName Id;
    FName AssetId;
    FVector Size = FVector(100.0, 100.0, 10.0);
    TArray<FAwPose> Poses;
    int32 StartPose = 0;
    /** Cycles by itself between pose 0 and 1 while not held and not reduced motion. */
    bool bAutoCycle = false;
    /** Flickers between its pose colour and ink until told to steady (not under reduced motion). */
    bool bFlicker = false;
    bool bSolid = true;
};

struct FAwStationSpec
{
    int32 Index = 0;
    FName Situation;
    FVector Console = FVector::ZeroVector;
    FVector Stand = FVector::ZeroVector;
    FVector ResetPad = FVector::ZeroVector;
    /** The marked hazard strip (world box); entering it asks for a gentle reset. Zero-size when the station has none. */
    FBox Hazard = FBox(ForceInit);
    /** Where the primary mover, in its pose 0, covers the hazard (so standing on it is safe); zero-size when none. */
    FBox SafeWhenAligned = FBox(ForceInit);
    TArray<FAwPadSpec> Pads;
    FAwMoverSpec Primary;
    FAwMoverSpec Secondary;
    FVector FigureFeet = FVector::ZeroVector;
    double FigureYaw = 180.0;
    FName FigurePerson;
    FVector StatusAt = FVector::ZeroVector;
};

struct FAwFigureSpec
{
    FName PersonId;
    FString Name;
    FVector Feet = FVector::ZeroVector;
    double Yaw = 0.0;
    bool bTeacher = false;
};

/** Where the floor goes and where it is entered from. */
struct FAwFloorSite
{
    /** World position of the floor's local origin (floor top, south-centre). */
    FVector Origin = FVector::ZeroVector;
    /** Feet of the player standing at the campus terminal, facing +Y. */
    FVector EntryStand = FVector::ZeroVector;
    double EntryYaw = 90.0;
    /** Centre of the terminal's front face at floor level. */
    FVector EntryKiosk = FVector::ZeroVector;
    bool bValid = false;
};

/** The default site: the floor beneath the playground, the terminal on the forecourt in front of the teaching block's east end. */
FAwFloorSite AwSiteFromCampus();

/** Settings the integrator, tests and the acceptance driver may change. Nothing here touches rules. */
struct FAwFloorConfig
{
    /** Empty: use AwSiteFromCampus(). */
    TOptional<FAwFloorSite> Site;
    FAwCast Cast;
    /** Multiplies every on-screen and in-world text size; clamped 1.0..2.0. The presentation settings own the real value (hook). */
    float TextScale = 1.0f;
    /** Empty follows UPerfectWorldPresentationSettings. */
    TOptional<bool> ReducedMotion;
    /** Tests: transfers complete in the same call (no fade, no hold). */
    bool bInstantTransitions = false;
    /** Spawn the NPC-kit people when the floor is spawned (otherwise on entering). */
    bool bSpawnFiguresAtOnce = false;
    /** Spawn the HUD overlay (needs a game viewport; off in tests). */
    bool bUseHud = true;
};

struct FAwFloorLayout
{
    bool bValid = false;
    FAwFloorSite Site;
    FBox Bounds = FBox(ForceInit);
    FVector Arrival = FVector::ZeroVector;
    double ArrivalYaw = 90.0;
    FVector SafeStand = FVector::ZeroVector;
    double SafeYaw = 90.0;
    FBox SafeArea = FBox(ForceInit);
    FAwPadSpec EntryPad;
    FAwPadSpec LeavePad;
    TArray<FAwStationSpec> Stations;
    TArray<FAwFigureSpec> Figures;
    TArray<FAwPart> Parts;
    TArray<FAwText> Texts;
    TArray<FAwLight> Lights;

    const FAwPart* FindPart(const TCHAR* Id) const;
    TArray<const FAwPart*> FindParts(const FString& Prefix) const;
    /** Every pad: three transfer-free station sets plus the two transfer pads. */
    TArray<const FAwPadSpec*> AllPads() const;
};

/** Pad numbers shared by the builder and the tests. */
namespace AwNumbers
{
constexpr double PadRadiusCm = 95.0;
constexpr double PadSizeCm = 42.0;
constexpr double PadPointZ = 45.0;
constexpr double BayStride = 950.0;
constexpr double BayBaseY = 700.0;
constexpr double StandX = 300.0;
constexpr double StandBack = 120.0;
constexpr double CeilingZ = 520.0;
constexpr int32 PadsPerStation = 6;
/** Degrees from the +Y facing: three choices, withdraw, advance, reset. */
constexpr double PadAngles[PadsPerStation] = {-75.0, -45.0, -15.0, 15.0, 45.0, 75.0};
} // namespace AwNumbers

/** Builds the whole floor and the terminal into Out. False (Out untouched) when the site is not valid. */
bool AwBuildLayout(FAwFloorLayout& Out, const FAwFloorSite& Site, const FAwCast& Cast);

/** The boxes of a pad's raised shape, as (centre offset from the pad centre, size) pairs. Pure. */
void AwShapeBoxes(EAwShape Shape, TArray<TPair<FVector, FVector>>& Out);

// ---- Builders in separate files ----
/** Shell, grid, safe area, arrival and return pads, lights (PerfectWorldAwakeningLayoutShell.cpp). */
void AwBuildShell(FAwFloorLayout& L);
/** The campus terminal (PerfectWorldAwakeningLayoutShell.cpp). */
void AwBuildTerminal(FAwFloorLayout& L);
/** The three stations (PerfectWorldAwakeningLayoutStations.cpp). */
void AwBuildStations(FAwFloorLayout& L);

/** Box helper shared by the builders: local coordinates (world = Site.Origin + local). */
void AwAddPart(FAwFloorLayout& L, const TCHAR* Id, const TCHAR* Asset, EAwColour Colour, EAwRole Role, double X0, double Y0, double Z0, double X1,
               double Y1, double Z1);
/** A box given in world coordinates (the terminal). */
void AwAddWorldPart(FAwFloorLayout& L, const TCHAR* Id, const TCHAR* Asset, EAwColour Colour, EAwRole Role, const FVector& A, const FVector& B);
void AwAddText(FAwFloorLayout& L, const TCHAR* Id, const FString& Text, double X, double Y, double Z, float SizeCm);
/** The slab and raised shape of one pad (visual only; defined in PerfectWorldAwakeningLayoutShell.cpp). */
void AwAddPadVisual(FAwFloorLayout& L, const FAwPadSpec& Pad, EAwColour Plate, EAwColour Shape);
inline FVector AwWorld(const FAwFloorLayout& L, double X, double Y, double Z)
{
    return L.Site.Origin + FVector(X, Y, Z);
}
} // namespace PWAwakening
