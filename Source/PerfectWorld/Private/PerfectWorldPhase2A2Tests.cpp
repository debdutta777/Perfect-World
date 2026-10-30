// Lessons (orders A2 + A3 consolidated): the model and explain screens, and the shared shell and words they sit on (SPWClassLesson). Run in
// the editor under "PerfectWorld.Phase2A2". The widgets are the real ones, built without a window: SlatePrepass evaluates bound text and
// visibility; buttons are pressed with SButton::SimulateClick or the shell's PressAction. The world is the portable core itself (FCoreBridge
// over real SchoolSessions state and the real Data/Content), so a price asserted here is the core's quote and a charge is the core's charge.
// Focus and the controller need a real window and are not proved here. The Investigate and Coordinate screens are in PerfectWorldPhase2A3Tests.cpp.
// STATUS: written without a compiler; NOT RUN.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING

#include "HAL/FileManager.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Json.h"
#include "SchoolSessions.h"
#include "UI/SPWClassExplain.h"
#include "UI/SPWClassModel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#include <algorithm>
#include <memory>


namespace
{
// ---- The real content, loaded once ----------------------------------------------------------------------------------------

struct FA2TFixture
{
	PerfectWorld::ContentDb Content;
	PerfectWorld::ActivityCatalogue Activities;
	PerfectWorld::BellScheduleSet Bells;
	PerfectWorld::SchoolCalendarSet Calendars;
	PerfectWorld::ModelCatalogue Model;
	PerfectWorld::ExplainCatalogue Explain;
	PerfectWorld::InvestigateCatalogue Investigate;
	PerfectWorld::CoordinateCatalogue Coordinate;

	FA2TFixture() = default;
	FA2TFixture(const FA2TFixture&) = delete;
	FA2TFixture& operator=(const FA2TFixture&) = delete;

	bool Load(FAutomationTestBase& Test)
	{
		const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/Content")));
		TArray<FString> Paths;
		IFileManager::Get().FindFilesRecursive(Paths, *Root, TEXT("*.json"), true, false);
		Paths.Sort();
		if (Paths.IsEmpty())
		{
			Test.AddError(FString::Printf(TEXT("no content under %s"), *Root));
			return false;
		}
		std::vector<PerfectWorld::ContentFile> Files;
		for (FString Path : Paths)
		{
			FPaths::NormalizeFilename(Path);
			FString Text;
			if (!FFileHelper::LoadFileToString(Text, *Path))
			{
				Test.AddError(FString::Printf(TEXT("cannot read %s"), *Path));
				return false;
			}
			Files.push_back({std::string(TCHAR_TO_UTF8(*Path)), std::string(TCHAR_TO_UTF8(*Text))});
		}
		PerfectWorld::ContentLoadResult Loaded = PerfectWorld::LoadContent(Files);
		if (!Loaded.Ok())
		{
			Test.AddError(TEXT("the content database does not validate"));
			return false;
		}
		Content = std::move(Loaded.Db);
		PerfectWorld::ActivityCatalogueResult A = PerfectWorld::LoadActivityCatalogue(Content);
		PerfectWorld::BellScheduleResult B = PerfectWorld::LoadBellSchedules(Content);
		PerfectWorld::SchoolCalendarResult C = PerfectWorld::LoadSchoolCalendars(Content);
		PerfectWorld::ModelCatalogueResult M = PerfectWorld::LoadModelCatalogue(Content);
		PerfectWorld::ExplainCatalogueResult E = PerfectWorld::LoadExplainCatalogue(Content);
		PerfectWorld::InvestigateCatalogueResult I = PerfectWorld::LoadInvestigateCatalogue(Content);
		PerfectWorld::CoordinateCatalogueResult O = PerfectWorld::LoadCoordinateCatalogue(Content);
		if (!A.Ok() || !B.Ok() || !C.Ok() || !M.Ok() || !E.Ok() || !I.Ok() || !O.Ok())
		{
			Test.AddError(TEXT("a school or structure catalogue does not load"));
			return false;
		}
		Activities = std::move(A.Catalogue);
		Bells = std::move(B.Set);
		Calendars = std::move(C.Set);
		Model = std::move(M.Catalogue);
		Explain = std::move(E.Catalogue);
		Investigate = std::move(I.Catalogue);
		Coordinate = std::move(O.Catalogue);
		return true;
	}

	bool Cats(PerfectWorld::SchoolSessionCatalogues& Out, const char* Stream = "science") const
	{
		const auto Bell = std::find_if(Bells.Schedules.begin(), Bells.Schedules.end(), [&](const PerfectWorld::BellSchedule& S) { return S.Stream == Stream; });
		if (Bell == Bells.Schedules.end())
		{
			return false;
		}
		const PerfectWorld::SchoolCalendar* Calendar = Calendars.Find(Bell->InstitutionId, Bell->StageId);
		if (Calendar == nullptr)
		{
			return false;
		}
		Out = PerfectWorld::SchoolSessionCatalogues{{&Activities, &*Bell, Calendar}, {&Model, &Explain, &Investigate, &Coordinate}};
		return true;
	}

	/** A binding for the given period (zero based, periods only) of the science stream, for the first family of the structure. */
	PerfectWorld::SchoolLessonBinding Binding(PerfectWorld::SchoolStructure Structure, int32 Period, const PerfectWorld::SchoolSessionCatalogues& Cats) const
	{
		PerfectWorld::SchoolLessonBinding Binding;
		int32 Index = 0;
		for (const PerfectWorld::BellBlock& Block : Cats.Day.School->Blocks)
		{
			if (Block.Kind == PerfectWorld::BellBlockKind::Period && Index++ == Period)
			{
				Binding.Subject = Block.Subject;
			}
		}
		Binding.Id = "school.lesson.a2_" + std::to_string(Period);
		Binding.Stream = "science";
		Binding.Period = Period;
		Binding.Structure = Structure;
		Binding.FamilyId = Structure == PerfectWorld::SchoolStructure::Model ? Model.Families.front().Id : Explain.Families.front().Id;
		Binding.TitleTextKey = "phase2.a2.test.title";
		Binding.BriefTextKey = "phase2.a2.test.brief";
		Binding.RoleTextKey = "phase2.a2.test.role";
		return Binding;
	}
};

const FA2TFixture* A2TShared(FAutomationTestBase& Test)
{
	static TUniquePtr<FA2TFixture> Fixture;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		TUniquePtr<FA2TFixture> Loaded = MakeUnique<FA2TFixture>();
		if (Loaded->Load(Test))
		{
			Fixture = MoveTemp(Loaded);
		}
	}
	if (!Fixture.IsValid())
	{
		Test.AddError(TEXT("the shared lesson fixture is unavailable"));
	}
	return Fixture.Get();
}

int32 A2TBellMinute(const PerfectWorld::BellSchedule& School, int32 Period)
{
	int32 Index = 0;
	for (const PerfectWorld::BellBlock& Block : School.Blocks)
	{
		if (Block.Kind == PerfectWorld::BellBlockKind::Period && Index++ == Period)
		{
			return Block.StartMinute;
		}
	}
	return -1;
}

/** The day, planned and attended, standing at the bell of a period. */
bool A2TDayAtBell(FAutomationTestBase& Test, const PerfectWorld::SchoolSessionCatalogues& Cats, PerfectWorld::DayLoopState& OutDay, int64 CalendarDay = 0, int32 Period = 0)
{
	const PerfectWorld::GameInstant Midnight = PerfectWorld::CalendarDayStartInstant(CalendarDay);
	PerfectWorld::DayLoopState Day = PerfectWorld::NewDayLoopState(std::max<PerfectWorld::GameInstant>(0, Midnight + PerfectWorld::BellFirstBell(*Cats.Day.School)));
	PerfectWorld::DayCommand Command;
	Command.Kind = PerfectWorld::DayCommandKind::PlanDay;
	if (!PerfectWorld::ApplyDayCommand(Day, Cats.Day, Command).Ok())
	{
		Test.AddError(TEXT("the morning does not plan"));
		return false;
	}
	Command.Kind = PerfectWorld::DayCommandKind::ArriveSchool;
	if (!PerfectWorld::ApplyDayCommand(Day, Cats.Day, Command).Ok())
	{
		Test.AddError(TEXT("school arrival is refused"));
		return false;
	}
	const PerfectWorld::GameInstant Target = Midnight + A2TBellMinute(*Cats.Day.School, Period);
	if (Target > Day.Now)
	{
		Command.Kind = PerfectWorld::DayCommandKind::Pass;
		Command.Minutes = Target - Day.Now;
		if (!PerfectWorld::ApplyDayCommand(Day, Cats.Day, Command).Ok())
		{
			Test.AddError(TEXT("waiting for the bell is refused"));
			return false;
		}
	}
	OutDay = std::move(Day);
	return true;
}

