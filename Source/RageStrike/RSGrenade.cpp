#include "RSGrenade.h"
#include "RSFireZone.h"
#include "RSCharacter.h"
#include "RSBot.h"
#include "RSAudio.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

ARSGrenade::ARSGrenade()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	Sphere->InitSphereRadius(8.f);
	// отскакивает от карты, но не толкает игроков
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Sphere->SetCollisionObjectType(ECC_WorldDynamic);
	Sphere->SetCollisionResponseToAllChannels(ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SetRootComponent(Sphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Sphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.16f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Ball.Succeeded())
	{
		SphereMesh = Ball.Object;
		Mesh->SetStaticMesh(SphereMesh);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Mat.Succeeded())
	{
		BaseMat = Mat.Object;
	}
	static ConstructorHelpers::FObjectFinder<USoundBase> Snd(
		TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"));
	if (Snd.Succeeded())
	{
		BlastSound = Snd.Object;
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->bShouldBounce = true;
	Movement->Bounciness = 0.45f;
	Movement->Friction = 0.4f;
	Movement->ProjectileGravityScale = 1.f;
	Movement->bRotationFollowsVelocity = false;
}

void ARSGrenade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARSGrenade, Type);
	DOREPLIFETIME(ARSGrenade, Team);
}

void ARSGrenade::InitThrow(ERSWeapon InType, AActor* InThrower, ERSTeam InTeam, const FVector& Velocity)
{
	Type = InType;
	Thrower = InThrower;
	Team = InTeam;
	Movement->Velocity = Velocity;
}

void ARSGrenade::BeginPlay()
{
	Super::BeginPlay();

	// красим корпус: дым белёсый, огонь рыжий, остальные тёмно-зелёные
	if (BaseMat)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		FLinearColor C(0.08f, 0.12f, 0.08f);
		if (Type == ERSWeapon::SmokeGrenade) { C = FLinearColor(0.5f, 0.5f, 0.5f); }
		if (Type == ERSWeapon::Molotov || Type == ERSWeapon::Incendiary) { C = FLinearColor(0.7f, 0.3f, 0.05f); }
		if (Type == ERSWeapon::Flashbang) { C = FLinearColor(0.35f, 0.35f, 0.4f); }
		MID->SetVectorParameterValue(TEXT("Color"), C);
		Mesh->SetMaterial(0, MID);
	}

	ThrownAt = GetWorld()->GetTimeSeconds();

	if (!HasAuthority())
	{
		return;
	}

	Movement->OnProjectileBounce.AddDynamic(this, &ARSGrenade::OnBounce);

	// таймер подрыва: HE и флешка рвутся в полёте, молотов — по удару
	// (с запасным таймером), дым решает сам в Tick
	float Fuse = 0.f;
	switch (Type)
	{
	case ERSWeapon::HEGrenade:
	case ERSWeapon::Flashbang:  Fuse = 1.6f; break;
	case ERSWeapon::Molotov:
	case ERSWeapon::Incendiary: Fuse = 2.5f; break;
	case ERSWeapon::SmokeGrenade: Fuse = 4.f; break;
	default: break;
	}
	GetWorldTimerManager().SetTimer(FuseTimer, this, &ARSGrenade::Detonate, Fuse, false);
}

void ARSGrenade::OnBounce(const FHitResult& Hit, const FVector& ImpactVelocity)
{
	// стук о стену слышно всем: по нему в CS понимают, что летит граната
	if (USoundBase* Snd = RSAudio::Get(RSAudio::ESound::NadeBounce))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Snd, Hit.ImpactPoint,
			FMath::Clamp(ImpactVelocity.Size() / 900.f, 0.2f, 1.f));
	}

	// молотов вспыхивает от первого касания пола (не стены)
	if ((Type == ERSWeapon::Molotov || Type == ERSWeapon::Incendiary) && Hit.ImpactNormal.Z > 0.5f)
	{
		Detonate();
	}
}

void ARSGrenade::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HasAuthority() || bDetonated)
	{
		return;
	}

	// дым распускается, когда граната улеглась
	if (Type == ERSWeapon::SmokeGrenade
		&& GetWorld()->GetTimeSeconds() - ThrownAt > 0.8f
		&& Movement->Velocity.SizeSquared() < 60.f * 60.f)
	{
		Detonate();
	}
}

