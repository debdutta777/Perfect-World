#include "PerfectWorldAcceptanceHomesR5.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceHomes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneKitFrontages.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldResidentsR5Bindings.h"
#include "PerfectWorldRupaBindings.h"
#include "WorldSave.h"

// homes_round5: fresh-process new/continue pair. Only the declared new-phase start teleports.
// Shared-file hooks are supplied verbatim in R5-02_notes.md. No admission or agreement is fabricated.
namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FR52Home
{
    bool Upper = false;
    const TCHAR* Person;
    const TCHAR* Door;
    const char* Tail;
    const char* Fact;
    double Height;
};
const FR52Home R52Old{false, PWLane::R52Prabir, PWLane::R52OldDoor, "prabir_chatterjee",
    "fact.prabir_chatterjee.lane_stories_agreed", 169};
const FR52Home R52Upper{true, PWLane::R52Ishita, PWLane::R52UpperDoor, "ishita_banerjee",
    "fact.ishita_banerjee.quiet_evening_agreed", 165};
struct FR52Restart
{
    bool Continue = false;
    PerfectWorld::WorldSaveData Saved;
};

FStepDef R52Action(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Out;
    Out.Name = Name;
    Out.TimeoutSeconds = 20;
    Out.Run = MoveTemp(Run);
    return Out;
}

FStepDef R52Phase(FStepDef Def, TSharedRef<FR52Restart> State, bool Continue = false)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, Continue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    { return State->Continue == Continue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("other fresh-process phase")); };
    return Def;
}

APWDoorActor* R52Door(UWorld* World, const TCHAR* Id)
{
    for (TActorIterator<APWDoorActor> It(World); It; ++It) if (It->GetDoorId() == FName(Id)) return *It;
    return nullptr;
}

FVector R52Point(bool Upper, double U, double V, double Floor = -1)
{
    PWLane::FLaneBuilding Building;
    if (!PWLane::MakeRupaBuilding(Upper ? PWLane::R52TallFlats : PWLane::R52OldHouse, Building)) return FVector::ZeroVector;
    return Building.Frame.Pt(U, V, Floor >= 0 ? Floor : Upper ? 315 : 15);
}

FStepDef R52Walk(const FString& Name, const FVector& Feet, double Tolerance = 8)
{
    auto Def = R52Action(Name, [Feet, Tolerance](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Pawn = Sub.GetPawn();
        if (!Pawn) return FStepOutcome::Fail(TEXT("R52 no pawn"));
        const auto Result = Sub.TickWalk(Feet, Tolerance, Rt);
        if (Result.State != PWAcceptance::EStepState::Passed) return Result;
        const double Actual = Pawn->GetActorLocation().Z - Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if (!Pawn->GetCharacterMovement()->IsMovingOnGround() || FMath::Abs(Actual - Feet.Z) > 4)
        { Rt.Note = FString::Printf(TEXT("R52 XY reached; supported feet %.2f, expected %.2f"), Actual, Feet.Z); return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("R52 physically walked; supported feet Z %.2f"), Actual));
    });
    Def.TimeoutSeconds = 90;
    return Def;
}

FStepDef R52Clock(const FString& Name, int32 Hour)
{
    return R52Action(Name, [Hour](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Life) return FStepOutcome::Fail(TEXT("R52 no clock"));
        const int32 Day = static_cast<int32>((Life->GetNow() + 360) / 1440) + 1;
        // Reuse the existing detached reducer advance and production reconciliation; never set a clock field.
        auto Def = PWAcceptance::Homes::HmClockStep(TEXT("R52_clock"), Day, Hour, 0);
        return Def.Run(Sub, Rt);
    });
}

FStepDef R52DoorMove(const FString& Name, const TCHAR* Id, bool Open)
{
    return PWAcceptance::MakeInteractStep(Name,
        [Id](UPWAcceptanceSubsystem& Sub) -> AActor* { return R52Door(Sub.GetPlayWorld(), Id); },
        [Id, Open](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Door = R52Door(Sub.GetPlayWorld(), Id);
            if (!Door || Door->IsMoving() || Door->IsOpen() != Open) return false;
            const bool Full = Open ? Door->GetOpenFraction() >= 0.99f : Door->GetOpenFraction() <= 0.01f;
            Detail = TEXT("R52 real leaf completed its sweep without obstruction");
            return Full;
        }, [Id, Open](UPWAcceptanceSubsystem& Sub)
        {
            const auto* Door = R52Door(Sub.GetPlayWorld(), Id);
            return Door && !Door->IsMoving() && Door->IsOpen() == Open &&
                (Open ? Door->GetOpenFraction() >= 0.99f : Door->GetOpenFraction() <= 0.01f);
        }, 15);
}

