#include "PerfectWorldSchoolLive.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PerfectWorldCharacterCreatorSubsystem.h"
#include "PerfectWorldContentSubsystem.h"
#include "PerfectWorldLaneLifeSubsystem.h"
#include "PerfectWorldSchoolGate.h"

#define LOCTEXT_NAMESPACE "PerfectWorldSchoolLive"

// S03: content, the unrouted-state port, the school-day bridge and the lesson bridge. Study and planner bridges: PerfectWorldSchoolLiveWork.cpp.

namespace PWSchoolLive
{
FString StreamOf(UWorld& World)
{
    FString Stream = TEXT("science");
    if (const UGameInstance* Instance = World.GetGameInstance())
    {
        if (const auto* Creator = Instance->GetSubsystem<UPerfectWorldCharacterCreatorSubsystem>())
        {
            for (const auto& Row : Creator->GetSchoolStreams())
            {
                if (Row.bSelected)
                {
                    Stream = Row.Id.ToString();
                }
            }
        }
    }
    return Stream;
}

const FS03Content* SharedContent(UWorld& World, FString& OutWhy)
{
    static TMap<FString, TUniquePtr<FS03Content>> S03Cache;
    const FString Stream = StreamOf(World);
    if (const TUniquePtr<FS03Content>* Found = S03Cache.Find(Stream))
    {
        return Found->Get();
    }
    const PWSchool::FScSchoolContent* School = PWSchool::ScSharedSchoolContent(World, Stream, OutWhy);
    const UGameInstance* Instance = World.GetGameInstance();
    const UPWContentSubsystem* Content = Instance != nullptr ? Instance->GetSubsystem<UPWContentSubsystem>() : nullptr;
    const PerfectWorld::ContentDb* Db = Content != nullptr ? Content->GetDb() : nullptr;
    if (School == nullptr || Db == nullptr)
    {
        if (OutWhy.IsEmpty())
        {
            OutWhy = TEXT("the validated content database is not available");
        }
        return nullptr;
    }
    TUniquePtr<FS03Content> Built = MakeUnique<FS03Content>();
    Built->Stream = Stream;
    auto Model = PerfectWorld::LoadModelCatalogue(*Db);
    auto Explain = PerfectWorld::LoadExplainCatalogue(*Db);
    auto Investigate = PerfectWorld::LoadInvestigateCatalogue(*Db);
    auto Coordinate = PerfectWorld::LoadCoordinateCatalogue(*Db);
    if (!Model.Ok() || !Explain.Ok() || !Investigate.Ok() || !Coordinate.Ok())
    {
        OutWhy = TEXT("a structure catalogue did not load");
        return nullptr;
    }
    Built->Model = MoveTemp(Model.Catalogue);
    Built->Explain = MoveTemp(Explain.Catalogue);
    Built->Investigate = MoveTemp(Investigate.Catalogue);
    Built->Coordinate = MoveTemp(Coordinate.Catalogue);
    Built->Cats.Day = School->Binding;
    Built->Cats.Structures = {&Built->Model, &Built->Explain, &Built->Investigate, &Built->Coordinate};
    auto Papers = PerfectWorld::LoadAssessmentCatalogue(*Db, Built->Cats.Structures);
    if (!Papers.Ok())
    {
        OutWhy = TEXT("the assessment papers did not load");
        return nullptr;
    }
    Built->Papers = MoveTemp(Papers.Catalogue);
    FString Text;
    const FString TextPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Text/en/phase2_a1.json"));
    if (!FFileHelper::LoadFileToString(Text, *TextPath))
    {
        OutWhy = TEXT("Data/Text/en/phase2_a1.json cannot be read");
        return nullptr;
    }
    auto Lessons = PerfectWorld::LoadSchoolLessonContent(*Db, PerfectWorld::ContentFile{std::string(TCHAR_TO_UTF8(*TextPath)), std::string(TCHAR_TO_UTF8(*Text))});
    if (!Lessons.Ok())
    {
        OutWhy = TEXT("the school lesson catalogue was refused: ") + PWLaneLife::ToFString(Lessons.Issues.empty() ? std::string() : Lessons.Issues.front().Message);
        return nullptr;
    }
    Built->Lessons = MoveTemp(Lessons.Catalogue);
    return S03Cache.Add(Stream, MoveTemp(Built)).Get();
}

// ---- School state of the world ----

bool CommandsRouted()
{
    return true; // S05: the reducer routes school.lesson.*, school.work.apply and school.week.* (S04)
}

const PerfectWorld::SchoolSessionsState& SessionsOf(const PerfectWorld::WorldState& State)
{
    return State.SchoolSessions;
}

const PerfectWorld::SchoolWorkState& WorkOf(const PerfectWorld::WorldState& State)
{
    return State.SchoolWork;
}

const PerfectWorld::SchoolWeekState& StandingOf(const PerfectWorld::WorldState& State)
{
    return State.SchoolWeek;
}

const PerfectWorld::SchoolLessonBinding* BindingForNow(UWorld& World)
{
    FString Why;
    const FS03Content* Content = SharedContent(World, Why);
    const UPWLaneLifeSubsystem* Life = UPWLaneLifeSubsystem::Get(&World);
    if (Content == nullptr || Life == nullptr)
    {
        return nullptr;
    }
    const PerfectWorld::WorldState& State = Life->GetWorldState();
    const PerfectWorld::SchoolDayView View = PerfectWorld::BuildSchoolDayView(Content->Lessons, SessionsOf(State), State.DayLoop, Content->Cats);
    if (!View.Ok() || View.Rows.empty())
    {
        return nullptr;
    }
    const PerfectWorld::SchoolDayRow* Pick = nullptr;
    for (const PerfectWorld::SchoolDayRow& Row : View.Rows)
    {
        if (Row.Status == PerfectWorld::SchoolDayRowStatus::Active || Row.Status == PerfectWorld::SchoolDayRowStatus::Open)
        {
            Pick = &Row;
            break;
        }
    }
    for (const PerfectWorld::SchoolDayRow& Row : View.Rows)
    {
        if (Pick == nullptr && Row.Status == PerfectWorld::SchoolDayRowStatus::Upcoming)
        {
            Pick = &Row;
        }
    }
    if (Pick == nullptr)
    {
        Pick = &View.Rows.front();
    }
    return PerfectWorld::FindSchoolLessonBinding(Content->Lessons, Pick->BindingId);
}

namespace
{
const UPWLaneLifeSubsystem* S03Life(const TWeakObjectPtr<UWorld>& World)
{
    UWorld* Live = World.Get();
    return Live != nullptr ? UPWLaneLifeSubsystem::Get(Live) : nullptr;
}

/** The core's refusal in words: A1's explanation for a school.session.<reason> code, else a neutral sentence. Never a code. */
FText RefusalWords(const FS03Content& Content, const std::string& Code)
{
    constexpr std::string_view Prefix = "school.session.";
    if (Code.rfind(Prefix.data(), 0) == 0)
    {
        if (const PerfectWorld::SchoolLessonExplanation* Explanation = PerfectWorld::FindSchoolLessonExplanation(Content.Lessons, Code.substr(Prefix.size())))
        {
            if (const std::string* Found = PerfectWorld::FindSchoolLessonText(Content.Lessons, Explanation->TextKey))
            {
                return PWLaneLife::ToText(*Found);
            }
        }
    }
    if (Code == "school.lesson_open" || Code == "school.paper_open")
    {
        return LOCTEXT("S05Held", "Finish your lesson or paper first.");
    }
    return LOCTEXT("S05Neutral", "The lesson cannot be opened right now.");
}

/** The school-day panel's bridge. Reads are the portable day view; opening is refused until the reducer routes school.lesson.open. */
class FS03DayWorld final : public IPWSchoolDayWorld
{
public:
    explicit FS03DayWorld(UWorld& InWorld) : World(&InWorld) {}

