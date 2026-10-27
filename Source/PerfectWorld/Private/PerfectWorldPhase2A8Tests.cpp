#include "UI/SPWTowerResults.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/AutomationTest.h"
#include "UI/PWWidgets.h"

namespace
{
class FA8FakeWorld final : public IA8ResultsWorld
{
public:
    PerfectWorld::TowerSessionView View;
    PerfectWorld::SchoolFollowupContent Content;
    std::string LastAction;
    int32 Calls = 0;
    bool bAccept = true;

    FA8FakeWorld()
    {
        View.Participation = PerfectWorld::TowerParticipation::Ejected;
        View.Result.Decile = 10;
        View.Result.NationalRank = 1000;
        View.Result.OfferIds = {"offer.tower.apprenticeship", "offer.tower.study_bridge", "offer.tower.community_practice"};
        for (const auto* Suffix : {"apprenticeship", "study_bridge", "community_practice"})
        {
            PerfectWorld::SchoolFollowupAction Action;
            Action.Id = "school_followup.p2." + std::string(Suffix);
            Action.OfferId = "offer.tower." + std::string(Suffix);
            Action.Outcome = "all"; Action.Kind = "appointment";
            Action.TitleTextKey = "phase2.a8.test.title"; Action.BriefTextKey = "phase2.a8.test.brief";
            Action.AppointmentId = "appointment.p2." + std::string(Suffix);
            Content.Actions.push_back(Action);
        }
    }
    virtual PerfectWorld::TowerSessionView QueryResults() const override { return View; }
    virtual PerfectWorld::SchoolFollowupContent FollowupContent() const override { return Content; }
    virtual FText ResolveText(const std::string& Key) const override
    {
        return FText::FromString(Key == "phase2.a8.test.brief" ?
            TEXT("Bring a question to your adviser. This is a request, and school continues.") : TEXT("Choose a next step"));
    }
    virtual bool OpenNextAction(const std::string& Id) override { ++Calls; LastAction = Id; return bAccept; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA8ResultsBridgeTest, "PerfectWorld.Phase2.A8.ViewAndDispatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA8ResultsBridgeTest::RunTest(const FString& Parameters)
{
    const auto World = MakeShared<FA8FakeWorld>();
    const TSharedRef<SPWTowerResults> Panel = SNew(SPWTowerResults).World(World).Space(FVector2D(853, 480));
    TestEqual(TEXT("three actionable bottom-decile offers"), static_cast<int32>(Panel->GetActions().size()), 3);
    TestEqual(TEXT("reading results has no dispatch"), World->Calls, 0);
    TestFalse(TEXT("unknown action is refused"), Panel->OpenAction("school_followup.p2.unknown"));
    TestEqual(TEXT("refusal never reaches world"), World->Calls, 0);
    const std::string Id = Panel->GetActions().front().Id;
    TestTrue(TEXT("eligible next step forwards stable ID"), Panel->OpenAction(Id));
    TestTrue(TEXT("bridge receives exactly selected ID"), World->LastAction == Id);
    TestEqual(TEXT("one action dispatch"), World->Calls, 1);
    World->View.Result.OfferIds.clear();
    TestFalse(TEXT("stale eligibility is re-read before dispatch"), Panel->OpenAction(Id));
    TestEqual(TEXT("stale action never reaches world"), World->Calls, 1);
    World->View.Result.OfferIds = {"offer.tower.apprenticeship"};
    World->bAccept = false;
    TestFalse(TEXT("bridge refusal is propagated"), Panel->OpenAction(Id));
    TestEqual(TEXT("one attempted authoritative refusal"), World->Calls, 2);
    PWUI::CancelPendingFocus(Panel);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA8ResultsFitTest, "PerfectWorld.Phase2.A8.FitAndBack",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA8ResultsFitTest::RunTest(const FString& Parameters)
{
    const auto World = MakeShared<FA8FakeWorld>();
    int32 Closed = 0;
    // Logical space is viewport / UI scale. Rendered window/focus acceptance is separate.
    for (const FVector2D Viewport : {FVector2D(853, 480), FVector2D(1920, 1080)})
    for (const float Scale : {1.0f, 1.5f})
    {
        const FVector2D Space = Viewport / Scale;
        const TSharedRef<SPWTowerResults> Panel = SNew(SPWTowerResults).World(World).Space(Space)
            .OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));
        Panel->SlatePrepass(1.0f);
        TestTrue(TEXT("panel fits logical width at text scale"), Panel->GetDesiredSize().X <= Space.X);
        TestTrue(TEXT("scrolling content fits logical height at text scale"), Panel->GetDesiredSize().Y <= Space.Y);
        const FGeometry Geometry;
        const int32 BeforeBack = Closed;
        Panel->OnKeyDown(Geometry, FKeyEvent(EKeys::Gamepad_FaceButton_Right, FModifierKeysState(), 0, true, 0, 0));
        TestEqual(TEXT("held controller Back does not repeat close"), Closed, BeforeBack);
        Panel->OnKeyDown(Geometry, FKeyEvent(EKeys::Gamepad_FaceButton_Right, FModifierKeysState(), 0, false, 0, 0));
    }
    TestEqual(TEXT("controller Back closes once per panel"), Closed, 4);
    TestEqual(TEXT("layout and Back never mutate world"), World->Calls, 0);
    World->View.Participation = PerfectWorld::TowerParticipation::Entered;
    const TSharedRef<SPWTowerResults> Pending = SNew(SPWTowerResults).World(World);
    TestEqual(TEXT("no final offers during runner"), static_cast<int32>(Pending->GetActions().size()), 0);
    PWUI::CancelPendingFocus(Pending);
    return true;
}

#endif
