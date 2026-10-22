#include "PerfectWorldAcceptanceTailor.h"

#include "Components/CapsuleComponent.h"
#include "Doors.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceProprietorWork.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldLaneKitStalls.h"
#include "PerfectWorldLaneLifeConversation.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcLooks.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldProprietorBindings.h"
#include "PerfectWorldTailorBindings.h"

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;
const FName R66Person(PWLane::R66TailorPerson);
constexpr const char* R66Fact = "fact.satyajit_dutta.bring_garment_agreed";
struct FR66Restart { bool bContinue = false; PerfectWorld::WorldSaveData Saved; };
struct FR66Plan
{
    FVector Door, Customer, Counter, Out;
};
bool R66Plan(FR66Plan& Plan)
{
    PWLane::FLaneBuilding Building;
    if (!PWLane::MakeProprietorBuilding(PWLane::R66TailorBuilding, Building)) return false;
    const auto Provider = PWLane::R66TailorBindings(Building);
    const auto* Door = Provider.Anchors.Find(FName(TEXT("anchor.tailor_shop.front_door")));
    const auto* Customer = Provider.Anchors.Find(FName(TEXT("anchor.tailor_shop.customer_counter")));
    const auto* Counter = Provider.Anchors.Find(FName(TEXT("anchor.tailor_shop.staff_counter")));
    if (Door == nullptr || Customer == nullptr || Counter == nullptr) return false;
    Plan = {Door->Feet.GetLocation(), Customer->Feet.GetLocation(), Counter->Feet.GetLocation(), -Building.Frame.V};
    return true;
}
FStepDef R66Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run, double Timeout = 30)
{
    FStepDef Step;
    Step.Name = Name; Step.Run = MoveTemp(Run); Step.TimeoutSeconds = Timeout;
    return Step;
}
FStepDef R66Phase(FStepDef Step, TSharedRef<FR66Restart> State, bool bContinue)
{
    auto Run = MoveTemp(Step.Run);
    Step.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
        { return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("other restart-pair phase")); };
    return Step;
}
FStepDef R66Camera(const TCHAR* Name, EPWCameraMode Mode)
{
    return R66Action(Name, [Mode](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Pawn = Sub.GetPawn();
        if (Pawn == nullptr) return FStepOutcome::Fail(TEXT("no pawn for camera switch"));
        const FVector Before = Pawn->GetActorLocation();
        Pawn->SetCameraMode(Mode);
        return Pawn->GetCameraMode() == Mode && Pawn->GetActorLocation().Equals(Before, 0.01) ?
            FStepOutcome::Pass(TEXT("camera switched without moving player")) : FStepOutcome::Fail(TEXT("camera switch changed position or failed"));
    });
}
bool R66Slot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Data)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Data).HashVerified;
}
APWDoorActor* R66Door(UPWAcceptanceSubsystem& Sub)
{
    for (TActorIterator<APWDoorActor> It(Sub.GetPlayWorld()); It; ++It)
        if (It->GetDoorId() == FName(PWLane::R66TailorDoor)) return *It;
    return nullptr;
}
FStepDef R66Walk(const TCHAR* Name, bool bCustomer)
{
    return PWAcceptance::MakeWalkStep(Name, [bCustomer](UPWAcceptanceSubsystem&, FVector& Point, FString& Why)
    {
        FR66Plan Plan;
        if (!R66Plan(Plan)) { Why = TEXT("tailor provider unavailable"); return false; }
        Point = bCustomer ? Plan.Customer : Plan.Door;
        return true;
    }, 15, 45);
}
FStepDef R66AtCounter(const TCHAR* Name)
{
    return R66Action(Name, [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        FR66Plan Plan;
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        const auto* Actor = Presence != nullptr ? Presence->FindPerson(R66Person) : nullptr;
        if (!R66Plan(Plan)) return FStepOutcome::Fail(TEXT("tailor plan unavailable"));
        if (Actor == nullptr || FVector::Dist2D(Actor->GetActorLocation(), Plan.Counter) > 75 ||
            Actor->GetNpcMovement()->GetPresentedState() != EPWNpcState::Work ||
            Actor->GetNpcMovement()->GetIntent().WorkActionId != FName(TEXT("work.measure"))) return FStepOutcome::Wait();
        return FStepOutcome::Pass(TEXT("Satyajit physically measures at the staff counter"));
    }, 60);
}
FStepDef R66SelectTarget(const TCHAR* Name, bool bBrowse)
{
    return R66Action(Name, [bBrowse](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Interaction = Sub.GetInteraction();
        AActor* Target = nullptr;
        if (bBrowse)
        {
            for (TActorIterator<AActor> It(Sub.GetPlayWorld()); It; ++It)
                if (It->ActorHasTag(FName(TEXT("interaction.nabapara.tailor_shop.browse")))) { Target = *It; break; }
        }
        else if (auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>()) Target = Presence->FindPerson(R66Person);
        const auto* Interactable = Cast<IPWInteractable>(Target);
        if (Interaction == nullptr || Interactable == nullptr) return FStepOutcome::Fail(TEXT("no physical interaction target"));
        Sub.FaceWorldPoint(Interactable->GetInteractionPoint());
        if (Interaction->GetTarget() == Target && Interaction->IsPromptVisible()) return FStepOutcome::Pass(TEXT("real prompt selected"));
        // People outrank browse displays; use the player's real cycle action to select the display, then back to Talk.
        if (Rt.Elapsed - Rt.LastProbe >= 0.2) { Interaction->CycleTarget(); Rt.LastProbe = Rt.Elapsed; }
        return FStepOutcome::Wait();
    });
}
FStepDef R66ServiceLine(const TCHAR* Name, bool bRemembered)
{
    return R66Action(Name, [bRemembered](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        const auto* Memory = Life != nullptr ? Life->FindConversation(R66Person) : nullptr;
        if (Life == nullptr || Talk == nullptr || !Talk->IsOpen() || Memory == nullptr || !Memory->Open ||
            PerfectWorld::OpenConversationPersonId(Life->GetWorldState()) != "person.satyajit_dutta")
            return FStepOutcome::Fail(TEXT("no real tailor conversation panel"));
        FPWCoreConversationSession Session(TWeakObjectPtr<UPWLaneLifeSubsystem>(Life), R66Person,
            FText::FromString(TEXT("Satyajit Dutta")), Memory->SessionCount);
        const auto View = Session.GetView();
        const FString Line = View.Line.ToString();
        if (!View.bCanLeave || !Line.Contains(TEXT("Purchasing is unavailable")) ||
            (bRemembered && (!PerfectWorld::HasConversationFact(*Memory, R66Fact) || !Line.Contains(TEXT("You agreed")))))
            return FStepOutcome::Fail(TEXT("panel projection lacks unavailable purchasing, Leave or remembered agreement"));
        return FStepOutcome::Pass(Line);
    });
}
}