    virtual bool ReadModel(FPWSchoolDayModel& OutModel, FString& OutWhy) override
    {
        UWorld* Live = World.Get();
        const UPWLaneLifeSubsystem* Life = S03Life(World);
        if (Live == nullptr || Life == nullptr)
        {
            OutWhy = TEXT("there is no lane-life world");
            return false;
        }
        const FS03Content* Content = SharedContent(*Live, OutWhy);
        if (Content == nullptr)
        {
            return false;
        }
        const PerfectWorld::WorldState& State = Life->GetWorldState();
        const PerfectWorld::SchoolDayView View = PerfectWorld::BuildSchoolDayView(Content->Lessons, SessionsOf(State), State.DayLoop, Content->Cats);
        if (!View.Ok())
        {
            OutWhy = PWLaneLife::ToFString(View.Issue);
            return false;
        }
        OutModel = PWSchoolDay::BuildModel(View, Content->Lessons);
        return true;
    }

    virtual FText OpenLesson(const FString& BindingId, PerfectWorld::SchoolLessonMode Mode, const FString& GoalName) override
    {
        UWorld* Live = World.Get();
        UPWLaneLifeSubsystem* Life = Live != nullptr ? UPWLaneLifeSubsystem::Get(Live) : nullptr;
        FString Why;
        const FS03Content* Content = Live != nullptr ? SharedContent(*Live, Why) : nullptr;
        if (Life == nullptr || Content == nullptr)
        {
            return LOCTEXT("S05NoSchool", "The school is not available right now.");
        }
        const std::string Id = TCHAR_TO_UTF8(*BindingId);
        const PerfectWorld::SchoolLessonBinding* Binding = PerfectWorld::FindSchoolLessonBinding(Content->Lessons, Id);
        if (Binding == nullptr)
        {
            return LOCTEXT("S05NoBinding", "That lesson is not on the timetable.");
        }
        PerfectWorld::SchoolLessonGoal Goal = PerfectWorld::ModelGoal::UnderstandModel;
        if (Mode == PerfectWorld::SchoolLessonMode::Foreground)
        {
            // The goal comes from the content's own options, never from the widget: an unknown name is refused here.
            const std::string Name = TCHAR_TO_UTF8(*GoalName);
            bool bFound = false;
            for (const PerfectWorld::SchoolLessonGoalOption& Option : PerfectWorld::SchoolLessonGoalOptions(Content->Lessons, Binding->Structure))
            {
                if (Option.Name == Name)
                {
                    Goal = Option.Goal;
                    bFound = true;
                    break;
                }
            }
            if (!bFound)
            {
                return LOCTEXT("S05NoGoal", "Choose what you want from this lesson first.");
            }
        }
        PerfectWorld::SchoolLessonOpen Open;
        Open.BindingId = Id;
        Open.Mode = Mode;
        Open.Goal = Goal;
        const PerfectWorld::CommandResult Done = Life->SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, Open});
        if (Done.Accepted)
        {
            return FText::GetEmpty();
        }
        return RefusalWords(*Content, Done.Why.Code);
    }

private:
    TWeakObjectPtr<UWorld> World;
};

