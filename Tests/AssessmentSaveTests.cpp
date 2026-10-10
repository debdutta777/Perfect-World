// ROADMAP M3-07: a paper saved mid-sitting restores exactly and finishes once; every truncation
// and every edited line is refused; and random command sequences keep every invariant (the
// units add up, a refusal changes nothing, a quote equals its charge, the text round-trips).
#include "AssessmentTestSupport.h"

using namespace PerfectWorld;

namespace
{
std::vector<std::string> SplitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t at = text.find('\n'); at != std::string::npos; at = text.find('\n', start))
    {
        lines.push_back(text.substr(start, at - start));
        start = at + 1;
    }
    return lines;
}

std::string JoinLines(const std::vector<std::string>& lines)
{
    std::string text;
    for (const std::string& line : lines)
        text += line + "\n";
    return text;
}

void ExpectRoundTrip(const AssessmentState& state)
{
    const std::string text = SerialiseAssessment(state);
    Check(!text.empty(), "a valid state serialises");
    const AssessmentParse parsed = DeserialiseAssessment(text);
    Check(parsed.Ok() && parsed.State == state, "and reads back unchanged");
    Check(SerialiseAssessment(parsed.State) == text, "the same state always gives the same text");
}

void ExpectEveryTruncationRefused(const std::string& text)
{
    for (std::size_t length = 0; length < text.size(); ++length)
        Check(!DeserialiseAssessment(std::string_view(text).substr(0, length)).Ok(), "a truncated save is refused");
    Check(DeserialiseAssessment(text).Ok(), "the whole text is read");
}

/** A character who is practised in some methods, shaky in some and without others. */
AssessmentSkills MixedSkills(const AssessmentPaperDef& paper)
{
    AssessmentSkills skills = PreparedSkills(paper);
    for (AssessmentSkillLevel& level : skills)
    {
        if (level.SkillId == "skill.academics")
            level.Milli = 115000;
        else if (level.SkillId == "skill.communication")
            level.Milli = 0;
    }
    return skills;
}