bool A2TPassTo(const PerfectWorld::SchoolSessionCatalogues& Cats, PerfectWorld::DayLoopState& Day, int64 CalendarDay, int32 Period)
{
	const PerfectWorld::GameInstant Target = PerfectWorld::CalendarDayStartInstant(CalendarDay) + A2TBellMinute(*Cats.Day.School, Period);
	if (Target <= Day.Now)
	{
		return Target == Day.Now;
	}
	PerfectWorld::DayCommand Command;
	Command.Kind = PerfectWorld::DayCommandKind::Pass;
	Command.Minutes = Target - Day.Now;
	return PerfectWorld::ApplyDayCommand(Day, Cats.Day, Command).Ok();
}

// ---- Looking at the widgets -------------------------------------------------------------------------------------------------

void A2TWalk(const TSharedRef<SWidget>& Widget, const bool bShownAbove, const TFunctionRef<void(const TSharedRef<SWidget>&, bool)>& Visit)
{
	const bool bShown = bShownAbove && Widget->GetVisibility().IsVisible();
	Visit(Widget, bShown);
	FChildren* Children = Widget->GetChildren();
	for (int32 Index = 0; Children != nullptr && Index < Children->Num(); ++Index)
	{
		A2TWalk(Children->GetChildAt(Index), bShown, Visit);
	}
}

TArray<FString> A2TShownTexts(const TSharedRef<SWidget>& Root)
{
	Root->SlatePrepass(1.0f);
	TArray<FString> Texts;
	A2TWalk(Root, true, [&Texts](const TSharedRef<SWidget>& Widget, const bool bShown)
	{
		if (bShown && Widget->GetTypeAsString() == TEXT("STextBlock"))
		{
			Texts.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
		}
	});
	return Texts;
}

bool A2THas(const TArray<FString>& Texts, const FString& Exact) { return Texts.Contains(Exact); }

bool A2THasSubstring(const TArray<FString>& Texts, const FString& Part)
{
	for (const FString& Text : Texts)
	{
		if (Text.Contains(Part))
		{
			return true;
		}
	}
	return false;
}

int32 A2TCountTag(const TSharedRef<SWidget>& Root, const TCHAR* Tag)
{
	Root->SlatePrepass(1.0f);
	int32 Count = 0;
	A2TWalk(Root, true, [&Count, Tag](const TSharedRef<SWidget>& Widget, const bool bShown)
	{
		if (bShown && Widget->GetTag() == FName(Tag))
		{
			++Count;
		}
	});
	return Count;
}

/** The first shown button whose text is exactly this, or null. */
TSharedPtr<SButton> A2TFindButton(const TSharedRef<SWidget>& Root, const FString& Label)
{
	Root->SlatePrepass(1.0f);
	TSharedPtr<SButton> Target;
	A2TWalk(Root, true, [&Target, &Label](const TSharedRef<SWidget>& Widget, const bool bShown)
	{
		if (!Target.IsValid() && bShown && Widget->GetTypeAsString() == TEXT("SButton") && A2THas(A2TShownTexts(Widget), Label))
		{
			Target = StaticCastSharedRef<SButton>(Widget);
		}
	});
	return Target;
}

bool A2TClick(const TSharedRef<SWidget>& Root, const FString& Label)
{
	const TSharedPtr<SButton> Button = A2TFindButton(Root, Label);
	if (!Button.IsValid())
	{
		return false;
	}
	Button->SimulateClick();
	Root->SlatePrepass(1.0f);
	return true;
}

FString A2TWord(const TCHAR* Key) { return PWClassLesson::Text(Key).ToString(); }

FKeyEvent A2TKey(const FKey Key, const bool bRepeat = false)
{
	return FKeyEvent(Key, FModifierKeysState(), 0, bRepeat, 0, 0);
}

// ---- Playing a lesson as a player who knows the answer: the TEST reads the catalogue, the screens never do --------------------

/** Plays a foreground model lesson to its end through the panel's own controls, checking each charge against the core's quote. */
bool A2TPlayModel(FAutomationTestBase& Test, const TSharedRef<SPWClassModel>& Panel, const PerfectWorld::SchoolSessionsState& Sessions,
	const PerfectWorld::SchoolSessionCatalogues& Cats, int32& OutCharged)
{
	OutCharged = 0;
	for (int32 Guard = 0; Guard < 16 && Sessions.Active && !Sessions.Active->ReadyToClose; ++Guard)
	{
		const PerfectWorld::ModelCaseDef* Def = Cats.Structures.Model->FindCase(Sessions.Model.Exposure.CaseId);
		if (Def == nullptr)
		{
			Test.AddError(TEXT("the open case is not in the catalogue"));
			return false;
		}
		const PerfectWorld::ModelAction Wanted = PerfectWorld::ModelStandardAction(*Def, Sessions.Model.Exposure);
		PerfectWorld::SchoolLessonAction Quoted = Wanted;
		PerfectWorld::SchoolSessionResult Result;
		switch (Wanted.Kind)
		{
		case PerfectWorld::ModelActionKind::WorkedExample:
		{
			const PerfectWorld::SchoolLessonQuote Quote = PerfectWorld::QuoteSchoolLessonAction(Sessions, Cats, Sessions.Active->Session, Quoted);
			Test.TestTrue(TEXT("the screen shows the core's price for the worked example"),
				A2THas(A2TShownTexts(Panel), PWClassLesson::PriceText(Quote, Sessions.Model.Exposure.UnitsLeft).ToString()));
			Result = Panel->ExecuteWorkedExample();
			Test.TestEqual(TEXT("the worked example is charged at its quote"), Result.ChargedUnits, Quote.Units);
			break;
		}
		case PerfectWorld::ModelActionKind::CommitModel:
		{
			for (const std::string& Id : Wanted.Links)
			{
				Panel->ToggleLink(FString(UTF8_TO_TCHAR(Id.c_str())));
			}
			const PerfectWorld::SchoolLessonQuote Quote = PerfectWorld::QuoteSchoolLessonAction(Sessions, Cats, Sessions.Active->Session, Quoted);
			Panel->RefreshModel();
			Test.TestTrue(TEXT("the screen shows the core's price for committing the links"),
				A2THas(A2TShownTexts(Panel), PWClassLesson::PriceText(Quote, Sessions.Model.Exposure.UnitsLeft).ToString()));
			Result = Panel->ExecuteCommitModel();
			Test.TestEqual(TEXT("the model is charged at its quote"), Result.ChargedUnits, Quote.Units);
			break;
		}
		case PerfectWorld::ModelActionKind::Investigate:
		{
			Panel->SelectInvestigation(Wanted.Investigation);
			const PerfectWorld::SchoolLessonQuote Quote = PerfectWorld::QuoteSchoolLessonAction(Sessions, Cats, Sessions.Active->Session, Quoted);
			Result = Panel->ExecuteCommitInvestigation();
			Test.TestEqual(TEXT("the check is charged at its quote"), Result.ChargedUnits, Quote.Units);
			break;
		}
		case PerfectWorld::ModelActionKind::PresentClaim:
		{
			Panel->SelectClaim(Wanted.Claim);
			Panel->SetExplain(Wanted.Explain);
			const PerfectWorld::SchoolLessonQuote Quote = PerfectWorld::QuoteSchoolLessonAction(Sessions, Cats, Sessions.Active->Session, Quoted);
			Result = Panel->ExecutePresentClaim();
			Test.TestEqual(TEXT("the claim is charged at its quote"), Result.ChargedUnits, Quote.Units);
			break;
		}
		}
		if (!Result.Ok())
		{
			Test.AddError(FString::Printf(TEXT("the core refused a choice the screen sent: %s"), UTF8_TO_TCHAR(Result.Issue.Code.c_str())));
			return false;
		}
		OutCharged += Result.ChargedUnits;
	}
	return Sessions.Active && Sessions.Active->ReadyToClose;
}

