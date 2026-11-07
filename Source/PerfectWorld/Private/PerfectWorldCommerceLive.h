#pragma once

#include "PerfectWorldCommerceBridge.h"
#include "CommerceContent.h"
#include "NpcScheduleContent.h"
#include "WorldReducer.h"

class UPWNpcPresenceSubsystem;

namespace PWCommerce
{
/** R88 owns every string/schedule/span borrowed by the live reducer. Never moves after Load. */
class FCmLiveContent
{
public:
    FCmLiveContent() = default;
    FCmLiveContent(const FCmLiveContent&) = delete;
    FCmLiveContent& operator=(const FCmLiveContent&) = delete;
    bool Load(const PerfectWorld::ContentDb& Db);
    /** Project split weekly openings with the portable content query before each commerce operation. */
    void Refresh(PerfectWorld::GameInstant Now, const UPWNpcPresenceSubsystem* Presence,
        const PerfectWorld::WorldSim* Sim = nullptr);
    PerfectWorld::WorldCommerceCatalogues Catalogues() const;
    const PerfectWorld::CommerceContent& Content() const { return Data; }

private:
    struct FKeeper
    {
        PerfectWorld::NpcSchedule Schedule;
        std::vector<std::string_view> Anchors;
        bool bR88Registered = false;
    };
    struct FShop
    {
        std::vector<FKeeper> Keepers;
    };
    PerfectWorld::CommerceContent Data;
    std::vector<PerfectWorld::CommerceCatalogueEntry> Entries;
    std::vector<FShop> Shops;
    std::vector<PerfectWorld::CommerceServiceBinding> Bindings;
    PerfectWorld::GameInstant R88RefreshedAt = -1;
};

/** R88 callbacks always resolve the current simulation, including after Start/Continue. */
struct FCmLiveAccess
{
    TSharedPtr<const FCmLiveContent> Lifetime; // R88: panels may outlive subsystem teardown
    TFunction<const PerfectWorld::WorldSim*()> Read;
    TFunction<PerfectWorld::CommandResult(const PerfectWorld::Command&)> Submit;
    /** Actual physical approach -> authored replay anchor; empty means out of reach. */
    TFunction<std::string(FName)> Approach;
};

class FCmLiveCommerceWorld final : public ICmCommerceWorld
{
public:
    FCmLiveCommerceWorld(const PerfectWorld::CommerceContent& Content, FCmLiveAccess Access);
    virtual FCmServiceView BeginService(FName ShopId) override;
    virtual FCmQuoteView Quote(FName ShopId, FName ItemId, int32 Quantity) override;
    virtual FCmBuyResult Buy(FName ShopId, FName ItemId, int32 Quantity, uint64 ExpectedRevision) override;
    virtual void EndService(FName ShopId) override;
    virtual FCmInventoryView ReadInventory() override;
    virtual FCmUseResult Consume(FName ItemId) override;
    virtual TOptional<int64> ReadWallet() override;

private:
    const PerfectWorld::CommerceContent& Data;
    FCmLiveAccess Access;
};

/** R88 lane registration: derives customer points from registered spatial providers. */
void R88SpawnLiveCounters(UWorld& World);
std::string R88CounterApproach(UWorld& World, FName ShopId);
} // namespace PWCommerce
