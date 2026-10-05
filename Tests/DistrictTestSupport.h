#pragma once

// Measuring helpers for DistrictGeneratorTests.cpp, kept apart so each file stays under 500 lines.
// Include once, from that test only: everything is file-private to the including translation unit.

#include "DistrictGenerator.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <vector>

/** Defined by the including test: counts a check, and exits with a message when it fails. */
void Check(bool condition, const char* message);

namespace
{
using namespace PerfectWorld;

constexpr int N = DistrictGridEdgeCells;
constexpr int SeedCount = 200;

std::uint64_t SeedOf(int i)
{
    return static_cast<std::uint64_t>(i) * 40503ULL + 17ULL;
}

bool Is(const GeneratedDistrict& d, int x, int y, DistrictCellKind kind)
{
    return x >= 0 && y >= 0 && x < N && y < N && d.At(x, y).Kind == kind;
}

bool Same(const GeneratedDistrict& a, const GeneratedDistrict& b)
{
    for (int i = 0; i < DistrictCellCount; ++i)
    {
        if (a.Cells[static_cast<std::size_t>(i)].Kind != b.Cells[static_cast<std::size_t>(i)].Kind ||
            a.Cells[static_cast<std::size_t>(i)].Storeys != b.Cells[static_cast<std::size_t>(i)].Storeys)
        {
            return false;
        }
    }
    return true;
}

/** Size of the 4-connected component of `kind` cells containing (x, y); fills `seen`. */
int Flood(const GeneratedDistrict& d, int x, int y, DistrictCellKind kind, std::vector<char>& seen, int* minX = nullptr, int* maxX = nullptr,
          int* minY = nullptr, int* maxY = nullptr)
{
    int size = 0;
    std::vector<int> stack{y * N + x};
    seen[static_cast<std::size_t>(y * N + x)] = 1;
    while (!stack.empty())
    {
        const int at = stack.back();
        stack.pop_back();
        ++size;
        const int cx = at % N;
        const int cy = at / N;
        if (minX != nullptr)
        {
            *minX = std::min(*minX, cx);
            *maxX = std::max(*maxX, cx);
            *minY = std::min(*minY, cy);
            *maxY = std::max(*maxY, cy);
        }
        const int nx[4] = {cx - 1, cx + 1, cx, cx};
        const int ny[4] = {cy, cy, cy - 1, cy + 1};
        for (int k = 0; k < 4; ++k)
        {
            if (Is(d, nx[k], ny[k], kind) && seen[static_cast<std::size_t>(ny[k] * N + nx[k])] == 0)
            {
                seen[static_cast<std::size_t>(ny[k] * N + nx[k])] = 1;
                stack.push_back(ny[k] * N + nx[k]);
            }
        }
    }
    return size;
}

struct Measure
{
    int Streets = 0;
    int Buildings = 0;
    int Open = 0;
    int MinStoreys = 255;
    int MaxStoreys = 0;
    int FullStreetRows = 0;
    int FullStreetColumns = 0;
    int LargestOpen = 0;
    int OpenCentreX2 = 0; // twice the centre of the largest open area's bounding box
    int OpenCentreY2 = 0;
    int Water = 0;
    int Rail = 0;
    int DeadEnds = 0;           // street cells, not on the grid border, with exactly one street neighbour
    int PierTips = 0;           // dead ends that touch water
    int LongestRun = 0;         // longest straight run of building cells, either way
    int StreetsByWater = 0;     // street cells with a water neighbour (promenade, quay, piers, bridges)
    int OpenByWater = 0;        // open cells with a water neighbour (ghat steps)
    int Bridges = 0;            // street cells with water on two opposite sides
    int Crossings = 0;          // street cells with rail on two opposite sides
    int RailLines = 0;          // rows and columns that hold at least 40 rail cells
    int BoulevardLines = 0;     // rows and columns that start a run of three full-street lines
    int WaterSpan = 0;          // narrowest extent of the water's bounding box, which is its width across the band
    bool WaterOnOneEdge = true; // the water's bounding box hugs one grid edge
};

bool NeighbourIs(const GeneratedDistrict& d, int x, int y, DistrictCellKind kind)
{
    return Is(d, x - 1, y, kind) || Is(d, x + 1, y, kind) || Is(d, x, y - 1, kind) || Is(d, x, y + 1, kind);
}

int StreetNeighbours(const GeneratedDistrict& d, int x, int y)
{
    return (Is(d, x - 1, y, DistrictCellKind::Street) ? 1 : 0) + (Is(d, x + 1, y, DistrictCellKind::Street) ? 1 : 0) +
           (Is(d, x, y - 1, DistrictCellKind::Street) ? 1 : 0) + (Is(d, x, y + 1, DistrictCellKind::Street) ? 1 : 0);
}

/** The guarantees every archetype makes; returns the figures the per-archetype ranges are about. */
Measure CheckGuarantees(const GeneratedDistrict& d)
{
    Measure m;
    const bool waterAllowed = d.Archetype == DistrictArchetype::Riverfront || d.Archetype == DistrictArchetype::Port;
    const bool railAllowed = d.Archetype == DistrictArchetype::RailYard;
    int waterMinX = N, waterMaxX = -1, waterMinY = N, waterMaxY = -1;
    for (int y = 0; y < N; ++y)
    {
        for (int x = 0; x < N; ++x)
        {
            const DistrictCell& cell = d.At(x, y);
            Check(waterAllowed || cell.Kind != DistrictCellKind::Water, "water only in riverfront and port");
            Check(railAllowed || cell.Kind != DistrictCellKind::Rail, "rail only in rail_yard");
            if (cell.Kind == DistrictCellKind::Water)
            {
                ++m.Water;
                waterMinX = std::min(waterMinX, x);
                waterMaxX = std::max(waterMaxX, x);
                waterMinY = std::min(waterMinY, y);
                waterMaxY = std::max(waterMaxY, y);
            }
            if (cell.Kind == DistrictCellKind::Rail)
            {
                ++m.Rail;
            }
            if (cell.Kind == DistrictCellKind::Open && NeighbourIs(d, x, y, DistrictCellKind::Water))
            {
                ++m.OpenByWater;
            }
            if (cell.Kind == DistrictCellKind::Building)
            {
                ++m.Buildings;
                m.MinStoreys = std::min(m.MinStoreys, static_cast<int>(cell.Storeys));
                m.MaxStoreys = std::max(m.MaxStoreys, static_cast<int>(cell.Storeys));
                Check(cell.Storeys >= 1, "a building has at least one storey");
                Check(NeighbourIs(d, x, y, DistrictCellKind::Street), "every building lot touches a street");
            }
            else
            {
                Check(cell.Storeys == 0, "only a building has storeys");
            }
            if (cell.Kind == DistrictCellKind::Open)
            {
                ++m.Open;
            }
            if (cell.Kind == DistrictCellKind::Street)
            {
                ++m.Streets;
                Check(NeighbourIs(d, x, y, DistrictCellKind::Street), "no street cell is isolated");
                const bool byWater = NeighbourIs(d, x, y, DistrictCellKind::Water);
                m.StreetsByWater += byWater ? 1 : 0;
                m.Bridges += (Is(d, x - 1, y, DistrictCellKind::Water) && Is(d, x + 1, y, DistrictCellKind::Water)) ||
                                     (Is(d, x, y - 1, DistrictCellKind::Water) && Is(d, x, y + 1, DistrictCellKind::Water))
                                 ? 1
                                 : 0;
                m.Crossings += (Is(d, x - 1, y, DistrictCellKind::Rail) && Is(d, x + 1, y, DistrictCellKind::Rail)) ||
                                       (Is(d, x, y - 1, DistrictCellKind::Rail) && Is(d, x, y + 1, DistrictCellKind::Rail))
                                   ? 1
                                   : 0;
                if (x > 0 && y > 0 && x < N - 1 && y < N - 1 && StreetNeighbours(d, x, y) == 1)
                {
                    ++m.DeadEnds;
                    m.PierTips += byWater ? 1 : 0;
                }
            }
        }
    }
    Check(m.Streets + m.Buildings + m.Open + m.Water + m.Rail == DistrictCellCount, "every cell is street, building, open, water or rail");
    if (m.Water > 0)
    {
        // The widest water band is 20 cells (the port), so the box must sit within 20 cells of one edge.
        m.WaterOnOneEdge = waterMaxX <= 19 || waterMinX >= N - 20 || waterMaxY <= 19 || waterMinY >= N - 20;
        m.WaterSpan = std::min(waterMaxX - waterMinX, waterMaxY - waterMinY) + 1;
    }
    for (int a = 0; a < N; ++a)
    {
        int rowRun = 0;
        int columnRun = 0;
        int rowRail = 0;
        int columnRail = 0;
        for (int b = 0; b < N; ++b)
        {
            rowRun = Is(d, b, a, DistrictCellKind::Building) ? rowRun + 1 : 0;
            columnRun = Is(d, a, b, DistrictCellKind::Building) ? columnRun + 1 : 0;
            m.LongestRun = std::max({m.LongestRun, rowRun, columnRun});
            rowRail += Is(d, b, a, DistrictCellKind::Rail) ? 1 : 0;
            columnRail += Is(d, a, b, DistrictCellKind::Rail) ? 1 : 0;
        }
        m.RailLines += (rowRail >= 40 ? 1 : 0) + (columnRail >= 40 ? 1 : 0);
    }
    Check(m.Streets == CountDistrictCells(d, DistrictCellKind::Street), "CountDistrictCells agrees");

    // One 4-connected street network.
    std::vector<char> seen(DistrictCellCount, 0);
    bool found = false;
    for (int i = 0; i < DistrictCellCount && !found; ++i)
    {
        if (d.Cells[static_cast<std::size_t>(i)].Kind == DistrictCellKind::Street)
        {
            found = true;
            Check(Flood(d, i % N, i / N, DistrictCellKind::Street, seen) == m.Streets, "all street cells are 4-connected");
        }
    }
    Check(found, "a district has streets");

    // The network reaches all four edges.
    bool edge[4] = {false, false, false, false};
    for (int i = 0; i < N; ++i)
    {
        edge[0] |= Is(d, i, 0, DistrictCellKind::Street);
        edge[1] |= Is(d, i, N - 1, DistrictCellKind::Street);
        edge[2] |= Is(d, 0, i, DistrictCellKind::Street);
        edge[3] |= Is(d, N - 1, i, DistrictCellKind::Street);
    }
    Check(edge[0] && edge[1] && edge[2] && edge[3], "streets reach all four edges");

    bool fullRow[N] = {};
    bool fullColumn[N] = {};
    for (int i = 0; i < N; ++i)
    {
        bool row = true;
        bool column = true;
        for (int k = 0; k < N; ++k)
        {
            row = row && Is(d, k, i, DistrictCellKind::Street);
            column = column && Is(d, i, k, DistrictCellKind::Street);
        }
        fullRow[i] = row;
        fullColumn[i] = column;
        m.FullStreetRows += row ? 1 : 0;
        m.FullStreetColumns += column ? 1 : 0;
    }
    for (int i = 0; i + 2 < N; ++i)
    {
        m.BoulevardLines += (fullRow[i] && fullRow[i + 1] && fullRow[i + 2] ? 1 : 0) +
                            (fullColumn[i] && fullColumn[i + 1] && fullColumn[i + 2] ? 1 : 0);
    }

    std::vector<char> openSeen(DistrictCellCount, 0);
    for (int i = 0; i < DistrictCellCount; ++i)
    {
        if (d.Cells[static_cast<std::size_t>(i)].Kind == DistrictCellKind::Open && openSeen[static_cast<std::size_t>(i)] == 0)
        {
            int x0 = N, x1 = 0, y0 = N, y1 = 0;
            const int size = Flood(d, i % N, i / N, DistrictCellKind::Open, openSeen, &x0, &x1, &y0, &y1);
            if (size > m.LargestOpen)
            {
                m.LargestOpen = size;
                m.OpenCentreX2 = x0 + x1;
                m.OpenCentreY2 = y0 + y1;
            }
        }
    }
    return m;
}

/** The smallest and largest value seen over the seeds. */
struct Range
{
    int Min = 1 << 20;
    int Max = 0;
    void Add(int v)
    {
        Min = std::min(Min, v);
        Max = std::max(Max, v);
    }
};

struct Totals
{
    long Streets = 0;
    long Buildings = 0;
    long Open = 0;
    long StoreySum = 0;
    int MinStreets = 1 << 20, MaxStreets = 0;
    int MinBuildings = 1 << 20, MaxBuildings = 0;
    int MinOpen = 1 << 20, MaxOpen = 0;
    int MinStoreys = 255, MaxStoreys = 0;
    int MinFullRows = 1 << 20, MaxFullRows = 0;
    int MinLargestOpen = 1 << 20, MaxLargestOpen = 0;
    int MaxCentreOffset = 0;
    Range Water, Rail, DeadEnds, PierTips, LongestRun, StreetsByWater, OpenByWater, Bridges, Crossings, RailLines, BoulevardLines,
        WaterSpan;
    bool WaterOnOneEdge = true;
};

/** Generate 200 seeds of one archetype, check every guarantee on each, and gather the ranges. */
Totals Sweep(DistrictArchetype archetype, bool printRanges)
{
    Totals t;
    for (int i = 0; i < SeedCount; ++i)
    {
        const GeneratedDistrict d = GenerateDistrict(archetype, SeedOf(i));
        Check(d.Archetype == archetype && d.Seed == SeedOf(i), "the district records its recipe");
        Check(Same(d, GenerateDistrict(archetype, SeedOf(i))), "the same archetype and seed regenerate identically");
        const Measure m = CheckGuarantees(d);

        t.Streets += m.Streets;
        t.Buildings += m.Buildings;
        t.Open += m.Open;
        t.StoreySum += static_cast<long>(m.MinStoreys + m.MaxStoreys);
        t.MinStreets = std::min(t.MinStreets, m.Streets);
        t.MaxStreets = std::max(t.MaxStreets, m.Streets);
        t.MinBuildings = std::min(t.MinBuildings, m.Buildings);
        t.MaxBuildings = std::max(t.MaxBuildings, m.Buildings);
        t.MinOpen = std::min(t.MinOpen, m.Open);
        t.MaxOpen = std::max(t.MaxOpen, m.Open);
        t.MinStoreys = std::min(t.MinStoreys, m.MinStoreys);
        t.MaxStoreys = std::max(t.MaxStoreys, m.MaxStoreys);
        t.MinFullRows = std::min(t.MinFullRows, m.FullStreetRows);
        t.MaxFullRows = std::max(t.MaxFullRows, m.FullStreetRows);
        t.MinLargestOpen = std::min(t.MinLargestOpen, m.LargestOpen);
        t.MaxLargestOpen = std::max(t.MaxLargestOpen, m.LargestOpen);
        t.MaxCentreOffset = std::max({t.MaxCentreOffset, std::abs(m.OpenCentreX2 - N), std::abs(m.OpenCentreY2 - N)});
        t.Water.Add(m.Water);
        t.Rail.Add(m.Rail);
        t.DeadEnds.Add(m.DeadEnds);
        t.PierTips.Add(m.PierTips);
        t.LongestRun.Add(m.LongestRun);
        t.StreetsByWater.Add(m.StreetsByWater);
        t.OpenByWater.Add(m.OpenByWater);
        t.Bridges.Add(m.Bridges);
        t.Crossings.Add(m.Crossings);
        t.RailLines.Add(m.RailLines);
        t.BoulevardLines.Add(m.BoulevardLines);
        t.WaterSpan.Add(m.WaterSpan);
        t.WaterOnOneEdge = t.WaterOnOneEdge && m.WaterOnOneEdge;
    }
    if (printRanges)
    {
        std::cout << DistrictArchetypeName(archetype) << ": streets " << t.MinStreets << ".." << t.MaxStreets << ", buildings "
                  << t.MinBuildings << ".." << t.MaxBuildings << ", open " << t.MinOpen << ".." << t.MaxOpen << ", storeys "
                  << t.MinStoreys << ".." << t.MaxStoreys << ", full street rows " << t.MinFullRows << ".." << t.MaxFullRows
                  << ", largest open " << t.MinLargestOpen << ".." << t.MaxLargestOpen << ", centre offset x2 " << t.MaxCentreOffset
                  << '\n';
        std::cout << "    water " << t.Water.Min << ".." << t.Water.Max << " (span " << t.WaterSpan.Min << ".." << t.WaterSpan.Max
                  << ", one edge " << t.WaterOnOneEdge << "), rail " << t.Rail.Min << ".." << t.Rail.Max << ", rail lines "
                  << t.RailLines.Min << ".." << t.RailLines.Max << ", crossings " << t.Crossings.Min << ".." << t.Crossings.Max
                  << ", bridges " << t.Bridges.Min << ".." << t.Bridges.Max << ", streets by water " << t.StreetsByWater.Min << ".."
                  << t.StreetsByWater.Max << ", open by water " << t.OpenByWater.Min << ".." << t.OpenByWater.Max << ", pier tips "
                  << t.PierTips.Min << ".." << t.PierTips.Max << ", dead ends " << t.DeadEnds.Min << ".." << t.DeadEnds.Max
                  << ", longest run " << t.LongestRun.Min << ".." << t.LongestRun.Max << ", boulevard lines "
                  << t.BoulevardLines.Min << ".." << t.BoulevardLines.Max << '\n';
    }
    return t;
}
} // namespace
