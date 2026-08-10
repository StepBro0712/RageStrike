#include "RSMenuCamera.h"
#include "RSArena.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

ARSMenuCamera::ARSMenuCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	// в одиночной игре меню ставит паузу — без этого камера замирает
	PrimaryActorTick.bTickEvenWhenPaused = true;
	SetActorTickEnabled(true);

	bReplicates = false;
	SetActorEnableCollision(false);

	if (UCameraComponent* Cam = GetCameraComponent())
	{
		Cam->SetFieldOfView(70.f);
	}

	StartSeconds = FPlatformTime::Seconds();
}

bool ARSMenuCamera::ResolveTarget(FVector& OutCenter, float& OutRadius, float& OutGround) const
{
	for (TActorIterator<ARSArena> It(GetWorld()); It; ++It)
	{
		OutCenter = It->MapCenter;
		OutRadius = FMath::Max(1200.f, It->PlayRadius);
		OutGround = It->GroundLevel;
		return true;
	}
	return false;
}

void ARSMenuCamera::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Center;
	float Radius = 3000.f;
	float Ground = 0.f;
	if (!ResolveTarget(Center, Radius, Ground))
	{
		return; // карта ещё строится
	}

	// один оборот примерно за две минуты
	const double Elapsed = FPlatformTime::Seconds() - StartSeconds;
	const float Angle = FMath::Fmod((float)Elapsed * 3.f, 360.f);

	// смотрим в центр карты у самого пола
	const FVector LookAt(Center.X, Center.Y, Ground + 100.f);

	// Камера идёт по кругу высоко над картой и смотрит вниз примерно под 35°:
	// низкий облёт упирался в стены домов и половину кадра занимала пустота.
	const float Dist = Radius * 0.45f;
	const float Height = Ground + 1900.f + FMath::Sin((float)Elapsed * 0.3f) * 150.f;
	FVector Desired(
		Center.X + FMath::Cos(FMath::DegreesToRadians(Angle)) * Dist,
		Center.Y + FMath::Sin(FMath::DegreesToRadians(Angle)) * Dist,
		Height);

	// если под камерой оказалась крыша, приподнимаемся над ней
	FHitResult Floor;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	Params.AddIgnoredActor(this);
	const FVector Probe(Desired.X, Desired.Y, Desired.Z + 3000.f);
	if (GetWorld()->LineTraceSingleByChannel(Floor, Probe, Probe - FVector(0.f, 0.f, 9000.f),
		ECC_Visibility, Params))
	{
		Desired.Z = FMath::Max(Desired.Z, Floor.ImpactPoint.Z + 500.f);
	}

	SetActorLocation(Desired);
	SetActorRotation((LookAt - Desired).Rotation());
}
