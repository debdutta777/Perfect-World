#include "PerfectWorldReadingPresenter.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "PerfectWorldConversationPresenter.h"
#include "PerfectWorldFrontEndHUD.h"
#include "UI/SPWReadingPanel.h"
#include "Widgets/SWeakWidget.h"

#define LOCTEXT_NAMESPACE "PerfectWorldReadingPresenter"

namespace
{
/** Above the conversation panel (110) and the front end (100), so the reading view takes the clicks and the keyboard. */
constexpr int32 ReadingZOrder = 112;
} // namespace

UPWReadingPresenter* UPWReadingPresenter::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    return World != nullptr ? World->GetSubsystem<UPWReadingPresenter>() : nullptr;
}

bool UPWReadingPresenter::FindBuiltInReading(const FName ContentId, FText& OutTitle, TArray<FText>& OutPages)
{
    // The children's home's own readables (Nabapara Shishu Griha): the entry noticeboard and the resident's exercise book.
    // Plain, practical and dignified: no real people, no religion, no pity; residents are capable young people.
    if (ContentId == FName(TEXT("text.nabapara.care_noticeboard")))
    {
        OutTitle = LOCTEXT("CareNoticeboardTitle", "Noticeboard - Nabapara Shishu Griha");
        OutPages.Reset();
        OutPages.Add(LOCTEXT("CareNoticeboardPage1",
            "WELCOME TO THE NEW TERM\n"
            "Week beginning Monday, 2 June 2042\n\n"
            "Welcome back, everyone. A new term starts today, and a few of us are starting in a new class. Please put your "
            "timetable and your identity card in your school bag tonight, and tell the staff on duty if anything is missing.\n\n"
            "STUDY ROOM HOURS\n"
            "Monday to Friday, 4.30 pm to 6.00 pm: homework hour, in the common room. Staff are there to help with any "
            "subject. Quiet reading or revision is also fine at the front tables.\n"
            "Saturday, 10.00 am to 12.00 noon: open study, for anyone who wants it. Sunday is free."));
        OutPages.Add(LOCTEXT("CareNoticeboardPage2",
            "SHARED JOBS ROTA\n"
            "The rota for breakfast set-out, tidying the study room after homework hour and the water stand is on the "
            "bookshelf side of this board. If you would like to swap a day, or take on a job for good, talk to the "
            "day-shift care worker and agree it together. Please write any change on the rota yourself, so everyone can see "
            "who has what.\n\n"
            "WEEKEND OUTING\n"
            "On Saturday we are walking to the riverside park, leaving at 2.00 pm and back by 5.30 pm. Bring water and a "
            "hat. Tell the staff on duty by Friday evening if you would like to come, and if you have plans of your own "
            "that day, let them know where you will be and when you will be back."));
        OutPages.Add(LOCTEXT("CareNoticeboardPage3",
            "STAFF ON DUTY\n"
            "Day shift: 7.00 am to 3.00 pm.\n"
            "Handover at the staff desk: 3.00 pm. The day shift hands the register and the notes to the evening staff.\n"
            "Evening shift: 3.00 pm to 10.00 pm.\n"
            "Handover at the staff desk: 10.00 pm, to the night staff, who are on duty until 7.00 am.\n\n"
            "If you will be back late, or your plans change, tell the staff before the handover so the note is right. "
            "Someone is always on duty, and you can always ask for help.\n\n"
            "Doors are locked at night. Front door: please close it gently behind you."));
        return true;
    }
    if (ContentId == FName(TEXT("text.nabapara.care_exercise_book")))
    {
        OutTitle = LOCTEXT("CareExerciseBookTitle", "Exercise book - Class XI");
        OutPages.Reset();
        OutPages.Add(LOCTEXT("CareExerciseBookPage1",
            "MATHS - Quadratic equations\n"
            "Mon 2 June\n\n"
            "Standard form: a x^2 + b x + c = 0, with a not equal to 0.\n\n"
            "Q1. x^2 - 5x + 6 = 0\n"
            "Two numbers that multiply to 6 and add to -5: -2 and -3.\n"
            "(x - 2)(x - 3) = 0, so x = 2 or x = 3.\n"
            "Check: 4 - 10 + 6 = 0, and 9 - 15 + 6 = 0. Good.\n\n"
            "Q2. 2x^2 + 3x - 2 = 0\n"
            "Discriminant: b^2 - 4ac = 9 + 16 = 25, which is a perfect square, so the roots are rational.\n"
            "x = (-3 + 5) / 4 = 1/2, or x = (-3 - 5) / 4 = -2.\n"
            "Check x = -2: 8 - 6 - 2 = 0. Good.\n\n"
            "To remember: if the discriminant is positive, two real roots; zero, one repeated root; negative, no real roots."));
        OutPages.Add(LOCTEXT("CareExerciseBookPage2",
            "ENGLISH - Reflection\n"
            "Tue 3 June\n\n"
            "Our teacher asked us to write one paragraph on the poem we began this week, about a small town waking before "
            "sunrise. What I notice first is how little happens: a shutter goes up, a kettle is filled, someone sweeps a "
            "step. But the poet gives each small act the same weight, and by the end I felt that the town was held together "
            "by people doing ordinary jobs well, day after day, without anyone watching.\n\n"
            "I think the poem is about care more than about morning. I want to read it again slowly, and to ask whether "
            "the quiet in it is peace or only tiredness.\n\n"
            "To do: finish the maths exercise 4.2 by Friday. Ask for the library card form at school."));
        return true;
    }
    // A fictional, neutral local paper of the alternate world, Monday 2 June 2042. No real people, no politics, no religion.
    if (ContentId != FName(TEXT("text.nabapara.home_newspaper")))
    {
        return false;
    }
    OutTitle = LOCTEXT("NewspaperTitle", "The Kolkata Morning Courier");
    OutPages.Reset();
    OutPages.Add(LOCTEXT("NewspaperPage1",
        "Monday, 2 June 2042\n\n"
        "WEATHER\n"
        "A warm, humid morning across the city. Expect a high of 36 degrees this afternoon, with a fair chance of a short "
        "evening shower in the south and east. Winds will be light from the south.\n\n"
        "The weather office says the pre-monsoon showers should become more frequent over the coming week. Residents are "
        "reminded to keep drains in front of their homes clear of leaves and litter, and to carry an umbrella in the "
        "evening."));
    OutPages.Add(LOCTEXT("NewspaperPage2",
        "SCHOOL NOTICE\n"
        "The new term begins today. Classes for all senior secondary students start at the usual morning time, and "
        "students are asked to bring their timetable and their identity card.\n\n"
        "Parents and guardians are invited to collect the term calendar from the school office during office hours. The "
        "calendar lists the half-term break, the dates of the internal tests and the holidays. Students in Class XI "
        "should note that the first unit tests are set for the third week of term."));
    OutPages.Add(LOCTEXT("NewspaperPage3",
        "AROUND THE CITY\n"
        "The municipal corporation has announced that the evening tram service on the central route will run every ten "
        "minutes from this week, instead of every fifteen. Timetables are posted at all stops.\n\n"
        "The corporation also reminds residents that household waste is collected before nine each morning. Please keep "
        "bins on the roadside only until collection, and keep wet and dry waste in separate bags.\n\n"
        "LOCAL MARKETS\n"
        "Fresh fish and seasonal fruit are in good supply this week. Prices at the neighbourhood markets are steady."));
    return true;
}

