#include "NeighbourhoodTraffic.h"

#include <algorithm>
#include <array>
#include <utility>

namespace PerfectWorld
{
namespace
{
std::uint64_t R72Mix(std::uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
bool R72Valid(NeighbourhoodTrafficTime time)
{
    return time.Minute >= 0 && time.Millisecond >= 0 && time.Millisecond < 60000;
}
int R72DayMs(NeighbourhoodTrafficTime time)
{
    return static_cast<int>(time.Minute % MinutesPerDay) * 60000 + time.Millisecond / 100 * 100;
}
int R72PhaseMs(std::uint64_t seed, int dayMs)
{
    return (dayMs + static_cast<int>(R72Mix(seed) % 600) * 100) % 60000;
}
NeighbourhoodRect R72Body(const NeighbourhoodVehicle& v, bool horizontal)
{
    const int x = horizontal ? v.LengthCm / 2 : v.WidthCm / 2;
    const int y = horizontal ? v.WidthCm / 2 : v.LengthCm / 2;
    return {v.Position.X-x,v.Position.Y-y,v.Position.X+x,v.Position.Y+y};
}
bool R72Intersects(NeighbourhoodRect a, NeighbourhoodRect b)
{
    return b.X0 <= b.X1 && b.Y0 <= b.Y1 && a.X0 <= b.X1 && b.X0 <= a.X1 && a.Y0 <= b.Y1 && b.Y0 <= a.Y1;
}
int R72Project(NeighbourhoodPoint origin, int dx, int dy, int x, int y)
{
    return (x-origin.X)*dx + (y-origin.Y)*dy;
}
std::pair<int,int> R72Interval(NeighbourhoodPoint origin, int dx, int dy, NeighbourhoodRect rect)
{
    const int a=R72Project(origin,dx,dy,rect.X0,rect.Y0), b=R72Project(origin,dx,dy,rect.X1,rect.Y1);
    return {std::min(a,b),std::max(a,b)};
}
void R72Pose(NeighbourhoodVehicle& vehicle, NeighbourhoodPoint start, int dx, int dy)
{
    vehicle.Position = {start.X+dx*vehicle.AlongLaneCm,start.Y+dy*vehicle.AlongLaneCm,start.Z};
}
} // namespace

int NeighbourhoodTrafficHeadwayMs(NeighbourhoodStreetClass street, GameInstant minute)
{
    if(minute<0 || (street!=NeighbourhoodStreetClass::MainRoad && street!=NeighbourhoodStreetClass::ParaRoad)) return 0;
    const int hour=static_cast<int>(minute%MinutesPerDay)/60;
    const bool rush=(hour>=7 && hour<10) || (hour>=17 && hour<20);
    const bool night=hour<5 || hour>=21;
    return (night?60000:rush?12000:24000) * (street==NeighbourhoodStreetClass::ParaRoad?2:1);
}

NeighbourhoodTraffic::NeighbourhoodTraffic(const NeighbourhoodPlan& plan,std::uint64_t worldSeed) : Seed_(worldSeed)
{
    constexpr std::array<std::string_view,4> R72LaneIds{
        "street.nabapara.main.vehicle_forward","street.nabapara.main.vehicle_reverse",
        "street.nabapara.para.vehicle_forward","street.nabapara.para.vehicle_reverse"};
    for(const auto& feature:plan.Features)
        if(feature.Kind==NeighbourhoodFeatureKind::Crossing) CrossingIds_.push_back(feature.Id);
    for(auto id:R72LaneIds)
    {
        const auto* f=FindNeighbourhoodFeature(plan,id);
        if(!f || f->Kind!=NeighbourhoodFeatureKind::VehicleLane || f->Geometry.size()!=2 || f->WidthCm<250) break;
        const auto a=f->Geometry[0],b=f->Geometry[1];
        if((a.X==b.X)==(a.Y==b.Y)) break;
        Lane lane; lane.Id=f->Id; lane.Start=a; lane.WidthCm=f->WidthCm; lane.Street=f->StreetClass;
        lane.Dx=(b.X>a.X)-(b.X<a.X); lane.Dy=(b.Y>a.Y)-(b.Y<a.Y);
        lane.LengthCm=R72Project(a,lane.Dx,lane.Dy,b.X,b.Y);
        // Main-road T-junction, crossing and both bus stops share ONE fair reservation.
        const auto region=R72Interval(a,lane.Dx,lane.Dy,{15300,3700,16700,7200});
        lane.JunctionBeginCm=region.first; lane.JunctionEndCm=region.second;
        for(const auto& stop:plan.Features)
        {
            if(stop.ParentId!=lane.Id || stop.Geometry.empty()) continue;
            if(stop.Kind==NeighbourhoodFeatureKind::BusStop)
                lane.BusCm=R72Project(a,lane.Dx,lane.Dy,stop.Geometry[0].X,stop.Geometry[0].Y);
            if(stop.Kind!=NeighbourhoodFeatureKind::StopLine) continue;
            const auto* crossing=FindNeighbourhoodFeature(plan,stop.RelatedId);
            if(!crossing || crossing->Kind!=NeighbourhoodFeatureKind::Crossing) continue;
            const auto interval=R72Interval(a,lane.Dx,lane.Dy,crossing->Bounds);
            lane.Crossings.push_back({crossing->Id,interval.first,interval.second,
                R72Project(a,lane.Dx,lane.Dy,stop.Geometry[0].X,stop.Geometry[0].Y)});
        }
        if(lane.Crossings.empty() || (lane.Street==NeighbourhoodStreetClass::MainRoad && lane.BusCm<0)) break;
        Lanes_.push_back(std::move(lane));
    }
    if(Lanes_.size()!=4) Lanes_.clear();
}

NeighbourhoodCrossingPhase NeighbourhoodTraffic::CrossingPhaseAt(std::string_view id,NeighbourhoodTrafficTime now) const
{
    if(!R72Valid(now) || std::find(CrossingIds_.begin(),CrossingIds_.end(),id)==CrossingIds_.end())
        return NeighbourhoodCrossingPhase::Unknown;
    const int phase=R72PhaseMs(Seed_,R72DayMs(now));
    return phase<16000?NeighbourhoodCrossingPhase::Walk:phase<20000?
        NeighbourhoodCrossingPhase::Clearance:NeighbourhoodCrossingPhase::Drive;
}

void NeighbourhoodTraffic::Reconstruct(NeighbourhoodTrafficTime now)
{
    Slots_.clear(); JunctionOwner_.clear(); HasPendingArrival_=false; NextLane_=0; Day_=-1; Millisecond_=-100;
    if(!R72Valid(now)) return;
    Day_=now.Minute/MinutesPerDay;
    AdvanceTo(now);
}

void NeighbourhoodTraffic::AdvanceTo(NeighbourhoodTrafficTime now,std::span<const NeighbourhoodPedestrianOccupancy> pedestrians)
{
    if(!R72Valid(now) || Lanes_.empty()) return;
    const auto day=now.Minute/MinutesPerDay;
    const int target=R72DayMs(now);
    if(day!=Day_ || target<Millisecond_)
    {
        Reconstruct(now);
        std::erase_if(Slots_,[&](const auto& s){
            const auto body=R72Body(s.Vehicle,Lanes_[s.LaneIndex].Dx!=0);
            return std::any_of(pedestrians.begin(),pedestrians.end(),[&](const auto& p){return R72Intersects(body,p.Bounds);});
        });
        return;
    }
    while(Millisecond_<target)
    {
        Millisecond_+=NeighbourhoodTrafficStepMs;
        Step(day,Millisecond_,pedestrians);
    }
}

void NeighbourhoodTraffic::Spawn(std::int64_t day,int ms,std::span<const NeighbourhoodPedestrianOccupancy> pedestrians)
{
    const int minute=ms/60000,within=ms%60000;
    for(std::size_t i=0;i<Lanes_.size();++i)
    {
        const auto& lane=Lanes_[i];
        const bool pending=HasPendingArrival_ && PendingArrival_.LaneIndex==i;
        if(!pending && (minute<15 || minute>=1425)) continue;
        const int headway=NeighbourhoodTrafficHeadwayMs(lane.Street,minute);
        const int windowMinutes=(headway+59999)/60000,windowMinute=minute-minute%windowMinutes;
        const int local=within+(minute-windowMinute)*60000,slot=local/headway;
        const auto key=R72Mix(Seed_^R72Mix(static_cast<std::uint64_t>(day))^
            R72Mix(static_cast<std::uint64_t>(windowMinute)*128+i*16+static_cast<unsigned>(slot)));
        const int offset=static_cast<int>(key%static_cast<unsigned>(headway/100))*100;
        if(!pending && local!=slot*headway+offset) continue;
        if(std::count_if(Slots_.begin(),Slots_.end(),[&](const auto& s){return s.LaneIndex==i;})>=3) continue;
        Slot s; s.LaneIndex=i;
        auto& v=s.Vehicle;
        v.Id="traffic.r72."+std::to_string(Seed_)+"."+std::to_string(day)+"."+
            std::to_string(windowMinute)+"."+std::to_string(i)+"."+std::to_string(slot);
        v.LaneId=lane.Id;
        v.Kind=static_cast<NeighbourhoodVehicleKind>((key>>32)%(lane.Street==NeighbourhoodStreetClass::MainRoad?5:3));
        if(lane.Street==NeighbourhoodStreetClass::ParaRoad && v.Kind==NeighbourhoodVehicleKind::Bus)
            v.Kind=NeighbourhoodVehicleKind::CycleRickshaw;
        switch(v.Kind)
        {
        case NeighbourhoodVehicleKind::YellowTaxi: v.LengthCm=440; v.WidthCm=170; s.CruiseCmPerSecond=700; break;
        case NeighbourhoodVehicleKind::Bus: v.LengthCm=900; v.WidthCm=250; s.CruiseCmPerSecond=600; break;
        case NeighbourhoodVehicleKind::SharedAuto: v.LengthCm=280; v.WidthCm=140; s.CruiseCmPerSecond=550; break;
        case NeighbourhoodVehicleKind::CycleRickshaw: v.LengthCm=260; v.WidthCm=110; s.CruiseCmPerSecond=350; break;
        case NeighbourhoodVehicleKind::FictionalTram: v.LengthCm=1100; v.WidthCm=240; s.CruiseCmPerSecond=600; break;
        }
        v.AlongLaneCm=v.LengthCm/2;
        R72Pose(v,lane.Start,lane.Dx,lane.Dy);
        if(pending) s=PendingArrival_;
        bool blocked=false;
        for(const auto& other:Slots_) if(other.LaneIndex==i &&
            other.Vehicle.AlongLaneCm-other.Vehicle.LengthCm/2 < v.LengthCm+NeighbourhoodTrafficGapCm) blocked=true;
        for(const auto& p:pedestrians) if(R72Intersects(R72Body(v,lane.Dx!=0),p.Bounds)) blocked=true;
        const bool junction=v.AlongLaneCm+v.LengthCm/2>=lane.JunctionBeginCm && v.AlongLaneCm-v.LengthCm/2<=lane.JunctionEndCm;
        // The reverse para lane starts inside the T: admit only when all approaches are free.
        if(junction && !JunctionOwner_.empty() && JunctionOwner_!=v.Id) blocked=true;
        if(blocked)
        {
            if(junction && i==3) { PendingArrival_=s; HasPendingArrival_=true; }
            continue;
        }
        if(junction) { JunctionOwner_=v.Id; NextLane_=(i+1)%Lanes_.size(); }
        if(pending) HasPendingArrival_=false;
        Slots_.push_back(std::move(s));
    }
}

void NeighbourhoodTraffic::Step(std::int64_t day,int ms,std::span<const NeighbourhoodPedestrianOccupancy> pedestrians)
{
    if(!JunctionOwner_.empty())
    {
        const auto owner=std::find_if(Slots_.begin(),Slots_.end(),[&](const auto& s){return s.Vehicle.Id==JunctionOwner_;});
        if((owner==Slots_.end() && (!HasPendingArrival_ || PendingArrival_.Vehicle.Id!=JunctionOwner_)) ||
            (owner!=Slots_.end() && owner->Vehicle.AlongLaneCm-owner->Vehicle.LengthCm/2>
            Lanes_[owner->LaneIndex].JunctionEndCm)) JunctionOwner_.clear();
    }
    // Fair rotation; a queued approach gets a turn before new reverse-para arrivals can enter.
    if(JunctionOwner_.empty()) for(std::size_t n=0;n<Lanes_.size();++n)
    {
        const auto i=(NextLane_+n)%Lanes_.size(); const auto& lane=Lanes_[i];
        const Slot* leader=nullptr;
        for(const auto& s:Slots_) if(s.LaneIndex==i && s.Vehicle.AlongLaneCm-s.Vehicle.LengthCm/2<=lane.JunctionEndCm &&
            (!leader || s.Vehicle.AlongLaneCm>leader->Vehicle.AlongLaneCm)) leader=&s;
        if(!leader && HasPendingArrival_ && PendingArrival_.LaneIndex==i) leader=&PendingArrival_;
        if(!leader || leader->Vehicle.AlongLaneCm+leader->Vehicle.LengthCm/2+100<lane.JunctionBeginCm) continue;
        JunctionOwner_=leader->Vehicle.Id; NextLane_=(i+1)%Lanes_.size(); break;
    }
    Spawn(day,ms,pedestrians);
    const int phase=R72PhaseMs(Seed_,ms);
    for(std::size_t i=0;i<Lanes_.size();++i)
    {
        const auto& lane=Lanes_[i];
        std::array<Slot*,3> order{}; std::size_t count=0;
        for(auto& s:Slots_) if(s.LaneIndex==i) order[count++]=&s;
        std::sort(order.begin(),order.begin()+count,[](const auto* a,const auto* b){return a->Vehicle.AlongLaneCm>b->Vehicle.AlongLaneCm;});
        int leaderTail=lane.LengthCm+NeighbourhoodTrafficGapCm;
        for(std::size_t n=0;n<count;++n)
        {
            auto& s=*order[n]; auto& v=s.Vehicle; const int half=v.LengthCm/2,old=v.AlongLaneCm;
            int target=old+s.CruiseCmPerSecond/10;
            v.State=NeighbourhoodVehicleState::Moving;
            const auto limit=[&](int maximum,NeighbourhoodVehicleState state){
                if(target>maximum) { target=std::max(old,maximum); v.State=state; }
            };
            if(s.DwellRemainingMs>0)
            {
                s.DwellRemainingMs-=100;
                if(s.DwellRemainingMs>0) { target=old; v.State=NeighbourhoodVehicleState::DwellingAtBusStop; }
            }
            if(v.Id!=JunctionOwner_ && old-half<=lane.JunctionEndCm)
                limit(lane.JunctionBeginCm-half-100,NeighbourhoodVehicleState::HoldingJunction);
            for(const auto& crossing:lane.Crossings)
            {
                // Reserve enough drive time to clear the WHOLE body before the walk starts.
                const int clearMs=(crossing.EndCm+half-old)*1000/s.CruiseCmPerSecond+200;
                if(old+half<=crossing.StopCm && (phase<20000 || 60000-phase<=clearMs))
                    limit(crossing.StopCm-half,NeighbourhoodVehicleState::StoppedAtStopLine);
                if(phase<20000 && old+half>crossing.StopCm && old-half<=crossing.EndCm)
                    limit(old,NeighbourhoodVehicleState::StoppedAtStopLine);
            }
            // Dock the REAR at the stop anchor; the west bus then clears the crossing behind it.
            const int busCentre=lane.BusCm+half;
            if(v.Kind==NeighbourhoodVehicleKind::Bus && !s.ServedBus && lane.BusCm>=0)
                limit(busCentre,NeighbourhoodVehicleState::DwellingAtBusStop);
            limit(leaderTail-NeighbourhoodTrafficGapCm-half,NeighbourhoodVehicleState::Queued);
            auto swept=v; swept.AlongLaneCm=target; R72Pose(swept,lane.Start,lane.Dx,lane.Dy);
            const auto before=R72Body(v,lane.Dx!=0),after=R72Body(swept,lane.Dx!=0);
            const NeighbourhoodRect envelope{std::min(before.X0,after.X0),std::min(before.Y0,after.Y0),
                std::max(before.X1,after.X1),std::max(before.Y1,after.Y1)};
            for(const auto& p:pedestrians) if(R72Intersects(envelope,p.Bounds))
            {
                const auto interval=R72Interval(lane.Start,lane.Dx,lane.Dy,p.Bounds);
                limit(interval.first-half-1,NeighbourhoodVehicleState::YieldingToPedestrians);
                if(target==old) v.State=NeighbourhoodVehicleState::YieldingToPedestrians;
            }
            v.AlongLaneCm=std::min(target,lane.LengthCm-half);
            v.SpeedCmPerSecond=(v.AlongLaneCm-old)*10;
            R72Pose(v,lane.Start,lane.Dx,lane.Dy);
            if(v.Kind==NeighbourhoodVehicleKind::Bus && !s.ServedBus && v.AlongLaneCm==busCentre)
            {
                s.ServedBus=true; s.DwellRemainingMs=NeighbourhoodBusDwellMs;
                v.State=NeighbourhoodVehicleState::DwellingAtBusStop; v.SpeedCmPerSecond=0;
            }
            leaderTail=v.AlongLaneCm-half;
        }
    }
    std::erase_if(Slots_,[&](const auto& s){return s.Vehicle.AlongLaneCm+s.Vehicle.LengthCm/2>=Lanes_[s.LaneIndex].LengthCm;});
}

std::vector<NeighbourhoodVehicle> NeighbourhoodTraffic::VehiclesNear(NeighbourhoodPoint centre,int radiusCm) const
{
    std::vector<NeighbourhoodVehicle> out;
    if(radiusCm<0) return out;
    for(const auto& s:Slots_)
    {
        const auto r=R72Body(s.Vehicle,Lanes_[s.LaneIndex].Dx!=0);
        const auto dx=std::int64_t(centre.X)-std::clamp(centre.X,r.X0,r.X1);
        const auto dy=std::int64_t(centre.Y)-std::clamp(centre.Y,r.Y0,r.Y1);
        // Bound both components before squaring, avoiding overflow for arbitrary int32 centres.
        if(dx>radiusCm || dx<-std::int64_t(radiusCm) || dy>radiusCm || dy<-std::int64_t(radiusCm)) continue;
        if(dx*dx+dy*dy<=std::int64_t(radiusCm)*radiusCm) out.push_back(s.Vehicle);
    }
    return out;
}

std::vector<NeighbourhoodVehicle> NeighbourhoodTraffic::VehiclesAt(NeighbourhoodTrafficTime now,
    NeighbourhoodPoint centre,int radiusCm,std::span<const NeighbourhoodPedestrianOccupancy> pedestrians)
{
    if(!R72Valid(now)) return {};
    AdvanceTo(now,pedestrians);
    return VehiclesNear(centre,radiusCm);
}
} // namespace PerfectWorld
