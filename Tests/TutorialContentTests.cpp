#include "TutorialContent.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <set>

namespace
{
using namespace PerfectWorld;
int R93Assertions = 0;
void R93Expect(bool condition, std::string_view message)
{
    ++R93Assertions;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
ContentFile R93Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    R93Expect(input.good(), "input file opens");
    return {path.generic_string(), std::string(std::istreambuf_iterator<char>(input), {})};
}
std::vector<ContentFile> R93Files(const std::filesystem::path& root)
{
    std::vector<ContentFile> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
        if (entry.is_regular_file() && entry.path().extension() == ".json") files.push_back(R93Read(entry.path()));
    std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.Path < b.Path; });
    return files;
}
void R93Refused(const std::vector<ContentFile>& files, const ContentFile& text,
    std::string_view from, std::string_view to)
{
    auto mutated = files;
    bool found = false;
    for (auto& file : mutated)
        if (file.Path.ends_with("/tutorial_content/tutorial.awakening.json"))
        {
            const auto at = file.Text.find(from);
            R93Expect(at != std::string::npos, "mutation target exists");
            file.Text.replace(at, from.size(), to);
            found = true;
        }
    R93Expect(found, "tutorial file exists");
    const auto invalid = LoadContent(mutated);
    R93Expect(!invalid.Ok(), "registry refuses malformed tutorial");
    const auto decoded = LoadTutorialContent(invalid.Db, text);
    R93Expect(!decoded.Ok() && decoded.Content == TutorialContent{}, "typed loading also refuses atomically");
}
void R93Pins(const ContentDb& db, const TutorialContent& content)
{
    R93Expect(db.CountOfType("tutorial_content") == 1 && db.CountOfType("tutorial_actor") == 5 &&
        db.CountOfType("tutorial_scene") == 3 && db.CountOfType("tutorial_situation") == 3 &&
        db.CountOfType("tutorial_choice") == 12, "one top-level tutorial, 23 nested records");
    R93Expect(content.Id == "tutorial.awakening" && content.SchoolId == "institution.nabapara_vidyalaya", "stable campus tutorial");
    R93Expect(content.Actors.size() == 5 && content.Scenes.size() == 3 && content.Situations.size() == 3 &&
        content.Text.size() == 64, "five cast aliases, three scenes, three situations, 64 text entries");
    constexpr std::array<std::string_view, 3> situations{"crossing", "corridor", "delivery"};
    constexpr std::array<std::array<ConductAction, 4>, 3> actions{{
        {ConductAction::Assist, ConductAction::Observe, ConductAction::Hold, ConductAction::Withdraw},
        {ConductAction::Hold, ConductAction::Improvise, ConductAction::Evade, ConductAction::Withdraw},
        {ConductAction::Assist, ConductAction::Observe, ConductAction::Evade, ConductAction::Withdraw}}};
    std::set<std::string> usedText, usedChoices;
    for (std::size_t index = 0; index < situations.size(); ++index)
    {
        const auto& situation = content.Situations[index];
        R93Expect(situation.Id == "situation.awakening." + std::string(situations[index]) &&
            FindTutorialSituation(content, situation.Id) == &situation && situation.Stakes == 1, "ordered IDs and equal stakes pinned");
        R93Expect(situation.Choices.size() == 4 && situation.ActorIds.size() == 3, "all four choices offered to the same group");
        usedText.insert(situation.PromptKey);
        usedText.insert(situation.RecoveryKey);
        for (std::size_t choiceIndex = 0; choiceIndex < situation.Choices.size(); ++choiceIndex)
        {
            const auto& choice = situation.Choices[choiceIndex];
            R93Expect(choice.Action == actions[index][choiceIndex] &&
                choice.Id == "choice.awakening." + std::string(situations[index]) + "." + std::string(ConductActionName(choice.Action)),
                "R9-02 choice IDs and action mapping pinned");
            R93Expect(FindTutorialChoice(situation, choice.Id) == &choice && usedChoices.insert(choice.Id).second &&
                IsConductTokenFair(situation.Id) && IsConductTokenFair(choice.Id), "distinct fair named evidence");
            usedText.insert(choice.LabelKey); usedText.insert(choice.ActionKey); usedText.insert(choice.ConsequenceKey);
            for (int stakes = ConductMinStakes; stakes <= ConductMaxStakes; ++stakes)
            {
                const auto points = ConductActionEvidence(choice.Action, stakes);
                R93Expect(std::accumulate(points.begin(), points.end(), 0) == ConductActionTotal(stakes),
                    "every tag including withdrawal carries equal evidence at equal stakes");
            }
        }
    }
    for (const auto& actor : content.Actors)
    {
        usedText.insert(actor.NameKey);
        if (actor.Id == "actor.awakening.arup")
            R93Expect(actor.ClassroomId == "coordinate_case.xi_debate_hall" && actor.MemberId == "m_arup" &&
                *FindTutorialText(content, actor.NameKey) == "Arup Ghosh", "existing classroom member retained");
        if (actor.Id == "actor.awakening.mira")
            R93Expect(actor.ScheduleId == "npc_schedule.mira_sen" && *FindTutorialText(content, actor.NameKey) == "Mira Sen",
                "existing scheduled classmate retained");
    }
    for (const auto& id : {content.AnnouncementId, content.FamilyEveningId, content.CareEveningId})
    {
        const auto* scene = FindTutorialScene(content, id);
        R93Expect(scene != nullptr && !scene->Lines.empty(), "each contextual scene is reachable by its reference");
        for (const auto& line : scene->Lines)
        {
            R93Expect(db.Find(line.ActorId) != nullptr, "every speaker registered");
            usedText.insert(line.TextKey);
        }
    }
    R93Expect(usedText.size() == content.Text.size(), "no unused or missing text entries");
    for (const auto& key : usedText) R93Expect(FindTutorialText(content, key) != nullptr, "every text reference resolves");
    R93Expect(FindTutorialSituation(content, "missing") == nullptr && FindTutorialScene(content, "missing") == nullptr &&
        FindTutorialChoice(content.Situations.front(), "missing") == nullptr && FindTutorialText(content, "missing") == nullptr,
        "unknown references have no invented fallback");
}

