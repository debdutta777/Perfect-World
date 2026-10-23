#include "PerfectWorldAcceptanceHomesR6.h"

#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "LaneFrontageCatalogue.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceDoorBell.h"
#include "PerfectWorldAcceptanceHomes.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldDoorActor.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcCharacter.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldResidentsR6Bindings.h"
#include "WorldSave.h"

// homes_round6 (R6-04): the alley house (Madhabi Das) and the stair flats (Arup Basu), both on the -Y side. Fresh-process new/continue
// pair; only the declared new-phase start teleports. Day indices count from the campaign Monday (0). Nothing fabricates admission or an
// agreement: each visit rings the real bell, opens the real leaf and walks in. The two homes share one interior, so the waypoints are
// derived from the portable layout (door axis, bed_side, the cross line just inside the internal doorway), never fixed numbers.
namespace
{
namespace LI = PerfectWorld::LaneInterior;
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FR64Scn
{
    const PWLane::FR64Home* Home;
    const char* Tail;
    const char* Fact;
    int32 TalkCost;
    double DoorU = 250, BedU = 120, BedV = 642, CrossV = 500, InsideV = 170;
};
FR64Scn R64Make(const PWLane::FR64Home& Home, const char* Tail, const char* Fact, int32 Cost)
{
    FR64Scn Out{&Home, Tail, Fact, Cost};
    const auto* Row = LI::FindFrontageById(std::string(TCHAR_TO_UTF8(Home.Building)));
    if (!Row) return Out;
    const auto Layout = LI::GenerateInteriorLayout(LI::MakeFrontageInput(*Row, 15, 300));
    const auto* Bed = LI::FindAnchor(Layout, Layout.BuildingId + ".anchor.bed_side");
    for (const auto& Opening : Layout.Openings)
    {
        if (Opening.DoorId == std::string(TCHAR_TO_UTF8(Home.Door))) Out.DoorU = (Opening.Plan.U0 + Opening.Plan.U1) * 0.5;
        if (Opening.Kind == LI::OpeningKind::Doorway && Opening.RoomA != "outside") Out.CrossV = Opening.Plan.V1 + 58;
    }
    if (Bed) { Out.BedU = Bed->U; Out.BedV = Bed->V; }
    return Out;
}
const FR64Scn& R64Alley()
{
    static const FR64Scn Value = R64Make(PWLane::R64MadhabiHome(), "madhabi_das", "fact.madhabi_das.ward_records_agreed", 15);
    return Value;
}
const FR64Scn& R64Stair()
{
    static const FR64Scn Value = R64Make(PWLane::R64ArupHome(), "arup_basu", "fact.arup_basu.parcel_round_agreed", 10);
    return Value;
}

struct FR64Restart
{
    bool Continue = false;
    PerfectWorld::WorldSaveData Saved;
};

FStepDef R64Action(const FString& Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Out;
    Out.Name = Name;
    Out.TimeoutSeconds = 20;
    Out.Run = MoveTemp(Run);
    return Out;
}

FStepDef R64Phase(FStepDef Def, TSharedRef<FR64Restart> State, bool Continue = false)
{
    auto Run = MoveTemp(Def.Run);
    Def.Run = [Run = MoveTemp(Run), State, Continue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    { return State->Continue == Continue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("other fresh-process phase")); };
    return Def;
}

APWDoorActor* R64Door(UWorld* World, const TCHAR* Id)
{
    for (TActorIterator<APWDoorActor> It(World); It; ++It) if (It->GetDoorId() == FName(Id)) return *It;
    return nullptr;
}

FVector R64Point(const FR64Scn& Home, double U, double V, double Floor = 15)
{
    PWLane::FLaneBuilding Building;
    if (!PWLane::MakeResidentsR6Building(*Home.Home, Building)) return FVector::ZeroVector;
    return Building.Frame.Pt(U, V, Floor);
}

FStepDef R64Walk(const FString& Name, const FVector& Feet, double Tolerance = 8)
{
    auto Def = R64Action(Name, [Feet, Tolerance](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Pawn = Sub.GetPawn();
        if (!Pawn) return FStepOutcome::Fail(TEXT("R64 no pawn"));
        const auto Result = Sub.TickWalk(Feet, Tolerance, Rt);
        if (Result.State != PWAcceptance::EStepState::Passed) return Result;
        const double Actual = Pawn->GetActorLocation().Z - Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        if (!Pawn->GetCharacterMovement()->IsMovingOnGround() || FMath::Abs(Actual - Feet.Z) > 4)
        { Rt.Note = FString::Printf(TEXT("R64 XY reached; supported feet %.2f, expected %.2f"), Actual, Feet.Z); return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("R64 physically walked; supported feet Z %.2f"), Actual));
    });
    Def.TimeoutSeconds = 90;
    return Def;
}

