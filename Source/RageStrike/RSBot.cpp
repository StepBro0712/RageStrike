#include "RSBot.h"
#include "RSCharacter.h"
#include "RSGameMode.h"
#include "RSGameState.h"
#include "RSArena.h"
#include "RSGrenade.h"
#include "RSFireZone.h"
#include "RSAudio.h"
#include "RSTracer.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"

ARSBot::ARSBot()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bReplicates = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	// пресет Pawn игнорирует канал Visibility — без этого выстрелы пролетают сквозь ботов
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// те же модели, что у игрока: CT — военный, T — боевик
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CTMesh(
		TEXT("/Game/QuantumCharacter/Mesh/SKM_QuantumCharacter.SKM_QuantumCharacter"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TMesh(
		TEXT("/Game/Insurgent_2/Mesh/SK_Preset3.SK_Preset3"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> UnarmedAnim(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));

	if (CTMesh.Succeeded())
	{
		CTBodyMesh = CTMesh.Object;
	}
	if (TMesh.Succeeded())
	{
		TBodyMesh = TMesh.Object;
		GetMesh()->SetSkeletalMesh(TBodyMesh);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
	if (UnarmedAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(UnarmedAnim.Class);
	}

	static ConstructorHelpers::FObjectFinder<USoundBase> Fire(
		TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"));
	if (Fire.Succeeded())
	{
		FireSound = Fire.Object;
	}

	SpectateArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpectateArm"));
	SpectateArm->SetupAttachment(RootComponent);
	SpectateArm->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	SpectateArm->TargetArmLength = 280.f;
	SpectateArm->SocketOffset = FVector(0.f, 55.f, 25.f);
	SpectateArm->bUsePawnControlRotation = false;
	SpectateArm->bInheritPitch = false;
	SpectateArm->bInheritRoll = false;
	SpectateArm->bDoCollisionTest = true;

	SpectateCam = CreateDefaultSubobject<UCameraComponent>(TEXT("SpectateCam"));
	SpectateCam->SetupAttachment(SpectateArm);

	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GunMesh->SetCastShadow(true);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = 420.f;
	bUseControllerRotationYaw = false;

	// боты тоже реплицируются клиентам — сглаживаем их движение
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(30.f);
	SetReplicateMovement(true);
	Move->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
}

void ARSBot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARSBot, Health);
	DOREPLIFETIME(ARSBot, Team);
	DOREPLIFETIME(ARSBot, Kills);
	DOREPLIFETIME(ARSBot, Deaths);
	DOREPLIFETIME(ARSBot, BotNumber);
	DOREPLIFETIME(ARSBot, Weapon);
}

void ARSBot::OnRep_Weapon()
{
	ApplyWeaponVisuals();
}

void ARSBot::ApplyWeaponVisuals()
{
	if (!GunMesh)
	{
		return;
	}

	UStaticMesh* Gun = RSWeapons::LoadWeaponMesh(Weapon);
	GunMesh->SetStaticMesh(Gun);
	GunMesh->SetVisibility(Gun != nullptr);
	if (!Gun)
	{
		return;
	}

	// та же подгонка, что у игрока: модели скачаны у разных авторов, поэтому
	// размер и разворот считаем по габаритам меша
	const FVector Extent = Gun->GetBounds().BoxExtent;
	const float Longest = FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 2.f;
	const float Fit = (Longest > 1.f) ? RSWeapons::RealLength(Weapon) / Longest : 1.f;
	GunMesh->SetRelativeScale3D(FVector(Fit));

	GunPivot = Gun->GetBounds().Origin * Fit;
	GunHandLoc = FVector(-8.f, 3.f, -2.f);
}

void ARSBot::RespawnForRound(const FVector& Location)
{
	Health = 100.f;
	bRagdolled = false;
	bHasTarget = false;
	StuckTime = 0.f;
	FireCooldown = FMath::FRandRange(0.3f, 1.f);
	BlindUntil = -10.f;

	// оружие раунда: в первом раунде половины — пистолеты, дальше случайный
	// арсенал своей стороны, изредка AWP
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	const bool bPistolRound = State && (State->RoundNumber <= 1 || State->RoundNumber == 13);
	if (bPistolRound)
	{
		Weapon = (Team == ERSTeam::CT) ? ERSWeapon::USP : ERSWeapon::Glock;
	}
	else if (FMath::FRand() < 0.1f)
	{
		Weapon = ERSWeapon::AWP;
	}
	else if (Team == ERSTeam::CT)
	{
		static const ERSWeapon CTGuns[] = { ERSWeapon::M4A4, ERSWeapon::FAMAS, ERSWeapon::MP9, ERSWeapon::AUG };
		Weapon = CTGuns[FMath::RandRange(0, 3)];
	}
	else
	{
		static const ERSWeapon TGuns[] = { ERSWeapon::AK47, ERSWeapon::GalilAR, ERSWeapon::MAC10, ERSWeapon::SG553 };
		Weapon = TGuns[FMath::RandRange(0, 3)];
	}
	ApplyWeaponVisuals();

	// поднимаем из рэгдолла: возвращаем меш на капсулу и включаем движение
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->AttachToComponent(GetCapsuleComponent(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->StopMovementImmediately();

	SetActorHiddenInGame(false);
	SetActorLocation(Location);
	LastLocation = Location;
}

void ARSBot::FellOutOfWorld(const UDamageType& DmgType)
{
	// движок по умолчанию уничтожает упавшего, из-за чего команда
	// «заканчивалась» сама и раунд завершался без единого выстрела
	if (Health > 0.f)
	{
		UE_LOG(LogTemp, Log, TEXT("RageStrike: bot fell out of world, returning to spawn"));
		SetActorLocation(ARSArena::FindSpawnPoint(GetWorld(), Team));
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	Super::FellOutOfWorld(DmgType);
}

void ARSBot::OutsideWorldBounds()
{
	if (Health > 0.f)
	{
		SetActorLocation(ARSArena::FindSpawnPoint(GetWorld(), Team));
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	Super::OutsideWorldBounds();
}

void ARSBot::SetTeam(ERSTeam NewTeam)
{
	Team = NewTeam;
	ApplyTeamVisuals();
}

void ARSBot::OnRep_Team()
{
	ApplyTeamVisuals();
}

void ARSBot::ApplyTeamVisuals()
{
	USkeletalMesh* Body = (Team == ERSTeam::CT) ? CTBodyMesh : TBodyMesh;
	if (Body && GetMesh()->GetSkeletalMeshAsset() != Body)
	{
		GetMesh()->SetSkeletalMeshAsset(Body);
	}
}

bool ARSBot::IsEnemy(const AActor* Other) const
{
	if (const ARSCharacter* Player = Cast<ARSCharacter>(Other))
	{
		return Player->bAlive && Player->Team != Team;
	}
	if (const ARSBot* Bot = Cast<ARSBot>(Other))
	{
		return Bot != this && Bot->Health > 0.f && Bot->Team != Team;
	}
	return false;
}

AActor* ARSBot::FindNearestEnemy() const
{
	AActor* Best = nullptr;
	float BestDistSq = FLT_MAX;

	auto Consider = [&](AActor* Candidate)
	{
		if (!IsValid(Candidate) || !IsEnemy(Candidate))
		{
			return;
		}
		const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Candidate;
		}
	};

	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		Consider(*It);
	}
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		Consider(*It);
	}
	return Best;
}

bool ARSBot::CanSee(const AActor* Target) const
{
	// ослеплён флешкой — никого не видит
	if (GetWorld()->GetTimeSeconds() < BlindUntil)
	{
		return false;
	}

	UWorld* World = GetWorld();
	const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	const FVector Aim = Target->GetActorLocation() + FVector(0.f, 0.f, 40.f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Eye, Aim, ECC_Visibility, Params);
	if (bBlocked && Hit.GetActor() != Target)
	{
		return false;
	}

	// дымовая завеса блокирует канал камеры — сквозь дым боты не видят
	FHitResult SmokeHit;
	if (World->LineTraceSingleByChannel(SmokeHit, Eye, Aim, ECC_Camera, Params)
		&& SmokeHit.GetActor() != Target)
	{
		return false;
	}
	return true;
}

void ARSBot::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Оружие в руке: мировой поворот пересчитываем в систему координат кости
	// hand_r, потому что относительный анимация затирает каждый кадр.
	// Делается у всех, включая клиентские копии — там ИИ не работает.
	if (GunMesh && GunMesh->GetStaticMesh() && Health > 0.f)
	{
		const FQuat DesiredWorld = (GetActorRotation() + FRotator(-5.f, -90.f, 0.f)).Quaternion();
		const FQuat BoneQuat = GetMesh()->GetSocketQuaternion(TEXT("hand_r"));
		const FQuat RelQuat = BoneQuat.Inverse() * DesiredWorld;
		GunMesh->SetRelativeRotation(RelQuat);
		GunMesh->SetRelativeLocation(GunHandLoc - RelQuat.RotateVector(GunPivot));
	}

	// ИИ работает только на сервере; клиенты получают движение по репликации
	if (!HasAuthority() || Health <= 0.f)
	{
		return;
	}

	// в закупку боты тоже замирают, иначе перебьют друг друга до раунда
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (State && State->Phase == ERSPhase::Intermission)
	{
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}

	// провалившихся за карту возвращаем, иначе они гибнут от KillZ
	// и раунд заканчивается сам собой
	if (GetActorLocation().Z < ARSArena::GetMapFloor(GetWorld()) - 1000.f)
	{
		SetActorLocation(ARSArena::FindSpawnPoint(GetWorld(), Team));
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}

	AActor* Enemy = FindNearestEnemy();
	const FVector Location = GetActorLocation();

	// --- бой: врага видно ---
	if (Enemy && CanSee(Enemy))
	{
		const FVector ToEnemy = Enemy->GetActorLocation() - Location;
		const float Dist = ToEnemy.Size2D();
		if (Dist < 3000.f)
		{
			const FVector Dir2D = FVector(ToEnemy.X, ToEnemy.Y, 0.f).GetSafeNormal();
			SetActorRotation(FRotator(0.f, ToEnemy.Rotation().Yaw, 0.f));
			bHasTarget = false;

			StrafeTimer -= DeltaTime;
			if (StrafeTimer <= 0.f)
			{
				StrafeDir = FMath::RandBool() ? 1.f : -1.f;
				StrafeTimer = FMath::FRandRange(0.7f, 1.6f);
			}

			// стрейфим только туда, где есть место
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Dir2D);
			if (!IsPathClear(Right * StrafeDir, 150.f))
			{
				StrafeDir = -StrafeDir;
			}
			AddMovementInput(Right, StrafeDir * 0.7f);

			if (Dist > 1200.f && IsPathClear(Dir2D, 200.f))
			{
				AddMovementInput(Dir2D, 0.8f);
			}

			FireCooldown -= DeltaTime;
			if (FireCooldown <= 0.f)
			{
				ShootAt(Enemy);
				// темп зависит от оружия: AWP бьёт редко, ПП — очередями
				const float Interval = RSWeapons::Get(Weapon).Interval;
				FireCooldown = FMath::Max(0.25f, Interval * FMath::FRandRange(2.5f, 4.f));
			}
			LastLocation = Location;
			StuckTime = 0.f;
			return;
		}
	}

	// --- патруль: идём к маршрутной точке, обходя препятствия ---
	if (!bHasTarget || FVector::Dist2D(Location, MoveTarget) < 200.f)
	{
		PickNewTarget(Enemy ? Enemy->GetActorLocation() : Location);
	}

	// застряли — берём другую точку вместо прыжков в стену
	StuckTime = (FVector::Dist2D(Location, LastLocation) < 4.f) ? StuckTime + DeltaTime : 0.f;
	LastLocation = Location;
	if (StuckTime > 1.2f)
	{
		StuckTime = 0.f;
		PickNewTarget(Location);
	}

	const FVector Steer = SteerTowards(MoveTarget);
	if (Steer.IsNearlyZero())
	{
		// направление вырождено — берём новую точку, а не стоим столбом
		PickNewTarget(Location);
		return;
	}
	AddMovementInput(Steer, 1.f);
	SetActorRotation(FRotator(0.f, Steer.Rotation().Yaw, 0.f));
}

void ARSBot::PickNewTarget(const FVector& PreferNear)
{
	// идём в сторону противника, если он известен, иначе гуляем по карте
	const FVector Location = GetActorLocation();
	const bool bChase = !PreferNear.Equals(Location, 1.f)
		&& FVector::Dist2D(PreferNear, Location) > 300.f;

	MoveTarget = bChase
		? PreferNear
		: ARSArena::GetRandomNavPoint(GetWorld(), Location, 1200.f);

	// вырожденная цель бесполезна — уводим в случайную сторону
	if (FVector::Dist2D(MoveTarget, Location) < 100.f)
	{
		MoveTarget = Location + FMath::VRand().GetSafeNormal2D() * 1000.f;
	}
	bHasTarget = true;
}

bool ARSBot::IsPathClear(const FVector& Dir, float Distance) const
{
	const FVector Flat = Dir.GetSafeNormal2D();
	if (Flat.IsNearlyZero())
	{
		return false;
	}

	const FVector Start = GetActorLocation();
	const FVector End = Start + Flat * Distance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;

	// Только геометрия карты: по каналу пешек боты видели друг друга стенами
	// и на кучном спавне намертво запирали сами себя.
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity,
		ECC_WorldStatic, FCollisionShape::MakeCapsule(34.f, 70.f), Params);

	// если капсула уже пересекает геометрию, считаем путь свободным,
	// иначе бот запрётся в углу и не сдвинется
	return !bHit || Hit.bStartPenetrating;
}

