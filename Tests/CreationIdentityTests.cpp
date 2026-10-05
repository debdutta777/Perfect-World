// ROADMAP M0-18: identity and stream in the creation record, schema v5.

#include "CharacterCreation.h"
#include "CreationSaveSerialization.h"
#include "GameClock.h"

#include <cstdlib>
#include <iostream>
#include <set>
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

CharacterCreator ReadyCreator()
{
    CharacterCreator creator;
    Check(creator.ApplyPreset("balanced_scholar").Succeeded(), "preset applies");
    return creator;
}

// ---- The age rule ----

void TestAgeRule()
{
    // The campaign opens on 2 June 2042 and the player is exactly 16 that day.
    Check(AgeOnCampaignStart({2025, 6, 2}) == 17, "born 2 Jun 2025 is already 17 on 2 Jun 2042");
    Check(AgeOnCampaignStart({2025, 6, 3}) == 16, "born 3 Jun 2025 turns 17 the day after the opening");
    Check(AgeOnCampaignStart({2026, 6, 2}) == 16, "born 2 Jun 2026 turns 16 on the opening day");
    Check(AgeOnCampaignStart({2026, 6, 3}) == 15, "born 3 Jun 2026 is still 15");

    CharacterCreator creator;
    const auto result = [&creator](std::int32_t y, std::int32_t m, std::int32_t d)
    { return creator.SetDateOfBirth({y, m, d}); };
    Check(result(2025, 6, 2).Error == CreationError::InvalidDateOfBirth, "born 2 Jun 2025 refused");
    Check(result(2025, 6, 3).Succeeded(), "born 3 Jun 2025 accepted");
    Check(creator.Identity().Birth == DateOfBirth{2025, 6, 3}, "the accepted date is kept");
    Check(result(2026, 6, 2).Succeeded(), "born 2 Jun 2026 accepted");
    Check(result(2026, 6, 3).Error == CreationError::InvalidDateOfBirth, "born 3 Jun 2026 refused");
    Check(creator.Identity().Birth == DateOfBirth{2026, 6, 2}, "a refused date leaves the last good one");
    Check(EarliestDateOfBirth == DateOfBirth{2025, 6, 3} && LatestDateOfBirth == DateOfBirth{2026, 6, 2},
          "the published window matches the rule");
    Check(DescribeDateOfBirthProblem(DefaultDateOfBirth).empty(), "the migration default is a valid date of birth");

    // Dates that do not exist are refused, not normalised.
    for (const DateOfBirth& impossible :
         {DateOfBirth{2026, 2, 29}, DateOfBirth{2025, 6, 31}, DateOfBirth{2025, 13, 1}, DateOfBirth{2025, 0, 10},
          DateOfBirth{2025, 9, 0}, DateOfBirth{2025, 9, -1}, DateOfBirth{0, 0, 0}, DateOfBirth{-2026, 1, 1},
          DateOfBirth{2147483647, 12, 31}, DateOfBirth{-2147483647 - 1, 1, 1}})
    {
        Check(creator.SetDateOfBirth(impossible).Error == CreationError::InvalidDateOfBirth,
              "an impossible calendar date is refused");
    }

    // Every day for four years: accepted exactly from 3 Jun 2025 to 2 Jun 2026, which is 365 days.
    int accepted = 0;
    for (std::int32_t year = 2024; year <= 2027; ++year)
    {
        for (std::int32_t month = 1; month <= 12; ++month)
        {
            for (std::int32_t day = 1; day <= 31; ++day)
            {
                const DateOfBirth birth{year, month, day};
                if (!IsValidCivilDate(year, month, day))
                {
                    Check(!DescribeDateOfBirthProblem(birth).empty(), "a nonexistent day is never valid");
                    continue;
                }
                const bool inWindow = (year == 2025 && (month > 6 || (month == 6 && day >= 3))) ||
                                      (year == 2026 && (month < 6 || (month == 6 && day <= 2)));
                Check(DescribeDateOfBirthProblem(birth).empty() == inWindow, "the window is exactly 3 Jun 2025 to 2 Jun 2026");
                accepted += inWindow ? 1 : 0;
            }
        }
    }
    Check(accepted == 365, "one year of birthdays is accepted");
}

