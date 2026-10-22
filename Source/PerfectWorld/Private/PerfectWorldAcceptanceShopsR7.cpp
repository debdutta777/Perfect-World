#include "PerfectWorldAcceptanceShopsR7.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "PerfectWorldAcceptanceProprietorWork.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldInteractionComponent.h"
#include "PerfectWorldInteractable.h"
#include "PerfectWorldLaneLifeConversation.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldNpcLooks.h"
#include "PerfectWorldNpcPresenceSubsystem.h"
#include "PerfectWorldPlayerCharacter.h"
#include "PerfectWorldShopsR7Bindings.h"

#include <string>
#include <string_view>

namespace
{
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FR74ARestart { bool bContinue = false; PerfectWorld::WorldSaveData Saved; };

const PerfectWorld::ShopStaffR7* R74AStaff(const char* Person)
{
    for (const auto& Staff : PerfectWorld::ShopStaffR7Definitions())
    {
        if (Staff.PersonId == Person) { return &Staff; }
    }
    return nullptr;
}
FName R74AName(const char* Person) { return FName(UTF8_TO_TCHAR(Person)); }
FString R74AStem(const char* Person) { return FString(UTF8_TO_TCHAR(Person)).RightChop(7); } // after "person."
FName R74AChoice(const char* Person, const TCHAR* Kind) { return FName(*(FString(TEXT("choice.")) + R74AStem(Person) + TEXT(".") + Kind)); }
std::string R74AFact(const char* Person)
{
    // The one remembered agreement of each person (ConversationShopsR7.cpp).
    const std::string_view Who(Person);
    if (Who == "person.bimal_modak") { return "fact.bimal_modak.morning_stories_agreed"; }
    if (Who == "person.tithi_ghoshal") { return "fact.tithi_ghoshal.tray_stories_agreed"; }
    if (Who == "person.aparna_lahiri") { return "fact.aparna_lahiri.evening_talk_agreed"; }
    if (Who == "person.gouranga_nandi") { return "fact.gouranga_nandi.dawn_talk_agreed"; }
    return "fact.koushik_mondal.mixing_talk_agreed";
}

FStepDef R74AAction(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run, double Timeout = 30.0)
{
    FStepDef Step;
    Step.Name = Name;
    Step.Run = MoveTemp(Run);
    Step.TimeoutSeconds = Timeout;
    return Step;
}
FStepDef R74APhase(FStepDef Step, TSharedRef<FR74ARestart> State, const bool bContinue)
{
    auto Run = MoveTemp(Step.Run);
    Step.Run = [Run = MoveTemp(Run), State, bContinue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
        { return State->bContinue == bContinue ? Run(Sub, Rt) : FStepOutcome::Pass(TEXT("other restart-pair phase")); };
    return Step;
}
bool R74ASlot(UPWAcceptanceSubsystem& Sub, PerfectWorld::WorldSaveData& Data)
{
    const int32 Slot = Sub.GetArgs().Slot >= 0 ? Sub.GetArgs().Slot : UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    return Slot >= 0 && FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot)) &&
        PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Data).HashVerified;
}
FStepDef R74ACamera(const TCHAR* Name, const EPWCameraMode Mode)
{
    return R74AAction(Name, [Mode](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Pawn = Sub.GetPawn();
        if (Pawn == nullptr) { return FStepOutcome::Fail(TEXT("no pawn for camera switch")); }
        const FVector Before = Pawn->GetActorLocation();
        Pawn->SetCameraMode(Mode);
        return Pawn->GetCameraMode() == Mode && Pawn->GetActorLocation().Equals(Before, 0.01) ?
            FStepOutcome::Pass(TEXT("camera switched without moving the player")) : FStepOutcome::Fail(TEXT("camera switch moved the player or failed"));
    });
}

/** Walk target: the footpath in front of the counter, or the customer standing point (a flap or counter front within reach). */
FStepDef R74AWalk(const TCHAR* Name, const char* Person, const bool bFootpath)
{
    return PWAcceptance::MakeWalkStep(Name, [Person, bFootpath](UPWAcceptanceSubsystem&, FVector& Point, FString& Why)
    {
        const auto* Staff = R74AStaff(Person);
        if (Staff == nullptr) { Why = TEXT("unknown shop staff"); return false; }
        Point = bFootpath ? FVector(PWLane::R74StaffFeet(*Staff).X, 3700.0, PWLane::R74StaffFeet(*Staff).Z) : PWLane::R74CustomerPoint(*Staff);
        return true;
    }, 15.0, 60.0);
}