FVector ARSBot::SteerTowards(const FVector& Destination) const
{
	const FVector Direct = (Destination - GetActorLocation()).GetSafeNormal2D();
	if (IsPathClear(Direct, 250.f))
	{
		return Direct;
	}

	// пробуем обойти: сначала небольшими углами, потом всё круче
	static const float Angles[] = { 30.f, -30.f, 60.f, -60.f, 95.f, -95.f, 140.f, -140.f };
	for (float Angle : Angles)
	{
		const FVector Test = Direct.RotateAngleAxis(Angle, FVector::UpVector);
		if (IsPathClear(Test, 250.f))
		{
			return Test;
		}
	}
	return -Direct; // тупик — разворачиваемся
}

void ARSBot::ShootAt(AActor* Target)
{
	UWorld* World = GetWorld();
	const FVector Eye = GetActorLocation() + FVector(0.f, 0.f, 50.f);
	FVector Dir = (Target->GetActorLocation() + FVector(0.f, 0.f, 30.f) - Eye).GetSafeNormal();
	Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(4.f));

	const FVector End = Eye + Dir * 6000.f;
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;
	const bool bHit = World->LineTraceSingleByChannel(Hit, Eye, End, ECC_Visibility, Params);

	MulticastShot(Eye, bHit ? Hit.ImpactPoint : End);

	// пуля могла зацепить союзника на линии огня — по своим урона нет
	if (bHit && IsEnemy(Hit.GetActor()))
	{
		// урон от оружия раунда, ослабленный: боты стреляют часто и без разброса игрока
		const float Damage = RSWeapons::Get(Weapon).BodyDamage * 0.35f;
		UGameplayStatics::ApplyDamage(Hit.GetActor(), Damage, GetController(), this, nullptr);
	}
}