// Traverse all 27 full routes plus 13 early-withdrawal prefixes. This establishes
// content reachability/ledger compatibility, not R9-02 state-machine completion.
void R93Reach(const TutorialContent& content, std::size_t index, const ConductLedger& before,
    std::set<std::string>& reached, std::size_t& terminals)
{
    if (index == content.Situations.size()) { ++terminals; return; }
    const auto& situation = content.Situations[index];
    for (const auto& choice : situation.Choices)
    {
        auto ledger = before;
        const ConductRecord record{situation.Id, choice.Id, choice.Action, situation.Stakes, static_cast<GameInstant>(index), {}};
        R93Expect(RecordConduct(ledger, record).Ok(), "every offered choice produces accepted named evidence");
        reached.insert(choice.Id);
        const auto once = ledger;
        R93Expect(RecordConduct(ledger, record).Status == ConductStatus::Replay && ledger == once, "duplicate action cannot add evidence");
        R93Expect(ledger.Entries.size() == index + 1, "unplayed stations create no evidence");
        if (choice.Action == ConductAction::Withdraw) ++terminals;
        else R93Reach(content, index + 1, ledger, reached, terminals);
    }
}
void R93TextFailures(const ContentDb& db, const ContentFile& text, const TutorialContent& content)
{
    for (const auto& [key, value] : content.Text)
    {
        (void)value;
        auto missing = text;
        const auto at = missing.Text.find(key);
        R93Expect(at != std::string::npos, "text key exists in source table");
        missing.Text.replace(at, key.size(), "text.awakening.unreferenced");
        const auto result = LoadTutorialContent(db, missing);
        R93Expect(!result.Ok() && result.Content == TutorialContent{} &&
            std::any_of(result.Issues.begin(), result.Issues.end(), [&](const auto& issue) { return issue.Id == key; }),
            "deleting any individual required key is diagnosed atomically");
    }
    for (const auto& bytes : {"[]", "{", "{\"text.awakening.a\":\"x\",\"text.awakening.a\":\"y\"}",
        "{\"text.awakening.a\":3}", "{\"text.awakening.a\":\"   \"}", "{\"wrong.a\":\"x\"}"})
        R93Expect(!LoadTutorialContent(db, {text.Path, bytes}).Ok(), "malformed text table refused");
    auto blank = text;
    const auto name = blank.Text.find("Arup Ghosh");
    blank.Text.replace(name, std::string_view("Arup Ghosh").size(), "   ");
    R93Expect(!LoadTutorialContent(db, blank).Ok(), "blank required translation refused");
}
} // namespace