// Reuses the existing detached reducer advance and production reconciliation; never sets a clock field. Day 0 is Monday.
FStepDef R64Clock(const FString& Name, int32 Day, int32 Hour, int32 Minute)
{
    return PWAcceptance::Homes::HmClockStep(Name, Day, Hour, Minute);
}

FStepDef R64DoorMove(const FString& Name, const TCHAR* Id, bool Open)
{
    return PWAcceptance::MakeInteractStep(Name,
        [Id](UPWAcceptanceSubsystem& Sub) -> AActor* { return R64Door(Sub.GetPlayWorld(), Id); },
        [Id, Open](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Door = R64Door(Sub.GetPlayWorld(), Id);
            if (!Door || Door->IsMoving() || Door->IsOpen() != Open) return false;
            Detail = TEXT("R64 real leaf completed its sweep without obstruction");
            return Open ? Door->GetOpenFraction() >= 0.99f : Door->GetOpenFraction() <= 0.01f;
        }, [Id, Open](UPWAcceptanceSubsystem& Sub)
        {
            const auto* Door = R64Door(Sub.GetPlayWorld(), Id);
            return Door && !Door->IsMoving() && Door->IsOpen() == Open &&
                (Open ? Door->GetOpenFraction() >= 0.99f : Door->GetOpenFraction() <= 0.01f);
        }, 15);
}

// The ring is E on the home's calling bell. An admitted ring unlatches the door; the following R64DoorMove presses the leaf.
FStepDef R64Ring(const FString& Name, const FR64Scn& Home, bool Admit)
{
    const TCHAR* DoorId = Home.Home->Door;
    const TCHAR* Person = Home.Home->Person;
    return PWAcceptance::MakeInteractStep(Name,
        [DoorId](UPWAcceptanceSubsystem& Sub) -> AActor* { return PWAcceptance::DoorBell::FindBell(Sub.GetPlayWorld(), FName(DoorId)); },
        [DoorId, Person, Admit](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            const auto* Door = R64Door(Sub.GetPlayWorld(), DoorId);
            const auto* Def = PerfectWorld::FindDoor(TCHAR_TO_UTF8(DoorId));
            if (!Life || !Door || !Def || !Sub.GetInteraction()) return false;
            Detail = Sub.GetInteraction()->GetNotice().ToString();
            auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
            const auto* Intent = Presence ? Presence->FindIntent(FName(Person)) : nullptr;
            // A refusal must be the real thing: the resident is away (anchor elsewhere) by day, or asleep by night.
            const bool RefusalState = Admit || (Intent && (Life->GetMinuteOfDay() >= 22 * 60
                ? Intent->State == PerfectWorld::NpcState::Sleep : Intent->AnchorId != Def->LocationId));
            return RefusalState && !Door->IsOpen() && Life->IsHostHomeForAcceptance(*Def) == Admit &&
                Detail.Contains(Admit ? TEXT("Someone is coming") : TEXT("Nobody answers"));
        }, nullptr, 15);
}