/** The lesson shell's bridge: every read is a portable query; every command is a school.lesson.* reducer command (S05). */
class FS03LessonBridge final : public PWClassLesson::IBridge
{
public:
    FS03LessonBridge(UWorld& InWorld, PerfectWorld::SchoolLessonBinding InBinding) : World(&InWorld), Binding(MoveTemp(InBinding)) {}

    virtual const PerfectWorld::SchoolLessonBinding& GetBinding() const override { return Binding; }
    virtual const PerfectWorld::SchoolSessionCatalogues& GetCatalogues() const override
    {
        FString Why;
        const FS03Content* Content = World.IsValid() ? SharedContent(*World.Get(), Why) : nullptr;
        return Content != nullptr ? Content->Cats : S03NoCats;
    }
    virtual PerfectWorld::SchoolPeriodView QueryPeriod() const override
    {
        const UPWLaneLifeSubsystem* Life = S03Life(World);
        PerfectWorld::SchoolPeriodView View;
        if (Life == nullptr || !HasContent())
        {
            View.Issue = {"school.session.invalid_state", "no world"};
            return View;
        }
        const PerfectWorld::WorldState& State = Life->GetWorldState();
        return PerfectWorld::QuerySchoolPeriod(SessionsOf(State), State.DayLoop, Binding, GetCatalogues());
    }
    virtual PerfectWorld::SchoolLessonView QueryLesson() const override
    {
        const UPWLaneLifeSubsystem* Life = S03Life(World);
        return Life != nullptr ? PerfectWorld::QuerySchoolLesson(SessionsOf(Life->GetWorldState())) : PerfectWorld::SchoolLessonView{};
    }
    virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode Mode, const PerfectWorld::SchoolLessonGoal& Goal) override
    {
        PerfectWorld::SchoolLessonOpen Open;
        Open.BindingId = Binding.Id;
        Open.Mode = Mode;
        Open.Goal = Goal;
        return Submit(PerfectWorld::CommandPayload(MoveTemp(Open)));
    }
    virtual PerfectWorld::SchoolLessonQuote QuoteAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) const override
    {
        const UPWLaneLifeSubsystem* Life = S03Life(World);
        PerfectWorld::SchoolLessonQuote Quote;
        if (Life == nullptr || !HasContent())
        {
            Quote.Issue = {"school.session.invalid_state", "no world"};
            return Quote;
        }
        return PerfectWorld::QuoteSchoolLessonAction(SessionsOf(Life->GetWorldState()), GetCatalogues(), Session, Action);
    }
    virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64 Session, const PerfectWorld::SchoolLessonAction& Action) override
    {
        PerfectWorld::SchoolLessonChoose Choose;
        Choose.Session = Session;
        Choose.Action = Action;
        return Submit(PerfectWorld::CommandPayload(MoveTemp(Choose)));
    }
    virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64 Session, bool bCancel, int32 ElapsedMinutes) override
    {
        PerfectWorld::SchoolLessonClose Close;
        Close.Session = Session;
        Close.Cancel = bCancel;
        Close.ElapsedMinutes = ElapsedMinutes;
        return Submit(PerfectWorld::CommandPayload(MoveTemp(Close)));
    }
    virtual FText ResolveText(const FString& Key) const override
    {
        FString Why;
        const FS03Content* Content = World.IsValid() ? SharedContent(*World.Get(), Why) : nullptr;
        if (Content != nullptr)
        {
            if (const std::string* Found = PerfectWorld::FindSchoolLessonText(Content->Lessons, std::string(TCHAR_TO_UTF8(*Key))))
            {
                return PWLaneLife::ToText(*Found);
            }
        }
        return IBridge::ResolveText(Key);
    }

