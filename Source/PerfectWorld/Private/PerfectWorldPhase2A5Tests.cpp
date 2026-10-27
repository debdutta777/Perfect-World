#include "UI/SPWStudyDesk.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "UI/SPWAcademicAssessment.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
using namespace PerfectWorld;

/** Real authored paper and structures, fake world boundary. This deliberately does
 * not stand in for the integrator's reducer/time/persistence acceptance tests. */
class FA5FakeWorld final : public PWA5::IStudyWorld
{
public:
    SchoolWorkState State;
    AssessmentCatalogue Papers;
    AssessmentSkills Skills;
    ActivityCatalogue Activities;
    GameInstant Now = 200;
    int32 Applied = 0;
    int32 PracticeStarted = 0;
    bool bAtHome = true;
    bool bReadable = true;

    bool Init(FAutomationTestBase& Test)
    {
        const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Content")));
        TArray<FString> Paths;
        IFileManager::Get().FindFilesRecursive(Paths, *Root, TEXT("*.json"), true, false);
        Paths.Sort();
        std::vector<ContentFile> Files;
        for (FString Path : Paths)
        {
            FPaths::NormalizeFilename(Path);
            FString Text;
            if (!FFileHelper::LoadFileToString(Text, *Path)) { Test.AddError(TEXT("A5 content unreadable")); return false; }
            Files.push_back({std::string(TCHAR_TO_UTF8(*Path)), std::string(TCHAR_TO_UTF8(*Text))});
        }
        const auto Db = LoadContent(Files);
        if (!Test.TestTrue(TEXT("real content available"), !Paths.IsEmpty() && Db.Ok())) return false;
        const auto Model = LoadModelCatalogue(Db.Db);
        const auto Explain = LoadExplainCatalogue(Db.Db);
        const auto Investigate = LoadInvestigateCatalogue(Db.Db);
        const auto Coordinate = LoadCoordinateCatalogue(Db.Db);
        if (!Test.TestTrue(TEXT("structure catalogues load"), Model.Ok() && Explain.Ok() && Investigate.Ok() && Coordinate.Ok())) return false;
        const auto Loaded = LoadAssessmentCatalogue(Db.Db, {&Model.Catalogue, &Explain.Catalogue, &Investigate.Catalogue, &Coordinate.Catalogue});
        const auto LoadedActivities = LoadActivityCatalogue(Db.Db);
        if (!Test.TestTrue(TEXT("assessment and activities load"), Loaded.Ok() && LoadedActivities.Ok())) return false;
        Papers = Loaded.Catalogue;
        Activities = LoadedActivities.Catalogue;
        const auto* Paper = Papers.FindPaper("assessment_paper.xi_half_yearly_mixed");
        if (!Test.TestNotNull(TEXT("the existing eighteen-unit paper"), Paper)) return false;
        // Duplicate skill IDs are invalid; the real paper has two model questions.
        for (const auto& Question : Paper->Questions)
        {
            bool bFound = false;
            for (const auto& Skill : Skills) bFound |= Skill.SkillId == Question.SkillId;
            if (!bFound) Skills.push_back({Question.SkillId, 200000});
        }
        SchoolWorkBrief Brief{"school.work.a5", Paper->Id, SchoolWorkKind::Homework, 0, 100,
            "phase2.a5.homework.title", "phase2.a5.homework.brief", "phase2.a5.homework.criteria", "phase2.a5.homework.recovery"};
        return Test.TestTrue(TEXT("homework issued by system, not widget"), IssueSchoolWork(State, Brief, Papers, 0).Ok());
    }