FStepDef R64Heights(const FString& Name, const FR64Scn& Home)
{
    const TCHAR* Person = Home.Home->Person;
    const double Height = Home.Home->HeightCm;
    return R64Action(Name, [Person, Height](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        auto* Npc = Presence ? Presence->FindPerson(FName(Person)) : nullptr;
        if (!Npc) return FStepOutcome::Fail(TEXT("R64 resident not physically present"));
        Npc->ApplyPose(FPWNpcPose{});
        TArray<UStaticMeshComponent*> Meshes;
        Npc->GetComponents<UStaticMeshComponent>(Meshes);
        FBox Body(ForceInit);
        for (const auto* Mesh : Meshes) Body += Mesh->Bounds.GetBox();
        const double Capsule = Npc->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2;
        const double Visible = Body.GetSize().Z;
        const FString Detail = FString::Printf(TEXT("R64 %s: authored %.1f, capsule %.2f, visible body %.2f cm"), Person, Height, Capsule, Visible);
        return FMath::Abs(Capsule - Height) <= 1 && FMath::Abs(Visible - Height) <= 1 ? FStepOutcome::Pass(Detail) : FStepOutcome::Fail(Detail);
    });
}

FStepDef R64Talk(const FString& Name, const FR64Scn& Home)
{
    const TCHAR* Person = Home.Home->Person;
    return PWAcceptance::MakeInteractStep(Name,
        [Person](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
            return Presence ? Presence->FindPerson(FName(Person)) : nullptr;
        }, [Person](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
            auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
            const auto* Memory = Life ? Life->FindConversation(FName(Person)) : nullptr;
            Detail = TEXT("R64 physical Talk target opened the production panel and core session");
            return Talk && Talk->IsOpen() && Memory && Memory->Open && Memory->PersonId == TCHAR_TO_UTF8(Person);
        }, nullptr, 15);
}

FStepDef R64Choices(const FString& Name, const FR64Scn& Home, bool Continue)
{
    return R64Action(Name, [Home, Continue](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const TCHAR* Person = Home.Home->Person;
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
        if (!Life || !Talk || !Talk->IsOpen()) return FStepOutcome::Fail(TEXT("R64 no physical conversation"));
        const auto* Memory = Life->FindConversation(FName(Person));
        const bool Remembered = Memory && PerfectWorld::HasConversationFact(*Memory, Home.Fact);
        if (Continue && (!Remembered || Memory->NodeId.find("_remembered") == std::string::npos))
            return FStepOutcome::Fail(TEXT("R64 Continue did not select the remembered greeting"));
        if (!Remembered) Talk->ChooseOption(FName(UTF8_TO_TCHAR((std::string("choice.") + Home.Tail + ".agree").c_str())));
        const auto Before = Life->GetNow();
        if (!Continue) Talk->ChooseOption(FName(UTF8_TO_TCHAR((std::string("choice.") + Home.Tail + ".talk").c_str())));
        const auto AfterTalk = Life->GetNow();
        Talk->LeaveConversation();
        Memory = Life->FindConversation(FName(Person));
        const auto Cost = Continue ? 0 : Home.TalkCost;
        if (!Memory || !PerfectWorld::HasConversationFact(*Memory, Home.Fact) || Memory->Open || Talk->IsOpen() ||
            !PerfectWorld::OpenConversationPersonId(Life->GetWorldState()).empty() || AfterTalk - Before != Cost || Life->GetNow() != AfterTalk)
            return FStepOutcome::Fail(TEXT("R64 agreement, paid Talk or free Leave differed"));
        return FStepOutcome::Pass(TEXT("R64 agreement remembered, authored Talk cost charged once, Leave free and closed"));
    });
}

FStepDef R64Camera(const FString& Name, EPWCameraMode Camera)
{
    return R64Action(Name, [Camera](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (!Sub.GetPawn()) return FStepOutcome::Fail(TEXT("R64 no pawn"));
        Sub.GetPawn()->SetCameraMode(Camera);
        return Sub.GetPawn()->GetCameraMode() == Camera ? FStepOutcome::Pass(TEXT("R64 camera active")) : FStepOutcome::Fail(TEXT("R64 camera differs"));
    });
}

