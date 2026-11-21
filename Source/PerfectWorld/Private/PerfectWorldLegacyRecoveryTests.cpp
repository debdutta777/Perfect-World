#include "PerfectWorldLegacyRecovery.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "Misc/AutomationTest.h"
#include "PerfectWorldAwakeningResultBridge.h"
#include "UI/SPWCountdownPanel.h"
#include "UI/SPWStudyDesk.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
using namespace PerfectWorld;

/** Deliberately fakes the reducer/storage boundary, to exercise production load ordering without a map. */
struct FP314ULoadFake
{
    WorldState State;
    TArray<FString> Calls;
    bool bMissingWork = true;
    bool bCreationPassed = false;
    bool bRefuseRecovery = false;
    bool bSaveFails = false;
    bool bAllSystem = true;

    PWLegacyRecovery::FP314ULoadActions Actions()
    {
        PWLegacyRecovery::FP314ULoadActions Out;
        Out.Read = [this]() -> const WorldState& { return State; };
        Out.PendingWork = [this]() -> std::vector<std::string>
        {
            return bMissingWork ? std::vector<std::string>{"school.work.p314u"} : std::vector<std::string>{};
        };
        Out.Submit = [this](const Command& Command)
        {
            CommandResult Done;
            bAllSystem &= Command.Source == CommandSource::System;
            Done.Accepted = Command.Source == CommandSource::System;
            if (std::holds_alternative<SettleDueEvents>(Command.Payload))
            {
                Calls.Add(TEXT("settle"));
                (void)State.Clock.Advance(0);
            }
            else if (const auto* Recovery = std::get_if<AwakeningRecover>(&Command.Payload))
            {
                Calls.Add(TEXT("awakening"));
                bCreationPassed = Recovery->Creation.HouseholdTemplateId == "p314u.slot_creation";
                if (bRefuseRecovery)
                {
                    Done.Accepted = false;
                    Done.Why = {"awakening.recovery_missing_content", "fake unavailable content"};
                }
                else
                {
                    State.Awakening.Seeded = true;
                    State.Flags[std::string(AwakeningSeededOnRecoveryFlag)] = true;
                }
            }
            else if (std::holds_alternative<SchoolWorkReconcile>(Command.Payload))
            {
                Calls.Add(TEXT("homework"));
                bMissingWork = false;
            }
            else Done.Accepted = false;
            return Done;
        };
        Out.Autosave = [this]()
        {
            Calls.Add(TEXT("save"));
            return bSaveFails ? FText::FromString(TEXT("P314U fake autosave failed; earlier slot kept.")) : FText::GetEmpty();
        };
        return Out;
    }
};

FString P314UVisibleText(const TSharedRef<SWidget>& Widget)
{
    if (!Widget->GetVisibility().IsVisible()) return {};
    FString Text;
    if (Widget->GetTypeAsString() == TEXT("STextBlock"))
        Text = StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
    FChildren* Children = Widget->GetChildren();
    for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
        Text += TEXT("\n") + P314UVisibleText(Children->GetChildAt(Index));
    return Text;
}

class FP314UStudyFake final : public PWA5::IStudyWorld
{
public:
    SchoolWorkState Work;
    SchoolWorkBriefCatalogue Authored;
    int32 Applied = 0;
    int32 Practised = 0;