FStepDef R52Knock(const FString& Name, FR52Home Home, bool Admit)
{
    return PWAcceptance::MakeInteractStep(Name,
        [Home](UPWAcceptanceSubsystem& Sub) -> AActor* { return R52Door(Sub.GetPlayWorld(), Home.Door); },
        [Home, Admit](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            const auto* Door = R52Door(Sub.GetPlayWorld(), Home.Door);
            const auto* Def = PerfectWorld::FindDoor(TCHAR_TO_UTF8(Home.Door));
            if (!Life || !Door || !Def || !Sub.GetInteraction()) return false;
            Detail = Sub.GetInteraction()->GetNotice().ToString();
            auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
            const auto* Intent = Presence ? Presence->FindIntent(FName(Home.Person)) : nullptr;
            const bool RefusalState = Admit || (Intent && (Life->GetMinuteOfDay() >= 22 * 60
                ? Intent->State == PerfectWorld::NpcState::Sleep : Intent->AnchorId != Def->LocationId));
            return RefusalState && !Door->IsOpen() && Life->IsHostHomeForAcceptance(*Def) == Admit &&
                Detail.Contains(Admit ? TEXT("Someone answers") : TEXT("Nobody answers"));
        }, nullptr, 15);
}

FStepDef R52Heights(const FString& Name, FR52Home Home)
{
    return R52Action(Name, [Home](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        auto* Npc = Presence ? Presence->FindPerson(FName(Home.Person)) : nullptr;
        if (!Npc) return FStepOutcome::Fail(TEXT("R52 resident not physically present"));
        Npc->ApplyPose(FPWNpcPose{});
        TArray<UStaticMeshComponent*> Meshes;
        Npc->GetComponents<UStaticMeshComponent>(Meshes);
        FBox Body(ForceInit);
        for (const auto* Mesh : Meshes) Body += Mesh->Bounds.GetBox();
        const double Capsule = Npc->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2;
        const double Visible = Body.GetSize().Z;
        const FString Detail = FString::Printf(TEXT("R52 %s: authored %.1f, capsule %.2f, visible body %.2f cm"), Home.Person, Home.Height, Capsule, Visible);
        return FMath::Abs(Capsule - Home.Height) <= 1 && FMath::Abs(Visible - Home.Height) <= 1
            ? FStepOutcome::Pass(Detail) : FStepOutcome::Fail(Detail);
    });
}

FStepDef R52Talk(const FString& Name, FR52Home Home)
{
    return PWAcceptance::MakeInteractStep(Name,
        [Home](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
            return Presence ? Presence->FindPerson(FName(Home.Person)) : nullptr;
        }, [Home](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
            const auto* Memory = Life ? Life->FindConversation(FName(Home.Person)) : nullptr;
            Detail = TEXT("R52 physical Talk target opened the production panel and core session");
            return Talk && Talk->IsOpen() && Memory && Memory->Open && Memory->PersonId == TCHAR_TO_UTF8(Home.Person);
        }, nullptr, 15);
}

FStepDef R52Choices(const FString& Name, FR52Home Home, bool Continue)
{
    return R52Action(Name, [Home, Continue](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        if (!Life || !Talk || !Talk->IsOpen()) return FStepOutcome::Fail(TEXT("R52 no physical conversation"));
        const auto* Memory = Life->FindConversation(FName(Home.Person));
        const bool Remembered = Memory && PerfectWorld::HasConversationFact(*Memory, Home.Fact);
        if (Continue && (!Remembered || Memory->NodeId.find("_remembered") == std::string::npos))
            return FStepOutcome::Fail(TEXT("R52 Continue did not select remembered greeting"));
        if (!Remembered) Talk->ChooseOption(FName(UTF8_TO_TCHAR((std::string("choice.") + Home.Tail + ".agree").c_str())));
        const auto Before = Life->GetNow();
        if (!Continue) Talk->ChooseOption(FName(UTF8_TO_TCHAR((std::string("choice.") + Home.Tail + ".talk").c_str())));
        const auto AfterTalk = Life->GetNow();
        Talk->LeaveConversation();
        Memory = Life->FindConversation(FName(Home.Person));
        const auto Cost = Continue ? 0 : Home.Upper ? 15 : 10;
        if (!Memory || !PerfectWorld::HasConversationFact(*Memory, Home.Fact) || Memory->Open || Talk->IsOpen() ||
            !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty() || AfterTalk - Before != Cost || Life->GetNow() != AfterTalk)
            return FStepOutcome::Fail(TEXT("R52 agreement, paid Talk or free Leave differed"));
        return FStepOutcome::Pass(TEXT("R52 agreement remembered, authored Talk cost charged once, Leave free and closed"));
    });
}