// ---- Pronouns, languages, stream ----

std::string Bytes(std::size_t count, char fill = 'a') { return std::string(count, fill); }

void TestFreeText()
{
    CharacterCreator creator;
    Check(creator.SetPronouns("she/her").Succeeded() && creator.Identity().Pronouns == "she/her", "pronouns accepted");
    Check(creator.SetPronouns("").Succeeded() && creator.Identity().Pronouns.empty(), "empty pronouns are valid");
    Check(creator.SetPronouns("they/them").Succeeded(), "any pronouns are accepted");
    Check(creator.SetPronouns("\xe0\xa6\xb8\xe0\xa7\x87").Succeeded(), "non-ASCII pronouns are accepted");
    Check(creator.SetPronouns(Bytes(MaxPronounsBytes)).Succeeded(), "pronouns at the limit are accepted");
    Check(creator.SetPronouns(Bytes(MaxPronounsBytes + 1)).Error == CreationError::InvalidIdentity, "pronouns over the limit refused");
    Check(creator.SetPronouns("a\nb").Error == CreationError::InvalidIdentity, "a control character is refused");
    Check(creator.SetPronouns(std::string("a\0b", 3)).Error == CreationError::InvalidIdentity, "an embedded NUL is refused");
    Check(creator.SetPronouns("\xff\xfe").Error == CreationError::InvalidIdentity, "invalid UTF-8 is refused");
    Check(creator.SetPronouns("\xe0\xa6").Error == CreationError::InvalidIdentity, "truncated UTF-8 is refused");
    Check(creator.SetPronouns("\xc0\xaf").Error == CreationError::InvalidIdentity, "overlong UTF-8 is refused");
    Check(creator.SetPronouns("\xed\xa0\x80").Error == CreationError::InvalidIdentity, "a surrogate is refused");
    Check(creator.Identity().Pronouns == Bytes(MaxPronounsBytes), "refusals leave the pronouns alone");

    Check(creator.SetLanguages("Bengali", {"Hindi", "English"}).Succeeded(), "languages accepted");
    Check(creator.Identity().PrimaryLanguage == "Bengali" && creator.Identity().AdditionalLanguages.size() == 2 &&
              creator.Identity().AdditionalLanguages[0] == "Hindi", "languages kept in order");
    std::vector<std::string> six;
    for (std::size_t i = 0; i < MaxAdditionalLanguages; ++i) six.push_back("Language" + std::to_string(i));
    Check(creator.SetLanguages("Bengali", six).Succeeded(), "the maximum number of languages is accepted");
    six.push_back("Language9");
    Check(creator.SetLanguages("Bengali", six).Error == CreationError::InvalidIdentity, "too many languages refused");
    Check(creator.SetLanguages("Bengali", {"Hindi", "hindi"}).Error == CreationError::InvalidIdentity, "a repeat ignoring case is refused");
    Check(creator.SetLanguages("Bengali", {"BENGALI"}).Error == CreationError::InvalidIdentity, "repeating the primary language is refused");
    Check(creator.SetLanguages("Bengali", {""}).Error == CreationError::InvalidIdentity, "an empty additional language is refused");
    Check(creator.SetLanguages("Bengali", {Bytes(MaxLanguageBytes + 1)}).Error == CreationError::InvalidIdentity, "a long language is refused");
    Check(creator.SetLanguages(Bytes(MaxLanguageBytes + 1), {}).Error == CreationError::InvalidIdentity, "a long primary language is refused");
    Check(creator.SetLanguages("Beng\tali", {}).Error == CreationError::InvalidIdentity, "a control character in a language is refused");
    Check(creator.Identity().AdditionalLanguages.size() == MaxAdditionalLanguages && creator.Identity().PrimaryLanguage == "Bengali",
          "refusals leave the languages alone");
    Check(creator.SetLanguages("", {}).Succeeded(), "no languages at all is valid for a draft");

    // The whole-identity setter validates every part and refuses as a whole.
    IdentityProfile profile{"Ananya", "Sen", "she/her", "Kolkata", "Bengali", {"Hindi"}, {2026, 1, 15}};
    Check(creator.SetIdentity(profile).Succeeded() && creator.Identity().Birth == DateOfBirth{2026, 1, 15}, "a full identity is accepted");
    IdentityProfile badDate = profile;
    badDate.Birth = {2024, 1, 1};
    badDate.GivenName = "Changed";
    Check(creator.SetIdentity(badDate).Error == CreationError::InvalidDateOfBirth, "a bad date refuses the identity");
    IdentityProfile badPronouns = profile;
    badPronouns.GivenName = "Changed";
    badPronouns.Pronouns = Bytes(MaxPronounsBytes + 1);
    Check(creator.SetIdentity(badPronouns).Error == CreationError::InvalidIdentity, "bad pronouns refuse the identity");
    Check(creator.Identity().GivenName == "Ananya", "a refused identity changes nothing");
}