    bool CanUseDesk(FText& OutReason) const override
    {
        OutReason = FText::FromString(TEXT("Go home to use the desk."));
        return bAtHome;
    }
    bool ReadRecord(SchoolAcademicRecordView& Out, FText& OutReason) const override
    {
        if (!bReadable) { OutReason = FText::FromString(TEXT("Your record is unavailable.")); return false; }
        Out = QuerySchoolAcademicRecord(State, Papers, Now);
        return true;
    }
    SchoolWorkResult Apply(const SchoolWorkCommand& Command) override
    {
        ++Applied;
        if (!bAtHome)
        {
            SchoolWorkResult Refused;
            Refused.Issue.Code = "school.work.unavailable";
            return Refused;
        }
        const auto Result = ApplySchoolWork(State, Papers, Command, Skills, Now);
        if (Result.Ok()) Now += Result.WorldMinutes;
        return Result;
    }
    AssessmentQuote Quote(std::string_view Id, const AssessmentAnswer& Answer) const override
    { return QuoteSchoolWorkAnswer(State, Id, Answer); }
    FText Text(std::string_view, const FText& Fallback) const override { return Fallback; }
    FText GroupPrompt(std::string_view, std::size_t Group) const override
    {
        const auto* Paper = Papers.FindPaper("assessment_paper.xi_half_yearly_mixed");
        return Paper && Group < Paper->Questions.size() ? FText::FromString(UTF8_TO_TCHAR(Paper->Questions[Group].Name.c_str())) : FText::GetEmpty();
    }
    TArray<PWA5::FPracticeOffer> PracticeOffers() const override
    {
        const auto* Activity = Activities.Find("activity.study_at_home");
        if (!Activity) return {};
        bool bBusy = false;
        for (const auto& Work : State.Records) bBusy |= !Work.SubmittedAt && !Work.Paper.PaperId.empty();
        return {PWA5::StudyOffer(*Activity, FText::FromString(TEXT("Use the existing study activity and practice window.")), bAtHome && !bBusy)};
    }
    SchoolIssue StartPractice(std::string_view Id) override
    {
        for (const auto& Offer : PracticeOffers())
            if (Offer.Id == Id && Offer.bAvailable) { ++PracticeStarted; return {}; }
        return {"school.work.busy", {}};
    }
};

void A5Walk(const TSharedRef<SWidget>& Widget, const TFunctionRef<void(const TSharedRef<SWidget>&)>& Visit)
{
    if (!Widget->GetVisibility().IsVisible()) return;
    Visit(Widget);
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index) A5Walk(Children->GetChildAt(Index), Visit);
}

TArray<FString> A5Texts(const TSharedRef<SWidget>& Root)
{
    Root->SlatePrepass(1.0f);
    TArray<FString> Texts;
    A5Walk(Root, [&Texts](const TSharedRef<SWidget>& Widget)
    {
        if (Widget->GetTypeAsString() == TEXT("STextBlock")) Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
    });
    return Texts;
}

bool A5Has(const TSharedRef<SWidget>& Root, const FString& Fragment)
{
    for (const auto& Text : A5Texts(Root)) if (Text.Contains(Fragment)) return true;
    return false;
}

