// pwstatediff: compares two world saves section by section (ROADMAP M3-02; SYSTEMS "Determinism and replay").
//
//   pwstatediff <a.save> <b.save> [--ignore <path>]...
//
// Prints the report of StateDiff.h to standard output: which sections differ and, in each, which fields,
// as "<path>: <a> -> <b>". Exit codes: 0 the saves hold the same persistent state, 1 they differ,
// 2 a file could not be read or is not a loadable save, or the arguments are wrong.
// --ignore leaves a path (and what is below it) out of the comparison, for example "meta.seq".
#include "StateDiff.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

using namespace PerfectWorld;

namespace
{
bool ReadWholeFile(const std::string& path, std::string& text)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return !in.bad();
}

int Usage()
{
    std::cerr << "usage: pwstatediff <a.save> <b.save> [--ignore <path>]...\n"
                 "exit 0: identical, 1: differences, 2: unreadable save or bad arguments\n";
    return 2;
}
} // namespace

int main(int argc, char** argv)
{
    std::vector<std::string> files;
    StateDiffOptions options;
    for (int i = 1; i < argc; ++i)
    {
        const std::string argument = argv[i];
        if (argument == "--ignore")
        {
            if (i + 1 >= argc)
                return Usage();
            options.IgnorePaths.push_back(argv[++i]);
        }
        else
        {
            files.push_back(argument);
        }
    }
    if (files.size() != 2)
        return Usage();

    std::string textA;
    std::string textB;
    if (!ReadWholeFile(files[0], textA))
    {
        std::cerr << "pwstatediff: cannot read " << files[0] << '\n';
        return 2;
    }
    if (!ReadWholeFile(files[1], textB))
    {
        std::cerr << "pwstatediff: cannot read " << files[1] << '\n';
        return 2;
    }
    const SaveTextDiff diff = DiffWorldSaveTexts(textA, textB, options);
    if (!diff.Ok())
    {
        std::cerr << "pwstatediff: " << diff.Error << '\n';
        return 2;
    }
    std::cout << RenderStateDiff(diff.Report);
    return diff.Report.Identical() ? 0 : 1;
}