    bool CanUseDesk(FText&) const override { return true; }
    bool ReadRecord(SchoolAcademicRecordView& Out, FText&) const override
    {
        Out = {};
        for (const auto& Record : Work.Records)
        {
            SchoolWorkView Row;
            Row.Brief = Record.Brief;
            Out.Work.push_back(Row);
        }
        return true;
    }
    SchoolWorkResult Apply(const SchoolWorkCommand&) override { ++Applied; return {}; }
    AssessmentQuote Quote(std::string_view, const AssessmentAnswer&) const override { return {}; }
    FText Text(std::string_view, const FText& Fallback) const override { return Fallback; }
    FText GroupPrompt(std::string_view, std::size_t) const override { return FText::GetEmpty(); }
    FText RecoveryNotice(std::string_view Id) const override
    {
        return PWLegacyRecovery::P314URecoveredWorkNotice(Work, Authored, Id);
    }
    TArray<PWA5::FPracticeOffer> PracticeOffers() const override { return {}; }
    SchoolIssue StartPractice(std::string_view) override { ++Practised; return {}; }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP314ULoadOrder, "PerfectWorld.P314U.LoadOrderAndRepeat",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP314ULoadOrder::RunTest(const FString& Parameters)
{
    FP314ULoadFake Fake;
    CharacterCreationSave Creation;
    Creation.HouseholdTemplateId = "p314u.slot_creation";
    const auto Actions = Fake.Actions();
    const FText Notice = PWLegacyRecovery::P314URecoverLoadedLife(Creation, Actions);
    TestTrue(TEXT("recovery, homework, then exactly one save"),
        Fake.Calls == TArray<FString>{TEXT("awakening"), TEXT("homework"), TEXT("save")});
    TestTrue(TEXT("the slot creation was carried to the System command"), Fake.bCreationPassed);
    TestTrue(TEXT("all recovery commands use System source"), Fake.bAllSystem);
    TestTrue(TEXT("first seed disclosure shown"), Notice.ToString().Contains(TEXT("nothing you did not record has been added")));
    Fake.Calls.Reset();
    TestTrue(TEXT("repeat load has no seed notice"), PWLegacyRecovery::P314URecoverLoadedLife(Creation, Actions).IsEmpty());
    TestTrue(TEXT("repeat load makes only its one autosave attempt"), Fake.Calls == TArray<FString>{TEXT("save")});

    FP314ULoadFake Unsettled;
    TestTrue(TEXT("fake due event installed"), Unsettled.State.Clock.Schedule(0, "p314u.due") > 0);
    (void)PWLegacyRecovery::P314URecoverLoadedLife(Creation, Unsettled.Actions());
    TestTrue(TEXT("due events settle before recovery"), Unsettled.Calls ==
        TArray<FString>{TEXT("settle"), TEXT("awakening"), TEXT("homework"), TEXT("save")});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP314ULoadFailure, "PerfectWorld.P314U.LoadFailuresDisclosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP314ULoadFailure::RunTest(const FString& Parameters)
{
    FP314ULoadFake Fake;
    Fake.bRefuseRecovery = true;
    Fake.bSaveFails = true;
    const FText Notice = PWLegacyRecovery::P314URecoverLoadedLife(CharacterCreationSave{}, Fake.Actions());
    TestFalse(TEXT("refused recovery cannot claim a successful seed"), Fake.State.Awakening.Seeded);
    TestFalse(TEXT("no seed disclosure on refusal"), Notice.ToString().Contains(TEXT("has been prepared")));
    TestTrue(TEXT("recovery failure disclosed"), Notice.ToString().Contains(TEXT("recovery could not finish")));
    TestTrue(TEXT("autosave failure disclosed"), Notice.ToString().Contains(TEXT("earlier slot kept")));
    TestTrue(TEXT("refusal does not loop or save more than once"),
        Fake.Calls == TArray<FString>{TEXT("awakening"), TEXT("homework"), TEXT("save")});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP314UCompatibilityPanel, "PerfectWorld.P314U.CompatibilityPanel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP314UCompatibilityPanel::RunTest(const FString& Parameters)
{
    WorldState State;
    State.Awakening.Seeded = true;
    State.Awakening.MakeUp.Phase = AwakeningMakeUpPhase::Required;
    State.Flags[std::string(AwakeningCountdownUnavailableFlag)] = true;
    const auto Now = GameClock::FromCalendar(2043, 1, 29, 12, 0);
    (void)State.Clock.AdvanceTo(Now);
    const auto BeforeFlags = State.Flags;
    const auto Fake = PWAwakening::MakeFakeAwakeningWorld();
    Fake->SetCountdownModel(PWAwakening::BuildCountdownModelFromCore(Now, State, nullptr, {}));
    const auto Panel = SNew(SPWCountdownPanel).BridgeOverride(Fake);
    FString Text = P314UVisibleText(Panel);
    TestTrue(TEXT("make-up requirement visible"), Text.Contains(TEXT("supervised make-up is required")));
    TestTrue(TEXT("last start read from core view"), Text.Contains(PWA5::Deadline(QueryAwakeningCompatibility(State).LastSessionStart).ToString()));
    TestTrue(TEXT("missing countdown explained"), Text.Contains(TEXT("countdown opportunities are unavailable")));
    TestTrue(TEXT("ordinary preparation remains meaningful"), Text.Contains(TEXT("ordinary preparation still counts")));
    TestTrue(TEXT("reading compatibility leaves flags intact"), State.Flags == BeforeFlags);

    State.Flags[std::string(AwakeningPostDeadlineFlag)] = true;
    (void)State.Clock.AdvanceTo(AwakeningLastSessionStart(State.Awakening) + 1);
    Fake->SetCountdownModel(PWAwakening::BuildCountdownModelFromCore(State.Clock.Now(), State, nullptr, {}));
    Panel->RefreshFromBridge();
    Text = P314UVisibleText(Panel);
    TestTrue(TEXT("legacy mode visible"), Text.Contains(TEXT("Legacy compatibility mode")));
    TestTrue(TEXT("new-life pointer visible"), Text.Contains(TEXT("start a new life")) && Text.Contains(TEXT("New Game")));
    TestTrue(TEXT("slot retention visible"), Text.Contains(TEXT("This save is kept")));
    TestFalse(TEXT("no expired make-up prompt alongside legacy mode"), Text.Contains(TEXT("supervised make-up is required")));
    TestEqual(TEXT("reading panel never confirms practice"), Fake->GetConfirmOfferCount(), 0);
    TestEqual(TEXT("reading panel never chooses ordinary day"), Fake->GetKeepOrdinaryDayCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FP314URecoveredDesk, "PerfectWorld.P314U.RecoveredHomeworkDesk",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FP314URecoveredDesk::RunTest(const FString& Parameters)
{
    const auto Fake = MakeShared<FP314UStudyFake>();
    SchoolWorkBriefDef Def;
    Def.Brief.Id = "p314u.homework";
    Def.Brief.AvailableAt = 100;
    Def.Brief.DueAt = 200;
    Fake->Authored.Briefs.push_back(Def);
    SchoolWorkRecord Record;
    Record.IssuedAt = 500;
    Record.Brief = RecoveredSchoolWorkBrief(Def.Brief, Record.IssuedAt);
    Fake->Work.Records.push_back(Record);
    const auto Before = Fake->Work;
    const auto Desk = SNew(SPWStudyDesk).World(Fake);
    const FString Text = P314UVisibleText(Desk);
    TestTrue(TEXT("recovered label rendered"), Text.Contains(TEXT("Recovered homework")));
    TestTrue(TEXT("recovery beginning disclosed"), Text.Contains(PWA5::Deadline(Record.IssuedAt).ToString()));
    TestTrue(TEXT("actual recovery deadline disclosed"), Text.Contains(PWA5::Deadline(Record.Brief.DueAt).ToString()));
    TestTrue(TEXT("seven-day minimum disclosed"), Text.Contains(TEXT("at least a seven-day recovery window")));
    Desk->Refresh();
    TestTrue(TEXT("opening and refreshing never issues or alters work"), Fake->Work == Before);
    TestEqual(TEXT("opening and refreshing submits no command"), Fake->Applied, 0);
    TestEqual(TEXT("opening and refreshing starts no practice"), Fake->Practised, 0);

    Fake->Work.Records.front().Brief = Def.Brief;
    Fake->Work.Records.front().IssuedAt = Def.Brief.AvailableAt;
    Desk->Refresh();
    TestFalse(TEXT("ordinary homework has no recovery label"), P314UVisibleText(Desk).Contains(TEXT("Recovered homework")));
    Fake->Work.Records.front() = Record;
    ++Fake->Work.Records.front().Brief.DueAt;
    TestTrue(TEXT("a modified brief is not labelled as a valid recovery"), Fake->RecoveryNotice(Record.Brief.Id).IsEmpty());
    // Authored deadlines later than seven days still win; disclose the actual end, without calling it exactly seven days.
    Fake->Authored.Briefs.front().Brief.DueAt = Record.IssuedAt + 10 * MinutesPerDay;
    Fake->Work.Records.front().Brief = RecoveredSchoolWorkBrief(Fake->Authored.Briefs.front().Brief, Record.IssuedAt);
    TestTrue(TEXT("later authored deadline is disclosed"), Fake->RecoveryNotice(Record.Brief.Id).ToString().Contains(
        PWA5::Deadline(Fake->Authored.Briefs.front().Brief.DueAt).ToString()));
    Fake->Authored.Briefs.clear();
    TestTrue(TEXT("missing authored brief never guesses recovery"), Fake->RecoveryNotice(Record.Brief.Id).IsEmpty());
    return true;
}

#endif
