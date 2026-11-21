#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "PerfectWorldAcceptanceSchool.h"
#include "PerfectWorldCommerceText.h"
#include "PerfectWorldGuardNotice.h"
#include "UI/SPWClassModel.h"
#include "UI/SPWClassExplain.h"
#include "UI/SPWClassInvestigate.h"
#include "UI/SPWClassCoordinate.h"
#include "UI/SPWSchoolPlanner.h"
#include "UI/SPWStudyDesk.h"

namespace
{
class FP313LessonFake final : public PWClassLesson::IBridge
{
public:
    PerfectWorld::SchoolLessonBinding Binding;
    PerfectWorld::SchoolSessionCatalogues Catalogues;
    PerfectWorld::SchoolLessonView Saved;
    int32 Opens = 0;
    int32 Closes = 0;
    uint64 ClosedSession = 0;
    bool bCancelled = false;
    int32 Elapsed = -1;

    virtual const PerfectWorld::SchoolLessonBinding& GetBinding() const override { return Binding; }
    virtual const PerfectWorld::SchoolSessionCatalogues& GetCatalogues() const override { return Catalogues; }
    virtual PerfectWorld::SchoolPeriodView QueryPeriod() const override
    {
        PerfectWorld::SchoolPeriodView Period;
        Period.StartMinute = 600;
        Period.EndMinute = 645;
        return Period;
    }
    virtual PerfectWorld::SchoolLessonView QueryLesson() const override { return Saved; }
    virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode, const PerfectWorld::SchoolLessonGoal&) override
    {
        ++Opens;
        PerfectWorld::SchoolSessionResult Result;
        Result.Issue.Code = "school.lesson_open";
        return Result;
    }
    virtual PerfectWorld::SchoolLessonQuote QuoteAction(uint64, const PerfectWorld::SchoolLessonAction&) const override { return {}; }
    virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64, const PerfectWorld::SchoolLessonAction&) override { return {}; }
    virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64 Session, bool bCancel, int32 Minutes) override
    {
        ++Closes;
        ClosedSession = Session;
        bCancelled = bCancel;
        Elapsed = Minutes;
        Saved.Run.reset();
        PerfectWorld::SchoolSessionResult Result;
        Result.Session = Session;
        return Result;
    }
};