int main(int argc, char** argv)
{
    R93Expect(argc == 3, "content directory and text file arguments");
    const auto files = R93Files(argv[1]);
    const auto text = R93Read(argv[2]);
    const auto loaded = LoadContent(files);
    for (const auto& issue : loaded.Issues) std::cerr << issue.Id << ": " << issue.Message << '\n';
    R93Expect(loaded.Ok(), "all authored content and references validate");
    const auto result = LoadTutorialContent(loaded.Db, text);
    for (const auto& issue : result.Issues) std::cerr << issue.Id << ": " << issue.Message << '\n';
    R93Expect(result.Ok(), "typed content and complete text table load");
    R93Pins(loaded.Db, result.Content);
    std::set<std::string> reached;
    std::size_t terminals = 0;
    R93Reach(result.Content, 0, {}, reached, terminals);
    R93Expect(reached.size() == 12 && terminals == 40, "all twelve choices reachable over forty terminal content routes");
    R93TextFailures(loaded.Db, text, result.Content);
    auto reversed = files;
    std::reverse(reversed.begin(), reversed.end());
    const auto reordered = LoadContent(reversed);
    R93Expect(reordered.Ok() && LoadTutorialContent(reordered.Db, text).Content == result.Content, "enumeration order cannot alter content");
    R93Expect(!LoadTutorialContent({}, text).Ok(), "missing tutorial refused at typed boundary");
    auto direct = loaded.Db;
    direct.Records.at("tutorial.awakening").Data.Object.push_back({"unknown", JsonValue{}});
    R93Expect(!LoadTutorialContent(direct, text).Ok(), "typed boundary refuses unknown fields");
    direct = loaded.Db;
    direct.Records.at("tutorial.awakening").Data.Object.push_back(direct.Records.at("tutorial.awakening").Data.Object.front());
    R93Expect(!LoadTutorialContent(direct, text).Ok(), "typed boundary refuses duplicate fields");
    direct = loaded.Db;
    direct.Records.erase("actor.awakening.arup");
    R93Expect(!LoadTutorialContent(direct, text).Ok(), "typed boundary refuses missing nested registration");
    R93Refused(files, text, "\"stakes\": 1", "\"stakes\": 0");
    R93Refused(files, text, "\"action\": \"assist\"", "\"action\": \"attack\"");
    R93Refused(files, text, "\"action\": \"withdraw\"", "\"action\": \"improvise\"");
    R93Refused(files, text, "\"action\": \"assist\"", "\"action\": \"observe\"");
    R93Refused(files, text, "\"member\": \"m_arup\"", "\"member\": \"m_missing\"");
    R93Refused(files, text, "\"member\": \"m_arup\"", "\"member\": \"m_you\"");
    R93Refused(files, text, "\"schedule\": \"npc_schedule.mira_sen\"", "\"schedule\": \"npc_schedule.missing\"");
    R93Refused(files, text, "\"schedule\": \"npc_schedule.mira_sen\"", "\"schedule\": \"institution.nabapara_vidyalaya\"");
    R93Refused(files, text, "\"school\": \"institution.nabapara_vidyalaya\"", "\"school\": \"institution.missing\"");
    R93Refused(files, text, "\"actor\": \"actor.awakening.teacher\"", "\"actor\": \"actor.awakening.family\"");
    R93Refused(files, text, "\"announcement\": \"scene.awakening.announcement\"",
        "\"announcement\": \"scene.awakening.evening_family\"");
    R93Refused(files, text, "[\"actor.awakening.teacher\", \"actor.awakening.arup\", \"actor.awakening.mira\"]",
        "[\"actor.awakening.family\", \"actor.awakening.arup\", \"actor.awakening.mira\"]");
    R93Refused(files, text, "choice.awakening.crossing.assist", "choice.awakening.corridor.assist");
    R93Refused(files, text, "choice.awakening.crossing.assist", "choice.awakening.crossing.family");
    R93Refused(files, text, "choice.awakening.crossing.assist", "choice.awakening.crossing.help");
    R93Refused(files, text, "\"action\": \"assist\"", "\"action\": \"improvise\"");
    R93Refused(files, text, "\"stakes\": 1", "\"stakes\": 1, \"timerBonus\": 2");
    std::cout << R93Assertions << " tutorial content assertions passed; " << files.size() << " files, "
        << loaded.Db.Count() << " records, " << KnownContentTypes().size() << " top-level kinds\n";
}
