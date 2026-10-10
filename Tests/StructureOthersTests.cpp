// ROADMAP M2-09: the explain, investigate and coordinate structures, the three that follow
// "model a system". Each runner is held to the properties of M2-08 (every case can be
// completed; a correct committed answer is never refused for a low stat; costs show before
// commit; a secure family resolves by routine at full normal value), and the four structures
// are shown to differ. The catalogues are the real Data/Content.
#include "StructureOthersTestSupport.h"

using namespace PerfectWorld;

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: PerfectWorldStructureOthersTests <Data/Content>\n";
        return 2;
    }
    const std::vector<ContentFile> files = ReadContentDirectory(argv[1]);
    const ContentLoadResult content = LoadContent(files);
    for (const ContentIssue& issue : content.Issues)
        std::cerr << issue.File << ": " << issue.Message << '\n';
    Check(content.Ok(), "the real content validates");

    OthersCatalogues catalogues;
    const ExplainCatalogueResult explain = LoadExplainCatalogue(content.Db);
    Check(explain.Ok(), explain.Error.c_str());
    const InvestigateCatalogueResult investigate = LoadInvestigateCatalogue(content.Db);
    Check(investigate.Ok(), investigate.Error.c_str());
    const CoordinateCatalogueResult coordinate = LoadCoordinateCatalogue(content.Db);
    Check(coordinate.Ok(), coordinate.Error.c_str());
    const ModelCatalogueResult model = LoadModelCatalogue(content.Db);
    Check(model.Ok(), model.Error.c_str());
    catalogues.Explain = explain.Catalogue;
    catalogues.Investigate = investigate.Catalogue;
    catalogues.Coordinate = coordinate.Catalogue;
    catalogues.Model = model.Catalogue;
    TestCatalogues = &catalogues;

    TestExplain();
    TestInvestigate();
    TestCoordinate();
    TestContentRules(files);
    TestCatalogueRules(files);
    TestDistinct(files);
    std::cout << "StructureOthersTests passed (" << OthersTestChecks << " checks)\n";
    return 0;
}