void TestStream()
{
    CharacterCreator creator = ReadyCreator();
    Check(creator.School().StreamId == "science" && creator.School().Reason.empty(), "a fresh draft is in science");
    Check(creator.Review().SchoolStreamId == "science", "the review names the stream");
    Check(SchoolStreams().size() == 3 && FindSchoolStream("science") && FindSchoolStream("commerce") && FindSchoolStream("humanities"),
          "the three streams exist");
    std::set<std::string_view> ids;
    for (const auto& stream : SchoolStreams()) { ids.insert(stream.Id); Check(!stream.DisplayName.empty() && !stream.TimetableSummary.empty(), "a stream is described"); }
    Check(ids.size() == 3, "stream IDs are distinct");

    // The stream sets a timetable, not a destiny: no cost, no stat, no home.
    const CreationReview before = creator.Review();
    const auto ranksBefore = creator.CaptureSave().TraitRanks;
    const std::string homeBefore(creator.ResolveOpeningHome().HomeKitId);
    for (const char* stream : {"commerce", "humanities", "science"})
    {
        Check(creator.SetSchoolStream(stream, "because it suits me").Succeeded(), "each stream is selectable");
        Check(creator.School().StreamId == stream && creator.Review().SchoolStreamId == stream, "the stream is kept");
        const CreationReview after = creator.Review();
        Check(after.Spent == before.Spent && after.Remaining == before.Remaining && after.RankFourCount == before.RankFourCount &&
                  after.ReadyToStart == before.ReadyToStart && after.HouseholdTemplateId == before.HouseholdTemplateId &&
                  after.HomeKitId == before.HomeKitId, "the stream changes no cost");
        Check(creator.CaptureSave().TraitRanks == ranksBefore, "the stream changes no trait rank");
        Check(creator.ResolveOpeningHome().HomeKitId == homeBefore, "the stream changes no home");
        for (std::size_t i = 0; i < after.Allocations.size(); ++i)
            Check(after.Allocations[i].Rank == before.Allocations[i].Rank && after.Allocations[i].Cost == before.Allocations[i].Cost,
                  "the stream changes no allocation");
    }

    // An unknown stream is refused whole, whatever else was passed.
    Check(creator.SetSchoolStream("commerce", "keep this").Succeeded(), "commerce for the refusal test");
    for (const char* unknown : {"arts", "Science", "SCIENCE", "science ", "", "humanities\n", "vocational"})
    {
        Check(creator.SetSchoolStream(unknown, "changed").Error == CreationError::UnknownSchoolStream, "an unknown stream is refused");
    }
    Check(creator.School().StreamId == "commerce" && creator.School().Reason == "keep this", "a refused stream changes nothing");

    Check(creator.SetSchoolStream("science", Bytes(MaxSchoolReasonBytes)).Succeeded(), "a reason at the limit is accepted");
    Check(creator.SetSchoolStream("humanities", Bytes(MaxSchoolReasonBytes + 1)).Error == CreationError::InvalidSchoolReason, "a long reason is refused");
    Check(creator.SetSchoolStream("humanities", "line one\nline two").Error == CreationError::InvalidSchoolReason, "a reason with a newline is refused");
    Check(creator.SetSchoolStream("humanities", "\xff").Error == CreationError::InvalidSchoolReason, "a reason with invalid UTF-8 is refused");
    Check(creator.School().StreamId == "science" && creator.School().Reason.size() == MaxSchoolReasonBytes, "a refused reason changes nothing");
    Check(creator.SetSchoolStream("humanities", "").Succeeded(), "the reason may be empty");

    // Everything is locked once life begins, and Reset gives a clean draft.
    Check(creator.SetDateOfBirth({2026, 1, 1}).Succeeded() && creator.StartLife().Succeeded(), "life starts");
    Check(creator.SetDateOfBirth({2026, 2, 2}).Error == CreationError::CreationAlreadyStarted, "the date of birth is locked");
    Check(creator.SetPronouns("he/him").Error == CreationError::CreationAlreadyStarted, "pronouns are locked");
    Check(creator.SetLanguages("Hindi", {}).Error == CreationError::CreationAlreadyStarted, "languages are locked");
    Check(creator.SetSchoolStream("science", "").Error == CreationError::CreationAlreadyStarted, "the stream is locked");
    Check(creator.SetIdentity({}).Error == CreationError::CreationAlreadyStarted, "the identity is locked");
    creator.Reset();
    Check(creator.School().StreamId == "science" && creator.Identity().Birth == DefaultDateOfBirth && !creator.IdentityNeedsConfirmation(),
          "reset restores the defaults");
}

