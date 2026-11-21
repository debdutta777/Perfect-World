#include "PerfectWorldExaminationBridge.h"
#include "UI/SPWExaminationPanel.h"

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Layout/Children.h"
#include "Misc/AutomationTest.h"
#include "Widgets/Text/STextBlock.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
P406Exam::FAction P406TestAction(const TCHAR* P406Label, PerfectWorld::TowerSessionAction P406Kind, bool P406Enabled = true)
{
    P406Exam::FAction P406Row;
    P406Row.Label = FText::FromString(P406Label);
    P406Row.Kind = P406Kind;
    P406Row.bEnabled = P406Enabled;
    return P406Row;
}

P406Exam::FModel P406TestDecision()
{
    P406Exam::FModel P406Model;
    P406Model.Screen = P406Exam::EScreen::Decision;
    P406Model.World.DecisionRequired = true;
    P406Model.Title = FText::FromString(TEXT("06:00 - 2 February 2043"));
    P406Model.World.SitRefusal = "exam.missing_content";
    P406Model.Lines.Add(P406Exam::P406RefusalText(TEXT("exam.missing_content")));
    P406Model.Actions.Add(P406TestAction(TEXT("Sit"), PerfectWorld::TowerSessionAction::Enter, false));
    P406Model.Actions.Add(P406TestAction(TEXT("Defer"), PerfectWorld::TowerSessionAction::Defer));
    P406Model.Actions.Add(P406TestAction(TEXT("Decline"), PerfectWorld::TowerSessionAction::Decline));
    return P406Model;
}

P406Exam::FModel P406TestResult()
{
    P406Exam::FModel P406Model;
    P406Model.Screen = P406Exam::EScreen::Result;
    P406Model.World.Finished = true;
    P406Model.Lines.Add(FText::FromString(TEXT("You withdrew. Cleared evidence is retained.")));
    P406Model.Offers.Add(FText::FromString(TEXT("Ask the school about an apprenticeship.")));
    return P406Model;
}

void P406TestTexts(const TSharedRef<SWidget>& P406Root, TArray<FString>& P406Out)
{
    if (!P406Root->GetVisibility().IsVisible()) return;
    if (P406Root->GetType() == FName(TEXT("STextBlock")))
        P406Out.Add(static_cast<const STextBlock&>(P406Root.Get()).GetText().ToString());
    FChildren* P406Children = P406Root->GetChildren();
    for (int32 P406Index = 0; P406Children != nullptr && P406Index < P406Children->Num(); ++P406Index)
        P406TestTexts(P406Children->GetChildAt(P406Index), P406Out);
}

FString P406TestAllText(const TSharedRef<SWidget>& P406Root)
{
    TArray<FString> P406Texts;
    P406TestTexts(P406Root, P406Texts);
    return FString::Join(P406Texts, TEXT("\n"));
}

