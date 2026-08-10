#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RSCharacter.h"
#include "RSWeaponPickup.generated.h"

class UStaticMeshComponent;
class USphereComponent;

// Оружие, лежащее на земле: выпадает при смерти или по клавише сброса.
UCLASS()
class RAGESTRIKE_API ARSWeaponPickup : public AActor
{
	GENERATED_BODY()

public:
	ARSWeaponPickup();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// сервер: положить оружие на землю под указанной точкой
	void DropAt(ERSWeapon Type, const FVector& From);

	UPROPERTY(ReplicatedUsing = OnRep_Weapon)
	ERSWeapon WeaponType = ERSWeapon::AK47;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Weapon();

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp,
		int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep);

	void ApplyMesh();

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* Mesh;

	UPROPERTY(VisibleAnywhere)
	USphereComponent* PickupSphere;

	UPROPERTY()
	UStaticMesh* RifleAsset = nullptr;

	UPROPERTY()
	UStaticMesh* SniperAsset = nullptr;

	UPROPERTY()
	UStaticMesh* PistolAsset = nullptr;

	float ArmedAt = 0.f; // не подбираем то, что сами только что бросили

	// центр сферы подбора стоит выше пола на эту величину: меш опускаем
	// на столько же, чтобы оружие лежало на поверхности
	static constexpr float GroundOffset = 45.f;
};
