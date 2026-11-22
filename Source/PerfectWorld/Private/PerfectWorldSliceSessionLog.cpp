#include "PerfectWorldSliceSessionLog.h"
#include "CreationSaveSerialization.h"
#include "Async/Async.h"
#include "Containers/Queue.h"
#include "HAL/Event.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "WorldHash.h"
#include "WorldSave.h"

#include <atomic>
#include <type_traits>

DEFINE_LOG_CATEGORY_STATIC(LogP509Session, Log, All);

namespace
{
using namespace PerfectWorld;

FString P509Text(std::string_view Text) { return UTF8_TO_TCHAR(std::string(Text).c_str()); }
FString P509Quoted(const FString& Text)
{
    FString Out(TEXT("\""));
    for (TCHAR Ch : Text)
    {
        if (Ch == TEXT('"') || Ch == TEXT('\\')) { Out += TEXT('\\'); Out += Ch; }
        else if (Ch < 32) Out += FString::Printf(TEXT("\\u%04x"), static_cast<uint32>(Ch));
        else Out += Ch;
    }
    return Out + TEXT("\"");
}
FString P509Field(const TCHAR* Name, const FString& Value)
{
    return TEXT(",\"") + FString(Name) + TEXT("\":") + P509Quoted(Value);
}
FString P509Id(const TCHAR* Name, std::string_view Value) { return P509Field(Name, P509Text(Value)); }
FString P509Number(const TCHAR* Name, int64 Value)
{
    return FString::Printf(TEXT(",\"%s\":%lld"), Name, static_cast<long long>(Value));
}
// Large counters and seeds remain exact in readers using IEEE-754 numbers.
FString P509Counter(const TCHAR* Name, uint64 Value)
{
    return P509Field(Name, FString::Printf(TEXT("%llu"), static_cast<unsigned long long>(Value)));
}
FString P509Hash(uint64 Value) { return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Value)); }
template <size_t N> const TCHAR* P509Name(const TCHAR* const (&Names)[N], uint64 Index)
{
    return Index < N ? Names[Index] : TEXT("unknown");
}

// Streaming fingerprinting runs only on the writer. No executable paths or machine/user names enter the log.
FString P509Digest(const FString& Path)
{
    TUniquePtr<FArchive> File(IFileManager::Get().CreateFileReader(*Path));
    if (!File) return TEXT("unavailable");
    FSHA1 Hash;
    uint8 Buffer[65536];
    while (File->Tell() < File->TotalSize() && !File->IsError())
    {
        const int64 Size = FMath::Min<int64>(sizeof(Buffer), File->TotalSize() - File->Tell());
        File->Serialize(Buffer, Size);
        Hash.Update(Buffer, static_cast<uint32>(Size));
    }
    if (File->IsError()) return TEXT("unavailable");
    Hash.Final();
    uint8 Bytes[20];
    Hash.GetHash(Bytes);
    return BytesToHex(Bytes, UE_ARRAY_COUNT(Bytes)).ToLower();
}

FString P509ContentDigest()
{
    const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/Content/"));
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.json"), true, false);
    if (Files.IsEmpty()) return TEXT("unavailable");
    Files.Sort();
    FSHA1 Hash;
    for (const auto& File : Files)
    {
        FString Relative = File;
        FPaths::MakePathRelativeTo(Relative, *Root);
        FPaths::NormalizeFilename(Relative);
        const FString Digest = P509Digest(File);
        if (Digest == TEXT("unavailable")) return Digest;
        const FTCHARToUTF8 Bytes(*(Relative + TEXT("\n") + Digest + TEXT("\n")));
        Hash.Update(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length());
    }
    Hash.Final();
    uint8 Bytes[20];
    Hash.GetHash(Bytes);
    return BytesToHex(Bytes, UE_ARRAY_COUNT(Bytes)).ToLower();
}