void ARSGrenade::Detonate()
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;
	GetWorldTimerManager().ClearTimer(FuseTimer);

	const FVector Where = GetActorLocation();
	UWorld* World = GetWorld();

	if (Type == ERSWeapon::HEGrenade)
	{
		// урон по прямой видимости, спадает с расстоянием
		const float MaxDamage = RSWeapons::Get(Type).BodyDamage;
		const float R = 700.f;

		auto Hurt = [&](AActor* Victim)
		{
			const float Dist = FVector::Dist(Where, Victim->GetActorLocation());
			if (Dist > R)
			{
				return;
			}
			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(this);
			Params.bTraceComplex = true;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, Where,
				Victim->GetActorLocation(), ECC_Visibility, Params)
				&& Hit.GetActor() != Victim;
			const float Damage = MaxDamage * (1.f - Dist / R) * (bBlocked ? 0.25f : 1.f);
			if (Damage > 1.f)
			{
				UGameplayStatics::ApplyDamage(Victim, Damage, nullptr, this, nullptr);
			}
		};
		for (TActorIterator<ARSCharacter> It(World); It; ++It)
		{
			if (IsValid(*It) && It->bAlive) { Hurt(*It); }
		}
		for (TActorIterator<ARSBot> It(World); It; ++It)
		{
			if (IsValid(*It) && It->Health > 0.f) { Hurt(*It); }
		}
		MulticastDetonate(Where);
		SetActorHiddenInGame(true);
		SetLifeSpan(0.3f);
	}
	else if (Type == ERSWeapon::Flashbang)
	{
		// ослепляем ботов на сервере, игроков — мультикастом на клиентах
		for (TActorIterator<ARSBot> It(World); It; ++It)
		{
			if (!IsValid(*It) || It->Health <= 0.f)
			{
				continue;
			}
			const float Dist = FVector::Dist(Where, It->GetActorLocation());
			if (Dist > 2500.f)
			{
				continue;
			}
			FHitResult Hit;
			FCollisionQueryParams Params;
			Params.AddIgnoredActor(this);
			Params.bTraceComplex = true;
			const bool bBlocked = World->LineTraceSingleByChannel(Hit, Where,
				It->GetActorLocation() + FVector(0, 0, 50.f), ECC_Visibility, Params)
				&& Hit.GetActor() != *It;
			if (!bBlocked)
			{
				It->BlindUntil = World->GetTimeSeconds() + 3.f * (1.f - Dist / 2500.f);
			}
		}
		MulticastDetonate(Where);
		SetActorHiddenInGame(true);
		SetLifeSpan(0.3f);
	}
	else if (Type == ERSWeapon::SmokeGrenade)
	{
		Movement->StopMovementImmediately();
		Movement->ProjectileGravityScale = 0.f;
		MulticastDetonate(Where);
		SetLifeSpan(16.f); // дым висит, потом актор исчезает вместе с ним
	}
	else // молотов и зажигалка
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ARSFireZone* Zone = World->SpawnActor<ARSFireZone>(
			ARSFireZone::StaticClass(), Where, FRotator::ZeroRotator, SP))
		{
			Zone->Thrower = Thrower;
			Zone->Team = Team;
		}
		MulticastDetonate(Where);
		SetActorHiddenInGame(true);
		SetLifeSpan(0.3f);
	}
}

void ARSGrenade::MulticastDetonate_Implementation(FVector Where)
{
	if (Type == ERSWeapon::SmokeGrenade)
	{
		if (USoundBase* Snd = RSAudio::Get(RSAudio::ESound::Smoke))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Snd, Where, 1.f);
		}
		SpawnSmokeCloud(Where);
		return;
	}

	if (Type == ERSWeapon::Flashbang)
	{
		if (USoundBase* Snd = RSAudio::Get(RSAudio::ESound::Flash))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Snd, Where, 1.f);
		}
		ApplyLocalFlash(Where);
		return;
	}

	// HE, молотов и зажигалка — взрыв
	if (USoundBase* Snd = RSAudio::Get(RSAudio::ESound::Explode))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Snd, Where, 1.f);
	}
	if (Type == ERSWeapon::HEGrenade)
	{
		DrawDebugSphere(GetWorld(), Where, 220.f, 16, FColor::Orange, false, 0.25f, 0, 3.f);
	}
}

void ARSGrenade::ApplyLocalFlash(const FVector& Where)
{
	// белая пелена у локального игрока: зависит от дистанции, LOS и угла взгляда
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	ARSCharacter* Me = PC ? Cast<ARSCharacter>(PC->GetPawn()) : nullptr;
	if (!Me || !Me->bAlive)
	{
		return;
	}

	const FVector Eye = Me->GetActorLocation() + FVector(0, 0, 60.f);
	const float Dist = FVector::Dist(Where, Eye);
	if (Dist > 3000.f)
	{
		return;
	}

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Me);
	Params.bTraceComplex = true;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Where, ECC_Visibility, Params))
	{
		return; // за стеной не слепит
	}

	const FVector ToNade = (Where - Eye).GetSafeNormal();
	const float Facing = FVector::DotProduct(Me->GetControlRotation().Vector(), ToNade);
	// смотришь в сторону вспышки — слепнешь надолго, отвернулся — вскользь
	const float AngleFactor = (Facing > 0.2f) ? 1.f : 0.35f;
	const float Strength = (1.f - Dist / 3000.f) * AngleFactor;

	Me->FlashDuration = FMath::Max(0.5f, 4.5f * Strength);
	Me->FlashEndTime = GetWorld()->GetTimeSeconds() + Me->FlashDuration;
}

void ARSGrenade::SpawnSmokeCloud(const FVector& Where)
{
	if (bSmokeDeployed || !SphereMesh)
	{
		return;
	}
	bSmokeDeployed = true;

	Mesh->SetVisibility(false);

	// облако из серых сфер; блокирует канал камеры — боты сквозь дым не видят
	FRandomStream Rand(GetTypeHash(Where));
	for (int32 i = 0; i < 12; i++)
	{
		UStaticMeshComponent* Puff = NewObject<UStaticMeshComponent>(this);
		Puff->SetStaticMesh(SphereMesh);
		Puff->SetupAttachment(GetRootComponent());
		if (BaseMat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			const float Shade = Rand.FRandRange(0.55f, 0.75f);
			MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(Shade, Shade, Shade));
			Puff->SetMaterial(0, MID);
		}
		Puff->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Puff->SetCollisionObjectType(ECC_WorldDynamic);
		Puff->SetCollisionResponseToAllChannels(ECR_Ignore);
		Puff->SetCollisionResponseToChannel(ECC_Camera, ECR_Block);
		Puff->SetCastShadow(false);
		const FVector Offset(Rand.FRandRange(-140.f, 140.f), Rand.FRandRange(-140.f, 140.f),
			Rand.FRandRange(20.f, 180.f));
		Puff->SetRelativeLocation(Offset);
		Puff->SetRelativeScale3D(FVector(Rand.FRandRange(2.6f, 3.6f)));
		Puff->RegisterComponent();
	}
}
