#pragma once

#include "SchoolWorkIssuance.h"

namespace PerfectWorld
{
struct SchoolWorkBriefContentResult
{
    SchoolWorkBriefCatalogue Catalogue;
    std::vector<ContentIssue> Issues;
    [[nodiscard]] bool Ok() const noexcept { return Issues.empty(); }
};
/** Strict shape, registered identity, references and calendar/period semantics.
 * Empty content is valid for registry subsets. No partial catalogue on failure. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolWorkBriefContentResult DecodeSchoolWorkBriefContent(const ContentDb& db);
/** Also requires a nonempty catalogue and exactly its referenced translations. */
[[nodiscard]] PERFECTWORLDCORE_API SchoolWorkBriefContentResult LoadSchoolWorkBriefContent(
    const ContentDb& db, const ContentFile& textFile);
namespace ContentSchema
{
struct TypeSchema;
const TypeSchema& SchoolWorkBriefSchema();
void ValidateSchoolWorkBriefSemantics(const ContentDb& db, std::vector<ContentIssue>& issues);
}
} // namespace PerfectWorld