FStepDef R52Headroom(const FString& Name)
{
    return R52Action(Name, [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        UWorld* World = Sub.GetPlayWorld();
        FCollisionQueryParams Params(FName(TEXT("R52Landing")), false);
        for (TActorIterator<APawn> It(World); It; ++It) Params.AddIgnoredActor(*It);
        double Lowest = 1e9;
        for (double U : {550.0, 580.0}) for (double V : {700.0, 740.0})
        {
            const auto Feet = R52Point(true, U, V);
            FHitResult Floor, Ceiling;
            if (!World->LineTraceSingleByChannel(Floor, Feet + FVector(0, 0, 20), Feet - FVector(0, 0, 20), ECC_Pawn, Params) ||
                FMath::Abs(Floor.ImpactPoint.Z - Feet.Z) > 1.5 ||
                !World->LineTraceSingleByChannel(Ceiling, Feet + FVector(0, 0, 2), Feet + FVector(0, 0, 400), ECC_Pawn, Params))
                return FStepOutcome::Fail(TEXT("R52 landing has no real floor or ceiling"));
            Lowest = FMath::Min(Lowest, Ceiling.ImpactPoint.Z - Floor.ImpactPoint.Z);
        }
        const FString Detail = FString::Printf(TEXT("R52 upper landing minimum measured headroom %.2f cm (220 required)"), Lowest);
        return Lowest >= 220 ? FStepOutcome::Pass(Detail) : FStepOutcome::Fail(Detail);
    });
}

void R52ApproachPerson(TArray<FStepDef>& Out, const FString& Tag, FR52Home Home)
{
    // These lines stay in the kit's free approach zones and stop 100/115 cm short of the standing resident.
    Out.Add(R52Walk(Tag + TEXT("_column"), R52Point(Home.Upper, Home.Upper ? 715 : 250, 500)));
    Out.Add(R52Walk(Tag + TEXT("_cross"), R52Point(Home.Upper, Home.Upper ? 865 : 220, 500)));
    Out.Add(R52Walk(Tag + TEXT("_person"), R52Point(Home.Upper, Home.Upper ? 865 : 220, 642)));
    Out.Add(R52Heights(Tag + TEXT("_height"), Home));
    Out.Add(R52Talk(Tag + TEXT("_Talk"), Home));
}

void R52Visit(TArray<FStepDef>& Out, const FString& Tag, FR52Home Home, EPWCameraMode Camera)
{
    Out.Add(R52Clock(Tag + TEXT("_visiting_clock"), 16));
    Out.Add(R52Action(Tag + TEXT("_camera"), [Camera](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (!Sub.GetPawn()) return FStepOutcome::Fail(TEXT("R52 no pawn"));
        Sub.GetPawn()->SetCameraMode(Camera);
        return Sub.GetPawn()->GetCameraMode() == Camera ? FStepOutcome::Pass(TEXT("R52 camera active")) : FStepOutcome::Fail(TEXT("R52 camera differs"));
    }));
    if (Home.Upper) Out.Add(R52Headroom(Tag + TEXT("_landing_headroom")));
    Out.Add(R52Knock(Tag + TEXT("_knock_admission"), Home, true));
    Out.Add(R52DoorMove(Tag + TEXT("_open_sweep"), Home.Door, true));
    Out.Add(R52Walk(Tag + TEXT("_enter"), R52Point(Home.Upper, Home.Upper ? 800 : 250, Home.Upper ? 715 : 170)));
    R52ApproachPerson(Out, Tag, Home);
    Out.Add(PWAcceptance::MakeShotStep(Tag + TEXT("_conversation")));
    Out.Add(R52Choices(Tag + TEXT("_agree_paid_Talk_Leave"), Home, false));
    Out.Add(R52Walk(Tag + TEXT("_return_cross"), R52Point(Home.Upper, Home.Upper ? 865 : 220, 500)));
    Out.Add(R52Walk(Tag + TEXT("_return_column"), R52Point(Home.Upper, Home.Upper ? 715 : 250, 500)));
    Out.Add(R52Walk(Tag + TEXT("_return_door"), R52Point(Home.Upper, Home.Upper ? 715 : 250, Home.Upper ? 715 : 170)));
    if (Home.Upper) Out.Add(R52Walk(Tag + TEXT("_clear_sweep"), R52Point(true, 800, 715)));
    Out.Add(R52DoorMove(Tag + TEXT("_close_inside"), Home.Door, false));
    Out.Add(R52Clock(Tag + TEXT("_sleeping_clock"), 23));
    // Admission has expired, the resident sleeps and visiting hours are over. Inside-side exit must still work.
    Out.Add(R52DoorMove(Tag + TEXT("_unrestricted_exit_open"), Home.Door, true));
    Out.Add(R52Walk(Tag + TEXT("_exit"), R52Point(Home.Upper, Home.Upper ? 550 : 250, Home.Upper ? 715 : -85)));
    Out.Add(R52DoorMove(Tag + TEXT("_shut_outside"), Home.Door, false));
    Out.Add(PWAcceptance::MakeShotStep(Tag + TEXT("_exited")));
    if (Home.Upper)
    {
        Out.Add(R52Walk(Tag + TEXT("_down_stair"), R52Point(true, 550, 195, 15)));
        Out.Add(R52Walk(Tag + TEXT("_shared_exit"), R52Point(true, 550, -85, 15)));
        Out.Add(R52Walk(Tag + TEXT("_shared_return"), R52Point(true, 550, 195, 15)));
        Out.Add(R52Walk(Tag + TEXT("_up_stair"), R52Point(true, 550, 715)));
    }
}

