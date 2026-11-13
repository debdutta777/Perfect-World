#include "PerfectWorldAmbientBindings.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "LaneAmbient.h"
#include "PerfectWorldAcceptanceStationery.h"
#include "PerfectWorldAcceptanceSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldLaneBlocks.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldVendorCart.h"
#include "WorldReducer.h"

namespace
{
using AmAmbientStep = PWAcceptance::FStepDef;
using AmAmbientOutcome = PWAcceptance::FStepOutcome;
using AmAmbientRuntime = PWAcceptance::FStepRuntime;
FName AmAmbientAcceptanceName(std::string_view Id) { return FName(UTF8_TO_TCHAR(std::string(Id).c_str())); }
APWNpcCharacter* AmAmbientFind(UPWAcceptanceSubsystem& Sub, FName Person)
{
    auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
    return Presence ? Presence->FindPerson(Person) : nullptr;
}
APWVendorCart* AmAmbientCart(UPWAcceptanceSubsystem& Sub)
{
    for (TActorIterator<APWVendorCart> It(Sub.GetPlayWorld()); It; ++It) { return *It; }
    return nullptr;
}
AmAmbientStep AmAmbientAction(const FString& Name,
    TFunction<AmAmbientOutcome(UPWAcceptanceSubsystem&, AmAmbientRuntime&)> Run, double Timeout = 30)
{
    AmAmbientStep Step;
    Step.Name = Name;
    Step.TimeoutSeconds = Timeout;
    Step.Run = MoveTemp(Run);
    return Step;
}
AmAmbientStep AmAmbientWalk(const FString& Name, FVector Feet)
{
    return PWAcceptance::MakeWalkStep(Name,
        [Feet](UPWAcceptanceSubsystem&, FVector& Target, FString&) { Target = Feet; return true; }, 25, 40);
}
AmAmbientStep AmAmbientTalkPerson(FName Person)
{
    return PWAcceptance::MakeInteractStep(TEXT("talk_") + Person.ToString(),
        [Person](UPWAcceptanceSubsystem& Sub) -> AActor* { return AmAmbientFind(Sub, Person); },
        [Person](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            const auto* Presenter = UPWConversationPresenter::Get(Sub.GetPlayWorld());
            if (!Life || !Presenter || !Presenter->IsOpen() ||
                AmAmbientAcceptanceName(PerfectWorld::OpenConversationPersonId(Life->GetWorldState())) != Person) { return false; }
            Detail = TEXT("real interaction opened the authored core conversation");
            return true;
        }, nullptr, 25);
}
void AmAmbientConversation(TArray<AmAmbientStep>& Out, FName Person)
{
    Out.Add(AmAmbientTalkPerson(Person));
    Out.Add(AmAmbientAction(TEXT("hold_and_leave_") + Person.ToString(),
        [Person](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime& Rt)
        {
            auto* Npc = AmAmbientFind(Sub, Person);
            auto* Presenter = UPWConversationPresenter::Get(Sub.GetPlayWorld());
            if (!Npc || !Presenter) { return AmAmbientOutcome::Fail(TEXT("speaker/presenter missing")); }
            auto* Cart = Person == FName(TEXT("person.purnima_das")) ? AmAmbientCart(Sub) : nullptr;
            if (Rt.Phase == 0)
            {
                Rt.LastPos = Npc->GetActorLocation();
                if (Cart) { Rt.Target = Cart->GetActorLocation(); }
                Rt.NextPhase();
            }
            if (Rt.Phase == 1)
            {
                if (FVector::Dist(Rt.LastPos, Npc->GetActorLocation()) > 8 ||
                    (Cart && FVector::Dist(Rt.Target, Cart->GetActorLocation()) > 8))
                    return AmAmbientOutcome::Fail(TEXT("speaker or cart moved during Talk"));
                if (Rt.InPhase() < 3) { return AmAmbientOutcome::Wait(); }
                Presenter->LeaveConversation();
                Rt.NextPhase();
            }
            const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            if (Presenter->IsOpen() || !Life || !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty())
                return AmAmbientOutcome::Wait();
            if (Npc->GetNpcMovement()->GetPresentedState() == EPWNpcState::Chat) { return AmAmbientOutcome::Wait(); }
            return AmAmbientOutcome::Pass(TEXT("held safely for three seconds; Leave restored the schedule intent"));
        }));
}
}
namespace PWAcceptance
{
void AppendAmbientLaneSteps(TArray<FStepDef>& Out)
{
    Out.Add(MakeWorldReadyStep());
    Out.Add(MakeEnterWorldStep());
    Out.Add(Stationery::JumpClockStep(TEXT("ambient_tuesday_0845"), 8, 45));
    Out.Add(AmAmbientAction(TEXT("ambient_declared_start"), [](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime&)
    {
        auto* Pawn = Sub.GetPawn();
        if (!Pawn || !Sub.GetController()) { return AmAmbientOutcome::Fail(TEXT("no possessed pawn")); }
        Pawn->SetActorLocation(FVector(2200, -150, PWLane::GroundZ + PWPlayer::CapsuleHalfHeight + 2), false);
        Sub.GetController()->SetControlRotation(FRotator(0, 180, 0)); // arrival threshold behind the camera
        return AmAmbientOutcome::Pass(TEXT("only declared teleport; road start facing west"));
    }));
    Out.Add(AmAmbientAction(TEXT("vendor_arrives_from_north_alley"), [](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime&)
    {
        const auto* Person = AmAmbientFind(Sub, FName(TEXT("person.purnima_das")));
        if (!Person) { return AmAmbientOutcome::Wait(); }
        if (FVector::Dist2D(Person->GetActorLocation(), FVector(3100, 600, 0)) > 350)
            return AmAmbientOutcome::Fail(TEXT("first appearance was away from the side-alley threshold"));
        return AmAmbientOutcome::Pass(TEXT("vendor first appeared at the visible alley threshold"));
    }, 30));
    Out.Add(AmAmbientWalk(TEXT("walk_north_footpath"), FVector(2500, 340, PWLane::GroundZ + 15)));
    for (const auto Person : {FName(TEXT("person.arun_paul")), FName(TEXT("person.rekha_bose"))})
    {
        Out.Add(MakeWalkStep(TEXT("approach_") + Person.ToString(),
            [Person](UPWAcceptanceSubsystem& Sub, FVector& Target, FString&)
            {
                if (const auto* Npc = AmAmbientFind(Sub, Person))
                {
                    Target = Npc->GetActorLocation() + FVector(-110, 0, 0);
                    return true;
                }
                // Wait near their north-alley pause, facing away so arrivals can be presented.
                Target = FVector(2940, 340, PWLane::GroundZ + 15);
                return true;
            }, 45, 50));
        // MakeWalk's static fallback can finish before arrival. This separate step waits for the real person.
        Out.Add(AmAmbientAction(TEXT("wait_for_") + Person.ToString(),
            [Person](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime&)
            {
                Sub.FaceWorldPoint(FVector(2000, 340, PWLane::GroundZ + 100));
                const auto* Npc = AmAmbientFind(Sub, Person);
                return Npc ? AmAmbientOutcome::Pass(TEXT("scheduled passer arrived")) : AmAmbientOutcome::Wait();
            }, 55));
        Out.Add(MakeWalkStep(TEXT("reach_") + Person.ToString(),
            [Person](UPWAcceptanceSubsystem& Sub, FVector& Target, FString&)
            {
                const auto* Npc = AmAmbientFind(Sub, Person);
                if (!Npc) { return false; }
                Target = Npc->GetActorLocation() + FVector(-110, 0, 0);
                return true;
            }, 35, 25));
        AmAmbientConversation(Out, Person);
    }
    Out.Add(AmAmbientWalk(TEXT("return_to_road_line"), FVector(2400, 80, PWLane::GroundZ)));
    Out.Add(MakeWalkStep(TEXT("follow_cart_to_east_bay"),
        [](UPWAcceptanceSubsystem& Sub, FVector& Target, FString&)
        {
            const auto* Npc = AmAmbientFind(Sub, FName(TEXT("person.purnima_das")));
            if (!Npc) { return false; }
            Target = Npc->GetActorLocation() + FVector(0, -200, 0);
            return true;
        }, 40, 40));
    Out.Add(AmAmbientAction(TEXT("cart_pair_at_bay"), [](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime&)
    {
        const auto* Npc = AmAmbientFind(Sub, FName(TEXT("person.purnima_das")));
        const auto* Cart = AmAmbientCart(Sub);
        if (!Npc || !Cart || Cart->IsHidden()) { return AmAmbientOutcome::Wait(); }
        if (FVector::Dist2D(Npc->GetActorLocation(), FVector(2000, 80, 0)) > 100) { return AmAmbientOutcome::Wait(); }
        const FVector Delta = Cart->GetActorLocation() - Npc->GetActorLocation();
        if (FMath::Abs(Delta.X) > 4 || FMath::Abs(Delta.Y + 100) > 4)
            return AmAmbientOutcome::Fail(TEXT("cart lost its fixed route offset"));
        return AmAmbientOutcome::Pass(TEXT("vendor serves at east bay with paired cart"));
    }, 40));
    AmAmbientConversation(Out, FName(TEXT("person.purnima_das")));
    Out.Add(AmAmbientWalk(TEXT("obstruct_cart_only"), FVector(1750, -20, PWLane::GroundZ)));
    Out.Add(AmAmbientAction(TEXT("cart_swept_obstruction"), [](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime& Rt)
    {
        const auto* Cart = AmAmbientCart(Sub);
        const auto* Npc = AmAmbientFind(Sub, FName(TEXT("person.purnima_das")));
        if (!Cart || !Npc) { return AmAmbientOutcome::Fail(TEXT("paired actors missing")); }
        if (!Cart->IsRouteBlocked()) { Rt.Phase = 0; return AmAmbientOutcome::Wait(); }
        if (Rt.Phase == 0) { Rt.LastPos = Npc->GetActorLocation(); Rt.Target = Cart->GetActorLocation(); Rt.NextPhase(); }
        if (FVector::Dist(Rt.LastPos, Npc->GetActorLocation()) > 8 || FVector::Dist(Rt.Target, Cart->GetActorLocation()) > 8)
            return AmAmbientOutcome::Fail(TEXT("blocked pair continued through the player"));
        return Rt.InPhase() >= 3 ? AmAmbientOutcome::Pass(TEXT("cart footprint held both actors without passing through player")) : AmAmbientOutcome::Wait();
    }, 110));
    Out.Add(AmAmbientWalk(TEXT("clear_cart_route"), FVector(1750, -190, PWLane::GroundZ)));
    Out.Add(AmAmbientAction(TEXT("cart_resumes_without_teleport"), [](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime& Rt)
    {
        const auto* Cart = AmAmbientCart(Sub);
        const auto* Npc = AmAmbientFind(Sub, FName(TEXT("person.purnima_das")));
        if (!Cart || !Npc) { return AmAmbientOutcome::Fail(TEXT("pair lost")); }
        const FVector Now = Npc->GetActorLocation();
        if (Rt.Phase == 0) { Rt.Target = Now; Rt.LastPos = Now; Rt.NextPhase(); return AmAmbientOutcome::Wait(); }
        if (FVector::Dist(Now, Rt.LastPos) > FMath::Max(70.0, Rt.Dt * 260))
            return AmAmbientOutcome::Fail(TEXT("visible route discontinuity"));
        Rt.LastPos = Now;
        if (Cart->IsRouteBlocked() || FVector::Dist2D(Now, Rt.Target) < 150) { return AmAmbientOutcome::Wait(); }
        return AmAmbientOutcome::Pass(TEXT("pair walked at least 150 cm after clearance restored"));
    }, 35));
    Out.Add(AmAmbientAction(TEXT("ambient_no_duplicates"), [](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime&)
    {
        for (const auto Person : PerfectWorld::AmbientPeople)
        {
            int Count = 0;
            for (TActorIterator<APWNpcCharacter> It(Sub.GetPlayWorld()); It; ++It)
                if (It->GetPersonId() == AmAmbientAcceptanceName(Person)) { ++Count; }
            if (Count > 1) { return AmAmbientOutcome::Fail(TEXT("duplicate ambient identity")); }
        }
        int Carts = 0;
        for (TActorIterator<APWVendorCart> It(Sub.GetPlayWorld()); It; ++It) { ++Carts; }
        return Carts == 1 ? AmAmbientOutcome::Pass(TEXT("at most three ambient people and one inert cart; no duplicate identities"))
                          : AmAmbientOutcome::Fail(TEXT("cart count differs from one"));
    }));
    Out.Add(AmAmbientAction(TEXT("passers_left_off_lane"), [](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime&)
    {
        Sub.FaceWorldPoint(FVector(1750, -1000, PWLane::GroundZ + 100));
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        if (!Presence) { return AmAmbientOutcome::Fail(TEXT("no presence subsystem")); }
        for (const auto Person : {FName(TEXT("person.arun_paul")), FName(TEXT("person.rekha_bose"))})
        {
            const auto* Intent = Presence->FindIntent(Person);
            if (!Intent || !Intent->AnchorId.ends_with(".out")) { return AmAmbientOutcome::Fail(TEXT("off-lane destination not resolved")); }
            if (Presence->FindPerson(Person)) { return AmAmbientOutcome::Wait(); }
        }
        return AmAmbientOutcome::Pass(TEXT("both passers exited at their authored thresholds; registration retained"));
    }, 45));
    Out.Add(AmAmbientAction(TEXT("ambient_frame_sample"), [](UPWAcceptanceSubsystem& Sub, AmAmbientRuntime& Rt)
    {
        if (Rt.Phase == 0)
        {
            if (auto* Controller = Sub.GetController())
            {
                Controller->ConsoleCommand(TEXT("stat unit"), false);
                Controller->ConsoleCommand(TEXT("stat RHI"), false);
            }
            Rt.Target = FVector::ZeroVector;
            Rt.NextPhase();
        }
        Rt.Target.X += Rt.Dt;
        Rt.Target.Y = FMath::Max(Rt.Target.Y, Rt.Dt);
        Rt.Target.Z += 1;
        if (Rt.Target.Z < 120) { return AmAmbientOutcome::Wait(); }
        return AmAmbientOutcome::Pass(FString::Printf(
            TEXT("120 runner samples: mean %.2f ms, max %.2f ms. Runner Dt is capped; unit/RHI overlay capture follows. Added cart: six meshes, zero lights. Baseline/GPU budget comparison remains an integrator measurement."),
            1000 * Rt.Target.X / Rt.Target.Z, 1000 * Rt.Target.Y));
    }, 20));
    Out.Add(MakeShotStep(TEXT("ambient_lane_complete")));
}
}