void UPWReadingPresenter::Deinitialize()
{
    // The world is going away: remove the panel, but do not touch input modes of a controller that is being destroyed.
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Panel.Reset();
    Controller.Reset();
    OpenContentId = NAME_None;
    Super::Deinitialize();
}

bool UPWReadingPresenter::OpenBuiltIn(const FName ContentId)
{
    FText Title;
    TArray<FText> Pages;
    return FindBuiltInReading(ContentId, Title, Pages) && Open(ContentId, Title, MoveTemp(Pages));
}

bool UPWReadingPresenter::Open(const FName ContentId, FText Title, TArray<FText> Pages)
{
    if (IsOpen() || Pages.IsEmpty() || GEngine == nullptr || GEngine->GameViewport == nullptr)
    {
        return false;
    }
    const UPWConversationPresenter* Conversation = UPWConversationPresenter::Get(this);
    if (Conversation != nullptr && Conversation->IsOpen())
    {
        return false; // a conversation already owns the screen and the input
    }
    APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    if (PlayerController == nullptr)
    {
        return false;
    }
    const APerfectWorldFrontEndHUD* Hud = Cast<APerfectWorldFrontEndHUD>(PlayerController->GetHUD());
    if (Hud != nullptr && Hud->IsMenuOpen())
    {
        return false; // a menu already owns the screen and the input
    }

    OpenContentId = ContentId;
    Controller = PlayerController;
    Panel = SNew(SPWReadingPanel)
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWReadingPresenter::Close));
    Holder = SNew(SWeakWidget).PossiblyNullContent(Panel);
    GEngine->GameViewport->AddViewportWidgetContent(Holder.ToSharedRef(), ReadingZOrder);
    Panel->SetContent(Title, Pages);
    ApplyReadingInput(*PlayerController);
    OnOpened.Broadcast(ContentId);
    return true;
}

void UPWReadingPresenter::Close()
{
    if (!IsOpen())
    {
        return;
    }
    // The key handler or button that called this is still on the Slate stack; it holds its own reference to the panel.
    const FName Closed = OpenContentId;
    if (Holder.IsValid() && GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(Holder.ToSharedRef());
    }
    Holder.Reset();
    Panel.Reset();
    OpenContentId = NAME_None;
    RestoreGameInput();
    Controller.Reset();
    // Last, so listeners (the clock) see a world that is already playable again.
    OnClosed.Broadcast(Closed);
}

void UPWReadingPresenter::ApplyReadingInput(APlayerController& PlayerController) const
{
    // The same switch the conversation panel and the front end make: Slate gets the keyboard and controller, the walking
    // player stops, and the cursor is shown. The view target and control rotation are not touched.
    PlayerController.SetShowMouseCursor(true);
    FInputModeUIOnly InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PlayerController.SetInputMode(InputMode);
    PlayerController.FlushPressedKeys();
}

void UPWReadingPresenter::RestoreGameInput() const
{
    APlayerController* PlayerController = Controller.Get();
    if (PlayerController == nullptr)
    {
        return;
    }
    const APerfectWorldFrontEndHUD* Hud = Cast<APerfectWorldFrontEndHUD>(PlayerController->GetHUD());
    if (Hud != nullptr && Hud->IsMenuOpen())
    {
        return; // a menu opened meanwhile and owns the input mode
    }
    PlayerController->SetShowMouseCursor(false);
    PlayerController->SetInputMode(FInputModeGameOnly());
    PlayerController->FlushPressedKeys();
}

#undef LOCTEXT_NAMESPACE