bool R52Facts(const PerfectWorld::WorldState& World)
{
    for (const auto& Home : {R52Old, R52Upper})
    {
        const auto Found = World.Conversations.find(TCHAR_TO_UTF8(Home.Person));
        if (Found == World.Conversations.end() || Found->second.Open || !PerfectWorld::HasConversationFact(Found->second, Home.Fact)) return false;
    }
    return true;
}

bool R52Read(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Saved)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Saved).HashVerified;
}
}

void PWAcceptance::AppendHomesRound5Steps(TArray<FStepDef>& Out)
{
    const auto State = MakeShared<FR52Restart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(R52Action(TEXT("R52_restart_phase"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        State->Continue = Sub.GetArgs().Start == TEXT("continue") || (Sub.GetArgs().Start == TEXT("auto") && UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot() >= 0);
        if (State->Continue && (!R52Read(Sub, State->Saved) || !R52Facts(State->Saved.World)))
            return FStepOutcome::Fail(TEXT("R52 Continue slot lacks both remembered agreements"));
        return FStepOutcome::Pass(State->Continue ? TEXT("R52 fresh-process Continue; slot captured before load") : TEXT("R52 new physical visits"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(R52Action(TEXT("R52_start_or_resume"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Pawn = Sub.GetPawn();
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Pawn || !Life) return FStepOutcome::Fail(TEXT("R52 missing pawn or life"));
        if (State->Continue)
        {
            const auto& P = State->Saved.Player.PositionCm;
            const FVector Saved(static_cast<double>(P[0]), static_cast<double>(P[1]), static_cast<double>(P[2]));
            const FVector Endpoint = R52Point(true, 715, 715) + FVector(0, 0, PWPlayer::CapsuleHalfHeight);
            const auto Camera = State->Saved.Player.Camera == PerfectWorld::CameraMode::FirstPerson ? EPWCameraMode::FirstPerson : EPWCameraMode::ThirdPerson;
            if (!R52Facts(Life->GetWorldState()) || FVector::Dist(Saved, Endpoint) > 10 || FVector::Dist(Pawn->GetActorLocation(), Saved) > 5 || Pawn->GetCameraMode() != Camera)
                return FStepOutcome::Fail(TEXT("R52 Continue lost agreement, upper placement or camera"));
            for (const auto& Home : {R52Old, R52Upper})
                if (Life->GetWorldState().Conversations.find(TCHAR_TO_UTF8(Home.Person))->second != State->Saved.World.Conversations.find(TCHAR_TO_UTF8(Home.Person))->second)
                    return FStepOutcome::Fail(TEXT("R52 Continue memory differs from pre-load slot"));
        }
        else if (Rt.Phase == 0)
        {
            if (!Pawn->TeleportTo(R52Point(false, 250, -85) + FVector(0, 0, PWPlayer::CapsuleHalfHeight + 2), FRotator(0, 90, 0)))
                return FStepOutcome::Fail(TEXT("R52 declared old-house start refused"));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        return Pawn->GetCharacterMovement()->IsMovingOnGround() ? FStepOutcome::Pass(TEXT("R52 supported start/resume")) : FStepOutcome::Wait();
    }));
    TArray<FStepDef> New;
    New.Add(R52Clock(TEXT("R52_old_absent_clock"), 9));
    New.Add(R52Knock(TEXT("R52_old_absent_refusal"), R52Old, false));
    New.Add(R52Clock(TEXT("R52_old_asleep_clock"), 23));
    New.Add(R52Knock(TEXT("R52_old_asleep_refusal"), R52Old, false));
    R52Visit(New, TEXT("R52_old_first"), R52Old, EPWCameraMode::FirstPerson);
    R52Visit(New, TEXT("R52_old_third"), R52Old, EPWCameraMode::ThirdPerson);
    // Walk between homes on the road, then the public passage and actual stair. No second start teleport.
    New.Add(R52Walk(TEXT("R52_old_to_road"), FVector(4750, 100, PWLane::GroundZ)));
    New.Add(R52Walk(TEXT("R52_lane_to_flats"), FVector(-750, 100, PWLane::GroundZ)));
    New.Add(R52Walk(TEXT("R52_shared_front"), R52Point(true, 550, -85, 15)));
    New.Add(R52DoorMove(TEXT("R52_shared_open"), PWLane::R52SharedDoor, true));
    New.Add(R52Walk(TEXT("R52_stair_foot"), R52Point(true, 550, 195, 15)));
    New.Add(R52Walk(TEXT("R52_climb_to_upper"), R52Point(true, 550, 715)));
    New.Add(R52Headroom(TEXT("R52_upper_landing_headroom")));
    New.Add(R52Clock(TEXT("R52_upper_absent_clock"), 9));
    New.Add(R52Knock(TEXT("R52_upper_absent_refusal"), R52Upper, false));
    New.Add(R52Clock(TEXT("R52_upper_asleep_clock"), 23));
    New.Add(R52Knock(TEXT("R52_upper_asleep_refusal"), R52Upper, false));
    R52Visit(New, TEXT("R52_upper_first"), R52Upper, EPWCameraMode::FirstPerson);
    R52Visit(New, TEXT("R52_upper_third"), R52Upper, EPWCameraMode::ThirdPerson);
    New.Add(R52Clock(TEXT("R52_save_visiting_clock"), 16));
    New.Add(R52Knock(TEXT("R52_save_admission"), R52Upper, true));
    New.Add(R52DoorMove(TEXT("R52_save_open"), PWLane::R52UpperDoor, true));
    New.Add(R52Walk(TEXT("R52_save_enter"), R52Point(true, 715, 715)));
    New.Add(R52Action(TEXT("R52_save_agreements"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        bool Saved = false;
        if (!Sub.GetHud()) return FStepOutcome::Fail(TEXT("R52 no HUD"));
        const auto Why = Sub.GetHud()->SaveActiveGame(Saved);
        PerfectWorld::WorldSaveData Read;
        return Saved && R52Read(Sub, Read) && R52Facts(Read.World) ? FStepOutcome::Pass(TEXT("R52 both agreements saved; run fresh process Continue")) : FStepOutcome::Fail(Why.ToString());
    }));
    for (auto& Def : New) Out.Add(R52Phase(MoveTemp(Def), State));
    TArray<FStepDef> Continued;
    R52ApproachPerson(Continued, TEXT("R52_continue"), R52Upper);
    Continued.Add(R52Choices(TEXT("R52_continue_remembered_Leave"), R52Upper, true));
    Continued.Add(MakeShotStep(TEXT("R52_continue_remembered")));
    Continued.Add(R52Walk(TEXT("R52_continue_back_cross"), R52Point(true, 865, 500)));
    Continued.Add(R52Walk(TEXT("R52_continue_back_column"), R52Point(true, 715, 500)));
    Continued.Add(R52Walk(TEXT("R52_continue_back_door"), R52Point(true, 715, 715)));
    Continued.Add(R52Walk(TEXT("R52_continue_exit"), R52Point(true, 550, 715)));
    Continued.Add(R52Walk(TEXT("R52_continue_descend"), R52Point(true, 550, 195, 15)));
    Continued.Add(R52Walk(TEXT("R52_continue_shared_exit"), R52Point(true, 550, -85, 15)));
    for (auto& Def : Continued) Out.Add(R52Phase(MoveTemp(Def), State, true));
}