namespace PWAcceptance
{
void AppendTailorWorkdaySteps(TArray<FStepDef>& Out)
{
    auto State = MakeShared<FR66Restart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(R66Action(TEXT("tailor_restart_pair_contract"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("tailor_workday requires explicit new then fresh-process continue"));
        State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (State->bContinue && !R66Slot(Sub, State->Saved)) return FStepOutcome::Fail(TEXT("cannot authenticate original slot"));
        return FStepOutcome::Pass(TEXT("explicit restart-pair phase"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(R66Phase(R66Action(TEXT("tailor_continued_memory"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (Life == nullptr) return FStepOutcome::Fail(TEXT("no continued world"));
        const auto* Pawn = Sub.GetPawn();
        const auto& P = State->Saved.Player.PositionCm;
        const FVector SavedPosition(static_cast<double>(P[0]), static_cast<double>(P[1]), static_cast<double>(P[2]));
        const auto SavedCamera = State->Saved.Player.Camera == PerfectWorld::CameraMode::FirstPerson ?
            EPWCameraMode::FirstPerson : EPWCameraMode::ThirdPerson;
        if (Pawn == nullptr || FVector::Dist(Pawn->GetActorLocation(), SavedPosition) > 5 || Pawn->GetCameraMode() != SavedCamera)
            return FStepOutcome::Fail(TEXT("Continue changed saved customer position or camera"));
        const auto Found = State->Saved.World.Conversations.find("person.satyajit_dutta");
        const auto* Memory = Life->FindConversation(R66Person);
        if (Found == State->Saved.World.Conversations.end() || Memory == nullptr || *Memory != Found->second ||
            !PerfectWorld::HasConversationFact(*Memory, R66Fact) || Memory->Facts.size() != 1 || !Memory->Receipts.empty() ||
            Life->GetWorldState().Journal != State->Saved.World.Journal)
            return FStepOutcome::Fail(TEXT("agreement/history changed across fresh-process Continue"));
        return FStepOutcome::Pass(TEXT("exact saved agreement and dated history survived; no purchase receipt"));
    }), State, true));
    Out.Add(R66Camera(TEXT("tailor_first_person"), EPWCameraMode::FirstPerson));
    Out.Add(R66Phase(R66Action(TEXT("declared_start_outside_tailor"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        FR66Plan Plan;
        auto* Pawn = Sub.GetPawn();
        if (Pawn == nullptr || !R66Plan(Plan)) return FStepOutcome::Fail(TEXT("no pawn or tailor plan"));
        if (Rt.Phase == 0)
        {
            if (!Pawn->TeleportTo(Plan.Door + Plan.Out * 150 + FVector(0, 0, PWPlayer::CapsuleHalfHeight + 5), FRotator::ZeroRotator))
                return FStepOutcome::Fail(TEXT("declared-start placement refused"));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Sub.FaceWorldPoint(Plan.Door + Plan.Out * 1000);
            Rt.NextPhase();
        }
        return Rt.InPhase() >= 0.8 && Pawn->GetCharacterMovement()->IsMovingOnGround() ?
            FStepOutcome::Pass(TEXT("one declared start; remaining approach and exit use the real pawn")) : FStepOutcome::Wait();
    }), State, false));
    Out.Add(R66Phase(R55PilotTimeStep(TEXT("tailor_clock_to_1000"), 10, 0), State, false));
    Out.Add(R66Phase(R66AtCounter(TEXT("tailor_arrives_at_counter")), State, false));
    Out.Add(R66Phase(R66Walk(TEXT("tailor_approach_front_door"), false), State, false));
    Out.Add(R66Phase(MakeInteractStep(TEXT("tailor_open_real_door"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor* { return R66Door(Sub); },
        [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Door = R66Door(Sub);
            Detail = TEXT("real tailor door is open");
            return Door != nullptr && Door->IsOpen() && !Door->IsMoving();
        }, [](UPWAcceptanceSubsystem& Sub) { const auto* Door = R66Door(Sub); return Door != nullptr && Door->IsOpen() && !Door->IsMoving(); }, 20), State, false));
    Out.Add(R66Phase(R66Walk(TEXT("tailor_walk_to_customer_counter"), true), State, false));
    Out.Add(R66Phase(R66SelectTarget(TEXT("tailor_select_browse_prompt"), true), State, false));
    Out.Add(R66Phase(MakeInteractStep(TEXT("tailor_read_staffed_browse_notice"),
        [](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            const FName Tag(TEXT("interaction.nabapara.tailor_shop.browse"));
            for (TActorIterator<AActor> It(Sub.GetPlayWorld()); It; ++It) if (It->ActorHasTag(Tag)) return *It;
            return nullptr;
        }, [](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Interaction = Sub.GetInteraction();
            const auto* Door = PerfectWorld::FindDoor("door.nabapara.tailor_shop_front");
            if (Interaction == nullptr || Door == nullptr) return false;
            Detail = Interaction->GetNotice().ToString();
            return Interaction->IsNoticeVisible() && Detail.Contains(PWLane::KitPublic::R66TailorStaffedPhrase) &&
                Detail.Contains(PWLane::PostedHoursText(*Door)) && !Detail.Contains(TEXT("Unstaffed"));
        }, nullptr, 20), State, false));
    Out.Add(R66AtCounter(TEXT("tailor_service_present_in_this_process")));
    Out.Add(R66Action(TEXT("tailor_measured_height"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        const auto* Actor = Presence != nullptr ? Presence->FindPerson(R66Person) : nullptr;
        if (Actor == nullptr) return FStepOutcome::Fail(TEXT("no physical tailor to measure"));
        const double Capsule = Actor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2;
        TArray<FPWNpcBlock> Blocks;
        PWNpcLooks::BuildBlocks(R66Person, Actor->GetOutfit(), Actor->GetStandingHeightCm(), Blocks);
        double Bottom = 1e9, Top = -1e9;
        for (const auto& Block : Blocks)
        {
            Bottom = FMath::Min(Bottom, Block.Centre.Z - Block.Size.Z * 0.5);
            Top = FMath::Max(Top, Block.Centre.Z + Block.Size.Z * 0.5);
        }
        if (!FMath::IsNearlyEqual(Capsule, 167.0, 0.05) || !FMath::IsNearlyEqual(Top - Bottom, 167.0, 0.05))
            return FStepOutcome::Fail(TEXT("tailor capsule or standing body differs from frozen 167 cm"));
        return FStepOutcome::Pass(FString::Printf(TEXT("live capsule %.2f cm; standing block body %.2f cm"), Capsule, Top - Bottom));
    }));
    Out.Add(R66SelectTarget(TEXT("tailor_select_talk_prompt"), false));
    Out.Add(R55SafeTalkStep(TEXT("tailor_real_talk_defers_to_safe_pause"), R66Person));
    Out.Add(R66Phase(R66ServiceLine(TEXT("tailor_remembers_after_restart"), true), State, true));
    Out.Add(R66Phase(R66ServiceLine(TEXT("tailor_purchasing_unavailable_service_line"), false), State, false));
    Out.Add(MakeShotStep(TEXT("shot_tailor_service_and_unavailable_purchasing")));
    Out.Add(R66Phase(R66Action(TEXT("tailor_agreement_is_no_sale"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        if (Life == nullptr || Talk == nullptr || !Talk->IsOpen()) return FStepOutcome::Fail(TEXT("no tailor panel"));
        const auto Before = Life->GetNow();
        Talk->ChooseOption(FName(TEXT("choice.satyajit_dutta.agree")));
        const auto* Memory = Life->FindConversation(R66Person);
        if (Memory == nullptr || !PerfectWorld::HasConversationFact(*Memory, R66Fact) || Memory->Facts.size() != 1 ||
            !Memory->Receipts.empty() || Life->GetNow() != Before) return FStepOutcome::Fail(TEXT("agreement mutated more than the free intention"));
        return FStepOutcome::Pass(TEXT("one free intention; no money, inventory, sale or completed alteration"));
    }), State, false));
    Out.Add(R55LeaveResumeStep(TEXT("tailor_leave_resumes_unfinished_step"), R66Person));
    Out.Add(R66AtCounter(TEXT("tailor_visible_work_resumed")));
    Out.Add(MakeShotStep(TEXT("shot_tailor_resumed_work")));
    Out.Add(R66Camera(TEXT("tailor_third_person"), EPWCameraMode::ThirdPerson));
    Out.Add(R66SelectTarget(TEXT("tailor_third_person_talk_prompt"), false));
    Out.Add(R55SafeTalkStep(TEXT("tailor_third_person_safe_pause"), R66Person));
    Out.Add(R66ServiceLine(TEXT("tailor_third_person_remembered_line"), true));
    Out.Add(MakeShotStep(TEXT("shot_tailor_third_person_talk")));
    Out.Add(R55LeaveResumeStep(TEXT("tailor_third_person_leave_resume"), R66Person));
    Out.Add(R66AtCounter(TEXT("tailor_third_person_visible_work_resumed")));
    Out.Add(R66Phase(R66Action(TEXT("tailor_save_agreement_for_fresh_process"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Hud = Sub.GetHud();
        if (Hud == nullptr) return FStepOutcome::Fail(TEXT("no save owner"));
        bool bSaved = false;
        const FText Detail = Hud->SaveActiveGame(bSaved);
        if (!bSaved || !R66Slot(Sub, State->Saved)) return FStepOutcome::Fail(Detail.ToString());
        const auto Found = State->Saved.World.Conversations.find("person.satyajit_dutta");
        if (Found == State->Saved.World.Conversations.end() || Found->second.Open ||
            !PerfectWorld::HasConversationFact(Found->second, R66Fact) || !Found->second.Receipts.empty())
            return FStepOutcome::Fail(TEXT("authenticated slot lacks closed agreement session"));
        return FStepOutcome::Pass(TEXT("authenticated slot contains agreement for fresh-process Continue"));
    }), State, false));
}
}
