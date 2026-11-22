#include "PerfectWorldAcceptanceHomework.h"

#include "HAL/FileManager.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldFrontEndHUD.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldSliceSessionLog.h"
#include "WorldHash.h"
#include "WorldSave.h"

// P509: run twice in fresh processes, Start=new then Start=continue, sharing only Saved.
// The existing live homework path supplies the real desk/paper commands. No images are captured here.
namespace
{
using namespace PerfectWorld;
using PWAcceptance::FStepDef;
using PWAcceptance::FStepOutcome;
using PWAcceptance::FStepRuntime;

struct FP509Run
{
    bool Continue = false;
    WorldSaveData Saved;
    std::string WorkId;
    uint64 Hash = 0;
};

FStepDef P509Action(const TCHAR* Name, TFunction<FStepOutcome(UPWAcceptanceSubsystem&, FStepRuntime&)> Run)
{
    FStepDef Step;
    Step.Name = Name;
    Step.TimeoutSeconds = 90;
    Step.Run = MoveTemp(Run);
    return Step;
}

FStepDef P509Phase(FStepDef Step, const TSharedRef<FP509Run>& Run, bool Continue)
{
    auto Inner = MoveTemp(Step.Run);
    Step.Name = TEXT("P509_") + Step.Name;
    Step.Run = [Inner = MoveTemp(Inner), Run, Continue](UPWAcceptanceSubsystem& Sub, FStepRuntime& Rt)
    {
        return Run->Continue == Continue ? Inner(Sub, Rt) : FStepOutcome::Pass(TEXT("belongs to the other process phase"));
    };
    return Step;
}

bool P509Slot(UPWAcceptanceSubsystem& Sub, FP509Run& Run)
{
    const int32 Slot = UPerfectWorldCharacterCreatorSubsystem::FindMostRecentSaveSlot();
    FString Text;
    if (Slot < 0 || (Sub.GetArgs().Slot >= 0 && Slot != Sub.GetArgs().Slot) ||
        !FFileHelper::LoadFileToString(Text, *UPerfectWorldCharacterCreatorSubsystem::GetSaveSlotPath(Slot))) return false;
    const auto Parsed = DeserializeWorldSave(TCHAR_TO_UTF8(*Text), Run.Saved);
    if (!Parsed.Succeeded() || Parsed.SourceVersion != WorldSaveSchemaVersion) return false;
    Run.Hash = HashWorldState(Run.Saved.World);
    for (const auto& Work : Run.Saved.World.SchoolWork.Records)
        if (Work.SubmittedAt) { Run.WorkId = Work.Brief.Id; break; }
    return !Run.WorkId.empty();
}

std::string P509String(const JsonValue& Row, std::string_view Key)
{
    const auto* Value = Row.Find(Key);
    return Value && Value->IsString() ? Value->String : std::string();
}
int64 P509Int(const JsonValue& Row, std::string_view Key)
{
    const auto* Value = Row.Find(Key);
    return Value && Value->Integer ? *Value->Integer : -1;
}
bool P509Accepted(const JsonValue& Row)
{
    const auto* Value = Row.Find("accepted");
    return Value && Value->Type == JsonValue::Kind::Bool && Value->Bool;
}
bool P509Rows(const FString& Path, std::vector<JsonValue>& Rows)
{
    TArray<FString> Lines;
    if (!FFileHelper::LoadFileToStringArray(Lines, *Path) || Lines.IsEmpty()) return false;
    uint64 Last = 0;
    for (const auto& Line : Lines)
    {
        const auto Parsed = ParseJson(TCHAR_TO_UTF8(*Line));
        if (!Parsed.Value && &Line == &Lines.Last()) break; // exit/crash can interrupt the optional final session_end row
        if (!Parsed.Value || P509String(*Parsed.Value, "schema") != "pw.slice_session/1" || Parsed.Value->Find("dropped_before")) return false;
        if (const auto* Order = Parsed.Value->Find("order"))
        {
            if (!Order->IsString()) return false;
            const uint64 Number = FCString::Strtoui64(UTF8_TO_TCHAR(Order->String.c_str()), nullptr, 10);
            if (Number != Last + 1) return false;
            Last = Number;
            if (P509Int(*Parsed.Value, "elapsed_ms") < 0) return false;
        }
        Rows.push_back(*Parsed.Value);
    }
    if (Rows.empty() || P509String(Rows.front(), "type") != "session") return false;
    for (const auto* Key : {"build", "executable_sha1", "manifest_sha1", "content_sha1"})
        if (P509String(Rows.front(), Key).empty() || P509String(Rows.front(), Key) == "unavailable") return false;
    return true;
}

bool P509Original(const std::vector<JsonValue>& Rows, const FP509Run& Run)
{
    const std::string Hash = TCHAR_TO_UTF8(*FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Run.Hash)));
    int Stage = 0, Outcomes = 0;
    for (const auto& Row : Rows)
    {
        const auto Type = P509String(Row, "type");
        const auto Id = P509String(Row, "work_id");
        const auto Action = P509String(Row, "action");
        if (Type == "command" && Id == Run.WorkId)
        {
            if (Stage == 0 && Action == "begin" && P509Accepted(Row)) Stage = 1;
            else if (Stage == 1 && Action == "answer" && P509Accepted(Row)) Stage = 2;
            else if (Stage == 2 && Action == "submit" && P509Accepted(Row) && P509Int(Row, "charged_minutes") == 90) Stage = 3;
            else if (Stage == 3 && Action == "submit" && !P509Accepted(Row) && P509Int(Row, "charged_minutes") == 0) Stage = 4;
        }
        if (Type == "event" && P509String(Row, "event_id") == "day.event.completed" && P509String(Row, "activity_id") == Run.WorkId)
        { if (Stage != 3) return false; ++Outcomes; }
        if (Type == "boundary" && P509String(Row, "boundary") == "save" && P509String(Row, "world_hash") == Hash && Stage == 4) Stage = 5;
    }
    return Stage == 5 && Outcomes == 1;
}
}