bool A5Click(const TSharedRef<SWidget>& Root, const FString& Label)
{
    Root->SlatePrepass(1.0f);
    TSharedPtr<SButton> Button;
    A5Walk(Root, [&Button, &Label](const TSharedRef<SWidget>& Widget)
    {
        if (!Button.IsValid() && Widget->GetTypeAsString() == TEXT("SButton") && A5Texts(Widget).Contains(Label))
            Button = StaticCastSharedRef<SButton>(Widget);
    });
    if (!Button.IsValid() || !Button->IsEnabled()) return false;
    Button->SimulateClick();
    return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA5DeskTest, "PerfectWorld.Phase2.A5.DeskBridgeAndRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA5DeskTest::RunTest(const FString& Parameters)
{
    const auto Fake = MakeShared<FA5FakeWorld>();
    if (!Fake->Init(*this)) return false;
    int32 Opened = 0;
    FText Why;
    const auto Open = FSimpleDelegate::CreateLambda([&Opened]() { ++Opened; });
    TestTrue(TEXT("desk interaction opens the host"), PWA5::InteractStudyDesk(Fake, Open, Why));
    Fake->bAtHome = false;
    TestFalse(TEXT("desk bridge enforces home context"), PWA5::InteractStudyDesk(Fake, Open, Why));
    TestEqual(TEXT("refused interaction never opens"), Opened, 1);
    Fake->bAtHome = true;
    std::string Selected;
    const auto Desk = SNew(SPWStudyDesk).World(Fake).Space(FVector2D(853, 480))
        .OnOpenPaper(FA5OpenPaper::CreateLambda([&Selected](const std::string& Id) { Selected = Id; }));
    TestTrue(TEXT("missed work recovery is visible"), A5Has(Desk, TEXT("Deadline missed")));
    TestTrue(TEXT("deadline and availability shown"), A5Has(Desk, TEXT("Due:")) && A5Has(Desk, TEXT("Available:")));
    TestTrue(TEXT("open brief forwards stable work ID"), A5Click(Desk, TEXT("Open brief")) && Selected == "school.work.a5");
    TestEqual(TEXT("reading and opening do not submit or charge"), Fake->Applied, 0);
    TestEqual(TEXT("clock unchanged"), Fake->Now, GameInstant(200));
    TestTrue(TEXT("authored study duration reused"), A5Click(Desk, TEXT("Study at home (120 minutes)")));
    TestEqual(TEXT("practice dispatch once"), Fake->PracticeStarted, 1);
    Fake->bReadable = false;
    Desk->Refresh();
    TestTrue(TEXT("unavailable record explanation"), A5Has(Desk, TEXT("Your record is unavailable.")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA5PaperTest, "PerfectWorld.Phase2.A5.PaperCommandsAndExactlyOnceMarks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA5PaperTest::RunTest(const FString& Parameters)
{
    const auto Fake = MakeShared<FA5FakeWorld>();
    if (!Fake->Init(*this)) return false;
    const auto Panel = SNew(SPWAcademicAssessment).World(Fake).WorkId("school.work.a5");
    TestTrue(TEXT("explicit block before begin"), A5Has(Panel, TEXT("reserves 90 minutes")));
    TestTrue(TEXT("begin through actual button"), A5Click(Panel, TEXT("Begin paper (90-minute block)")));
    TestTrue(TEXT("paper has eighteen units"), A5Has(Panel, TEXT("Units left: 18 / 18")));
    TestTrue(TEXT("survey quotes one unit"), A5Click(Panel, TEXT("Survey paper (1 unit)")));
    TestTrue(TEXT("survey familiarity and estimates visible"), A5Has(Panel, TEXT("Practised")) && A5Has(Panel, TEXT("Estimated work:")));
    TestEqual(TEXT("paper still pins world clock"), Fake->Now, GameInstant(200));
    TestFalse(TEXT("practice unavailable during paper"), Fake->PracticeOffers()[0].bAvailable);
    // Exercise real Slate allocation, answer method and confirmation callbacks.
    for (int32 Unit = 0; Unit < 3; ++Unit) TestTrue(TEXT("allocation increment"), A5Click(Panel, TEXT("Allocate one more unit")));
    TestTrue(TEXT("verification toggle"), A5Click(Panel, TEXT("Verification: no")));
    TestTrue(TEXT("exact answer cost presented"), A5Has(Panel, TEXT("Answer cost: 3 units")));
    TestTrue(TEXT("commit through button"), A5Click(Panel, TEXT("Commit this group answer")));
    SchoolWorkCommand Command;
    for (std::size_t Group = 1; Group < AssessmentGroupCount; ++Group)
    {
        Command.Kind = SchoolWorkActionKind::Allocate; Command.Group = Group; Command.Units = 3;
        TestTrue(TEXT("allocate callback"), Panel->Send(Command).Ok());
        Command.Kind = SchoolWorkActionKind::Answer; Command.Answer = {Group, AssessmentMode::Work, true, false};
        const auto Quote = Fake->Quote("school.work.a5", Command.Answer);
        const auto Result = Panel->Send(Command);
        TestTrue(TEXT("authoritative quote equals charge"), Quote.Ok() && Result.Ok() && Quote.Units == Result.ChargedUnits);
    }
    TestTrue(TEXT("submit through button"), A5Click(Panel, TEXT("Submit paper (90 world minutes)")));
    TestTrue(TEXT("academic marks labelled separately"), A5Has(Panel, TEXT("Academic marks: 100 / 100. Separate from tower rank.")));
    TestTrue(TEXT("diagnostic advice visible"), A5Has(Panel, TEXT("Sound method")));
    TestEqual(TEXT("paid once"), Fake->Now, GameInstant(290));
    const auto Before = Fake->State;
    Command.Kind = SchoolWorkActionKind::Submit;
    const auto Duplicate = Panel->Send(Command);
    TestFalse(TEXT("stale submit callback refused"), Duplicate.Ok());
    TestTrue(TEXT("duplicate leaves record intact"), Fake->State == Before);
    TestEqual(TEXT("duplicate charges no time"), Fake->Now, GameInstant(290));
    TestTrue(TEXT("late result preserves academic mark"), QuerySchoolAcademicRecord(Fake->State, Fake->Papers, Fake->Now).Work.front().LateSubmission);
    const auto Desk = SNew(SPWStudyDesk).World(Fake)
        .OnOpenPaper(FA5OpenPaper::CreateLambda([](const std::string&) {}));
    TestTrue(TEXT("submitted work opens feedback"), A5Has(Desk, TEXT("Read academic feedback")));
    TestFalse(TEXT("submitted work has no begin button"), A5Click(Panel, TEXT("Begin paper (90-minute block)")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA5CancelTest, "PerfectWorld.Phase2.A5.CancelRestartAndUnfinishedFeedback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA5CancelTest::RunTest(const FString& Parameters)
{
    const auto Fake = MakeShared<FA5FakeWorld>();
    if (!Fake->Init(*this)) return false;
    Fake->Skills.clear();
    int32 Closed = 0;
    const auto Panel = SNew(SPWAcademicAssessment).World(Fake).WorkId("school.work.a5")
        .OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));
    A5Click(Panel, TEXT("Begin paper (90-minute block)"));
    A5Click(Panel, TEXT("Survey paper (1 unit)"));
    TestTrue(TEXT("unfamiliar estimates disclosed"), A5Has(Panel, TEXT("Unfamiliar")));
    const auto BeforeBack = Fake->State;
    Panel->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
    TestEqual(TEXT("escape requests host close"), Closed, 1);
    TestTrue(TEXT("back retains draft"), Fake->State == BeforeBack);
    Panel->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Gamepad_FaceButton_Right, FModifierKeysState(), 0, true, 0, 0));
    TestEqual(TEXT("held controller back does not repeat close"), Closed, 1);
    Panel->OnKeyDown(FGeometry(), FKeyEvent(EKeys::Gamepad_FaceButton_Right, FModifierKeysState(), 0, false, 0, 0));
    TestEqual(TEXT("controller back requests close"), Closed, 2);
    TestTrue(TEXT("cancel discloses actual spent time"), A5Click(Panel, TEXT("Discard draft (5 world minutes)")));
    TestEqual(TEXT("cancel pays survey only"), Fake->Now, GameInstant(205));
    TestTrue(TEXT("recovery can restart"), A5Click(Panel, TEXT("Begin paper (90-minute block)")));
    TestTrue(TEXT("unfinished submission allowed"), A5Click(Panel, TEXT("Submit paper (90 world minutes)")));
    TestTrue(TEXT("unfinished feedback points to planning"), A5Has(Panel, TEXT("Unfinished work")));
    TestTrue(TEXT("zero academic marks are not tower rank"), A5Has(Panel, TEXT("Academic marks: 0 / 100")));
    Fake->State.Records.clear();
    const auto Empty = SNew(SPWStudyDesk).World(Fake);
    TestTrue(TEXT("no homework still offers practice"), A5Has(Empty, TEXT("No work has been issued")) &&
        A5Click(Empty, TEXT("Study at home (120 minutes)")));
    Panel->Refresh();
    TestTrue(TEXT("removed work has safe fallback"), A5Has(Panel, TEXT("no longer available")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA5LayoutTest, "PerfectWorld.Phase2.A5.LayoutAndDiagnostics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA5LayoutTest::RunTest(const FString& Parameters)
{
    const auto Fake = MakeShared<FA5FakeWorld>();
    if (!Fake->Init(*this)) return false;
    for (const FVector2D Size : {FVector2D(853, 480), FVector2D(1280, 720), FVector2D(1920, 1080)})
        for (const float Scale : {1.0f, 1.5f})
        {
            const FVector2D Logical = Size / Scale;
            const auto Desk = SNew(SPWStudyDesk).World(Fake).Space(Logical);
            const auto Panel = SNew(SPWAcademicAssessment).World(Fake).WorkId("school.work.a5").Space(Logical);
            Desk->SlatePrepass(Scale); Panel->SlatePrepass(Scale);
            TestTrue(TEXT("desk fits logical screen"), Desk->GetDesiredSize().X <= Logical.X && Desk->GetDesiredSize().Y <= Logical.Y);
            TestTrue(TEXT("paper fits logical screen"), Panel->GetDesiredSize().X <= Logical.X && Panel->GetDesiredSize().Y <= Logical.Y);
        }
    AssessmentGroupMark Mark;
    Mark.Grade = AssessmentGrade::Unsound;
    Mark.Failures = AssessmentFailWrongModel | AssessmentFailUnsupported | AssessmentFailWastedTime | AssessmentFailUnclear;
    const FString Advice = PWA5::Diagnostics(Mark).ToString();
    TestTrue(TEXT("all diagnostic kinds yield next actions"), Advice.Contains(TEXT("concept")) && Advice.Contains(TEXT("evidence")) && Advice.Contains(TEXT("time")) && Advice.Contains(TEXT("limits")));
    return true;
}

#endif
