// ROADMAP M2-10: the text form of the relationship book. Linked into PerfectWorldRelationshipTests; main() is in RelationshipsTests.cpp.
#include "RandomStreams.h"
#include "RelationshipsTestSupport.h"

#include <array>
#include <string_view>

namespace
{
RelationshipBook RelRichBook(EventLog& log)
{
    RelationshipBook book;
    RandomStream rng(9);
    GameInstant now = 0;
    for (std::size_t i = 0; i < 60; ++i)
    {
        now += static_cast<GameInstant>(rng.NextBelow(3000));
        const std::size_t a = static_cast<std::size_t>(rng.NextBelow(3));
        const std::size_t b = 3 + static_cast<std::size_t>(rng.NextBelow(2));
        const std::uint64_t seq = RelAppend(log, now, "scene_" + std::to_string(i % 4), {RelPerson(a), RelPerson(b)});
        book.Apply(log, RelAct(seq, RelPerson(a), RelPerson(b), static_cast<RelationshipActKind>(rng.NextBelow(RelationshipActKindCount))));
    }
    return book;
}
} // namespace

void RelTestSaveRefusals()
{
    EventLog log;
    const RelationshipBook book = RelRichBook(log);
    const std::string text = SerialiseRelationships(book);
    Check(book.Size() >= 4 && text.size() > 1000, "the sample book has several pairs and memories");
    const auto loaded = ParseRelationships(text);
    Check(loaded && *loaded == book && RelationshipsMatchLog(*loaded, log), "the sample survives a save and still matches the log");
    Check(SerialiseRelationships(RelationshipBook()) == "pw_relationships 1\ncount 0\nend " + std::to_string(HashRelationships(RelationshipBook())) + "\n" &&
              ParseRelationships(SerialiseRelationships(RelationshipBook())).has_value(),
          "an empty book round-trips");

    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!ParseRelationships(std::string_view(text).substr(0, length)), "every truncation is refused");

    const std::array<char, 4> replacements = {'0', '7', 'z', ' '};
    for (std::size_t i = 0; i < text.size(); ++i)
    {
        for (const char replacement : replacements)
        {
            if (text[i] == replacement || text[i] == '\n')
                continue;
            std::string corrupt = text;
            corrupt[i] = replacement;
            Check(!ParseRelationships(corrupt), "every single-byte corruption is refused");
        }
    }
    Check(!ParseRelationships(text + "x") && !ParseRelationships(text + "\n") && !ParseRelationships("\n" + text), "extra bytes are refused");
    std::string crlf = text;
    crlf.replace(crlf.find('\n'), 1, "\r\n");
    Check(!ParseRelationships(crlf), "carriage returns are refused");
    std::string wrongVersion = text;
    wrongVersion.replace(text.find(" 1\n"), 3, " 2\n");
    Check(!ParseRelationships(wrongVersion), "a wrong version is refused");
    std::string padded = text;
    padded.replace(padded.find("count "), 6, "count 0");
    Check(!ParseRelationships(padded), "a leading-zero or wrong count is refused");
}