bool A2TPlayExplain(FAutomationTestBase& Test, const TSharedRef<SPWClassExplain>& Panel, const PerfectWorld::SchoolSessionsState& Sessions,
	const PerfectWorld::SchoolSessionCatalogues& Cats, int32& OutCharged)
{
	OutCharged = 0;
	for (int32 Guard = 0; Guard < 16 && Sessions.Active && !Sessions.Active->ReadyToClose; ++Guard)
	{
		const PerfectWorld::ExplainCaseDef* Def = Cats.Structures.Explain->FindCase(Sessions.Explain.Exposure.CaseId);
		if (Def == nullptr)
		{
			Test.AddError(TEXT("the open case is not in the catalogue"));
			return false;
		}
		const PerfectWorld::ExplainAction Wanted = PerfectWorld::ExplainStandardAction(*Def, Sessions.Explain.Exposure);
		const PerfectWorld::SchoolLessonQuote Quote = PerfectWorld::QuoteSchoolLessonAction(Sessions, Cats, Sessions.Active->Session, PerfectWorld::SchoolLessonAction(Wanted));
		PerfectWorld::SchoolSessionResult Result;
		switch (Wanted.Kind)
		{
		case PerfectWorld::ExplainActionKind::ModelParagraph:
			Result = Panel->ExecuteModelParagraph();
			break;
		case PerfectWorld::ExplainActionKind::ChooseClaim:
			Panel->SelectClaim(FString(UTF8_TO_TCHAR(Wanted.Claim.c_str())));
			Result = Panel->ExecuteChooseClaim();
			break;
		case PerfectWorld::ExplainActionKind::PlaceEvidence:
			for (const std::string& Id : Wanted.Evidence)
			{
				Panel->ToggleEvidence(FString(UTF8_TO_TCHAR(Id.c_str())));
			}
			Result = Panel->ExecutePlaceEvidence();
			break;
		case PerfectWorld::ExplainActionKind::PlaceLimitation:
			if (Wanted.Limitation.empty())
			{
				Result = Panel->ExecuteStateNoLimitation();
			}
			else
			{
				Panel->SelectLimitation(FString(UTF8_TO_TCHAR(Wanted.Limitation.c_str())));
				Result = Panel->ExecutePlaceLimitation();
			}
			break;
		}
		Test.TestEqual(TEXT("each explain choice is charged at its quote"), Result.ChargedUnits, Quote.Units);
		if (!Result.Ok())
		{
			Test.AddError(FString::Printf(TEXT("the core refused a choice the screen sent: %s"), UTF8_TO_TCHAR(Result.Issue.Code.c_str())));
			return false;
		}
		OutCharged += Result.ChargedUnits;
	}
	return Sessions.Active && Sessions.Active->ReadyToClose;
}

bool A2TCompleted(const std::vector<PerfectWorld::DayLoopEvent>& Events)
{
	return std::any_of(Events.begin(), Events.end(), [](const PerfectWorld::DayLoopEvent& E) { return E.Kind == PerfectWorld::DayLoopEventKind::Completed; });
}

// ---- A bridge that answers from a script ----------------------------------------------------------------------------------------

class FA2TFakeBridge final : public PWClassLesson::IBridge
{
public:
	PerfectWorld::SchoolSessionCatalogues Catalogues;
	PerfectWorld::SchoolLessonBinding Binding;
	PerfectWorld::SchoolPeriodView Period;
	PerfectWorld::SchoolLessonView Lesson;
	PerfectWorld::SchoolSessionResult OpenAnswer;
	PerfectWorld::SchoolSessionResult ChooseAnswer;
	PerfectWorld::SchoolSessionResult CloseAnswer;

	int32 OpenCalls = 0;
	int32 ChooseCalls = 0;
	int32 CloseCalls = 0;
	uint64 LastChooseSession = 0;
	uint64 LastCloseSession = 0;
	bool bLastCloseCancel = false;
	int32 LastElapsed = -1;
	PerfectWorld::SchoolLessonMode LastMode = PerfectWorld::SchoolLessonMode::Attend;

	virtual const PerfectWorld::SchoolLessonBinding& GetBinding() const override { return Binding; }
	virtual const PerfectWorld::SchoolSessionCatalogues& GetCatalogues() const override { return Catalogues; }
	virtual PerfectWorld::SchoolPeriodView QueryPeriod() const override { return Period; }
	virtual PerfectWorld::SchoolLessonView QueryLesson() const override { return Lesson; }
	virtual PerfectWorld::SchoolSessionResult OpenLesson(PerfectWorld::SchoolLessonMode Mode, const PerfectWorld::SchoolLessonGoal&) override
	{
		++OpenCalls;
		LastMode = Mode;
		return OpenAnswer;
	}
	virtual PerfectWorld::SchoolLessonQuote QuoteAction(uint64, const PerfectWorld::SchoolLessonAction&) const override { return PerfectWorld::SchoolLessonQuote(); }
	virtual PerfectWorld::SchoolSessionResult ChooseAction(uint64 Session, const PerfectWorld::SchoolLessonAction&) override
	{
		++ChooseCalls;
		LastChooseSession = Session;
		return ChooseAnswer;
	}
	virtual PerfectWorld::SchoolSessionResult CloseLesson(uint64 Session, bool bCancel, int32 ElapsedMinutes) override
	{
		++CloseCalls;
		LastCloseSession = Session;
		bLastCloseCancel = bCancel;
		LastElapsed = ElapsedMinutes;
		return CloseAnswer;
	}
};
} // namespace

