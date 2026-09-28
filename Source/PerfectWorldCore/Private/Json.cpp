#include "Json.h"

#include <charconv>
#include <cstring>

namespace PerfectWorld
{
const JsonValue* JsonValue::Find(std::string_view key) const
{
    for (const auto& [name, value] : Object)
    {
        if (name == key)
        {
            return &value;
        }
    }
    return nullptr;
}

namespace
{
constexpr int MaxDepth = 64;

class Parser
{
public:
    explicit Parser(std::string_view text) : Text(text) {}

    JsonParseResult Run()
    {
        JsonParseResult result;
        JsonValue value;
        if (ParseValue(value, 0))
        {
            SkipSpace();
            if (Pos == Text.size())
            {
                result.Value = std::move(value);
                return result;
            }
            Fail("unexpected text after the document");
        }
        result.Error = Error;
        result.Line = LineAt(ErrorPos);
        return result;
    }

private:
    std::string_view Text;
    std::size_t Pos = 0;
    std::string Error;
    std::size_t ErrorPos = 0;

    bool Fail(const char* message)
    {
        if (Error.empty())
        {
            Error = message;
            ErrorPos = Pos;
        }
        return false;
    }

    std::size_t LineAt(std::size_t pos) const
    {
        std::size_t line = 1;
        for (std::size_t i = 0; i < pos && i < Text.size(); ++i)
        {
            line += Text[i] == '\n' ? 1 : 0;
        }
        return line;
    }

    void SkipSpace()
    {
        while (Pos < Text.size() && (Text[Pos] == ' ' || Text[Pos] == '\t' || Text[Pos] == '\n' || Text[Pos] == '\r'))
        {
            ++Pos;
        }
    }

    bool Literal(const char* word)
    {
        const std::size_t length = std::strlen(word);
        if (Text.substr(Pos, length) != word)
        {
            return Fail("unknown literal");
        }
        Pos += length;
        return true;
    }

    bool ParseValue(JsonValue& out, int depth)
    {
        if (depth > MaxDepth)
        {
            return Fail("nesting too deep");
        }
        SkipSpace();
        if (Pos >= Text.size())
        {
            return Fail("unexpected end of document");
        }
        const char c = Text[Pos];
        if (c == '{')
        {
            return ParseObject(out, depth);
        }
        if (c == '[')
        {
            return ParseArray(out, depth);
        }
        if (c == '"')
        {
            out.Type = JsonValue::Kind::String;
            return ParseString(out.String);
        }
        if (c == 't')
        {
            out.Type = JsonValue::Kind::Bool;
            out.Bool = true;
            return Literal("true");
        }
        if (c == 'f')
        {
            out.Type = JsonValue::Kind::Bool;
            return Literal("false");
        }
        if (c == 'n')
        {
            out.Type = JsonValue::Kind::Null;
            return Literal("null");
        }
        if (c == '-' || (c >= '0' && c <= '9'))
        {
            return ParseNumber(out);
        }
        return Fail("unexpected character");
    }

    bool ParseObject(JsonValue& out, int depth)
    {
        out.Type = JsonValue::Kind::Object;
        ++Pos;
        SkipSpace();
        if (Pos < Text.size() && Text[Pos] == '}')
        {
            ++Pos;
            return true;
        }
        while (true)
        {
            SkipSpace();
            std::string key;
            if (Pos >= Text.size() || Text[Pos] != '"' || !ParseString(key))
            {
                return Fail("expected a member name");
            }
            if (out.Find(key) != nullptr)
            {
                return Fail("duplicate member name");
            }
            SkipSpace();
            if (Pos >= Text.size() || Text[Pos] != ':')
            {
                return Fail("expected ':'");
            }
            ++Pos;
            JsonValue value;
            if (!ParseValue(value, depth + 1))
            {
                return false;
            }
            out.Object.emplace_back(std::move(key), std::move(value));
            SkipSpace();
            if (Pos < Text.size() && Text[Pos] == ',')
            {
                ++Pos;
                continue;
            }
            if (Pos < Text.size() && Text[Pos] == '}')
            {
                ++Pos;
                return true;
            }
            return Fail("expected ',' or '}'");
        }
    }

    bool ParseArray(JsonValue& out, int depth)
    {
        out.Type = JsonValue::Kind::Array;
        ++Pos;
        SkipSpace();
        if (Pos < Text.size() && Text[Pos] == ']')
        {
            ++Pos;
            return true;
        }
        while (true)
        {
            JsonValue value;
            if (!ParseValue(value, depth + 1))
            {
                return false;
            }
            out.Array.push_back(std::move(value));
            SkipSpace();
            if (Pos < Text.size() && Text[Pos] == ',')
            {
                ++Pos;
                continue;
            }
            if (Pos < Text.size() && Text[Pos] == ']')
            {
                ++Pos;
                return true;
            }
            return Fail("expected ',' or ']'");
        }
    }

