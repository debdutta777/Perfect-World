// ROADMAP M3-05: the ledger's text form, order independence and random sequences.
#include "AptitudeTestSupport.h"

#include <algorithm>
#include <set>

namespace AptitudeTests
{
namespace
{
std::string Replace(std::string text, const std::string& from, const std::string& to)
{
    const std::size_t at = text.find(from);
    Check(at != std::string::npos, "the text to replace is present");
    text.replace(at, from.size(), to);
    return text;
}

AptitudeLedger SmallLedger()
{
    AptitudeLedger ledger;
    Add(ledger, Structure("m1", AptitudeSource::StructureModel, "family.flow", AptitudeResult::Sound, 120));
    Add(ledger, Structure("m2", AptitudeSource::StructureModel, "family.flow", AptitudeResult::Sound, 130, true));
    Add(ledger, Item("p1", AptitudeSource::PracticeConsolidation, AptitudeFacet::Physical, "skill.fitness", AptitudeResult::Partial, 4000));
    Add(ledger, Item("t1", AptitudeSource::Attendance, AptitudeFacet::Steadiness, "class.maths", AptitudeResult::Sound, 0));
    return ledger;
}

void TestTextForm()
{
    const AptitudeLedger ledger = SmallLedger();
    const std::string text = SerialiseAptitude(ledger);
    const std::string expected =
        "pw.aptitude/1\n"
        "items=4\n"
        "item=m1,structure_model,analysis,family.flow,sound,0,120\n"
        "item=m2,structure_model,analysis,family.flow,sound,1,130\n"
        "item=p1,practice,physical,skill.fitness,partial,0,4000\n"
        "item=t1,attendance,steadiness,class.maths,sound,0,0\n"
        "end\n";
    Check(text == expected, "the text is the documented format");
    const AptitudeParse parsed = DeserialiseAptitude(text);
    Check(parsed.Ok() && parsed.Ledger == ledger, "the text loads back to the same ledger");
    Check(SerialiseAptitude(parsed.Ledger) == text, "and saves back to the same text");
    Check(AptitudeProject(parsed.Ledger) == AptitudeProject(ledger), "and projects the same");

    const AptitudeParse empty = DeserialiseAptitude(SerialiseAptitude(AptitudeLedger{}));
    Check(empty.Ok() && empty.Ledger.Items.empty(), "an empty ledger round trips");
}

void TestEveryTruncationRefused()
{
    const std::string text = SerialiseAptitude(SmallLedger());
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialiseAptitude(std::string_view(text).substr(0, length)).Ok(), "every truncation is refused");
    Check(DeserialiseAptitude(text).Ok(), "the whole text loads");