void P406TestKey(const TSharedRef<SPWExaminationPanel>& P406Panel, const FKey& P406Key, bool P406Repeat = false)
{
    P406Panel->OnPreviewKeyDown(FGeometry(), FKeyEvent(P406Key, FModifierKeysState(), 0, P406Repeat, 0, 0));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP406DecisionPanelTest, "PerfectWorld.Examination.P406.Decision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP406DecisionPanelTest::RunTest(const FString& Parameters)
{
    const auto P406Fake = MakeShared<P406Exam::FFakeWorld>();
    P406Fake->Model = P406TestDecision();
    P406Fake->AfterAccepted = P406TestResult();
    const auto P406Panel = SNew(SPWExaminationPanel).BridgeOverride(P406Fake).ViewportSize(FVector2D(853.0, 480.0));
    TestEqual(TEXT("construction is read-only"), P406Fake->Submitted.Num(), 0);
    TestEqual(TEXT("disabled Sit is skipped on initial focus"), P406Panel->P406GetFocusIndex(), 1);
    TestFalse(TEXT("unavailable Sit cannot submit"), P406Panel->P406TriggerAction(0));
    const auto P406Text = P406TestAllText(P406Panel);
    TestTrue(TEXT("Sit reason is visible in words"), P406Text.Contains(TEXT("passages are unavailable")));
    TestFalse(TEXT("raw refusal code is hidden"), P406Text.Contains(TEXT("exam.missing_content")));
    TestTrue(TEXT("all participation choices are present"), P406Text.Contains(TEXT("Sit")) &&
        P406Text.Contains(TEXT("Defer")) && P406Text.Contains(TEXT("Decline")));
    P406TestKey(P406Panel, EKeys::Gamepad_DPad_Down);
    TestEqual(TEXT("controller reaches Decline"), P406Panel->P406GetFocusIndex(), 2);
    P406TestKey(P406Panel, EKeys::Gamepad_FaceButton_Bottom);
    TestEqual(TEXT("one controller press submits once"), P406Fake->Submitted.Num(), 1);
    if (P406Fake->Submitted.Num() != 1) return false;
    TestTrue(TEXT("Decline command"), P406Fake->Submitted[0].Kind == PerfectWorld::TowerSessionAction::Decline);
    TestTrue(TEXT("result appears without reopening entry"), P406Panel->P406GetModel().Screen == P406Exam::EScreen::Result);
    TestTrue(TEXT("next step is visible"), P406TestAllText(P406Panel).Contains(TEXT("apprenticeship")));
    TestFalse(TEXT("terminal result has no Withdraw action"), P406Panel->P406TriggerWithdraw());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP406SessionPanelTest, "PerfectWorld.Examination.P406.RouteBeatResume",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP406SessionPanelTest::RunTest(const FString& Parameters)
{
    const auto P406Fake = MakeShared<P406Exam::FFakeWorld>();
    auto& P406Model = P406Fake->Model;
    P406Model.Screen = P406Exam::EScreen::Routes;
    P406Model.World.CanWithdraw = true;
    P406Model.World.OutsideMinutes = 7;
    auto P406Route = P406TestAction(TEXT("Analysis route - opening costs no time"), PerfectWorld::TowerSessionAction::OpenRoute);
    P406Route.RouteId = TEXT("test.analysis");
    P406Route.Method = PerfectWorld::TowerMethod::Analysis;
    P406Model.Actions.Add(P406Route);
    P406Model.Session.Runner = 12;
    P406Model.Session.Beat = 3;
    P406Fake->AfterAccepted = P406Model;
    P406Fake->AfterAccepted.Screen = P406Exam::EScreen::Beat;
    P406Fake->AfterAccepted.Session.Runner = 13;
    P406Fake->AfterAccepted.Session.Beat = 0;
    P406Fake->AfterAccepted.Actions.Reset();
    auto P406Choice = P406TestAction(TEXT("Study the clue | 4 mission minutes | 1 strain"), PerfectWorld::TowerSessionAction::Choose);
    P406Choice.ChoiceId = TEXT("test.study");
    P406Fake->AfterAccepted.Actions.Add(P406Choice);
    P406Fake->AfterAccepted.Lines.Add(FText::FromString(TEXT("Prompt: where did the supply go?")));
    P406Fake->AfterAccepted.Lines.Add(FText::FromString(TEXT("Clue: the seal is intact.")));
    const auto P406Panel = SNew(SPWExaminationPanel).BridgeOverride(P406Fake);
    TestTrue(TEXT("open route accepted"), P406Panel->P406TriggerAction(0));
    TestEqual(TEXT("one route command"), P406Fake->Submitted.Num(), 1);
    if (P406Fake->Submitted.Num() != 1) return false;
    TestEqual(TEXT("route ID preserved"), FString(UTF8_TO_TCHAR(P406Fake->Submitted[0].RouteId.c_str())), FString(TEXT("test.analysis")));
    TestTrue(TEXT("method preserved"), P406Fake->Submitted[0].Method == PerfectWorld::TowerMethod::Analysis);
    const auto P406Text = P406TestAllText(P406Panel);
    TestTrue(TEXT("prompt, clue and costs visible"), P406Text.Contains(TEXT("supply go")) && P406Text.Contains(TEXT("seal is intact")) &&
        P406Text.Contains(TEXT("4 mission minutes | 1 strain")));
    TestTrue(TEXT("withdraw cost visible on beat"), P406Text.Contains(TEXT("Withdraw - 7 outside minutes")));
    // A resumed panel must read the existing beat, without Enter or OpenRoute side effects.
    const auto P406Resumed = SNew(SPWExaminationPanel).BridgeOverride(P406Fake);
    TestEqual(TEXT("resume submits nothing"), P406Fake->Submitted.Num(), 1);
    P406Fake->Reply.Accepted = false;
    P406Fake->Reply.Why.Code = "tower.session.stale_beat";
    TestFalse(TEXT("refused choice is returned honestly"), P406Resumed->P406TriggerAction(0));
    TestEqual(TEXT("one choice attempt"), P406Fake->Submitted.Num(), 2);
    if (P406Fake->Submitted.Num() != 2) return false;
    TestTrue(TEXT("current runner counter and beat are submitted"), P406Fake->Submitted[1].Runner == 13 && P406Fake->Submitted[1].Beat == 0);
    TestEqual(TEXT("choice ID preserved"), FString(UTF8_TO_TCHAR(P406Fake->Submitted[1].ChoiceId.c_str())), FString(TEXT("test.study")));
    TestTrue(TEXT("stale refusal displayed in words"), P406TestAllText(P406Resumed).Contains(TEXT("passage has moved on")));
    TestTrue(TEXT("refusal preserves active beat"), P406Resumed->P406GetModel().Screen == P406Exam::EScreen::Beat);
    P406Fake->Model.Session.Runner = 21;
    P406Fake->Model.Session.Beat = 5;
    P406Fake->Reply.Accepted = true;
    P406Fake->AfterAccepted = P406TestResult();
    TestTrue(TEXT("withdraw remains possible after refusal"), P406Resumed->P406TriggerWithdraw());
    TestEqual(TEXT("one Withdraw attempt"), P406Fake->Submitted.Num(), 3);
    if (P406Fake->Submitted.Num() != 3) return false;
    TestTrue(TEXT("Withdraw uses freshly read counters"), P406Fake->Submitted[2].Runner == 21 && P406Fake->Submitted[2].Beat == 5);
    TestTrue(TEXT("withdraw result has next step"), P406TestAllText(P406Resumed).Contains(TEXT("apprenticeship")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP406FocusPanelTest, "PerfectWorld.Examination.P406.FocusAndBack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP406FocusPanelTest::RunTest(const FString& Parameters)
{
    const auto P406Fake = MakeShared<P406Exam::FFakeWorld>();
    P406Fake->Model = P406TestDecision();
    int32 P406Closed = 0;
    const auto P406Panel = SNew(SPWExaminationPanel).BridgeOverride(P406Fake)
        .OnClose(FSimpleDelegate::CreateLambda([&P406Closed]() { ++P406Closed; }));
    P406TestKey(P406Panel, EKeys::Up);
    TestEqual(TEXT("keyboard wraps past disabled Sit"), P406Panel->P406GetFocusIndex(), 3);
    P406TestKey(P406Panel, EKeys::Down);
    TestEqual(TEXT("keyboard wraps to Defer"), P406Panel->P406GetFocusIndex(), 1);
    P406TestKey(P406Panel, EKeys::Enter, true);
    TestEqual(TEXT("held activation never submits"), P406Fake->Submitted.Num(), 0);
    P406TestKey(P406Panel, EKeys::Gamepad_FaceButton_Right);
    TestEqual(TEXT("Back closes without choosing participation"), P406Closed, 1);
    TestEqual(TEXT("Back is not an examination command"), P406Fake->Submitted.Num(), 0);
    TestTrue(TEXT("decision survives close"), P406Fake->Model.World.DecisionRequired);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP406ExitPanelTest, "PerfectWorld.Examination.P406.ParticipationAndExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP406ExitPanelTest::RunTest(const FString& Parameters)
{
    for (const auto P406Kind : {PerfectWorld::TowerSessionAction::Enter, PerfectWorld::TowerSessionAction::Defer,
        PerfectWorld::TowerSessionAction::Decline})
    {
        const auto P406Fake = MakeShared<P406Exam::FFakeWorld>();
        P406Fake->Model = P406TestDecision();
        P406Fake->Model.Actions.Reset();
        P406Fake->Model.Actions.Add(P406TestAction(TEXT("Participation"), P406Kind));
        P406Fake->AfterAccepted = P406TestResult();
        const auto P406Panel = SNew(SPWExaminationPanel).BridgeOverride(P406Fake);
        TestTrue(TEXT("enabled participation action accepted"), P406Panel->P406TriggerAction(0));
        TestEqual(TEXT("one participation command"), P406Fake->Submitted.Num(), 1);
        if (P406Fake->Submitted.Num() != 1) return false;
        TestTrue(TEXT("participation kind preserved"), P406Fake->Submitted[0].Kind == P406Kind);
    }
    const auto P406Fake = MakeShared<P406Exam::FFakeWorld>();
    P406Fake->Model.Screen = P406Exam::EScreen::Routes;
    P406Fake->Model.World.CanWithdraw = true;
    // A resumed session with missing content has no route or choice buttons; exit must still work.
    P406Fake->AfterAccepted = P406TestResult();
    const auto P406Panel = SNew(SPWExaminationPanel).BridgeOverride(P406Fake).ViewportSize(FVector2D(569.0, 320.0));
    TestEqual(TEXT("missing content retains Withdraw and Close"), P406Panel->P406GetFocusCount(), 2);
    TestTrue(TEXT("Withdraw is present without route content"), P406TestAllText(P406Panel).Contains(TEXT("Withdraw")));
    TestTrue(TEXT("withdraw without content succeeds"), P406Panel->P406TriggerWithdraw());
    TestEqual(TEXT("one exit command"), P406Fake->Submitted.Num(), 1);
    if (P406Fake->Submitted.Num() != 1) return false;
    TestTrue(TEXT("exit really submits Withdraw"), P406Fake->Submitted[0].Kind == PerfectWorld::TowerSessionAction::Withdraw);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP406TextTableTest, "PerfectWorld.Examination.P406.AuthoredWords",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP406TextTableTest::RunTest(const FString& Parameters)
{
    const auto P406Original = P406Exam::P406RunnerText("p2.a7.encounter.b1.prompt", TEXT("Missing"));
    const auto P406Expanded = P406Exam::P406RunnerText("p2.a7.alliance_p404.b1.prompt", TEXT("Missing"));
    TestTrue(TEXT("original passage resolves"), !P406Original.IsEmpty() && P406Original.ToString() != TEXT("Missing"));
    TestTrue(TEXT("expanded passage resolves"), !P406Expanded.IsEmpty() && P406Expanded.ToString() != TEXT("Missing"));
    TestEqual(TEXT("unknown key uses words instead of exposing ID"),
        P406Exam::P406RunnerText("p406.unknown_key", TEXT("Text unavailable")).ToString(), FString(TEXT("Text unavailable")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP407UMentorSessionTest, "PerfectWorld.Examination.P407U.MentorSession",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FP407UMentorSessionTest::RunTest(const FString& Parameters)
{
    const auto P407UFake = MakeShared<P406Exam::FFakeWorld>();
    P407UFake->Model = P406TestResult();
    PerfectWorld::P407UFollowupView P407URow;
    P407URow.Action.Id = "school_followup.p2.p4_withdrawn_school_mentor";
    P407URow.Action.TitleTextKey = "phase2.a8.p4_withdrawn_school_mentor.title";
    P407URow.Action.BriefTextKey = "phase2.a8.p4_withdrawn_school_mentor.brief";
    P407URow.Action.DurationMinutes = 30;
    P407URow.At = 12345;
    P407URow.Booked = P407URow.CanAttend = true;
    P407UFake->Model.P407UFollowups.push_back(P407URow);
    P406Exam::FAction P407UAction;
    P407UAction.Label = FText::FromString(TEXT("Attend school mentor session"));
    P407UAction.bP407UFollowup = true;
    P407UAction.P407UCommand = {PerfectWorld::P407UFollowupAction::Attend, P407URow.Action.Id, P407URow.At};
    P407UFake->Model.Actions.Add(P407UAction);
    P407UFake->AfterAccepted = P406TestResult();
    const auto P407UPanel = SNew(SPWExaminationPanel).BridgeOverride(P407UFake).ViewportSize(FVector2D(569.0, 320.0));
    TestTrue(TEXT("session opens"), P407UPanel->P406TriggerAction(0));
    TestTrue(TEXT("a separate session is presented"), P407UPanel->P406GetModel().Screen == P406Exam::EScreen::P407USession);
    TestEqual(TEXT("reading sends no attendance"), P407UFake->P407USubmitted.Num(), 0);
    TestTrue(TEXT("mentor has readable words"), P406TestAllText(P407UPanel).Contains(TEXT("Amrita Basu")));
    TestTrue(TEXT("duration is disclosed"), P406TestAllText(P407UPanel).Contains(TEXT("30 minutes")));
    TestTrue(TEXT("session finishes through the bridge"), P407UPanel->P407UFinishSession());
    TestEqual(TEXT("exactly one attendance command"), P407UFake->P407USubmitted.Num(), 1);
    TestEqual(TEXT("no examination command is sent"), P407UFake->Submitted.Num(), 0);
    if (P407UFake->P407USubmitted.Num() != 1) return false;
    TestTrue(TEXT("stable identity and instant survive"), P407UFake->P407USubmitted[0].ActionId == P407URow.Action.Id &&
        P407UFake->P407USubmitted[0].ExpectedAt == P407URow.At &&
        P407UFake->P407USubmitted[0].Kind == PerfectWorld::P407UFollowupAction::Attend);
    TestFalse(TEXT("finishing twice is unavailable"), P407UPanel->P407UFinishSession());
    return true;
}

#endif