// ---- Words ------------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TTextBundleTest, "PerfectWorld.Phase2A2.TextTableMatchesTheJsonBundles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TTextBundleTest::RunTest(const FString& Parameters)
{
	// The one code table holds exactly the keys of both bundles, and no key is in both bundles.
	TSet<FString> JsonKeys;
	for (const TCHAR* File : {TEXT("Data/Text/en/phase2_a2.json"), TEXT("Data/Text/en/phase2_a3.json")})
	{
		const FString Path = FPaths::Combine(FPaths::ProjectDir(), File);
		FString Source;
		if (!TestTrue(TEXT("the bundle file is readable"), FFileHelper::LoadFileToString(Source, *Path)))
		{
			return false;
		}
		const PerfectWorld::JsonParseResult Parsed = PerfectWorld::ParseJson(TCHAR_TO_UTF8(*Source));
		if (!TestTrue(TEXT("the bundle parses"), Parsed.Value.has_value()))
		{
			return false;
		}
		const PerfectWorld::JsonValue* Strings = Parsed.Value->Find("strings");
		if (!TestTrue(TEXT("the bundle has strings"), Strings != nullptr && Strings->IsObject()))
		{
			return false;
		}
		for (const auto& Member : Strings->Object)
		{
			const FString Key = FString(UTF8_TO_TCHAR(Member.first.c_str()));
			TestFalse(*(Key + TEXT(": is in one bundle only")), JsonKeys.Contains(Key));
			JsonKeys.Add(Key);
			FText Found;
			if (TestTrue(*(Key + TEXT(": the code table has it")), PWClassLesson::FindText(Key, Found)))
			{
				TestEqual(*(Key + TEXT(": same words")), Found.ToString(), FString(UTF8_TO_TCHAR(Member.second.String.c_str())));
			}
		}
	}
	for (const FString& Key : PWClassLesson::KnownTextKeys())
	{
		TestTrue(*(Key + TEXT(": the bundle has it")), JsonKeys.Contains(Key));
	}

	const FString Unknown = PWClassLesson::IssueText("nothing.known.here").ToString();
	TestEqual(TEXT("an unknown refusal reads neutrally"), Unknown, PWClassLesson::Text(TEXT("phase2.a2.issue.unknown")).ToString());
	for (const char* Code : {"school.session.closed_day", "school.session.foreground_limit", "school.session.stale_session", "model.units_short",
		"structure.units_short", "structure.bad_choice", "dayloop.busy"})
	{
		const FString Words = PWClassLesson::IssueText(Code).ToString();
		TestFalse(*(FString(UTF8_TO_TCHAR(Code)) + TEXT(": has its own sentence")), Words.IsEmpty() || Words == Unknown);
		TestFalse(*(FString(UTF8_TO_TCHAR(Code)) + TEXT(": never shows the code")), Words.Contains(TEXT("school.session")) || Words.Contains(TEXT("_")));
	}
	TestEqual(TEXT("an unknown text key reads neutrally, never as the key"), PWClassLesson::Text(TEXT("phase2.a2.no.such.key")).ToString(),
		FString(TEXT("Text unavailable.")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TLayoutTest, "PerfectWorld.Phase2A2.LayoutRulesFitEveryWindowAndTextScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TLayoutTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("853x480 is compact"), PWClassLesson::Compact(FVector2D(853.0, 480.0)));
	TestTrue(TEXT("853x480 at 150% text (569x320 logical) is compact"), PWClassLesson::Compact(FVector2D(853.0 / 1.5, 480.0 / 1.5)));
	TestTrue(TEXT("1920x1080 is two pane"), PWClassLesson::TwoPane(FVector2D(1920.0, 1080.0)));
	TestFalse(TEXT("1920x1080 is not compact"), PWClassLesson::Compact(FVector2D(1920.0, 1080.0)));
	for (const FVector2D Space : {FVector2D(569.0, 320.0), FVector2D(853.0, 480.0), FVector2D(1280.0, 720.0), FVector2D(1920.0, 1080.0), FVector2D(2560.0, 1440.0)})
	{
		const FString Name = FString::Printf(TEXT("%dx%d"), static_cast<int32>(Space.X), static_cast<int32>(Space.Y));
		TestTrue(*(Name + TEXT(": the card is not wider than the space")), PWClassLesson::PanelWidth(Space) <= static_cast<float>(Space.X));
		TestTrue(*(Name + TEXT(": the card is not taller than the space")), PWClassLesson::PanelMaxHeight(Space) <= static_cast<float>(Space.Y));
		TestTrue(*(Name + TEXT(": the card is usable")), PWClassLesson::PanelWidth(Space) >= 280.0f && PWClassLesson::PanelMaxHeight(Space) >= 200.0f);
	}
	return true;
}

// ---- The model screen through the core --------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TModelFlowTest, "PerfectWorld.Phase2A2.ModelLessonPlaysThroughTheCoreAndChargesItsQuotes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TModelFlowTest::RunTest(const FString& Parameters)
{
	const FA2TFixture* F = A2TShared(*this);
	PerfectWorld::SchoolSessionCatalogues Cats;
	if (F == nullptr || !TestTrue(TEXT("a science timetable"), F->Cats(Cats)))
	{
		return false;
	}
	PerfectWorld::SchoolSessionsState Sessions;
	PerfectWorld::DayLoopState Day;
	if (!A2TDayAtBell(*this, Cats, Day))
	{
		return false;
	}
	const PerfectWorld::SchoolLessonBinding Binding = F->Binding(PerfectWorld::SchoolStructure::Model, 0, Cats);
	const TSharedRef<PWClassLesson::FCoreBridge> Bridge = MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, Binding, 100000, 20261025ULL);
	int32 Closed = 0;
	const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel)
		.Bridge(Bridge)
		.Space(FVector2D(1920.0, 1080.0))
		.OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));

	// The period screen: three ways to take it, the foreground one open, the routine one refused with the core's own reason.
	TArray<FString> Texts = A2TShownTexts(Panel);
	TestTrue(TEXT("the lesson is named after its family when no title text exists"), A2THas(Texts, FString(UTF8_TO_TCHAR(F->Model.Families.front().Name.c_str()))));
	TestEqual(TEXT("three ways to take the period and three goals"), A2TCountTag(Panel, PWClassLesson::RowTag), 6);
	if (!TestTrue(TEXT("all three modes and Start are offered"), Panel->HasAction(TEXT("mode.foreground")) && Panel->HasAction(TEXT("mode.routine")) &&
		Panel->HasAction(TEXT("mode.attend")) && Panel->HasAction(TEXT("open"))))
	{
		return false;
	}
	TestTrue(TEXT("foreground is open"), Panel->IsActionEnabled(TEXT("mode.foreground")));
	TestFalse(TEXT("routine needs mastery"), Panel->IsActionEnabled(TEXT("mode.routine")));
	TestTrue(TEXT("the routine refusal is the core's reason in words"), A2THas(Texts, A2TWord(TEXT("school.session.not_secure"))));
	TestTrue(TEXT("the goal is chosen first"), A2THas(Texts, A2TWord(TEXT("phase2.a2.model.goal.test_measurements"))));

	TestTrue(TEXT("the goal is chosen first"), Panel->SetGoalIndex(1));
	const PerfectWorld::SchoolSessionResult Opened = Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground);
	if (!TestTrue(TEXT("the core opens the lesson"), Opened.Ok()) || !TestTrue(TEXT("a session exists"), Sessions.Active.has_value()))
	{
		return false;
	}
	TestEqual(TEXT("the screen shows the session the core opened"), Panel->GetDisplayedSession(), Opened.Session);
	TestTrue(TEXT("the chosen goal reached the core"), Sessions.Model.Exposure.Goal == PerfectWorld::ModelGoal::TestMeasurements);
	Texts = A2TShownTexts(Panel);
	TestTrue(TEXT("the first step is named"), A2THas(Texts, A2TWord(TEXT("phase2.a2.model.beat.build_model"))));
	TestFalse(TEXT("no result is shown before the lesson resolves"), A2THasSubstring(Texts, TEXT("Result:")));
	TestFalse(TEXT("the hidden fault is never named"), A2THasSubstring(Texts, TEXT("hidden_flow")));
	TestTrue(TEXT("units are shown"), A2THasSubstring(Texts, TEXT("Time left:")));
	TestTrue(TEXT("every link is a choice"), A2TCountTag(Panel, PWClassLesson::RowTag) >= 2);

	int32 Charged = 0;
	if (!TestTrue(TEXT("the lesson plays to its end"), A2TPlayModel(*this, Panel, Sessions, Cats, Charged)))
	{
		return false;
	}
	const PerfectWorld::ModelOutcome* Outcome = std::get_if<PerfectWorld::ModelOutcome>(&Sessions.Active->Outcome);
	if (!TestTrue(TEXT("a typed model outcome"), Outcome != nullptr))
	{
		return false;
	}
	TestEqual(TEXT("units spent are the sum of the charges"), Outcome->UnitsSpent, Charged);
	Texts = A2TShownTexts(Panel);
	TestTrue(TEXT("the core's grade is shown"), A2THas(Texts, A2TWord(TEXT("phase2.a2.common.grade.sound"))));
	TestEqual(TEXT("the outcome block"), A2TCountTag(Panel, PWClassLesson::OutcomeTag), 1);
	TestEqual(TEXT("nothing is credited before the close"), A2TCompleted(Panel->GetLastResult().Events) ? 1 : 0, 0);
	TestTrue(TEXT("a ready lesson offers the close"), A2TFindButton(Panel, A2TWord(TEXT("phase2.a2.common.complete_lesson"))).IsValid());
	TestFalse(TEXT("a resolved foreground lesson cannot be cancelled for free mastery"),
		A2TFindButton(Panel, A2TWord(TEXT("phase2.a3.common.leave"))).IsValid());

	if (!TestTrue(TEXT("the close button is pressed"), A2TClick(Panel, A2TWord(TEXT("phase2.a2.common.complete_lesson")))))
	{
		return false;
	}
	TestFalse(TEXT("the session is closed"), Sessions.Active.has_value());
	TestTrue(TEXT("the period is recorded as completed"), !Sessions.Periods.empty() && Sessions.Periods.back().Completed && !Sessions.Periods.back().Cancelled);
	TestTrue(TEXT("learning was emitted once, at the close"), A2TCompleted(Panel->GetLastResult().Events));
	TestTrue(TEXT("the summary is shown"), Panel->IsLessonClosed() && A2TCountTag(Panel, PWClassLesson::SummaryTag) == 1);

	// A second close of the same session is the core's refusal, never a second credit.
	const PerfectWorld::SchoolSessionsState Before = Sessions;
	const PerfectWorld::SchoolSessionResult Again = Bridge->CloseLesson(Opened.Session, false, 0);
	TestFalse(TEXT("a second close is refused"), Again.Ok());
	TestTrue(TEXT("and changes nothing"), Sessions == Before);

	Panel->RequestClose();
	TestEqual(TEXT("leaving tells the host once"), Closed, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TCancelTest, "PerfectWorld.Phase2A2.CancellationGivesNoLearningAndStillSpendsTheForegroundSlot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TCancelTest::RunTest(const FString& Parameters)
{
	const FA2TFixture* F = A2TShared(*this);
	PerfectWorld::SchoolSessionCatalogues Cats;
	if (F == nullptr || !TestTrue(TEXT("a science timetable"), F->Cats(Cats)))
	{
		return false;
	}
	PerfectWorld::SchoolSessionsState Sessions;
	PerfectWorld::DayLoopState Day;
	if (!A2TDayAtBell(*this, Cats, Day))
	{
		return false;
	}

	// Period 0: begin, spend some working time, cancel by pressing the button twice.
	{
		const TSharedRef<PWClassLesson::FCoreBridge> Bridge =
			MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, F->Binding(PerfectWorld::SchoolStructure::Model, 0, Cats), 100000, 7ULL);
		const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel).Bridge(Bridge).Space(FVector2D(1280.0, 720.0));
		if (!TestTrue(TEXT("opens"), Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok()))
		{
			return false;
		}
		const PerfectWorld::ModelCaseDef* Def = Cats.Structures.Model->FindCase(Sessions.Model.Exposure.CaseId);
		if (Def == nullptr)
		{
			return false;
		}
		const PerfectWorld::ModelAction First = PerfectWorld::ModelStandardAction(*Def, Sessions.Model.Exposure);
		if (First.Kind == PerfectWorld::ModelActionKind::WorkedExample)
		{
			TestTrue(TEXT("a first choice is made"), Panel->ExecuteWorkedExample().Ok());
		}
		else
		{
			for (const std::string& Id : First.Links)
			{
				Panel->ToggleLink(FString(UTF8_TO_TCHAR(Id.c_str())));
			}
			TestTrue(TEXT("a first choice is made"), Panel->ExecuteCommitModel().Ok());
		}
		TestTrue(TEXT("a foreground lesson can be cancelled"), A2TFindButton(Panel, A2TWord(TEXT("phase2.a3.common.leave"))).IsValid());
		TestTrue(TEXT("first press arms"), A2TClick(Panel, A2TWord(TEXT("phase2.a3.common.leave"))));
		TestTrue(TEXT("armed, and nothing is cancelled yet"), Panel->IsLeaveArmed() && Sessions.Active.has_value());
		TestTrue(TEXT("the button now asks for confirmation"), A2TFindButton(Panel, A2TWord(TEXT("phase2.a2.common.confirm_cancel"))).IsValid());
		TestTrue(TEXT("second press cancels"), A2TClick(Panel, A2TWord(TEXT("phase2.a2.common.confirm_cancel"))));
		TestFalse(TEXT("the session is gone"), Sessions.Active.has_value());
		TestTrue(TEXT("recorded as cancelled, not completed"), Sessions.Periods.back().Cancelled && !Sessions.Periods.back().Completed);
		TestFalse(TEXT("cancellation emits no learning"), A2TCompleted(Panel->GetLastResult().Events));
		TestTrue(TEXT("the summary says it was cancelled"),
			A2THas(A2TShownTexts(Panel), A2TWord(TEXT("phase2.a2.common.summary.cancelled"))) && Panel->IsLessonClosed());
	}

	// Period 1: a second foreground lesson is allowed; cancel it directly.
	if (!TestTrue(TEXT("to the next bell"), A2TPassTo(Cats, Day, 0, 1)))
	{
		return false;
	}
	{
		const TSharedRef<PWClassLesson::FCoreBridge> Bridge =
			MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, F->Binding(PerfectWorld::SchoolStructure::Model, 1, Cats), 100000, 8ULL);
		const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel).Bridge(Bridge).Space(FVector2D(1280.0, 720.0));
		TestTrue(TEXT("the second foreground lesson opens"), Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok());
		TestTrue(TEXT("and cancels"), Panel->ExecuteClose(true).Ok());
	}

	// Period 2: the third foreground lesson of the day is refused by the core, and the screen says so in words.
	if (!TestTrue(TEXT("to the third bell"), A2TPassTo(Cats, Day, 0, 2)))
	{
		return false;
	}
	{
		const TSharedRef<PWClassLesson::FCoreBridge> Bridge =
			MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, F->Binding(PerfectWorld::SchoolStructure::Model, 2, Cats), 100000, 9ULL);
		const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel).Bridge(Bridge).Space(FVector2D(1280.0, 720.0));
		TestTrue(TEXT("the foreground choice is there and closed"), Panel->HasAction(TEXT("mode.foreground")) && !Panel->IsActionEnabled(TEXT("mode.foreground")));
		TestTrue(TEXT("the limit is explained"), A2THas(A2TShownTexts(Panel), A2TWord(TEXT("school.session.foreground_limit"))));
		const PerfectWorld::SchoolSessionsState Before = Sessions;
		const PerfectWorld::SchoolSessionResult Refused = Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground);
		TestFalse(TEXT("the core refuses a third"), Refused.Ok());
		TestEqual(TEXT("with its own code"), Panel->GetLastIssueCode(), FString(TEXT("school.session.foreground_limit")));
		TestTrue(TEXT("nothing changed"), Sessions == Before);
		TestEqual(TEXT("the status line is the sentence"), Panel->GetBanner().ToString(), A2TWord(TEXT("school.session.foreground_limit")));
		// Attend is still a legitimate choice and never costs a foreground slot.
		TestTrue(TEXT("attend remains open"), Panel->IsActionEnabled(TEXT("mode.attend")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TRoutineParityTest, "PerfectWorld.Phase2A2.RoutineAndCarefulForegroundEarnTheSameLearningAndTime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TRoutineParityTest::RunTest(const FString& Parameters)
{
	const FA2TFixture* F = A2TShared(*this);
	PerfectWorld::SchoolSessionCatalogues Cats;
	if (F == nullptr || !TestTrue(TEXT("a science timetable"), F->Cats(Cats)))
	{
		return false;
	}
	const PerfectWorld::SchoolLessonBinding Binding = F->Binding(PerfectWorld::SchoolStructure::Model, 0, Cats);
	PerfectWorld::SchoolSessionsState Sessions;
	int64 CalendarDay = 0;
	// Earn mastery by actually playing foreground lessons through the screen, one a day.
	for (int32 Round = 0; Round < 12 && !PerfectWorld::ModelFamilySecure(Sessions.Model, Binding.FamilyId); ++Round)
	{
		while (!PerfectWorld::AttendanceIsSchoolDay(Cats.Day.Calendar, CalendarDay))
		{
			++CalendarDay;
		}
		PerfectWorld::DayLoopState Day;
		if (!A2TDayAtBell(*this, Cats, Day, CalendarDay++))
		{
			return false;
		}
		const TSharedRef<PWClassLesson::FCoreBridge> Bridge = MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, Binding, 100000, 17ULL);
		const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel).Bridge(Bridge).Space(FVector2D(1280.0, 720.0));
		int32 Charged = 0;
		if (!TestTrue(TEXT("a practice lesson opens"), Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok()) ||
			!A2TPlayModel(*this, Panel, Sessions, Cats, Charged) || !TestTrue(TEXT("and closes"), Panel->ExecuteClose(false).Ok()))
		{
			return false;
		}
	}
	if (!TestTrue(TEXT("mastery was earned by play, not granted"), PerfectWorld::ModelFamilySecure(Sessions.Model, Binding.FamilyId)))
	{
		return false;
	}
	while (!PerfectWorld::AttendanceIsSchoolDay(Cats.Day.Calendar, CalendarDay))
	{
		++CalendarDay;
	}
	PerfectWorld::DayLoopState RoutineDay;
	if (!A2TDayAtBell(*this, Cats, RoutineDay, CalendarDay))
	{
		return false;
	}
	PerfectWorld::DayLoopState ManualDay = RoutineDay;
	PerfectWorld::SchoolSessionsState ManualSessions = Sessions;

	std::vector<PerfectWorld::DayLoopEvent> RoutineEvents;
	{
		const TSharedRef<PWClassLesson::FCoreBridge> Bridge = MakeShared<PWClassLesson::FCoreBridge>(Sessions, RoutineDay, Cats, Binding, 100000, 17ULL);
		const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel).Bridge(Bridge).Space(FVector2D(1280.0, 720.0));
		TestTrue(TEXT("routine is offered once the method is secure"), Panel->IsActionEnabled(TEXT("mode.routine")));
		TestTrue(TEXT("mastery is stated"), A2THas(A2TShownTexts(Panel), A2TWord(TEXT("phase2.a2.common.mastery.secure"))));
		if (!TestTrue(TEXT("the routine button is pressed"), Panel->PressAction(TEXT("mode.routine")) && Panel->PressAction(TEXT("open"))) ||
			!TestTrue(TEXT("routine is ready at once"), Sessions.Active.has_value() && Sessions.Active->ReadyToClose))
		{
			return false;
		}
		TestTrue(TEXT("the routine screen says no manual play"), A2THas(A2TShownTexts(Panel), A2TWord(TEXT("phase2.a2.common.outcome.routine"))));
		if (!TestTrue(TEXT("routine closes"), Panel->ExecuteClose(false).Ok()))
		{
			return false;
		}
		RoutineEvents = Panel->GetLastResult().Events;
	}
	std::vector<PerfectWorld::DayLoopEvent> ManualEvents;
	{
		const TSharedRef<PWClassLesson::FCoreBridge> Bridge =
			MakeShared<PWClassLesson::FCoreBridge>(ManualSessions, ManualDay, Cats, Binding, 100000, 17ULL);
		const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel).Bridge(Bridge).Space(FVector2D(1280.0, 720.0));
		int32 Charged = 0;
		if (!TestTrue(TEXT("the careful foreground lesson opens"), Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok()) ||
			!A2TPlayModel(*this, Panel, ManualSessions, Cats, Charged) || !TestTrue(TEXT("and closes"), Panel->ExecuteClose(false).Ok()))
		{
			return false;
		}
		ManualEvents = Panel->GetLastResult().Events;
	}
	if (!TestTrue(TEXT("both emitted learning"), A2TCompleted(RoutineEvents) && A2TCompleted(ManualEvents)))
	{
		return false;
	}
	const auto Credited = [](const std::vector<PerfectWorld::DayLoopEvent>& Events)
	{
		std::vector<PerfectWorld::SkillCredit> All;
		for (const PerfectWorld::DayLoopEvent& Event : Events)
		{
			if (Event.Kind == PerfectWorld::DayLoopEventKind::Completed)
			{
				All.insert(All.end(), Event.Credited.begin(), Event.Credited.end());
			}
		}
		return All;
	};
	TestTrue(TEXT("identical normal learning"), Credited(RoutineEvents) == Credited(ManualEvents));
	TestEqual(TEXT("identical time"), RoutineDay.Now, ManualDay.Now);
	return true;
}

