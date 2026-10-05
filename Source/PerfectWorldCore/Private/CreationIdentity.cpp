#include "CreationIdentity.h"

#include "CharacterCreation.h"
#include "GameClock.h"

#include <algorithm>
#include <utility>

namespace PerfectWorld
{
namespace
{
constexpr std::array<SchoolStreamDefinition, SchoolStreamCount> IdentityStreams{{
    {"science", "Science", "A timetable of mathematics, physics, chemistry and biology, with laboratory periods."},
    {"commerce", "Commerce", "A timetable of accountancy, business studies and economics, with a practical bent."},
    {"humanities", "Humanities", "A timetable of history, political science, literature and geography, heavy on discussion."},
}};

bool IdentityTextIsContinuation(unsigned char byte) noexcept
{
    return (byte & 0xC0U) == 0x80U;
}

/**
 * True when the text is well-formed UTF-8 with no control characters. The save
 * writer passes bytes through, so a stray byte would otherwise reach a player's
 * screen or break a conversion in the engine.
 */
bool IdentityTextIsClean(std::string_view text) noexcept
{
    std::size_t index = 0;
    while (index < text.size())
    {
        const unsigned char lead = static_cast<unsigned char>(text[index]);
        std::size_t length = 0;
        unsigned char secondLow = 0x80;
        unsigned char secondHigh = 0xBF;
        if (lead < 0x80)
        {
            if (lead < 0x20 || lead == 0x7F)
            {
                return false;
            }
            ++index;
            continue;
        }
        if (lead >= 0xC2 && lead <= 0xDF)
        {
            length = 2;
            if (lead == 0xC2)
            {
                secondLow = 0xA0; // U+0080 to U+009F are C1 control characters.
            }
        }
        else if (lead >= 0xE0 && lead <= 0xEF)
        {
            length = 3;
            if (lead == 0xE0) secondLow = 0xA0;
            if (lead == 0xED) secondHigh = 0x9F;
        }
        else if (lead >= 0xF0 && lead <= 0xF4)
        {
            length = 4;
            if (lead == 0xF0) secondLow = 0x90;
            if (lead == 0xF4) secondHigh = 0x8F;
        }
        else
        {
            return false;
        }
        if (index + length > text.size())
        {
            return false;
        }
        const unsigned char second = static_cast<unsigned char>(text[index + 1]);
        if (second < secondLow || second > secondHigh)
        {
            return false;
        }
        for (std::size_t offset = 2; offset < length; ++offset)
        {
            if (!IdentityTextIsContinuation(static_cast<unsigned char>(text[index + offset])))
            {
                return false;
            }
        }
        index += length;
    }
    return true;
}

bool IdentityTextEqualsIgnoringAsciiCase(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size())
    {
        return false;
    }
    for (std::size_t index = 0; index < a.size(); ++index)
    {
        const char x = a[index] >= 'A' && a[index] <= 'Z' ? static_cast<char>(a[index] - 'A' + 'a') : a[index];
        const char y = b[index] >= 'A' && b[index] <= 'Z' ? static_cast<char>(b[index] - 'A' + 'a') : b[index];
        if (x != y)
        {
            return false;
        }
    }
    return true;
}

OperationResult IdentityLocked(const char* what)
{
    return OperationResult::Failure(CreationError::CreationAlreadyStarted,
                                    std::string(what) + " is locked after life begins.");
}

} // namespace

const std::array<SchoolStreamDefinition, SchoolStreamCount>& SchoolStreams() noexcept
{
    return IdentityStreams;
}

const SchoolStreamDefinition* FindSchoolStream(std::string_view streamId) noexcept
{
    const auto found = std::find_if(IdentityStreams.begin(), IdentityStreams.end(),
                                    [streamId](const SchoolStreamDefinition& stream) { return stream.Id == streamId; });
    return found == IdentityStreams.end() ? nullptr : &*found;
}

std::int64_t AgeOnCampaignStart(const DateOfBirth& birth) noexcept
{
    std::int64_t years = static_cast<std::int64_t>(CampaignStartYear) - birth.Year;
    const bool birthdayStillToCome =
        birth.Month > CampaignStartMonth || (birth.Month == CampaignStartMonth && birth.Day > CampaignStartDay);
    return birthdayStillToCome ? years - 1 : years;
}