std::uint64_t Hash(std::uint64_t seed, const std::string& text)
{
    for (const char c : text)
        seed = (seed ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
    return seed;
}

/** One random sitting; returns a hash of every state text it passed through. */
std::uint64_t RunSequence(std::uint64_t seed)
{
    const AssessmentPaperDef& paper = *TestWorld->Paper;
    OthersRng rng{seed};
    std::uint64_t hash = 14695981039346656037ULL;
    AssessmentState state;
    for (int step = 0; step < 90; ++step)
    {
        if (!state.Active() && rng.Below(3) != 0)
        {
            AssessmentSkills skills = PreparedSkills(paper);
            for (AssessmentSkillLevel& level : skills)
                level.Milli = static_cast<std::int32_t>(rng.Below(4) == 0 ? 0 : rng.Below(150000));
            Check(BeginAssessment(state, paper, skills) == AssessmentError::None, "a new paper begins once the last is handed in");
        }
        const AssessmentState before = state;
        const std::size_t action = rng.Below(8);
        const bool wasFinished = state.Finished;
        if (action == 0)
        {
            const AssessmentQuote quote = QuoteAssessmentSurvey(state);
            const AssessmentSurveyResult result = SurveyAssessment(state, paper);
            Check(result.Error == quote.Error, "the survey succeeds exactly when it was quoted to");
            Check(result.Ok() ? (result.Charged == quote.Units && state.UnitsLeft == before.UnitsLeft - quote.Units) : state == before,
                  "a survey is charged its quote, or changes nothing");
        }
        else if (action == 1)
        {
            const std::size_t group = rng.Below(6);
            const std::int32_t units = static_cast<std::int32_t>(rng.Below(21)) - 1;
            const AssessmentError error = AllocateAssessmentUnits(state, group, units);
            if (error == AssessmentError::None)
                Check(state.Groups[group].Allocated == units && AssessmentUnallocated(state) >= 0, "an allocation is kept and fits the pool");
            else
                Check(state == before, "a refused allocation changes nothing");
        }
        else if (action <= 4)
        {
            AssessmentAnswer answer = Answer(rng.Below(6), rng.Below(4) == 0 ? AssessmentMode::Guess : AssessmentMode::Work, rng.Below(2) == 0, rng.Below(3) == 0);
            if (rng.Below(2) == 0 && answer.Group < state.Groups.size() && state.Active())
                AllocateAssessmentUnits(state, answer.Group, AssessmentAnswerUnits(state.Groups[answer.Group].Known, answer.Mode, answer.Confirm));
            const AssessmentState prepared = state;
            const AssessmentStepResult result = Play(state, answer);
            if (!result.Ok())
                Check(state == prepared, "a refused answer changes nothing");
            else
                Check(state.Groups[answer.Group].Answered && state.Groups[answer.Group].Spent == result.Charged, "an answered group records what it spent");
        }
        else if (action == 5)
        {
            const AssessmentError error = FinishAssessment(state);
            Check(error == AssessmentError::None ? state.Finished : state == before, "handing in works once, then changes nothing");
        }
        else if (action == 6 && rng.Below(4) == 0 && !state.PaperId.empty())
        {
            Check(CancelAssessment(state) == AssessmentError::None && state == AssessmentState{}, "cancelling returns to idle");
        }

        Check(IsAssessmentStateValid(state), "every command leaves a valid state");
        if (wasFinished)
            Check(state == before || state == AssessmentState{}, "a finished paper only ever goes idle");
        ExpectRoundTrip(state);
        hash = Hash(hash, SerialiseAssessment(state));

        // Resume from the save: the rest of the sitting continues from the text alone.
        if (rng.Below(5) == 0)
            state = DeserialiseAssessment(SerialiseAssessment(state)).State;

        if (state.Finished)
        {
            const AssessmentMarking marking = MarkAssessment(paper, state);
            Check(marking.Ok(), "a finished paper is marked");
            std::int32_t awarded = 0;
            std::int32_t spent = state.Surveyed ? AssessmentSurveyUnits : 0;
            for (std::size_t g = 0; g < marking.Groups.size(); ++g)
            {
                const AssessmentGroupMark& mark = marking.Groups[g];
                awarded += mark.Awarded;
                spent += mark.Spent;
                Check(mark.Awarded >= 0 && mark.Awarded <= mark.Marks, "marks stay within the group's");
                if (state.Groups[g].Answered)
                    Check(mark.Failures == AssessmentKindsOf(mark.Structure, mark.Native) && mark.Grade != AssessmentGrade::Unattempted,
                          "an answered group's kinds are its native bits mapped");
                else
                    Check(mark.Grade == AssessmentGrade::Unattempted && mark.Failures == AssessmentFailWastedTime && mark.Awarded == 0,
                          "an unanswered group is unfinished");
            }
            Check(awarded == marking.Total && marking.Total <= marking.MaxTotal && marking.MaxTotal == 100, "the total is the sum of the groups");
            Check(spent + marking.UnitsUnused == AssessmentPaperUnits && marking.UnitsSpent == spent, "every unit is spent or unused");
            Check(SurveyAssessment(state, paper).Error == AssessmentError::Finished &&
                      ApplyAssessmentAnswer(state, Answer(0, AssessmentMode::Work, false, false)).Error == AssessmentError::Finished,
                  "nothing works on a finished paper");
        }
        else if (state.Active())
        {
            std::int32_t spent = state.Surveyed ? AssessmentSurveyUnits : 0;
            for (const AssessmentGroupState& group : state.Groups)
                spent += group.Spent;
            Check(state.UnitsLeft == AssessmentPaperUnits - spent && AssessmentUnallocated(state) >= 0, "the units left are the paper less what was spent");
        }
    }
    return hash;
}
} // namespace