// ---- Refusals, stale screens and keys, through a scripted world ---------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TFakeBridgeTest, "PerfectWorld.Phase2A2.ScreenSendsOnlyCommandsWithItsSessionAndShowsRefusalsInWords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TFakeBridgeTest::RunTest(const FString& Parameters)
{
	const TSharedRef<FA2TFakeBridge> Fake = MakeShared<FA2TFakeBridge>();
	Fake->Binding.Id = "school.lesson.fake";
	Fake->Binding.Structure = PerfectWorld::SchoolStructure::Model;
	Fake->Binding.FamilyId = "family.fake";
	Fake->Period.Issue = PerfectWorld::SchoolIssue();
	Fake->Period.ForegroundAvailable = true;
	Fake->Period.AttendAvailable = true;
	Fake->Period.StartMinute = 600;
	Fake->Period.EndMinute = 645;
	Fake->OpenAnswer.Issue.Code = "school.session.not_at_bell";

	const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel).Bridge(Fake).Space(FVector2D(1280.0, 720.0));
	const PerfectWorld::SchoolSessionResult Refused = Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Attend);
	TestFalse(TEXT("the bridge's refusal is passed on"), Refused.Ok());
	TestEqual(TEXT("one open command"), Fake->OpenCalls, 1);
	TestEqual(TEXT("the mode is the one chosen"), static_cast<int32>(Fake->LastMode), static_cast<int32>(PerfectWorld::SchoolLessonMode::Attend));
	TestEqual(TEXT("the code is kept"), Panel->GetLastIssueCode(), FString(TEXT("school.session.not_at_bell")));
	TestEqual(TEXT("the sentence is shown"), Panel->GetBanner().ToString(), A2TWord(TEXT("school.session.not_at_bell")));
	TestTrue(TEXT("on screen"), A2THas(A2TShownTexts(Panel), A2TWord(TEXT("school.session.not_at_bell"))));
	TestFalse(TEXT("no session appears"), Panel->GetDisplayedSession() != 0);

	// A lesson the world says is running, with session 7: every command carries 7.
	PerfectWorld::SchoolLessonRun Run;
	Run.Session = 7;
	Run.Binding = Fake->Binding;
	Run.Mode = PerfectWorld::SchoolLessonMode::Foreground;
	Fake->Lesson.Run = Run;
	Fake->Lesson.Model.Exposure.FamilyId = "family.fake";
	Fake->Lesson.Model.Exposure.CaseId = "case.fake";
	Fake->Lesson.Model.Exposure.UnitsTotal = 9;
	Fake->Lesson.Model.Exposure.UnitsLeft = 5;
	Panel->RefreshModel();
	TestEqual(TEXT("the screen shows session 7"), Panel->GetDisplayedSession(), static_cast<uint64>(7));
	Fake->ChooseAnswer.Issue.Code = "school.session.stale_session";
	const PerfectWorld::SchoolSessionResult Stale = Panel->ExecuteWorkedExample();
	TestFalse(TEXT("a stale choice is refused by the world"), Stale.Ok());
	TestEqual(TEXT("and carried the displayed session"), Fake->LastChooseSession, static_cast<uint64>(7));
	TestEqual(TEXT("its sentence is shown"), Panel->GetBanner().ToString(), A2TWord(TEXT("school.session.stale_session")));

	// Cancelling reports the working time spent so far: (9 - 5) units of five minutes.
	const PerfectWorld::SchoolSessionResult Cancelled = Panel->ExecuteClose(true);
	TestEqual(TEXT("one close command"), Fake->CloseCalls, 1);
	TestTrue(TEXT("a cancel"), Fake->bLastCloseCancel);
	TestEqual(TEXT("with session 7"), Fake->LastCloseSession, static_cast<uint64>(7));
	TestEqual(TEXT("and the elapsed working time"), Fake->LastElapsed, 20);
	TestTrue(TEXT("the fake accepts"), Cancelled.Ok());
	TestTrue(TEXT("after the core accepted it the screen is closed and sends nothing more"), Panel->IsLessonClosed() && !Panel->ExecuteClose(true).Ok() && Fake->CloseCalls == 1);

	// A normal close sends elapsed 0, on a fresh screen over the finished lesson.
	Fake->Lesson.Run->ReadyToClose = true;
	{
		const TSharedRef<SPWClassModel> Finished = SNew(SPWClassModel).Bridge(Fake).Space(FVector2D(1280.0, 720.0));
		TestTrue(TEXT("a lesson left open is resumed by a new screen"), Finished->GetDisplayedSession() == 7);
		Finished->ExecuteClose(false);
		TestTrue(TEXT("a normal close"), !Fake->bLastCloseCancel && Fake->LastElapsed == 0 && Fake->LastCloseSession == 7);
	}

	// A lesson open for another period is never driven by this screen.
	Fake->Lesson.Run->Binding.Id = "school.lesson.other";
	{
		const TSharedRef<SPWClassModel> Other = SNew(SPWClassModel).Bridge(Fake).Space(FVector2D(1280.0, 720.0));
		TestTrue(TEXT("another lesson is open, and the screen says so"), A2THas(A2TShownTexts(Other), A2TWord(TEXT("phase2.a3.common.other_lesson"))));
		TestFalse(TEXT("and Leave early is not offered for it"), Other->CanLeaveEarly());
		const int32 Calls = Fake->CloseCalls;
		TestFalse(TEXT("nor can it be closed from here"), Other->ExecuteClose(false).Ok());
		TestEqual(TEXT("no command was sent"), Fake->CloseCalls, Calls);
	}

	// A binding of another structure is not this screen's.
	Fake->Binding.Structure = PerfectWorld::SchoolStructure::Explain;
	{
		const TSharedRef<SPWClassModel> Wrong = SNew(SPWClassModel).Bridge(Fake).Space(FVector2D(1280.0, 720.0));
		TestTrue(TEXT("the wrong kind of lesson is refused in words"), A2THas(A2TShownTexts(Wrong), A2TWord(TEXT("phase2.a2.common.wrong_screen"))));
		const int32 Opens = Fake->OpenCalls;
		TestFalse(TEXT("and never driven"), Wrong->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok());
		TestEqual(TEXT("no open was sent"), Fake->OpenCalls, Opens);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TKeysTest, "PerfectWorld.Phase2A2.LeavingKeepsTheLessonOpenAndAHeldKeyLeavesOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TKeysTest::RunTest(const FString& Parameters)
{
	const FA2TFixture* F = A2TShared(*this);
	PerfectWorld::SchoolSessionCatalogues Cats;
	if (F == nullptr || !TestTrue(TEXT("a science timetable"), F->Cats(Cats)))
	{
		return false;
	}
	PerfectWorld::SchoolSessionsState Sessions;
	PerfectWorld::DayLoopState Day;
	if (!A2TDayAtBell(*this, Cats, Day))
	{
		return false;
	}
	const TSharedRef<PWClassLesson::FCoreBridge> Bridge =
		MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, F->Binding(PerfectWorld::SchoolStructure::Model, 0, Cats), 100000, 5ULL);
	int32 Closed = 0;
	const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel)
		.Bridge(Bridge)
		.Space(FVector2D(853.0, 480.0))
		.OnClose(FSimpleDelegate::CreateLambda([&Closed]() { ++Closed; }));
	if (!TestTrue(TEXT("opens"), Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok()))
	{
		return false;
	}
	const uint64 Session = Sessions.Active->Session;
	TestTrue(TEXT("Esc is handled"), Panel->OnKeyDown(FGeometry(), A2TKey(EKeys::Escape)).IsEventHandled());
	TestEqual(TEXT("Esc leaves"), Closed, 1);
	TestTrue(TEXT("a held Esc is handled"), Panel->OnKeyDown(FGeometry(), A2TKey(EKeys::Escape, true)).IsEventHandled());
	TestEqual(TEXT("and does not leave again"), Closed, 1);
	TestTrue(TEXT("B on a controller leaves"), Panel->OnKeyDown(FGeometry(), A2TKey(EKeys::Gamepad_FaceButton_Right)).IsEventHandled());
	TestEqual(TEXT("once more"), Closed, 2);
	TestTrue(TEXT("leaving did not cancel the lesson"), Sessions.Active.has_value() && Sessions.Active->Session == Session);

	// Reopening the screen on the same world resumes the same session.
	const TSharedRef<SPWClassModel> Again = SNew(SPWClassModel).Bridge(Bridge).Space(FVector2D(853.0, 480.0));
	TestEqual(TEXT("the same session is shown"), Again->GetDisplayedSession(), Session);
	TestTrue(TEXT("with its first step"), A2THas(A2TShownTexts(Again), A2TWord(TEXT("phase2.a2.model.beat.build_model"))));
	TestTrue(TEXT("and says it is a lesson resumed"), A2THas(A2TShownTexts(Again), A2TWord(TEXT("phase2.a2.common.resumed"))));
	TestTrue(TEXT("Back and Leave early are both on screen at 853x480"),
		A2TFindButton(Again, A2TWord(TEXT("phase2.a3.common.back"))).IsValid() && A2TFindButton(Again, A2TWord(TEXT("phase2.a3.common.leave"))).IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TFitTest, "PerfectWorld.Phase2A2.ScreenFitsEveryWindowAndSwitchesPanes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TFitTest::RunTest(const FString& Parameters)
{
	const FA2TFixture* F = A2TShared(*this);
	PerfectWorld::SchoolSessionCatalogues Cats;
	if (F == nullptr || !TestTrue(TEXT("a science timetable"), F->Cats(Cats)))
	{
		return false;
	}
	PerfectWorld::SchoolSessionsState Sessions;
	PerfectWorld::DayLoopState Day;
	if (!A2TDayAtBell(*this, Cats, Day))
	{
		return false;
	}
	const TSharedRef<PWClassLesson::FCoreBridge> Bridge =
		MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, F->Binding(PerfectWorld::SchoolStructure::Model, 0, Cats), 100000, 5ULL);
	const TSharedRef<FVector2D> Space = MakeShared<FVector2D>(1920.0, 1080.0);
	const TSharedRef<SPWClassModel> Panel = SNew(SPWClassModel)
		.Bridge(Bridge)
		.Space(TAttribute<FVector2D>::CreateLambda([Space]() { return *Space; }));
	for (const FVector2D Size : {FVector2D(569.0, 320.0), FVector2D(853.0, 480.0), FVector2D(1280.0, 720.0), FVector2D(1920.0, 1080.0)})
	{
		*Space = Size;
		Panel->RefreshLayout();
		Panel->SlatePrepass(1.0f);
		const FVector2D Desired = Panel->GetDesiredSize();
		const FString Name = FString::Printf(TEXT("%dx%d"), static_cast<int32>(Size.X), static_cast<int32>(Size.Y));
		TestTrue(*(Name + TEXT(": the period screen is not wider than the space")), Desired.X <= Size.X + 0.5);
		TestTrue(*(Name + TEXT(": the period screen is not taller than the space")), Desired.Y <= Size.Y + 0.5);
		TestFalse(*(Name + TEXT(": the period screen is one pane")), Panel->IsSplit());
	}
	if (!TestTrue(TEXT("opens"), Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok()))
	{
		return false;
	}
	for (const FVector2D Size : {FVector2D(569.0, 320.0), FVector2D(853.0, 480.0), FVector2D(1920.0, 1080.0), FVector2D(1280.0, 720.0)})
	{
		*Space = Size;
		const bool bChanged = Panel->RefreshLayout();
		Panel->SlatePrepass(1.0f);
		const FVector2D Desired = Panel->GetDesiredSize();
		const FString Name = FString::Printf(TEXT("%dx%d"), static_cast<int32>(Size.X), static_cast<int32>(Size.Y));
		TestTrue(*(Name + TEXT(": the running lesson is not wider than the space")), Desired.X <= Size.X + 0.5);
		TestTrue(*(Name + TEXT(": the running lesson is not taller than the space")), Desired.Y <= Size.Y + 0.5);
		TestEqual(*(Name + TEXT(": the running lesson is split exactly when there is room")), Panel->IsSplit(), PWClassLesson::TwoPane(Size));
		TestTrue(*(Name + TEXT(": the first step is on screen")), A2THas(A2TShownTexts(Panel), A2TWord(TEXT("phase2.a2.model.beat.build_model"))));
		(void)bChanged;
	}
	TestTrue(TEXT("Back is always on screen"), A2TFindButton(Panel, A2TWord(TEXT("phase2.a3.common.back"))).IsValid());
	TestTrue(TEXT("and so is Leave early while a lesson runs"), A2TFindButton(Panel, A2TWord(TEXT("phase2.a3.common.leave"))).IsValid());
	return true;
}

