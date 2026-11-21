#include "NeighbourhoodTraffic.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <set>

using namespace PerfectWorld;
namespace
{
std::uint64_t R72Assertions=0;
constexpr NeighbourhoodPoint R72Centre{8000,4000,-200};
constexpr int R72Radius=50000;
void R72Expect(bool value,const char* message)
{
    ++R72Assertions;
    if(!value) { std::cerr<<"R72 FAIL: "<<message<<'\n'; std::exit(1); }
}
NeighbourhoodTrafficTime R72Time(int ms,GameInstant day=0)
{
    return {day*MinutesPerDay+ms/60000,ms%60000};
}
const NeighbourhoodFeature& R72Feature(const NeighbourhoodPlan& plan,std::string_view id)
{
    const auto* result=FindNeighbourhoodFeature(plan,id);
    R72Expect(result!=nullptr,"spatial reference exists"); return *result;
}
NeighbourhoodRect R72Bounds(const NeighbourhoodVehicle& v,const NeighbourhoodFeature& lane)
{
    const bool horizontal=lane.Geometry[0].Y==lane.Geometry[1].Y;
    const int x=horizontal?v.LengthCm/2:v.WidthCm/2,y=horizontal?v.WidthCm/2:v.LengthCm/2;
    return {v.Position.X-x,v.Position.Y-y,v.Position.X+x,v.Position.Y+y};
}
bool R72Overlap(NeighbourhoodRect a,NeighbourhoodRect b)
{
    return a.X0<b.X1 && b.X0<a.X1 && a.Y0<b.Y1 && b.Y0<a.Y1;
}
void R72Safety(const NeighbourhoodPlan& plan,const std::vector<NeighbourhoodVehicle>& vehicles)
{
    R72Expect(vehicles.size()<=12,"actor-cap upper bound");
    std::set<std::string> ids;
    for(const auto& v:vehicles)
    {
        const auto& lane=R72Feature(plan,v.LaneId);
        R72Expect(ids.insert(v.Id).second,"unique transient vehicle id");
        R72Expect(lane.StreetClass==NeighbourhoodStreetClass::MainRoad || lane.StreetClass==NeighbourhoodStreetClass::ParaRoad,
            "traffic excludes the narrow lane and alley");
        R72Expect(v.Kind!=NeighbourhoodVehicleKind::FictionalTram || lane.StreetClass==NeighbourhoodStreetClass::MainRoad,
            "fictional tram stays on main corridor");
        R72Expect(v.Kind!=NeighbourhoodVehicleKind::Bus || lane.StreetClass==NeighbourhoodStreetClass::MainRoad,"main-road bus service");
        const auto body=R72Bounds(v,lane);
        const auto& road=R72Feature(plan,lane.ParentId+".carriageway");
        R72Expect(body.X0>=road.Bounds.X0 && body.Y0>=road.Bounds.Y0 && body.X1<=road.Bounds.X1 && body.Y1<=road.Bounds.Y1,
            "whole body remains on carriageway");
        R72Expect(v.WidthCm<=lane.WidthCm && v.AlongLaneCm>=v.LengthCm/2,"body fits directed lane");
        const auto start=lane.Geometry[0],end=lane.Geometry[1];
        const int dx=(end.X>start.X)-(end.X<start.X),dy=(end.Y>start.Y)-(end.Y<start.Y);
        R72Expect(v.Position==NeighbourhoodPoint{start.X+dx*v.AlongLaneCm,start.Y+dy*v.AlongLaneCm,start.Z},"pose follows directed lane");
        R72Expect(v.SpeedCmPerSecond>=0 && v.SpeedCmPerSecond<=700,"bounded nonnegative speed");
    }
    for(std::size_t i=0;i<vehicles.size();++i) for(std::size_t j=i+1;j<vehicles.size();++j)
    {
        const auto& a=vehicles[i]; const auto& b=vehicles[j];
        R72Expect(!R72Overlap(R72Bounds(a,R72Feature(plan,a.LaneId)),R72Bounds(b,R72Feature(plan,b.LaneId))),
            "no vehicle body overlap, including T-junction");
        if(a.LaneId==b.LaneId)
            R72Expect(std::abs(a.AlongLaneCm-b.AlongLaneCm)-(a.LengthCm+b.LengthCm)/2>=NeighbourhoodTrafficGapCm,"minimum following gap");
    }
}
void R72Headways()
{
    for(auto street:{NeighbourhoodStreetClass::MainRoad,NeighbourhoodStreetClass::ParaRoad})
    {
        R72Expect(NeighbourhoodTrafficHeadwayMs(street,7*60)==NeighbourhoodTrafficHeadwayMs(street,17*60),"both rush bands");
        R72Expect(NeighbourhoodTrafficHeadwayMs(street,8*60)<NeighbourhoodTrafficHeadwayMs(street,12*60),"rush headway shorter than day");
        R72Expect(NeighbourhoodTrafficHeadwayMs(street,12*60)<NeighbourhoodTrafficHeadwayMs(street,23*60),"night headway longer than day");
        R72Expect(NeighbourhoodTrafficHeadwayMs(street,5*60)==NeighbourhoodTrafficHeadwayMs(street,20*60+59),"day boundaries");
        R72Expect(NeighbourhoodTrafficHeadwayMs(street,10*60)==NeighbourhoodTrafficHeadwayMs(street,16*60+59),"midday band");
        R72Expect(NeighbourhoodTrafficHeadwayMs(street,4*60+59)==NeighbourhoodTrafficHeadwayMs(street,21*60),"night boundaries");
    }
    R72Expect(NeighbourhoodTrafficHeadwayMs(NeighbourhoodStreetClass::Lane,480)==0,"no narrow-lane arrivals");
    R72Expect(NeighbourhoodTrafficHeadwayMs(NeighbourhoodStreetClass::CrossStreet,480)==0,"no alley arrivals");
    R72Expect(NeighbourhoodTrafficHeadwayMs(NeighbourhoodStreetClass::MainRoad,-1)==0,"invalid time refused");
}
void R72DayRun(const NeighbourhoodPlan& plan)
{
    NeighbourhoodTraffic traffic(plan,72);
    std::map<std::string,int> firstSeen;
    std::map<std::string,int> completed;
    std::map<std::string,std::string> lanes;
    std::array<int,24> arrivals{};
    std::array<bool,5> kinds{};
    bool bus=false,junction=false;
    int departures=0,previousHourDepartures=0;
    for(int minute=0;minute<=1440;++minute)
    {
        const auto vehicles=traffic.VehiclesAt({minute,0},R72Centre,R72Radius);
        R72Safety(plan,vehicles);
        std::set<std::string> present;
        for(const auto& v:vehicles)
        {
            present.insert(v.Id); lanes[v.Id]=v.LaneId;
            if(firstSeen.emplace(v.Id,minute).second && minute<1440) ++arrivals[minute/60];
            R72Expect(minute-firstSeen[v.Id]<15,"every observed vehicle clears within the finite drain bound");
            kinds[static_cast<std::size_t>(v.Kind)]=true;
            bus|=v.State==NeighbourhoodVehicleState::DwellingAtBusStop;
            junction|=v.State==NeighbourhoodVehicleState::HoldingJunction;
        }
        for(auto it=firstSeen.begin();it!=firstSeen.end();)
        {
            if(present.contains(it->first)) { ++it; continue; }
            ++departures; ++completed[lanes[it->first]]; lanes.erase(it->first); it=firstSeen.erase(it);
        }
        if(minute>0 && minute%60==0)
        {
            R72Expect(departures>previousHourDepartures,"vehicles complete routes in every hour, including sparse night service");
            previousHourDepartures=departures;
        }
    }
    R72Expect(firstSeen.empty(),"daily drain is genuinely empty before reconstruction");
    for(bool seen:kinds) R72Expect(seen,"all five requested vehicle kinds appear");
    R72Expect(bus && junction,"dwell and junction states exercised by minute samples");
    R72Expect(completed.size()==4,"all four approaches complete vehicles");
    for(const auto& [lane,count]:completed)
    {
        std::cout<<"R72 completed "<<lane<<": "<<count<<'\n';
        R72Expect(count>80,"no approach starves over 24 hours");
    }
    const int rush=arrivals[7]+arrivals[8]+arrivals[9],day=arrivals[11]+arrivals[12]+arrivals[13];
    const int night=arrivals[1]+arrivals[2]+arrivals[3];
    R72Expect(rush>day && day>night,"observed seeded traffic follows demand bands despite admission throttling");
    std::cout<<"R72 observed three-hour arrivals: rush="<<rush<<" day="<<day<<" night="<<night<<'\n';
}
void R72SignalsAndBuses(const NeighbourhoodPlan& plan)
{
    NeighbourhoodTraffic traffic(plan,73);
    const int start=7*60*60000,end=9*60*60000;
    traffic.Reconstruct(R72Time(start));
    struct R72Dwell { int First=0,Last=0; bool Released=false; };
    std::map<std::string,R72Dwell> dwell;
    std::set<std::string> stopLanes,dwellLanes;
    std::map<std::string,NeighbourhoodVehicle> previous;
    for(int ms=start;ms<end;ms+=100)
    {
        const auto time=R72Time(ms);
        const auto vehicles=traffic.VehiclesAt(time,R72Centre,R72Radius);
        if(ms%1000==0) R72Safety(plan,vehicles);
        for(const auto& v:vehicles)
        {
            const auto& lane=R72Feature(plan,v.LaneId);
            const auto body=R72Bounds(v,lane);
            for(const auto& stop:plan.Features) if(stop.Kind==NeighbourhoodFeatureKind::StopLine && stop.ParentId==v.LaneId)
            {
                const auto& crossing=R72Feature(plan,stop.RelatedId);
                const auto phase=traffic.CrossingPhaseAt(crossing.Id,time);
                if(phase!=NeighbourhoodCrossingPhase::Drive)
                    R72Expect(!R72Overlap(body,crossing.Bounds),"full vehicle clears before walk/clearance");
                if(v.State==NeighbourhoodVehicleState::StoppedAtStopLine)
                {
                    const auto a=lane.Geometry[0],b=lane.Geometry[1],p=stop.Geometry[0];
                    const int stopCm=(p.X-a.X)*((b.X>a.X)-(b.X<a.X))+(p.Y-a.Y)*((b.Y>a.Y)-(b.Y<a.Y));
                    R72Expect(v.AlongLaneCm+v.LengthCm/2<=stopCm,"front respects authored stop line");
                    if(v.AlongLaneCm+v.LengthCm/2==stopCm) stopLanes.insert(v.LaneId);
                }
            }
            if(v.State==NeighbourhoodVehicleState::DwellingAtBusStop)
            {
                R72Expect(v.Kind==NeighbourhoodVehicleKind::Bus && v.SpeedCmPerSecond==0,"bus dwell is stationary");
                const auto& stop=R72Feature(plan,v.LaneId.ends_with("forward")?"stop.nabapara.bus_west":"stop.nabapara.bus_east");
                const int rearY=v.LaneId.ends_with("forward")?body.Y0:body.Y1;
                R72Expect(rearY==stop.Geometry[0].Y,"bus rear docks at authored stop anchor");
                dwellLanes.insert(v.LaneId);
                auto [it,inserted]=dwell.emplace(v.Id,R72Dwell{ms,ms,false});
                if(!inserted) { R72Expect(!it->second.Released,"one dwell per traversal"); it->second.Last=ms; }
            }
            else if(auto it=dwell.find(v.Id);it!=dwell.end() && !it->second.Released)
            {
                R72Expect(ms-it->second.First==NeighbourhoodBusDwellMs,"exact full bus dwell duration"); it->second.Released=true;
            }
            if(auto it=previous.find(v.Id);it!=previous.end())
                R72Expect(v.AlongLaneCm>=it->second.AlongLaneCm && v.AlongLaneCm-it->second.AlongLaneCm<=70,"continuous forward motion, no rollback");
        }
        previous.clear(); for(const auto& v:vehicles) previous.emplace(v.Id,v);
    }
    R72Expect(stopLanes.size()==4,"each direction obeys its own stop line");
    R72Expect(dwellLanes.size()==2,"buses serve both authored stops");
    R72Expect(std::count_if(dwell.begin(),dwell.end(),[](const auto& item){return item.second.Released;})>10,"repeated completed bus dwells");
}
void R72Pedestrians(const NeighbourhoodPlan& plan)
{
    std::set<std::string> exercised;
    for(int minute=420;minute<480 && exercised.size()<4;++minute)
    {
        NeighbourhoodTraffic traffic(plan,74);
        const auto vehicles=traffic.VehiclesAt({minute,0},R72Centre,R72Radius);
        for(const auto& v:vehicles)
        {
            if(exercised.contains(v.LaneId) || v.State!=NeighbourhoodVehicleState::Moving) continue;
            const auto& lane=R72Feature(plan,v.LaneId);
            const auto a=lane.Geometry[0],b=lane.Geometry[1];
            const int dx=(b.X>a.X)-(b.X<a.X),dy=(b.Y>a.Y)-(b.Y<a.Y);
            const int front=v.AlongLaneCm+v.LengthCm/2;
            const int x=a.X+dx*(front+15),y=a.Y+dy*(front+15);
            const NeighbourhoodPedestrianOccupancy pedestrian{dx!=0?NeighbourhoodRect{x,y-150,x,y+150}:NeighbourhoodRect{x-150,y,x+150,y}};
            const std::array occupancy{pedestrian};
            // Thin footprint is between two otherwise clear endpoints: requires a swept test.
            traffic.AdvanceTo({minute,2000},occupancy);
            const auto held=traffic.VehiclesNear(R72Centre,R72Radius);
            const auto it=std::find_if(held.begin(),held.end(),[&](const auto& item){return item.Id==v.Id;});
            R72Expect(it!=held.end(),"occupied approach retains its vehicle");
            R72Expect(it->AlongLaneCm<=v.AlongLaneCm+14 && !R72Overlap(R72Bounds(*it,lane),pedestrian.Bounds),"swept body cannot enter pedestrian footprint");
            R72Expect(it->State==NeighbourhoodVehicleState::YieldingToPedestrians && it->SpeedCmPerSecond==0,"pedestrian yielding state");
            traffic.AdvanceTo({minute+1,0},occupancy);
            const auto later=traffic.VehiclesNear(R72Centre,R72Radius);
            const auto waiting=std::find_if(later.begin(),later.end(),[&](const auto& item){return item.Id==v.Id;});
            R72Expect(waiting!=later.end() && waiting->AlongLaneCm==it->AlongLaneCm,"continued occupancy holds without pushing");
            traffic.AdvanceTo({minute+14,0});
            const auto cleared=traffic.VehiclesNear(R72Centre,R72Radius);
            R72Expect(std::none_of(cleared.begin(),cleared.end(),[&](const auto& item){return item.Id==v.Id;}),"released pedestrian cannot leave junction deadlocked");
            exercised.insert(v.LaneId); break;
        }
    }
    R72Expect(exercised.size()==4,"occupancy exercised in all four lane directions");
}
void R72Reconstruction(const NeighbourhoodPlan& plan)
{
    NeighbourhoodTraffic incremental(plan,75),other(plan,75),different(plan,76);
    for(int minute:{0,30,419,480,720,1080,1439})
    {
        const NeighbourhoodTrafficTime time{minute,23700};
        const auto a=incremental.VehiclesAt(time,R72Centre,R72Radius);
        other.Reconstruct(time);
        R72Expect(a==other.VehiclesNear(R72Centre,R72Radius),"incremental and fresh reconstruction are identical");
        R72Expect(a==incremental.VehiclesAt(time,R72Centre,R72Radius),"same-time query is idempotent");
    }
    const auto a=incremental.VehiclesAt({480,23700},R72Centre,R72Radius);
    other.Reconstruct({480,23700});
    R72Expect(a==other.VehiclesNear(R72Centre,R72Radius),"backward seek replays exact baseline");
    R72Expect(a!=different.VehiclesAt({480,23700},R72Centre,R72Radius),"seed changes transient traffic");
    const auto nextDay=incremental.VehiclesAt({1440+480,23700},R72Centre,R72Radius);
    R72Expect(a!=nextDay,"date changes minute-window identity and traffic");
    other.Reconstruct({1440+480,23799});
    R72Expect(nextDay==other.VehiclesNear(R72Centre,R72Radius),"sub-step samples round down deterministically");
    R72Expect(incremental.ActorCapHint()==12,"presenter cap hint");
    R72Expect(incremental.VehiclesNear(R72Centre,-1).empty(),"negative radius refused");
    if(!nextDay.empty())
        R72Expect(!incremental.VehiclesNear(nextDay[0].Position,0).empty(),"zero radius includes vehicle body");
    R72Expect(incremental.VehiclesNear({std::numeric_limits<int>::min(),std::numeric_limits<int>::min()},
        std::numeric_limits<int>::max()).empty(),"extreme query has no arithmetic overflow");
    R72Expect(incremental.CrossingPhaseAt("missing",{480,0})==NeighbourhoodCrossingPhase::Unknown,"unknown crossing refused");
    for(const auto& f:plan.Features) if(f.Kind==NeighbourhoodFeatureKind::Crossing)
    {
        std::set<NeighbourhoodCrossingPhase> phases;
        for(int ms=0;ms<60000;ms+=100) phases.insert(incremental.CrossingPhaseAt(f.Id,{480,ms}));
        R72Expect(phases.size()==3,"every authored crossing has walk, clearance and drive");
    }
    R72Expect(incremental.VehiclesAt({-1,0},R72Centre,R72Radius).empty(),"negative minute refused");
    R72Expect(incremental.VehiclesAt({1,60000},R72Centre,R72Radius).empty(),"invalid sub-minute refused");
    NeighbourhoodTraffic empty({},0);
    R72Expect(empty.VehiclesAt({480,0},R72Centre,R72Radius).empty(),"missing spatial geometry fails closed");
    NeighbourhoodTraffic extreme(plan,std::numeric_limits<std::uint64_t>::max());
    const auto far=extreme.VehiclesAt({std::numeric_limits<GameInstant>::max(),0},R72Centre,R72Radius);
    R72Safety(plan,far);
    const auto extremeCopy=extreme.VehiclesAt({std::numeric_limits<GameInstant>::max(),0},R72Centre,R72Radius);
    R72Expect(far==extremeCopy,"largest seed and minute reconstruct without overflow");
}
void R72FrameInputs(const NeighbourhoodPlan& plan)
{
    NeighbourhoodTraffic a(plan,77),b(plan,77);
    a.Reconstruct({480,0}); b.Reconstruct({480,0});
    std::vector<NeighbourhoodPedestrianOccupancy> paths;
    for(const auto& f:plan.Features) if(f.Kind==NeighbourhoodFeatureKind::Footpath) paths.push_back({f.Bounds});
    a.AdvanceTo({485,0},paths);
    for(int ms=480*60000+1000;ms<=485*60000;ms+=1000) b.AdvanceTo(R72Time(ms));
    R72Expect(a.VehiclesNear(R72Centre,R72Radius)==b.VehiclesNear(R72Centre,R72Radius),
        "frame partition and pedestrians safely on footpaths cannot change traffic");
    const auto full=a.VehiclesNear(R72Centre,R72Radius);
    R72Expect(!full.empty(),"query fixture contains traffic");
    R72Expect(a.VehiclesAt({485,0},{-1000000,-1000000},1).empty(),"far radius culls presentation only");
    R72Expect(a.VehiclesNear(R72Centre,R72Radius)==full,"culling does not erase offscreen vehicles");
    const std::array occupied{NeighbourhoodPedestrianOccupancy{{-10000,-10000,30000,30000}}};
    NeighbourhoodTraffic loaded(plan,77);
    R72Expect(loaded.VehiclesAt({485,0},R72Centre,R72Radius,occupied).empty(),"load projection cannot appear inside supplied occupancy");
    loaded.AdvanceTo({486,0},occupied);
    R72Expect(loaded.VehiclesNear(R72Centre,R72Radius).empty(),"spawn footprints respect occupancy too");
    loaded.Reconstruct({485,0});
    R72Expect(loaded.VehiclesNear(R72Centre,R72Radius)==full,"live pedestrian history is transient on reconstruction");
    for(auto suffix:{"forward","reverse"})
    {
        const auto& main=R72Feature(plan,std::string("street.nabapara.main.vehicle_")+suffix);
        const auto& para=R72Feature(plan,std::string("street.nabapara.para.vehicle_")+suffix);
        const bool forward=std::string_view(suffix)=="forward";
        R72Expect(forward?main.Geometry[0].X<16000:main.Geometry[0].X>16000,"main road runs on the left");
        R72Expect(forward?para.Geometry[0].Y>4200:para.Geometry[0].Y<4200,"para road runs on the left");
    }
}
} // namespace
int main()
{
    const auto plan=BuildNeighbourhoodPlan(72);
    R72Headways(); R72DayRun(plan); R72SignalsAndBuses(plan); R72Pedestrians(plan); R72Reconstruction(plan); R72FrameInputs(plan);
    R72Expect(plan==BuildNeighbourhoodPlan(72),"traffic never mutates authored plan");
    std::cout<<"R72: "<<R72Assertions<<" assertions passed\n";
    return 0;
}