// Straight down the doorway axis, then 100 cm beside the standing resident: free approach zones of the kit layout.
void R64Approach(TArray<FStepDef>& Out, const FString& Tag, const FR64Scn& Home)
{
    Out.Add(R64Walk(Tag + TEXT("_axis"), R64Point(Home, Home.DoorU, Home.CrossV)));
    Out.Add(R64Walk(Tag + TEXT("_cross"), R64Point(Home, Home.BedU + 100, Home.CrossV)));
    Out.Add(R64Walk(Tag + TEXT("_person"), R64Point(Home, Home.BedU + 100, Home.BedV)));
    Out.Add(R64Heights(Tag + TEXT("_height"), Home));
    Out.Add(R64Talk(Tag + TEXT("_Talk"), Home));
}

void R64Retreat(TArray<FStepDef>& Out, const FString& Tag, const FR64Scn& Home)
{
    Out.Add(R64Walk(Tag + TEXT("_return_cross"), R64Point(Home, Home.BedU + 100, Home.CrossV)));
    Out.Add(R64Walk(Tag + TEXT("_return_axis"), R64Point(Home, Home.DoorU, Home.CrossV)));
    Out.Add(R64Walk(Tag + TEXT("_return_door"), R64Point(Home, Home.DoorU, Home.InsideV)));
}

void R64Visit(TArray<FStepDef>& Out, const FString& Tag, const FR64Scn& Home, EPWCameraMode Camera, int32 Day)
{
    const TCHAR* Door = Home.Home->Door;
    Out.Add(R64Clock(Tag + TEXT("_visiting_clock"), Day, 16, 30));
    Out.Add(R64Camera(Tag + TEXT("_camera"), Camera));
    Out.Add(R64Ring(Tag + TEXT("_ring_admission"), Home, true));
    Out.Add(R64DoorMove(Tag + TEXT("_open_sweep"), Door, true));
    Out.Add(R64Walk(Tag + TEXT("_enter"), R64Point(Home, Home.DoorU, Home.InsideV)));
    R64Approach(Out, Tag, Home);
    Out.Add(PWAcceptance::MakeShotStep(Tag + TEXT("_conversation")));
    Out.Add(R64Choices(Tag + TEXT("_agree_paid_Talk_Leave"), Home, false));
    R64Retreat(Out, Tag, Home);
    Out.Add(R64DoorMove(Tag + TEXT("_close_inside"), Door, false));
    Out.Add(R64Clock(Tag + TEXT("_sleeping_clock"), Day, 23, 0));
    // Admission has expired, the resident sleeps and visiting hours are over. Leaving from inside must still work.
    Out.Add(R64DoorMove(Tag + TEXT("_unrestricted_exit_open"), Door, true));
    Out.Add(R64Walk(Tag + TEXT("_exit"), R64Point(Home, Home.DoorU, -85)));
    Out.Add(R64DoorMove(Tag + TEXT("_shut_outside"), Door, false));
    Out.Add(PWAcceptance::MakeShotStep(Tag + TEXT("_exited")));
}

// Pavement transit at |Y| 325, as R6-01 walked it. Both homes are on the -Y side.
void R64Transit(TArray<FStepDef>& Out, const FString& Tag, const FR64Scn& From, const FR64Scn& To)
{
    const FVector Start = R64Point(From, From.DoorU, -85);
    const FVector End = R64Point(To, To.DoorU, -85);
    Out.Add(R64Walk(Tag + TEXT("_to_pavement"), FVector(Start.X, -325, 15)));
    Out.Add(R64Walk(Tag + TEXT("_along_pavement"), FVector(End.X, -325, 15)));
    Out.Add(R64Walk(Tag + TEXT("_to_door"), End));
}

bool R64Facts(const PerfectWorld::WorldState& World)
{
    for (const FR64Scn* Home : {&R64Alley(), &R64Stair()})
    {
        const auto Found = World.Conversations.find(TCHAR_TO_UTF8(Home->Home->Person));
        if (Found == World.Conversations.end() || Found->second.Open || !PerfectWorld::HasConversationFact(Found->second, Home->Fact)) return false;
    }
    return true;
}

