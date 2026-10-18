// UP-05: the stair contract (LaneStairLayout.h). Covers the shared-entry passage, a switchback, all four climb directions,
// mirrored arrangements, the supported rise extremes (every whole centimetre from 250 to 450), risers that land exactly,
// refusal codes at the exact footprint limits, guards and slab openings, scale inputs, determinism, and the validator
// catching each kind of damaged layout.

#include "LaneStairLayout.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace PerfectWorld::LaneStair;
using PerfectWorld::LaneInterior::Rect;

namespace
{
int Assertions = 0;
void Expect(bool condition, const char* message)
{
    ++Assertions;
    if (condition) return;
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
}

/** A flight in the shared entry's 130 cm passage: 650 cm of interior depth behind a 25 cm front wall, rise 300. */
StairInput PassageInput()
{
    StairInput in;
    in.StairId = "stair.test.passage";
    in.Footprint = Rect{285, 415, 25, 675};
    return in;
}

StairInput SwitchbackInput()
{
    StairInput in;
    in.StairId = "stair.test.switchback";
    in.Kind = Arrangement::Switchback;
    in.Footprint = Rect{0, 212, 0, 450};
    return in;
}

StairInput WithRise(StairInput in, int riseCm)
{
    in.UpperFloorCm = in.LowerFloorCm + riseCm;
    return in;
}

Rect R(int u0, int u1, int v0, int v1) { return Rect{u0, u1, v0, v1}; }

Refusal Code(const StairInput& in) { return GenerateStairLayout(in).Status; }

void ExpectGood(const StairLayout& l, const char* what)
{
    Expect(l.Ok(), what);
    Expect(ValidateStairLayout(l) == Refusal::None, what);
    Expect(l.Levels.size() == static_cast<size_t>(l.RiserCount) + 1 && l.Levels.front() == 0 && l.Levels.back() == l.RiseMm, what);
    Expect(l.RiserMmMax - l.RiserMmMin <= 1 && l.RiserMmMin >= l.Scale.MinRiserMm && l.RiserMmMax <= l.Scale.MaxRiserMm, what);
}

void TestPassageStraight()
{
    const StairLayout l = GenerateStairLayout(PassageInput());
    ExpectGood(l, "passage stair");
    Expect(l.RiseMm == 3000 && l.RiserCount == 17 && l.RiserMmMin == 176 && l.RiserMmMax == 177, "17 risers of 176 / 177 mm land on 3000 exactly");
    Expect(l.Steps.size() == 16 && l.Flights.size() == 1 && l.Landings.size() == 2 && l.Openings.size() == 1, "16 treads, one flight, two landings, one opening");
    Expect(l.FlightWidthCm == 130 && l.Flights[0].Risers == 17 && l.Flights[0].FromMm == 0 && l.Flights[0].ToMm == 3000, "flight record");
    Expect(l.Steps[0].Id == "stair.test.passage.f1.t01" && l.Steps[15].Id == "stair.test.passage.f1.t16", "stable step ids");
    Expect(l.Steps[0].Plan == R(285, 415, 125, 150) && l.Steps[15].Plan == R(285, 415, 500, 525), "25 cm treads start 100 cm behind the foot");
    Expect(l.Steps[15].TopMm == l.Levels[16] && 3000 - l.Steps[15].TopMm == 176, "the last riser lands on the floor");
    Expect(l.Landings[0].Kind == LandingKind::Bottom && l.Landings[0].Plan == R(285, 415, 25, 125), "100 cm approach");
    Expect(l.Landings[1].Kind == LandingKind::Top && l.Landings[1].TopMm == 3000 && l.Landings[1].Plan == R(285, 415, 525, 675), "150 cm top landing");
    // Slab underside 2740, headroom 2200: a tread whose far nosing is above 540 mm needs the slab opened; the 4th level (708) is the first.
    // The headroom volume reaches back 68 cm (two capsule radii) behind each tread, so the opening starts 68 cm before the 3rd tread.
    Expect(l.SlabUndersideMm == 2740 && l.Openings[0].Plan == R(285, 415, 175 - 68, 525), "the opening starts 68 cm before the 3rd tread and ends at the last riser");
    Expect(l.Clearances[2].Plan == R(285, 415, 125 - 68, 150), "a tread's headroom volume reaches back over the treads behind it");
    Expect(l.Openings[0].ArriveU == 0 && l.Openings[0].ArriveV == 1, "arrival at the head of the flight");
    Expect(l.Guards.size() == 1 && l.Guards[0].Kind == GuardKind::Rim && l.Guards[0].Plan == R(285, 415, 169 - 68, 175 - 68) &&
               l.Guards[0].BaseMm == 3000 && l.Guards[0].TopMm == 4000,
           "walled passage: one 100 cm rim guard behind the opening, none at the arrival edge");
    Expect(l.Clearances.size() == 18 && l.Clearances[0].Kind == ClearKind::Approach && l.Clearances[1].Kind == ClearKind::TopLanding && l.Clearances[2].Kind == ClearKind::Strip &&
               l.Clearances[2].BottomMm == l.Levels[1] && l.Clearances[2].TopMm == l.Levels[2] + 2200,
           "a headroom volume per tread, plus approach and top landing");
    Expect(DescribeStairLayout(l) == DescribeStairLayout(GenerateStairLayout(PassageInput())), "deterministic");
    Expect(!DescribeStairLayout(l).empty() && RefusalCode(Refusal::None) == "ok", "describe and code");
}

void TestSwitchback()
{
    const StairLayout l = GenerateStairLayout(SwitchbackInput());
    ExpectGood(l, "switchback");
    Expect(l.RiserCount == 17 && l.Flights.size() == 2 && l.Flights[0].Risers == 8 && l.Flights[1].Risers == 9, "8 + 9 risers");
    Expect(l.Steps.size() == 15 && l.FlightWidthCm == 100, "7 + 8 treads, 100 cm flights");
    const Landing* mid = nullptr;
    const Landing* top = nullptr;
    for (const Landing& x : l.Landings) { if (x.Kind == LandingKind::Mid) mid = &x; if (x.Kind == LandingKind::Top) top = &x; }
    Expect(mid != nullptr && mid->Solid && mid->TopMm == l.Levels[8] && l.Flights[0].ToMm == mid->TopMm && l.Flights[1].FromMm == mid->TopMm, "the mid landing is exactly 8 risers up");
    Expect(top != nullptr && top->Plan.Depth() == 100 && top->TopMm == 3000, "the top landing is 100 cm deep beside flight 2");
    Expect(l.Steps[7].Plan.U0 > l.Steps[0].Plan.U1 - 1 && l.Steps[7].Riser == 9, "flight 2 lies beside flight 1");
    Expect(l.Steps[6].Plan.V0 + 25 == mid->Plan.V0 && l.Steps[7].Plan.V1 == mid->Plan.V0, "both flights meet the landing's near edge");
    Expect(l.Openings.size() == 3, "landing above the slab line: three openings");
    Expect(l.Openings[2].ArriveV == -1 && l.Openings[2].Plan.V0 == top->Plan.V1, "flight 2 arrives toward the foot");
    int spine = 0;
    for (const Guard& g : l.Guards) { if (g.Id.find(".spine") != std::string::npos) ++spine; }
    Expect(spine == 15, "a spine guard beside every tread");
    // Flight 2's first tread stands one riser above the landing, not above flight 1's last tread.
    Expect(l.Steps[7].TopMm == l.Levels[9] && l.Levels[9] - mid->TopMm <= 177, "first riser of flight 2");
}

void TestDirectionsAndMirror()
{
    const Rect vFoot{50, 180, 40, 740};     // straight, climbing along V
    const Rect uFoot{40, 740, 50, 180};     // straight, climbing along U
    const Rect vSwitch{50, 274, 40, 540};   // switchback along V
    const Rect uSwitch{40, 540, 50, 274};
    for (const Climb c : {Climb::PlusV, Climb::MinusV, Climb::PlusU, Climb::MinusU})
    {
        const bool alongV = c == Climb::PlusV || c == Climb::MinusV;
        for (const Arrangement k : {Arrangement::Straight, Arrangement::Switchback})
        {
            StairInput in = k == Arrangement::Straight ? PassageInput() : SwitchbackInput();
            in.Direction = c;
            in.Footprint = k == Arrangement::Straight ? (alongV ? vFoot : uFoot) : (alongV ? vSwitch : uSwitch);
            const StairLayout l = GenerateStairLayout(in);
            ExpectGood(l, "every climb direction");
            const Step& a = l.Steps[0];
            const Step& b = l.Steps[k == Arrangement::Straight ? 5 : 3];
            const int da = alongV ? (b.Plan.V0 - a.Plan.V0) : (b.Plan.U0 - a.Plan.U0);
            const bool plus = c == Climb::PlusV || c == Climb::PlusU;
            Expect(plus ? da > 0 : da < 0, "treads climb in the requested direction");
            Expect(l.Flights[0].DirU == (c == Climb::PlusU ? 1 : c == Climb::MinusU ? -1 : 0) && l.Flights[0].DirV == (c == Climb::PlusV ? 1 : c == Climb::MinusV ? -1 : 0), "flight direction");
            for (const bool mirror : {false, true})
            {
                for (const bool wallLeft : {true, false})
                {
                    StairInput m = in;
                    m.Walls.U0 = wallLeft;
                    m.MirrorU = mirror;
                    const StairLayout ml = GenerateStairLayout(m);
                    ExpectGood(ml, "mirrored stairs validate");
                    // The mirrored stair is the reflection of the stair for the reflected walls and direction.
                    StairInput plain = m;
                    plain.MirrorU = false;
                    if (mirror)
                    {
                        plain.Walls.U0 = m.Walls.U1;
                        plain.Walls.U1 = m.Walls.U0;
                        plain.Direction = c == Climb::PlusU ? Climb::MinusU : c == Climb::MinusU ? Climb::PlusU : c;
                    }
                    StairLayout ref = GenerateStairLayout(plain);
                    if (mirror)
                    {
                        ReflectStairU(ref, in.Footprint.U0 + in.Footprint.U1);
                        Expect(ml.Mirrored && DescribeStairLayout(ml) == DescribeStairLayout(ref), "MirrorU is the exact reflection");
                    }
                    Expect(ml.Steps.size() == ref.Steps.size() && ml.Steps[0].Id == ref.Steps[0].Id, "ids do not change under mirroring");
                }
            }
        }
    }
    // Switchback, V climb: unmirrored puts flight 1 on the low-U side, mirrored on the high-U side.
    StairInput s = SwitchbackInput();
    const StairLayout plain = GenerateStairLayout(s);
    s.MirrorU = true;
    const StairLayout mirrored = GenerateStairLayout(s);
    Expect(plain.Steps[0].Plan.U0 == 0 && mirrored.Steps[0].Plan.U1 == 212 && plain.Steps[0].Plan.V0 == mirrored.Steps[0].Plan.V0, "handedness flips, depth does not");
    // A whole-building reflection across the frontage (as ReflectLayoutU does) keeps ids and heights and is its own inverse.
    StairLayout whole = plain;
    ReflectStairU(whole, 700);
    Expect(whole.Steps[0].Plan == R(600, 700, plain.Steps[0].Plan.V0, plain.Steps[0].Plan.V1) && whole.Steps[0].TopMm == plain.Steps[0].TopMm && whole.Levels == plain.Levels, "frontage reflection");
    ReflectStairU(whole, 700);
    Expect(DescribeStairLayout(whole) == DescribeStairLayout(plain), "reflecting twice restores the layout");
    // A shifted footprint shifts the stair unchanged.
    StairInput moved = PassageInput();
    moved.Footprint = Rect{1285, 1415, 325, 975};
    const StairLayout m = GenerateStairLayout(moved);
    const StairLayout o = GenerateStairLayout(PassageInput());
    Expect(m.Steps[3].Plan == R(1285, 1415, 500, 525) && m.Levels == o.Levels && m.Openings[0].Plan.V0 - o.Openings[0].Plan.V0 == 300, "translation invariance");
}

/** The walking body as a square of its radius at every 3 cm of the footprint: it stands at the highest solid under it, and wherever
 *  slab lies over any part of it the gap between its head and the slab must be the headroom less the body. Squares touching a
 *  stringer guard or the top landing are not places to stand on the flight. */
bool BodyClearsSlab(const StairLayout& l, int radius)
{
    const int body = l.Scale.BodyHeightCm * 10;
    const int need = (l.Scale.HeadroomCm - l.Scale.BodyHeightCm) * 10;
    const Rect& f = l.Footprint;
    for (int u = f.U0 + radius; u <= f.U1 - radius; u += 3)
    {
        for (int v = f.V0 + radius; v <= f.V1 - radius; v += 3)
        {
            const Rect sq{u - radius, u + radius, v - radius, v + radius};
            bool skip = false;
            int feet = 0;
            int holes = 0;
            for (const Guard& g : l.Guards) { skip = skip || (g.Kind == GuardKind::Stringer && g.Plan.Overlaps(sq)); }
            for (const Landing& x : l.Landings) { skip = skip || (x.Kind == LandingKind::Top && x.Plan.Overlaps(sq)); }
            for (const Step& st : l.Steps) { if (st.Plan.Overlaps(sq)) feet = std::max(feet, st.TopMm); }
            for (const Landing& x : l.Landings) { if (x.Solid && x.Plan.Overlaps(sq)) feet = std::max(feet, x.TopMm); }
            for (const SlabOpening& o : l.Openings)
            {
                const int du = std::min(o.Plan.U1, sq.U1) - std::max(o.Plan.U0, sq.U0);
                const int dv = std::min(o.Plan.V1, sq.V1) - std::max(o.Plan.V0, sq.V0);
                if (du > 0 && dv > 0) holes += du * dv;
            }
            if (!skip && holes < sq.Width() * sq.Depth() && l.SlabUndersideMm - (feet + body) < need) return false;
        }
    }
    return true;
}

void TestBodyHeadroom()
{
    std::vector<StairInput> inputs;
    for (const int rise : {250, 300, 360, 450})
    {
        StairInput a = WithRise(PassageInput(), rise);
        a.Footprint = Rect{0, 130, 0, 1000};
        StairInput b = WithRise(SwitchbackInput(), rise);
        b.Footprint = Rect{0, 230, 0, 1000};
        StairInput c = b;
        c.MirrorU = true;
        c.Walls = Enclosure{false, true, true, false};
        c.Footprint = Rect{0, 234, 0, 1010};
        StairInput d = a;
        d.Walls = Enclosure{false, false, false, false};
        for (const StairInput& in : {a, b, c, d}) inputs.push_back(in);
    }
    StairInput alongU = SwitchbackInput();
    alongU.Direction = Climb::MinusU;
    alongU.Footprint = Rect{0, 500, 0, 230};
    inputs.push_back(alongU);
    StairInput fat = PassageInput();
    fat.Scale.CapsuleRadiusCm = 40;
    fat.Footprint = Rect{0, 150, 0, 700};
    inputs.push_back(fat);
    StairInput spill = WithRise(SwitchbackInput(), 450); // a low headroom requirement leaves the landing under the slab but not flight 2's reach over it
    spill.Scale.HeadroomCm = 185;
    spill.Footprint = Rect{0, 230, 0, 1000};
    inputs.push_back(spill);
    for (const StairInput& in : inputs)
    {
        const StairLayout l = GenerateStairLayout(in);
        ExpectGood(l, "body headroom fixtures");
        Expect(BodyClearsSlab(l, l.Scale.CapsuleRadiusCm), "a walking body keeps headroom less its height under the slab everywhere");
    }
    const StairLayout spilled = GenerateStairLayout(inputs.back());
    Expect(spilled.Openings.size() == 2 && spilled.Openings[1].Plan.V0 == spilled.Openings[0].Plan.V1 && spilled.Openings[1].Plan.Depth() == 68, "spill case: flight 2 plus a partial landing opening");
    // The check can fail: a stair derived for a 1 cm body is not safe for the 34 cm body.
    StairInput thin = PassageInput();
    thin.Scale.CapsuleRadiusCm = 1;
    const StairLayout thinLayout = GenerateStairLayout(thin);
    ExpectGood(thinLayout, "thin body stair");
    Expect(!BodyClearsSlab(thinLayout, 34), "a stair built only for the tread itself leaves the body's head under the slab");
}

void TestExtremes()
{
    for (int rise = MinRiseCm; rise <= MaxRiseCm; ++rise)
    {
        StairInput straight = WithRise(PassageInput(), rise);
        straight.Footprint = Rect{0, 130, 0, 1000};
        StairInput back = WithRise(SwitchbackInput(), rise);
        back.Footprint = Rect{0, 230, 0, 1000};
        for (const StairInput& in : {straight, back})
        {
            const StairLayout l = GenerateStairLayout(in);
            ExpectGood(l, "every supported rise");
            Expect(l.RiseMm == rise * 10 && l.Levels.back() == rise * 10, "risers land on the upper floor exactly");
            Expect(l.RiserCount >= (rise * 10 + 189) / 190 && l.RiserCount <= rise * 10 / 150, "riser count within the comfort range");
            Expect(l.Steps.back().TopMm + l.RiserMmMax >= l.RiseMm && l.Steps.back().TopMm < l.RiseMm, "the last tread is one riser under the floor");
        }
    }
    const StairLayout low = GenerateStairLayout(WithRise(PassageInput(), 250));
    StairInput tallPassage = WithRise(PassageInput(), 450);
    tallPassage.Footprint = Rect{285, 415, 25, 925};
    const StairLayout high = GenerateStairLayout(tallPassage);
    Expect(low.RiserCount == 14 && high.RiserCount == 26, "250 cm takes 14 risers, 450 cm takes 26");
    Expect(Code(WithRise(PassageInput(), 249)) == Refusal::RiseOutOfRange && Code(WithRise(PassageInput(), 451)) == Refusal::RiseOutOfRange, "outside the supported range");
    StairInput thick = WithRise(PassageInput(), 250);
    thick.UpperSlabCm = 40;
    Expect(Code(thick) == Refusal::HeadroomImpossible, "a slab too thick for the storey leaves no headroom");
    StairInput older = WithRise(PassageInput(), 360); // an older house storey
    older.Footprint = Rect{285, 415, 25, 775};
    ExpectGood(GenerateStairLayout(older), "360 cm storey");
}

void TestRefusals()
{
    StairInput in = PassageInput();
    in.StairId.clear();
    Expect(Code(in) == Refusal::InvalidStairId, "empty id");
    in = PassageInput();
    in.Scale.TreadCm = 0;
    Expect(Code(in) == Refusal::ScaleInvalid, "bad scale");
    in = PassageInput();
    in.Scale.MaxStepUpCm = 20; // a 19 cm riser would no longer be climbable with the margin
    Expect(Code(in) == Refusal::ScaleInvalid, "a step-up height below the largest riser");
    in = PassageInput();
    in.Footprint = Rect{0, 0, 0, 100};
    Expect(Code(in) == Refusal::FootprintInvalid, "empty footprint");
    in = PassageInput();
    in.UpperFloorCm = in.LowerFloorCm;
    Expect(Code(in) == Refusal::FloorDatumInvalid, "upper floor not above the lower");
    in = PassageInput();
    in.Scale.MinRiserMm = 170;
    in.Scale.MaxRiserMm = 175;
    in.UpperFloorCm = in.LowerFloorCm + 250;
    Expect(Code(in) == Refusal::NoValidRiser, "no whole number of risers fits a narrow comfort band");
    // Exact footprint limits.
    in = PassageInput();
    in.Footprint = Rect{0, 99, 25, 675};
    Expect(Code(in) == Refusal::FootprintTooNarrow, "99 cm wide");
    in.Footprint = Rect{0, 100, 25, 675};
    Expect(Code(in) == Refusal::None, "100 cm wide");
    in.Footprint = Rect{0, 160, 25, 675};
    Expect(Code(in) == Refusal::None, "160 cm wide");
    in.Footprint = Rect{0, 161, 25, 675};
    Expect(Code(in) == Refusal::FootprintTooWide, "161 cm wide");
    in.Footprint = Rect{0, 130, 0, 599};
    Expect(Code(in) == Refusal::FootprintTooShort, "599 cm deep");
    in.Footprint = Rect{0, 130, 0, 600};
    Expect(Code(in) == Refusal::None, "600 cm deep");
    StairInput sw = SwitchbackInput();
    sw.Footprint = Rect{0, 211, 0, 450};
    Expect(Code(sw) == Refusal::FootprintTooNarrow, "switchback 211 wide");
    sw.Footprint = Rect{0, 332, 0, 460};
    Expect(Code(sw) == Refusal::None, "switchback 332 wide");
    sw.Footprint = Rect{0, 333, 0, 460};
    Expect(Code(sw) == Refusal::FootprintTooWide, "switchback 333 wide: flight 2 would be 161");
    sw.Footprint = Rect{0, 212, 0, 399};
    Expect(Code(sw) == Refusal::FootprintTooShort, "switchback 399 deep");
    sw.Footprint = Rect{0, 212, 0, 400};
    Expect(Code(sw) == Refusal::None, "switchback 400 deep");
    // A refusal is an otherwise empty layout.
    in.Footprint = Rect{0, 99, 0, 700};
    const StairLayout empty = GenerateStairLayout(in);
    Expect(!empty.Ok() && empty.Steps.empty() && empty.Guards.empty() && ValidateStairLayout(empty) == Refusal::FootprintTooNarrow, "refusals carry nothing");
}

void TestGuardsAndSlab()
{
    StairInput open = PassageInput();
    open.Walls = Enclosure{false, false, false, false};
    open.Footprint = Rect{285, 415, 25, 675};
    const StairLayout l = GenerateStairLayout(open); // 130 - 24 = 106 cm between the stringers
    ExpectGood(l, "unwalled passage");
    Expect(l.FlightWidthCm == 106, "the guard margins come out of the footprint");
    int lo = 0;
    int hi = 0;
    int rim = 0;
    for (const Guard& g : l.Guards)
    {
        if (g.Id.find(".lo") != std::string::npos) ++lo;
        if (g.Id.find(".hi") != std::string::npos) ++hi;
        if (g.Kind == GuardKind::Rim) ++rim;
        Expect(g.TopMm - g.BaseMm == 1000 && g.Plan.Width() > 0 && g.Plan.Depth() > 0, "100 cm guards");
    }
    Expect(lo == 16 && hi == 16 && rim == 3, "a stringer each side of every tread; rim guards behind and either side of the opening");
    for (const Step& s : l.Steps) { Expect(s.Plan.U0 == 297 && s.Plan.U1 == 403, "flight clear of the stringers"); }
    // Switchback unwalled on a side and the end: landing guards appear.
    StairInput back = SwitchbackInput();
    back.Walls = Enclosure{false, true, true, false};
    back.Footprint = Rect{0, 224, 0, 462};
    const StairLayout b = GenerateStairLayout(back);
    ExpectGood(b, "partly walled switchback");
    int landing = 0;
    for (const Guard& g : b.Guards) { if (g.Id.find("landing.") != std::string::npos) ++landing; }
    Expect(landing == 2, "landing guards on the open side and the open end");
    // Slab opening arithmetic with a taller headroom requirement: the opening grows.
    StairInput tall = PassageInput();
    tall.Scale.HeadroomCm = 240;
    tall.Scale.BodyHeightCm = 170;
    const StairLayout t = GenerateStairLayout(tall);
    ExpectGood(t, "taller headroom input");
    Expect(t.Openings[0].Plan.V0 < GenerateStairLayout(PassageInput()).Openings[0].Plan.V0, "more headroom opens the slab earlier");
    // A lower storey where every tread needs the opening still validates.
    const StairLayout shortStorey = GenerateStairLayout(WithRise(PassageInput(), 250));
    Expect(shortStorey.Openings[0].Plan.V0 == 25 + 100 - 68, "the opening starts at the first tread in a 250 cm storey");
}

void TestDamage()
{
    const StairLayout base = GenerateStairLayout(PassageInput());
    StairInput open = PassageInput();
    open.Walls = Enclosure{false, false, false, false};
    const StairLayout unwalled = GenerateStairLayout(open);
    const auto damaged = [&](const StairLayout& from, auto&& edit, Refusal want, const char* what)
    {
        StairLayout l = from;
        edit(l);
        Expect(ValidateStairLayout(l) == want, what);
    };
    damaged(base, [](StairLayout& l) { l.Steps[3].TopMm += 5; }, Refusal::LayoutInconsistent, "a tread that is not at its level");
    damaged(base, [](StairLayout& l) { l.Levels.back() -= 10; }, Refusal::RisersDoNotLand, "risers that stop short of the floor");
    damaged(base, [](StairLayout& l) { l.Levels[3] += 4; }, Refusal::RiserUneven, "one riser 4 mm taller");
    damaged(base, [](StairLayout& l) { l.Scale.TargetRiserMm = 160; l.Scale.MaxRiserMm = 170; }, Refusal::RiserTooSteep, "risers above the comfort maximum");
    damaged(base, [](StairLayout& l) { l.Steps[0].Plan.V1 -= 5; }, Refusal::TreadTooShallow, "a 20 cm tread");
    damaged(base, [](StairLayout& l) { l.Flights[0].WidthCm = 90; }, Refusal::FlightTooNarrow, "a 90 cm flight");
    damaged(base, [](StairLayout& l) { l.Steps[2].Plan.U1 += 500; }, Refusal::GeometryOutsideFootprint, "a tread outside the footprint");
    damaged(base, [](StairLayout& l) { l.Steps[1].Plan = l.Steps[0].Plan; }, Refusal::MassOverlap, "two treads in one place");
    damaged(base, [](StairLayout& l) { l.Steps[1].Id = l.Steps[0].Id; }, Refusal::DuplicateId, "duplicate ids");
    damaged(base, [](StairLayout& l) { l.Steps.pop_back(); }, Refusal::LayoutInconsistent, "a missing tread");
    damaged(base, [](StairLayout& l)
            {
                Guard g{"stair.test.passage.guard.x", GuardKind::Stringer, l.Steps[3].Plan, l.Steps[3].TopMm + 500, l.Steps[3].TopMm + 1500};
                l.Guards.push_back(g);
            },
            Refusal::ClearanceIntruded, "something in the headroom volume");
    damaged(base, [](StairLayout& l) { l.Clearances[2].Plan.V0 += 68; }, Refusal::LayoutInconsistent, "a headroom volume that forgets the body's reach");
    damaged(base, [](StairLayout& l) { l.Openings[0].Plan.V1 -= 25; }, Refusal::SlabNotOpen, "slab left over the last tread");
    damaged(base, [](StairLayout& l) { l.Guards.clear(); }, Refusal::GuardMissing, "no guard behind the opening");
    damaged(base, [](StairLayout& l) { l.Guards[0].TopMm = l.RiseMm + 500; }, Refusal::GuardTooLow, "a 50 cm rim guard");
    damaged(unwalled, [](StairLayout& l) { l.Guards.erase(l.Guards.begin() + 5); }, Refusal::GuardMissing, "a tread side without its stringer");
    damaged(unwalled, [](StairLayout& l) { l.Guards[4].TopMm = l.Guards[4].BaseMm + 600; }, Refusal::GuardTooLow, "a 60 cm stringer");
    StairLayout refused;
    refused.Status = Refusal::FootprintTooShort;
    Expect(ValidateStairLayout(refused) == Refusal::FootprintTooShort, "validating a refusal returns it");
    // Independent derivations from the finished layout.
    for (const StairLayout* l : {&base, &unwalled})
    {
        for (size_t i = 1; i < l->Steps.size(); ++i) { Expect(l->Steps[i].TopMm - l->Steps[i - 1].TopMm <= 177, "consecutive treads differ by one riser"); }
        Expect(l->Steps[0].TopMm <= 177 && l->RiseMm - l->Steps.back().TopMm <= 177, "first and last riser");
        Expect((l->Scale.MaxStepUpCm - 5) * 10 >= l->RiserMmMax, "every riser is under the character's step-up height");
        for (const Step& s : l->Steps) { Expect(s.TopMm > 0 && s.TopMm < l->RiseMm, "treads sit between the floors"); }
        for (const Clearance& c : l->Clearances)
        {
            if (c.Kind == ClearKind::Strip) { Expect(c.TopMm - c.BottomMm >= 2200 + l->Scale.MinRiserMm && c.TopMm - c.BottomMm <= 2200 + l->Scale.MaxRiserMm, "headroom is 220 cm above the far nosing"); }
        }
    }
}

/** No footprint, wall combination or direction yields a layout that the validator rejects: a stair is either refused or safe. */
void TestEveryFootprintIsRefusedOrSafe()
{
    int made = 0;
    int refused = 0;
    for (const int rise : {250, 300, 450})
    {
        for (const Arrangement k : {Arrangement::Straight, Arrangement::Switchback})
        {
            for (const Climb c : {Climb::PlusV, Climb::MinusV, Climb::PlusU, Climb::MinusU})
            {
                for (int walls = 0; walls < 16; ++walls)
                {
                    for (int w = 90; w <= 380; w += 11)
                    {
                        for (int d = 300; d <= 1100; d += 37)
                        {
                            StairInput in = WithRise(PassageInput(), rise);
                            in.Kind = k;
                            in.Direction = c;
                            in.MirrorU = (walls & 1) != 0;
                            in.Walls = Enclosure{(walls & 1) != 0, (walls & 2) != 0, (walls & 4) != 0, (walls & 8) != 0};
                            in.Footprint = (c == Climb::PlusV || c == Climb::MinusV) ? Rect{-40, -40 + w, 30, 30 + d} : Rect{-40, -40 + d, 30, 30 + w};
                            const StairLayout l = GenerateStairLayout(in);
                            if (l.Ok()) { ++made; Expect(ValidateStairLayout(l) == Refusal::None, "a generated stair always validates"); }
                            else { ++refused; Expect(l.Steps.empty() && l.Status != Refusal::None, "refusals are empty"); }
                        }
                    }
                }
            }
        }
    }
    Expect(made > 1000 && refused > 1000, "the sweep covers both fitting and refused footprints");
}
} // namespace

int main()
{
    TestPassageStraight();
    TestSwitchback();
    TestDirectionsAndMirror();
    TestBodyHeadroom();
    TestExtremes();
    TestRefusals();
    TestGuardsAndSlab();
    TestDamage();
    TestEveryFootprintIsRefusedOrSafe();
    std::cout << "LaneStairLayoutTests: " << Assertions << " assertions passed\n";
    return 0;
}
