#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "RSMenuCamera.generated.h"

// Камера меню: медленно облетает карту по кругу, как заставка в CS2.
// Живёт только у локального игрока, не реплицируется.
UCLASS()
class RAGESTRIKE_API ARSMenuCamera : public ACameraActor
{
	GENERATED_BODY()

public:
	ARSMenuCamera();

	virtual void Tick(float DeltaTime) override;

private:
	// центр и размер облёта берём у построенной карты
	bool ResolveTarget(FVector& OutCenter, float& OutRadius, float& OutGround) const;

	// стартовое время в реальных секундах: игра в меню стоит на паузе,
	// поэтому угол считаем от системных часов, а не от DeltaTime
	double StartSeconds = 0.0;
};
