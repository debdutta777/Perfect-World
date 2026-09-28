#include "ContentRegistry.h"
#include "Json.h"

#include <cstdlib>
#include <iostream>
#include <string>

using namespace PerfectWorld;

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

namespace
{
bool HasIssue(const ContentLoadResult& result, const std::string& text, const std::string& id = "")
{
    for (const ContentIssue& issue : result.Issues)
    {
        if (issue.Message.find(text) != std::string::npos && (id.empty() || issue.Id == id) && !issue.File.empty())
        {
            return true;
        }
    }
    return false;
}

ContentFile Stratum(const std::string& id, int rank, const std::string& extra = "")
{
    return {"/stratum/" + id + ".json", "{\"type\":\"stratum\",\"id\":\"" + id + "\",\"householdResourcesRank\":" +
                                            std::to_string(rank) + ",\"name\":\"N\",\"housing\":\"h\",\"buffer\":\"small\"" + extra + "}"};
}
} // namespace

int main()
{
    // JSON: strict, exact, and safe on bad input.
    {
        const JsonParseResult ok = ParseJson(R"({"a": [1, -2, 3.5, 1e3, true, false, null], "s": "xé😀\n"})");
        Check(ok.Value.has_value(), "a valid document parses");
        const JsonValue& a = *ok.Value->Find("a");
        Check(a.Array.size() == 7 && a.Array[0].Integer == 1 && a.Array[1].Integer == -2, "integers are exact");
        Check(!a.Array[2].Integer && a.Array[2].Number == 3.5, "a fraction is not an integer");
        Check(!a.Array[3].Integer && a.Array[3].Number == 1000.0, "an exponent is not an integer");
        Check(a.Array[4].Bool && !a.Array[5].Bool && a.Array[6].IsNull(), "literals");
        Check(ok.Value->Find("s")->String == "x\xC3\xA9\xF0\x9F\x98\x80\n", "escapes decode to UTF-8, surrogate pairs included");

        for (const char* bad : {R"({"a":1,})", R"({"a":1 "b":2})", R"([1,2)", R"({"a":01})", R"({"a":"\x"})", R"({"a":"\ud83d"})",
                                R"({} {})", R"({"a":1,"a":2})", "", "{\"a\":\"line\nbreak\"}", R"({"a":.5})", R"({"a":tru})"})
        {
            Check(!ParseJson(bad).Value.has_value(), "malformed JSON is refused");
        }
        Check(ParseJson("{\n\n\"a\": }").Line == 3, "errors report their line");
        std::string deep(100, '[');
        deep += std::string(100, ']');
        Check(!ParseJson(deep).Value.has_value(), "nesting deeper than 64 is refused");
        Check(ParseJson(std::string(60, '[') + std::string(60, ']')).Value.has_value(), "nesting of 60 is fine");
        Check(ParseJson("9223372036854775807").Value->Integer == INT64_MAX, "the largest int64 is exact");
        Check(!ParseJson("9223372036854775808").Value->Integer, "one past int64 is a double, not an integer");
    }

    // The registry: a small valid set loads, and each rule fires.
    {
        const ContentLoadResult good = LoadContent({Stratum("stratum.a", 0), Stratum("stratum.b", 1)});
        Check(good.Ok(), "two valid strata load");
        Check(good.Db.Count() == 2 && good.Db.CountOfType("stratum") == 2, "both are registered");
        Check(good.Db.Find("stratum.a") != nullptr && good.Db.Find("stratum.a")->File == "/stratum/stratum.a.json", "records know their file");

        Check(HasIssue(LoadContent({Stratum("stratum.a", 0), Stratum("stratum.b", 0)}), "share a householdResourcesRank"),
              "semantic rules run on valid shapes");
        Check(HasIssue(LoadContent({Stratum("stratum.a", 0, ",\"mood\":1")}), "unknown field", "stratum.a"), "unknown fields fail");
        Check(HasIssue(LoadContent({Stratum("stratum.a", 9)}), "outside 0..4", "stratum.a"), "ranges are enforced");
        Check(HasIssue(LoadContent({{"/stratum/stratum.a.json", "{\"type\":\"stratum\",\"id\":\"stratum.a\"}"}}), "missing required field"),
              "required fields are enforced");
        Check(HasIssue(LoadContent({{"/stratum/wrong_name.json", Stratum("stratum.a", 0).Text}}), "must be <type>/<id>.json"),
              "the file name must match the ID");
        Check(HasIssue(LoadContent({{"/x/x.json", "{\"type\":\"dragon\",\"id\":\"dragon.x\"}"}}), "unknown content type"),
              "unknown types fail");
        Check(HasIssue(LoadContent({{"/x/x.json", "[]"}}), "one object"), "a file must be one object");

        // A reference must name a record of the right type, not merely an existing ID.
        const ContentFile school = {"/institution/institution.s.json",
                                    R"({"type":"institution","id":"institution.s","name":"S","kind":"guild","city":"stratum.a"})"};
        Check(HasIssue(LoadContent({Stratum("stratum.a", 0), school}), "but needs a city"), "a reference of the wrong type fails");
    }

    // Nested records share the global ID table and know their parent.
    {
        const std::string city = R"({"type":"city","id":"city.t","name":"T","nation":"nation.t","state":"S","role":"R",
            "districts":[{"id":"district.t.a","name":"A","kind":"authored"},{"id":"district.t.a","name":"B","kind":"authored"}],
            "gateSites":[]})";
        const ContentLoadResult result = LoadContent({{"/city/city.t.json", city}});
        Check(HasIssue(result, "duplicate ID"), "two nested records with one ID fail");
        Check(HasIssue(result, "names nation.t, which does not exist"), "dangling references fail, naming the target");
        Check(result.Db.Find("district.t.a") != nullptr && result.Db.Find("district.t.a")->ParentId == "city.t",
              "a nested record knows its city");
    }

    std::cout << "PASS: " << Checks << " content registry checks\n";
    return 0;
}