struct FP509Writer
{
    const FString Id = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString File = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SessionLogs"), TEXT("session_") + Id + TEXT(".jsonl"));
    const double Started = FPlatformTime::Seconds();
    const FString Utc = FDateTime::UtcNow().ToIso8601();
    const FString Module = FModuleManager::Get().GetModuleFilename(FName(TEXT("PerfectWorld")));
    const FString CoreModule = FModuleManager::Get().GetModuleFilename(FName(TEXT("PerfectWorldCore")));
    TQueue<FString, EQueueMode::Mpsc> Queue;
    std::atomic<int32> Pending{0};
    std::atomic<bool> Stopping{false};
    FEvent* Wake = FPlatformProcess::GetSynchEventFromPool(false);
    TFuture<void> Work;
    uint64 Order = 0; // assigned only by the writer, including observed saves
    uint64 Dropped = 0;
    GameInstant LastAt = 0;
    uint64 LastSeq = 0;
    TMap<FString, FString> SlotStamps; // worker only; never stores creation/identity/placement data
    bool Writable = false;

    ~FP509Writer() { FPlatformProcess::ReturnSynchEventToPool(Wake); }

    void Put(const TCHAR* Type, GameInstant At, uint64 Seq, const FString& Fields)
    {
        if (Stopping.load()) return;
        LastAt = At;
        LastSeq = Seq;
        if (Pending.load() >= 2048)
        {
            if (Dropped++ == 0) UE_LOG(LogP509Session, Warning, TEXT("Session log queue full; observations dropped, play continues."));
            return;
        }
        FString Line = Row(Type, At, Seq, Fields);
        if (Dropped != 0) { Line += P509Counter(TEXT("dropped_before"), Dropped); Dropped = 0; }
        Pending.fetch_add(1);
        Queue.Enqueue(MoveTemp(Line));
        Wake->Trigger();
    }

    FString Row(const TCHAR* Type, GameInstant At, uint64 Seq, const FString& Fields) const
    {
        return TEXT("{\"schema\":\"pw.slice_session/1\"") + P509Field(TEXT("session"), Id) + P509Field(TEXT("type"), Type) +
            P509Number(TEXT("game_time"), At) + P509Counter(TEXT("world_seq"), Seq) +
            P509Number(TEXT("elapsed_ms"), static_cast<int64>((FPlatformTime::Seconds() - Started) * 1000.0)) + Fields;
    }

    void Write(const FString& RowText)
    {
        const FString Line = RowText + P509Counter(TEXT("order"), ++Order) + TEXT("}\n");
        if (Writable && !FFileHelper::SaveStringToFile(Line, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
            &IFileManager::Get(), FILEWRITE_Append | FILEWRITE_AllowRead))
        {
            Writable = false;
            UE_LOG(LogP509Session, Warning, TEXT("Local session log write failed; play continues."));
        }
    }

