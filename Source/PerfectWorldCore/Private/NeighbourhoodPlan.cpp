#include "NeighbourhoodPlan.h"

#include "LaneFrontageCatalogue.h"
#include "LaneTallFlatsLayout.h"

#include <algorithm>
#include <limits>
#include <set>
#include <utility>

namespace PerfectWorld
{
namespace
{
using R71Point = NeighbourhoodPoint;
using R71Rect = NeighbourhoodRect;
using R71Kind = NeighbourhoodFeatureKind;
using R71Foot = NeighbourhoodFootprintKind;
using R71Class = NeighbourhoodStreetClass;
using R71Link = NeighbourhoodLinkKind;
bool R71Ordered(R71Rect r) { return r.X0 <= r.X1 && r.Y0 <= r.Y1; }
bool R71Intersects(R71Rect a, R71Rect b, bool area = false)
{
    return area ? a.X0 < b.X1 && b.X0 < a.X1 && a.Y0 < b.Y1 && b.Y0 < a.Y1
                : a.X0 <= b.X1 && b.X0 <= a.X1 && a.Y0 <= b.Y1 && b.Y0 <= a.Y1;
}
R71Rect R71Bounds(const std::vector<R71Point>& points)
{
    if (points.empty()) return {};
    R71Rect r{points[0].X, points[0].Y, points[0].X, points[0].Y};
    for (auto p : points) { r.X0 = std::min(r.X0, p.X); r.X1 = std::max(r.X1, p.X);
        r.Y0 = std::min(r.Y0, p.Y); r.Y1 = std::max(r.Y1, p.Y); }
    return r;
}
std::int64_t R71Length(const std::vector<R71Point>& points)
{
    std::int64_t n = 0;
    for (std::size_t i = 1; i < points.size(); ++i)
        for (auto d : {std::int64_t(points[i].X) - points[i-1].X, std::int64_t(points[i].Y) - points[i-1].Y,
                       std::int64_t(points[i].Z) - points[i-1].Z}) n += d < 0 ? -d : d;
    return n;
}
struct R71Author
{
    NeighbourhoodPlan P;
    NeighbourhoodFeature& Shape(std::string id, R71Kind kind, std::vector<R71Point> points, std::string parent = {})
    {
        NeighbourhoodFeature f; f.Id = std::move(id); f.Kind = kind; f.Geometry = std::move(points);
        f.Bounds = R71Bounds(f.Geometry); f.ParentId = std::move(parent); P.Features.push_back(std::move(f));
        return P.Features.back();
    }
    NeighbourhoodFeature& Area(std::string id, R71Kind kind, R71Rect r, std::string parent = {}, int z = -185)
    { return Shape(std::move(id), kind, {{r.X0,r.Y0,z},{r.X1,r.Y0,z},{r.X1,r.Y1,z},{r.X0,r.Y1,z}}, std::move(parent)); }
    void Node(std::string id, int x, int y, int z = -185) { P.Nodes.push_back({std::move(id), {x,y,z}}); }
    void Edge(std::string a, std::string b, R71Link kind = R71Link::Walk, std::string constraint = {},
              std::vector<R71Point> via = {})
    {
        const auto* from = FindNeighbourhoodNode(P,a); const auto* to = FindNeighbourhoodNode(P,b);
        if (!from || !to) return;
        via.insert(via.begin(), from->Position); via.push_back(to->Position);
        P.Links.push_back({"link." + a + "." + b, std::move(a), std::move(b), via, R71Length(via), kind, std::move(constraint)});
    }
    void Point(std::string id, R71Kind kind, const std::string& node, std::string parent = {}, std::string related = {})
    {
        const auto* n = FindNeighbourhoodNode(P,node); if (!n) return;
        auto& f = Shape(std::move(id),kind,{n->Position},std::move(parent)); f.LinkNodeId = node; f.RelatedId = std::move(related);
    }
    void Lot(std::string id, R71Rect r, R71Foot kind, int storeys = 0, bool existing = false, std::string parent = {})
    { auto& f = Area(std::move(id),R71Kind::Footprint,r,std::move(parent)); f.FootprintKind = kind; f.Storeys = storeys; f.Existing = existing; }
    void Street(const std::string& id, R71Point a, R71Point b, int width, int path, R71Class kind)
    {
        a.Z = b.Z = -200; const bool horizontal = a.Y == b.Y; const int h = width/2;
        auto& line = Shape(id,R71Kind::CentreLine,{a,b}); line.WidthCm = width; line.StreetClass = kind;
        const R71Rect road = horizontal ? R71Rect{a.X,a.Y-h,b.X,b.Y+h} : R71Rect{a.X-h,a.Y,b.X+h,b.Y};
        auto& c = Area(id+".carriageway",R71Kind::Carriageway,road,id,-200); c.StreetClass = kind; c.WidthCm = width;
        const R71Rect left = horizontal ? R71Rect{a.X,a.Y+h,b.X,b.Y+h+path} : R71Rect{a.X-h-path,a.Y,b.X-h,b.Y};
        const R71Rect right = horizontal ? R71Rect{a.X,a.Y-h-path,b.X,b.Y-h} : R71Rect{a.X+h,a.Y,b.X+h+path,b.Y};
        for (auto side : {true,false}) { auto& f = Area(id+(side?".footpath_left":".footpath_right"),R71Kind::Footpath,side?left:right,id);
            f.StreetClass = kind; f.WidthCm = path; }
        Area(id+".reserve",R71Kind::Reservation,{left.X0 < right.X0 ? left.X0 : right.X0,
            std::min(left.Y0,right.Y0),std::max(left.X1,right.X1),std::max(left.Y1,right.Y1)},id);
        // Canonical direction a->b: left lane is forward, right lane reverse (Indian left-running traffic).
        R71Point f0=a,f1=b,r0=b,r1=a;
        if (horizontal) { f0.Y += width/4; f1.Y += width/4; r0.Y -= width/4; r1.Y -= width/4; }
        else { f0.X -= width/4; f1.X -= width/4; r0.X += width/4; r1.X += width/4; }
        auto& f = Shape(id+".vehicle_forward",R71Kind::VehicleLane,{f0,f1},id); f.WidthCm=width/2; f.StreetClass=kind;
        auto& r = Shape(id+".vehicle_reverse",R71Kind::VehicleLane,{r0,r1},id); r.WidthCm=width/2; r.StreetClass=kind;
    }
    void Stop(std::string id, const std::string& street, bool forward, R71Point a, R71Point b, std::string crossing)
    { auto& f = Shape(std::move(id),R71Kind::StopLine,{a,b},street+(forward?".vehicle_forward":".vehicle_reverse")); f.RelatedId=std::move(crossing); }
};
void R71Lane(R71Author& a)
{
    a.Street("street.nabapara.lane",{-2400,0},{6400,0},500,150,R71Class::Lane);
    a.Area("reserve.nabapara.original_lane",R71Kind::Reservation,{-3300,-1500,7300,1500});
    // Catalogue coordinates and stable ids are references, never edits to the original frontages.
    for (const auto& row : LaneInterior::KitFrontageEntries())
    {
        const int y0 = row.Side * row.FrontAbsYCm, y1 = y0 + row.Side * row.DepthCm;
        a.Lot(std::string(row.BuildingId),{row.OriginXCm,std::min(y0,y1),row.OriginXCm+row.WidthCm,std::max(y0,y1)},R71Foot::ExistingBuilding,row.Storeys,true);
        const auto layout = LaneInterior::GenerateInteriorLayout(LaneInterior::MakeFrontageInput(row,15,300));
        const auto opening = std::find_if(layout.Openings.begin(),layout.Openings.end(),[&](const auto& o){return o.DoorId == row.EntranceDoorId;});
        if (opening == layout.Openings.end()) continue;
        const int x = row.OriginXCm + (opening->Plan.U0+opening->Plan.U1)/2;
        const std::string node = "walk."+std::string(row.EntranceDoorId);
        a.Node(node,x,y0); a.Point(std::string(row.EntranceDoorId),R71Kind::Entrance,node,std::string(row.BuildingId),std::string(row.EntranceDoorId));
        if (row.Kind == LaneInterior::Variant::CompactResidence) a.P.HomeEntrances.push_back(std::string(row.EntranceDoorId));
    }
    struct R71OldLot { const char* Id; R71Rect R; int Storeys; };
    for (const auto& row : std::vector<R71OldLot>{
        {"care_home",{-1400,-1300,-300,-400},3},{"opening_home",{-300,-1300,700,-400},5},
        {"stationery",{-100,460,600,1160},3},{"shanti_chai",{600,520,1300,1420},3},
        // alley_house, stair_flats and corner_stalls come from the kit catalogue above since R6-01 opened them.
        {"west_end_block",{-3300,-1300,-2400,1300},4},{"east_end_block",{6400,-1300,7300,1300},5}})
        a.Lot("building.nabapara."+std::string(row.Id),row.R,R71Foot::ExistingBuilding,row.Storeys,true);
    for (const auto& row : std::vector<std::pair<std::string,R71Point>>{
        {"door.nabapara.home_front",{400,-400}},{"door.nabapara.care_front",{-1165,-400}},
        {"door.nabapara.stationery_front",{150,460}},{"door.nabapara.shanti_chai_shutter",{950,520}}})
    {
        const std::string n="walk."+row.first; a.Node(n,row.second.X,row.second.Y); a.Point(row.first,R71Kind::Entrance,n,{},row.first);
        if (row.second.Y<0) a.P.HomeEntrances.push_back(row.first);
    }
    // Dwelling thresholds branch from the shared public passage; the upper route follows the existing stair.
    a.Node("walk.nabapara.flats_passage",-750,595);
    a.Edge("walk.door.nabapara.tall_flats_front","walk.nabapara.flats_passage",R71Link::Interior,"door.nabapara.tall_flats_front");
    const auto flats=LaneTallFlats::GenerateLayout();
    for (const auto* layout : {&flats.Ground,&flats.Upper}) for (const auto& o : layout->Openings)
    {
        if (o.DoorId.empty() || o.DoorId=="door.nabapara.tall_flats_front") continue;
        const std::string n="walk."+o.DoorId;
        a.Node(n,-1300+(o.Plan.U0+o.Plan.U1)/2,460+(o.Plan.V0+o.Plan.V1)/2,layout==&flats.Upper?115:-185);
        a.Point(o.DoorId,R71Kind::Entrance,n,"building.nabapara.tall_flats",o.DoorId); a.P.HomeEntrances.push_back(o.DoorId);
        if (layout==&flats.Upper) a.Edge("walk.nabapara.flats_passage",n,R71Link::Stair,o.DoorId,
            {{-750,715,-185},{-750,1115,115},{-750,1175,115}});
        else a.Edge("walk.nabapara.flats_passage",n,R71Link::Interior,o.DoorId);
    }
    // Exterior footpaths split at every entrance and at the alley crossing; no route through an end mass.
    for (const int side : {-1,1})
    {
        std::vector<int> xs{-2400,2860,3340,4600,6400};
        const auto entrances=a.P.Features;
        for (const auto& f:entrances) if(f.Kind==R71Kind::Entrance && f.Geometry[0].Z==-185 &&
            side*f.Geometry[0].Y>0 && f.Geometry[0].Y*side<=520) xs.push_back(f.Geometry[0].X);
        std::sort(xs.begin(),xs.end()); xs.erase(std::unique(xs.begin(),xs.end()),xs.end());
        std::string previous;
        for(int x:xs)
        {
            const std::string n="walk.nabapara.lane_"+std::string(side>0?"north_":"south_")+std::to_string(x);
            a.Node(n,x,side*325);
            if(!previous.empty()) a.Edge(previous,n,side>0 && x==3340?R71Link::Crossing:R71Link::Walk,
                side>0 && x==3340?"crossing.nabapara.cross_mouth":"");
            for(const auto& f:entrances) if(f.Kind==R71Kind::Entrance && f.Geometry[0].X==x &&
                side*f.Geometry[0].Y>0 && side*f.Geometry[0].Y<=520) a.Edge(f.LinkNodeId,n);
            previous=n;
        }
    }
    a.Edge("walk.nabapara.lane_south_2860","walk.nabapara.lane_north_2860",R71Link::Crossing,"crossing.nabapara.lane");
}
} // namespace

NeighbourhoodPlan BuildNeighbourhoodPlan(std::uint64_t seed)
{
    R71Author a; a.P.Seed=seed; R71Lane(a);
    // Cross street occupies the existing 600 cm alley: 360 cm shared carriageway + two 120 cm footpaths.
    a.Street("street.nabapara.cross",{3100,250},{3100,4200},360,120,R71Class::CrossStreet);
    a.Street("street.nabapara.para",{-4000,4200},{16000,4200},800,200,R71Class::ParaRoad);
    a.Street("street.nabapara.main",{16000,-6000},{16000,14000},1400,250,R71Class::MainRoad);
    // Trim sidewalk paving out of junction carriageways; never lay a raised slab across traffic.
    for(auto& f:a.P.Features) if(f.Kind==R71Kind::Footpath &&
        (f.ParentId=="street.nabapara.cross" || f.ParentId=="street.nabapara.para"))
    {
        for(auto& v:f.Geometry) {
            if(f.ParentId=="street.nabapara.cross") v.Y=std::min(v.Y,3800);
            else v.X=std::min(v.X,15300);
        }
        f.Bounds=R71Bounds(f.Geometry);
    }
    std::erase_if(a.P.Features,[](const auto& f){return f.Id=="street.nabapara.main.footpath_left";});
    std::erase_if(a.P.Features,[](const auto& f){return f.Id=="street.nabapara.lane.footpath_left";});
    std::erase_if(a.P.Features,[](const auto& f){return f.Id=="street.nabapara.para.footpath_right";});
    for(const auto& r:std::vector<std::pair<std::string,R71Rect>>{
        {"west",{-4000,3600,2920,3800}},{"east",{3280,3600,15300,3800}}})
    { auto& f=a.Area("street.nabapara.para.footpath_right_"+r.first,R71Kind::Footpath,r.second,"street.nabapara.para");
      f.WidthCm=200;f.StreetClass=R71Class::ParaRoad; }
    for(const auto& r:std::vector<std::pair<std::string,R71Rect>>{
        {"west",{-2400,250,2920,400}},{"east",{3280,250,6400,400}}})
    { auto& f=a.Area("street.nabapara.lane.footpath_left_"+r.first,R71Kind::Footpath,r.second,"street.nabapara.lane");
      f.WidthCm=150;f.StreetClass=R71Class::Lane; }
    for(const auto& r:std::vector<std::pair<std::string,R71Rect>>{
        {"south",{15050,-6000,15300,3800}},{"north",{15050,4600,15300,14000}}})
    { auto& f=a.Area("street.nabapara.main.footpath_left_"+r.first,R71Kind::Footpath,r.second,"street.nabapara.main");
      f.WidthCm=250;f.StreetClass=R71Class::MainRoad; }
    a.Area("site.nabapara.school",R71Kind::Site,{5000,4850,12000,11500},std::string(NeighbourhoodSchoolInstitutionId));
    a.Area("reserve.nabapara.school",R71Kind::Reservation,{5000,4800,12000,11500});
    a.Lot("school.nabapara.forecourt",{5500,4850,11500,6050},R71Foot::Forecourt,0,false,"site.nabapara.school");
    a.Lot("building.nabapara.school_teaching",{5500,6500,8000,11000},R71Foot::TeachingBlock,3,false,"site.nabapara.school");
    a.Lot("school.nabapara.playground",{8300,6500,11700,11000},R71Foot::Playground,0,false,"site.nabapara.school");
    a.Area("site.nabapara.club_pandal",R71Kind::Site,{-2000,5000,0,7000});
    a.Area("reserve.nabapara.club_pandal",R71Kind::Reservation,{-2300,4700,0,7000});
    a.Lot("building.nabapara.club",{-2000,5000,-1000,6000},R71Foot::Club,1);
    a.Lot("space.nabapara.pandal",{-900,5000,0,7000},R71Foot::PandalSpace);
    a.Lot("building.nabapara.sweet_shop",{6000,2300,7600,3500},R71Foot::SweetShop,2);
    a.Lot("building.nabapara.pharmacy",{8500,2500,9900,3500},R71Foot::Pharmacy,2);
    a.Lot("pitch.nabapara.tea",{11000,3100,11500,3500},R71Foot::TeaPitch);
    a.Lot("pitch.nabapara.vendor",{12000,3100,12500,3500},R71Foot::VendorPitch);
    a.Lot("stand.nabapara.rickshaw_auto",{13200,3100,14500,3500},R71Foot::TransportStand);
    struct R71NodeRow { const char* Name; int X,Y; };
    for(const auto& r:std::vector<R71NodeRow>{{"cross_east",3340,3700},{"cross_west",2860,3700},{"para_cross_south",4000,3700},
        {"para_cross_north",4000,4700},{"para_west_south",-4000,3700},{"para_west_north",-4000,4700},
        {"school_approach",6500,4700},{"school_gate",6500,4850},{"school_forecourt",6500,5850},{"school_campus_path",6500,6250},
        {"school_office",6500,6500},{"school_classroom",7500,6500},{"school_playground",8200,6250},
        {"club_approach",-1500,4700},{"club_entrance",-1500,5000},{"club_benches",-1500,6250},
        {"pandal",-450,6250},{"main_west",15175,3700},{"main_north_west",15175,4700},
        {"main_cross_west",15175,5200},{"main_cross_east",16825,5200},{"bus_west",15175,5500},
        {"bus_east",16825,6500},{"main_south_edge",15175,-6000},{"main_north_edge",15175,14000},
        {"main_east_south_edge",16825,-6000},{"main_east_north_edge",16825,14000},{"south_alley",4600,-950}})
        a.Node("walk.nabapara."+std::string(r.Name),r.X,r.Y);
    const auto edge=[&](const char* x,const char* y,R71Link k=R71Link::Walk,std::string c={}){
        a.Edge("walk.nabapara."+std::string(x),"walk.nabapara."+std::string(y),k,std::move(c));};
    edge("lane_north_3340","cross_east"); edge("cross_east","para_cross_south");
    edge("lane_north_2860","cross_west"); edge("para_west_south","cross_west");
    edge("cross_west","cross_east",R71Link::Crossing,"crossing.nabapara.cross_north");
    edge("para_cross_south","para_cross_north",R71Link::Crossing,"crossing.nabapara.para");
    edge("para_cross_north","school_approach"); edge("school_approach","school_gate"); edge("school_gate","school_forecourt");
    edge("school_forecourt","school_campus_path");
    edge("school_campus_path","school_office"); edge("school_campus_path","school_classroom",R71Link::Walk,{});
    a.P.Links.back().Points.insert(a.P.Links.back().Points.begin()+1,{7500,6250,-185}); a.P.Links.back().LengthCm=R71Length(a.P.Links.back().Points);
    edge("school_campus_path","school_playground"); edge("para_west_north","club_approach"); edge("club_approach","para_cross_north");
    edge("club_approach","club_entrance");
    a.Edge("walk.nabapara.club_approach","walk.nabapara.club_benches",R71Link::Walk,{},{{-2150,4700,-185},{-2150,6250,-185}});
    edge("club_benches","pandal"); edge("school_approach","main_north_west");
    edge("main_north_west","main_cross_west"); edge("main_cross_west","main_cross_east",R71Link::Crossing,"crossing.nabapara.main");
    edge("main_cross_west","bus_west"); edge("main_cross_east","bus_east"); edge("bus_west","main_north_edge");
    edge("bus_east","main_east_north_edge"); edge("main_west","main_south_edge"); edge("main_cross_east","main_east_south_edge");
    edge("lane_south_4600","south_alley");
    struct R71Counter {const char* Name; int X;};
    for(const auto& r:std::vector<R71Counter>{{"sweet_shop",6800},{"pharmacy",9200},{"tea",11250},{"vendor",12250},{"rickshaw_auto",13850}})
    {
        const std::string stem="walk.nabapara."+std::string(r.Name), owner=(r.X<10000?"building.nabapara.":r.X<13000?"pitch.nabapara.":"stand.nabapara.")+std::string(r.Name);
        a.Node(stem,r.X,3500); a.Node(stem+"_path",r.X,3700); a.Edge(stem,stem+"_path");
        a.Point("entrance.nabapara."+std::string(r.Name),R71Kind::Entrance,stem,owner);
        a.Node(stem+"_counter",r.X,r.X<10000?3200:3400); a.Edge(stem,stem+"_counter",R71Link::Interior);
        a.Point("anchor.nabapara."+std::string(r.Name)+"_counter",R71Kind::Destination,stem+"_counter",owner);
    }
    const std::vector<const char*> south{"para_cross_south","sweet_shop_path","pharmacy_path","tea_path","vendor_path","rickshaw_auto_path","main_west"};
    for(std::size_t i=1;i<south.size();++i) edge(south[i-1],south[i]);
    a.Point(std::string(NeighbourhoodSchoolGateId),R71Kind::Entrance,std::string(NeighbourhoodSchoolGateNodeId),"site.nabapara.school");
    a.P.Features.back().WidthCm=400;
    for(const auto& r:std::vector<std::pair<std::string,std::string>>{{"school_office","building.nabapara.school_teaching"},
        {"school_classroom","building.nabapara.school_teaching"},{"club_entrance","building.nabapara.club"}})
        a.Point("entrance.nabapara."+r.first,R71Kind::Entrance,"walk.nabapara."+r.first,r.second);
    for(const char* n:{"school_gate","club_benches","pandal","bus_west","bus_east"})
        a.Point("anchor.nabapara."+std::string(n),R71Kind::Destination,"walk.nabapara."+std::string(n));
    a.Point("stop.nabapara.bus_west",R71Kind::BusStop,"walk.nabapara.bus_west","street.nabapara.main.vehicle_forward");
    a.Point("stop.nabapara.bus_east",R71Kind::BusStop,"walk.nabapara.bus_east","street.nabapara.main.vehicle_reverse");
    for(const char* n:{"para_west_south","para_west_north","main_south_edge","main_north_edge","main_east_south_edge","main_east_north_edge","south_alley","lane_north_3340"})
        a.Point("population.nabapara."+std::string(n),R71Kind::PopulationEntrance,"walk.nabapara."+std::string(n));
    a.Area("crossing.nabapara.lane",R71Kind::Crossing,{2760,-250,2960,250},"street.nabapara.lane",-200);
    a.Area("crossing.nabapara.cross_mouth",R71Kind::Crossing,{2920,275,3280,375},"street.nabapara.cross",-200);
    a.Area("crossing.nabapara.cross_north",R71Kind::Crossing,{2920,3650,3280,3750},"street.nabapara.cross",-200);
    a.Area("crossing.nabapara.para",R71Kind::Crossing,{3900,3800,4100,4600},"street.nabapara.para",-200);
    a.Area("crossing.nabapara.main",R71Kind::Crossing,{15300,5100,16700,5300},"street.nabapara.main",-200);
    a.Stop("stop.nabapara.lane_east","street.nabapara.lane",true,{2660,0,-200},{2660,250,-200},"crossing.nabapara.lane");
    a.Stop("stop.nabapara.lane_west","street.nabapara.lane",false,{3060,-250,-200},{3060,0,-200},"crossing.nabapara.lane");
    a.Stop("stop.nabapara.cross_south","street.nabapara.cross",false,{3100,475,-200},{3280,475,-200},"crossing.nabapara.cross_mouth");
    a.Stop("stop.nabapara.cross_north","street.nabapara.cross",true,{2920,250,-200},{3100,250,-200},"crossing.nabapara.cross_mouth");
    a.Stop("stop.nabapara.cross_junction_north","street.nabapara.cross",true,{2920,3550,-200},{3100,3550,-200},"crossing.nabapara.cross_north");
    a.Stop("stop.nabapara.cross_junction_south","street.nabapara.cross",false,{3100,3850,-200},{3280,3850,-200},"crossing.nabapara.cross_north");
    a.Stop("stop.nabapara.para_east","street.nabapara.para",true,{3800,4200,-200},{3800,4600,-200},"crossing.nabapara.para");
    a.Stop("stop.nabapara.para_west","street.nabapara.para",false,{4200,3800,-200},{4200,4200,-200},"crossing.nabapara.para");
    a.Stop("stop.nabapara.main_north","street.nabapara.main",true,{15300,5000,-200},{16000,5000,-200},"crossing.nabapara.main");
    a.Stop("stop.nabapara.main_south","street.nabapara.main",false,{16000,5400,-200},{16700,5400,-200},"crossing.nabapara.main");
    // Whole authored lots, with 100 cm dressing allowance, exclude surrounding generated massing.
    const auto features=a.P.Features;
    for(const auto& f:features) if(f.Kind==R71Kind::Footprint) a.Area(f.Id+".reserve",R71Kind::Reservation,
        {f.Bounds.X0-100,f.Bounds.Y0-100,f.Bounds.X1+100,f.Bounds.Y1+100},f.Id);
    return std::move(a.P);
}

const NeighbourhoodFeature* FindNeighbourhoodFeature(const NeighbourhoodPlan& p,std::string_view id)
{
    for(const auto& f:p.Features) if(f.Id==id) return &f;
    return nullptr;
}
const NeighbourhoodLinkNode* FindNeighbourhoodNode(const NeighbourhoodPlan& p,std::string_view id)
{
    for(const auto& n:p.Nodes) if(n.Id==id) return &n;
    return nullptr;
}
std::vector<const NeighbourhoodFeature*> NeighbourhoodFeaturesInRect(const NeighbourhoodPlan& p,NeighbourhoodRect r)
{
    std::vector<const NeighbourhoodFeature*> out;
    if(!R71Ordered(r)) return out;
    for(const auto& f:p.Features) if(R71Intersects(f.Bounds,r)) out.push_back(&f);
    return out;
}
bool NeighbourhoodReservesRect(const NeighbourhoodPlan& p,NeighbourhoodRect r)
{
    if(!R71Ordered(r) || r.X0==r.X1 || r.Y0==r.Y1) return false;
    for(const auto& f:p.Features)
        if(f.Kind==R71Kind::Reservation && R71Intersects(f.Bounds,r,true)) return true;
    return false;
}
NeighbourhoodRoute RouteNeighbourhood(const NeighbourhoodPlan& p,std::string_view from,std::string_view to)
{
    const auto index=[&](std::string_view id){
        for(std::size_t i=0;i<p.Nodes.size();++i) if(p.Nodes[i].Id==id) return i;
        return p.Nodes.size();
    };
    const auto start=index(from),end=index(to),count=p.Nodes.size();
    if(start==count || end==count) return {};
    const auto infinity=std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> distance(count,infinity); std::vector<bool> done(count,false);
    std::vector<std::size_t> previous(count,count),links(count,p.Links.size()); distance[start]=0;
    for(std::size_t step=0;step<count;++step)
    {
        std::size_t u=count;
        for(std::size_t i=0;i<count;++i)
            if(!done[i] && distance[i]!=infinity && (u==count || distance[i]<distance[u])) u=i;
        if(u==count || u==end) break;
        done[u]=true;
        for(std::size_t k=0;k<p.Links.size();++k)
        {
            const auto& e=p.Links[k]; const auto v=e.From==p.Nodes[u].Id?index(e.To):e.To==p.Nodes[u].Id?index(e.From):count;
            if(v==count || done[v] || e.LengthCm<=0 || distance[u]>infinity-e.LengthCm) continue;
            if(distance[u]+e.LengthCm<distance[v])
            { distance[v]=distance[u]+e.LengthCm; previous[v]=u; links[v]=k; }
        }
    }
    if(distance[end]==infinity) return {};
    NeighbourhoodRoute result; result.Found=true; result.LengthCm=distance[end];
    for(auto n=end;;n=previous[n])
    {
        result.NodeIds.push_back(p.Nodes[n].Id);
        if(n==start) break;
        result.LinkIds.push_back(p.Links[links[n]].Id);
    }
    std::reverse(result.NodeIds.begin(),result.NodeIds.end());
    std::reverse(result.LinkIds.begin(),result.LinkIds.end());
    return result;
}
std::vector<std::string> ValidateNeighbourhoodPlan(const NeighbourhoodPlan& p)
{
    std::vector<std::string> errors; std::set<std::string> ids;
    for(const auto& f:p.Features)
    {
        if(f.Id.empty() || !ids.insert(f.Id).second) errors.push_back("feature.id:"+f.Id);
        if(f.Geometry.empty() || f.Bounds!=R71Bounds(f.Geometry) || !R71Ordered(f.Bounds)) errors.push_back("feature.geometry:"+f.Id);
        const bool polygon=f.Kind==R71Kind::Footprint || f.Kind==R71Kind::Site || f.Kind==R71Kind::Reservation ||
            f.Kind==R71Kind::Carriageway || f.Kind==R71Kind::Footpath || f.Kind==R71Kind::Crossing;
        const bool line=f.Kind==R71Kind::CentreLine || f.Kind==R71Kind::VehicleLane || f.Kind==R71Kind::StopLine;
        if(f.Geometry.size()!=(polygon?4u:line?2u:1u) || (polygon && (f.Bounds.X0==f.Bounds.X1 || f.Bounds.Y0==f.Bounds.Y1)))
            errors.push_back("feature.shape:"+f.Id);
        if(!f.LinkNodeId.empty() && !FindNeighbourhoodNode(p,f.LinkNodeId)) errors.push_back("feature.node:"+f.Id);
        if(!polygon && !line)
        {
            const auto* node=FindNeighbourhoodNode(p,f.LinkNodeId);
            if(!node || f.Geometry.empty() || node->Position!=f.Geometry[0]) errors.push_back("feature.access:"+f.Id);
        }
        if(f.Kind==R71Kind::StopLine)
        {
            const auto* lane=FindNeighbourhoodFeature(p,f.ParentId);
            const auto* crossing=FindNeighbourhoodFeature(p,f.RelatedId);
            if(!lane || lane->Kind!=R71Kind::VehicleLane || !crossing || crossing->Kind!=R71Kind::Crossing ||
                lane->ParentId!=crossing->ParentId) errors.push_back("stop.reference:"+f.Id);
        }
        if(f.Kind==R71Kind::Footpath) for(const auto& road:p.Features)
            if(road.Kind==R71Kind::Carriageway && R71Intersects(f.Bounds,road.Bounds,true)) errors.push_back("footpath.street:"+f.Id);
        if(f.Kind!=R71Kind::Footprint) continue;
        for(const auto& g:p.Features) if(g.Kind==R71Kind::Footprint && f.Id<g.Id && R71Intersects(f.Bounds,g.Bounds,true)) errors.push_back("footprint.overlap:"+f.Id+":"+g.Id);
        for(const auto& g:p.Features) if(g.Kind==R71Kind::Carriageway && R71Intersects(f.Bounds,g.Bounds,true)) errors.push_back("footprint.street:"+f.Id+":"+g.Id);
    }
    ids.clear();
    for(const auto& n:p.Nodes) if(n.Id.empty() || !ids.insert(n.Id).second) errors.push_back("node.id:"+n.Id);
    ids.clear();
    for(const auto& e:p.Links)
    {
        const auto* f=FindNeighbourhoodNode(p,e.From);const auto* t=FindNeighbourhoodNode(p,e.To);
        if(e.Id.empty() || !ids.insert(e.Id).second) errors.push_back("link.id:"+e.Id);
        if(!f || !t || e.Points.size()<2 || e.Points.front()!=f->Position || e.Points.back()!=t->Position || e.LengthCm<=0 || e.LengthCm!=R71Length(e.Points)) errors.push_back("link.geometry:"+e.Id);
        if(e.Kind==R71Link::Crossing && (!FindNeighbourhoodFeature(p,e.ConstraintId) || FindNeighbourhoodFeature(p,e.ConstraintId)->Kind!=R71Kind::Crossing)) errors.push_back("link.crossing:"+e.Id);
        for(std::size_t i=1;i<e.Points.size();++i)
        {
            const auto u=e.Points[i-1],v=e.Points[i];
            if(u.X!=v.X && u.Y!=v.Y) errors.push_back("link.diagonal:"+e.Id);
            if(e.Kind==R71Link::Interior || e.Kind==R71Link::Stair) continue;
            for(const auto& solid:p.Features)
            {
                if((solid.Kind!=R71Kind::Footprint && solid.Kind!=R71Kind::Carriageway) || solid.FootprintKind==R71Foot::Forecourt || solid.FootprintKind==R71Foot::Playground ||
                    solid.FootprintKind==R71Foot::PandalSpace || solid.FootprintKind==R71Foot::TransportStand) continue;
                const auto r=solid.Bounds;
                const bool hit=u.X==v.X ? u.X>r.X0 && u.X<r.X1 && std::max(u.Y,v.Y)>r.Y0 && std::min(u.Y,v.Y)<r.Y1
                    : u.Y>r.Y0 && u.Y<r.Y1 && std::max(u.X,v.X)>r.X0 && std::min(u.X,v.X)<r.X1;
                if(!hit) continue;
                if(solid.Kind==R71Kind::Carriageway)
                {
                    const auto* crossing=FindNeighbourhoodFeature(p,e.ConstraintId);
                    if(e.Kind!=R71Link::Crossing || !crossing || crossing->ParentId!=solid.ParentId)
                        errors.push_back("link.unmarked_crossing:"+e.Id+":"+solid.Id);
                    else
                    {
                        // The road-interior portion must actually use the named crossing, not merely its street.
                        const R71Rect portion{std::max(std::min(u.X,v.X),r.X0),std::max(std::min(u.Y,v.Y),r.Y0),
                            std::min(std::max(u.X,v.X),r.X1),std::min(std::max(u.Y,v.Y),r.Y1)};
                        const auto c=crossing->Bounds;
                        if(portion.X0<c.X0 || portion.Y0<c.Y0 || portion.X1>c.X1 || portion.Y1>c.Y1)
                            errors.push_back("link.outside_crossing:"+e.Id+":"+crossing->Id);
                    }
                }
                else errors.push_back("link.solid:"+e.Id+":"+solid.Id);
            }
        }
    }
    if(!errors.empty()) return errors;
    for(const auto& n:p.Nodes) if(!RouteNeighbourhood(p,n.Id,NeighbourhoodSchoolGateNodeId).Found) errors.push_back("node.disconnected:"+n.Id);
    if(p.HomeEntrances.empty()) errors.push_back("home.empty");
    for(const auto& home:p.HomeEntrances)
    {
        const auto* f=FindNeighbourhoodFeature(p,home);
        if(!f || f->Kind!=R71Kind::Entrance || !RouteNeighbourhood(p,f->LinkNodeId,NeighbourhoodSchoolGateNodeId).Found ||
            !RouteNeighbourhood(p,f->LinkNodeId,NeighbourhoodMainRoadNodeId).Found) errors.push_back("home.route:"+home);
    }
    const auto* lane=FindNeighbourhoodFeature(p,"street.nabapara.lane.carriageway");
    if(!lane || lane->Bounds!=R71Rect{-2400,-250,6400,250}) errors.push_back("lane.preservation");
    for(const auto& row:LaneInterior::KitFrontageEntries())
    {
        const auto* f=FindNeighbourhoodFeature(p,row.BuildingId);
        const int y=row.Side*row.FrontAbsYCm,back=y+row.Side*row.DepthCm;
        if(!f || !f->Existing || f->Storeys!=row.Storeys || f->Bounds!=R71Rect{row.OriginXCm,std::min(y,back),row.OriginXCm+row.WidthCm,std::max(y,back)})
            errors.push_back("frontage.preservation:"+std::string(row.BuildingId));
        if(!FindNeighbourhoodFeature(p,row.EntranceDoorId)) errors.push_back("frontage.entrance:"+std::string(row.EntranceDoorId));
        const auto layout=LaneInterior::GenerateInteriorLayout(LaneInterior::MakeFrontageInput(row,15,300));
        for(const auto& o:layout.Openings) if(o.DoorId==row.EntranceDoorId)
        {
            const auto* entrance=FindNeighbourhoodFeature(p,row.EntranceDoorId);
            const R71Point threshold{row.OriginXCm+(o.Plan.U0+o.Plan.U1)/2,y};
            if(!entrance || entrance->Geometry!=std::vector<R71Point>{threshold} || entrance->RelatedId!=row.EntranceDoorId)
                errors.push_back("frontage.threshold:"+std::string(row.EntranceDoorId));
        }
    }
    for(const char* id:{"door.nabapara.home_front","door.nabapara.care_front","door.nabapara.caretaker_flat_front",
        "door.nabapara.tea_row_house_front","door.nabapara.old_house_front","door.nabapara.tall_flats_unit_a",
        "door.nabapara.tall_flats_unit_b","door.nabapara.tall_flats_upper_a"})
        if(std::find(p.HomeEntrances.begin(),p.HomeEntrances.end(),id)==p.HomeEntrances.end()) errors.push_back("home.missing:"+std::string(id));
    return errors;
}
} // namespace PerfectWorld
