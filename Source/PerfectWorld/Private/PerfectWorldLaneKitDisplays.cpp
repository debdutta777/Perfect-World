#include "PerfectWorldLaneKitDisplays.h"

#include "Doors.h"
#include "Engine/World.h"
#include "PerfectWorldLaneKitStalls.h"
#include "PerfectWorldProprietorBindings.h"
#include "PerfectWorldShopDisplayActor.h"

namespace PWLane
{
const LI::Anchor* FindKitBrowseAnchor(const LI::InteriorLayout& Layout)
{
    for (const char* Suffix : {"browse_shelf", "browse_bins", "customer_counter"})
    {
        if (const LI::Anchor* Anchor = LI::FindAnchor(Layout, Layout.BuildingId + ".anchor." + Suffix))
        {
            return Anchor;
        }
    }
    return nullptr;
}

FVector KitBrowsePoint(const FLaneBuilding& B, const LI::InteriorLayout& L, const LI::Anchor& A)
{
    return KitPoint(B, L, A.U + A.FaceU * KitPublic::BrowseReachCm, A.V + A.FaceV * KitPublic::BrowseReachCm, KitPublic::BrowseHeightCm);
}

int32 SpawnKitShopDisplays(UWorld& World, const FLaneScene& Scene)
{
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    int32 Served = 0;
    for (const FName Id : UnstaffedBrowseShopIds())
    {
        FLaneBuilding Building;
        FLaneKitFrontage Kit;
        if (!MakeProprietorBuilding(*Id.ToString(), Building) || !IsKitFrontage(Building, Kit))
        {
            UE_LOG(LogTemp, Error, TEXT("%s: the catalogue row did not resolve to a built frontage; no browse display."), *Id.ToString());
            continue;
        }
        if (!HasKitDoor(Scene, *Kit.DoorId.ToString()))
        {
            UE_LOG(LogTemp, Warning, TEXT("%s: the shop's door was not built; no browse display."), *Id.ToString());
            continue;
        }
        const PerfectWorld::DoorDef* Door = Kit.Entry != nullptr ? PerfectWorld::FindDoor(Kit.Entry->EntranceDoorId) : nullptr;
        const LI::InteriorLayout Layout = MakeKitFrontageLayout(Building, Kit);
        const LI::Anchor* Anchor = Layout.Ok() ? FindKitBrowseAnchor(Layout) : nullptr;
        const LI::Opening* Entrance = Layout.Ok() ? KitFrontEntrance(Layout) : nullptr;
        const FString Notice = Door != nullptr ? BrowseNoticeText(*Door) : FString();
        const FString SignText = Door != nullptr ? PostedSignText(*Door) : FString();
        const FName Tag = BrowseActorTag(Id);
        if (Anchor == nullptr || Entrance == nullptr || Notice.IsEmpty() || SignText.IsEmpty() || Tag.IsNone())
        {
            UE_LOG(LogTemp, Error, TEXT("%s: no browse anchor, entrance or business hours resolved; no browse display."), *Id.ToString());
            continue;
        }

        APWShopBrowseActor* Browse = World.SpawnActor<APWShopBrowseActor>(KitBrowsePoint(Building, Layout, *Anchor), FRotator::ZeroRotator, Params);
        if (Browse != nullptr)
        {
            Browse->Configure(Id, Tag, FText::FromString(KitPublic::BrowseVerb), FText::FromString(Notice),
                              KitBrowsePoint(Building, Layout, *Anchor));
        }
        const double DoorU = 0.5 * (Entrance->Plan.U0 + Entrance->Plan.U1);
        APWShopHoursSignActor* Sign = World.SpawnActor<APWShopHoursSignActor>(KitSignLocation(Building, DoorU), FRotator::ZeroRotator, Params);
        if (Sign != nullptr)
        {
            Sign->Configure(Id, SignText, KitSignLocation(Building, DoorU), KitSignYaw(Building), KitPublic::SignTextSizeCm);
        }
        Served += (Browse != nullptr && Sign != nullptr) ? 1 : 0;
    }
    return Served;
}
} // namespace PWLane