APWNpcCharacter* R74AActor(UPWAcceptanceSubsystem& Sub, const char* Person)
{
    const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
    return Presence != nullptr ? Presence->FindPerson(R74AName(Person)) : nullptr;
}

/** The keeper stands at the counter and visibly performs the named work (presenter state Work and the intent's work action). */
FStepDef R74AAtWork(const TCHAR* Name, const char* Person, const TCHAR* Action)
{
    return R74AAction(Name, [Person, Action](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Staff = R74AStaff(Person);
        const auto* Actor = R74AActor(Sub, Person);
        if (Staff == nullptr) { return FStepOutcome::Fail(TEXT("unknown shop staff")); }
        if (Actor == nullptr || FVector::Dist2D(Actor->GetActorLocation(), PWLane::R74StaffFeet(*Staff)) > 75.0 ||
            Actor->GetNpcMovement()->GetPresentedState() != EPWNpcState::Work ||
            Actor->GetNpcMovement()->GetIntent().WorkActionId != FName(Action)) { return FStepOutcome::Wait(); }
        return FStepOutcome::Pass(FString::Printf(TEXT("%hs physically performs %s at the counter"), Person, Action));
    }, 90.0);
}

/** The scheduled break: no work phase, rest or meal priority, and the keeper still stays at the counter. */
FStepDef R74ABreak(const TCHAR* Name, const char* Person, const bool bNear)
{
    return R74AAction(Name, [Person, bNear](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Staff = R74AStaff(Person);
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        const auto* Decision = Presence != nullptr ? Presence->FindPilotDecision(R74AName(Person)) : nullptr;
        if (Staff == nullptr || Decision == nullptr) { return FStepOutcome::Fail(TEXT("no pilot decision for the break")); }
        if (Decision->Work.Phase != PerfectWorld::NpcWorkPhase::None || Decision->Needs.Priority < PerfectWorld::NpcNeedPriority::Rest)
            { return FStepOutcome::Fail(TEXT("a posted break is still presented as counter work")); }
        const auto* Actor = R74AActor(Sub, Person);
        if (bNear && (Actor == nullptr || FVector::Dist2D(Actor->GetActorLocation(), PWLane::R74StaffFeet(*Staff)) > 75.0 ||
                      Actor->GetNpcMovement()->GetPresentedState() == EPWNpcState::Work))
            { return FStepOutcome::Fail(TEXT("the keeper is not resting at the counter")); }
        return FStepOutcome::Pass(TEXT("posted break: no work phase, rest or meal priority"));
    });
}

/** After closing: the keeper is not working at the counter any more. */
FStepDef R74AClosed(const TCHAR* Name, const char* Person)
{
    return R74AAction(Name, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Staff = R74AStaff(Person);
        const auto* Presence = Sub.GetPlayWorld()->GetSubsystem<UPWNpcPresenceSubsystem>();
        const auto* Decision = Presence != nullptr ? Presence->FindPilotDecision(R74AName(Person)) : nullptr;
        const auto* Actor = R74AActor(Sub, Person);
        if (Staff == nullptr || Decision == nullptr) { return FStepOutcome::Fail(TEXT("no pilot decision after closing")); }
        const bool bAtCounter = Actor != nullptr && FVector::Dist2D(Actor->GetActorLocation(), PWLane::R74StaffFeet(*Staff)) <= 75.0;
        if (Decision->Work.Phase != PerfectWorld::NpcWorkPhase::None || (bAtCounter && Actor->GetNpcMovement()->GetPresentedState() == EPWNpcState::Work))
            { return FStepOutcome::Fail(TEXT("the keeper still works the closed counter")); }
        return FStepOutcome::Pass(TEXT("closed: no counter work remains; the opening hours explain it at the notice"));
    });
}

