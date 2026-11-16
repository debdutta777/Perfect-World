#include "PerfectWorldLaneLifeSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PerfectWorldCommerceLive.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldSchoolGate.h"
#include "PerfectWorldSchoolLive.h" // S05
#include "PerfectWorldAwakeningBridge.h" // P38
#include "PerfectWorldAwakeningResultBridge.h"
#include "PerfectWorldCountdownContacts.h"
#include "FaridaSchedule.h"
#include "SchoolLifeContent.h"
#include "LaneOrigin.h"

// R88: keep commerce's borrowed content alive across resets and save adoption.
void UPWLaneLifeSubsystem::R88InstallCommerce()
{
    if (!R88CommerceContent.IsValid())
    {
        const auto* Instance = GetWorld()->GetGameInstance();
        const auto* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
        if (Content == nullptr || Content->GetDb() == nullptr) return;
        auto Loaded = MakeShared<PWCommerce::FCmLiveContent>();
        if (!Loaded->Load(*Content->GetDb())) return;
        R88CommerceContent = MoveTemp(Loaded);
    }
    R88CommerceContent->Refresh(GetNow(), GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>());
    // Start/load already own a validated world. Rebind content without changing any persistent field.
    FString R88SchoolWhy;
    FString R88Stream = TEXT("science");
    if (const auto* R88Instance = GetWorld()->GetGameInstance())
        if (const auto* R88Creator = R88Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>())
            for (const auto& R88Row : R88Creator->GetSchoolStreams())
                if (R88Row.bSelected) R88Stream = R88Row.Id.ToString();
    const auto* R88School = PWSchool::ScSharedSchoolContent(*GetWorld(), R88Stream, R88SchoolWhy);
    // S05: the school content (lesson bindings, structure catalogues, papers) is bound to the ONE sim; FS03Content::Cats.Day is the same
    // ScSharedSchoolContent binding as R88School, so the session catalogues agree.
    FString S05Why;
    const PWSchoolLive::FS03Content* S05School = PWSchoolLive::SharedContent(*GetWorld(), S05Why);
    const PerfectWorld::WorldSchoolCatalogues S05Lessons = S05School != nullptr
        ? PerfectWorld::WorldSchoolCatalogues{S05School->Cats.Structures, &S05School->Lessons, &S05School->Papers, &S05School->WorkBriefs}
        : PerfectWorld::WorldSchoolCatalogues{};
    // P38: process-lifetime validated recipes, like the shared school catalogues.
    static TUniquePtr<PerfectWorld::AbilityRecipeSet> P38Recipes;
    if (!P38Recipes.IsValid())
    {
        const auto* P38Instance = GetWorld()->GetGameInstance();
        const auto* P38Content = P38Instance != nullptr ? P38Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
        if (P38Content != nullptr && P38Content->GetDb() != nullptr)
        {
            auto P38Loaded = PerfectWorld::LoadAbilityRecipes(*P38Content->GetDb());
            if (P38Loaded.Ok()) P38Recipes = MakeUnique<PerfectWorld::AbilityRecipeSet>(MoveTemp(P38Loaded.Set));
            else UE_LOG(LogPWLaneLife, Error, TEXT("P38 ability recipes unavailable: %hs"), P38Loaded.Error.c_str());
        }
    }
    PerfectWorld::WorldAwakeningCatalogues P38Awakening;
    P38Awakening.Recipes = P38Recipes.Get();
    // P315: retain registered authored identities, and validate each optional binding
    // with the opening validator before it can reach the sim's ordinary-time path.
    std::vector<std::string> P315People;
    for (const auto& Person : PerfectWorld::KnownSchoolLifeCast())
        if (!Person.bProposedNewCast) P315People.emplace_back(Person.PersonId);
    const PerfectWorld::LaneOriginBinding* P315Origin = nullptr;
    const auto* P312Instance = GetWorld()->GetGameInstance();
    const auto* P312Creator = P312Instance != nullptr ? P312Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>() : nullptr;
    if (P312Creator != nullptr)
    {
        const auto P312Origin = P312Creator->GetLaneOrigin();
        const auto P312Id = PWLaneLife::ToStd(P312Origin.CounterpartPersonId);
        if (P312Origin.bResolved)
            for (const auto& P312Binding : PerfectWorld::AuthoredLaneOrigins())
                if (P312Binding.CounterpartPersonId == P312Id && P312Binding.GrewUpInCare == P312Origin.bGrewUpInCare)
                {
                    P315Origin = &P312Binding;
                    break;
                }
    }
    const auto* P312Presence = GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
    if (P315Origin != nullptr && P312Presence != nullptr &&
        P312Presence->IsRegistered(PWLaneLife::ToName(P315Origin->CounterpartPersonId)))
        P315People.push_back(P315Origin->CounterpartPersonId);
    if (P312Presence != nullptr && P312Presence->IsRegistered(PWLaneLife::ToName(PerfectWorld::FaridaPersonId)))
        P315People.emplace_back(PerfectWorld::FaridaPersonId);
    std::vector<std::string> P315Diagnostics;
    P38Awakening.Countdown = PWAwakening::P315BuildCountdownContext(P315Origin, P315People, P315Diagnostics);
    for (const auto& Diagnostic : P315Diagnostics)
        UE_LOG(LogPWLaneLife, Warning, TEXT("P315 countdown contact %hs"), Diagnostic.c_str());
    // WorldState has no integrated relationship book yet. Leave it absent; never fabricate scores.
    Sim = MakeUnique<PerfectWorld::WorldSim>(Sim->State(), PerfectWorld::MaxEventsPerAdvance,
        R88School != nullptr ? R88School->Binding : PerfectWorld::DayLoopCatalogues{}, R88CommerceContent->Catalogues(), S05Lessons, P38Awakening);
    PWAwakening::AwBridge::Install(GetWorld(), PWAwakening::P38MakeLiveFloorBridge(*GetWorld()));
    PWAwakening::ApBridge::Install(GetWorld(), PWAwakening::MakeLaneLifeAwakeningBridge(*GetWorld()));
    const TWeakObjectPtr<UPWLaneLifeSubsystem> Weak(this);
    PWCommerce::FCmLiveAccess Access;
    Access.Lifetime = R88CommerceContent;
    Access.Read = [Weak]() -> const PerfectWorld::WorldSim*
    {
        auto* Life = Weak.Get();
        return Life != nullptr ? Life->R88CommerceSim() : nullptr;
    };
    Access.Submit = [Weak](const PerfectWorld::Command& Command)
    {
        auto* Life = Weak.Get();
        return Life != nullptr ? Life->R88CommerceSubmit(Command) : PerfectWorld::CommandResult{};
    };
    Access.Approach = [Weak](const FName Shop)
    {
        auto* Life = Weak.Get();
        return Life != nullptr ? PWCommerce::R88CounterApproach(*Life->GetWorld(), Shop) : std::string();
    };
    PWCommerce::CmBridge::Install(GetWorld(), MakeShared<PWCommerce::FCmLiveCommerceWorld>(R88CommerceContent->Content(), MoveTemp(Access)));
}

const PerfectWorld::WorldSim* UPWLaneLifeSubsystem::R88CommerceSim()
{
    if (Sim.IsValid() && R88CommerceContent.IsValid())
        R88CommerceContent->Refresh(GetNow(), GetWorld()->GetSubsystem<UPWNpcPresenceSubsystem>(), Sim.Get());
    return Sim.Get();
}

PerfectWorld::CommandResult UPWLaneLifeSubsystem::R88CommerceSubmit(const PerfectWorld::Command& Command)
{
    if (R88CommerceSim() == nullptr) return {};
    return SubmitCommand(Command);
}