void TestSaveAndLoad()
{
    const AssessmentPaperDef& paper = *TestWorld->Paper;

    const AssessmentState idle;
    Check(SerialiseAssessment(idle) == "pw.assessment/1\npaper=-\nend\n", "an idle state is three lines");
    ExpectRoundTrip(idle);
    ExpectEveryTruncationRefused(SerialiseAssessment(idle));

    // A paper saved halfway through: surveyed, promised, two groups answered.
    AssessmentState state = Begun(MixedSkills(paper));
    Check(state.Groups[0].Known && state.Groups[1].Shaky && state.Groups[2].Known && !state.Groups[3].Known && !state.Groups[3].Shaky && state.Groups[4].Shaky,
          "the character is practised, shaky and without, by method");
    Check(SurveyAssessment(state, paper).Ok(), "surveyed");
    Check(AllocateAssessmentUnits(state, 0, 3) == AssessmentError::None && AllocateAssessmentUnits(state, 1, 5) == AssessmentError::None &&
              AllocateAssessmentUnits(state, 3, 4) == AssessmentError::None,
          "units are promised");
    Check(Play(state, Answer(0, AssessmentMode::Work, true, false)).Ok(), "the first group is answered");
    Check(AllocateAndPlay(state, Answer(2, AssessmentMode::Work, false, true)).Ok(), "the third group is answered");
    const std::string text = SerialiseAssessment(state);
    ExpectRoundTrip(state);
    ExpectEveryTruncationRefused(text);

    // The loaded paper finishes exactly as the original does, and an answered group is answered once.
    AssessmentState loaded = DeserialiseAssessment(text).State;
    Check(loaded == state && loaded.Active(), "the paper resumes in the same place");
    Check(ApplyAssessmentAnswer(loaded, Answer(0, AssessmentMode::Work, true, false)).Error == AssessmentError::AlreadyAnswered, "a group answered before the save is not answered again");
    for (AssessmentState* side : {&state, &loaded})
    {
        Check(Play(*side, Answer(1, AssessmentMode::Work, true, false)).Ok(), "the second group is answered after the resume");
        Check(AllocateAndPlay(*side, Answer(3, AssessmentMode::Guess, false, false)).Ok(), "the fourth is guessed");
        Check(AllocateAndPlay(*side, Answer(4, AssessmentMode::Work, false, false)).Finished, "the last answer hands the paper in");
    }
    Check(state == loaded && state.Finished, "the original and the resumed paper end in the same state");
    Check(MarkAssessment(paper, state) == MarkAssessment(paper, loaded), "and are marked the same");
    ExpectRoundTrip(loaded);
    ExpectEveryTruncationRefused(SerialiseAssessment(loaded));

    // A save is exactly its text: damage of any kind is refused.
    Check(!DeserialiseAssessment(text + "x").Ok() && !DeserialiseAssessment(text + "end\n").Ok() && !DeserialiseAssessment("\n" + text).Ok(),
          "text after the end is refused");
    std::string crlf = text;
    for (std::size_t at = crlf.find('\n'); at != std::string::npos; at = crlf.find('\n', at + 2))
        crlf.replace(at, 1, "\r\n");
    Check(!DeserialiseAssessment(crlf).Ok(), "other line endings are refused");
    Check(SerialiseAssessment([&] {
              AssessmentState invalid = state;
              invalid.UnitsLeft += 1;
              return invalid;
          }()).empty(),
          "an inconsistent state does not serialise");

    const std::vector<std::string> lines = SplitLines(text);
    Check(lines.size() == 10 && lines[0] == "pw.assessment/1" && lines[3] == "groups=5" && lines[9] == "end", "the layout is a header, a paper, a state, a count, five groups and an end");
    struct LineEdit
    {
        std::size_t Index;
        const char* Replacement;
        const char* Why;
    };
    const LineEdit edits[] = {
        {0, "pw.assessment/2", "an unknown version"},
        {1, "paper=Not An Id", "a malformed paper ID"},
        {1, "paper=", "an empty paper ID"},
        {1, "paper=-", "no paper but a state"},
        {2, "state=1,0,016", "a leading zero"},
        {2, "state=2,0,14", "a flag that is not 0 or 1"},
        {2, "state=1,0,-1", "a negative count"},
        {2, "state=1,0,13", "units that do not add up"},
        {2, "state=1,0", "a missing field"},
        {2, "state=1,0,14,0", "an extra field"},
        {3, "groups=4", "too few groups"},
        {3, "groups=6", "too many groups"},
        {3, "groups=05", "a leading zero in the count"},
        {4, "group=1,1,0,0,work,0,0,0", "a method both practised and shaky"},
        {4, "group=0,0,19,0,work,0,0,0", "an allocation beyond the paper"},
        {4, "group=0,0,0,1,work,0,0,0", "an answered group that spent nothing"},
        {4, "group=0,0,0,0,guess,0,0,0", "an unanswered group with an answer"},
        {4, "group=0,0,0,0,work,0,0,1", "an unanswered group that spent"},
        {4, "group=0,0,0,0,walk,0,0,0", "an unknown mode"},
        {4, "group=0,0,0,0,work,0,0", "a missing field"},
        {4, "group=0,0,0,0,work,0,0,0,0", "an extra field"},
        {4, "group=0,0,0,0,work,3,0,0", "a flag that is not 0 or 1"},
        {4, "group 0,0,0,0,work,0,0,0", "a missing equals sign"},
        {9, "stop", "no end line"}};
    for (const LineEdit& edit : edits)
    {
        std::vector<std::string> damaged = lines;
        damaged[edit.Index] = edit.Replacement;
        Check(!DeserialiseAssessment(JoinLines(damaged)).Ok(), edit.Why);
    }
    std::vector<std::string> fewer = lines;
    fewer.erase(fewer.begin() + 5);
    Check(!DeserialiseAssessment(JoinLines(fewer)).Ok(), "a missing group line is refused");
    std::vector<std::string> more = lines;
    more.insert(more.begin() + 5, lines[5]);
    Check(!DeserialiseAssessment(JoinLines(more)).Ok(), "an extra group line is refused");
    Check(DeserialiseAssessment(JoinLines(lines)).Ok(), "the untouched lines still read");
}

void TestRandomSequences()
{
    // The same seed gives the same sitting, state for state.
    Check(RunSequence(0x1234ABCDULL) == RunSequence(0x1234ABCDULL), "a random sitting is deterministic");
    std::uint64_t seeds = 0;
    for (std::uint64_t seed = 1; seed <= 150; ++seed)
        seeds ^= RunSequence(seed * 0x9E3779B97F4A7C15ULL);
    Check(seeds != 0, "a hundred and fifty random sittings run");
}
