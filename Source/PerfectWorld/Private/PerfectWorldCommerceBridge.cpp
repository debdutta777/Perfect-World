#include "PerfectWorldCommerceBridge.h"

#include "Engine/World.h"

namespace PWCommerce
{
namespace
{
TMap<TWeakObjectPtr<const UWorld>, TSharedPtr<ICmCommerceWorld>>& CmRegistry()
{
    static TMap<TWeakObjectPtr<const UWorld>, TSharedPtr<ICmCommerceWorld>> Registry;
    return Registry;
}

void CmPruneRegistry()
{
    for (auto It = CmRegistry().CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid())
        {
            It.RemoveCurrent();
        }
    }
}
} // namespace

namespace CmBridge
{
void Install(UWorld* World, TSharedPtr<ICmCommerceWorld> Bridge)
{
    CmPruneRegistry();
    if (World == nullptr)
    {
        return;
    }
    if (Bridge.IsValid())
    {
        CmRegistry().Add(World, MoveTemp(Bridge));
    }
    else
    {
        CmRegistry().Remove(World);
    }
}

void Remove(UWorld* World)
{
    CmPruneRegistry();
    if (World != nullptr)
    {
        CmRegistry().Remove(World);
    }
}

TSharedPtr<ICmCommerceWorld> Resolve(const UWorld* World)
{
    if (World == nullptr)
    {
        return nullptr;
    }
    const TSharedPtr<ICmCommerceWorld>* Found = CmRegistry().Find(World);
    return Found != nullptr ? *Found : nullptr;
}
} // namespace CmBridge
} // namespace PWCommerce