    void ObserveSlots(bool Baseline)
    {
        const FString Directory = FPaths::ProjectSavedDir() / TEXT("SaveGames");
        for (int32 Slot = 0; Slot < PerfectWorld::SaveSlotCount; ++Slot)
        {
            const FString Name = P509Text(PerfectWorld::SaveSlotFileName(Slot));
            const FString Path = Directory / Name;
            if (!IFileManager::Get().FileExists(*Path)) { SlotStamps.Remove(Name); continue; }
            const auto StampOf = [&Path]() { return FString::Printf(TEXT("%lld:%lld"),
                static_cast<long long>(IFileManager::Get().GetTimeStamp(*Path).GetTicks()),
                static_cast<long long>(IFileManager::Get().FileSize(*Path))); };
            const FString Stamp = StampOf();
            if (Baseline) { SlotStamps.Add(Name, Stamp); continue; }
            const auto* Earlier = SlotStamps.Find(Name);
            if (Earlier && *Earlier == Stamp) continue;
            FString Text;
            PerfectWorld::WorldSaveData Saved;
            // Atomic slot replacement may be in flight. Retry next poll; only a complete valid world emits a boundary.
            if (!FFileHelper::LoadFileToString(Text, *Path)) continue;
            const auto Parsed = PerfectWorld::DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Saved);
            if (!Parsed.Succeeded() || StampOf() != Stamp) continue;
            SlotStamps.Add(Name, Stamp);
            if (!Saved.Creation.HasStarted || Parsed.SourceVersion != PerfectWorld::WorldSaveSchemaVersion || !Parsed.HashVerified) continue;
            // Drain commands preceding the save before its observed boundary. The game thread never waits for this read.
            FString Queued;
            while (Queue.Dequeue(Queued)) { Write(Queued); Pending.fetch_sub(1); }
            Write(Row(TEXT("boundary"), Saved.World.Clock.Now(), Saved.World.Seq, P509Field(TEXT("boundary"), TEXT("save")) +
                P509Field(TEXT("source"), TEXT("completed_slot_snapshot")) + P509Field(TEXT("slot_id"), Name) +
                P509Field(TEXT("world_hash"), P509Hash(PerfectWorld::HashWorldState(Saved.World))) +
                P509Counter(TEXT("world_seed"), Saved.World.WorldSeed)));
        }
    }

    void Run()
    {
        ObserveSlots(true); // existing slots are history, never new saves by this session
        const FString Header = TEXT("{\"schema\":\"pw.slice_session/1\",\"type\":\"session\"") + P509Field(TEXT("session"), Id) +
            P509Field(TEXT("started_utc"), Utc) + P509Field(TEXT("build"), FApp::GetBuildVersion()) +
            P509Field(TEXT("engine"), FEngineVersion::Current().ToString()) +
            P509Field(TEXT("executable_sha1"), P509Digest(FPlatformProcess::ExecutablePath())) +
            P509Field(TEXT("game_module_sha1"), P509Digest(Module)) + P509Field(TEXT("content_sha1"), P509ContentDigest()) +
            P509Field(TEXT("core_module_sha1"), P509Digest(CoreModule)) +
            P509Field(TEXT("manifest_sha1"), P509Digest(FPaths::ProjectDir() / TEXT("Data/Content/manifest.json"))) + TEXT("}\n");
        Writable = IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true) &&
            FFileHelper::SaveStringToFile(Header, *File, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                &IFileManager::Get(), FILEWRITE_NoReplaceExisting | FILEWRITE_AllowRead);
        if (!Writable) UE_LOG(LogP509Session, Warning, TEXT("Local session log unavailable; play continues."));
        for (;;)
        {
            FString Line;
            while (Queue.Dequeue(Line))
            {
                Write(Line);
                Pending.fetch_sub(1); // published only after the file handle has closed
            }
            if (Writable) ObserveSlots(false);
            if (Stopping.load() && Pending.load() == 0) return;
            Wake->Wait(100);
        }
    }
};

FP509Writer& P509Writer()
{
    static TSharedRef<FP509Writer, ESPMode::ThreadSafe> Writer = []()
    {
        auto New = MakeShared<FP509Writer, ESPMode::ThreadSafe>();
        New->Work = Async(EAsyncExecution::Thread, [New]() { New->Run(); });
        FCoreDelegates::OnPreExit.AddLambda([New]()
        {
            // Join only during engine shutdown, after play has ended, before modules/filesystem services unload.
            New->Put(TEXT("session_end"), New->LastAt, New->LastSeq, FString());
            New->Stopping.store(true);
            New->Wake->Trigger();
            New->Work.Wait();
        });
        return New;
    }();
    return Writer.Get();
}

const TCHAR* P509Participation(TowerParticipation Kind)
{
    switch (Kind)
    {
    case TowerParticipation::Unset: return TEXT("unset");
    case TowerParticipation::Entered: return TEXT("entered");
    case TowerParticipation::Completed: return TEXT("completed");
    case TowerParticipation::Ejected: return TEXT("ejected");
    case TowerParticipation::Withdrawn: return TEXT("withdrawn");
    case TowerParticipation::Deferred: return TEXT("deferred");
    case TowerParticipation::Declined: return TEXT("declined");
    }
    return TEXT("unknown");
}