namespace PWAcceptance
{
void P509AppendSessionLogSteps(TArray<FStepDef>& Out)
{
    const auto Run = MakeShared<FP509Run>();
    Out.Add(MakeWorldReadyStep());
    Out.Add(P509Action(TEXT("P509_process_phase"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        if (Sub.GetArgs().Start != TEXT("new") && Sub.GetArgs().Start != TEXT("continue"))
            return FStepOutcome::Fail(TEXT("session_log_round10 requires two fresh processes: explicit new, then continue"));
        Run->Continue = Sub.GetArgs().Start == TEXT("continue");
        if (Run->Continue && !P509Slot(Sub, *Run)) return FStepOutcome::Fail(TEXT("Continue requires the original completed homework slot"));
        return FStepOutcome::Pass(TEXT("process phase recorded before world entry"));
    }));
    Out.Add(P509Phase(MakeEnterWorldStep(), Run, false));
    Out.Add(P509Phase(P509Action(TEXT("title_continue"), [](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Hud = Sub.GetHud();
        if (!Hud) return FStepOutcome::Fail(TEXT("title HUD absent"));
        const FText Why = Hud->ContinueLatest();
        if (!Why.IsEmpty()) return FStepOutcome::Fail(Why.ToString());
        // Pause in the same callback as Continue, before another free-roam frame can advance the saved clock.
        Hud->ToggleInWorldMenu();
        return Hud->IsMenuOpen() ? FStepOutcome::Pass(TEXT("title Continue restored the life, then paused for hash comparison")) :
            FStepOutcome::Fail(TEXT("the resumed life did not enter its pause menu"));
    }), Run, true));
    TArray<FStepDef> Homework;
    P36UAppendHomeworkSteps(Homework);
    for (int32 Index = 2; Index < Homework.Num(); ++Index)
        if (!Homework[Index].Name.Contains(TEXT("shot"))) Out.Add(P509Phase(MoveTemp(Homework[Index]), Run, false));
    Out.Add(P509Phase(P509Action(TEXT("save_after_homework"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Hud = Sub.GetHud();
        const auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Hud || !Life) return FStepOutcome::Fail(TEXT("live HUD/world absent"));
        if (!Hud->IsMenuOpen()) Hud->ToggleInWorldMenu();
        bool Saved = false;
        const auto Why = Hud->SaveActiveGame(Saved);
        if (!Saved || !P509Slot(Sub, *Run)) return FStepOutcome::Fail(Why.ToString());
        const auto Before = HashWorldState(Life->GetWorldState());
        P509SessionLog::Boundary(TEXT("observation"), Life->GetSim());
        return Before == Run->Hash && HashWorldState(Life->GetWorldState()) == Before ?
            FStepOutcome::Pass(TEXT("saved homework through the HUD; logging left the world hash unchanged")) :
            FStepOutcome::Fail(TEXT("logging or saving changed the authoritative world"));
    }), Run, false));
    Out.Add(P509Phase(P509Action(TEXT("original_log_order"), [Run](UPWAcceptanceSubsystem&, FStepRuntime& Rt)
    {
        if (!P509SessionLog::Idle()) return FStepOutcome::Wait();
        std::vector<JsonValue> Rows;
        if (!P509Rows(P509SessionLog::Path(), Rows)) return FStepOutcome::Fail(TEXT("local log or reproduction metadata unreadable"));
        Rt.Note = TEXT("waiting for the background writer to observe the completed slot snapshot");
        return P509Original(Rows, *Run) ? FStepOutcome::Pass(TEXT("begin, answer, 90-minute submit, one completion, refused repeat, observed save in order")) :
            FStepOutcome::Wait();
    }), Run, false));
    Out.Add(P509Phase(P509Action(TEXT("continued_hash_and_stale_submit"), [Run](UPWAcceptanceSubsystem& Sub, FStepRuntime&)
    {
        auto* Hud = Sub.GetHud();
        auto* Life = UPWLaneLifeSubsystem::Get(Sub.GetPlayWorld());
        if (!Hud || !Life) return FStepOutcome::Fail(TEXT("continued live world absent"));
        if (!Hud->IsMenuOpen()) Hud->ToggleInWorldMenu();
        if (HashWorldState(Life->GetWorldState()) != Run->Hash) return FStepOutcome::Fail(TEXT("Continue differs from the original saved hash"));
        const auto Before = HashWorldState(Life->GetWorldState());
        P509SessionLog::Boundary(TEXT("observation"), Life->GetSim());
        SchoolWorkCommand Again;
        Again.Kind = SchoolWorkActionKind::Submit;
        Again.WorkId = Run->WorkId;
        const auto Reply = Life->SubmitSchoolCommand({CommandSource::Player, Again});
        return !Reply.Accepted && Reply.Why.Code == "school.work.already_submitted" && Reply.From == Reply.To &&
            HashWorldState(Life->GetWorldState()) == Before ?
            FStepOutcome::Pass(TEXT("fresh-process Continue preserved the hash; stale submit produced no time or outcome")) :
            FStepOutcome::Fail(TEXT("repeated submission or logging mutated the resumed world"));
    }), Run, true));
    Out.Add(P509Phase(P509Action(TEXT("continued_log_order"), [Run](UPWAcceptanceSubsystem&, FStepRuntime&)
    {
        if (!P509SessionLog::Idle()) return FStepOutcome::Wait();
        std::vector<JsonValue> Rows;
        if (!P509Rows(P509SessionLog::Path(), Rows)) return FStepOutcome::Fail(TEXT("continued session JSONL unreadable"));
        bool Loaded = false, Refused = false;
        const std::string Hash = TCHAR_TO_UTF8(*FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Run->Hash)));
        for (const auto& Row : Rows)
        {
            if (P509String(Row, "boundary") == "load_or_continue" && P509String(Row, "world_hash") == Hash)
                Loaded = !P509String(Row, "next_step").empty();
            if (P509String(Row, "work_id") == Run->WorkId && P509String(Row, "action") == "submit")
                Refused = Loaded && !P509Accepted(Row) && P509Int(Row, "charged_minutes") == 0;
            if ((P509String(Row, "event_id") == "day.event.completed" || P509String(Row, "type") == "receipt") &&
                P509String(Row, "activity_id") == Run->WorkId) return FStepOutcome::Fail(TEXT("Continue duplicated the original completion or receipt"));
        }
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *(FPaths::GetPath(P509SessionLog::Path()) / TEXT("session_*.jsonl")), true, false);
        bool Original = false;
        for (const auto& File : Files)
        {
            const FString Path = FPaths::GetPath(P509SessionLog::Path()) / File;
            if (Path == P509SessionLog::Path()) continue;
            std::vector<JsonValue> Earlier;
            if (P509Rows(Path, Earlier) && P509Original(Earlier, *Run)) { Original = true; break; }
        }
        return Loaded && Refused && Original ?
            FStepOutcome::Pass(TEXT("two session files link by saved hash; Continue has a next step and no duplicate outcome")) :
            FStepOutcome::Fail(TEXT("save/Continue link, next step, refused repeat or original session absent"));
    }), Run, true));
}
}
