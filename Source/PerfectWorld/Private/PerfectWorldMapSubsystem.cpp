#include "PerfectWorldMapSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/PWMapData.h"
#include "UI/SPWMinimap.h"
#include "UI/SPWWorldMap.h"
#include "Widgets/SWeakWidget.h"

namespace
{
/** Slate draws viewport content in Z order: above the clock readout (35) and the prompt (40), below the front end (100) and the conversation (110). */
constexpr int32 MpMinimapZOrder = 36;
constexpr int32 MpWorldMapZOrder = 105;
} // namespace

UPWMapSubsystem* UPWMapSubsystem::Get(const UObject* WorldContext)
{
    const UWorld* World = WorldContext != nullptr ? WorldContext->GetWorld() : nullptr;
    return World != nullptr ? World->GetSubsystem<UPWMapSubsystem>() : nullptr;
}

bool UPWMapSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    // Only worlds being played; the editor's own worlds have no lane to map.
    const UWorld* World = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && World != nullptr && World->IsGameWorld();
}

void UPWMapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    EnsureLive();
}

void UPWMapSubsystem::Deinitialize()
{
    CloseWorldMap();
    RemoveMinimap();
    Live.Reset();
    Super::Deinitialize();
}

TSharedRef<FPWMapLive> UPWMapSubsystem::EnsureLive()
{
    if (!Live.IsValid())
    {
        Live = MakeShared<FPWMapLive>(TWeakObjectPtr<UWorld>(GetWorld()));
    }
    return Live.ToSharedRef();
}

void UPWMapSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    EnsureMinimap();
}

void UPWMapSubsystem::EnsureMinimap()
{
    if (MinimapHolder.IsValid() || GEngine == nullptr || GEngine->GameViewport == nullptr || IsRunningDedicatedServer())
    {
        return;
    }
    const UWorld* World = GetWorld();
    if (World == nullptr || World->GetFirstPlayerController() == nullptr)
    {
        return;
    }
    // The widgets read this subsystem through a weak pointer only; they never keep it (or each other) alive.
    const TWeakObjectPtr<UPWMapSubsystem> Weak(this);
    Minimap = SNew(SPWMinimap)
        .Live(EnsureLive())
        .WorldMapOpen(TAttribute<bool>::CreateLambda([Weak]() { return Weak.IsValid() && Weak->IsWorldMapOpen(); }));
    MinimapHolder = SNew(SWeakWidget).PossiblyNullContent(Minimap);
    MinimapHolder->SetVisibility(EVisibility::HitTestInvisible);
    GEngine->GameViewport->AddViewportWidgetContent(MinimapHolder.ToSharedRef(), MpMinimapZOrder);
}

void UPWMapSubsystem::RemoveMinimap()
{
    if (GEngine != nullptr && GEngine->GameViewport != nullptr && MinimapHolder.IsValid())
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(MinimapHolder.ToSharedRef());
    }
    MinimapHolder.Reset();
    Minimap.Reset();
}

bool UPWMapSubsystem::IsWorldMapOpen() const
{
    return WorldMapHolder.IsValid();
}

bool UPWMapSubsystem::OpenWorldMap()
{
    UWorld* World = GetWorld();
    if (IsWorldMapOpen() || World == nullptr || GEngine == nullptr || GEngine->GameViewport == nullptr || IsRunningDedicatedServer())
    {
        return false;
    }
    APlayerController* Controller = World->GetFirstPlayerController();
    const TSharedRef<FPWMapLive> Source = EnsureLive();
    Source->Refresh();
    if (Controller == nullptr || !PerfectWorld::LaneMap::CanOpenWorldMap(Source->ReadHudState(false)))
    {
        return false;
    }

    WorldMap = SNew(SPWWorldMap)
        .Live(Source)
        .ExtraCloseKeys(CloseKeys)
        .OnClose(FSimpleDelegate::CreateUObject(this, &UPWMapSubsystem::CloseWorldMap));
    WorldMapHolder = SNew(SWeakWidget).PossiblyNullContent(WorldMap);
    GEngine->GameViewport->AddViewportWidgetContent(WorldMapHolder.ToSharedRef(), MpWorldMapZOrder);

    // The same three things the pause menu does when it opens: pause the game, show the cursor, give input to Slate.
    UGameplayStatics::SetGamePaused(World, true);
    Controller->SetShowMouseCursor(true);
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(WorldMap);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Controller->SetInputMode(InputMode);
    Controller->FlushPressedKeys();
    return true;
}

void UPWMapSubsystem::CloseWorldMap()
{
    if (!WorldMapHolder.IsValid())
    {
        return;
    }
    if (GEngine != nullptr && GEngine->GameViewport != nullptr)
    {
        GEngine->GameViewport->RemoveViewportWidgetContent(WorldMapHolder.ToSharedRef());
    }
    WorldMapHolder.Reset();
    WorldMap.Reset();
    RestoreGameInput();
}

void UPWMapSubsystem::RestoreGameInput()
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }
    UGameplayStatics::SetGamePaused(World, false);
    if (APlayerController* Controller = World->GetFirstPlayerController())
    {
        // The same as the front end HUD's ApplyGameInput.
        Controller->SetShowMouseCursor(false);
        Controller->SetInputMode(FInputModeGameOnly());
        Controller->FlushPressedKeys();
    }
}

void UPWMapSubsystem::ToggleWorldMap()
{
    if (IsWorldMapOpen())
    {
        CloseWorldMap();
    }
    else
    {
        OpenWorldMap();
    }
}

void UPWMapSubsystem::SetMapCloseKeys(const TArray<FKey>& Keys)
{
    CloseKeys = Keys;
}

bool UPWMapSubsystem::IsMinimapEnabled() const
{
    return Live.IsValid() ? Live->GetPreferences().bMinimapEnabled : FPWMapPreferences::Load().bMinimapEnabled;
}

bool UPWMapSubsystem::DoesMinimapRotate() const
{
    return Live.IsValid() ? Live->GetPreferences().bMinimapRotates : FPWMapPreferences::Load().bMinimapRotates;
}

void UPWMapSubsystem::SetMinimapEnabled(bool bEnabled)
{
    FPWMapPreferences Preferences = EnsureLive()->GetPreferences();
    Preferences.bMinimapEnabled = bEnabled;
    EnsureLive()->SetPreferences(Preferences);
}

void UPWMapSubsystem::SetMinimapRotates(bool bRotates)
{
    FPWMapPreferences Preferences = EnsureLive()->GetPreferences();
    Preferences.bMinimapRotates = bRotates;
    EnsureLive()->SetPreferences(Preferences);
}
