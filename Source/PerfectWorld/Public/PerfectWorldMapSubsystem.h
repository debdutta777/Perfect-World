#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "PerfectWorldMapSubsystem.generated.h"

class FPWMapLive;
class SPWMinimap;
class SPWWorldMap;
class SWeakWidget;

/**
 * The owner of the two maps (order F2). It installs the minimap in the upper-right corner of the viewport as soon as a local player exists
 * (the same way the clock readout is installed), keeps the shared live snapshot, and opens and closes the full-screen map.
 *
 * It decides nothing: the portable lane map (LaneMap.h) says what the world looks like, LaneMapView.h says when the minimap hides and when
 * the full map may open, and this class only follows those rules. The minimap needs no hook; the front end HUD's menu state, the
 * conversation panel and the reading panel are read through the same presenters the interaction prompt uses.
 *
 * Opening the full map pauses exactly as the pause menu does: UGameplayStatics::SetGamePaused(true) with UI-only input and the cursor
 * shown; closing undoes the same three things (unpause, hide the cursor, game-only input, pressed keys flushed). The world clock stands
 * still while paused. Escape, B, the View/Back button, M and the player's own Map binding close it.
 *
 * The Map action of the key binding store (EKbAction::Map) is bound by the player character's input code to ToggleWorldMap; the keys the
 * player has chosen for it are handed to SetMapCloseKeys so the same key also closes the map from inside Slate, where game input is off.
 */
UCLASS()
class PERFECTWORLD_API UPWMapSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    /** The subsystem of the world WorldContext lives in, or null. */
    static UPWMapSubsystem* Get(const UObject* WorldContext);

    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UPWMapSubsystem, STATGROUP_Tickables); }

    bool IsWorldMapOpen() const;
    /** Opens the full map when the rules allow it (a player, no menu, no conversation, no reading, not already open). True when it opened. */
    bool OpenWorldMap();
    void CloseWorldMap();
    /** What the Map action calls: closes the map when open, otherwise opens it. */
    void ToggleWorldMap();

    /** Keys that close the open map from inside Slate besides M, Esc, B, Back and Menu: the player's keys for the Map action. */
    void SetMapCloseKeys(const TArray<FKey>& Keys);

    /** The settings-ready switches (stored in GameUserSettings). */
    bool IsMinimapEnabled() const;
    bool DoesMinimapRotate() const;
    void SetMinimapEnabled(bool bEnabled);
    void SetMinimapRotates(bool bRotates);

    /** The minimap widget, for acceptance scenarios and tests; null before a player exists. */
    TSharedPtr<SPWMinimap> GetMinimap() const { return Minimap; }

private:
    void EnsureMinimap();
    void RemoveMinimap();
    void RestoreGameInput();
    TSharedRef<FPWMapLive> EnsureLive();

    TSharedPtr<FPWMapLive> Live;
    TSharedPtr<SPWMinimap> Minimap;
    TSharedPtr<SWeakWidget> MinimapHolder;
    TSharedPtr<SPWWorldMap> WorldMap;
    TSharedPtr<SWeakWidget> WorldMapHolder;
    TArray<FKey> CloseKeys;
};