FString P509Next(const WorldSim& Sim)
{
    if (Sim.State().Tutorial.Phase == TutorialDungeonPhase::Active)
    {
        const auto Situations = TutorialSituations();
        const auto Next = Sim.State().Tutorial.NextSituation;
        return Next < Situations.size() ? TEXT("tutorial.act:") + P509Text(Situations[Next].StationId) : TEXT("tutorial.withdraw");
    }
    if (Sim.State().Tutorial.Phase == TutorialDungeonPhase::ReadyToGrant) return TEXT("tutorial.finish");
    if (Sim.State().SchoolSessions.Active) return Sim.State().SchoolSessions.Active->ReadyToClose ? TEXT("school.lesson.close") : TEXT("school.lesson.choose");
    for (const auto& Work : Sim.State().SchoolWork.Records)
        if (Work.Paper.Active()) return TEXT("school.work.apply:resume:") + P509Text(Work.Brief.Id);
    for (const auto& Row : Sim.P407UFollowups())
    {
        if (Row.P502CanResume) return TEXT("exam.followup.apply:resume:") + P509Text(Row.Action.Id);
        if (Row.P502CanResolve) return TEXT("exam.followup.apply:resolve:") + P509Text(Row.Action.Id);
        if (Row.Booked && !Row.Attended) return TEXT("exam.followup.apply:attend:") + P509Text(Row.Action.Id);
    }
    if (Sim.ExaminationWorldView().DecisionRequired) return TEXT("tower.session.apply:participation");
    if (Sim.State().Examination.Participation == TowerParticipation::Entered)
        return Sim.ExaminationView().Choices.empty() ? TEXT("tower.session.apply:open_route") : TEXT("tower.session.apply:choose");
    return TEXT("school.plan_day");
}