TSharedRef<SPWClassLesson> P313Screen(const TSharedRef<FP313LessonFake>& Bridge, FSimpleDelegate Back = {})
{
    switch (Bridge->Binding.Structure)
    {
    case PerfectWorld::SchoolStructure::Explain: return SNew(SPWClassExplain).Bridge(Bridge).OnClose(Back).Space(FVector2D(853, 480));
    case PerfectWorld::SchoolStructure::Investigate: return SNew(SPWClassInvestigate).Bridge(Bridge).OnClose(Back).Space(FVector2D(853, 480));
    case PerfectWorld::SchoolStructure::Coordinate: return SNew(SPWClassCoordinate).Bridge(Bridge).OnClose(Back).Space(FVector2D(853, 480));
    default: return SNew(SPWClassModel).Bridge(Bridge).OnClose(Back).Space(FVector2D(853, 480));
    }
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP313LessonResumeTest, "PerfectWorld.P313.FreshLessonScreenCanLeaveSavedRun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP313LessonResumeTest::RunTest(const FString& Parameters)
{
    for (const auto Structure : {PerfectWorld::SchoolStructure::Model, PerfectWorld::SchoolStructure::Explain,
             PerfectWorld::SchoolStructure::Investigate, PerfectWorld::SchoolStructure::Coordinate})
    {
    const auto Original = MakeShared<FP313LessonFake>();
    Original->Binding.Id = "school.lesson.p313";
    Original->Binding.Structure = Structure;
    PerfectWorld::SchoolLessonRun Run;
    Run.Session = 313;
    Run.Binding = Original->Binding;
    Run.Mode = PerfectWorld::SchoolLessonMode::Foreground;
    Original->Saved.Run = Run;
    const auto Exposure = [](auto& Value)
    {
        Value.CaseId = "case.p313"; // Active() depends on the persisted case ID, not the budget.
        Value.UnitsTotal = 9;
        Value.UnitsLeft = 5;
    };
    Exposure(Original->Saved.Model.Exposure);
    Exposure(Original->Saved.Explain.Exposure);
    Exposure(Original->Saved.Investigate.Exposure);
    Exposure(Original->Saved.Coordinate.Exposure);
    int32 Back = 0;
    const auto Before = P313Screen(Original, FSimpleDelegate::CreateLambda([&Back]() { ++Back; }));
    Before->RequestClose();
    TestTrue(TEXT("Back retains the run"), Original->Saved.Run.has_value());
    TestEqual(TEXT("Back requested once"), Back, 1);
    const auto Restored = MakeShared<FP313LessonFake>();
    Restored->Binding = Original->Binding;
    Restored->Saved = Original->Saved;
    const auto Resumed = P313Screen(Restored);
    TestEqual(TEXT("saved session is displayed"), Resumed->GetDisplayedSession(), uint64(313));
    TestEqual(TEXT("presentation submits no new open"), Restored->Opens, 0);
    TestTrue(TEXT("Leave early remains available"), Resumed->CanLeaveEarly());
    TestFalse(TEXT("first press only arms"), Resumed->PressLeaveEarly());
    TestEqual(TEXT("no close before confirmation"), Restored->Closes, 0);
    TestTrue(TEXT("second press exits"), Resumed->PressLeaveEarly());
    TestEqual(TEXT("one close"), Restored->Closes, 1);
    TestEqual(TEXT("saved session carried to close"), Restored->ClosedSession, uint64(313));
    TestTrue(TEXT("early exit flag sent"), Restored->bCancelled);
    TestEqual(TEXT("saved spent units determine elapsed minutes"), Restored->Elapsed, 20);
    TestFalse(TEXT("run released"), Restored->Saved.Run.has_value());

    const auto Finished = MakeShared<FP313LessonFake>();
    Finished->Binding = Original->Binding;
    Finished->Saved = Original->Saved;
    Finished->Saved.Run->ReadyToClose = true;
    const auto Ready = P313Screen(Finished);
    TestTrue(TEXT("a resolved saved run retains its paid Record exit"), Ready->IsActionEnabled(TEXT("record")));
    TestTrue(TEXT("Record is reachable after recreation"), Ready->PressAction(TEXT("record")));
    TestEqual(TEXT("one paid close"), Finished->Closes, 1);
    TestFalse(TEXT("resolved mastery is not cancelled"), Finished->bCancelled);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP313GuardWordsTest, "PerfectWorld.P313.GuardNoticesNameTheExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP313GuardWordsTest::RunTest(const FString& Parameters)
{
    for (const FString Code : {FString(TEXT("school.lesson_open")), FString(TEXT("school.paper_open")), FString(TEXT("tutorial.active"))})
    {
        const FText Words = P313GuardNotice(Code);
        const std::string Utf8 = TCHAR_TO_UTF8(*Code);
        TestFalse(TEXT("guard has words"), Words.IsEmpty());
        TestEqual(TEXT("commerce keeps directing words instead of invalid-state fallback"),
            PWCommerce::CmRefusalText(PerfectWorld::CommerceError::InvalidState, FText::GetEmpty(), Code).ToString(), Words.ToString());
        TestEqual(TEXT("planner keeps the same direction"), PWSchoolPlanner::RefusalText(Code).ToString(), Words.ToString());
        TestEqual(TEXT("lesson keeps the same direction"), PWClassLesson::IssueText(Utf8).ToString(), Words.ToString());
        TestEqual(TEXT("paper keeps the same direction"), PWA5::IssueText({Utf8, {}}).ToString(), Words.ToString());
    }
    TestTrue(TEXT("lesson names seat and exit"), P313GuardNotice(TEXT("school.lesson_open")).ToString().Contains(TEXT("classroom seat")) &&
        P313GuardNotice(TEXT("school.lesson_open")).ToString().Contains(TEXT("Leave early")));
    TestTrue(TEXT("paper names desk and both exits"), P313GuardNotice(TEXT("school.paper_open")).ToString().Contains(TEXT("study desk")) &&
        P313GuardNotice(TEXT("school.paper_open")).ToString().Contains(TEXT("Hand in or Cancel")));
    TestTrue(TEXT("tutorial names terminal and withdrawal"), P313GuardNotice(TEXT("tutorial.active")).ToString().Contains(TEXT("forecourt terminal")) &&
        P313GuardNotice(TEXT("tutorial.active")).ToString().Contains(TEXT("Withdraw")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP313ScenarioTest, "PerfectWorld.P313.GuardsResumeScenarioIsRegistered",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP313ScenarioTest::RunTest(const FString& Parameters)
{
    TArray<PWAcceptance::FStepDef> Steps;
    if (!TestTrue(TEXT("scenario builds"), PWAcceptance::BuildScenario(TEXT("guards_resume_round9"), Steps))) return false;
    TSet<FString> Names;
    for (const auto& Step : Steps)
    {
        TestFalse(TEXT("step names are unique"), Names.Contains(Step.Name));
        Names.Add(Step.Name);
    }
    TestTrue(TEXT("lesson entry present"), Names.Contains(TEXT("open_a_lesson_from_the_panel")));
    TestTrue(TEXT("directing refusal required"), Names.Contains(TEXT("P313_buy_refused_with_classroom_direction")));
    TestTrue(TEXT("seat resume required"), Names.Contains(TEXT("P313_seat_resumes_same_session")));
    TestTrue(TEXT("Leave early required"), Names.Contains(TEXT("P313_leave_early_releases_guard")));
    TestTrue(TEXT("accepted purchase required"), Names.Contains(TEXT("P313_purchase_succeeds_after_exit")));
    return true;
}

#endif
