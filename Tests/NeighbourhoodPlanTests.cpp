#include "NeighbourhoodPlan.h"

#include "Doors.h"
#include "LaneFrontageCatalogue.h"
#include "LaneTallFlatsLayout.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <set>

using namespace PerfectWorld;
namespace
{
int R71Assertions=0;
void R71Expect(bool value,const std::string& message)
{
    ++R71Assertions;
    if(!value) { std::cerr<<"FAIL: "<<message<<'\n'; std::exit(1); }
}
const NeighbourhoodFeature& R71Feature(const NeighbourhoodPlan& plan,std::string_view id)
{
    const auto* feature=FindNeighbourhoodFeature(plan,id);
    R71Expect(feature!=nullptr,"feature exists: "+std::string(id)); return *feature;
}
NeighbourhoodFeature& R71Mutable(NeighbourhoodPlan& plan,std::string_view id)
{
    for(auto& f:plan.Features) if(f.Id==id) return f;
    R71Expect(false,"mutable feature exists"); return plan.Features[0];
}
void R71Dimensions(const NeighbourhoodPlan& p,std::string_view id,int w,int d)
{
    const auto& f=R71Feature(p,id);
    R71Expect(f.Bounds.X1-f.Bounds.X0==w && f.Bounds.Y1-f.Bounds.Y0==d,"dimensions: "+std::string(id));
}
void R71Routes(const NeighbourhoodPlan& p)
{
    R71Expect(p.HomeEntrances.size()==8,"all eight current dwelling entrances (including three flat units)");
    for(const auto& home:p.HomeEntrances)
    {
        const auto& entrance=R71Feature(p,home);
        for(auto target:{NeighbourhoodSchoolGateNodeId,NeighbourhoodMainRoadNodeId})
        {
            const auto route=RouteNeighbourhood(p,entrance.LinkNodeId,target);
            R71Expect(route.Found && route.LengthCm>0,"home route: "+home);
            R71Expect(route.NodeIds.front()==entrance.LinkNodeId && route.NodeIds.back()==target,"route endpoints");
            R71Expect(route.NodeIds.size()==route.LinkIds.size()+1,"route topology");
            std::int64_t length=0;
            for(std::size_t i=0;i<route.LinkIds.size();++i)
            {
                const auto edge=std::find_if(p.Links.begin(),p.Links.end(),[&](const auto& e){return e.Id==route.LinkIds[i];});
                R71Expect(edge!=p.Links.end(),"route edge exists");
                R71Expect((edge->From==route.NodeIds[i] && edge->To==route.NodeIds[i+1]) ||
                    (edge->To==route.NodeIds[i] && edge->From==route.NodeIds[i+1]),"route edge joins endpoints");
                length+=edge->LengthCm;
            }
            R71Expect(length==route.LengthCm,"route length is sum of authored cm");
            const auto reverse=RouteNeighbourhood(p,target,entrance.LinkNodeId);
            R71Expect(reverse.Found && reverse.LengthCm==route.LengthCm,"free spatial return route");
        }
    }
    for(const auto& node:p.Nodes) R71Expect(RouteNeighbourhood(p,node.Id,NeighbourhoodSchoolGateNodeId).Found,"entire graph connected");
    for(const auto& feature:p.Features) if(feature.Kind==NeighbourhoodFeatureKind::PopulationEntrance)
        R71Expect(RouteNeighbourhood(p,feature.LinkNodeId,NeighbourhoodSchoolGateNodeId).Found,"population boundary connected");
    const auto same=RouteNeighbourhood(p,NeighbourhoodSchoolGateNodeId,NeighbourhoodSchoolGateNodeId);
    R71Expect(same.Found && same.LengthCm==0 && same.NodeIds.size()==1 && same.LinkIds.empty(),"identity route");
    R71Expect(!RouteNeighbourhood(p,"missing",NeighbourhoodSchoolGateNodeId).Found,"unknown source refused");
    R71Expect(!RouteNeighbourhood(p,NeighbourhoodSchoolGateNodeId,"missing").Found,"unknown target refused");
    auto split=p; split.Links.clear();
    R71Expect(!RouteNeighbourhood(split,NeighbourhoodMainRoadNodeId,NeighbourhoodSchoolGateNodeId).Found,"disconnected route refused");
    // Competing routes exercise weighted shortest paths, independent of the authored map topology.
    NeighbourhoodPlan diamond;
    diamond.Nodes={{"a",{0,0}},{"b",{100,0}},{"c",{0,100}},{"d",{100,100}}};
    diamond.Links={{"ab","a","b",{},100},{"bd","b","d",{},100},{"ac","a","c",{},40},{"cd","c","d",{},40}};
    const auto cheap=RouteNeighbourhood(diamond,"a","d");
    R71Expect(cheap.Found && cheap.LengthCm==80 && cheap.NodeIds==std::vector<std::string>{"a","c","d"},"weighted shortest path");
    diamond.Links[2].LengthCm=100; diamond.Links[3].LengthCm=100;
    R71Expect(RouteNeighbourhood(diamond,"a","d").NodeIds==std::vector<std::string>{"a","b","d"},"authored-order tie");
    diamond.Links[0].LengthCm=std::numeric_limits<std::int64_t>::max();
    R71Expect(RouteNeighbourhood(diamond,"a","d").LengthCm==200,"overflow path is ignored");
}
void R71Preservation(const NeighbourhoodPlan& p)
{
    R71Expect(R71Feature(p,"street.nabapara.lane.carriageway").Bounds==NeighbourhoodRect{-2400,-250,6400,250},"frozen lane extent");
    R71Expect(R71Feature(p,"street.nabapara.lane").Geometry==std::vector<NeighbourhoodPoint>{{-2400,0,-200},{6400,0,-200}},"lane centreline and datum");
    for(const auto& row:LaneInterior::KitFrontageEntries())
    {
        const auto& f=R71Feature(p,row.BuildingId);
        const int front=row.Side*row.FrontAbsYCm,back=front+row.Side*row.DepthCm;
        R71Expect(f.Bounds==NeighbourhoodRect{row.OriginXCm,std::min(front,back),row.OriginXCm+row.WidthCm,std::max(front,back)},"catalogue footprint untouched");
        R71Expect(f.Existing && f.Storeys==row.Storeys,"catalogue metadata untouched");
        const auto layout=LaneInterior::GenerateInteriorLayout(LaneInterior::MakeFrontageInput(row,15,300));
        const auto opening=std::find_if(layout.Openings.begin(),layout.Openings.end(),[&](const auto& o){return o.DoorId==row.EntranceDoorId;});
        R71Expect(opening!=layout.Openings.end(),"existing opening resolves");
        const auto& entrance=R71Feature(p,row.EntranceDoorId);
        R71Expect(entrance.RelatedId==row.EntranceDoorId && entrance.Geometry[0].X==row.OriginXCm+(opening->Plan.U0+opening->Plan.U1)/2 &&
            entrance.Geometry[0].Y==front,"door id and exterior threshold match actual mirrored opening");
    }
    R71Expect(R71Feature(p,"door.nabapara.home_front").Geometry[0]==NeighbourhoodPoint{400,-400},"opening-home front centre");
    R71Expect(R71Feature(p,"door.nabapara.care_front").Geometry[0]==NeighbourhoodPoint{-1165,-400},"care-home front centre");
    const auto flats=LaneTallFlats::GenerateLayout();
    R71Expect(flats.Ok(),"existing flats layout resolves");
    for(const auto* layout:{&flats.Ground,&flats.Upper}) for(const auto& o:layout->Openings)
    {
        if(o.DoorId.empty() || o.DoorId=="door.nabapara.tall_flats_front") continue;
        const auto& entrance=R71Feature(p,o.DoorId);
        R71Expect(entrance.Geometry[0]==NeighbourhoodPoint{-1300+(o.Plan.U0+o.Plan.U1)/2,460+(o.Plan.V0+o.Plan.V1)/2,layout==&flats.Upper?115:-185},"unit door transform");
    }
    for(const auto& home:p.HomeEntrances) R71Expect(FindDoor(home)!=nullptr,"home id belongs to original door roster");
    R71Expect(R71Feature(p,"building.nabapara.west_end_block").Bounds==NeighbourhoodRect{-3300,-1300,-2400,1300},"west end mass retained");
    R71Expect(R71Feature(p,"building.nabapara.east_end_block").Bounds==NeighbourhoodRect{6400,-1300,7300,1300},"east end mass retained");
}
void R71Spatial(const NeighbourhoodPlan& p)
{
    R71Expect(R71Feature(p,"street.nabapara.cross.carriageway").Bounds==NeighbourhoodRect{2920,250,3280,4200},"cross street fits alley with two 120 cm paths");
    R71Expect(R71Feature(p,"street.nabapara.para.carriageway").Bounds==NeighbourhoodRect{-4000,3800,16000,4600},"para road 800 cm");
    R71Expect(R71Feature(p,"street.nabapara.main.carriageway").Bounds==NeighbourhoodRect{15300,-6000,16700,14000},"main road 1400 cm");
    R71Expect(R71Feature(p,"site.nabapara.school").Bounds==NeighbourhoodRect{5000,4850,12000,11500},"school campus extent");
    R71Dimensions(p,"school.nabapara.forecourt",6000,1200);
    R71Dimensions(p,"building.nabapara.school_teaching",2500,4500);
    R71Dimensions(p,"school.nabapara.playground",3400,4500);
    R71Expect(R71Feature(p,"building.nabapara.school_teaching").Storeys==3,"three-storey teaching block");
    R71Expect(R71Feature(p,NeighbourhoodSchoolGateId).WidthCm==400,"400 cm school gate");
    R71Expect(NeighbourhoodSchoolContentLocationId=="loc_nhss" && NeighbourhoodSchoolScheduleLocationId=="location.nabapara.school","explicit school id bridge");
    for(const auto& f:p.Features)
    {
        if(f.Kind==NeighbourhoodFeatureKind::VehicleLane)
        {
            const auto* centre=FindNeighbourhoodFeature(p,f.ParentId);
            R71Expect(centre && centre->Kind==NeighbourhoodFeatureKind::CentreLine,"lane parent is a street");
            const auto u=f.Geometry[0],v=f.Geometry[1],c=centre->Geometry[0];
            const auto cross=std::int64_t(v.X-u.X)*(u.Y-c.Y)-std::int64_t(v.Y-u.Y)*(u.X-c.X);
            R71Expect(cross>0,"directed vehicle lane lies left of its direction of travel");
        }
        if(f.Kind==NeighbourhoodFeatureKind::StopLine)
        {
            const auto& lane=R71Feature(p,f.ParentId); const auto& crossing=R71Feature(p,f.RelatedId);
            R71Expect(lane.Kind==NeighbourhoodFeatureKind::VehicleLane && crossing.Kind==NeighbourhoodFeatureKind::Crossing,"stop-line references");
            const auto u=lane.Geometry[0],v=lane.Geometry[1];
            const auto ahead=std::int64_t(v.X-u.X)*(crossing.Bounds.X0+crossing.Bounds.X1-f.Bounds.X0-f.Bounds.X1)+
                std::int64_t(v.Y-u.Y)*(crossing.Bounds.Y0+crossing.Bounds.Y1-f.Bounds.Y0-f.Bounds.Y1);
            R71Expect(ahead>0,"stop line precedes crossing in travel direction");
        }
        if(f.Kind==NeighbourhoodFeatureKind::Footpath) for(const auto& road:p.Features)
            if(road.Kind==NeighbourhoodFeatureKind::Carriageway)
                R71Expect(!(f.Bounds.X0<road.Bounds.X1 && road.Bounds.X0<f.Bounds.X1 && f.Bounds.Y0<road.Bounds.Y1 && road.Bounds.Y0<f.Bounds.Y1),"raised footpath excludes junction traffic: "+f.Id+":"+road.Id);
    }
}
void R71Queries(const NeighbourhoodPlan& p)
{
    R71Expect(!FindNeighbourhoodFeature(p,"not-authored"),"unknown feature returns null");
    R71Expect(NeighbourhoodFeaturesInRect(p,{10,0,0,10}).empty(),"inverted rectangle refused");
    R71Expect(NeighbourhoodFeaturesInRect(p,{40000,40000,50000,50000}).empty(),"distant rectangle empty");
    const auto gate=NeighbourhoodFeaturesInRect(p,{6500,4850,6500,4850});
    R71Expect(std::any_of(gate.begin(),gate.end(),[](auto* f){return f->Id==NeighbourhoodSchoolGateId;}),"point query includes gate on boundary");
    const auto all=NeighbourhoodFeaturesInRect(p,{-50000,-50000,50000,50000});
    R71Expect(all.size()==p.Features.size(),"full extent yields all features");
    for(std::size_t i=0;i<all.size();++i) R71Expect(all[i]==&p.Features[i],"query preserves authored order and ownership");
    for(const auto& f:p.Features) if(f.Kind==NeighbourhoodFeatureKind::Footprint)
        R71Expect(NeighbourhoodReservesRect(p,f.Bounds),"every footprint reserved from surrounding generator");
    R71Expect(NeighbourhoodReservesRect(p,{3000,1800,3200,2000}),"cross street excludes old decorative rows");
    R71Expect(NeighbourhoodReservesRect(p,{7600,5000,8700,6000}),"east skyline excludes campus");
    R71Expect(!NeighbourhoodReservesRect(p,{30000,30000,30800,30800}),"surrounding district remains available");
    R71Expect(!NeighbourhoodReservesRect(p,{10,10,0,0}) && !NeighbourhoodReservesRect(p,{0,0,0,0}),"invalid/zero area reservation request refused");
    int reserved=0,free=0;
    for(int y=-8000;y<16000;y+=800) for(int x=-4800;x<18400;x+=800)
        (NeighbourhoodReservesRect(p,{x,y,x+800,y+800})?reserved:free)++;
    R71Expect(reserved>0 && free>0,"8 m generated lots can be filtered without snapping authored geometry");
}
void R71Refusals(const NeighbourhoodPlan& p)
{
    auto bad=p; bad.Features.push_back(bad.Features[0]); R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"duplicate feature refused");
    bad=p; bad.Nodes.push_back(bad.Nodes[0]); R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"duplicate node refused");
    bad=p; bad.Links[0].LengthCm++; R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"incorrect edge length refused");
    bad=p; bad.Links[0].To="missing"; R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"dangling edge refused");
    bad=p; bad.Links.clear(); R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"disconnected graph refused");
    bad=p; bad.HomeEntrances.clear(); R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"missing homes refused");
    bad=p; R71Mutable(bad,"street.nabapara.lane.carriageway").Bounds.X0--; R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"lane drift refused");
    bad=p; auto duplicate=R71Feature(p,"building.nabapara.pharmacy"); duplicate.Id="test.overlapping_footprint"; bad.Features.push_back(duplicate);
    R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"overlapping footprints refused");
    bad=p; bad.Nodes.push_back({"isolated",{30000,30000}}); R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"isolated boundary node refused");
    bad=p; R71Mutable(bad,"stop.nabapara.main_south").RelatedId="missing"; R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"dangling crossing refused");
    bad=p; R71Mutable(bad,"stop.nabapara.main_south").ParentId="street.nabapara.main";
    R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"stop line requires a vehicle lane parent");
    bad=p; R71Mutable(bad,"stop.nabapara.main_south").RelatedId="crossing.nabapara.para";
    R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"stop line and crossing must share a street");
    bad=p;
    for(auto& e:bad.Links) if(e.ConstraintId=="crossing.nabapara.cross_mouth") e.ConstraintId="crossing.nabapara.cross_north";
    R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"crossing link must pass through its actual crossing polygon");
    bad=p; R71Mutable(bad,NeighbourhoodSchoolGateId).LinkNodeId="missing"; R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"unlinked entrance refused");
    bad=p;
    for(auto& e:bad.Links) if(e.Kind==NeighbourhoodLinkKind::Crossing) { e.Kind=NeighbourhoodLinkKind::Walk; break; }
    R71Expect(!ValidateNeighbourhoodPlan(bad).empty(),"unmarked road crossing refused");
}
} // namespace
int main()
{
    const auto plan=BuildNeighbourhoodPlan(71);
    const auto errors=ValidateNeighbourhoodPlan(plan);
    for(const auto& e:errors) std::cerr<<e<<'\n';
    R71Expect(errors.empty(),"authored plan passes spatial validation");
    R71Preservation(plan); R71Spatial(plan); R71Routes(plan); R71Queries(plan); R71Refusals(plan);
    for(auto seed:{std::uint64_t{0},std::uint64_t{71},std::numeric_limits<std::uint64_t>::max()})
    {
        auto first=BuildNeighbourhoodPlan(seed); const auto second=BuildNeighbourhoodPlan(seed);
        R71Expect(first==second,"same seed reconstructs all fields exactly");
        R71Expect(ValidateNeighbourhoodPlan(first).empty(),"every seed has valid geometry");
        first.Seed=plan.Seed; R71Expect(first==plan,"seed cannot move authored identities or geometry");
    }
    std::cout<<"R71: "<<R71Assertions<<" assertions; "<<plan.Features.size()<<" features, "<<plan.Nodes.size()<<" nodes, "<<plan.Links.size()<<" links\n";
    return 0;
}