template <typename T> FString P509CommandFields(const T& Input)
{
    FString Fields;
    if constexpr (requires { Input.ChoiceId; }) Fields += P509Id(TEXT("choice_id"), Input.ChoiceId);
    if constexpr (requires { Input.RouteId; }) Fields += P509Id(TEXT("route_id"), Input.RouteId);
    if constexpr (requires { Input.PersonId; }) Fields += P509Id(TEXT("person_id"), Input.PersonId);
    if constexpr (requires { Input.ActivityId; }) Fields += P509Id(TEXT("activity_id"), Input.ActivityId);
    if constexpr (requires { Input.WorkId; }) Fields += P509Id(TEXT("work_id"), Input.WorkId);
    if constexpr (requires { Input.BindingId; }) Fields += P509Id(TEXT("binding_id"), Input.BindingId);
    if constexpr (requires { Input.OpportunityId; }) Fields += P509Id(TEXT("opportunity_id"), Input.OpportunityId);
    if constexpr (requires { Input.ObligationId; }) Fields += P509Id(TEXT("obligation_id"), Input.ObligationId);
    if constexpr (requires { Input.StationId; }) Fields += P509Id(TEXT("station_id"), Input.StationId);
    if constexpr (std::is_same_v<T, TowerSessionCommand>)
    {
        static const TCHAR* Names[] = {TEXT("enter"), TEXT("open_route"), TEXT("choose"), TEXT("withdraw"), TEXT("defer"), TEXT("decline")};
        Fields += P509Field(TEXT("action"), P509Name(Names, static_cast<uint8>(Input.Kind))) + P509Counter(TEXT("runner"), Input.Runner) + P509Counter(TEXT("beat"), Input.Beat);
    }
    if constexpr (std::is_same_v<T, SchoolWorkCommand>)
    {
        static const TCHAR* Names[] = {TEXT("begin"), TEXT("survey"), TEXT("allocate"), TEXT("answer"), TEXT("submit"), TEXT("cancel")};
        Fields += P509Field(TEXT("action"), P509Name(Names, static_cast<uint8>(Input.Kind))) + P509Number(TEXT("group"), Input.Group) + P509Number(TEXT("units"), Input.Units);
        if (Input.Kind == SchoolWorkActionKind::Answer)
            Fields += P509Number(TEXT("answer_group"), Input.Answer.Group) + P509Number(TEXT("answer_mode"), static_cast<int64>(Input.Answer.Mode)) +
                P509Number(TEXT("answer_confirm"), Input.Answer.Confirm) + P509Number(TEXT("answer_hedge"), Input.Answer.Hedge);
    }
    if constexpr (std::is_same_v<T, P407UFollowupCommand>)
    {
        static const TCHAR* Names[] = {TEXT("book"), TEXT("attend"), TEXT("cancel"), TEXT("start"), TEXT("leave"), TEXT("resume"), TEXT("resolve")};
        Fields += P509Field(TEXT("action"), P509Name(Names, static_cast<uint8>(Input.Kind))) + P509Id(TEXT("appointment_id"), Input.ActionId) +
            P509Id(TEXT("choice_id"), Input.P502ChoiceId) + P509Counter(TEXT("revision"), Input.P502ExpectedRevision);
    }
    if constexpr (std::is_same_v<T, CountdownNegotiateObligation>)
    {
        static const TCHAR* Names[] = {TEXT("honour"), TEXT("share"), TEXT("defer"), TEXT("decline")};
        Fields += P509Field(TEXT("action"), P509Name(Names, static_cast<uint8>(Input.Proposal)));
    }
    if constexpr (std::is_same_v<T, SchoolWeekOrder>)
        Fields += P509Id(TEXT("order_id"), Input.Order.Id) + P509Number(TEXT("weekdays"), Input.Order.Weekdays) +
            P509Number(TEXT("attend_school"), Input.Order.AttendSchool) + P509Number(TEXT("lesson_mode"), static_cast<int64>(Input.Order.LessonMode)) + P509Number(TEXT("remove"), Input.Remove);
    if constexpr (std::is_same_v<T, SchoolLessonChoose>)
    {
        Fields += P509Counter(TEXT("lesson_session"), Input.Session);
        Fields += std::visit([](const auto& Action)
        {
            FString Detail = P509Number(TEXT("lesson_action"), static_cast<int64>(Action.Kind));
            if constexpr (requires { Action.Cause; }) Detail += P509Id(TEXT("cause_id"), Action.Cause);
            if constexpr (requires { Action.Test; }) Detail += P509Id(TEXT("test_id"), Action.Test);
            if constexpr (requires { Action.Member; }) Detail += P509Id(TEXT("member_id"), Action.Member);
            if constexpr (requires { Action.Limitation; }) Detail += P509Id(TEXT("limitation_id"), Action.Limitation);
            if constexpr (requires { Action.Claim; })
                if constexpr (std::is_convertible_v<decltype(Action.Claim), std::string_view>) Detail += P509Id(TEXT("claim_id"), Action.Claim);
                else Detail += P509Number(TEXT("claim"), static_cast<int64>(Action.Claim));
            if constexpr (requires { Action.Investigation; }) Detail += P509Number(TEXT("investigation"), static_cast<int64>(Action.Investigation));
            if constexpr (requires { Action.Assignment; })
            {
                Detail += TEXT(",\"assignments\":[");
                bool First = true;
                for (const auto& Item : Action.Assignment)
                { if (!First) Detail += TEXT(","); First = false; Detail += TEXT("{\"task_id\":") + P509Quoted(P509Text(Item.Task)) + P509Id(TEXT("member_id"), Item.Member) + TEXT("}"); }
                Detail += TEXT("]");
            }
            const auto Ids = [&Detail](const auto& Values, const TCHAR* Name)
            {
                Detail += TEXT(",\"") + FString(Name) + TEXT("\":[");
                bool First = true;
                for (const auto& Id : Values) { if (!First) Detail += TEXT(","); First = false; Detail += P509Quoted(P509Text(Id)); }
                Detail += TEXT("]");
            };
            if constexpr (requires { Action.Links; }) Ids(Action.Links, TEXT("link_ids"));
            if constexpr (requires { Action.Evidence; }) Ids(Action.Evidence, TEXT("evidence_ids"));
            return Detail;
        }, Input.Action);
    }
    if constexpr (std::is_same_v<T, SchoolLessonOpen>) Fields += P509Number(TEXT("lesson_mode"), static_cast<int64>(Input.Mode));
    if constexpr (std::is_same_v<T, SchoolLessonClose>) Fields += P509Field(TEXT("action"), Input.Cancel ? TEXT("leave") : TEXT("close"));
    if constexpr (std::is_same_v<T, WorldDayCommand>) Fields += P509Id(TEXT("action"), DayCommandName(Input.Day.Kind)) + P509Id(TEXT("activity_id"), Input.Day.ActivityId);
    return Fields;
}