// ---- Text round trip ----

CharacterCreator FullCreator()
{
    CharacterCreator creator = ReadyCreator();
    Check(creator.SetIdentity({"Ananya \"Ana\"", "Sen\\Roy", "she/her", "Kolkata", "Bengali", {"Hindi", "English", "\xe0\xa6\xac\xe0\xa6\xbe"}, {2025, 12, 5}}).Succeeded(),
          "identity for the round trip");
    Check(creator.SetSchoolStream("humanities", "I want to study \"history\" \\ more").Succeeded(), "stream for the round trip");
    return creator;
}

void TestRoundTrip()
{
    CharacterCreator creator = FullCreator();
    creator.SetResumePoint(ResumePoint::Review);
    const std::string text = SerializeCreationSave(creator.CaptureSave());
    Check(text.find("\"schemaVersion\": 5") != std::string::npos, "the writer emits schema 5");
    Check(text.find("\"dateOfBirth\": \"2025-12-05\"") != std::string::npos, "the date is written as ISO text");
    Check(text.find("\"streamId\": \"humanities\"") != std::string::npos, "the stream is written by ID");
    Check(text.find("\"identityMigrated\": false") != std::string::npos, "the migrated flag is written");
    Check(text.find("\"resumePoint\": \"review\"") != std::string::npos, "the review resume point is written");

    CharacterCreationSave parsed;
    Check(DeserializeCreationSave(text, parsed).Succeeded(), "the v5 text parses");
    CharacterCreator back;
    Check(back.RestoreSave(parsed).Succeeded(), "the v5 record restores");
    Check(back.Identity().Birth == DateOfBirth{2025, 12, 5}, "the date round-trips");
    Check(back.Identity().Pronouns == "she/her" && back.Identity().PrimaryLanguage == "Bengali", "pronouns and language round-trip");
    Check(back.Identity().AdditionalLanguages == creator.Identity().AdditionalLanguages, "additional languages round-trip, UTF-8 included");
    Check(back.Identity().GivenName == "Ananya \"Ana\"" && back.Identity().FamilyName == "Sen\\Roy", "names still round-trip");
    Check(back.School().StreamId == "humanities" && back.School().Reason == "I want to study \"history\" \\ more", "the stream and escaped reason round-trip");
    Check(!back.IdentityNeedsConfirmation(), "a chosen identity is not flagged");
    Check(back.Resume() == ResumePoint::Review, "the review resume point round-trips");
    Check(SerializeCreationSave(back.CaptureSave()) == text, "restore then write is byte-identical");

    // Truncating a v5 file anywhere fails rather than half-loading.
    for (std::size_t cut = 1; cut + 1 < text.size(); ++cut)
    {
        CharacterCreationSave partial;
        if (DeserializeCreationSave(std::string_view(text).substr(0, cut), partial).Succeeded())
            Check(false, "a truncated v5 save must not parse");
    }

    // Damaged v5 content is refused, and a refusal leaves the target alone.
    const auto replaced = [&text](const std::string& from, const std::string& to)
    {
        std::string copy = text;
        const std::size_t at = copy.find(from);
        Check(at != std::string::npos, "fixture contains the text to damage");
        copy.replace(at, from.size(), to);
        return copy;
    };
    CharacterCreationSave target = parsed;
    for (const char* badDate : {"2025-12-5", "2025/12/05", "20251205", "2025-12-05x", "abcd-ef-gh", ""})
    {
        Check(DeserializeCreationSave(replaced("2025-12-05", badDate), target).Error == CreationError::MalformedSaveText, "a badly written date is refused");
    }
    Check(DeserializeCreationSave(replaced("\"dateOfBirth\"", "\"birthday\""), target).Error == CreationError::MalformedSaveText, "an unknown identity field is refused");
    Check(DeserializeCreationSave(replaced("\"streamId\"", "\"stream\""), target).Error == CreationError::MalformedSaveText, "an unknown school field is refused");
    Check(DeserializeCreationSave(replaced("\"identityMigrated\": false", "\"identityMigrated\": 0"), target).Error == CreationError::MalformedSaveText, "a non-boolean migrated flag is refused");
    Check(DeserializeCreationSave(replaced("[\"Hindi\"", "[Hindi"), target).Error == CreationError::MalformedSaveText, "a malformed language list is refused");
    Check(target.School.StreamId == parsed.School.StreamId && target.Identity.Birth == parsed.Identity.Birth, "refused text leaves the target untouched");

    // A v5 file may not omit the date of birth or the stream: that would be a silent default.
    std::string noDate = replaced(",\n    \"dateOfBirth\": \"2025-12-05\"", "");
    Check(DeserializeCreationSave(noDate, target).Error == CreationError::MalformedSaveText, "a v5 file without a date of birth is refused");
    std::string noStream = replaced("\"streamId\": \"humanities\",\n    ", "");
    Check(DeserializeCreationSave(noStream, target).Error == CreationError::MalformedSaveText, "a v5 file without a stream is refused");

    // Parsed but impossible: Restore refuses with InvalidSave and changes nothing.
    CharacterCreator loader = FullCreator();
    const auto restoreMessage = [&loader](const CharacterCreationSave& record)
    {
        const OperationResult result = loader.RestoreSave(record);
        Check(result.Error == CreationError::InvalidSave && !result.Message.empty(), "an invalid v5 record is refused with a message");
        Check(loader.Identity().Birth == DateOfBirth{2025, 12, 5}, "a refused restore changes nothing");
    };
    CharacterCreationSave bad = parsed;
    bad.Identity.Birth = {2026, 6, 3};
    restoreMessage(bad);
    bad = parsed; bad.Identity.Birth = {2026, 2, 30}; restoreMessage(bad);
    bad = parsed; bad.School.StreamId = "arts"; restoreMessage(bad);
    bad = parsed; bad.School.Reason = Bytes(MaxSchoolReasonBytes + 1); restoreMessage(bad);
    bad = parsed; bad.Identity.Pronouns = Bytes(MaxPronounsBytes + 1); restoreMessage(bad);
    bad = parsed; bad.Identity.AdditionalLanguages = {"Hindi", "hindi"}; restoreMessage(bad);
    bad = parsed; bad.SchemaVersion = SaveSchemaVersion + 1;
    Check(loader.RestoreSave(bad).Error == CreationError::UnsupportedSaveVersion, "a newer record is unsupported");
    Check(DeserializeCreationSave(replaced("\"schemaVersion\": 5", "\"schemaVersion\": 99"), target).Succeeded() &&
              loader.RestoreSave(target).Error == CreationError::UnsupportedSaveVersion,
          "a future file is unsupported rather than damaged");
}