private:
    bool HasContent() const
    {
        FString Why;
        return World.IsValid() && SharedContent(*World.Get(), Why) != nullptr;
    }
    /** Submits one school.lesson.* command through the reducer and returns the sim's own result, or the core's refusal code unchanged. */
    PerfectWorld::SchoolSessionResult Submit(PerfectWorld::CommandPayload Payload) const
    {
        UWorld* Live = World.Get();
        UPWLaneLifeSubsystem* Life = Live != nullptr ? UPWLaneLifeSubsystem::Get(Live) : nullptr;
        PerfectWorld::SchoolSessionResult Result;
        if (Life == nullptr)
        {
            Result.Issue = {"school.session.invalid_state", "no world"};
            return Result;
        }
        const PerfectWorld::CommandResult Done = Life->SubmitSchoolCommand(PerfectWorld::Command{PerfectWorld::CommandSource::Player, MoveTemp(Payload)});
        if (!Done.Accepted)
        {
            Result.Issue = {Done.Why.Code, Done.Why.Message};
            return Result;
        }
        const std::optional<PerfectWorld::SchoolSessionResult>& Last = Life->GetSim().LastSchoolLessonResult();
        if (!Last.has_value())
        {
            Result.Issue = {"school.session.invalid_state", "the reducer accepted the command but published no result"};
            return Result;
        }
        return *Last;
    }

    inline static const PerfectWorld::SchoolSessionCatalogues S03NoCats{};
    TWeakObjectPtr<UWorld> World;
    PerfectWorld::SchoolLessonBinding Binding;
};
} // namespace

TSharedRef<IPWSchoolDayWorld> MakeDayWorld(UWorld& World)
{
    return MakeShared<FS03DayWorld>(World);
}

TSharedRef<PWClassLesson::IBridge> MakeLessonBridge(UWorld& World, const PerfectWorld::SchoolLessonBinding& Binding)
{
    return MakeShared<FS03LessonBridge>(World, Binding);
}
} // namespace PWSchoolLive

#undef LOCTEXT_NAMESPACE
