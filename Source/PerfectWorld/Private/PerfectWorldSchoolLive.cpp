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
FText UnwiredText()
{
    return LOCTEXT("S03Unwired", "School lessons, homework and the week plan are not connected to the world clock in this build yet, so nothing was started or recorded. Reading is unaffected.");
}

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

// ---- S03Port ----

bool CommandsRouted()
{
    return false;
}

const PerfectWorld::SchoolSessionsState& SessionsOf(const PerfectWorld::WorldState& /*State*/)
{
    static const PerfectWorld::SchoolSessionsState S03Empty;
    return S03Empty; // historical worlds start empty; becomes State.SchoolSessions when the reducer owns it
}

const PerfectWorld::SchoolWorkState& WorkOf(const PerfectWorld::WorldState& /*State*/)
{
    static const PerfectWorld::SchoolWorkState S03Empty;
    return S03Empty;
}

const PerfectWorld::SchoolWeekState& StandingOf(const PerfectWorld::WorldState& /*State*/)
{
    static const PerfectWorld::SchoolWeekState S03Empty;
    return S03Empty;
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

    virtual FText OpenLesson(const FString& /*BindingId*/, PerfectWorld::SchoolLessonMode /*Mode*/, const FString& /*GoalName*/) override
    {
        // S03Port: submit school.lesson.open here once the reducer routes it; a refusal's code goes through the phase2.a1.unavailable texts.
        return UnwiredText();
    }

private:
    TWeakObjectPtr<UWorld> World;
};

/** The lesson shell's bridge: every read is a portable query; every command is refused until routed. */
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
    virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode, const PerfectWorld::SchoolLessonGoal&) override { return Refused(); }
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
    virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64, const PerfectWorld::SchoolLessonAction&) override { return Refused(); }
    virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64, bool, int32) override { return Refused(); }
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
    static PerfectWorld::SchoolSessionResult Refused()
    {
        PerfectWorld::SchoolSessionResult Result;
        Result.Issue = {std::string(TCHAR_TO_UTF8(UnwiredCode)), "school.lesson.* is not routed through the reducer"};
        return Result;
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