// ---- Migration (decision R2) ----

/** A creation-only file as the schema 4 build wrote it. */
std::string VersionFourText(bool started, const std::string& pronouns)
{
    CharacterCreator scholar = ReadyCreator();
    std::string text = "{\n  \"schemaVersion\": 4,\n  \"age\": 16,\n  \"hasStarted\": ";
    text += started ? "true" : "false";
    text += ",\n  \"resumePoint\": \"";
    text += started ? "in_world" : "allocation";
    text += "\",\n  \"householdTemplateId\": \"appointed_guardian\",\n  \"identity\": {\n    \"givenName\": \"Ananya\",\n    \"familyName\": \"Sen\",\n    \"pronouns\": \"" + pronouns +
            "\",\n    \"homeCity\": \"Kolkata\",\n    \"primaryLanguage\": \"Bengali\"\n  },\n"
            "  \"appearance\": {\n    \"schemaVersion\": 1,\n    \"faceShapeId\": \"face_heart\",\n    \"hairStyleId\": \"hair_long_braid\",\n    \"buildId\": \"build_sturdy\",\n"
            "    \"uniformVariantId\": \"uniform_casual\",\n    \"skinTone\": { \"r\": 123, \"g\": 84, \"b\": 61 },\n    \"hairColor\": { \"r\": 20, \"g\": 16, \"b\": 14 },\n"
            "    \"eyeColor\": { \"r\": 90, \"g\": 120, \"b\": 60 }\n  },\n  \"traitRanks\": {\n";
    const auto& definitions = CharacterCreator::TraitDefinitions();
    for (std::size_t i = 0; i < TraitCount; ++i)
        text += "    \"" + std::string(definitions[i].Id) + "\": " + std::to_string(scholar.GetTraitRank(definitions[i].Id)) + (i + 1 < TraitCount ? ",\n" : "\n");
    return text + "  }\n}\n";
}