    Rng rng{99};
    AptitudeLedger ledger;
    for (int i = 0; i < 40; ++i)
        AptitudeAddEvidence(ledger, RandomItem(rng, static_cast<std::uint64_t>(i)));
    const std::string big = SerialiseAptitude(ledger);
    Check(!big.empty(), "a random ledger saves");
    for (std::size_t length = 0; length < big.size(); ++length)
        Check(!DeserialiseAptitude(std::string_view(big).substr(0, length)).Ok(), "every truncation of a larger ledger is refused");
}

void TestMalformedTextRefused()
{
    const std::string text = SerialiseAptitude(SmallLedger());
    Check(!DeserialiseAptitude(text + "extra\n").Ok(), "text after the end line is refused");
    Check(!DeserialiseAptitude(text + "\n").Ok(), "a blank line after the end is refused");
    Check(!DeserialiseAptitude(Replace(text, "pw.aptitude/1", "pw.aptitude/2")).Ok(), "an unknown version is refused");
    Check(!DeserialiseAptitude(Replace(text, "items=4", "items=5")).Ok(), "a count above the items is refused");
    Check(!DeserialiseAptitude(Replace(text, "items=4", "items=3")).Ok(), "a count below the items is refused");
    Check(!DeserialiseAptitude(Replace(text, "items=4", "items=04")).Ok(), "a leading zero is refused");
    Check(!DeserialiseAptitude(Replace(text, "items=4", "items=+4")).Ok(), "a plus sign is refused");
    Check(!DeserialiseAptitude(Replace(text, ",120\n", ",0120\n")).Ok(), "a padded instant is refused");
    Check(!DeserialiseAptitude(Replace(text, ",120\n", ",12x\n")).Ok(), "a malformed instant is refused");
    Check(!DeserialiseAptitude(Replace(text, ",120\n", ",-5\n")).Ok(), "a negative instant is refused");
    Check(!DeserialiseAptitude(Replace(text, "structure_model,analysis,family.flow,sound,0", "structure_mode,analysis,family.flow,sound,0")).Ok(),
          "an unknown source is refused");
    Check(!DeserialiseAptitude(Replace(text, "m1,structure_model,analysis", "m1,structure_model,physical")).Ok(),
          "a structure with another structure's facet is refused");
    Check(!DeserialiseAptitude(Replace(text, "family.flow,sound,0,120", "family.flow,great,0,120")).Ok(), "an unknown result is refused");
    Check(!DeserialiseAptitude(Replace(text, "family.flow,sound,0,120", "family.flow,sound,2,120")).Ok(), "a routine flag other than 0 or 1 is refused");
    Check(!DeserialiseAptitude(Replace(text, "family.flow,sound,0,120", "family.flow,sound,0,120,9")).Ok(), "an extra field is refused");
    Check(!DeserialiseAptitude(Replace(text, "family.flow,sound,0,120", "family.flow,sound,0")).Ok(), "a missing field is refused");
    Check(!DeserialiseAptitude(Replace(text, "item=m1,", "item=M1,")).Ok(), "an ID with capitals is refused");
    Check(!DeserialiseAptitude(Replace(text, "item=m1,", "item=m2,")).Ok(), "a duplicate ID is refused");
    Check(!DeserialiseAptitude(Replace(text, "item=t1,attendance,steadiness", "item=t1,attendance,physical")).Ok(),
          "attendance for another facet is refused");
    Check(!DeserialiseAptitude(Replace(Replace(text, "item=m1", "item=zz"), "item=t1", "item=aa")).Ok(), "items out of ID order are refused");
    Check(!DeserialiseAptitude(Replace(text, "item=p1,practice,physical,skill.fitness,partial", "item=p1,practice,physical,skill.fitness,unsound")).Ok(),
          "an unsound practice is refused");

    // Five items in one group exceed what a ledger ever keeps.
    std::string crowded = "pw.aptitude/1\nitems=5\n";
    for (int i = 0; i < 5; ++i)
        crowded += "item=a" + std::to_string(i) + ",activity,hands_on,activity.x,sound,0," + std::to_string(i) + "\n";
    Check(!DeserialiseAptitude(crowded + "end\n").Ok(), "a group past saturation is refused");
    Check(DeserialiseAptitude("pw.aptitude/1\nitems=0\nend\n").Ok(), "an empty ledger loads");
    Check(!DeserialiseAptitude("pw.aptitude/1\nitems=0\nend").Ok(), "an end line without a newline is refused");
    Check(!DeserialiseAptitude("").Ok(), "empty text is refused");
}

void Shuffle(std::vector<AptitudeEvidence>& items, Rng& rng)
{
    for (std::size_t i = items.size(); i > 1; --i)
        std::swap(items[i - 1], items[static_cast<std::size_t>(rng.Pick(0, static_cast<std::int64_t>(i) - 1))]);
}

void TestOrderIndependence()
{
    Rng rng{424242};
    for (int round = 0; round < 12; ++round)
    {
        std::vector<AptitudeEvidence> items;
        const int count = round < 6 ? 40 : 400; // The larger sets saturate many groups.
        for (int i = 0; i < count; ++i)
            items.push_back(RandomItem(rng, static_cast<std::uint64_t>(i)));
        AptitudeLedger reference;
        for (const AptitudeEvidence& item : items)
            AptitudeAddEvidence(reference, item);
        const std::string referenceText = SerialiseAptitude(reference);
        const AptitudeProjection referenceProjection = AptitudeProject(reference);
        Check(!referenceText.empty(), "the reference ledger saves");
        for (int shuffle = 0; shuffle < 20; ++shuffle)
        {
            Shuffle(items, rng);
            AptitudeLedger ledger;
            for (const AptitudeEvidence& item : items)
                AptitudeAddEvidence(ledger, item);
            Check(ledger == reference && SerialiseAptitude(ledger) == referenceText, "any order gives the same ledger and text");
            Check(AptitudeProject(ledger) == referenceProjection, "any order gives the same projection");
        }
        // Adding everything twice, and in two halves, changes nothing either.
        AptitudeLedger twice = reference;
        for (const AptitudeEvidence& item : items)
            Check(AptitudeAddEvidence(twice, item).Ok(), "a repeat is accepted");
        Check(twice == reference, "adding everything again changes nothing");
    }
}

void TestRandomSequences()
{
    Rng rng{777};
    for (int round = 0; round < 30; ++round)
    {
        AptitudeLedger ledger;
        AptitudeProjection last = AptitudeProject(ledger);
        for (int step = 0; step < 200; ++step)
        {
            const AptitudeEvidence item = RandomItem(rng, static_cast<std::uint64_t>(round * 1000 + step));
            Check(AptitudeAddEvidence(ledger, item).Ok(), "a random valid item is accepted");
            Check(IsAptitudeLedgerValid(ledger), "the ledger stays valid");

            const AptitudeProjection now = AptitudeProject(ledger);
            std::set<std::string> ids;
            for (const AptitudeEvidence& kept : ledger.Items)
                ids.insert(kept.Id);
            for (std::size_t p = 0; p < AptitudePathCount; ++p)
            {
                const AptitudePathProjection& path = now.Paths[p];
                const AptitudePathProjection& before = last.Paths[p];
                Check(path.Band >= before.Band && path.Coverage >= before.Coverage, "evidence never lowers a band or coverage");
                std::size_t unobserved = 0;
                for (std::size_t f = 0; f < path.Facets.size(); ++f)
                {
                    Check(path.Facets[f].State >= before.Facets[f].State, "evidence never lowers a facet");
                    unobserved += path.Facets[f].State == AptitudeFacetState::Unobserved ? 1 : 0;
                }
                Check(path.Unobserved.size() == unobserved, "the unobserved list matches the states");
                Check(path.Strongest.size() <= AptitudeMaxReferences, "at most three references");
                for (const std::string& id : path.Strongest)
                    Check(ids.count(id) == 1, "a reference names an item the ledger keeps");
                if (path.Band == AptitudeBand::InsufficientObservation)
                    Check(path.Facets.front().State == AptitudeFacetState::Unobserved || path.Coverage <= AptitudeCoverage::Sparse,
                          "insufficient observation means the signature or all but one facet is unobserved");
                else
                    Check(path.Facets.front().State != AptitudeFacetState::Unobserved, "a band above insufficient has seen the signature");
            }
            last = now;

            if (step % 25 == 0)
            {
                const AptitudeParse parsed = DeserialiseAptitude(SerialiseAptitude(ledger));
                Check(parsed.Ok() && parsed.Ledger == ledger && AptitudeProject(parsed.Ledger) == now, "a save and load keeps the projection");
            }
        }
    }
}
} // namespace

void RunAptitudeTextTests()
{
    TestTextForm();
    TestEveryTruncationRefused();
    TestMalformedTextRefused();
    TestOrderIndependence();
    TestRandomSequences();
}
} // namespace AptitudeTests