template <typename T> FString P509EventFields(const T& Event)
{
    FString Fields;
    if constexpr (requires { Event.GateId; }) Fields += P509Id(TEXT("gate_id"), Event.GateId);
    if constexpr (requires { Event.PersonId; }) Fields += P509Id(TEXT("person_id"), Event.PersonId);
    if constexpr (requires { Event.ChoiceId; }) Fields += P509Id(TEXT("choice_id"), Event.ChoiceId);
    if constexpr (requires { Event.DoorId; }) Fields += P509Id(TEXT("door_id"), Event.DoorId);
    if constexpr (requires { Event.EventId; }) Fields += P509Id(TEXT("scheduled_id"), Event.EventId);
    if constexpr (requires { Event.FlagId; }) Fields += P509Id(TEXT("flag_id"), Event.FlagId) + P509Number(TEXT("value"), Event.Value);
    if constexpr (std::is_same_v<T, ConversationChoiceAccepted>) Fields += P509Number(TEXT("minutes"), Event.CostMinutes) + P509Counter(TEXT("conversation_session"), Event.Session);
    if constexpr (std::is_same_v<T, DoorChanged>) Fields += P509Number(TEXT("open"), Event.Open) + P509Number(TEXT("swing_side"), Event.SwingSide);
    if constexpr (std::is_same_v<T, TimeAdvanced>) Fields += P509Number(TEXT("from"), Event.From) + P509Number(TEXT("to"), Event.To);
    if constexpr (std::is_same_v<T, DayLoopEvent>)
        Fields += P509Id(TEXT("activity_id"), Event.ActivityId) + P509Id(TEXT("outcome"), Event.Outcome) +
            P509Counter(TEXT("instance"), Event.Instance) + P509Number(TEXT("minutes"), Event.Minutes) + P509Counter(TEXT("receipt_seq"), Event.ReceiptSeq);
    if constexpr (std::is_same_v<T, ExamTransition>)
    {
        static const TCHAR* Names[] = {TEXT("sat"), TEXT("withdrew"), TEXT("deferred"), TEXT("declined"), TEXT("finished")};
        Fields += P509Field(TEXT("transition"), P509Name(Names, static_cast<uint8>(Event.Kind))) + P509Number(TEXT("sitting"), Event.Sitting) +
            P509Field(TEXT("participation"), P509Participation(Event.Participation)) + P509Number(TEXT("late_minutes"), Event.LateMinutes) +
            P509Number(TEXT("outside_minutes"), Event.OutsideMinutes) + (Event.TimeCalled ? TEXT(",\"time_called\":true") : TEXT(",\"time_called\":false"));
    }
    return Fields;
}

