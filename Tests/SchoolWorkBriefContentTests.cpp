#include "SchoolWorkBriefContent.h"

#include "P2CTestSupport.h"

namespace
{
PerfectWorld::ContentFile P34ContentText(const char* path)
{
    std::ifstream input(path, std::ios::binary);
    P2CExpect(input.good(), "P34 text readable");
    std::ostringstream bytes;
    bytes << input.rdbuf();
    return {path, bytes.str()};
}
void P34ContentField(PerfectWorld::JsonValue& data, std::string_view field, std::string_view json)
{
    const auto parsed = PerfectWorld::ParseJson(json);
    P2CExpect(parsed.Value.has_value(), "P34 mutation JSON");
    for (auto& [name, value] : data.Object)
        if (name == field) { value = *parsed.Value; return; }
    P2CExpect(false, "P34 mutation field exists");
}
} // namespace

int main(int argc, char** argv)
{
    using namespace PerfectWorld;
    P2CExpect(argc == 3, "P34 content and text paths");
    const P2CFixture f(argv[1]);
    const auto text = P34ContentText(argv[2]);
    const auto loaded = LoadSchoolWorkBriefContent(f.Content, text);
    for (const auto& issue : loaded.Issues) std::cerr << issue.Id << ": " << issue.Message << '\n';
    P2CExpect(loaded.Ok() && loaded.Catalogue.Briefs.size() == 27, "the nine first-term briefs and the eighteen later ones (C3) load with complete translations");
    const auto types = KnownContentTypes();
    P2CExpect(std::find(types.begin(), types.end(), "school_work_brief") != types.end(), "content kind registered");
    std::string previous;
    for (const auto& def : loaded.Catalogue.Briefs)
    {
        P2CExpect(def.Brief.Id > previous, "stable catalogue ordering");
        previous = def.Brief.Id;
        const auto cats = f.Cats(def.Stream);
        P2CExpect(ValidateSchoolWorkBriefDef(def, *cats.Day.Calendar, *cats.Day.School, f.Papers).Ok(), "authoritative period and deadline");
        if (def.TermId != "term_1") continue; // C3 later-term briefs follow their own subject periods; the starter-set rules below are term 1's
        P2CExpect(def.Subject == "english" && def.Period == 3, "honest common-period starter set");
        P2CExpect(def.Brief.PaperId == "assessment_paper.xi_half_yearly_mixed", "only existing paper reused");
        P2CExpect(loaded.Catalogue.Text.at(def.Brief.BriefTextKey).find("same five authored cases") != std::string::npos,
            "paper reuse disclosed");
        P2CExpect(loaded.Catalogue.Text.at(def.Brief.BriefTextKey).find(FormatCalendarDate(CalendarDayOfInstant(def.Brief.DueAt))) != std::string::npos,
            "deadline date disclosed in words as well as authoritative fields");
    }
    for (const auto stream : {"science", "commerce", "humanities"})
        P2CExpect(std::count_if(loaded.Catalogue.Briefs.begin(), loaded.Catalogue.Briefs.end(),
            [&](const auto& d) { return d.Stream == stream && d.TermId == "term_1"; }) == 3, "three first-term briefs per stream");
    const auto id = loaded.Catalogue.Briefs.front().Brief.Id;
    const auto malformed = [&](std::string_view field, std::string_view value) {
        auto db = f.Content;
        P34ContentField(db.Records.at(id).Data, field, value);
        const auto result = DecodeSchoolWorkBriefContent(db);
        P2CExpect(!result.Ok() && result.Catalogue.Briefs.empty(), "malformed content gives no partial catalogue");
    };
    malformed("period", "8");
    malformed("period", "-1");
    malformed("period", "\"3\"");
    malformed("subject", "\"physics\"");
    malformed("stream", "\"science\""); // first sorted ID is commerce
    malformed("term", "\"term_2\"");
    malformed("calendar", "\"calendar.missing\"");
    malformed("bellSchedule", "\"assessment_paper.xi_half_yearly_mixed\"");
    malformed("paper", "\"assessment_paper.missing\"");
    malformed("paper", "\"calendar.nabapara_2042_43\"");
    malformed("issueDate", "\"2042-02-30\"");
    malformed("issueDate", "\"2042-06-21\""); // weekend
    malformed("issueDate", "\"2042-08-15\""); // holiday
    malformed("issueDate", "\"2042-07-28\""); // assessment
    malformed("issueDate", "\"2042-12-05\""); // sports / wrong term
    malformed("issueDate", "\"2042-10-20\""); // vacation
    malformed("issueDate", "\"2043-01-26\""); // preparation break
    malformed("dueDate", "\"2042-06-01\"");
    malformed("dueDate", "\"2042-11-03\""); // outside issue term
    malformed("dueMinute", "1440");
    malformed("titleKey", "\"bad key\"");
    malformed("recoveryKey", "\"phase2.wrong.key\"");
    malformed("id", "\"school.work.renamed\"");
    malformed("type", "\"school_lesson\"");
    {
        auto db = f.Content;
        auto& data = db.Records.at(id).Data;
        data.Object.emplace_back("surprise", JsonValue{});
        P2CExpect(!DecodeSchoolWorkBriefContent(db).Ok(), "unknown field refused");
        data.Object.pop_back();
        data.Object.push_back(data.Object.front());
        P2CExpect(!DecodeSchoolWorkBriefContent(db).Ok(), "duplicate field refused in directly constructed DB");
        data.Object.pop_back();
        data.Object.erase(std::remove_if(data.Object.begin(), data.Object.end(), [](const auto& p) { return p.first == "paper"; }), data.Object.end());
        P2CExpect(!DecodeSchoolWorkBriefContent(db).Ok(), "missing field refused");
    }
    {
        auto db = f.Content;
        db.Records.at(id).ParentId = "school.work.parent";
        P2CExpect(!DecodeSchoolWorkBriefContent(db).Ok(), "nested identity refused");
        db.Records.at(id).ParentId.clear();
        db.Records.emplace("school.work.alias", db.Records.at(id));
        P2CExpect(!DecodeSchoolWorkBriefContent(db).Ok(), "aliased registry identity refused");
    }
    P2CExpect(DecodeSchoolWorkBriefContent({}).Ok(), "empty registry subset is legal");
    P2CExpect(!LoadSchoolWorkBriefContent({}, text).Ok(), "runtime refuses empty catalogue");
    P2CExpect(!LoadSchoolWorkBriefContent(f.Content, {"missing.json", "{}"}).Ok(), "missing translations refused");
    P2CExpect(!LoadSchoolWorkBriefContent(f.Content, {"broken.json", "{"}).Ok(), "malformed text refused");
    auto words = ParseJson(text.Text);
    P2CExpect(words.Value && words.Value->IsObject(), "text object");
    const auto& word = words.Value->Object.front();
    const auto key = "\"" + word.first + "\"";
    auto extra = text;
    extra.Text.insert(extra.Text.rfind('}'), ",\"phase3.p304.unused\":\"Unused\"");
    P2CExpect(!LoadSchoolWorkBriefContent(f.Content, extra).Ok(), "unreferenced translation refused");
    P2CExpect(!LoadSchoolWorkBriefContent(f.Content, {"duplicate.json", "{" + key + ":\"A\"," + key + ":\"B\"}"}).Ok(), "duplicate translation refused");
    auto blank = text;
    const auto start = blank.Text.find(':', blank.Text.find(key));
    const auto firstQuote = blank.Text.find('"', start + 1);
    const auto lastQuote = blank.Text.find('"', firstQuote + 1);
    blank.Text.replace(firstQuote, lastQuote - firstQuote + 1, "\"  \"");
    P2CExpect(!LoadSchoolWorkBriefContent(f.Content, blank).Ok(), "blank translation refused");
    P2CExpect(LoadSchoolWorkBriefContent(f.Content, {"bom.json", "\xEF\xBB\xBF" + text.Text}).Ok(), "UTF-8 BOM accepted");
    std::cout << "P34 SchoolWorkBriefContent passed\n";
}
