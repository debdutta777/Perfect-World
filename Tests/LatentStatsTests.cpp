// ROADMAP M2-02: latent stats and the skill catalogue.
// Usage: PerfectWorldLatentStatsTests <path to Data/Content>
#include "CharacterCreation.h"
#include "ContentRegistry.h"
#include "LatentStats.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

using namespace PerfectWorld;

namespace
{
int Checks = 0;
void Check(bool condition, const char* message)
{
    ++Checks;
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::vector<ContentFile> ReadContentDirectory(const std::filesystem::path& root)
{
    namespace fs = std::filesystem;
    std::vector<fs::path> paths;
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
        {
            paths.push_back(entry.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    std::vector<ContentFile> files;
    for (const fs::path& path : paths)
    {
        std::ifstream in(path, std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        files.push_back({"/" + fs::relative(path, root).generic_string(), text.str()});
    }
    return files;
}

bool HasIssue(const ContentLoadResult& result, const std::string& text)
{
    return std::any_of(result.Issues.begin(), result.Issues.end(), [&](const ContentIssue& issue) {
        return issue.Message.find(text) != std::string::npos && !issue.File.empty();
    });
}

ContentFile Skill(const std::string& id, const std::string& extra = "")
{
    return {"/skill/" + id + ".json",
            "{\"type\":\"skill\",\"id\":\"" + id + "\",\"name\":\"N\",\"category\":\"personal\",\"baselineXp\":0" + extra + "}"};
}

ContentFile AttributeFile(const std::string& shortName, const std::string& extra = "", int maxMilli = 100000)
{
    const std::string id = "attribute." + shortName;
    return {"/attribute/" + id + ".json", "{\"type\":\"attribute\",\"id\":\"" + id + "\",\"name\":\"N\",\"startMilli\":10000,\"maxMilli\":" +
                                              std::to_string(maxMilli) + extra + "}"};
}

std::vector<ContentFile> SixAttributes(const std::string& bodyExtra = "", int bodyMax = 100000)
{
    return {AttributeFile("body", bodyExtra, bodyMax), AttributeFile("agility"), AttributeFile("intelligence"),
            AttributeFile("perception"), AttributeFile("willpower"), AttributeFile("charisma")};
}

// The oracle: CHARACTER_CREATION.md "Canonical trait catalogue", transcribed independently of the data files.
// Each row is (trait, skill, XP at ranks 1..4). Every package is +10 / +25 / +45 / +70.
struct DocSeed
{
    const char* Trait;
    const char* Skill;
};
constexpr DocSeed DocSeeds[] = {
    {"technical_aptitude", "skill.computing"},     {"technical_aptitude", "skill.practical_repair"},
    {"creative_aptitude", "skill.communication"},  {"creative_aptitude", "skill.creative"},
    {"athletic_aptitude", "skill.fitness"},        {"academic_foundation", "skill.academics"},
    {"practical_experience", "skill.practical_repair"}, {"practical_experience", "skill.household_tasks"},
};
constexpr std::int32_t DocXpAtRank[5] = {0, 10, 25, 45, 70};
/** The common Academics baseline for every stream: a placeholder in the data, equal for all characters. */
constexpr std::int32_t AcademicsBaselineXp = 100;
constexpr std::int32_t AttributeStartMilli = 10000;
const char* const AllSkills[] = {"skill.academics", "skill.commerce", "skill.communication", "skill.computing", "skill.creative",
                                 "skill.fitness", "skill.household_tasks", "skill.logistics", "skill.practical_repair",
                                 "skill.self_management"};

std::int32_t TraitIndex(const std::string& id)
{
    const auto& traits = CharacterCreator::TraitDefinitions();
    for (std::size_t i = 0; i < traits.size(); ++i)
    {
        if (traits[i].Id == id)
            return static_cast<std::int32_t>(i);
    }
    return -1;
}

std::int32_t ExpectedMilli(const std::string& skill, const TraitRankArray& ranks)
{
    std::int32_t xp = skill == "skill.academics" ? AcademicsBaselineXp : 0;
    for (const DocSeed& seed : DocSeeds)
    {
        if (skill == seed.Skill)
            xp += DocXpAtRank[ranks[static_cast<std::size_t>(TraitIndex(seed.Trait))]];
    }
    return xp * MilliPerXp;
}

void CheckAgainstDocs(const LatentStats& stats, const TraitRankArray& ranks, const char* message)
{
    bool same = stats.Skills.size() == std::size(AllSkills);
    for (const char* skill : AllSkills)
    {
        const SkillLevel* level = stats.FindSkill(skill);
        same = same && level != nullptr && level->LevelMilli == ExpectedMilli(skill, ranks);
    }
    for (std::size_t i = 0; i < AttributeCount; ++i)
        same = same && stats.Attributes[i] == AttributeStartMilli;
    Check(same, message);
}
} // namespace

int main(int argc, char** argv)
{
    Check(argc > 1, "usage: PerfectWorldLatentStatsTests <Data/Content>");
    const ContentLoadResult real = LoadContent(ReadContentDirectory(argv[1]));
    for (const ContentIssue& issue : real.Issues)
        std::cerr << issue.File << ": " << issue.Message << '\n';
    Check(real.Ok(), "the real content passes the validator, fairness rule included");
    const ContentDb& db = real.Db;

    // The catalogue: six attributes and ten skills, as data.
    {
        Check(db.CountOfType("attribute") == 6 && db.CountOfType("skill") == 10, "six attributes and ten skills");
        const char* names[] = {"Body", "Agility", "Intelligence", "Perception", "Willpower", "Charisma"};
        for (std::size_t i = 0; i < AttributeCount; ++i)
        {
            const ContentRecord* record = db.Find(AttributeContentId(static_cast<Attribute>(i)));
            Check(record != nullptr && record->Data.Find("name")->String == names[i], "attributes are in the documented order");
        }
        Check(AttributeContentId(static_cast<Attribute>(99)).empty(), "an attribute outside the enum has no ID");
        for (const char* skill : AllSkills)
            Check(db.Find(skill) != nullptr && db.Find(skill)->Type == "skill", "every documented skill is in the catalogue");
    }

    // Balanced Scholar starts at the documented values (CHARACTER_CREATION.md, Verified presets and the trait catalogue).
    CharacterCreator scholar;
    Check(scholar.ApplyPreset("balanced_scholar").Succeeded(), "the preset applies");
    const CharacterCreationSave scholarSave = scholar.CaptureSave();
    const LatentStatsResult base = ComputeStartingStats(db, scholarSave);
    Check(base.Ok(), "Balanced Scholar computes");
    {
        const LatentStats& s = base.Stats;
        // Academic Foundation R3 = +45 on a common 100: 145. Technical Aptitude R2 = +25 to Computing and Practical Repair;
        // Practical Experience R1 = +10 to Practical Repair and Household Tasks; Creative Aptitude R1 = +10 to
        // Communication and Creative; Athletic Aptitude R0 = nothing.
        Check(s.FindSkill("skill.academics")->LevelMilli == 145000, "Academics 145 XP");
        Check(s.FindSkill("skill.computing")->LevelMilli == 25000, "Computing 25 XP");
        Check(s.FindSkill("skill.practical_repair")->LevelMilli == 35000, "Practical Repair 25 + 10 = 35 XP");
        Check(s.FindSkill("skill.household_tasks")->LevelMilli == 10000, "Household Tasks 10 XP");
        Check(s.FindSkill("skill.communication")->LevelMilli == 10000, "Communication 10 XP");
        Check(s.FindSkill("skill.creative")->LevelMilli == 10000, "Creative 10 XP");
        for (const char* untouched : {"skill.fitness", "skill.commerce", "skill.logistics", "skill.self_management"})
            Check(s.FindSkill(untouched)->LevelMilli == 0, "an unseeded skill starts at zero");
        for (std::size_t i = 0; i < AttributeCount; ++i)
            Check(s.Attributes[i] == 10000, "every attribute starts at 10000");
        Check(s.AttributeMilli(Attribute::Charisma) == 10000, "AttributeMilli reads the array");
        Check(std::is_sorted(s.Skills.begin(), s.Skills.end(), [](const SkillLevel& a, const SkillLevel& b) { return a.SkillId < b.SkillId; }),
              "skills are sorted by ID");
        Check(s.FindSkill("skill.nonexistent") == nullptr, "an unknown skill is absent");
    }

    // Determinism, and the rank array alone is the input.
    {
        Check(ComputeStartingStats(db, scholarSave).Stats == base.Stats, "the same input gives the same stats");
        CharacterCreator restored;
        Check(restored.RestoreSave(scholarSave).Succeeded(), "the save restores");
        Check(ComputeStartingStats(db, restored.CaptureSave()).Stats == base.Stats, "a save/restore round trip changes nothing");
        Check(ComputeStartingStats(db, scholarSave.TraitRanks).Stats == base.Stats, "the rank array overload agrees");
    }

    // Stream, appearance, identity and household variants are identical.
    {
        int variants = 0;
        for (const SchoolStreamDefinition& stream : SchoolStreams())
        {
            for (const HouseholdDefinition& household : CharacterCreator::HouseholdDefinitions())
            {
                for (std::size_t look = 0; look < FaceShapeCount; ++look)
                {
                    CharacterCreator c;
                    Check(c.ApplyPreset("balanced_scholar").Succeeded(), "preset");
                    Check(c.SetSchoolStream(stream.Id, "a reason").Succeeded(), "any stream is accepted");
                    Check(c.SetHouseholdTemplate(household.Id).Succeeded(), "any household is accepted");
                    AppearanceRecord appearance = AppearanceRecord::Default();
                    appearance.FaceShapeId = std::string(FaceShapes()[look].Id);
                    appearance.HairStyleId = std::string(HairStyles()[look % HairStyleCount].Id);
                    appearance.BuildId = std::string(Builds()[look % BuildCount].Id);
                    appearance.UniformVariantId = std::string(UniformVariants()[look % UniformVariantCount].Id);
                    appearance.SkinTone = {static_cast<std::int32_t>(40 + look * 50), 90, 60};
                    Check(c.SetAppearance(appearance).Succeeded(), "appearance is free");
                    IdentityProfile identity;
                    identity.GivenName = "Ananya";
                    identity.FamilyName = look % 2 == 0 ? "Sen" : "Rahman";
                    identity.Pronouns = look % 2 == 0 ? "she/her" : "they/them";
                    identity.HomeCity = "Kolkata";
                    identity.PrimaryLanguage = look % 2 == 0 ? "Bengali" : "Urdu";
                    identity.AdditionalLanguages = look % 2 == 0 ? std::vector<std::string>{"English", "Hindi"} : std::vector<std::string>{};
                    identity.Birth = look % 2 == 0 ? EarliestDateOfBirth : LatestDateOfBirth;
                    Check(c.SetIdentity(identity).Succeeded(), "identity is free");
                    Check(ComputeStartingStats(db, c.CaptureSave()).Stats == base.Stats,
                          "stream, household, appearance and identity never change a starting value");
                    ++variants;
                }
            }
        }
        Check(variants == 3 * 5 * 4, "all sixty variants ran");
    }

    // Traits without a documented skill package change nothing, at any rank; every package matches the docs.
    {
        TraitRankArray zero{};
        const LatentStats zeroStats = ComputeStartingStats(db, zero).Stats;
        CheckAgainstDocs(zeroStats, zero, "all-zero ranks give the baseline");
        for (std::size_t trait = 0; trait < TraitCount; ++trait)
        {
            for (std::int32_t rank = 0; rank <= MaxRank; ++rank)
            {
                TraitRankArray ranks{};
                ranks[trait] = rank;
                const LatentStatsResult result = ComputeStartingStats(db, ranks);
                Check(result.Ok(), "a single trait computes");
                CheckAgainstDocs(result.Stats, ranks, "one trait at one rank matches CHARACTER_CREATION.md");
                const std::string id(CharacterCreator::TraitDefinitions()[trait].Id);
                const bool seeds = std::any_of(std::begin(DocSeeds), std::end(DocSeeds), [&](const DocSeed& s) { return id == s.Trait; });
                if (!seeds)
                    Check(result.Stats == zeroStats, "a trait with no documented skill package changes no starting value");
            }
        }
        Check(TraitIndex("household_resources") >= 0, "Household Resources is a trait");
        // A deterministic spread of rank arrays, including ones over budget: the maths is the table, nothing else.
        std::uint32_t state = 12345;
        for (int round = 0; round < 200; ++round)
        {
            TraitRankArray ranks{};
            for (std::int32_t& rank : ranks)
            {
                state = state * 1664525u + 1013904223u;
                rank = static_cast<std::int32_t>((state >> 16) % 5);
            }
            CheckAgainstDocs(ComputeStartingStats(db, ranks).Stats, ranks, "a mixed allocation matches the documented table");
        }
    }

    // Refusals.
    {
        TraitRankArray ranks = scholarSave.TraitRanks;
        ranks[3] = 5;
        Check(!ComputeStartingStats(db, ranks).Ok(), "a rank above 4 is refused, not coerced");
        ranks[3] = -1;
        Check(!ComputeStartingStats(db, ranks).Ok(), "a negative rank is refused");
        Check(!ComputeStartingStats(ContentDb{}, scholarSave).Ok(), "content with no attributes is refused");
    }

    // Seeds in data drive attributes too; the ceiling holds; the validator checks seed shape.
    {
        const std::string seed = ",\"seeds\":[{\"trait\":\"strength\",\"milliByRank\":[100,200,300,400]}]";
        ContentLoadResult custom = LoadContent(SixAttributes(seed, 10300));
        Check(custom.Ok(), "an attribute seeded by a creation trait is valid");
        TraitRankArray ranks{};
        ranks[static_cast<std::size_t>(TraitIndex("strength"))] = 2;
        Check(ComputeStartingStats(custom.Db, ranks).Stats.AttributeMilli(Attribute::Body) == 10200, "rank 2 adds the second value");
        ranks[static_cast<std::size_t>(TraitIndex("strength"))] = 4;
        Check(ComputeStartingStats(custom.Db, ranks).Stats.AttributeMilli(Attribute::Body) == 10300, "an attribute stops at its ceiling");

        Check(HasIssue(LoadContent(SixAttributes(",\"seeds\":[{\"trait\":\"strength\",\"milliByRank\":[400,300,200,100]}]")),
                       "must not fall as the rank rises"), "a seed that falls with rank is refused");
        Check(HasIssue(LoadContent(SixAttributes(",\"seeds\":[{\"trait\":\"strength\",\"milliByRank\":[1,2,3]}]")), "exactly four values"),
              "a seed needs four values");
        Check(HasIssue(LoadContent({Skill("skill.x", ",\"seeds\":[{\"trait\":\"vitality\",\"xpByRank\":[1,2,3,4]},{\"trait\":\"vitality\",\"xpByRank\":[1,2,3,4]}]")}),
                       "two seeds name the trait"), "a trait seeds a skill once");
        Check(HasIssue(LoadContent(SixAttributes("", 5000)), "startMilli is above maxMilli"), "start may not exceed the ceiling");
        std::vector<ContentFile> seven = SixAttributes();
        seven.push_back(AttributeFile("luck"));
        Check(HasIssue(LoadContent(seven), "not one of the six attributes"), "there are exactly six attributes");
    }

    // The validator refuses any religion, language, stream or family multiplier, however it is spelled.
    {
        for (const char* key : {"religionMultiplier", "languageBonus", "streamModifier", "familyMultiplier", "family_bonus",
                                "StreamXP", "streamXPBonus", "faithBoost", "languagesPenalty", "xp_by_family", "casteFactor"})
        {
            const ContentLoadResult result = LoadContent({Skill("skill.x", std::string(",\"") + key + "\":2")});
            Check(HasIssue(result, "fairness rule"), key);
        }
        Check(HasIssue(LoadContent({Skill("skill.x", ",\"modifiers\":{\"religion\":1100}")}), "keys a modifier on religion"),
              "a modifier table keyed by religion is refused");
        Check(HasIssue(LoadContent({Skill("skill.x", ",\"multipliers\":[{\"on\":\"stream\",\"value\":1100}]")}), "keys a modifier on stream"),
              "a modifier table that names a stream is refused");
        Check(HasIssue(LoadContent({Skill("skill.x", ",\"seeds\":[{\"trait\":\"vitality\",\"xpByRank\":[1,2,3,4],\"bonus\":{\"language\":2}}]")}),
                       "keys a modifier on language"), "a nested modifier is refused");
        Check(HasIssue(LoadContent({{"/stratum/stratum.a.json",
                                     "{\"type\":\"stratum\",\"id\":\"stratum.a\",\"householdResourcesRank\":1,\"name\":\"N\",\"housing\":\"h\","
                                     "\"buffer\":\"small\",\"familyScale\":3}"}}),
                       "keys a modifier on family"), "the rule covers every content type, not only skills");

        // Naming a dimension is not a modifier: the plain word is judged by the schema, not by the fairness rule.
        const ContentLoadResult plain = LoadContent({Skill("skill.x", ",\"streamName\":\"science\"")});
        Check(HasIssue(plain, "unknown field") && !HasIssue(plain, "fairness rule"), "a dimension word alone is not a modifier");
        Check(LoadContent({Skill("skill.x", ",\"note\":\"Languages and streams change nothing here.\"")}).Ok(), "prose in a note is fine");
    }

    std::cout << "PASS: " << Checks << " latent stats checks\n";
    return 0;
}