FStepDef R74AHeight(const TCHAR* Name, const char* Person)
{
    return R74AAction(Name, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Staff = R74AStaff(Person);
        const auto* Actor = R74AActor(Sub, Person);
        if (Staff == nullptr || Actor == nullptr) { return FStepOutcome::Fail(TEXT("no physical keeper to measure")); }
        const double Frozen = static_cast<double>(Staff->HeightCm);
        const double Capsule = Actor->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0;
        TArray<FPWNpcBlock> Blocks;
        PWNpcLooks::BuildBlocks(R74AName(Person), Actor->GetOutfit(), Actor->GetStandingHeightCm(), Blocks);
        double Bottom = 1e9, Top = -1e9;
        for (const auto& Block : Blocks)
        {
            Bottom = FMath::Min(Bottom, Block.Centre.Z - Block.Size.Z * 0.5);
            Top = FMath::Max(Top, Block.Centre.Z + Block.Size.Z * 0.5);
        }
        if (!FMath::IsNearlyEqual(Capsule, Frozen, 0.05) || !FMath::IsNearlyEqual(Top - Bottom, Frozen, 0.05))
            { return FStepOutcome::Fail(TEXT("capsule or standing block body differs from the frozen height")); }
        return FStepOutcome::Pass(FString::Printf(TEXT("capsule %.2f cm; blocks %.2f cm; frozen %.0f cm"), Capsule, Top - Bottom, Frozen));
    });
}

FStepDef R74ASelect(const TCHAR* Name, const char* Person, const bool bBrowse)
{
    return R74AAction(Name, [Person, bBrowse](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        const auto* Staff = R74AStaff(Person);
        auto* Interaction = Sub.GetInteraction();
        AActor* Target = nullptr;
        if (bBrowse && Staff != nullptr)
        {
            const FName Tag = PWLane::R74BrowseTag(*Staff);
            for (TActorIterator<AActor> It(Sub.GetPlayWorld()); It; ++It) { if (It->ActorHasTag(Tag)) { Target = *It; break; } }
        }
        else { Target = R74AActor(Sub, Person); }
        const auto* Interactable = Cast<IPWInteractable>(Target);
        if (Interaction == nullptr || Interactable == nullptr) { return FStepOutcome::Fail(TEXT("no physical interaction target in reach")); }
        Sub.FaceWorldPoint(Interactable->GetInteractionPoint());
        if (Interaction->GetTarget() == Target && Interaction->IsPromptVisible()) { return FStepOutcome::Pass(TEXT("real prompt selected")); }
        // People outrank browse displays; the player's own cycle action reaches the display.
        if (Rt.Elapsed - Rt.LastProbe >= 0.2) { Interaction->CycleTarget(); Rt.LastProbe = Rt.Elapsed; }
        return FStepOutcome::Wait();
    });
}

FStepDef R74ANotice(const TCHAR* Name, const char* Person)
{
    return PWAcceptance::MakeInteractStep(Name,
        [Person](UPWAcceptanceSubsystem& Sub) -> AActor*
        {
            const auto* Staff = R74AStaff(Person);
            if (Staff == nullptr) { return nullptr; }
            const FName Tag = PWLane::R74BrowseTag(*Staff);
            for (TActorIterator<AActor> It(Sub.GetPlayWorld()); It; ++It) { if (It->ActorHasTag(Tag)) { return *It; } }
            return nullptr;
        },
        [Person](UPWAcceptanceSubsystem& Sub, FString& Detail)
        {
            const auto* Staff = R74AStaff(Person);
            const auto* Interaction = Sub.GetInteraction();
            if (Staff == nullptr || Interaction == nullptr) { return false; }
            Detail = Interaction->GetNotice().ToString();
            return Interaction->IsNoticeVisible() && Detail.Contains(TEXT("Purchasing is unavailable")) &&
                Detail.Contains(PWLane::R74PostedHoursText(*Staff));
        }, nullptr, 20.0);
}

/** The open conversation panel's current line. Empty when no real panel of this person is open. */
bool R74ALine(UPWAcceptanceSubsystem& Sub, const char* Person, FString& OutLine, const PerfectWorld::ConversationState*& OutMemory)
{
    auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
    auto* Talk = UPWConversationPresenter::Get(Sub.GetPlayWorld());
    const auto* Staff = R74AStaff(Person);
    OutMemory = Life != nullptr ? Life->FindConversation(R74AName(Person)) : nullptr;
    if (Life == nullptr || Talk == nullptr || Staff == nullptr || !Talk->IsOpen() || OutMemory == nullptr || !OutMemory->Open ||
        PerfectWorld::OpenConversationPersonId(Life->GetWorldState()) != Person) { return false; }
    FPWCoreConversationSession Session(TWeakObjectPtr<UPWLaneLifeSubsystem>(Life), R74AName(Person),
        FText::FromString(UTF8_TO_TCHAR(std::string(Staff->DisplayName).c_str())), OutMemory->SessionCount);
    const auto View = Session.GetView();
    OutLine = View.Line.ToString();
    return View.bCanLeave;
}

