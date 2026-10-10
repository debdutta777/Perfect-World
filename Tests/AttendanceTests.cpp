// ROADMAP M2-07: the Nabapara Vidyalaya bell schedules, their content rules and the M2-06 routine they
// generate. Usage: PerfectWorldAttendanceTests <path to Data/Content>
#include "AttendanceTestSupport.h"

#include "Schedules.h"

#include <map>
#include <string>

using namespace PerfectWorld;

int AttendanceChecks = 0;

namespace
{
std::vector<ContentFile> Files;
BellScheduleSet Real;

constexpr const char* Science = "science";
constexpr const char* Commerce = "commerce";
constexpr const char* Humanities = "humanities";
constexpr const char* School = "institution.nabapara_vidyalaya";

bool HasIssue(const ContentLoadResult& result, const std::string& text)
{
    return std::any_of(result.Issues.begin(), result.Issues.end(),
                       [&](const ContentIssue& issue) { return issue.Message.find(text) != std::string::npos; });
}

/** The real content with one replacement made in the one file whose path contains `fragment`. */
ContentLoadResult LoadMutated(const std::string& fragment, const std::string& from, const std::string& to)
{
    std::vector<ContentFile> copy = Files;
    bool found = false;
    for (ContentFile& file : copy)
    {
        if (file.Path.find(fragment) == std::string::npos)
            continue;
        const std::size_t at = file.Text.find(from);
        Check(at != std::string::npos, "the mutation target text exists in the file");
        if (at != std::string::npos)
            file.Text.replace(at, from.size(), to);
        found = true;
    }
    Check(found, "the mutated file exists");
    return LoadContent(copy);
}

void ExpectRefused(const std::string& fragment, const std::string& from, const std::string& to, const char* issue)
{
    const ContentLoadResult result = LoadMutated(fragment, from, to);
    if (!HasIssue(result, issue))
    {
        for (const ContentIssue& found : result.Issues)
            std::cerr << "  issue: " << found.Message << '\n';
    }
    Check(HasIssue(result, issue), issue);
    Check(!result.Ok(), "a mutated record is not valid content");
}

void ExpectScienceRefused(const std::string& from, const std::string& to, const char* issue)
{
    ExpectRefused("bell_schedule.nabapara_xi_science.json", from, to, issue);
}

std::vector<std::string> Subjects(const BellSchedule& schedule)
{
    std::vector<std::string> out;
    for (const BellBlock& block : schedule.Blocks)
    {
        if (block.Kind == BellBlockKind::Period)
            out.push_back(block.Subject);
    }
    return out;
}

void TestContent()
{
    const ContentLoadResult loaded = LoadContent(Files);
    Check(loaded.Ok(), "Data/Content validates with the bell schedules");
    Check(loaded.Db.CountOfType("bell_schedule") == 3, "three bell schedules, one per stream");
    const BellScheduleResult result = LoadBellSchedules(loaded.Db);
    Check(result.Ok() && result.Set.Schedules.size() == 3, "the loader reads all three");
    Real = result.Set;
    Check(std::is_sorted(Real.Schedules.begin(), Real.Schedules.end(), [](const BellSchedule& a, const BellSchedule& b) { return a.Id < b.Id; }),
          "schedules come out sorted by ID");
    for (const char* stream : {Science, Commerce, Humanities})
    {
        const BellSchedule* schedule = Real.Find(School, stream);
        Check(schedule != nullptr && schedule->Stream == stream, "each stream has a schedule at Nabapara Vidyalaya");
        Check(schedule != nullptr && ValidateBellSchedule(*schedule).empty(), "each real schedule has no problems");
        Check(Real.FindById(schedule->Id) == schedule, "FindById agrees with Find");
    }
    Check(Real.Find(School, "arts") == nullptr && Real.Find("institution.nga", Science) == nullptr, "no schedule for an unknown stream or school");
    Check(Real.FindById("bell_schedule.nowhere") == nullptr, "no schedule for an unknown ID");
    Check(AttendanceIsStream("science") && !AttendanceIsStream("arts") && !AttendanceIsStream(""), "the three streams");
    Check(AttendanceSubjectTaught(Science, "physics") && !AttendanceSubjectTaught(Science, "accountancy") &&
              AttendanceSubjectTaught(Commerce, "accountancy") && AttendanceSubjectTaught(Humanities, "history") &&
              AttendanceSubjectTaught(Humanities, "english") && !AttendanceSubjectTaught("arts", "english"),
          "subjects belong to streams; english and bengali to all");
}

void TestStreamsDiffer()
{
    const BellSchedule& sci = *Real.Find(School, Science);
    const BellSchedule& com = *Real.Find(School, Commerce);
    const BellSchedule& hum = *Real.Find(School, Humanities);

    // One school, one set of bells: the same frame for every stream.
    Check(sci.Bells == com.Bells && com.Bells == hum.Bells, "all streams ring the same bells");
    Check(sci.GatesOpenMinute == 570 && BellFirstBell(sci) == 600 && sci.Blocks.back().EndMinute == 950, "gate 09:30, bell 10:00, dismissal 15:50");
    for (const BellSchedule* schedule : {&sci, &com, &hum})
    {
        Check(schedule->Blocks.size() == 8 && BellPeriodCount(*schedule) == 6, "assembly, six periods and tiffin");
        Check(schedule->Blocks[0].Kind == BellBlockKind::Assembly && schedule->Blocks[0].EndMinute - schedule->Blocks[0].StartMinute == 15,
              "a 15-minute assembly");
        Check(schedule->Blocks[4].Kind == BellBlockKind::Break && schedule->Blocks[4].EndMinute - schedule->Blocks[4].StartMinute == 30,
              "a 30-minute tiffin break after the third period");
        for (const BellBlock& block : schedule->Blocks)
        {
            if (block.Kind == BellBlockKind::Period)
                Check(block.EndMinute - block.StartMinute == 45, "periods are 45 minutes, as activity.attend_class");
        }
    }
    for (std::size_t i = 0; i < sci.Blocks.size(); ++i)
    {
        Check(sci.Blocks[i].Kind == com.Blocks[i].Kind && com.Blocks[i].Kind == hum.Blocks[i].Kind &&
                  sci.Blocks[i].StartMinute == com.Blocks[i].StartMinute && com.Blocks[i].StartMinute == hum.Blocks[i].StartMinute &&
                  sci.Blocks[i].EndMinute == com.Blocks[i].EndMinute && com.Blocks[i].EndMinute == hum.Blocks[i].EndMinute,
              "the frame (kinds and times) is identical in every stream");
    }

    // They differ as documented: subjects, home room, and the science lab.
    const std::vector<std::string> expectedScience{"mathematics", "physics", "chemistry", "english", "physics", "bengali"};
    const std::vector<std::string> expectedCommerce{"accountancy", "business_studies", "economics", "english", "mathematics", "bengali"};
    const std::vector<std::string> expectedHumanities{"history", "geography", "political_science", "english", "sociology", "bengali"};
    Check(Subjects(sci) == expectedScience && Subjects(com) == expectedCommerce && Subjects(hum) == expectedHumanities,
          "each stream's six subjects, in order");
    Check(Subjects(sci) != Subjects(com) && Subjects(com) != Subjects(hum) && Subjects(sci) != Subjects(hum), "no two streams share a day");
    Check(sci.Blocks[1].Room == "nhss.room_xi_science" && com.Blocks[1].Room == "nhss.room_xi_commerce" &&
              hum.Blocks[1].Room == "nhss.room_xi_humanities",
          "each stream has its own home room");
    Check(sci.Blocks[6].Room == "nhss.science_lab" && sci.Blocks[6].Subject == "physics", "science alone has a lab practical in the lab");
    for (const BellSchedule* schedule : {&com, &hum})
    {
        for (const BellBlock& block : schedule->Blocks)
            Check(block.Room != "nhss.science_lab", "no other stream uses the science lab");
    }
    Check(sci.Blocks[0].Room == "nhss.court" && sci.Blocks[4].Room == "nhss.court", "assembly and tiffin share the court in every stream");
    for (const BellSchedule* schedule : {&sci, &com, &hum})
    {
        std::int32_t english = 0;
        std::int32_t bengali = 0;
        for (const BellBlock& block : schedule->Blocks)
        {
            english += block.Subject == "english" ? 1 : 0;
            bengali += block.Subject == "bengali" ? 1 : 0;
        }
        Check(english == 1 && bengali == 1, "every stream has english and bengali");
    }
}

void TestContentRules()
{
    ExpectScienceRefused("\"start\": 670, \"end\": 715", "\"start\": 660, \"end\": 715", "overlaps block 2");
    ExpectScienceRefused("\"start\": 670, \"end\": 715", "\"start\": 610, \"end\": 715", "the blocks are out of order");
    ExpectScienceRefused("\"start\": 620, \"end\": 665", "\"start\": 640, \"end\": 665", "passing limit");
    ExpectScienceRefused("\"start\": 855, \"end\": 900", "\"start\": 851, \"end\": 900", "needed to walk");
    ExpectScienceRefused("\"stream\": \"science\"", "\"stream\": \"arts\"", "has an unknown value");
    ExpectScienceRefused("\"subject\": \"chemistry\"", "\"subject\": \"accountancy\"", "which is not taught in the science stream");
    ExpectScienceRefused(", \"subject\": \"chemistry\"", "", "is a period and has no subject");
    ExpectScienceRefused("\"end\": 615, \"room\": \"nhss.court\"", "\"end\": 615, \"room\": \"nhss.court\", \"subject\": \"english\"",
                         "is an assembly and has a subject");
    ExpectScienceRefused("\"end\": 800, \"room\": \"nhss.court\"", "\"end\": 800, \"room\": \"nhss.court\", \"subject\": \"english\"",
                         "is a break and has a subject");
    ExpectScienceRefused("\"subject\": \"english\"", "\"subject\": \"biology\"", "has no english period");
    ExpectScienceRefused("\"subject\": \"bengali\"", "\"subject\": \"biology\"", "has no bengali period");
    ExpectScienceRefused("{ \"kind\": \"period\", \"start\": 905,", "{ \"kind\": \"break\", \"start\": 905,", "the day must end with a period");
    ExpectScienceRefused("{ \"kind\": \"period\", \"start\": 620,", "{ \"kind\": \"assembly\", \"start\": 620,", "needs exactly one assembly");
    ExpectScienceRefused("{ \"kind\": \"period\", \"start\": 670,", "{ \"kind\": \"break\", \"start\": 670,", "not a full day");
    ExpectScienceRefused("{ \"kind\": \"assembly\", \"start\": 600,", "{ \"kind\": \"break\", \"start\": 600,", "the day must open with the assembly");
    ExpectScienceRefused("\"gatesOpen\": 570", "\"gatesOpen\": 530", "the gate opens at 08:50");
    ExpectScienceRefused("\"gatesOpen\": 570", "\"gatesOpen\": 600", "the gate opens at 10:00");
    ExpectScienceRefused("[600, 615, 620,", "[600, 615, 621,", "where no block starts or ends");
    ExpectScienceRefused("[600, 615, 620,", "[600, 615, 621,", "no bell rings at 10:20");
    ExpectScienceRefused("[600, 615, 620, 665,", "[600, 615, 665,", "no bell rings at 10:20");
    ExpectScienceRefused("[600, 615, 620, 665", "[600, 620, 615, 665", "the bells are not in ascending order");
    ExpectScienceRefused("[600, 615, 620, 665", "[600, 615, 615, 665", "the bells are not in ascending order");
    ExpectScienceRefused("\"start\": 770, \"end\": 800", "\"start\": 770, \"end\": 785", "needs a break of at least 20 minutes");
    ExpectScienceRefused("\"start\": 905, \"end\": 950", "\"start\": 905, \"end\": 990", "outside 30..60");
    ExpectScienceRefused("\"start\": 600, \"end\": 615", "\"start\": 600, \"end\": 605", "outside 10..30");
    ExpectScienceRefused("\"start\": 600, \"end\": 615", "\"start\": 530, \"end\": 615", "before the school hours begin");
    ExpectScienceRefused("\"room\": \"nhss.court\"", "\"room\": \"Court Yard\"", "needs a lowercase-token room");
    ExpectScienceRefused("\"gatesOpen\": 570,", "", "missing required field \"gatesOpen\"");
    ExpectScienceRefused("\"start\": 620, \"end\": 665", "\"start\": 620, \"end\": 665, \"colour\": 1", "unknown field");
    ExpectScienceRefused("\"start\": 620", "\"start\": 1440", "outside 0..1439");
    ExpectScienceRefused("\"stream\": \"science\",", "\"stream\": \"science\", \"streamBonus\": 5,", "keys a modifier on stream");
    ExpectScienceRefused("\"institution\": \"institution.nabapara_vidyalaya\"", "\"institution\": \"institution.nga\"", "is not a school");
    ExpectScienceRefused("\"stage\": \"stage.higher_secondary\"", "\"stage\": \"stage.secondary\"", "does not offer stage.secondary");
    ExpectScienceRefused("\"institution\": \"institution.nabapara_vidyalaya\"", "\"institution\": \"institution.nowhere\"", "which does not exist");
    ExpectScienceRefused("\"institution\": \"institution.nabapara_vidyalaya\"", "\"institution\": \"city.kolkata\"", "but needs a institution");
    ExpectRefused("stage.higher_secondary.json", "\"humanities\"", "\"arts\"", "has no humanities stream");
    ExpectRefused("bell_schedule.nabapara_xi_humanities.json", "\"stream\": \"humanities\"", "\"stream\": \"commerce\"",
                  "a second commerce schedule");
    ExpectRefused("bell_schedule.nabapara_xi_humanities.json", "\"stream\": \"humanities\"", "\"stream\": \"commerce\"",
                  "has no schedule for the humanities stream");
    ExpectRefused("bell_schedule.nabapara_xi_humanities.json", "\"bells\": [600,", "\"bells\": [601,", "one school rings one set of bells");

    // A whole stream missing from the content.
    std::vector<ContentFile> without;
    for (const ContentFile& file : Files)
    {
        if (file.Path.find("bell_schedule.nabapara_xi_commerce.json") == std::string::npos)
            without.push_back(file);
    }
    Check(without.size() + 1 == Files.size(), "one file removed");
    Check(HasIssue(LoadContent(without), "has no schedule for the commerce stream"), "a stream without a day is refused");

    // The loader refuses what the validator refuses.
    const ContentLoadResult bad = LoadMutated("bell_schedule.nabapara_xi_science.json", "\"start\": 670, \"end\": 715", "\"start\": 660, \"end\": 715");
    const BellScheduleResult refused = LoadBellSchedules(bad.Db);
    Check(!refused.Ok() && refused.Set.Schedules.empty() && refused.Error.find("bell_schedule.nabapara_xi_science") != std::string::npos,
          "the loader names the bad record and returns nothing");

    // ValidateBellSchedule on hand-built values.
    BellSchedule schedule = *Real.Find(School, Science);
    schedule.Blocks.clear();
    Check(!ValidateBellSchedule(schedule).empty(), "an empty day is refused");
    schedule = *Real.Find(School, Science);
    schedule.Blocks[2].StartMinute = schedule.Blocks[2].EndMinute;
    Check(!ValidateBellSchedule(schedule).empty(), "a block of no length is refused");
    schedule = *Real.Find(School, Science);
    schedule.Blocks.back().EndMinute = 1441;
    Check(!ValidateBellSchedule(schedule).empty(), "a block past midnight is refused");
    schedule = *Real.Find(School, Science);
    schedule.Stream = "arts";
    Check(!ValidateBellSchedule(schedule).empty(), "an unknown stream is refused");
    schedule = *Real.Find(School, Science);
    for (BellBlock& block : schedule.Blocks)
        block.StartMinute += 230, block.EndMinute += 230;
    for (std::int32_t& bell : schedule.Bells)
        bell += 230;
    schedule.GatesOpenMinute += 230;
    Check(ValidateBellSchedule(schedule).size() == 1 && ValidateBellSchedule(schedule)[0].find("after the school hours end") != std::string::npos,
          "a day shifted to end at 19:40 breaks only the hours rule");
}

} // namespace

const BellScheduleSet& RealSchedules()
{
    return Real;
}

const std::vector<ContentFile>& RealContentFiles()
{
    return Files;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldAttendanceTests <path to Data/Content>\n";
        return 2;
    }
    Files = ReadContentDirectory(argv[1]);
    Check(!Files.empty(), "content files were found");
    TestContent();
    TestStreamsDiffer();
    TestContentRules();
    TestContentAndRoutine();
    TestLateness();
    TestRecord();
    TestText();
    TestAttendanceCalendar();
    std::cout << "AttendanceTests passed (" << AttendanceChecks << " checks)\n";
    return 0;
}