std::string DescribeDateOfBirthProblem(const DateOfBirth& birth)
{
    if (!IsValidCivilDate(birth.Year, birth.Month, birth.Day))
    {
        return "That date does not exist on the calendar.";
    }
    if (AgeOnCampaignStart(birth) != CreationAge)
    {
        return "The story opens on 2 June 2042 and the character is exactly 16 that day, so the date of birth "
               "must be from 3 June 2025 to 2 June 2026.";
    }
    return {};
}

std::string DescribePronounsProblem(std::string_view pronouns)
{
    if (pronouns.size() > MaxPronounsBytes)
    {
        return "Pronouns may be at most " + std::to_string(MaxPronounsBytes) + " bytes long.";
    }
    if (!IdentityTextIsClean(pronouns))
    {
        return "Pronouns must be plain text with no control characters.";
    }
    return {};
}

std::string DescribeLanguagesProblem(std::string_view primary, const std::vector<std::string>& additional)
{
    if (primary.size() > MaxLanguageBytes || !IdentityTextIsClean(primary))
    {
        return "The primary language must be plain text of at most " + std::to_string(MaxLanguageBytes) + " bytes.";
    }
    if (additional.size() > MaxAdditionalLanguages)
    {
        return "At most " + std::to_string(MaxAdditionalLanguages) + " additional languages may be listed.";
    }
    for (std::size_t index = 0; index < additional.size(); ++index)
    {
        const std::string& language = additional[index];
        if (language.empty() || language.size() > MaxLanguageBytes || !IdentityTextIsClean(language))
        {
            return "Each additional language must be plain text of 1 to " + std::to_string(MaxLanguageBytes) +
                   " bytes.";
        }
        const bool repeatsPrimary = !primary.empty() && IdentityTextEqualsIgnoringAsciiCase(language, primary);
        bool repeatsEarlier = false;
        for (std::size_t earlier = 0; earlier < index; ++earlier)
        {
            repeatsEarlier = repeatsEarlier || IdentityTextEqualsIgnoringAsciiCase(language, additional[earlier]);
        }
        if (repeatsPrimary || repeatsEarlier)
        {
            return "A language may be listed only once: " + language;
        }
    }
    return {};
}

std::string DescribeSchoolReasonProblem(std::string_view reason)
{
    if (reason.size() > MaxSchoolReasonBytes)
    {
        return "The reason may be at most " + std::to_string(MaxSchoolReasonBytes) + " bytes long.";
    }
    if (!IdentityTextIsClean(reason))
    {
        return "The reason must be plain text on one line.";
    }
    return {};
}

std::string DescribeIdentityProblem(const IdentityProfile& identity)
{
    if (std::string problem = DescribeDateOfBirthProblem(identity.Birth); !problem.empty())
    {
        return problem;
    }
    if (std::string problem = DescribePronounsProblem(identity.Pronouns); !problem.empty())
    {
        return problem;
    }
    return DescribeLanguagesProblem(identity.PrimaryLanguage, identity.AdditionalLanguages);
}

OperationResult CharacterCreator::SetIdentity(IdentityProfile identity)
{
    if (HasStarted_)
    {
        return IdentityLocked("Identity");
    }
    if (std::string problem = DescribeDateOfBirthProblem(identity.Birth); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidDateOfBirth, std::move(problem));
    }
    if (std::string problem = DescribeIdentityProblem(identity); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidIdentity, std::move(problem));
    }
    Identity_ = std::move(identity);
    return OperationResult::Success();
}

OperationResult CharacterCreator::SetDateOfBirth(const DateOfBirth& birth)
{
    if (HasStarted_)
    {
        return IdentityLocked("The date of birth");
    }
    if (std::string problem = DescribeDateOfBirthProblem(birth); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidDateOfBirth, std::move(problem));
    }
    Identity_.Birth = birth;
    return OperationResult::Success();
}