void CheckMigratedDefaults(const CharacterCreator& creator, const char* what)
{
    Check(creator.Identity().Birth == DateOfBirth{2026, 1, 1}, what);
    Check(creator.School().StreamId == "science" && creator.School().Reason.empty(), what);
    Check(creator.Identity().AdditionalLanguages.empty(), what);
    Check(creator.IdentityNeedsConfirmation(), what);
    Check(creator.CaptureSave().IdentityMigrated && creator.CaptureSave().SchemaVersion == SaveSchemaVersion, what);
}

void TestMigration()
{
    for (const bool started : {true, false})
    {
        CharacterCreationSave parsed;
        Check(DeserializeCreationSave(VersionFourText(started, "she/her"), parsed).Succeeded(), "a v4 file parses");
        Check(parsed.SchemaVersion == 4, "the v4 file reports its version");
        CharacterCreator migrated;
        Check(migrated.RestoreSave(parsed).Succeeded(), "a v4 file restores");
        CheckMigratedDefaults(migrated, "a v4 file migrates to the R2 defaults");
        Check(migrated.Identity().GivenName == "Ananya" && migrated.Identity().Pronouns == "she/her" &&
                  migrated.Identity().PrimaryLanguage == "Bengali" && migrated.HasStarted() == started, "a v4 file keeps what it had");

        // What it saves is a v5 file that still says it is migrated, and loads back the same.
        const std::string v5 = SerializeCreationSave(migrated.CaptureSave());
        Check(v5.find("\"schemaVersion\": 5") != std::string::npos && v5.find("\"dateOfBirth\": \"2026-01-01\"") != std::string::npos &&
                  v5.find("\"streamId\": \"science\"") != std::string::npos && v5.find("\"identityMigrated\": true") != std::string::npos,
              "the migrated record is written as v5 with the flag set");
        CharacterCreationSave again;
        CharacterCreator reloaded;
        Check(DeserializeCreationSave(v5, again).Succeeded() && reloaded.RestoreSave(again).Succeeded(), "the migrated v5 file reloads");
        CheckMigratedDefaults(reloaded, "the migrated flag survives a save and load");
        Check(SerializeCreationSave(reloaded.CaptureSave()) == v5, "the migrated v5 file is stable");
    }

    // Each older version, at the struct level: whatever a record carries for v5 fields is ignored, then defaulted.
    for (std::int32_t version = 1; version <= 4; ++version)
    {
        CharacterCreator scholar = ReadyCreator();
        CharacterCreationSave record = scholar.CaptureSave();
        record.SchemaVersion = version;
        record.Identity.Birth = {1990, 3, 3};
        record.Identity.AdditionalLanguages = {"Tamil"};
        record.School = {"commerce", "stray"};
        record.IdentityMigrated = false;
        CharacterCreator migrated;
        Check(migrated.RestoreSave(record).Succeeded(), "an older record restores");
        CheckMigratedDefaults(migrated, "versions 1 to 4 migrate to the R2 defaults");
    }

    // Free text from before the limits is dropped rather than refusing the whole save.
    CharacterCreationSave longText;
    Check(DeserializeCreationSave(VersionFourText(true, Bytes(MaxPronounsBytes + 10)), longText).Succeeded(), "a v4 file with long pronouns parses");
    CharacterCreator trimmed;
    Check(trimmed.RestoreSave(longText).Succeeded() && trimmed.Identity().Pronouns.empty() && trimmed.Identity().GivenName == "Ananya",
          "over-long legacy pronouns are dropped, not fatal");

    // The flag is answered by an explicit confirmation, even after life has begun.
    CharacterCreationSave parsed;
    Check(DeserializeCreationSave(VersionFourText(true, "she/her"), parsed).Succeeded(), "a started v4 file parses");
    CharacterCreator started;
    Check(started.RestoreSave(parsed).Succeeded() && started.HasStarted() && started.IdentityNeedsConfirmation(), "a started migrated record waits for confirmation");
    Check(started.SetDateOfBirth({2025, 9, 9}).Error == CreationError::CreationAlreadyStarted, "an ordinary edit is still locked");
    Check(started.ConfirmMigratedIdentity({2024, 1, 1}, "commerce", "").Error == CreationError::InvalidDateOfBirth, "a bad confirmation date is refused");
    Check(started.ConfirmMigratedIdentity({2025, 9, 9}, "arts", "").Error == CreationError::UnknownSchoolStream, "an unknown confirmation stream is refused");
    Check(started.ConfirmMigratedIdentity({2025, 9, 9}, "commerce", Bytes(MaxSchoolReasonBytes + 1)).Error == CreationError::InvalidSchoolReason, "a long confirmation reason is refused");
    Check(started.IdentityNeedsConfirmation() && started.Identity().Birth == DefaultDateOfBirth, "a refused confirmation changes nothing");
    Check(started.ConfirmMigratedIdentity({2025, 9, 9}, "commerce", "numbers make sense to me").Succeeded(), "the player can confirm");
    Check(!started.IdentityNeedsConfirmation() && started.Identity().Birth == DateOfBirth{2025, 9, 9} &&
              started.School().StreamId == "commerce" && started.HasStarted(), "confirming replaces the defaults and clears the flag");
    Check(started.ConfirmMigratedIdentity({2025, 9, 9}, "commerce", "").Error == CreationError::NothingToConfirm, "there is nothing left to confirm");
    CharacterCreator fresh;
    Check(fresh.ConfirmMigratedIdentity({2025, 9, 9}, "commerce", "").Error == CreationError::NothingToConfirm, "a fresh draft has nothing to confirm");

    // Confirming the origin of a migrated draft confirms its review page.
    Check(DeserializeCreationSave(VersionFourText(false, "she/her"), parsed).Succeeded(), "a v4 draft parses");
    CharacterCreator draft;
    Check(draft.RestoreSave(parsed).Succeeded() && draft.IdentityNeedsConfirmation() && !draft.HasStarted(), "a migrated draft is flagged");
    Check(draft.SetDateOfBirth({2025, 10, 10}).Succeeded() && draft.IdentityNeedsConfirmation(), "editing a draft alone does not clear the flag");
    Check(draft.StartLife().Succeeded() && !draft.IdentityNeedsConfirmation(), "starting life clears the flag");
}