FString P509EventId(const EventPayload& Payload)
{
    FString Id = std::visit([](const auto& Event) -> FString
    {
        using T = std::decay_t<decltype(Event)>;
        if constexpr (std::is_same_v<T, TimeAdvanced>) return TEXT("time.advanced");
        else if constexpr (std::is_same_v<T, DayStarted>) return TEXT("day.started");
        else if constexpr (std::is_same_v<T, ScheduledEventFired>) return TEXT("world.scheduled_fired");
        else if constexpr (std::is_same_v<T, WorldFlagChanged>) return TEXT("world.flag_changed");
        else if constexpr (std::is_same_v<T, GateRegistered>) return TEXT("gate.registered");
        else if constexpr (std::is_same_v<T, GateOnset>) return TEXT("gate.onset");
        else if constexpr (std::is_same_v<T, GateOnsetHeld>) return TEXT("gate.onset_held");
        else if constexpr (std::is_same_v<T, GateContained>) return TEXT("gate.contained");
        else if constexpr (std::is_same_v<T, GateSealed>) return TEXT("gate.sealed");
        else if constexpr (std::is_same_v<T, ConversationOpened>) return TEXT("conversation.opened");
        else if constexpr (std::is_same_v<T, ConversationChoiceAccepted>) return TEXT("conversation.choice_accepted");
        else if constexpr (std::is_same_v<T, ConversationEnded>) return TEXT("conversation.ended");
        else if constexpr (std::is_same_v<T, DoorChanged>) return TEXT("door.changed");
        else if constexpr (std::is_same_v<T, DayLoopEvent>) return TEXT("day.event");
        else if constexpr (std::is_same_v<T, ExamTransition>) return TEXT("exam.transition");
        else { static_assert(!std::is_same_v<T, T>, "P509 event ID required"); return FString(); }
    }, Payload);
    if (const auto* Day = std::get_if<DayLoopEvent>(&Payload))
    {
        static const TCHAR* Names[] = {TEXT("planned"), TEXT("committed"), TEXT("started"), TEXT("completed"), TEXT("cancelled"),
            TEXT("passed"), TEXT("arrived_school"), TEXT("sleep_began"), TEXT("woke"), TEXT("recovery_due")};
        Id += TEXT(".") + FString(P509Name(Names, static_cast<uint8>(Day->Kind)));
    }
    return Id;
}
}

