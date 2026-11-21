#include "Countdown.h"

#include "ConductLedger.h"
#include "CountdownInternal.h"
#include "IdentityRegistry.h"
#include "IdentityText.h"

#include <algorithm>
#include <utility>

// Saving the countdown, and the receipt (ROADMAP M4-03). The text is the whole state; the parse
// re-derives every seed and roll from what it reads, so a save cannot be edited into a different outcome.
namespace PerfectWorld
{
namespace
{
namespace Text = IdentityText;

constexpr std::uint64_t CountdownHashTag = 0x5057434F554E5444ULL; // "PWCOUNTD"

std::string Num(std::int64_t value)
{
    return std::to_string(value);
}

std::string Token(const std::string& text)
{
    return text.empty() ? "-" : text;
}

std::string PointsLine(std::string_view head, const CountdownPoints& points)
{
    std::string line(head);
    for (const std::int32_t value : points)
    {
        line += ' ';
        line += Num(value);
    }
    line += '\n';
    return line;
}

std::string CountdownBody(const CountdownState& state)
{
    std::string out(CountdownTextHeader);
    out += ' ' + Num(CountdownFormatVersion) + "\nseed " + std::to_string(state.Seed) + "\nplayer " + state.Player + "\nspan " + Num(state.Start) + ' ' +
           Num(state.End) + ' ' + Num(state.EventOnset) + "\nflags " + Num(state.EventFired ? 1 : 0) + ' ' + Num(state.Closed ? 1 : 0) + ' ' +
           Num(state.PaddedMinutes) + '\n';
    out += PointsLine("accrue dayloop", state.DayLoopPrep);
    out += PointsLine("accrue outcome", state.OutcomePrep);
    out += "accrue extra " + Num(state.Composure) + ' ' + Num(state.Goodwill) + ' ' + Num(state.RouteMinutes) + '\n';
    out += "commitments " + Num(static_cast<std::int64_t>(state.Commitments.size())) + '\n';
    for (const CountdownCommitment& c : state.Commitments)
    {
        out += "c " + c.Id + ' ' + Token(c.Contact) + ' ' + std::to_string(c.Seed) + ' ' + Num(c.Odds[0]) + ' ' + Num(c.Odds[1]) + ' ' + Num(c.Odds[2]) +
               ' ' + Num(c.CommittedAt) + ' ' + Num(c.Start) + ' ' + Num(c.End) + ' ' + Num(c.Resolved ? 1 : 0) + ' ' +
               Num(static_cast<int>(c.Outcome)) + ' ' + Num(c.Roll) + '\n';
    }
    out += "obligations " + Num(static_cast<std::int64_t>(state.Obligations.size())) + '\n';
    for (const CountdownObligationRecord& o : state.Obligations)
    {
        out += "o " + o.Id + ' ' + o.Person + ' ' + Num(static_cast<int>(o.Disposition)) + ' ' + Num(o.Negotiated ? 1 : 0) + ' ' +
               Num(static_cast<int>(o.Attempt)) + ' ' + Num(o.Accepted ? 1 : 0) + ' ' + std::to_string(o.Seed) + ' ' + Num(o.AcceptPermille) + ' ' +
               Token(o.Helper) + ' ' + Num(o.Start) + ' ' + Num(o.Minutes) + ' ' + Num(o.Done ? 1 : 0) + '\n';
    }
    out += "requests " + Num(static_cast<std::int64_t>(state.Requests.size())) + '\n';
    for (const CountdownRequest& r : state.Requests)
        out += "r " + r.Actor + ' ' + r.Other + ' ' + std::string(RelationshipActKindName(r.Kind)) + ' ' + r.Reason + ' ' + Num(r.At) + '\n';
    out += "ledger " + Num(static_cast<std::int64_t>(state.Ledger.size())) + '\n';
    for (const CountdownLedgerLine& l : state.Ledger)
        out += "l " + Num(l.At) + ' ' + std::string(CountdownLedgerKindName(l.Kind)) + ' ' + l.Id + ' ' + Num(l.A) + ' ' + Num(l.B) + '\n';
    return out;
}

std::uint64_t CountdownHashText(const std::string& body)
{
    Text::HashWriter out;
    out.Unsigned(CountdownHashTag);
    out.Unsigned(CountdownFormatVersion);
    out.Text(body);
    return out.Result();
}

/** A strict line reader: each call takes the next line, which must start with the keyword and have exactly `count` fields. */
class Reader
{
public:
    explicit Reader(std::vector<std::string_view> lines) : Lines_(std::move(lines)) {}
    [[nodiscard]] bool Ok() const { return Ok_; }
    [[nodiscard]] bool AtEnd() const { return Cursor_ == Lines_.size(); }
    std::vector<std::string_view> Next(std::string_view keyword, std::size_t count)
    {
        if (!Ok_ || Cursor_ >= Lines_.size())
            return Fail();
        const auto fields = Text::SplitFields(Lines_[Cursor_++]);
        if (!fields || fields->size() != count || (*fields)[0] != keyword)
            return Fail();
        return *fields;
    }

private:
    std::vector<std::string_view> Fail()
    {
        Ok_ = false;
        return {};
    }
    std::vector<std::string_view> Lines_;
    std::size_t Cursor_ = 0;
    bool Ok_ = true;
};

std::optional<std::int64_t> Signed(std::string_view text, std::int64_t low, std::int64_t high)
{
    const std::optional<std::int64_t> value = Text::ParseSigned(text);
    if (!value || *value < low || *value > high)
        return std::nullopt;
    return value;
}

bool Flag(std::string_view text, bool& out)
{
    if (text != "0" && text != "1")
        return false;
    out = text == "1";
    return true;
}

bool ReadPoints(const std::vector<std::string_view>& fields, std::size_t first, CountdownPoints& points)
{
    for (std::size_t i = 0; i < CountdownDimensionCount; ++i)
    {
        const auto value = Signed(fields[first + i], -100000, 100000);
        if (!value)
            return false;
        points[i] = static_cast<std::int32_t>(*value);
    }
    return true;
}

template <typename Enum>
bool ReadEnum(std::string_view text, std::int64_t last, Enum& out)
{
    const auto value = Signed(text, 0, last);
    if (!value)
        return false;
    out = static_cast<Enum>(*value);
    return true;
}

bool ReadId(std::string_view text, std::string& out)
{
    if (!Text::IsToken(text))
        return false;
    out = std::string(text);
    return true;
}

bool ReadOptionalPerson(std::string_view text, std::string& out)
{
    if (text == "-")
    {
        out.clear();
        return true;
    }
    if (!IsValidIdentityId(text))
        return false;
    out = std::string(text);
    return true;
}

bool ReadCommitment(const std::vector<std::string_view>& f, const CountdownState& state, CountdownCommitment& c)
{
    if (f.size() != 13)
        return false;
    const auto o0 = Signed(f[4], 0, 1000), o1 = Signed(f[5], 0, 1000), o2 = Signed(f[6], 0, 1000);
    const auto seed = Text::ParseUnsigned(f[3]);
    const auto at = Signed(f[7], 0, INT64_MAX), start = Signed(f[8], 0, INT64_MAX), end = Signed(f[9], 0, INT64_MAX);
    const auto roll = Signed(f[12], 0, 999);
    if (!o0 || !o1 || !o2 || !seed || !at || !start || !end || !roll || !ReadId(f[1], c.Id) || !ReadOptionalPerson(f[2], c.Contact) ||
        !Flag(f[10], c.Resolved) || !ReadEnum(f[11], 2, c.Outcome))
        return false;
    c.Seed = *seed;
    c.Odds = {static_cast<std::int32_t>(*o0), static_cast<std::int32_t>(*o1), static_cast<std::int32_t>(*o2)};
    c.CommittedAt = *at;
    c.Start = *start;
    c.End = *end;
    c.Roll = static_cast<std::int32_t>(*roll);
    const CountdownOpportunityDef* def = CountdownFindOpportunity(c.Id);
    if (def == nullptr || c.Odds[0] + c.Odds[1] + c.Odds[2] != 1000 || c.Seed != CountdownCommitSeed(state.Seed, c.Id))
        return false;
    if (c.Start != CountdownInstantOf(def->Day, def->StartMinute) || c.End != c.Start + def->Block.Minutes || c.CommittedAt < state.Start ||
        c.CommittedAt > c.Start || (def->Role.empty() != c.Contact.empty()))
        return false;
    // Never rerolls: a resolved draw is the stored seed's draw, and the outcome is what the stored odds give it.
    if (c.Resolved)
        return c.Roll == CountdownRoll(c.Seed) && c.Outcome == CountdownOutcomeFor(c.Odds, c.Roll);
    return c.Outcome == CountdownOutcome::Fair && c.Roll == 0;
}

bool ReadObligation(const std::vector<std::string_view>& f, const CountdownState& state, CountdownObligationRecord& o)
{
    if (f.size() != 13)
        return false;
    const auto seed = Text::ParseUnsigned(f[7]);
    const auto permille = Signed(f[8], 0, 1000), start = Signed(f[10], 0, INT64_MAX), minutes = Signed(f[11], 0, MinutesPerDay);
    if (!seed || !permille || !start || !minutes || !ReadId(f[1], o.Id) || !IsValidIdentityId(f[2]) || !ReadEnum(f[3], 3, o.Disposition) ||
        !Flag(f[4], o.Negotiated) || !ReadEnum(f[5], 3, o.Attempt) || !Flag(f[6], o.Accepted) || !ReadOptionalPerson(f[9], o.Helper) ||
        !Flag(f[12], o.Done))
        return false;
    o.Person = std::string(f[2]);
    o.Seed = *seed;
    o.AcceptPermille = static_cast<std::int32_t>(*permille);
    o.Start = *start;
    o.Minutes = static_cast<std::int32_t>(*minutes);
    const CountdownObligationDef* def = CountdownFindObligation(o.Id);
    if (def == nullptr)
        return false;
    const GameInstant original = CountdownInstantOf(def->Day, def->StartMinute);
    const GameInstant deferred = CountdownInstantOf(def->DeferDay, def->DeferMinute);
    const bool untouched = o.Start == original && o.Minutes == def->Minutes;
    if (!o.Negotiated)
        return o.Disposition == CountdownDisposition::Standing && o.Attempt == CountdownProposal::Honour && !o.Accepted && o.Seed == 0 &&
               o.AcceptPermille == 0 && o.Helper.empty() && untouched;
    if (o.Attempt == CountdownProposal::Honour || (o.Done && o.Disposition == CountdownDisposition::Declined))
        return false;
    if (o.Attempt == CountdownProposal::Decline)
        return o.Accepted && o.Disposition == CountdownDisposition::Declined && o.Seed == 0 && o.AcceptPermille == 1000 && o.Helper.empty() &&
               o.Minutes == 0 && o.Start == original;
    if (o.Seed != CountdownNegotiationSeed(state.Seed, o.Id, o.Attempt) || o.Accepted != (CountdownRoll(o.Seed) < o.AcceptPermille))
        return false;
    if (!o.Accepted)
        return o.Disposition == CountdownDisposition::Standing && o.Helper.empty() && untouched;
    if (o.Attempt == CountdownProposal::Share)
        return o.Disposition == CountdownDisposition::Shared && !o.Helper.empty() && o.Minutes == def->ShareMinutes && o.Start == original;
    return o.Disposition == CountdownDisposition::Deferred && o.Helper.empty() && o.Minutes == def->Minutes && o.Start == deferred;
}

/** The accruals a state's resolved things imply: the strict parse requires the stored sums to equal them. */
bool AccrualsAgree(const CountdownState& state)
{
    CountdownState expected;
    for (const CountdownCommitment& c : state.Commitments)
    {
        if (c.Resolved)
            CountdownApplyEffect(expected, CountdownFindOpportunity(c.Id)->Effects[static_cast<std::size_t>(c.Outcome)]);
    }
    for (const CountdownObligationRecord& o : state.Obligations)
    {
        const CountdownObligationDef& def = *CountdownFindObligation(o.Id);
        if (o.Negotiated)
        {
            if (o.Attempt == CountdownProposal::Decline)
                expected.Goodwill += def.DeclineGoodwill;
            else if (o.Attempt == CountdownProposal::Share)
                expected.Goodwill += o.Accepted ? def.ShareAcceptedGoodwill : def.ShareRefusedGoodwill;
            else
                expected.Goodwill += o.Accepted ? def.DeferAcceptedGoodwill : def.DeferRefusedGoodwill;
        }
        expected.Goodwill += o.Done ? def.HonourGoodwill : 0;
    }
    return expected.OutcomePrep == state.OutcomePrep && expected.Composure == state.Composure && expected.Goodwill == state.Goodwill &&
           expected.RouteMinutes == state.RouteMinutes;
}

bool MutuallyExclusive(const CountdownState& state)
{
    for (std::size_t i = 0; i < state.Commitments.size(); ++i)
    {
        for (std::size_t j = i + 1; j < state.Commitments.size(); ++j)
        {
            const CountdownCommitment& a = state.Commitments[i];
            const CountdownCommitment& b = state.Commitments[j];
            if (a.Id == b.Id || CountdownFindOpportunity(a.Id)->Group == CountdownFindOpportunity(b.Id)->Group ||
                CountdownOverlaps(a.Start, a.End, b.Start, b.End))
                return false;
        }
    }
    return true;
}
} // namespace

std::uint64_t HashCountdown(const CountdownState& state)
{
    return CountdownHashText(CountdownBody(state));
}

std::string SerialiseCountdown(const CountdownState& state)
{
    const std::string body = CountdownBody(state);
    return body + "end " + std::to_string(CountdownHashText(body)) + '\n';
}

std::optional<CountdownState> ParseCountdown(std::string_view text)
{
    const auto lines = Text::SplitLines(text);
    if (!lines || lines->size() < 13)
        return std::nullopt;
    const std::string_view endLine = lines->back();
    const std::size_t bodyLength = text.size() - endLine.size() - 1;
    Reader in(std::vector<std::string_view>(lines->begin(), lines->end() - 1));
    CountdownState state;
    const auto header = in.Next(CountdownTextHeader, 2);
    if (!in.Ok() || header[1] != std::to_string(CountdownFormatVersion))
        return std::nullopt;
    const auto seed = in.Next("seed", 2);
    const auto player = in.Next("player", 2);
    const auto span = in.Next("span", 4);
    const auto flags = in.Next("flags", 4);
    const auto dayloop = in.Next("accrue", 8);
    if (!in.Ok() || dayloop[1] != "dayloop")
        return std::nullopt;
    const auto outcome = in.Next("accrue", 8);
    if (!in.Ok() || outcome[1] != "outcome")
        return std::nullopt;
    const auto extra = in.Next("accrue", 5);
    const auto commitmentCount = in.Next("commitments", 2);
    if (!in.Ok())
        return std::nullopt;
    const auto seedValue = Text::ParseUnsigned(seed[1]);
    const auto start = Signed(span[1], 0, INT64_MAX), end = Signed(span[2], 0, INT64_MAX), onset = Signed(span[3], 0, INT64_MAX);
    const auto padded = Signed(flags[3], 0, 100 * MinutesPerDay);
    const auto composure = Signed(extra[2], -100000, 100000), goodwill = Signed(extra[3], -100000, 100000), route = Signed(extra[4], 0, 100000);
    const auto commitments = Signed(commitmentCount[1], 0, static_cast<std::int64_t>(CountdownMaxCommitments));
    if (!seedValue || !IsValidIdentityId(player[1]) || !start || !end || !onset || !padded || !composure || !goodwill || !route || !commitments ||
        !Flag(flags[1], state.EventFired) || !Flag(flags[2], state.Closed) || extra[1] != "extra" || !ReadPoints(dayloop, 2, state.DayLoopPrep) ||
        !ReadPoints(outcome, 2, state.OutcomePrep))
        return std::nullopt;
    state.Seed = *seedValue;
    state.Player = std::string(player[1]);
    state.Start = *start;
    state.End = *end;
    state.EventOnset = *onset;
    state.PaddedMinutes = static_cast<std::int32_t>(*padded);
    state.Composure = static_cast<std::int32_t>(*composure);
    state.Goodwill = static_cast<std::int32_t>(*goodwill);
    state.RouteMinutes = static_cast<std::int32_t>(*route);
    const CountdownWorldEventDef& event = CountdownWorldEvent();
    if (state.Start != CountdownStartInstant() || state.End != CountdownEndInstant() ||
        state.EventOnset != CountdownInstantOf(event.Day, event.Minute) || (state.Closed && !state.EventFired))
        return std::nullopt;
    for (std::int64_t i = 0; i < *commitments; ++i)
    {
        CountdownCommitment commitment;
        if (!ReadCommitment(in.Next("c", 13), state, commitment) || CountdownFindCommitment(state, commitment.Id) != nullptr)
            return std::nullopt;
        state.Commitments.push_back(std::move(commitment));
    }
    const auto obligationCount = in.Next("obligations", 2);
    const auto obligations = in.Ok() ? Signed(obligationCount[1], 0, static_cast<std::int64_t>(CountdownMaxObligations)) : std::nullopt;
    if (!obligations)
        return std::nullopt;
    for (std::int64_t i = 0; i < *obligations; ++i)
    {
        CountdownObligationRecord record;
        if (!ReadObligation(in.Next("o", 13), state, record) || CountdownFindRecord(state, record.Id) != nullptr)
            return std::nullopt;
        state.Obligations.push_back(std::move(record));
    }
    const auto requestCount = in.Next("requests", 2);
    const auto requests = in.Ok() ? Signed(requestCount[1], 0, static_cast<std::int64_t>(CountdownMaxRequests)) : std::nullopt;
    if (!requests)
        return std::nullopt;
    for (std::int64_t i = 0; i < *requests; ++i)
    {
        const auto f = in.Next("r", 6);
        const auto kind = in.Ok() ? ParseRelationshipActKind(f[3]) : std::nullopt;
        const auto at = in.Ok() ? Signed(f[5], 0, INT64_MAX) : std::nullopt;
        if (!kind || !at || !IsValidIdentityId(f[1]) || !IsValidIdentityId(f[2]) || !Text::IsToken(f[4]))
            return std::nullopt;
        state.Requests.push_back({std::string(f[1]), std::string(f[2]), *kind, std::string(f[4]), *at});
    }
    const auto ledgerCount = in.Next("ledger", 2);
    const auto ledger = in.Ok() ? Signed(ledgerCount[1], 0, static_cast<std::int64_t>(CountdownMaxLedgerLines)) : std::nullopt;
    if (!ledger)
        return std::nullopt;
    for (std::int64_t i = 0; i < *ledger; ++i)
    {
        const auto f = in.Next("l", 6);
        if (!in.Ok())
            return std::nullopt;
        CountdownLedgerLine line;
        const auto at = Signed(f[1], 0, INT64_MAX), a = Signed(f[4], -1000000, 1000000), b = Signed(f[5], -1000000, 1000000);
        bool found = false;
        for (int kind = 0; kind <= static_cast<int>(CountdownLedgerKind::Closed); ++kind)
        {
            if (CountdownLedgerKindName(static_cast<CountdownLedgerKind>(kind)) == f[2])
            {
                line.Kind = static_cast<CountdownLedgerKind>(kind);
                found = true;
            }
        }
        if (!found || !at || !a || !b || !ReadId(f[3], line.Id) || (!state.Ledger.empty() && *at < state.Ledger.back().At))
            return std::nullopt;
        line.At = *at;
        line.A = static_cast<std::int32_t>(*a);
        line.B = static_cast<std::int32_t>(*b);
        state.Ledger.push_back(std::move(line));
    }
    // The end line carries the hash of everything above it; nothing may follow.
    const auto endFields = Text::SplitFields(endLine);
    const auto hash = endFields && endFields->size() == 2 && (*endFields)[0] == "end" ? Text::ParseUnsigned((*endFields)[1]) : std::nullopt;
    if (!in.AtEnd() || !in.Ok() || !hash || *hash != CountdownHashText(std::string(text.substr(0, bodyLength))) || !MutuallyExclusive(state) ||
        !AccrualsAgree(state) || std::string(text.substr(0, bodyLength)) != CountdownBody(state))
        return std::nullopt;
    return state;
}

std::string RenderCountdownReceipt(const CountdownState& state)
{
    std::string out = "The ten days, " + FormatCalendarDate(CalendarDayOfInstant(state.Start)) + " 18:00 to " +
                      FormatCalendarDate(CalendarDayOfInstant(state.End)) + " 06:00\n";
    for (const CountdownLedgerLine& line : state.Ledger)
    {
        const CalendarTime when = GameClock::ToCalendar(line.At);
        out += FormatCalendarDate(CalendarDayOfInstant(line.At)) + ' ' + (when.Hour < 10 ? "0" : "") + std::to_string(when.Hour) + ':' +
               (when.Minute < 10 ? "0" : "") + std::to_string(when.Minute) + ' ' + std::string(CountdownLedgerKindName(line.Kind)) + ' ' + line.Id;
        switch (line.Kind)
        {
        case CountdownLedgerKind::Opened: out += ": " + Num(line.A) + " obligations stand"; break;
        case CountdownLedgerKind::Committed: out += ": " + Num(line.B) + " minutes, closes " + Num(line.A) + " other options"; break;
        case CountdownLedgerKind::Resolved:
            out += ": " + std::string(CountdownOutcomeName(static_cast<CountdownOutcome>(line.A))) + " (draw " + Num(line.B) + " of 1000)";
            break;
        case CountdownLedgerKind::Negotiated:
            out += ": " + std::string(CountdownProposalName(static_cast<CountdownProposal>(line.A))) + (line.B != 0 ? ", accepted" : ", refused");
            break;
        case CountdownLedgerKind::Honoured: out += ": " + Num(line.B) + " minutes, standing " + (line.A >= 0 ? "+" : "") + Num(line.A); break;
        case CountdownLedgerKind::News: out += ": the road would add " + Num(line.A) + " minutes on examination morning"; break;
        case CountdownLedgerKind::Closed: out += ": " + Num(line.A) + " minutes spent awake on your behalf to reach 06:00"; break;
        }
        out += '\n';
    }
    return out;
}
} // namespace PerfectWorld