void TestResumePoints()
{
    const std::pair<ResumePoint, const char*> points[] = {
        {ResumePoint::Identity, "identity"}, {ResumePoint::Stream, "stream"}, {ResumePoint::Review, "review"},
        {ResumePoint::Appearance, "appearance"}, {ResumePoint::Allocation, "allocation"}, {ResumePoint::InWorld, "in_world"}};
    std::set<std::string_view> names;
    for (const auto& [point, name] : points)
    {
        names.insert(ResumePointId(point));
        Check(ResumePointId(point) == name, "a resume point has its stable text");
        ResumePoint parsed = ResumePoint::InWorld;
        Check(ParseResumePoint(name, parsed) && parsed == point, "a resume point parses from its text");
        if (point == ResumePoint::InWorld) continue;
        CharacterCreator creator = ReadyCreator();
        creator.SetResumePoint(point);
        CharacterCreationSave record;
        Check(DeserializeCreationSave(SerializeCreationSave(creator.CaptureSave()), record).Succeeded(), "a record with this resume point parses");
        CharacterCreator back;
        Check(back.RestoreSave(record).Succeeded() && back.Resume() == point, "a resume point survives the text round trip");
    }
    Check(names.size() == 6, "resume point texts are distinct");
    ResumePoint untouched = ResumePoint::Allocation;
    Check(!ParseResumePoint("Identity", untouched) && !ParseResumePoint("", untouched) && untouched == ResumePoint::Allocation, "an unknown resume point is refused");

    // The three values the Unreal mirror enum already uses keep their numbers.
    Check(static_cast<int>(ResumePoint::Appearance) == 0 && static_cast<int>(ResumePoint::Allocation) == 1 && static_cast<int>(ResumePoint::InWorld) == 2,
          "the original resume point values are unchanged");

    CharacterCreator started = ReadyCreator();
    Check(started.StartLife().Succeeded(), "life starts for the resume test");
    started.SetResumePoint(ResumePoint::Identity);
    Check(started.Resume() == ResumePoint::InWorld, "a confirmed origin cannot resume at identity");
}
} // namespace

int main()
{
    Check(SaveSchemaVersion == 5, "the creation schema is version 5");
    CharacterCreator creator;
    Check(creator.Identity().Birth == DefaultDateOfBirth && creator.School().StreamId == DefaultSchoolStreamId && !creator.IdentityNeedsConfirmation(),
          "a fresh draft has the defaults and is not flagged");
    TestAgeRule();
    TestFreeText();
    TestStream();
    TestRoundTrip();
    TestMigration();
    TestResumePoints();
    std::cout << "PASS: " << Checks << " checks\n";
}