OperationResult CharacterCreator::SetPronouns(std::string pronouns)
{
    if (HasStarted_)
    {
        return IdentityLocked("Pronouns");
    }
    if (std::string problem = DescribePronounsProblem(pronouns); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidIdentity, std::move(problem));
    }
    Identity_.Pronouns = std::move(pronouns);
    return OperationResult::Success();
}

OperationResult CharacterCreator::SetLanguages(std::string primary, std::vector<std::string> additional)
{
    if (HasStarted_)
    {
        return IdentityLocked("Languages");
    }
    if (std::string problem = DescribeLanguagesProblem(primary, additional); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidIdentity, std::move(problem));
    }
    Identity_.PrimaryLanguage = std::move(primary);
    Identity_.AdditionalLanguages = std::move(additional);
    return OperationResult::Success();
}

OperationResult CharacterCreator::SetSchoolStream(std::string_view streamId, std::string reason)
{
    if (HasStarted_)
    {
        return IdentityLocked("The school stream");
    }
    if (FindSchoolStream(streamId) == nullptr)
    {
        return OperationResult::Failure(CreationError::UnknownSchoolStream,
                                        "Unknown school stream: " + std::string(streamId));
    }
    if (std::string problem = DescribeSchoolReasonProblem(reason); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidSchoolReason, std::move(problem));
    }
    School_.StreamId = std::string(streamId);
    School_.Reason = std::move(reason);
    return OperationResult::Success();
}

OperationResult CharacterCreator::ConfirmMigratedIdentity(const DateOfBirth& birth, std::string_view streamId,
                                                          std::string reason)
{
    if (!IdentityMigrated_)
    {
        return OperationResult::Failure(CreationError::NothingToConfirm,
                                        "There is no migrated date of birth or stream to confirm.");
    }
    if (std::string problem = DescribeDateOfBirthProblem(birth); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidDateOfBirth, std::move(problem));
    }
    if (FindSchoolStream(streamId) == nullptr)
    {
        return OperationResult::Failure(CreationError::UnknownSchoolStream,
                                        "Unknown school stream: " + std::string(streamId));
    }
    if (std::string problem = DescribeSchoolReasonProblem(reason); !problem.empty())
    {
        return OperationResult::Failure(CreationError::InvalidSchoolReason, std::move(problem));
    }
    Identity_.Birth = birth;
    School_.StreamId = std::string(streamId);
    School_.Reason = std::move(reason);
    IdentityMigrated_ = false;
    return OperationResult::Success();
}

OperationResult CharacterCreator::ResolveRestoredIdentity(const CharacterCreationSave& save, IdentityProfile& identity,
                                                          SchoolChoice& school, bool& migrated)
{
    if (save.SchemaVersion >= 5)
    {
        std::string problem = DescribeIdentityProblem(save.Identity);
        if (problem.empty() && FindSchoolStream(save.School.StreamId) == nullptr)
        {
            problem = "The creation save names an unknown school stream: " + save.School.StreamId;
        }
        if (problem.empty())
        {
            problem = DescribeSchoolReasonProblem(save.School.Reason);
        }
        if (!problem.empty())
        {
            return OperationResult::Failure(CreationError::InvalidSave, std::move(problem));
        }
        identity = save.Identity;
        school = save.School;
        migrated = save.IdentityMigrated;
        return OperationResult::Success();
    }

    // Decision R2: an older record has no date of birth or stream, so it takes
    // the documented defaults and is marked for the player to confirm. Its free
    // text predates the limits; an entry that breaks them is dropped, not
    // truncated mid-character and not a reason to lose the whole save.
    identity = save.Identity;
    if (!DescribePronounsProblem(identity.Pronouns).empty())
    {
        identity.Pronouns.clear();
    }
    if (!DescribeLanguagesProblem(identity.PrimaryLanguage, {}).empty())
    {
        identity.PrimaryLanguage.clear();
    }
    identity.AdditionalLanguages.clear();
    identity.Birth = DefaultDateOfBirth;
    school = SchoolChoice{};
    migrated = true;
    return OperationResult::Success();
}
} // namespace PerfectWorld
