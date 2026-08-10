#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSTeam.h"
#include "RSFireZone.generated.h"

class UPointLightComponent;

// Огонь от молотова: горит несколько секунд и жжёт всех внутри.
UCLASS()
class RAGESTRIKE_API ARSFireZone : public AActor
{
	GENERATED_BODY()

public:
	ARSFireZone();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	TWeakObjectPtr<AActor> Thrower;

	UPROPERTY(Replicated)
	ERSTeam Team = ERSTeam::CT;

	static constexpr float Radius = 300.f;
	static constexpr float Duration = 7.f;

protected:
	virtual void BeginPlay() override;

private:
	void DamageTick();

	UPROPERTY()
	TArray<UStaticMeshComponent*> Flames;

	UPROPERTY(VisibleAnywhere)
	UPointLightComponent* Light;

	UPROPERTY()
	UStaticMesh* SphereMesh = nullptr;

	UPROPERTY()
	UMaterialInterface* BaseMat = nullptr;

	FTimerHandle DamageTimer;
};