bool R64Read(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Saved)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Saved).HashVerified;
}
}

void PWAcceptance::AppendHomesRound6Steps(TArray<FStepDef>& Out)
{
    const FR64Scn& Alley = R64Alley();
    const FR64Scn& Stair = R64Stair();
    const auto State = MakeShared<FR64Restart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(R64Action(TEXT("R64_restart_phase"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        State->Continue = Sub.GetArgs().Start == TEXT("continue") ||
            (Sub.GetArgs().Start == TEXT("auto") && UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot() >= 0);
        if (State->Continue && (!R64Read(Sub, State->Saved) || !R64Facts(State->Saved.World)))
            return FStepOutcome::Fail(TEXT("R64 Continue slot lacks both remembered agreements"));
        return FStepOutcome::Pass(State->Continue ? TEXT("R64 fresh-process Continue; slot captured before load") : TEXT("R64 new physical visits"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(R64Action(TEXT("R64_start_or_resume"), [State, &Alley, &Stair](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Pawn = Sub.GetPawn();
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Pawn || !Life) return FStepOutcome::Fail(TEXT("R64 missing pawn or life"));
        if (State->Continue)
        {
            const auto& P = State->Saved.Player.PositionCm;
            const FVector Saved(static_cast<double>(P[0]), static_cast<double>(P[1]), static_cast<double>(P[2]));
            const FVector Endpoint = R64Point(Stair, Stair.DoorU, Stair.InsideV) + FVector(0, 0, PWPlayer::CapsuleHalfHeight);
            const auto Camera = State->Saved.Player.Camera == PerfectWorld::CameraMode::FirstPerson ? EPWCameraMode::FirstPerson : EPWCameraMode::ThirdPerson;
            if (!R64Facts(Life->GetWorldState()) || FVector::Dist(Saved, Endpoint) > 10 || FVector::Dist(Pawn->GetActorLocation(), Saved) > 5 ||
                Pawn->GetCameraMode() != Camera)
                return FStepOutcome::Fail(TEXT("R64 Continue lost agreement, stair-flats placement or camera"));
            for (const FR64Scn* Home : {&Alley, &Stair})
                if (Life->GetWorldState().Conversations.find(TCHAR_TO_UTF8(Home->Home->Person))->second !=
                    State->Saved.World.Conversations.find(TCHAR_TO_UTF8(Home->Home->Person))->second)
                    return FStepOutcome::Fail(TEXT("R64 Continue memory differs from the pre-load slot"));
        }
        else if (Rt.Phase == 0)
        {
            // The sole declared teleport: just outside the alley house's door, facing it (the -Y side faces -Y).
            if (!Pawn->TeleportTo(R64Point(Alley, Alley.DoorU, -85) + FVector(0, 0, PWPlayer::CapsuleHalfHeight + 2), FRotator(0, -90, 0)))
                return FStepOutcome::Fail(TEXT("R64 declared alley-house start refused"));
            Pawn->GetCharacterMovement()->Velocity = FVector::ZeroVector;
            Rt.NextPhase();
        }
        return Pawn->GetCharacterMovement()->IsMovingOnGround() ? FStepOutcome::Pass(TEXT("R64 supported start/resume")) : FStepOutcome::Wait();
    }));

    TArray<FStepDef> New;
    // Tuesday: Madhabi is at her desk at 10:00 and asleep at 23:00. The real bell is rung; the door stays shut.
    New.Add(R64Clock(TEXT("R64_alley_absent_clock"), 1, 10, 0));
    New.Add(R64Ring(TEXT("R64_alley_absent_refusal"), Alley, false));
    New.Add(R64Clock(TEXT("R64_alley_asleep_clock"), 1, 23, 0));
    New.Add(R64Ring(TEXT("R64_alley_asleep_refusal"), Alley, false));
    R64Visit(New, TEXT("R64_alley_first"), Alley, EPWCameraMode::FirstPerson, 2);
    R64Visit(New, TEXT("R64_alley_third"), Alley, EPWCameraMode::ThirdPerson, 3);
    R64Transit(New, TEXT("R64_alley_to_stair"), Alley, Stair);
    // Friday: Arup is at the depot at 10:00 and asleep at 23:00.
    New.Add(R64Clock(TEXT("R64_stair_absent_clock"), 4, 10, 0));
    New.Add(R64Ring(TEXT("R64_stair_absent_refusal"), Stair, false));
    New.Add(R64Clock(TEXT("R64_stair_asleep_clock"), 4, 23, 0));
    New.Add(R64Ring(TEXT("R64_stair_asleep_refusal"), Stair, false));
    R64Visit(New, TEXT("R64_stair_first"), Stair, EPWCameraMode::FirstPerson, 5);
    R64Visit(New, TEXT("R64_stair_third"), Stair, EPWCameraMode::ThirdPerson, 6);
    // The saving visit: next Monday 16:30, third person, inside the stair flats with both agreements already made.
    New.Add(R64Clock(TEXT("R64_save_visiting_clock"), 7, 16, 30));
    New.Add(R64Ring(TEXT("R64_save_admission"), Stair, true));
    New.Add(R64DoorMove(TEXT("R64_save_open"), Stair.Home->Door, true));
    New.Add(R64Walk(TEXT("R64_save_enter"), R64Point(Stair, Stair.DoorU, Stair.InsideV)));
    New.Add(R64Action(TEXT("R64_save_agreements"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        bool Saved = false;
        if (!Sub.GetHud()) return FStepOutcome::Fail(TEXT("R64 no HUD"));
        const auto Why = Sub.GetHud()->SaveActiveGame(Saved);
        PerfectWorld::WorldSaveData Read;
        return Saved && R64Read(Sub, Read) && R64Facts(Read.World) ? FStepOutcome::Pass(TEXT("R64 both agreements saved; run the fresh-process Continue"))
            : FStepOutcome::Fail(Why.ToString());
    }));
    for (auto& Def : New) Out.Add(R64Phase(MoveTemp(Def), State));

    TArray<FStepDef> Continued;
    R64Approach(Continued, TEXT("R64_continue_stair"), Stair);
    Continued.Add(R64Choices(TEXT("R64_continue_stair_remembered_Leave"), Stair, true));
    Continued.Add(PWAcceptance::MakeShotStep(TEXT("R64_continue_stair_remembered")));
    R64Retreat(Continued, TEXT("R64_continue_stair"), Stair);
    Continued.Add(R64Walk(TEXT("R64_continue_stair_exit"), R64Point(Stair, Stair.DoorU, -85)));
    Continued.Add(R64DoorMove(TEXT("R64_continue_stair_shut"), Stair.Home->Door, false));
    R64Transit(Continued, TEXT("R64_continue_to_alley"), Stair, Alley);
    // Same evening, still inside the visiting window: the alley house remembers its own agreement.
    Continued.Add(R64Ring(TEXT("R64_continue_alley_ring"), Alley, true));
    Continued.Add(R64DoorMove(TEXT("R64_continue_alley_open"), Alley.Home->Door, true));
    Continued.Add(R64Walk(TEXT("R64_continue_alley_enter"), R64Point(Alley, Alley.DoorU, Alley.InsideV)));
    R64Approach(Continued, TEXT("R64_continue_alley"), Alley);
    Continued.Add(R64Choices(TEXT("R64_continue_alley_remembered_Leave"), Alley, true));
    Continued.Add(PWAcceptance::MakeShotStep(TEXT("R64_continue_alley_remembered")));
    R64Retreat(Continued, TEXT("R64_continue_alley"), Alley);
    Continued.Add(R64Walk(TEXT("R64_continue_alley_exit"), R64Point(Alley, Alley.DoorU, -85)));
    Continued.Add(R64DoorMove(TEXT("R64_continue_alley_shut"), Alley.Home->Door, false));
    for (auto& Def : Continued) Out.Add(R64Phase(MoveTemp(Def), State, true));
}