namespace P509SessionLog
{
void Boundary(const TCHAR* Kind, const PerfectWorld::WorldSim& Sim, const PerfectWorld::WorldState* LoadedSnapshot)
{
    const auto& State = Sim.State();
    P509Writer().Put(TEXT("boundary"), State.Clock.Now(), State.Seq, P509Field(TEXT("boundary"), Kind) +
        P509Field(TEXT("world_hash"), P509Hash(HashWorldState(State))) + P509Counter(TEXT("world_seed"), State.WorldSeed) +
        P509Field(TEXT("next_step"), P509Next(Sim)) + P509Field(TEXT("participation"), P509Participation(State.Examination.Participation)) +
        (LoadedSnapshot ? P509Field(TEXT("saved_hash"), P509Hash(HashWorldState(*LoadedSnapshot))) : FString()));
}

void Command(const PerfectWorld::Command& Input, const PerfectWorld::CommandResult& Result,
    const PerfectWorld::WorldSim& Sim, uint64 FirstReceipt)
{
    auto& Writer = P509Writer();
    Writer.Put(TEXT("command"), Result.To, Result.Seq, P509Id(TEXT("command_id"), CommandTypeId(Input.Payload)) +
        (Result.Accepted ? TEXT(",\"accepted\":true") : TEXT(",\"accepted\":false")) + P509Id(TEXT("refusal_code"), Result.Why.Code) +
        (Result.Partial ? TEXT(",\"partial\":true") : TEXT(",\"partial\":false")) +
        P509Number(TEXT("from"), Result.From) + P509Number(TEXT("to"), Result.To) +
        P509Number(TEXT("charged_minutes"), Result.To - Result.From) + std::visit([](const auto& Cmd) { return P509CommandFields(Cmd); }, Input.Payload));
    if (!Result.Accepted) return;
    uint64 Index = 0;
    for (const auto& Event : Result.Events)
        Writer.Put(TEXT("event"), Event.At, Event.Seq, P509Field(TEXT("event_id"), P509EventId(Event.Payload)) +
            P509Counter(TEXT("event_index"), Index++) + std::visit([](const auto& Payload) { return P509EventFields(Payload); }, Event.Payload));
    // Snapshot only receipts created by this command; adopting a save never replays stored outcomes.
    for (const auto& Receipt : Sim.State().DayLoop.Receipts.Entries)
        if (Receipt.Seq >= FirstReceipt)
        {
            FString Fields = P509Counter(TEXT("receipt_seq"), Receipt.Seq) + P509Id(TEXT("activity_id"), Receipt.ActivityId) +
                P509Number(TEXT("start"), Receipt.Start) + P509Number(TEXT("minutes"), Receipt.DurationMinutes) +
                P509Number(TEXT("omitted_count"), Receipt.OmittedCount) + P509Number(TEXT("omitted_overlap_minutes"), Receipt.OmittedOverlapMinutes);
            Fields += TEXT(",\"displaced\":[");
            bool First = true;
            for (const auto& Lost : Receipt.Displaced)
            {
                if (!First) Fields += TEXT(",");
                First = false;
                Fields += TEXT("{\"id\":") + P509Quoted(P509Text(Lost.Id)) + P509Id(TEXT("source"), Lost.Source) +
                    P509Number(TEXT("known_at"), Lost.KnownAt) + P509Number(TEXT("start"), Lost.Start) +
                    P509Number(TEXT("minutes"), Lost.DurationMinutes) + P509Number(TEXT("overlap_minutes"), Lost.OverlapMinutes) + TEXT("}");
            }
            Writer.Put(TEXT("receipt"), Receipt.DecidedAt, Result.Seq, Fields + TEXT("]"));
        }
    // Once daily detail is full, the rules retain totals rather than further alternative IDs.
    // Preserve that disclosure honestly as a cumulative snapshot, never as another completion.
    if (!Sim.State().DayLoop.Receipts.Overflow.empty())
        for (const auto& Event : Result.Events)
            if (const auto* Day = std::get_if<DayLoopEvent>(&Event.Payload);
                Day && Day->Kind == DayLoopEventKind::Completed && Day->ReceiptSeq == 0)
            {
                const auto Totals = ReceiptGrandTotals(Sim.State().DayLoop.Receipts);
                Writer.Put(TEXT("receipt_totals"), Result.To, Result.Seq,
                    P509Number(TEXT("commitments"), Totals.Commitments) + P509Number(TEXT("committed_minutes"), Totals.CommittedMinutes) +
                    P509Number(TEXT("displaced"), Totals.Displaced) + P509Number(TEXT("overlap_minutes"), Totals.DisplacedOverlapMinutes));
                break;
            }
    if (const auto* Followup = std::get_if<P407UFollowupCommand>(&Input.Payload))
        if (Followup->Kind == P407UFollowupAction::P502Resolve)
            for (const auto& Row : Sim.State().ExaminationFollowups)
                if (Row.Sitting == Sim.State().ExaminationEarlier.size() && Row.AppointmentId == Followup->ActionId &&
                    Row.Completion && Row.Completion->ChoiceId == Followup->P502ChoiceId)
                {
                    FString Fields = P509Id(TEXT("appointment_id"), Row.AppointmentId) + P509Number(TEXT("sitting"), Row.Sitting) +
                        P509Id(TEXT("task_id"), Row.Completion->TaskId) + P509Id(TEXT("choice_id"), Row.Completion->ChoiceId) +
                        P509Counter(TEXT("generation"), Row.Completion->Generation) + TEXT(",\"records\":[");
                    bool First = true;
                    for (const auto& Record : Row.Completion->Records)
                    {
                        if (!First) Fields += TEXT(",");
                        First = false;
                        Fields += P509Quoted(P509Text(Record));
                    }
                    Writer.Put(TEXT("followup_outcome"), Row.Completion->CompletedAt, Result.Seq, Fields + TEXT("]"));
                }
}

FString Path() { return P509Writer().File; }
bool Idle() { return P509Writer().Pending.load() == 0; }
}
