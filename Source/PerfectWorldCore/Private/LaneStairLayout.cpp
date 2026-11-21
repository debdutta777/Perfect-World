#include "LaneStairLayout.h"

#include <algorithm>
#include <set>
#include <sstream>

namespace PerfectWorld::LaneStair
{
using LaneInterior::Rect;

namespace
{
constexpr int SlG = GuardThicknessCm;

std::string SlPad(int n) { return (n < 10 ? "0" : "") + std::to_string(n); }

void SlUnit(Climb c, int& du, int& dv)
{
    du = c == Climb::PlusU ? 1 : c == Climb::MinusU ? -1 : 0;
    dv = c == Climb::PlusV ? 1 : c == Climb::MinusV ? -1 : 0;
}

bool SlScaleOk(const StairScale& s)
{
    return s.BodyHeightCm >= 100 && s.EyeHeightCm > 0 && s.EyeHeightCm < s.BodyHeightCm && s.CapsuleRadiusCm > 0 &&
           s.MaxStepUpCm > StepUpMarginCm && s.WalkableSlopePermille > 0 && s.MinRiserMm > 0 && s.MinRiserMm <= s.TargetRiserMm &&
           s.TargetRiserMm <= s.MaxRiserMm && s.MaxRiserMm <= (s.MaxStepUpCm - StepUpMarginCm) * 10 && s.TreadCm >= 20 &&
           s.HeadroomCm >= s.BodyHeightCm + 20 && s.BarrierHeightCm >= 90 && s.MinFlightWidthCm >= 2 * s.CapsuleRadiusCm + 20 &&
           s.MaxFlightWidthCm >= s.MinFlightWidthCm && s.MinLandingCm >= s.MinFlightWidthCm &&
           s.MaxRiserMm * 1000 <= s.TreadCm * 10 * s.WalkableSlopePermille;
}

int SlOverlapArea(const Rect& a, const Rect& b)
{
    const int u = std::min(a.U1, b.U1) - std::max(a.U0, b.U0);
    const int v = std::min(a.V1, b.V1) - std::max(a.V0, b.V0);
    return u > 0 && v > 0 ? u * v : 0;
}

bool SlCovered(const Rect& plan, const std::vector<SlabOpening>& holes)
{
    int area = 0;
    for (const SlabOpening& h : holes) { area += SlOverlapArea(plan, h.Plan); }
    return area == plan.Width() * plan.Depth();
}

/** Along (a, from the foot of the stair in the climb direction) and across (c, from the low side) to a plan rectangle. */
struct SlMap
{
    Rect F;
    Climb Dir = Climb::PlusV;
    [[nodiscard]] bool AlongV() const { return Dir == Climb::PlusV || Dir == Climb::MinusV; }
    [[nodiscard]] int La() const { return AlongV() ? F.Depth() : F.Width(); }
    [[nodiscard]] int Lc() const { return AlongV() ? F.Width() : F.Depth(); }
    [[nodiscard]] Rect R(int a0, int a1, int c0, int c1) const
    {
        switch (Dir)
        {
        case Climb::PlusV: return Rect{F.U0 + c0, F.U0 + c1, F.V0 + a0, F.V0 + a1};
        case Climb::MinusV: return Rect{F.U0 + c0, F.U0 + c1, F.V1 - a1, F.V1 - a0};
        case Climb::PlusU: return Rect{F.U0 + a0, F.U0 + a1, F.V0 + c0, F.V0 + c1};
        default: return Rect{F.U1 - a1, F.U1 - a0, F.V0 + c0, F.V0 + c1};
        }
    }
};

/** Equal risers (at most 1 mm apart) that sum to the rise exactly; empty when no count keeps them within the comfort range. */
std::vector<int> SlLevels(int riseMm, const StairScale& s)
{
    const int lo = (riseMm + s.MaxRiserMm - 1) / s.MaxRiserMm;
    const int hi = riseMm / s.MinRiserMm;
    if (lo < 2 || lo > hi) { return {}; }
    const int n = std::clamp((riseMm + s.TargetRiserMm / 2) / s.TargetRiserMm, lo, hi);
    std::vector<int> levels(1, 0);
    for (int k = 1; k <= n; ++k) { levels.push_back(levels.back() + riseMm / n + (k <= riseMm % n ? 1 : 0)); }
    return levels;
}

/** The rim guards the slab openings need: every opening edge not facing the arrival floor, not shared with another opening and not on a wall. */
std::vector<Rect> SlRims(const StairLayout& L)
{
    std::vector<Rect> out;
    for (size_t i = 0; i < L.Openings.size(); ++i)
    {
        const SlabOpening& o = L.Openings[i];
        for (int side = 0; side < 4; ++side) // 0 U0, 1 U1, 2 V0, 3 V1
        {
            const int nu = side == 0 ? -1 : side == 1 ? 1 : 0;
            const int nv = side == 2 ? -1 : side == 3 ? 1 : 0;
            if (nu == o.ArriveU && nv == o.ArriveV) { continue; }
            const bool alongU = side >= 2; // a V edge runs along U
            const int edge = side == 0 ? o.Plan.U0 : side == 1 ? o.Plan.U1 : side == 2 ? o.Plan.V0 : o.Plan.V1;
            std::vector<std::pair<int, int>> parts{alongU ? std::make_pair(o.Plan.U0, o.Plan.U1) : std::make_pair(o.Plan.V0, o.Plan.V1)};
            for (size_t j = 0; j < L.Openings.size(); ++j)
            {
                const Rect& p = L.Openings[j].Plan;
                const int opposite = side == 0 ? p.U1 : side == 1 ? p.U0 : side == 2 ? p.V1 : p.V0;
                if (j == i || opposite != edge) { continue; }
                const int x0 = alongU ? p.U0 : p.V0;
                const int x1 = alongU ? p.U1 : p.V1;
                std::vector<std::pair<int, int>> next;
                for (const auto& part : parts)
                {
                    if (std::min(part.second, x1) <= std::max(part.first, x0)) { next.push_back(part); continue; }
                    if (x0 > part.first) { next.emplace_back(part.first, x0); }
                    if (x1 < part.second) { next.emplace_back(x1, part.second); }
                }
                parts = next;
            }
            for (const auto& part : parts)
            {
                const Rect r = side == 0   ? Rect{edge - SlG, edge, part.first, part.second}
                               : side == 1 ? Rect{edge, edge + SlG, part.first, part.second}
                               : side == 2 ? Rect{part.first, part.second, edge - SlG, edge}
                                           : Rect{part.first, part.second, edge, edge + SlG};
                if (part.second > part.first && L.Footprint.Contains(r)) { out.push_back(r); }
            }
        }
    }
    return out;
}

struct SlBuild
{
    StairLayout& L;
    SlMap M;
    int Hd = 0, Lim = 0, GuardMm = 0; // headroom, highest surface that still clears the slab, barrier height (mm)
    int Reach = 0; // 2 x capsule radius: the body can be over plan this far behind a tread whose height it already stands at
    bool WallLo = true, WallHi = true, WallEnd = true;