/** The greeting names the remembered agreement (after a restart) or is the plain first greeting. */
FStepDef R74AGreeting(const TCHAR* Name, const char* Person, const bool bRemembered)
{
    return R74AAction(Name, [Person, bRemembered](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        FString Line;
        const PerfectWorld::ConversationState* Memory = nullptr;
        if (!R74ALine(Sub, Person, Line, Memory)) { return FStepOutcome::Fail(TEXT("no real conversation panel with Leave")); }
        const bool bFact = PerfectWorld::HasConversationFact(*Memory, R74AFact(Person));
        if (bRemembered != bFact || (bRemembered && !Line.Contains(TEXT("You said you would"))) || !Memory->Receipts.empty())
            { return FStepOutcome::Fail(TEXT("greeting does not match the remembered agreement or a receipt exists")); }
        return FStepOutcome::Pass(Line);
    });
}

/** Choose "Ask what is available here": the answer must explain that purchasing is unavailable. */
FStepDef R74AServiceLine(const TCHAR* Name, const char* Person)
{
    return R74AAction(Name, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        FString Line;
        const PerfectWorld::ConversationState* Memory = nullptr;
        if (!R74ALine(Sub, Person, Line, Memory)) { return FStepOutcome::Fail(TEXT("no real conversation panel with Leave")); }
        UPWConversationPresenter::Get(Sub.GetPlayWorld())->ChooseOption(R74AChoice(Person, TEXT("service")));
        if (!R74ALine(Sub, Person, Line, Memory) || !Line.Contains(TEXT("unavailable")) || !Memory->Receipts.empty())
            { return FStepOutcome::Fail(TEXT("the service answer does not explain that purchasing is unavailable")); }
        return FStepOutcome::Pass(Line);
    });
}

/** Choose the agreement: one fact, no receipt, no game time. */
FStepDef R74AAgree(const TCHAR* Name, const char* Person)
{
    return R74AAction(Name, [Person](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        FString Line;
        const PerfectWorld::ConversationState* Memory = nullptr;
        if (Life == nullptr || !R74ALine(Sub, Person, Line, Memory)) { return FStepOutcome::Fail(TEXT("no real conversation panel")); }
        const auto Before = Life->GetNow();
        UPWConversationPresenter::Get(Sub.GetPlayWorld())->ChooseOption(R74AChoice(Person, TEXT("agree")));
        Memory = Life->FindConversation(R74AName(Person));
        if (Memory == nullptr || !PerfectWorld::HasConversationFact(*Memory, R74AFact(Person)) ||
            Memory->Facts.size() != 1 || !Memory->Receipts.empty() || Life->GetNow() != Before)
            { return FStepOutcome::Fail(TEXT("the agreement changed more than one free fact")); }
        return FStepOutcome::Pass(TEXT("one free intention; no money, item, order or sale"));
    });
}

/**
 * One complete visit: stand at the counter, read the notice, check the height, Talk at a safe pause (service explanation), Leave and
 * see the work resume; optionally a second Talk for the agreement.
 */
void R74AVisit(TArray<FStepDef>& Out, TSharedRef<FR74ARestart> State, const char* Person, const TCHAR* Action, const bool bAgree)
{
    const FString Stem = R74AStem(Person);
    const auto Name = [&Stem](const TCHAR* Part) { return Stem + TEXT("_") + Part; };
    const auto Add = [&Out, State](FStepDef Step) { Out.Add(R74APhase(MoveTemp(Step), State, false)); };
    const FName Who = R74AName(Person);
    Add(R74AAtWork(*Name(TEXT("at_work")), Person, Action));
    Add(R74AWalk(*Name(TEXT("approach_footpath")), Person, true));
    Add(R74AWalk(*Name(TEXT("walk_to_customer_point")), Person, false));
    Add(R74ASelect(*Name(TEXT("select_browse_prompt")), Person, true));
    Add(R74ANotice(*Name(TEXT("read_unavailable_purchasing_notice")), Person));
    Add(R74AHeight(*Name(TEXT("measured_height")), Person));
    Add(R74ASelect(*Name(TEXT("select_talk_prompt")), Person, false));
    Add(R55SafeTalkStep(*Name(TEXT("real_talk_defers_to_safe_pause")), Who));
    Add(R74AGreeting(*Name(TEXT("first_greeting")), Person, false));
    Add(R74AServiceLine(*Name(TEXT("service_explains_unavailable")), Person));
    Add(MakeShotStep(Name(TEXT("shot_purchasing_unavailable"))));
    Add(R55LeaveResumeStep(*Name(TEXT("leave_resumes_work")), Who));
    Add(R74AAtWork(*Name(TEXT("visible_work_resumed")), Person, Action));
    if (bAgree)
    {
        Add(R74ASelect(*Name(TEXT("reselect_talk_prompt")), Person, false));
        Add(R55SafeTalkStep(*Name(TEXT("second_talk_safe_pause")), Who));
        Add(R74AAgree(*Name(TEXT("free_agreement")), Person));
        Add(R55LeaveResumeStep(*Name(TEXT("leave_after_agreement")), Who));
        Add(R74AAtWork(*Name(TEXT("work_resumed_after_agreement")), Person, Action));
    }
    // Back out to the footpath so the next leg does not cut across a wall.
    Add(R74AWalk(*Name(TEXT("return_to_footpath")), Person, true));
}
}