// ---- The explanation screen ------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TExplainFlowTest, "PerfectWorld.Phase2A2.ExplainLessonPlaysThroughTheCoreWithoutRevealingASolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TExplainFlowTest::RunTest(const FString& Parameters)
{
	const FA2TFixture* F = A2TShared(*this);
	PerfectWorld::SchoolSessionCatalogues Cats;
	if (F == nullptr || !TestTrue(TEXT("a science timetable"), F->Cats(Cats)))
	{
		return false;
	}
	PerfectWorld::SchoolSessionsState Sessions;
	PerfectWorld::DayLoopState Day;
	if (!A2TDayAtBell(*this, Cats, Day))
	{
		return false;
	}
	const TSharedRef<PWClassLesson::FCoreBridge> Bridge =
		MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, F->Binding(PerfectWorld::SchoolStructure::Explain, 0, Cats), 100000, 31ULL);
	const TSharedRef<SPWClassExplain> Panel = SNew(SPWClassExplain).Bridge(Bridge).Space(FVector2D(1920.0, 1080.0));
	TestTrue(TEXT("the explain goals are offered"), A2THas(A2TShownTexts(Panel), A2TWord(TEXT("phase2.a2.explain.goal.state_limits"))));
	TestTrue(TEXT("the third goal is chosen"), Panel->SetGoalIndex(2));
	if (!TestTrue(TEXT("opens"), Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok()))
	{
		return false;
	}
	TestTrue(TEXT("the chosen goal reached the core"), Sessions.Explain.Exposure.Goal == PerfectWorld::ExplainGoal::StateLimits);
	const PerfectWorld::ExplainCaseDef* Def = Cats.Structures.Explain->FindCase(Sessions.Explain.Exposure.CaseId);
	if (!TestTrue(TEXT("the case is known"), Def != nullptr))
	{
		return false;
	}
	TArray<FString> Texts = A2TShownTexts(Panel);
	TestTrue(TEXT("the case brief is shown"), A2THas(Texts, FString(UTF8_TO_TCHAR(Def->Brief.c_str()))));
	for (const PerfectWorld::ExplainClaim& Claim : Def->Claims)
	{
		TestTrue(TEXT("every claim is offered the same way"), A2THas(Texts, FString(UTF8_TO_TCHAR(Claim.Name.c_str()))));
	}
	TestFalse(TEXT("no word of defensibility is shown before the lesson resolves"), A2THasSubstring(Texts, TEXT("efensible")));
	TestFalse(TEXT("no result is shown before the lesson resolves"), A2THasSubstring(Texts, TEXT("Result:")));

	int32 Charged = 0;
	if (!TestTrue(TEXT("the lesson plays to its end"), A2TPlayExplain(*this, Panel, Sessions, Cats, Charged)))
	{
		return false;
	}
	const PerfectWorld::ExplainOutcome* Outcome = std::get_if<PerfectWorld::ExplainOutcome>(&Sessions.Active->Outcome);
	if (!TestTrue(TEXT("a typed explain outcome"), Outcome != nullptr))
	{
		return false;
	}
	TestEqual(TEXT("units spent are the sum of the charges"), Outcome->UnitsSpent, Charged);
	TestTrue(TEXT("the core's grade is shown"), A2THas(A2TShownTexts(Panel), A2TWord(TEXT("phase2.a2.common.grade.sound"))));
	if (!TestTrue(TEXT("the close button is pressed"), A2TClick(Panel, A2TWord(TEXT("phase2.a2.common.complete_lesson")))))
	{
		return false;
	}
	TestTrue(TEXT("recorded"), !Sessions.Active.has_value() && Sessions.Periods.back().Completed);
	TestTrue(TEXT("learning once"), A2TCompleted(Panel->GetLastResult().Events));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FA2TExplainCancelTest, "PerfectWorld.Phase2A2.ExplainSlotsAreBoundedAndCancellationIsClean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FA2TExplainCancelTest::RunTest(const FString& Parameters)
{
	const FA2TFixture* F = A2TShared(*this);
	PerfectWorld::SchoolSessionCatalogues Cats;
	if (F == nullptr || !TestTrue(TEXT("a science timetable"), F->Cats(Cats)))
	{
		return false;
	}
	PerfectWorld::SchoolSessionsState Sessions;
	PerfectWorld::DayLoopState Day;
	if (!A2TDayAtBell(*this, Cats, Day))
	{
		return false;
	}
	const TSharedRef<PWClassLesson::FCoreBridge> Bridge =
		MakeShared<PWClassLesson::FCoreBridge>(Sessions, Day, Cats, F->Binding(PerfectWorld::SchoolStructure::Explain, 0, Cats), 100000, 41ULL);
	const TSharedRef<SPWClassExplain> Panel = SNew(SPWClassExplain).Bridge(Bridge).Space(FVector2D(1280.0, 720.0));
	if (!TestTrue(TEXT("opens"), Panel->ExecuteOpen(PerfectWorld::SchoolLessonMode::Foreground).Ok()))
	{
		return false;
	}
	const PerfectWorld::ExplainCaseDef* Def = Cats.Structures.Explain->FindCase(Sessions.Explain.Exposure.CaseId);
	if (Def == nullptr || Def->Claims.empty() || Def->Evidence.size() < 3)
	{
		AddInfo(TEXT("the first case offers fewer than three evidence items; the slot bound is covered by the core's tests"));
		return true;
	}
	// Choosing nothing is refused by the screen without a command; a real claim is committed by the core.
	TestFalse(TEXT("no claim, no command"), Panel->ExecuteChooseClaim().Ok());
	Panel->SelectClaim(FString(UTF8_TO_TCHAR(Def->Claims.front().Id.c_str())));
	PerfectWorld::ExplainAction ClaimAction;
	ClaimAction.Kind = PerfectWorld::ExplainActionKind::ChooseClaim;
	ClaimAction.Claim = Def->Claims.front().Id;
	const PerfectWorld::SchoolLessonQuote ClaimQuote = PerfectWorld::QuoteSchoolLessonAction(Sessions, Cats, Sessions.Active->Session, PerfectWorld::SchoolLessonAction(ClaimAction));
	const PerfectWorld::SchoolSessionResult Chose = Panel->ExecuteChooseClaim();
	if (!TestTrue(TEXT("a claim is committed"), Chose.Ok()))
	{
		return false;
	}
	TestEqual(TEXT("at its quote"), Chose.ChargedUnits, ClaimQuote.Units);
	// Three evidence items: the third does not fit and the screen says so.
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Panel->ToggleEvidence(FString(UTF8_TO_TCHAR(Def->Evidence[static_cast<size_t>(Index)].Id.c_str())));
	}
	TestEqual(TEXT("two slots only"), Panel->GetSelectedEvidence().Num(), static_cast<int32>(PerfectWorld::ExplainEvidenceSlots));
	TestEqual(TEXT("the screen says why"), Panel->GetBanner().ToString(),
		PWClassLesson::Text1(TEXT("phase2.a2.explain.status.slots_full"), PWClassLesson::Number(static_cast<int64>(PerfectWorld::ExplainEvidenceSlots))).ToString());
	// Cancel with the working time spent so far; no learning, the period record says cancelled.
	const PerfectWorld::SchoolSessionResult Cancelled = Panel->ExecuteClose(true);
	TestTrue(TEXT("the core cancels"), Cancelled.Ok());
	TestTrue(TEXT("cancelled, not completed"), !Sessions.Active.has_value() && Sessions.Periods.back().Cancelled && !Sessions.Periods.back().Completed);
	TestFalse(TEXT("no learning"), A2TCompleted(Cancelled.Events));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