    void AddStep(int flight, int tread, int riser, const Rect& plan)
    {
        const std::string tag = ".f" + std::to_string(flight) + ".t" + SlPad(tread);
        L.Steps.push_back({L.StairId + tag, flight, riser, plan, L.Levels[riser]});
        // A character steps up onto a tread while its centre is still a radius short of the riser, so at that tread's height its
        // body can reach back 2 x radius behind the tread: the headroom volume covers all of it.
        int du = 0, dv = 0;
        SlUnit(flight == 2 ? Opposite() : M.Dir, du, dv);
        const Rect vol{plan.U0 - (du > 0 ? Reach : 0), plan.U1 + (du < 0 ? Reach : 0), plan.V0 - (dv > 0 ? Reach : 0), plan.V1 + (dv < 0 ? Reach : 0)};
        L.Clearances.push_back({L.StairId + ".clear" + tag, ClearKind::Strip, vol, L.Levels[riser], L.Levels[riser + 1] + Hd});
    }
    void GuardBox(const std::string& id, const Rect& plan, int base) { L.Guards.push_back({L.StairId + ".guard." + id, GuardKind::Stringer, plan, base, base + GuardMm}); }
    bool Violates(int riser) const { return L.Levels[riser + 1] > Lim; } // a tread whose far nosing's headroom reaches the slab
    void AddFlight(int index, const Rect& plan, int risers, int from, int to, int width)
    {
        int du = 0, dv = 0;
        SlUnit(index == 2 ? Opposite() : M.Dir, du, dv);
        L.Flights.push_back({L.StairId + ".f" + std::to_string(index), index, plan, risers, du, dv, from, to, width});
    }
    [[nodiscard]] Climb Opposite() const { return static_cast<Climb>(static_cast<int>(M.Dir) ^ 1); } // PlusV/MinusV and PlusU/MinusU are pairs
    void AddLanding(const char* name, LandingKind kind, const Rect& plan, int top, bool solid) { L.Landings.push_back({L.StairId + ".landing." + name, kind, plan, top, solid}); }
    void AddClear(const std::string& id, ClearKind kind, const Rect& plan, int bottom) { L.Clearances.push_back({L.StairId + ".clear." + id, kind, plan, bottom, bottom + Hd}); }
    void AddOpening(const Rect& plan, int au, int av) { L.Openings.push_back({L.StairId + ".opening." + SlPad(static_cast<int>(L.Openings.size()) + 1), plan, au, av}); }
};

Refusal SlBuildStraight(SlBuild& B, int ml, int mh)
{
    StairLayout& L = B.L;
    const StairScale& s = L.Scale;
    const int n = L.RiserCount;
    const int T = s.TreadCm;
    const int ap = std::max(s.MinLandingCm, B.Reach + SlG); // the opening starts a body-reach before the first high tread, with its rim guard
    const int w = B.M.Lc() - ml - mh;
    if (w < s.MinFlightWidthCm) { return Refusal::FootprintTooNarrow; }
    if (w > s.MaxFlightWidthCm) { return Refusal::FootprintTooWide; }
    const int ae = ap + (n - 1) * T;
    if (B.M.La() < ae + s.MinLandingCm) { return Refusal::FootprintTooShort; }
    L.FlightWidthCm = w;
    B.AddFlight(1, B.M.R(ap, ae, ml, ml + w), n, 0, L.RiseMm, w);
    B.AddLanding("bottom", LandingKind::Bottom, B.M.R(0, ap, ml, ml + w), 0, false);
    B.AddLanding("top", LandingKind::Top, B.M.R(ae, B.M.La(), ml, ml + w), L.RiseMm, false);
    B.AddClear("approach", ClearKind::Approach, B.M.R(0, ap, ml, ml + w), 0);
    B.AddClear("top", ClearKind::TopLanding, B.M.R(ae, B.M.La(), ml, ml + w), L.RiseMm);
    int open = 0;
    for (int i = 1; i < n; ++i)
    {
        const Rect tread = B.M.R(ap + (i - 1) * T, ap + i * T, ml, ml + w);
        B.AddStep(1, i, i, tread);
        const std::string tag = "f1.t" + SlPad(i);
        if (!B.WallLo) { B.GuardBox(tag + ".lo", B.M.R(ap + (i - 1) * T, ap + i * T, ml - SlG, ml), L.Levels[i]); }
        if (!B.WallHi) { B.GuardBox(tag + ".hi", B.M.R(ap + (i - 1) * T, ap + i * T, ml + w, ml + w + SlG), L.Levels[i]); }
        if (open == 0 && B.Violates(i)) { open = i; }
    }
    int du = 0, dv = 0;
    SlUnit(B.M.Dir, du, dv);
    B.AddOpening(B.M.R(ap + (open - 1) * T - B.Reach, ae, B.WallLo ? 0 : ml - SlG, B.WallHi ? B.M.Lc() : ml + w + SlG), du, dv);
    return Refusal::None;
}

Refusal SlBuildSwitchback(SlBuild& B, int ml, int mh)
{
    StairLayout& L = B.L;
    const StairScale& s = L.Scale;
    const int n1 = L.RiserCount / 2;
    const int n2 = L.RiserCount - n1;
    const int T = s.TreadCm;
    const int span = B.M.Lc() - ml - mh - SpineCm;
    const int w = span / 2;
    if (w < s.MinFlightWidthCm) { return Refusal::FootprintTooNarrow; }
    if (w > s.MaxFlightWidthCm) { return Refusal::FootprintTooWide; }
    const int me = B.WallEnd ? 0 : 2 * SlG;
    const int ap = std::max(s.MinLandingCm, B.Reach + SlG) + (n2 - n1) * T; // keeps the top landing as deep as the straight approach
    const int lstart = ap + (n1 - 1) * T;
    const int lend = B.M.La() - me;
    if (lend - lstart < std::max({w, s.MinLandingCm, B.Reach})) { return Refusal::FootprintTooShort; }
    const int top2 = lstart - (n2 - 1) * T; // the head of flight 2: the top landing lies in front of it
    const int c1 = ml + w;                  // flight 1 band [ml, c1], spine [c1, c2], flight 2 band [c2, c3]
    const int c2 = c1 + SpineCm;
    const int c3 = B.WallHi ? B.M.Lc() : c2 + w; // against a wall flight 2 takes the spare centimetre
    if (c3 - c2 > s.MaxFlightWidthCm) { return Refusal::FootprintTooWide; }
    const int cs = c1 + SlG;
    const int h1 = L.Levels[n1];
    L.FlightWidthCm = w;
    B.AddFlight(1, B.M.R(ap, lstart, ml, c1), n1, 0, h1, w);
    B.AddFlight(2, B.M.R(top2, lstart, c2, c3), n2, h1, L.RiseMm, c3 - c2);
    B.AddLanding("bottom", LandingKind::Bottom, B.M.R(0, ap, ml, c1), 0, false);
    B.AddLanding("mid", LandingKind::Mid, B.M.R(lstart, lend, ml, c3), h1, true);
    B.AddLanding("top", LandingKind::Top, B.M.R(0, top2, c2, c3), L.RiseMm, false);
    B.AddClear("approach", ClearKind::Approach, B.M.R(0, ap, ml, c1), 0);
    B.AddClear("mid", ClearKind::MidLanding, B.M.R(lstart, lend, ml, c3), h1);
    B.AddClear("top", ClearKind::TopLanding, B.M.R(0, top2, c2, c3), L.RiseMm);
    int first1 = 0, first2 = 0;
    for (int i = 1; i < n1; ++i)
    {
        const int a0 = ap + (i - 1) * T;
        B.AddStep(1, i, i, B.M.R(a0, a0 + T, ml, c1));
        const std::string tag = "f1.t" + SlPad(i);
        if (!B.WallLo) { B.GuardBox(tag + ".lo", B.M.R(a0, a0 + T, ml - SlG, ml), L.Levels[i]); }
        B.GuardBox(tag + ".spine", B.M.R(a0, a0 + T, c1, cs), L.Levels[i]);
        if (first1 == 0 && B.Violates(i)) { first1 = i; }
    }
    for (int j = 1; j < n2; ++j)
    {
        const int a1 = lstart - (j - 1) * T;
        B.AddStep(2, j, n1 + j, B.M.R(a1 - T, a1, c2, c3));
        const std::string tag = "f2.t" + SlPad(j);
        B.GuardBox(tag + ".spine", B.M.R(a1 - T, a1, cs, c2), L.Levels[n1 + j]);
        if (!B.WallHi) { B.GuardBox(tag + ".hi", B.M.R(a1 - T, a1, c3, c3 + SlG), L.Levels[n1 + j]); }
        if (first2 == 0 && B.Violates(n1 + j)) { first2 = j; }
    }
    if (!B.WallLo) { B.GuardBox("landing.lo", B.M.R(lstart, lend, ml - SlG, ml), h1); }
    if (!B.WallHi) { B.GuardBox("landing.hi", B.M.R(lstart, lend, c3, c3 + SlG), h1); }
    if (!B.WallEnd) { B.GuardBox("landing.end", B.M.R(lend, lend + SlG, ml, c3), h1); }
    const int openLo = B.WallLo ? 0 : ml - SlG;
    const int openHi = B.WallHi ? B.M.Lc() : c3 + SlG;
    const int openEnd = B.WallEnd ? lend : lend + SlG;
    int du = 0, dv = 0;
    SlUnit(B.Opposite(), du, dv); // flight 2 arrives going back toward the foot
    if (h1 > B.Lim) // the landing's headroom reaches the slab: open it, and flight 1 from its first violating tread
    {
        B.AddOpening(B.M.R(ap + (first1 - 1) * T - B.Reach, lstart, openLo, cs), 0, 0);
        B.AddOpening(B.M.R(lstart, openEnd, openLo, openHi), 0, 0);
    }
    // Flight 2's headroom volumes reach back over the landing; when the landing itself needs no opening, open just that part.
    const int end3 = lstart - (first2 - 1) * T + B.Reach;
    B.AddOpening(B.M.R(top2, std::min(end3, lstart), cs, openHi), du, dv);
    if (h1 <= B.Lim && end3 > lstart) { B.AddOpening(B.M.R(lstart, end3, cs, openHi), 0, 0); }
    return Refusal::None;
}
} // namespace

StairLayout GenerateStairLayout(const StairInput& in)
{
    StairLayout out;
    const auto refuse = [&](Refusal r) { out = StairLayout{}; out.Status = r; return out; };
    const StairScale& s = in.Scale;
    const int rise = in.UpperFloorCm - in.LowerFloorCm;
    if (in.StairId.empty()) { return refuse(Refusal::InvalidStairId); }
    if (!SlScaleOk(s)) { return refuse(Refusal::ScaleInvalid); }
    if (in.Footprint.Width() <= 0 || in.Footprint.Depth() <= 0) { return refuse(Refusal::FootprintInvalid); }
    if (in.LowerFloorCm < 0 || rise <= 0 || in.UpperSlabCm < 10 || in.UpperSlabCm > 60) { return refuse(Refusal::FloorDatumInvalid); }
    if (rise < MinRiseCm || rise > MaxRiseCm) { return refuse(Refusal::RiseOutOfRange); }
    if (rise - in.UpperSlabCm < s.HeadroomCm) { return refuse(Refusal::HeadroomImpossible); }

    // A mirrored stair is generated unmirrored from the mirror-image request, then reflected.
    Climb dir = in.Direction;
    Enclosure walls = in.Walls;
    if (in.MirrorU)
    {
        std::swap(walls.U0, walls.U1);
        dir = dir == Climb::PlusU ? Climb::MinusU : dir == Climb::MinusU ? Climb::PlusU : dir;
    }
    out.StairId = in.StairId;
    out.Kind = in.Kind;
    out.Direction = dir;
    out.Footprint = in.Footprint;
    out.Walls = walls;
    out.Scale = s;
    out.LowerFloorCm = in.LowerFloorCm;
    out.UpperFloorCm = in.UpperFloorCm;
    out.RiseMm = rise * 10;
    out.SlabUndersideMm = (rise - in.UpperSlabCm) * 10;
    out.Levels = SlLevels(out.RiseMm, s);
    if (out.Levels.empty()) { return refuse(Refusal::NoValidRiser); }
    out.RiserCount = static_cast<int>(out.Levels.size()) - 1;
    out.RiserMmMin = out.Levels[1];
    out.RiserMmMax = out.Levels[1];
    for (int k = 2; k <= out.RiserCount; ++k) { out.RiserMmMin = std::min(out.RiserMmMin, out.Levels[k] - out.Levels[k - 1]); out.RiserMmMax = std::max(out.RiserMmMax, out.Levels[k] - out.Levels[k - 1]); }

    SlBuild b{out};
    b.M.F = in.Footprint;
    b.M.Dir = dir;
    b.Hd = s.HeadroomCm * 10;
    b.Lim = out.SlabUndersideMm - b.Hd;
    b.GuardMm = s.BarrierHeightCm * 10;
    b.Reach = 2 * s.CapsuleRadiusCm;
    const bool v = b.M.AlongV();
    b.WallLo = v ? walls.U0 : walls.V0;
    b.WallHi = v ? walls.U1 : walls.V1;
    b.WallEnd = dir == Climb::PlusV ? walls.V1 : dir == Climb::MinusV ? walls.V0 : dir == Climb::PlusU ? walls.U1 : walls.U0;
    const int ml = b.WallLo ? 0 : 2 * SlG;
    const int mh = b.WallHi ? 0 : 2 * SlG;
    const Refusal r = in.Kind == Arrangement::Straight ? SlBuildStraight(b, ml, mh) : SlBuildSwitchback(b, ml, mh);
    if (r != Refusal::None) { return refuse(r); }
    int rims = 0;
    for (const Rect& rim : SlRims(out)) { out.Guards.push_back({out.StairId + ".rim." + SlPad(++rims), GuardKind::Rim, rim, out.RiseMm, out.RiseMm + b.GuardMm}); }
    if (in.MirrorU) { ReflectStairU(out, in.Footprint.U0 + in.Footprint.U1); }
    return out;
}

namespace
{
struct SlSolid
{
    Rect Plan;
    int Base = 0;
    int Top = 0;
};

/** 0 guarded or walled, 1 no guard, 2 a guard that is too low: the edge of a walked surface at topMm on one side (0 U0, 1 U1, 2 V0, 3 V1). */
int SlEdgeState(const StairLayout& L, const Rect& r, int side, int topMm)
{
    const int edge = side == 0 ? r.U0 : side == 1 ? r.U1 : side == 2 ? r.V0 : r.V1;
    const Rect& f = L.Footprint;
    const bool flush = edge == (side == 0 ? f.U0 : side == 1 ? f.U1 : side == 2 ? f.V0 : f.V1);
    if (flush && (side == 0 ? L.Walls.U0 : side == 1 ? L.Walls.U1 : side == 2 ? L.Walls.V0 : L.Walls.V1)) { return 0; }
    int state = 1;
    for (const Guard& g : L.Guards)
    {
        const Rect& p = g.Plan;
        const bool touches = side == 0   ? p.U1 == edge && p.V0 <= r.V0 && p.V1 >= r.V1
                             : side == 1 ? p.U0 == edge && p.V0 <= r.V0 && p.V1 >= r.V1
                             : side == 2 ? p.V1 == edge && p.U0 <= r.U0 && p.U1 >= r.U1
                                         : p.V0 == edge && p.U0 <= r.U0 && p.U1 >= r.U1;
        if (touches && g.BaseMm <= topMm)
        {
            state = g.TopMm >= topMm + L.Scale.BarrierHeightCm * 10 ? 0 : 2;
            if (state == 0) { break; }
        }
    }
    return state;
}

bool SlTouches(const Rect& a, const Rect& b, int side) // b lies against side of a
{
    return side == 0   ? b.U1 == a.U0 && b.V0 < a.V1 && a.V0 < b.V1
           : side == 1 ? b.U0 == a.U1 && b.V0 < a.V1 && a.V0 < b.V1
           : side == 2 ? b.V1 == a.V0 && b.U0 < a.U1 && a.U0 < b.U1
                       : b.V0 == a.V1 && b.U0 < a.U1 && a.U0 < b.U1;
}
} // namespace

Refusal ValidateStairLayout(const StairLayout& L)
{
    if (!L.Ok()) { return L.Status; }
    const StairScale& s = L.Scale;
    const int n = L.RiserCount;
    if (!SlScaleOk(s)) { return Refusal::ScaleInvalid; }
    if (n < 2 || static_cast<int>(L.Levels.size()) != n + 1 || L.Levels[0] != 0 || L.Flights.empty()) { return Refusal::LayoutInconsistent; }
    if (L.RiseMm != (L.UpperFloorCm - L.LowerFloorCm) * 10 || L.Levels[n] != L.RiseMm) { return Refusal::RisersDoNotLand; }
    int lo = L.RiseMm;
    int hi = 0;
    for (int k = 1; k <= n; ++k)
    {
        const int d = L.Levels[k] - L.Levels[k - 1];
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
    if (hi > s.MaxRiserMm || hi * 1000 > s.TreadCm * 10 * s.WalkableSlopePermille || hi > (s.MaxStepUpCm - StepUpMarginCm) * 10) { return Refusal::RiserTooSteep; }
    if (lo < s.MinRiserMm || hi - lo > 1) { return Refusal::RiserUneven; }
    std::set<std::string> ids;
    std::vector<SlSolid> solids;
    int mids = 0;
    int last = 0;
    for (const Step& st : L.Steps)
    {
        if (st.Riser < 1 || st.Riser >= n || st.Riser <= last || st.TopMm != L.Levels[st.Riser] || st.Flight < 1 || st.Flight > static_cast<int>(L.Flights.size()))
        {
            return Refusal::LayoutInconsistent;
        }
        last = st.Riser;
        if (std::min(st.Plan.Width(), st.Plan.Depth()) < s.TreadCm) { return Refusal::TreadTooShallow; }
        solids.push_back({st.Plan, 0, st.TopMm});
        ids.insert(st.Id);
    }
    for (const Landing& l : L.Landings)
    {
        ids.insert(l.Id);
        if (!l.Solid) { continue; }
        ++mids;
        if (std::find(L.Levels.begin(), L.Levels.end(), l.TopMm) == L.Levels.end()) { return Refusal::LayoutInconsistent; }
        solids.push_back({l.Plan, 0, l.TopMm});
    }
    const size_t massCount = solids.size();
    if (static_cast<int>(L.Steps.size()) + mids != n - 1) { return Refusal::LayoutInconsistent; }
    for (const Flight& f : L.Flights)
    {
        ids.insert(f.Id);
        if (f.WidthCm < s.MinFlightWidthCm) { return Refusal::FlightTooNarrow; }
    }
    for (const SlabOpening& o : L.Openings) { ids.insert(o.Id); }
    for (const Guard& g : L.Guards) { ids.insert(g.Id); }
    for (const Clearance& c : L.Clearances) { ids.insert(c.Id); }
    if (ids.size() != L.Steps.size() + L.Landings.size() + L.Flights.size() + L.Openings.size() + L.Guards.size() + L.Clearances.size()) { return Refusal::DuplicateId; }

    std::vector<Rect> plans;
    for (const SlSolid& x : solids) { plans.push_back(x.Plan); }
    for (const Landing& l : L.Landings) { plans.push_back(l.Plan); }
    for (const SlabOpening& o : L.Openings) { plans.push_back(o.Plan); }
    for (const Guard& g : L.Guards) { plans.push_back(g.Plan); }
    for (const Clearance& c : L.Clearances) { plans.push_back(c.Plan); }
    for (const Rect& r : plans)
    {
        if (r.Width() <= 0 || r.Depth() <= 0 || !L.Footprint.Contains(r)) { return Refusal::GeometryOutsideFootprint; }
    }
    for (size_t i = 0; i < massCount; ++i)
    {
        for (size_t j = i + 1; j < massCount; ++j)
        {
            if (solids[i].Plan.Overlaps(solids[j].Plan)) { return Refusal::MassOverlap; }
        }
    }
    for (const Guard& g : L.Guards) { solids.push_back({g.Plan, g.BaseMm, g.TopMm}); }
    for (const Clearance& c : L.Clearances)
    {
        if (c.Kind == ClearKind::Strip && std::min(c.Plan.Width(), c.Plan.Depth()) < s.TreadCm + 2 * s.CapsuleRadiusCm) { return Refusal::LayoutInconsistent; }
        for (const SlSolid& x : solids)
        {
            if (x.Plan.Overlaps(c.Plan) && x.Base < c.TopMm && x.Top > c.BottomMm) { return Refusal::ClearanceIntruded; }
        }
        if (c.BottomMm < L.RiseMm && c.TopMm > L.SlabUndersideMm && !SlCovered(c.Plan, L.Openings)) { return Refusal::SlabNotOpen; }
    }
    for (const SlSolid& x : solids)
    {
        if (x.Top > L.SlabUndersideMm && x.Base < L.RiseMm && !SlCovered(x.Plan, L.Openings)) { return Refusal::SlabNotOpen; }
    }

    int worst = 0;
    const auto edges = [&](const Rect& r, int top, int s0, int s1)
    {
        for (int side = s0; side <= s1; ++side) { worst = std::max(worst, SlEdgeState(L, r, side, top)); }
    };
    for (const Step& st : L.Steps)
    {
        if (L.Flights[st.Flight - 1].DirU != 0) { edges(st.Plan, st.TopMm, 2, 3); } else { edges(st.Plan, st.TopMm, 0, 1); }
    }
    for (const Landing& l : L.Landings)
    {
        for (int side = 0; l.Solid && side < 4; ++side)
        {
            const bool joined = std::any_of(L.Steps.begin(), L.Steps.end(), [&](const Step& st) { return SlTouches(l.Plan, st.Plan, side); });
            if (!joined) { edges(l.Plan, l.TopMm, side, side); }
        }
    }
    for (const Rect& rim : SlRims(L))
    {
        int state = 1;
        for (const Guard& g : L.Guards)
        {
            if (g.Kind == GuardKind::Rim && g.Plan.Contains(rim) && g.BaseMm <= L.RiseMm)
            {
                state = g.TopMm >= L.RiseMm + s.BarrierHeightCm * 10 ? 0 : 2;
                if (state == 0) { break; }
            }
        }
        worst = std::max(worst, state);
    }
    return worst == 1 ? Refusal::GuardMissing : worst == 2 ? Refusal::GuardTooLow : Refusal::None;
}

std::string DescribeStairLayout(const StairLayout& L)
{
    std::ostringstream o;
    const auto rect = [&](const Rect& r) { o << '[' << r.U0 << ',' << r.U1 << ',' << r.V0 << ',' << r.V1 << ']'; };
    o << "stair " << L.StairId << " status=" << RefusalCode(L.Status) << " kind=" << static_cast<int>(L.Kind) << " dir=" << static_cast<int>(L.Direction)
      << " mirrored=" << L.Mirrored << " rise=" << L.RiseMm << " slab=" << L.SlabUndersideMm << " risers=" << L.RiserCount << " (" << L.RiserMmMin << ".." << L.RiserMmMax
      << ") width=" << L.FlightWidthCm << " footprint=";
    rect(L.Footprint);
    o << '\n';
    for (const Step& x : L.Steps) { o << "step " << x.Id << " r" << x.Riser << " top=" << x.TopMm << ' '; rect(x.Plan); o << '\n'; }
    for (const Landing& x : L.Landings) { o << "landing " << x.Id << " top=" << x.TopMm << " solid=" << x.Solid << ' '; rect(x.Plan); o << '\n'; }
    for (const SlabOpening& x : L.Openings) { o << "opening " << x.Id << " arrive=" << x.ArriveU << ',' << x.ArriveV << ' '; rect(x.Plan); o << '\n'; }
    for (const Guard& x : L.Guards) { o << "guard " << x.Id << ' ' << x.BaseMm << ".." << x.TopMm << ' '; rect(x.Plan); o << '\n'; }
    for (const Clearance& x : L.Clearances) { o << "clear " << x.Id << ' ' << x.BottomMm << ".." << x.TopMm << ' '; rect(x.Plan); o << '\n'; }
    return o.str();
}
} // namespace PerfectWorld::LaneStair
