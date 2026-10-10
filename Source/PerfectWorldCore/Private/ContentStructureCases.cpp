#include "ContentStructureChecks.h"
#include "StructureCoordinate.h"

#include <set>

// The per-case rules for the explain, investigate and coordinate structures (ROADMAP
// M2-09): each case must fit its family's template and be completable in the way its
// structure asks. The record types and the family rules are in
// ContentStructureSchemas.cpp. The loaders (StructureExplain.cpp and its siblings)
// refuse the same cases again, so the data and the runner cannot drift apart.
namespace PerfectWorld::ContentSchema::StructureChecks
{
std::vector<std::string> CollectIds(const CaseContext& c, std::string_view arrayField, std::string_view label)
{
    std::vector<std::string> ids;
    for (const JsonValue& item : ItemsOf(&c.Case.Data, arrayField))
    {
        const std::string id = StrOf(&item, "id");
        if (!ModelIdWellFormed(id) || Contains(ids, id))
            c.Say(std::string(label) + " IDs must be well-formed and unique (" + id + ")");
        ids.push_back(id);
    }
    return ids;
}

// ---- explain ----------------------------------------------------------------------

void CheckExplainCase(const CaseContext& c)
{
    const std::vector<std::string> claims = CollectIds(c, "claims", "claim");
    const std::vector<std::string> evidenceIds = CollectIds(c, "evidence", "evidence");
    const std::vector<std::string> limitationIds = CollectIds(c, "limitations", "limitation");
    CheckCount(c, "minClaims", "maxClaims", static_cast<std::int64_t>(claims.size()), "claims");
    CheckCount(c, "minEvidence", "maxEvidence", static_cast<std::int64_t>(evidenceIds.size()), "evidence items");
    CheckCount(c, "minLimitations", "maxLimitations", static_cast<std::int64_t>(limitationIds.size()), "limitations");

    std::int64_t defensible = 0;
    for (const JsonValue& claim : ItemsOf(&c.Case.Data, "claims"))
        defensible += FlagOf(&claim, "defensible") ? 1 : 0;
    if (defensible < IntOf(c.Template, "minDefensible"))
        c.Say("the family template wants at least " + std::to_string(IntOf(c.Template, "minDefensible")) + " defensible claims, found " +
              std::to_string(defensible));
    if (defensible >= static_cast<std::int64_t>(claims.size()))
        c.Say("needs at least one indefensible claim: a reading the sources contradict");

    bool beside = false;
    for (const JsonValue& item : ItemsOf(&c.Case.Data, "evidence"))
    {
        const std::string id = StrOf(&item, "id");
        const std::vector<std::string> supports = StrsOf(&item, "supports");
        const std::vector<std::string> against = StrsOf(&item, "against");
        beside = beside || (supports.empty() && against.empty());
        for (const std::vector<std::string>* list : {&supports, &against})
        {
            for (const std::string& claim : *list)
            {
                if (!Contains(claims, claim))
                    c.Say("evidence " + id + " names the claim " + claim + ", which the case does not have");
                if (list == &supports && Contains(against, claim))
                    c.Say("evidence " + id + " both supports and cuts against the claim " + claim);
            }
        }
    }
    if (!beside)
        c.Say("has no evidence item beside the point; padding must be possible");

    for (const JsonValue& limitation : ItemsOf(&c.Case.Data, "limitations"))
    {
        const std::string id = StrOf(&limitation, "id");
        const std::vector<std::string> bounds = StrsOf(&limitation, "bounds");
        if (bounds.empty())
            c.Say("limitation " + id + " bounds no claim");
        for (const std::string& claim : bounds)
        {
            if (!Contains(claims, claim))
                c.Say("limitation " + id + " names the claim " + claim + ", which the case does not have");
        }
    }

    for (const JsonValue& claim : ItemsOf(&c.Case.Data, "claims"))
    {
        const std::string id = StrOf(&claim, "id");
        std::int64_t supporters = 0, contradicting = 0, bounding = 0;
        for (const JsonValue& item : ItemsOf(&c.Case.Data, "evidence"))
        {
            supporters += Contains(StrsOf(&item, "supports"), id) ? 1 : 0;
            contradicting += Contains(StrsOf(&item, "against"), id) ? 1 : 0;
        }
        for (const JsonValue& limitation : ItemsOf(&c.Case.Data, "limitations"))
            bounding += Contains(StrsOf(&limitation, "bounds"), id) ? 1 : 0;
        if (FlagOf(&claim, "defensible"))
        {
            if (supporters < IntOf(c.Template, "minSupport"))
                c.Say("the defensible claim " + id + " needs at least " + std::to_string(IntOf(c.Template, "minSupport")) +
                      " supporting evidence items, found " + std::to_string(supporters));
            if (bounding == 0)
                c.Say("the defensible claim " + id + " has no limitation that bounds it");
            else if (bounding == static_cast<std::int64_t>(limitationIds.size()))
                c.Say("every limitation bounds the defensible claim " + id + "; the choice of limitation must matter");
        }
        else
        {
            if (supporters > 0)
                c.Say("the indefensible claim " + id + " is supported by evidence");
            if (contradicting == 0)
                c.Say("the indefensible claim " + id + " is contradicted by no evidence");
        }
    }
}

// ---- investigate ------------------------------------------------------------------

void CheckInvestigateCase(const CaseContext& c)
{
    const std::vector<std::string> causes = CollectIds(c, "causes", "cause");
    const std::vector<std::string> tests = CollectIds(c, "tests", "test");
    CheckCount(c, "minCauses", "maxCauses", static_cast<std::int64_t>(causes.size()), "causes");
    CheckCount(c, "minTests", "maxTests", static_cast<std::int64_t>(tests.size()), "tests");

    std::int64_t faults = 0;
    for (const JsonValue& cause : ItemsOf(&c.Case.Data, "causes"))
        faults += FlagOf(&cause, "fault") ? 1 : 0;
    if (faults != 1)
        c.Say("exactly one cause must be the fault, found " + std::to_string(faults));

    std::set<std::int64_t> prices;
    std::vector<std::vector<std::string>> implicated;
    for (const JsonValue& test : ItemsOf(&c.Case.Data, "tests"))
    {
        const std::string id = StrOf(&test, "id");
        const std::vector<std::string> implicates = StrsOf(&test, "implicates");
        prices.insert(IntOf(&test, "units"));
        implicated.push_back(implicates);
        for (const std::string& cause : implicates)
        {
            if (!Contains(causes, cause))
                c.Say("test " + id + " implicates the cause " + cause + ", which the case does not have");
        }
        if (implicates.empty() || implicates.size() >= causes.size())
            c.Say("test " + id + " must implicate some, but not all, of the causes");
    }
    if (prices.size() < 2)
        c.Say("the tests all cost the same; tests must have different costs");

    // Every cause must be tellable from every other by some test, or the fault can never be isolated.
    for (std::size_t a = 0; a < causes.size(); ++a)
    {
        for (std::size_t b = a + 1; b < causes.size(); ++b)
        {
            const bool split = std::any_of(implicated.begin(), implicated.end(), [&](const std::vector<std::string>& list) {
                return Contains(list, causes[a]) != Contains(list, causes[b]);
            });
            if (!split)
                c.Say("no test tells the cause " + causes[a] + " from " + causes[b]);
        }
    }
}

// ---- coordinate -------------------------------------------------------------------

void CheckCoordinateCase(const CaseContext& c)
{
    const std::vector<std::string> members = CollectIds(c, "members", "member");
    const std::vector<std::string> tasks = CollectIds(c, "tasks", "task");
    const std::vector<std::string> facts = CollectIds(c, "facts", "fact");
    CheckCount(c, "minMembers", "maxMembers", static_cast<std::int64_t>(members.size()), "members");
    CheckCount(c, "minTasks", "maxTasks", static_cast<std::int64_t>(tasks.size()), "tasks");

    std::string self;
    std::int64_t selves = 0;
    for (const JsonValue& member : ItemsOf(&c.Case.Data, "members"))
    {
        if (FlagOf(&member, "self"))
        {
            ++selves;
            self = StrOf(&member, "id");
        }
    }
    if (selves != 1)
        c.Say("exactly one member must be the player's own (self), found " + std::to_string(selves));

    for (const JsonValue& task : ItemsOf(&c.Case.Data, "tasks"))
    {
        const std::string id = StrOf(&task, "id");
        const std::vector<std::string> fits = StrsOf(&task, "fits");
        if (fits.empty())
            c.Say("task " + id + " is suited to nobody");
        for (const std::string& member : fits)
        {
            if (!Contains(members, member))
                c.Say("task " + id + " is suited to " + member + ", who is not a member of the case");
        }
    }

    std::vector<std::string> holders;
    for (const JsonValue& fact : ItemsOf(&c.Case.Data, "facts"))
    {
        const std::string id = StrOf(&fact, "id");
        const std::string holder = StrOf(&fact, "holder");
        if (!Contains(members, holder))
            c.Say("fact " + id + " is held by " + holder + ", who is not a member of the case");
        if (FlagOf(&fact, "critical"))
        {
            if (holder == self)
                c.Say("the critical fact " + id + " is held by the player; a critical fact must be a classmate's");
            if (!Contains(holders, holder))
                holders.push_back(holder);
        }
    }
    if (holders.empty())
        c.Say("needs at least one critical fact");
    if (static_cast<std::int64_t>(holders.size()) > IntOf(c.Template, "maxCriticalHolders"))
        c.Say("the family template wants at most " + std::to_string(IntOf(c.Template, "maxCriticalHolders")) +
              " members holding a critical fact, found " + std::to_string(holders.size()));
    const bool idle = std::any_of(members.begin(), members.end(), [&](const std::string& m) { return m != self && !Contains(holders, m); });
    if (!idle)
        c.Say("every classmate holds a critical fact; a conversation must be able to find nothing");

    // Some assignment must be Sound: every task with a member who suits it, nobody over capacity.
    CoordinateCaseDef def;
    for (const JsonValue& member : ItemsOf(&c.Case.Data, "members"))
        def.Members.push_back({StrOf(&member, "id"), StrOf(&member, "name"), static_cast<std::int32_t>(IntOf(&member, "capacity")), FlagOf(&member, "self")});
    for (const JsonValue& task : ItemsOf(&c.Case.Data, "tasks"))
        def.Tasks.push_back({StrOf(&task, "id"), StrOf(&task, "name"), static_cast<std::int32_t>(IntOf(&task, "load")), StrsOf(&task, "fits")});
    if (!def.Tasks.empty() && CoordinateSoundAssignment(def).empty())
        c.Say("no assignment keeps every member within capacity and every task with a member who suits it");
}
} // namespace PerfectWorld::ContentSchema::StructureChecks
