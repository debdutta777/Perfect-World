// P3-14: homework reconciliation (school.work.reconcile, ReconcileMissedSchoolWork). Briefs whose boundary passed while no
// brief catalogue was bound are issued once each by stable id with a disclosed seven-day recovery window; existing records,
// submissions and deadlines never change; later scheduled issuance accepts the recovered form; the desk stays read-only;
// repeated reconciliation and reload are inert; a recovered paper handed in within its window is not late.
#include "SchoolWorkIssuance.h"

#include "P36HomeworkTestSupport.h"

using namespace PerfectWorld;

namespace
{
void P314HRefused(WorldSim& sim, CommandSource source, std::string_view code, std::string_view what)
{
    const std::uint64_t before = HashWorldState(sim.State());
    const CommandResult result = sim.Submit({source, SchoolWorkReconcile{}});
    if (result.Accepted || result.Why.Code != code)
        std::cerr << "expected " << code << " got " << (result.Accepted ? std::string("accepted") : result.Why.Code) << '\n';
    P2CExpect(!result.Accepted && result.Why.Code == code && HashWorldState(sim.State()) == before, what);
}

const SchoolWorkBrief& P314HAfter(const std::vector<SchoolWorkBrief>& briefs, GameInstant at)
{
    for (const auto& brief : briefs)
        if (brief.AvailableAt > at)
            return brief;
    P2CExpect(false, "a later brief exists");
    return briefs.front();
}

void P314HSubmit(WorldSim& sim, const std::string& id)
{
    P36AnswerPaper(sim, id);
    S04Accept(sim, P36Work(id, SchoolWorkActionKind::Submit), "the paper is handed in");
}

void P314HReconcile(const P36World& w)
{
    const std::vector<SchoolWorkBrief> science = w.Science();
    const SchoolWorkBrief first = science.front();
    std::vector<std::string> missed;
    for (const auto& brief : science)
        if (brief.AvailableAt <= first.AvailableAt)
            missed.push_back(brief.Id);
    std::sort(missed.begin(), missed.end());
    const SchoolWorkBrief second = P314HAfter(science, first.AvailableAt);
    const SchoolWorkBrief third = P314HAfter(science, second.AvailableAt);

    // A life lived past the first boundary with no catalogue bound (a legacy or pre-P3-06 build).
    WorldSim unbound = w.School.Sim(0x3140);
    P36To(unbound, first.AvailableAt + 1);
    P2CExpect(unbound.State().SchoolWork.Records.empty(), "nothing was issued while unbound");
    P314HRefused(unbound, CommandSource::System, "school.work.issue.missing_content", "reconciliation needs the catalogue");

    WorldSim sim = w.Continue(unbound.State());
    std::vector<std::string> pending = sim.PendingSchoolWorkReconciliation();
    std::sort(pending.begin(), pending.end());
    P2CExpect(pending == missed, "exactly the missed boundaries are pending");
    // Bound now: the next boundary issues normally, and its paper is handed in before reconciliation.
    P36To(sim, second.AvailableAt + 1);
    const SchoolWorkRecord* issued = P36Record(sim.State(), second.Id);
    P2CExpect(issued && issued->IssuedAt == second.AvailableAt && issued->Brief == second, "scheduled issuance still works");
    P314HSubmit(sim, second.Id);
    const SchoolWorkRecord submitted = *P36Record(sim.State(), second.Id);
    P2CExpect(submitted.SubmittedAt.has_value(), "the bound brief is submitted");

    P314HRefused(sim, CommandSource::Player, "recovery.source", "only the system reconciles");
    const SchoolWorkState before = sim.State().SchoolWork;
    const GameInstant now = sim.State().Clock.Now();
    S04Accept(sim, SchoolWorkReconcile{}, "the missed homework is reconciled", CommandSource::System);
    for (const std::string& id : missed)
    {
        const SchoolWorkRecord* record = P36Record(sim.State(), id);
        const auto authored = std::find_if(science.begin(), science.end(), [&](const auto& b) { return b.Id == id; });
        P2CExpect(record && record->IssuedAt == now && record->Brief.AvailableAt == now &&
                      record->Brief.DueAt == std::max(authored->DueAt, now + SchoolWorkRecoveryWindowMinutes) &&
                      record->Brief.DueAt >= now + SchoolWorkRecoveryWindowMinutes && IsRecoveredSchoolWork(*record, *authored) &&
                      !record->StartedAt && !record->SubmittedAt,
                  "each missed brief is issued once now, with its disclosed recovery window");
    }
    P2CExpect(*P36Record(sim.State(), second.Id) == submitted &&
                  sim.State().SchoolWork.Records.size() == before.Records.size() + missed.size(),
              "existing records, submissions and deadlines are untouched");
    P314HRefused(sim, CommandSource::System, "school.work.reconcile.nothing_missing", "repeated reconciliation is inert");
    P2CExpect(sim.PendingSchoolWorkReconciliation().empty(), "nothing remains pending");

    // The desk is a read; it neither issues nor changes anything.
    const std::uint64_t hash = HashWorldState(sim.State());
    const SchoolAcademicRecordView desk = QuerySchoolAcademicRecord(sim.State().SchoolWork, w.F.Papers, now);
    P2CExpect(HashWorldState(sim.State()) == hash, "opening the desk is read-only");
    for (const auto& line : desk.Work)
        if (line.Brief.Id == first.Id)
            P2CExpect(line.Available && !line.Overdue && line.CanBegin, "the recovered work is available and not overdue");

    // Later scheduled issuance accepts the recovered form; a recovered paper within its window is not late.
    P314HSubmit(sim, first.Id);
    for (const auto& line : QuerySchoolAcademicRecord(sim.State().SchoolWork, w.F.Papers, sim.State().Clock.Now()).Work)
        if (line.Brief.Id == first.Id)
            P2CExpect(!line.LateSubmission, "hand-in inside the recovery window is not late");
    P36To(sim, std::max(third.AvailableAt + 1, sim.State().Clock.Now() + 1));
    P2CExpect(P36Record(sim.State(), third.Id) != nullptr, "the next boundary issues alongside recovered records");

    // Reload: certified, nothing pending, reconciliation inert.
    WorldSaveData loaded;
    const WorldSaveResult result = DeserializeWorldSave(SerializeWorldSave(w.School.Save(sim.State())), loaded);
    P2CExpect(result.HashVerified && HashWorldState(loaded.World) == HashWorldState(sim.State()), "the reconciled life reloads");
    WorldSim again = w.Continue(loaded.World);
    P2CExpect(again.PendingSchoolWorkReconciliation().empty(), "reload needs no reconciliation");
    P314HRefused(again, CommandSource::System, "school.work.reconcile.nothing_missing", "reconciliation after reload is inert");
    S04Accept(again, AdvanceTime{MinutesPerDay}, "the reloaded life continues", CommandSource::System);

    // A record that is neither the scheduled nor the exact recovered form is an identity conflict.
    SchoolWorkState forged = again.State().SchoolWork;
    for (auto& record : forged.Records)
        if (record.Brief.Id == first.Id)
            record.Brief.DueAt += 1;
    const auto conflict = ReconcileMissedSchoolWork(forged, w.Briefs, *w.School.Cats.Day.Calendar, *w.School.Cats.Day.School,
                                                    w.F.Papers, again.State().Clock.Now());
    P2CExpect(!conflict.Ok() && conflict.Issue.Code == "school.work.issue.identity_conflict", "a forged window is refused");
}
} // namespace

int main(int argc, char** argv)
{
    P2CExpect(argc == 2, "content root");
    const P2CFixture fixture(argv[1]);
    const P36World world(fixture);
    P314HReconcile(world);
    std::cout << "P3-14 HomeworkReconcile passed\n";
    return 0;
}
