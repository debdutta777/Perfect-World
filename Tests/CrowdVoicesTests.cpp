#include "AmbientCrowd.h"
#include "Json.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

using namespace PerfectWorld;

namespace
{
int CvAssertions = 0;
void CvExpect(bool value, const std::string& message)
{
    ++CvAssertions;
    if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
} // namespace

int main(int argc, char** argv)
{
    CvExpect(argc == 2, "usage: PerfectWorldCrowdVoicesTests <Data/Text/en/ambient.crowd.json>");
    std::ifstream input(argv[1], std::ios::binary);
    CvExpect(input.good(), "crowd text file opens");
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    const auto json = ParseJson(text);
    CvExpect(json.Value.has_value() && json.Value->IsObject(), "crowd text is one JSON object");

    std::set<std::string> authored;
    for (const auto& [key, value] : json.Value->Object)
    {
        CvExpect(authored.insert(key).second, "no duplicate key " + key);
        CvExpect(value.IsString() && !value.String.empty() && value.String.size() <= 120, "short non-empty line " + key);
    }
    const auto keys = CrowdExchangeTextKeys();
    CvExpect(keys.size() == 110, "six cohorts x six bands x three lines, plus two direction lines");
    for (const auto& key : keys)
        CvExpect(authored.contains(key), "authored line for " + key);

    // Every key the selector returns is one of the listed keys, and the lane hears variety.
    const std::set<std::string> listed(keys.begin(), keys.end());
    std::set<std::string> heard;
    for (int cohort = 0; cohort < 6; ++cohort)
        for (int band = 0; band < 6; ++band)
            for (std::uint64_t seed = 0; seed < 200; ++seed)
            {
                const auto key = CrowdExchangeTextKey(static_cast<CrowdCohort>(cohort), static_cast<CrowdTimeBand>(band),
                                                      seed * 0x9e3779b97f4a7c15ULL);
                CvExpect(listed.contains(key), "selector returns a listed key: " + key);
                heard.insert(key);
            }
    CvExpect(heard.size() == keys.size(), "every listed line is reachable");
    CvExpect(CrowdExchangeTextKey(CrowdCohort::Vendor, CrowdTimeBand::Evening, 42) ==
                 CrowdExchangeTextKey(CrowdCohort::Vendor, CrowdTimeBand::Evening, 42), "same seed, same line");
    for (std::uint64_t seed = 0; seed < 200; ++seed)
        CvExpect(!CrowdExchangeTextKey(CrowdCohort::Elder, CrowdTimeBand::Night, seed).starts_with(
                     "ambient.exchange.directions"), "nobody gives directions at night");

    std::cout << "PerfectWorldCrowdVoicesTests: " << CvAssertions << " assertions passed\n";
    return 0;
}