    static void AppendUtf8(std::string& out, std::uint32_t code)
    {
        if (code < 0x80)
        {
            out.push_back(static_cast<char>(code));
        }
        else if (code < 0x800)
        {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else if (code < 0x10000)
        {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        else
        {
            out.push_back(static_cast<char>(0xF0 | (code >> 18)));
            out.push_back(static_cast<char>(0x80 | ((code >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    bool ReadHex4(std::uint32_t& out)
    {
        if (Pos + 4 > Text.size())
        {
            return Fail("truncated \\u escape");
        }
        out = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char h = Text[Pos++];
            out <<= 4;
            if (h >= '0' && h <= '9')
                out |= static_cast<std::uint32_t>(h - '0');
            else if (h >= 'a' && h <= 'f')
                out |= static_cast<std::uint32_t>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F')
                out |= static_cast<std::uint32_t>(h - 'A' + 10);
            else
                return Fail("bad hex digit in \\u escape");
        }
        return true;
    }

    bool ParseString(std::string& out)
    {
        ++Pos; // opening quote
        out.clear();
        while (Pos < Text.size())
        {
            const char c = Text[Pos++];
            if (c == '"')
            {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20)
            {
                return Fail("control character in string");
            }
            if (c != '\\')
            {
                out.push_back(c);
                continue;
            }
            if (Pos >= Text.size())
            {
                break;
            }
            const char e = Text[Pos++];
            switch (e)
            {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u':
            {
                std::uint32_t code = 0;
                if (!ReadHex4(code))
                {
                    return false;
                }
                if (code >= 0xD800 && code <= 0xDBFF)
                {
                    std::uint32_t low = 0;
                    if (Pos + 2 > Text.size() || Text[Pos] != '\\' || Text[Pos + 1] != 'u')
                    {
                        return Fail("unpaired surrogate");
                    }
                    Pos += 2;
                    if (!ReadHex4(low) || low < 0xDC00 || low > 0xDFFF)
                    {
                        return Fail("unpaired surrogate");
                    }
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                }
                else if (code >= 0xDC00 && code <= 0xDFFF)
                {
                    return Fail("unpaired surrogate");
                }
                AppendUtf8(out, code);
                break;
            }
            default:
                return Fail("unknown escape");
            }
        }
        return Fail("unterminated string");
    }

    bool ParseNumber(JsonValue& out)
    {
        const std::size_t start = Pos;
        bool integral = true;
        if (Text[Pos] == '-')
        {
            ++Pos;
        }
        if (Pos >= Text.size() || Text[Pos] < '0' || Text[Pos] > '9')
        {
            return Fail("bad number");
        }
        if (Text[Pos] == '0' && Pos + 1 < Text.size() && Text[Pos + 1] >= '0' && Text[Pos + 1] <= '9')
        {
            return Fail("leading zero in number");
        }
        while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9')
        {
            ++Pos;
        }
        if (Pos < Text.size() && Text[Pos] == '.')
        {
            integral = false;
            ++Pos;
            if (Pos >= Text.size() || Text[Pos] < '0' || Text[Pos] > '9')
            {
                return Fail("bad fraction");
            }
            while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9')
            {
                ++Pos;
            }
        }
        if (Pos < Text.size() && (Text[Pos] == 'e' || Text[Pos] == 'E'))
        {
            integral = false;
            ++Pos;
            if (Pos < Text.size() && (Text[Pos] == '+' || Text[Pos] == '-'))
            {
                ++Pos;
            }
            if (Pos >= Text.size() || Text[Pos] < '0' || Text[Pos] > '9')
            {
                return Fail("bad exponent");
            }
            while (Pos < Text.size() && Text[Pos] >= '0' && Text[Pos] <= '9')
            {
                ++Pos;
            }
        }
        const char* first = Text.data() + start;
        const char* last = Text.data() + Pos;
        out.Type = JsonValue::Kind::Number;
        if (std::from_chars(first, last, out.Number).ec != std::errc())
        {
            return Fail("number out of range");
        }
        if (integral)
        {
            std::int64_t whole = 0;
            if (std::from_chars(first, last, whole).ec == std::errc())
            {
                out.Integer = whole;
            }
        }
        return true;
    }
};
} // namespace

JsonParseResult ParseJson(std::string_view text)
{
    return Parser(text).Run();
}
} // namespace PerfectWorld