namespace PWAcceptance
{
void AppendShopsRound7Steps(TArray<FStepDef>& Out)
{
    auto State = MakeShared<FR74ARestart>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(R74AAction(TEXT("shops_restart_pair_contract"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            { return FStepOutcome::Fail(TEXT("shops_round7 requires explicit new then fresh-process continue")); }
        State->bContinue = Sub.GetArgs().Start == TEXT("continue");
        if (State->bContinue && !R74ASlot(Sub, State->Saved)) { return FStepOutcome::Fail(TEXT("cannot authenticate the original slot")); }
        return FStepOutcome::Pass(TEXT("explicit restart-pair phase"));
    }));
    Out.Add(MakeEnterWorldStep());
    Out.Add(R74APhase(R74AAction(TEXT("shops_continued_memory"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        const auto Found = State->Saved.World.Conversations.find("person.bimal_modak");
        const auto* Memory = Life != nullptr ? Life->FindConversation(R74AName("person.bimal_modak")) : nullptr;
        if (Life == nullptr || Memory == nullptr || Found == State->Saved.World.Conversations.end() || *Memory != Found->second ||
            !PerfectWorld::HasConversationFact(*Memory, R74AFact("person.bimal_modak")) || Memory->Facts.size() != 1 ||
            !Memory->Receipts.empty() || Life->GetWorldState().Journal != State->Saved.World.Journal)
            { return FStepOutcome::Fail(TEXT("the agreement or its dated history changed across the fresh-process Continue")); }
        return FStepOutcome::Pass(TEXT("exact saved agreement and journal survived; no receipt"));
    }), State, true));
    Out.Add(R74ACamera(TEXT("shops_first_person"), EPWCameraMode::FirstPerson));

    // Fresh-process phase: the sweet-shop owner remembers the agreement; the service answer is still honest.
    Out.Add(R74APhase(R74AAtWork(TEXT("bimal_modak_continued_at_work"), "person.bimal_modak", TEXT("work.prepare_sweets")), State, true));
    Out.Add(R74APhase(R74ASelect(TEXT("bimal_modak_continued_select_talk"), "person.bimal_modak", false), State, true));
    Out.Add(R74APhase(R55SafeTalkStep(TEXT("bimal_modak_continued_safe_pause"), R74AName("person.bimal_modak")), State, true));
    Out.Add(R74APhase(R74AGreeting(TEXT("bimal_modak_remembers_after_restart"), "person.bimal_modak", true), State, true));
    Out.Add(R74APhase(R74AServiceLine(TEXT("bimal_modak_continued_service"), "person.bimal_modak"), State, true));
    Out.Add(R74APhase(MakeShotStep(TEXT("shot_sweet_shop_remembered_agreement")), State, true));
    Out.Add(R74APhase(R55LeaveResumeStep(TEXT("bimal_modak_continued_leave"), R74AName("person.bimal_modak")), State, true));

    // New phase: one declared start on the para-road footpath, then the real pawn does everything.
    Out.Add(R74APhase(R74AAction(TEXT("declared_start_on_para_footpath"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        auto* Pawn = Sub.GetPawn();
        const auto* Staff = R74AStaff("person.bimal_modak");
        if (Pawn == nullptr || Staff == nullptr) { return FStepOutcome::Fail(TEXT("no pawn or sweet-shop plan")); }
        const FVector Feet = PWLane::R74StaffFeet(*Staff);
        if (Rt.Phase == 0)
        {
            if (!Pawn->TeleportTo(FVector(Feet.X, 3700.0, Feet.Z + PWPlayer::CapsuleHalfHeight + 5.0), FRotator(0.0, -90.0, 0.0)))
                { return FStepOutcome::Fail(TEXT("declared-start placement refused")); }
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Rt.NextPhase();
        }
        return Rt.InPhase() >= 0.8 && Pawn->GetCharacterMovement()->IsMovingOnGround() ?
            FStepOutcome::Pass(TEXT("one declared start; every approach after it uses the real pawn")) : FStepOutcome::Wait();
    }), State, false));
    Out.Add(R74APhase(R55PilotTimeStep(TEXT("clock_to_1015"), 10, 15), State, false));
    R74AVisit(Out, State, "person.bimal_modak", TEXT("work.prepare_sweets"), true);
    Out.Add(R74APhase(R74AAction(TEXT("save_sweet_shop_agreement_for_fresh_process"), [State](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Hud = Sub.GetHud();
        if (Hud == nullptr) { return FStepOutcome::Fail(TEXT("no save owner")); }
        bool bSaved = false;
        const FText Detail = Hud->SaveActiveGame(bSaved);
        if (!bSaved || !R74ASlot(Sub, State->Saved)) { return FStepOutcome::Fail(Detail.ToString()); }
        const auto Found = State->Saved.World.Conversations.find("person.bimal_modak");
        if (Found == State->Saved.World.Conversations.end() || Found->second.Open ||
            !PerfectWorld::HasConversationFact(Found->second, R74AFact("person.bimal_modak")) || !Found->second.Receipts.empty())
            { return FStepOutcome::Fail(TEXT("the authenticated slot lacks the closed agreement")); }
        return FStepOutcome::Pass(TEXT("authenticated slot holds the agreement for the fresh-process Continue"));
    }), State, false));

    // The tea stall in third person (11:30, serving), then the scheduled breaks, then the jhalmuri pitch, the assistant and the late pharmacy.
    Out.Add(R74APhase(R74ACamera(TEXT("shops_third_person"), EPWCameraMode::ThirdPerson), State, false));
    Out.Add(R74APhase(R55PilotTimeStep(TEXT("clock_to_1130"), 11, 30), State, false));
    R74AVisit(Out, State, "person.gouranga_nandi", TEXT("work.serve"), false);
    Out.Add(R74APhase(R55PilotTimeStep(TEXT("clock_to_1405_breaks"), 14, 5), State, false));
    Out.Add(R74APhase(R74ABreak(TEXT("tea_stall_break_at_counter"), "person.gouranga_nandi", true), State, false));
    Out.Add(R74APhase(R74ABreak(TEXT("sweet_shop_afternoon_lull"), "person.bimal_modak", false), State, false));
    Out.Add(R74APhase(R74ABreak(TEXT("sweet_shop_assistant_lull"), "person.tithi_ghoshal", false), State, false));
    Out.Add(R74APhase(R74ABreak(TEXT("pharmacy_meal_break"), "person.aparna_lahiri", false), State, false));
    Out.Add(R74APhase(R74ACamera(TEXT("shops_first_person_again"), EPWCameraMode::FirstPerson), State, false));
    Out.Add(R74APhase(R55PilotTimeStep(TEXT("clock_to_1530"), 15, 30), State, false));
    R74AVisit(Out, State, "person.koushik_mondal", TEXT("work.serve"), false);
    Out.Add(R74APhase(R55PilotTimeStep(TEXT("clock_to_1605"), 16, 5), State, false));
    Out.Add(R74APhase(R74AAtWork(TEXT("assistant_prepares_trays"), "person.tithi_ghoshal", TEXT("work.prepare_sweets")), State, false));
    Out.Add(R74APhase(R74AHeight(TEXT("assistant_measured_height"), "person.tithi_ghoshal"), State, false));
    Out.Add(R74APhase(R74ACamera(TEXT("shops_third_person_late"), EPWCameraMode::ThirdPerson), State, false));
    Out.Add(R74APhase(R55PilotTimeStep(TEXT("clock_to_2230"), 22, 30), State, false));
    R74AVisit(Out, State, "person.aparna_lahiri", TEXT("work.serve"), false);
    Out.Add(R74APhase(R55PilotTimeStep(TEXT("clock_to_2305_pharmacy_closed"), 23, 5), State, false));
    Out.Add(R74APhase(R74AClosed(TEXT("pharmacy_closed_after_2300"), "person.aparna_lahiri"), State, false));
}
}