void ARSBot::MulticastShot_Implementation(FVector Start, FVector End)
{
	const bool bBig = RSWeapons::Get(Weapon).Mesh == ERSMeshKind::Sniper;
	ARSTracer::Spawn(GetWorld(), Start, End, bBig);

	if (USoundBase* Shot = RSAudio::GetFireSound(Weapon))
	{
		UGameplayStatics::PlaySoundAtLocation(this, Shot, Start, 0.9f);
	}
}

void ARSBot::OnRep_Health()
{
	if (Health <= 0.f)
	{
		Ragdoll();
	}
}

void ARSBot::Ragdoll()
{
	if (bRagdolled)
	{
		return;
	}
	bRagdolled = true;

	// оружие выпускаем из руки вместе с падением тела
	if (GunMesh)
	{
		GunMesh->SetVisibility(false);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetMesh()->SetSimulatePhysics(true);
}

float ARSBot::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || Health <= 0.f)
	{
		return 0.f;
	}

	// гранаты и огонь: дружественного урона нет, счёт идёт бросавшему
	AActor* KillerActor = DamageCauser;
	FString WeaponName;
	if (ARSGrenade* Nade = Cast<ARSGrenade>(DamageCauser))
	{
		if (Nade->Team == Team)
		{
			return 0.f;
		}
		KillerActor = Nade->Thrower.Get();
		WeaponName = RSWeapons::Get(Nade->Type).Name;
	}
	else if (ARSFireZone* Fire = Cast<ARSFireZone>(DamageCauser))
	{
		if (Fire->Team == Team)
		{
			return 0.f;
		}
		KillerActor = Fire->Thrower.Get();
		WeaponName = TEXT("MOLOTOV");
	}

	Health -= DamageAmount;
	if (Health <= 0.f)
	{
		Deaths++;
		// килл на счёт того, кто стрелял: игрока или бота
		FString KillerName = TEXT("?");
		uint8 KillerTeam = (uint8)ERSTeam::CT;
		if (ARSCharacter* Killer = Cast<ARSCharacter>(KillerActor))
		{
			Killer->Kills++;
			Killer->AddMoney(RSKillReward(Killer->CurrentWeapon));
			KillerName = RSCombatantName(Killer);
			if (WeaponName.IsEmpty())
			{
				WeaponName = Killer->GetWeaponName();
			}
			KillerTeam = (uint8)Killer->Team;
		}
		else if (ARSBot* BotKiller = Cast<ARSBot>(KillerActor))
		{
			BotKiller->Kills++;
			KillerName = RSCombatantName(BotKiller);
			if (WeaponName.IsEmpty())
			{
				WeaponName = RSWeapons::Get(BotKiller->Weapon).Name;
			}
			KillerTeam = (uint8)BotKiller->Team;
		}
		if (ARSGameState* GS = GetWorld()->GetGameState<ARSGameState>())
		{
			GS->MulticastAddKill(KillerName, RSCombatantName(this), WeaponName,
				bLastHitHeadshot, KillerTeam, (uint8)Team);
		}
		bLastHitHeadshot = false;
		if (ARSGameMode* GM = Cast<ARSGameMode>(GetWorld()->GetAuthGameMode()))
		{
			GM->OnCombatantDied();
		}
		// тело остаётся до конца раунда: бот не уничтожается, а воскресает
		Ragdoll();
	}
	return DamageAmount;
}
