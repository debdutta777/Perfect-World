#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NeighbourhoodTraffic.h"
#include "PerfectWorldTrafficVehicles.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMesh;

/** One kinematic vehicle, including both tram cars, inside the portable body's footprint. */
UCLASS()
class APWTrafficVehicle : public AActor
{
    GENERATED_BODY()
public:
    APWTrafficVehicle();
    void Build(const PerfectWorld::NeighbourhoodVehicle& Vehicle);
    void SetNight(bool bNight);
    FVector CentreAt(const FVector& RoadPosition) const;
    bool CanPlace(const FVector& Centre, const FQuat& Heading) const;
    bool CanTravel(const FVector& Centre) const;
    bool Travel(const FVector& Centre);
    virtual bool CanBeBaseForCharacter(APawn* Pawn) const override { return false; }
    UBoxComponent* GetBody() const { return Body; }
    const std::string& GetTrafficId() const { return TrafficId; }
    int32 GetCarCount() const { return CarCount; }

private:
    void Block(const FVector& Centre, const FVector& Size, const FLinearColor& Colour, bool bLamp = false);
    void Board(const FVector& Centre, double Yaw, const FString& Text);
    UPROPERTY() TObjectPtr<UBoxComponent> Body;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UMaterialInterface> Paint;
    UPROPERTY() TObjectPtr<UMaterialInterface> Glow;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Lamps;
    std::string TrafficId;
    double Height = 160;
    int32 CarCount = 1;
};
