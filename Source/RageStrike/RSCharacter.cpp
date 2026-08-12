#include "RSCharacter.h"
#include "RSBot.h"
#include "RSGameMode.h"
#include "RSGameState.h"
#include "RSArena.h"
#include "RSWeaponPickup.h"
#include "RSPlayerController.h"
#include "RSGrenade.h"
#include "RSFireZone.h"
#include "RSHUD.h"
#include "RSMaps.h"
#include "RSMatchSettings.h"
#include "RSAudio.h"
#include "RSTracer.h"
#include "RSViewModel.h"
#include "RSCheatMenu.h"
#include "Widgets/SWeakWidget.h"
#include "Engine/GameViewportClient.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

int32 RSKillReward(ERSWeapon W)
{
	return RSWeapons::Get(W).KillReward;
}

FString RSCombatantName(const AActor* Who)
{
	if (const ARSBot* Bot = Cast<ARSBot>(Who))
	{
		// у ботов есть свой ник, номер оставляем только как запасной вариант
		return Bot->Nick.IsEmpty() ? FString::Printf(TEXT("Бот %d"), Bot->BotNumber) : Bot->Nick;
	}
	if (const ARSCharacter* Player = Cast<ARSCharacter>(Who))
	{
		return Player->Nick.IsEmpty() ? TEXT("Игрок") : Player->Nick;
	}
	return TEXT("?");
}

int32 RSPenetrationPower(ERSWeapon W)
{
	// Сколько преград осилит пуля. Как в CS: снайперка шьёт лучше винтовки,
	// пистолет — одну тонкую, дробь и нож не шьют вовсе.
	switch (RSWeapons::Get(W).Mesh)
	{
	case ERSMeshKind::Sniper:  return 3;
	case ERSMeshKind::RifleAK:
	case ERSMeshKind::RifleM4: return 2;
	case ERSMeshKind::Pistol:  return 1;
	default:                   return 0;
	}
}

uint8 RSComputeKillFlags(const AActor* Killer, const AActor* Victim, bool bHeadshot)
{
	uint8 Flags = bHeadshot ? RSKill::Headshot : 0;

	// Пробитие пришло с выстрелом, а не из состояния: читаем метку, которую
	// оставила на жертве FireOnePellet.
	if (const ARSCharacter* VC = Cast<ARSCharacter>(Victim))
	{
		if (VC->bLastHitThroughWall) { Flags |= RSKill::Penetrate; }
	}
	else if (const ARSBot* VB = Cast<ARSBot>(Victim))
	{
		if (VB->bLastHitThroughWall) { Flags |= RSKill::Penetrate; }
	}
	if (!Killer || !Victim || Killer == Victim)
	{
		return Flags;
	}

	const UWorld* World = Killer->GetWorld();
	if (!World)
	{
		return Flags;
	}
	const float Now = World->GetTimeSeconds();

	// Ослепление и «без прицела» берутся из состояния убийцы. У игрока и бота
	// это разные поля: игрок хранит момент конца засветки, бот — свой BlindUntil.
	if (const ARSCharacter* KC = Cast<ARSCharacter>(Killer))
	{
		if (Now < KC->FlashEndTime)
		{
			Flags |= RSKill::Blind;
		}
		if (RSWeapons::Get(KC->CurrentWeapon).Mesh == ERSMeshKind::Sniper && !KC->bAimingNow)
		{
			Flags |= RSKill::Noscope;
		}
	}
	else if (const ARSBot* KB = Cast<ARSBot>(Killer))
	{
		if (Now < KB->BlindUntil)
		{
			Flags |= RSKill::Blind;
		}
		// боты прицелом не пользуются, поэтому Noscope им не ставим:
		// иначе значок висел бы на каждом их убийстве снайперкой
	}

	// Дым опознаём по актору-гранате, а не по самому факту помехи: канал
	// камеры блокирует и обычная геометрия, из-за чего значок дыма вылезал
	// на убийствах, где никакого дыма не было — хватало задетого угла стены
	// или ступеньки. Дымовые сферы висят компонентами на ARSGrenade, поэтому
	// достаточно проверить владельца попадания.
	//
	// Трасса идёт от глаз, а не от груди: выстрел шёл именно оттуда, и над
	// низким укрытием линии от груди и от глаз расходятся.
	FVector From = Killer->GetActorLocation() + FVector(0.f, 0.f, 50.f);
	if (const APawn* KillerPawn = Cast<APawn>(Killer))
	{
		From = KillerPawn->GetPawnViewLocation();
	}
	const FVector To = Victim->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RSKillSmoke), false, Killer);
	Params.AddIgnoredActor(Victim);
	Params.bTraceComplex = true;

	// Мультитрасса, а не одиночная: если первой на пути окажется геометрия,
	// одиночная остановится на ней и дым за ней остался бы незамеченным.
	TArray<FHitResult> Hits;
	if (World->LineTraceMultiByChannel(Hits, From, To, ECC_Camera, Params))
	{
		for (const FHitResult& H : Hits)
		{
			if (Cast<ARSGrenade>(H.GetActor()))
			{
				Flags |= RSKill::Smoke;
				break;
			}
		}
	}

	return Flags;
}

ARSCharacter::ARSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	// Пули теперь ловит скелетный меш, а не капсула: только так у попадания
	// есть кость, а значит хитбокс. Капсула осталась для движения и толкания.
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(GetCapsuleComponent());
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	SpringArm->TargetArmLength = 300.f;
	SpringArm->SocketOffset = FVector(0.f, 60.f, 30.f);
	SpringArm->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
	Camera->bUsePawnControlRotation = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RifleMesh(
		TEXT("/Game/Fab/Soviet_Assault_Rifle/ak47fbx/StaticMeshes/ak47fbx.ak47fbx"));
	if (RifleMesh.Succeeded())
	{
		AKAsset = RifleMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> M4Mesh(
		TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle"));
	if (M4Mesh.Succeeded())
	{
		M4Asset = M4Mesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SniperMesh(
		TEXT("/Game/Weapons/Sniper/Meshes/SKM_SniperR700.SKM_SniperR700"));
	if (SniperMesh.Succeeded())
	{
		SniperAsset = SniperMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> KnifeMesh(
		TEXT("/Game/Weapons/Knife/Meshes/Combat_Knife.Combat_Knife"));
	if (KnifeMesh.Succeeded())
	{
		KnifeAsset = KnifeMesh.Object;
	}
	// в модели 9mm лежат три пистолета сразу, поэтому берём одиночный
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PistolMesh(
		TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol_G01.SM_Pistol_G01"));
	if (PistolMesh.Succeeded())
	{
		PistolAsset = PistolMesh.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Ball.Succeeded())
	{
		GrenadeAsset = Ball.Object;
	}

	// руки от первого лица: пока скрыты, идёт подбор посадки по замерам
	ArmsMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmsMesh"));
	ArmsMesh->SetupAttachment(Camera);
	ArmsMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArmsMesh->SetOnlyOwnerSee(true);
	ArmsMesh->SetCastShadow(false);
	ArmsMesh->SetRelativeLocation(FVector(-30.f, 0.f, -150.f));
	ArmsMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmsAsset(
		TEXT("/Game/FP_AKS74U_Animation/Demo/FirstPersonArms/Character/Mesh/SK_FP_Manny_Simple.SK_FP_Manny_Simple"));
	if (ArmsAsset.Succeeded())
	{
		ArmsMesh->SetSkeletalMesh(ArmsAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAnim(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Idle_Loop.A_FP_AKS74U_Idle_Loop"));
	if (IdleAnim.Succeeded()) { AnimIdle = IdleAnim.Object; }

	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Walk_F_Loop.A_FP_AKS74U_Walk_F_Loop"));
	if (WalkAnim.Succeeded()) { AnimWalk = WalkAnim.Object; }

	static ConstructorHelpers::FObjectFinder<UAnimSequence> FireAnim(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Fire.A_FP_AKS74U_Fire"));
	if (FireAnim.Succeeded()) { AnimFire = FireAnim.Object; }

	static ConstructorHelpers::FObjectFinder<UAnimSequence> ReloadAnim(
		TEXT("/Game/FP_AKS74U_Animation/AKS74U/Animations/A_FP_AKS74U_Reload.A_FP_AKS74U_Reload"));
	if (ReloadAnim.Succeeded()) { AnimReload = ReloadAnim.Object; }

	// оружие в руках: FP-версию видит только владелец, TP-версию — все остальные
	GunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GunMesh"));
	GunMesh->SetupAttachment(Camera);
	GunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GunMesh->SetStaticMesh(AKAsset);
	GunMesh->SetOnlyOwnerSee(true);

	// скелетная вьюмодель: висит у камеры, видна только владельцу
	FPGun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPGun"));
	FPGun->SetupAttachment(Camera);
	FPGun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FPGun->SetOnlyOwnerSee(true);
	FPGun->SetCastShadow(false);
	FPGun->SetVisibility(false);

	TPGunMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TPGunMesh"));
	TPGunMesh->SetupAttachment(GetMesh(), TEXT("hand_r"));
	TPGunMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TPGunMesh->SetStaticMesh(AKAsset);
	TPGunMesh->SetRelativeLocation(FVector(-8.f, 4.f, -2.f));
	TPGunMesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));

	// тело: в виде от первого лица скрыто от владельца, но тень отбрасывает
	// CT — военный, T — боевик. Скелеты у них свои, но помечены совместимыми
	// со скелетом манекена, поэтому общий ABP_Unarmed играет на обоих.
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> CTMesh(
		TEXT("/Game/QuantumCharacter/Mesh/SKM_QuantumCharacter.SKM_QuantumCharacter"));
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> TMesh(
		TEXT("/Game/Insurgent_2/Mesh/SK_Preset3.SK_Preset3"));
	static ConstructorHelpers::FClassFinder<UAnimInstance> UnarmedAnim(
		TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
	if (CTMesh.Succeeded())
	{
		CTBodyMesh = CTMesh.Object;
		GetMesh()->SetSkeletalMesh(CTBodyMesh);
		GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
		GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
	if (TMesh.Succeeded())
	{
		TBodyMesh = TMesh.Object;
	}
	if (UnarmedAnim.Succeeded())
	{
		GetMesh()->SetAnimInstanceClass(UnarmedAnim.Class);
	}
	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetCastHiddenShadow(true);

	// коллизия по костям: физический ассет уже есть (иначе не работал бы
	// рэгдолл), и его тела дают Hit.BoneName для расчёта хитбокса
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GetMesh()->SetCollisionObjectType(ECC_Pawn);
	GetMesh()->SetCollisionResponseToAllChannels(ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	static ConstructorHelpers::FObjectFinder<USoundBase> Fire(
		TEXT("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02"));
	if (Fire.Succeeded())
	{
		FireSound = Fire.Object;
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = 460.f;
	Move->MaxWalkSpeedCrouched = 160.f;
	Move->JumpZVelocity = 450.f;
	Move->AirControl = 0.35f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	// контр-стрейф как в CS: быстрый разгон и почти мгновенная остановка,
	// нажатие противоположной клавиши тормозит сразу
	Move->MaxAcceleration = 4500.f;
	Move->BrakingDecelerationWalking = 2500.f;
	Move->GroundFriction = 6.f;
	Move->BrakingFrictionFactor = 2.f;

	bUseControllerRotationYaw = true;

	// чаще шлём обновления: на 30 в секунду чужой игрок заметно дёргается
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(30.f);
	SetReplicateMovement(true);
	Move->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
}

float ARSCharacter::GetWeaponRealLength(ERSWeapon W)
{
	// реальные габариты в сантиметрах — по ним нормализуем чужие модели
	switch (RSWeapons::Get(W).Slot)
	{
	case ERSSlot::Secondary: return 22.f;
	case ERSSlot::Knife:     return 30.f;
	case ERSSlot::Grenade:   return 12.f;
	default:
		return (RSWeapons::Get(W).Mesh == ERSMeshKind::Sniper) ? 120.f : 90.f;
	}
}

void ARSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARSCharacter, Health);
	DOREPLIFETIME(ARSCharacter, Kills);
	DOREPLIFETIME(ARSCharacter, Deaths);
	DOREPLIFETIME(ARSCharacter, CurrentWeapon);
	DOREPLIFETIME(ARSCharacter, Team);
	DOREPLIFETIME(ARSCharacter, bAlive);
	DOREPLIFETIME(ARSCharacter, Nick);
	DOREPLIFETIME(ARSCharacter, Money);
	DOREPLIFETIME(ARSCharacter, Armor);
	DOREPLIFETIME(ARSCharacter, bHasHelmet);
	DOREPLIFETIME(ARSCharacter, bHasPrimary);
	DOREPLIFETIME(ARSCharacter, PrimaryType);
	DOREPLIFETIME(ARSCharacter, bHasSecondary);
	DOREPLIFETIME(ARSCharacter, SecondaryType);
	DOREPLIFETIME(ARSCharacter, Grenades);
	DOREPLIFETIME(ARSCharacter, AntiAimYaw);
	DOREPLIFETIME(ARSCharacter, AntiAimPitchRep);
}

void ARSCharacter::GiveWeapon(ERSWeapon Type)
{
	const FRSWeaponDef& Def = RSWeapons::Get(Type);
	if (Def.Slot == ERSSlot::Primary)
	{
		bHasPrimary = true;
		PrimaryType = Type;
	}
	else if (Def.Slot == ERSSlot::Secondary)
	{
		bHasSecondary = true;
		SecondaryType = Type;
	}
	Ammo[(uint8)Type] = Def.Mag;
	Reserve[(uint8)Type] = Def.ReserveMax;
}

bool ARSCharacter::TryPickUpWeapon(ERSWeapon Type)
{
	const FRSWeaponDef& Def = RSWeapons::Get(Type);
	if (Def.Slot == ERSSlot::Secondary)
	{
		if (bHasSecondary)
		{
			return false;
		}
		GiveWeapon(Type);
		return true;
	}
	if (Def.Slot != ERSSlot::Primary || bHasPrimary)
	{
		return false;
	}
	GiveWeapon(Type);
	return true;
}

void ARSCharacter::DropWeapon(ERSWeapon Type)
{
	const ERSSlot Slot = RSWeapons::Get(Type).Slot;
	if (!HasAuthority() || (Slot != ERSSlot::Primary && Slot != ERSSlot::Secondary))
	{
		return;
	}

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector Where = GetActorLocation() + GetActorForwardVector() * 80.f;

	if (ARSWeaponPickup* Pickup = GetWorld()->SpawnActor<ARSWeaponPickup>(
		ARSWeaponPickup::StaticClass(), Where, FRotator::ZeroRotator, SP))
	{
		Pickup->DropAt(Type, Where);
	}

	if (Slot == ERSSlot::Secondary)
	{
		bHasSecondary = false;
	}
	else
	{
		bHasPrimary = false;
	}
}

void ARSCharacter::DropCurrentWeapon()
{
	if (!bAlive)
	{
		return;
	}
	if (!HasAuthority())
	{
		ServerDropWeapon();
		return;
	}

	const ERSWeapon Type = CurrentWeapon;
	const ERSSlot Slot = RSWeapons::Get(Type).Slot;
	if (Slot != ERSSlot::Primary && Slot != ERSSlot::Secondary)
	{
		return; // нож и гранаты не выбрасываются
	}

	DropWeapon(Type);

	// берём в руки то, что осталось
	CurrentWeapon = bHasPrimary ? PrimaryType
		: (bHasSecondary ? SecondaryType : ERSWeapon::Knife);
	ApplyWeaponVisuals();
}

void ARSCharacter::ServerDropWeapon_Implementation()
{
	DropCurrentWeapon();
}

void ARSCharacter::AddMoney(int32 Amount)
{
	Money = FMath::Clamp(Money + Amount, 0, 16000);
}

void ARSCharacter::ServerBuyWeapon_Implementation(ERSWeapon Weapon)
{
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State || !State->IsBuyTime())
	{
		return; // покупки только между раундами
	}

	const FRSWeaponDef& Def = RSWeapons::Get(Weapon);
	if (Money < Def.Price || !RSWeapons::AllowedFor(Weapon, Team))
	{
		return;
	}

	if (Def.Slot == ERSSlot::Grenade)
	{
		const int32 GI = RSWeapons::GrenadeIndex(Weapon);
		int32 Total = 0;
		for (uint8 N : Grenades)
		{
			Total += N;
		}
		const int32 MaxOfType = (Weapon == ERSWeapon::Flashbang) ? 2 : 1;
		if (Grenades[GI] >= MaxOfType || Total >= 4)
		{
			return;
		}
		Money -= Def.Price;
		Grenades[GI]++;
		return;
	}

	if (Def.Slot == ERSSlot::Primary)
	{
		if (bHasPrimary)
		{
			if (PrimaryType == Weapon)
			{
				return; // уже это и держим
			}
			// как в CS: старое основное падает на землю, в руки идёт новое
			DropWeapon(PrimaryType);
		}
		Money -= Def.Price;
		GiveWeapon(Weapon);
		CurrentWeapon = Weapon;
		ApplyWeaponVisuals();
		return;
	}

	if (Def.Slot == ERSSlot::Secondary)
	{
		if (bHasSecondary && SecondaryType == Weapon)
		{
			return;
		}
		// старый пистолет тоже падает на землю, а не исчезает
		if (bHasSecondary)
		{
			DropWeapon(SecondaryType);
		}
		Money -= Def.Price;
		GiveWeapon(Weapon);
		CurrentWeapon = Weapon;
		ApplyWeaponVisuals();
	}
}

void ARSCharacter::ServerBuyArmor_Implementation(bool bWithHelmet)
{
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State || !State->IsBuyTime())
	{
		return;
	}
	const int32 Price = bWithHelmet ? PriceKevlarHelmet : PriceKevlar;
	// уже полная броня и шлем — покупать нечего
	if (Money < Price || (Armor >= 100.f && (!bWithHelmet || bHasHelmet)))
	{
		return;
	}
	Money -= Price;
	Armor = 100.f;
	if (bWithHelmet)
	{
		bHasHelmet = true;
	}
}

void ARSCharacter::SetTeam(ERSTeam NewTeam)
{
	if (Team == NewTeam)
	{
		return;
	}
	Team = NewTeam;
	ApplyTeamVisuals();

	if (HasAuthority())
	{
		if (ARSGameMode* GM = GetWorld()->GetAuthGameMode<ARSGameMode>())
		{
			GM->OnPlayerTeamChanged();
		}
	}
	else
	{
		ServerSetTeam(NewTeam);
	}
}

void ARSCharacter::ServerSetTeam_Implementation(ERSTeam NewTeam)
{
	Team = NewTeam;
	ApplyTeamVisuals();
	if (ARSGameMode* GM = GetWorld()->GetAuthGameMode<ARSGameMode>())
	{
		GM->OnPlayerTeamChanged();
	}
}

void ARSCharacter::OnRep_Team()
{
	ApplyTeamVisuals();
}

void ARSCharacter::ApplyTeamVisuals()
{
	USkeletalMesh* Body = (Team == ERSTeam::CT) ? CTBodyMesh : TBodyMesh;
	if (Body && GetMesh()->GetSkeletalMeshAsset() != Body)
	{
		GetMesh()->SetSkeletalMeshAsset(Body);
	}
}

void ARSCharacter::BeginPlay()
{
	Super::BeginPlay();

	ApplyWeaponVisuals();
	ApplyViewMode();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalController())
		{
			PC->bShowMouseCursor = false;
			// ник задан в контроллере ещё до появления пешки — забираем его
			if (const ARSPlayerController* RSPC = Cast<ARSPlayerController>(PC))
			{
				ApplyNick(RSPC->PlayerNick);
			}
		}
	}
}

void ARSCharacter::ApplyNick(const FString& NewNick)
{
	Nick = NewNick;
	if (!HasAuthority())
	{
		ServerSetNick(NewNick);
	}
}

void ARSCharacter::ServerSetNick_Implementation(const FString& NewNick)
{
	Nick = NewNick.Left(16);
}

void ARSCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAxis("MoveForward", this, &ARSCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ARSCharacter::MoveRight);
	PlayerInputComponent->BindAxis("Turn", this, &ARSCharacter::Turn);
	PlayerInputComponent->BindAxis("LookUp", this, &ARSCharacter::LookUp);

	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ARSCharacter::StartJump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ARSCharacter::StopJump);
	PlayerInputComponent->BindAction("CrouchAction", IE_Pressed, this, &ARSCharacter::StartCrouchInput);
	PlayerInputComponent->BindAction("CrouchAction", IE_Released, this, &ARSCharacter::StopCrouchInput);
	PlayerInputComponent->BindAction("Sprint", IE_Pressed, this, &ARSCharacter::StartWalk);
	PlayerInputComponent->BindAction("Sprint", IE_Released, this, &ARSCharacter::StopWalk);
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ARSCharacter::StartFire);
	PlayerInputComponent->BindAction("Fire", IE_Released, this, &ARSCharacter::StopFire);
	PlayerInputComponent->BindAction("Reload", IE_Pressed, this, &ARSCharacter::Reload);

	// Клавиш F1-F8 под читы больше нет: они переключаются мышью в оверлее,
	// который открывается на Del или Insert.

	// B освободили под закупку, управление ботами ушло на F9-F12
	PlayerInputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ARSCharacter::AddBot);
	PlayerInputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ARSCharacter::RemoveBot);
	// F11/F12 отмечают спавны команд там, где стоит игрок
	PlayerInputComponent->BindKey(EKeys::F11, IE_Pressed, this, &ARSCharacter::MarkSpawnT);
	PlayerInputComponent->BindKey(EKeys::F12, IE_Pressed, this, &ARSCharacter::MarkSpawnCT);
	// закупка держится на зажатой клавише, как просили
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &ARSCharacter::OpenBuyMenu);
	PlayerInputComponent->BindKey(EKeys::B, IE_Released, this, &ARSCharacter::CloseBuyMenu);
	PlayerInputComponent->BindKey(EKeys::G, IE_Pressed, this, &ARSCharacter::DropCurrentWeapon);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ARSCharacter::ShowScoreboard);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Released, this, &ARSCharacter::HideScoreboard);

	// цифры: слоты оружия или навигация в закупке
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &ARSCharacter::Num1);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ARSCharacter::Num2);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ARSCharacter::Num3);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ARSCharacter::Num4);
	PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ARSCharacter::Num5);
	PlayerInputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ARSCharacter::Num6);
	PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ARSCharacter::Num0);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &ARSCharacter::StartInspect);
	// оверлей читов открывается с двух привычных клавиш
	PlayerInputComponent->BindKey(EKeys::Delete, IE_Pressed, this, &ARSCharacter::ToggleCheatMenu);
	PlayerInputComponent->BindKey(EKeys::Insert, IE_Pressed, this, &ARSCharacter::ToggleCheatMenu);
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &ARSCharacter::ToggleView);

	// Режим подгонки вьюмодели: F8 включает, дальше нампад двигает модель.
	// Значения сразу пишутся в настройки, поэтому подбор переживает
	// перезапуск и не требует пересборки.
	PlayerInputComponent->BindKey(EKeys::F8, IE_Pressed, this, &ARSCharacter::ToggleVMTune);
	// Раскладка под клавиатуру без нампада: стрелки и навигационный блок.
	// Все они работают только при включённом режиме подгонки.
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ARSCharacter::VMFwd);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ARSCharacter::VMBack);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &ARSCharacter::VMLeft);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &ARSCharacter::VMRight);
	PlayerInputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &ARSCharacter::VMUp);
	PlayerInputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &ARSCharacter::VMDown);
	PlayerInputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &ARSCharacter::VMBigger);
	PlayerInputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &ARSCharacter::VMSmaller);
	PlayerInputComponent->BindKey(EKeys::Period, IE_Pressed, this, &ARSCharacter::VMYawPlus);
	PlayerInputComponent->BindKey(EKeys::Comma, IE_Pressed, this, &ARSCharacter::VMYawMinus);
	PlayerInputComponent->BindKey(EKeys::Apostrophe, IE_Pressed, this, &ARSCharacter::VMPitchPlus);
	PlayerInputComponent->BindKey(EKeys::Semicolon, IE_Pressed, this, &ARSCharacter::VMRollPlus);
	PlayerInputComponent->BindKey(EKeys::Home, IE_Pressed, this, &ARSCharacter::CycleVMStep);
	PlayerInputComponent->BindKey(EKeys::End, IE_Pressed, this, &ARSCharacter::ResetVMPlace);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ARSCharacter::StartAim);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &ARSCharacter::StopAim);
}

int32 ARSCharacter::GetAmmo() const        { return Ammo[(uint8)CurrentWeapon]; }
int32 ARSCharacter::GetMaxAmmo() const     { return RSWeapons::Get(CurrentWeapon).Mag; }
int32 ARSCharacter::GetReserveAmmo() const { return Reserve[(uint8)CurrentWeapon]; }

FString ARSCharacter::GetWeaponName() const
{
	return RSWeapons::Get(CurrentWeapon).Name;
}

float ARSCharacter::GetWeaponMaxSpeed() const
{
	// как в CS: с ножом бегаешь быстрее всех, с AWP — ползаешь
	return RSWeapons::Get(CurrentWeapon).Speed;
}

void ARSCharacter::PlayArmsAnim(UAnimSequence* Anim, bool bLoop, float LockSeconds)
{
	if (!ArmsMesh || !Anim)
	{
		return;
	}
	if (LockSeconds <= 0.f && CurrentArmsAnim == Anim)
	{
		return;
	}
	CurrentArmsAnim = Anim;
	ArmsMesh->PlayAnimation(Anim, bLoop);
	ArmsAnimLockUntil = GetWorld()->GetTimeSeconds() + LockSeconds;
}

void ARSCharacter::UpdateArmsAnimation()
{
	if (!ArmsMesh || !ArmsMesh->IsVisible() || !ArmsMesh->GetSkeletalMeshAsset())
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() < ArmsAnimLockUntil)
	{
		return; // выстрел или перезарядка доигрывают
	}

	const bool bMoving = GetVelocity().Size2D() > 20.f && !IsFrozen();
	PlayArmsAnim((bMoving && AnimWalk) ? AnimWalk : AnimIdle, true, 0.f);
}

void ARSCharacter::ToggleBuyMenu()
{
	SetBuyMenuOpen(!bBuyMenuOpen);
}

void ARSCharacter::SetBuyMenuOpen(bool bOpen)
{
	if (bBuyMenuOpen == bOpen)
	{
		return;
	}
	// открывается всегда: подсказка внутри объяснит, почему покупка недоступна
	bBuyMenuOpen = bOpen;
	BuyCategory = -1;

	// в закупке нужен курсор: карточки кликаются мышью
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalController())
		{
			PC->bShowMouseCursor = bBuyMenuOpen;
			if (bBuyMenuOpen)
			{
				FInputModeGameAndUI Mode;
				Mode.SetLockMouseToViewportBehavior(EMouseLockMode::LockOnCapture);
				Mode.SetHideCursorDuringCapture(false);
				PC->SetInputMode(Mode);
				// ставим курсор в центр, иначе он остаётся там, где был в меню
				int32 SizeX = 0, SizeY = 0;
				PC->GetViewportSize(SizeX, SizeY);
				PC->SetMouseLocation(SizeX / 2, SizeY / 2);
			}
			else
			{
				PC->SetInputMode(FInputModeGameOnly());
			}
		}
	}
}

void ARSCharacter::ToggleCheatMenu()
{
	bCheatMenuOpen = !bCheatMenuOpen;

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || !PC->IsLocalController() || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	if (bCheatMenuOpen)
	{
		SAssignNew(CheatMenuWidget, SRSCheatMenu).Owner(this);
		GEngine->GameViewport->AddViewportWidgetContent(
			SAssignNew(CheatMenuContainer, SWeakWidget)
				.PossiblyNullContent(CheatMenuWidget.ToSharedRef()), 90);

		// Только интерфейс: иначе мышь под меню продолжает крутить камеру,
		// а WASD уходят в персонажа — та же болячка, что была у главного меню.
		PC->bShowMouseCursor = true;
		FInputModeUIOnly Mode;
		Mode.SetWidgetToFocus(CheatMenuWidget);
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(Mode);
	}
	else
	{
		if (CheatMenuContainer.IsValid())
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(CheatMenuContainer.ToSharedRef());
		}
		CheatMenuContainer.Reset();
		CheatMenuWidget.Reset();

		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

void ARSCharacter::ToggleCheatByIndex(int32 Index)
{
	// порядок совпадает со строками оверлея
	switch (Index)
	{
	case 0: bAimbot = !bAimbot; break;
	case 1: bESP = !bESP; break;
	case 2: bTriggerbot = !bTriggerbot; break;
	case 3: bNoRecoilSpread = !bNoRecoilSpread; break;
	case 4: bSpeedhack = !bSpeedhack; SyncCheats(); break;
	case 5: bSilentAim = !bSilentAim; break;
	case 6: bGodMode = !bGodMode; SyncCheats(); break;
	case 7: bInfiniteMoney = !bInfiniteMoney; SyncCheats(); break;
	case 8: bAntiAim = !bAntiAim; SyncCheats(); break;
	case 9: bPredict = !bPredict; break;
	default: break;
	}
}

void ARSCharacter::ApplyCheatSetting(int32 Id, int32 Delta)
{
	switch (Id)
	{
	case 0: // режим анти-аима по кругу
		AntiAimMode = (AntiAimMode + Delta + AntiAimModes) % AntiAimModes;
		SyncCheats();
		break;
	case 1: // качание: 0 — ровно спиной, дальше заметнее
		AntiAimSwing = FMath::Clamp(AntiAimSwing + Delta * 5.f, 0.f, 90.f);
		SyncCheats();
		break;
	case 2: // наклон: вниз до -89, вверх до +89
		AntiAimPitch = FMath::Clamp(AntiAimPitch + Delta * 5.f, -89.f, 89.f);
		SyncCheats();
		break;
	case 3: // скорость кручения
		AntiAimSpin = FMath::Clamp(AntiAimSpin + Delta * 30.f, 30.f, 1440.f);
		SyncCheats();
		break;
	case 4: // глубина упреждения в тиках по 1/64 с
		PredictTicks = FMath::Clamp(PredictTicks + Delta, 1, 32);
		break;
	case 5: bPredictOnlyHidden = !bPredictOnlyHidden; break;
	case 11: // конус триггербота: 0 — только точный луч
		TriggerFov = FMath::Clamp(TriggerFov + Delta * 0.5f, 0.f, 20.f);
		break;
	case 6: bEspBox = !bEspBox; break;
	case 7: bEspHealth = !bEspHealth; break;
	case 8: bEspDist = !bEspDist; break;
	case 9: bEspLine = !bEspLine; break;
	case 10: bEspMark = !bEspMark; break;
	default: break;
	}
}

void ARSCharacter::HandleNumberKey(int32 N)
{
	if (bBuyMenuOpen)
	{
		if (N == 0)
		{
			BuyCategory = -1; // назад к категориям
			return;
		}
		if (BuyCategory < 0)
		{
			if (N >= 1 && N <= RSWeapons::BuyCategories)
			{
				BuyCategory = N - 1; // цифра совпадает с номером колонки на экране
			}
			return;
		}
		if (BuyCategory == RSWeapons::EquipmentCategory)
		{
			if (N == 1) { ServerBuyArmor(false); }
			if (N == 2) { ServerBuyArmor(true); }
			return;
		}
		const TArray<ERSWeapon> Items = RSWeapons::BuyCategory(BuyCategory, Team);
		if (N >= 1 && N <= Items.Num())
		{
			ServerBuyWeapon(Items[N - 1]);
		}
		return;
	}

	// вне закупки — слоты как в CS
	switch (N)
	{
	case 1:
		if (bHasPrimary) { RequestWeapon(PrimaryType); }
		break;
	case 2:
		if (bHasSecondary) { RequestWeapon(SecondaryType); }
		break;
	case 3:
		RequestWeapon(ERSWeapon::Knife);
		break;
	case 4:
		SelectGrenade();
		break;
	default:
		break;
	}
}

void ARSCharacter::SelectGrenade()
{
	// листаем имеющиеся гранаты по кругу
	const int32 CurrentGI = RSWeapons::GrenadeIndex(CurrentWeapon);
	for (int32 Step = 1; Step <= RSWeapons::GrenadeTypes; Step++)
	{
		const int32 GI = (CurrentGI + Step) % RSWeapons::GrenadeTypes;
		if (Grenades[GI] > 0)
		{
			RequestWeapon(RSWeapons::GrenadeByIndex(GI));
			return;
		}
	}
}

void ARSCharacter::StartInspect()
{
	// осмотр оружия, как F в CS2: только когда ничего не мешает
	const float Now = GetWorld()->GetTimeSeconds();
	if (bAlive && !bReloading && !bFireHeld && Now > InspectEndTime)
	{
		InspectEndTime = Now + 2.6f;
	}
}

void ARSCharacter::UpdateFootsteps(float DeltaTime)
{
	if (!bAlive || IsFrozen())
	{
		return;
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bFalling = Move->IsFalling();

	// приземление
	if (bWasFallingAudio && !bFalling)
	{
		RSAudio::PlayAt(this, RSAudio::Get(RSAudio::ESound::Land),
			GetActorLocation(), 0.8f, RSAudio::ERange::Step);
		StepDistance = 0.f;
	}
	bWasFallingAudio = bFalling;

	if (bFalling)
	{
		return;
	}

	const float Speed = GetVelocity().Size2D();
	if (Speed < 40.f)
	{
		StepDistance = 0.f;
		return;
	}

	// шаг раз в 180 см пути: на бегу чаще, на тихой ходьбе реже и глуше
	StepDistance += Speed * DeltaTime;
	if (StepDistance < 180.f)
	{
		return;
	}
	StepDistance = 0.f;

	const bool bQuiet = bWalking || bIsCrouched;
	const bool bRunning = !bQuiet && Speed > GetWeaponMaxSpeed() * 0.6f;
	// тихая ходьба и есть тихая ходьба: слышно только вблизи
	RSAudio::PlayAt(this, RSAudio::GetStepSound(bRunning), GetActorLocation(),
		bQuiet ? 0.25f : (bRunning ? 1.f : 0.7f), RSAudio::ERange::Step,
		!IsLocallyControlled());
}

void ARSCharacter::MarkSpawn(bool bCT)
{
	if (!IsLocallyControlled())
	{
		return;
	}
	const int32 MapIndex = RSMaps::GetSelectedIndex();
	RSMaps::SetCustomSpawn(MapIndex, bCT, GetActorLocation());

	// подтверждение прямо на экран: в бою в лог никто не смотрит
	SpawnMarkMessage = FString::Printf(TEXT("Спавн %s отмечен: %s"),
		bCT ? TEXT("КТ") : TEXT("Т"), *GetActorLocation().ToCompactString());
	SpawnMarkUntil = GetWorld()->GetTimeSeconds() + 4.f;

	UE_LOG(LogTemp, Log, TEXT("RageStrike: spawn %s for map %d = %s"),
		bCT ? TEXT("CT") : TEXT("T"), MapIndex, *GetActorLocation().ToString());
}

void ARSCharacter::RequestWeapon(ERSWeapon NewWeapon)
{
	if (CurrentWeapon == NewWeapon)
	{
		return;
	}
	// нельзя достать пустой слот
	const FRSWeaponDef& Def = RSWeapons::Get(NewWeapon);
	if (Def.Slot == ERSSlot::Secondary && (!bHasSecondary || SecondaryType != NewWeapon))
	{
		return;
	}
	if (Def.Slot == ERSSlot::Primary && (!bHasPrimary || PrimaryType != NewWeapon))
	{
		return;
	}
	if (Def.Slot == ERSSlot::Grenade && Grenades[RSWeapons::GrenadeIndex(NewWeapon)] <= 0)
	{
		return;
	}
	CurrentWeapon = NewWeapon;
	bReloading = false;
	bAiming = false;
	bAimingNow = false;
	Camera->SetFieldOfView(RSOptions::GetFov());
	GetWorldTimerManager().ClearTimer(ReloadTimer);
	ApplyWeaponVisuals();

	if (!HasAuthority())
	{
		ServerSetWeapon(NewWeapon);
	}
}

void ARSCharacter::ServerSetWeapon_Implementation(ERSWeapon NewWeapon)
{
	CurrentWeapon = NewWeapon;
	ApplyWeaponVisuals();
}

void ARSCharacter::OnRep_Weapon()
{
	ApplyWeaponVisuals();
}

void ARSCharacter::ApplyWeaponVisuals()
{
	if (!GunMesh || !TPGunMesh)
	{
		return;
	}

	// Пустые слоты означают нож: раньше заглушкой по умолчанию стоял автомат,
	// и после сброса основного с пистолетом в руках оказывался калаш даже у КТ.
	{
		const ERSSlot Slot = RSWeapons::Get(CurrentWeapon).Slot;
		const bool bSlotEmpty =
			(Slot == ERSSlot::Primary && (!bHasPrimary || PrimaryType != CurrentWeapon))
			|| (Slot == ERSSlot::Secondary && (!bHasSecondary || SecondaryType != CurrentWeapon))
			|| (Slot == ERSSlot::Grenade && Grenades[RSWeapons::GrenadeIndex(CurrentWeapon)] <= 0);
		if (bSlotEmpty)
		{
			CurrentWeapon = ERSWeapon::Knife;
		}
	}

	UStaticMesh* WeaponMesh = KnifeAsset;
	TPGunAlign = FQuat::Identity;            // у заглушек оси уже правильные
	FVector MeshPivot = FVector::ZeroVector; // смещение центра модели от пивота
	float FitFP = 0.f;                       // масштаб вьюмодели, 0 — как у мира
	FVector HandLoc(-6.f, 2.f, -2.f);   // в руке тела (вид от третьего лица)
	FRotator HandRot(0.f, 0.f, 90.f);
	FVector CamLoc(35.f, 18.f, -20.f);  // у камеры (вид от первого лица без рук)
	FRotator CamRot(0.f, -90.f, 0.f);
	FVector Scale = FVector::OneVector;

	switch (RSWeapons::Get(CurrentWeapon).Mesh)
	{
	case ERSMeshKind::Knife:
		WeaponMesh = KnifeAsset;
		HandLoc = FVector(-4.f, 2.f, 0.f);
		HandRot = FRotator(0.f, 90.f, 90.f);
		CamLoc = FVector(33.f, 13.f, -9.f);
		CamRot = FRotator(80.f, 0.f, 180.f);
		Scale = FVector(1.25f);
		break;

	case ERSMeshKind::Pistol:
		// модель почти в пять раз крупнее натуры, и её центр смещён вверх —
		// ужимаем до 22 см и компенсируем смещение
		WeaponMesh = PistolAsset;
		HandLoc = FVector(-3.f, 2.f, -9.f);
		CamLoc = FVector(28.f, 12.f, -19.f);
		Scale = FVector(0.21f);
		break;

	case ERSMeshKind::Sniper:
		WeaponMesh = SniperAsset;
		HandLoc = FVector(-10.f, 4.f, -2.f);
		break;

	case ERSMeshKind::RifleM4:
		WeaponMesh = M4Asset;
		HandLoc = FVector(-8.f, 3.f, -2.f);
		CamLoc = FVector(34.f, 16.f, -18.f);
		CamRot = FRotator(0.f, -90.f, 0.f);
		break;

	case ERSMeshKind::Grenade:
		WeaponMesh = GrenadeAsset;
		HandLoc = FVector(-4.f, 2.f, 0.f);
		CamLoc = FVector(30.f, 14.f, -16.f);
		Scale = FVector(0.16f);
		break;

	default: // RifleAK
		break;
	}

	// Настоящая модель CS2 вместо заглушки. Модели скачаны у разных авторов,
	// поэтому размер и разворот считаем по габаритам меша, а не подбираем руками:
	// длинная ось направляется вперёд, а длина приводится к реальной.
	{
		// общий загрузчик с ботами: у него свой кеш и рабочие пути для AK-47,
		// USP-S и пистолетов, чьи модели лежат не в /Game/Weapons/CS2
		if (UStaticMesh* Real = RSWeapons::LoadWeaponMesh(CurrentWeapon))
		{
			WeaponMesh = Real;

			const FVector Extent = Real->GetBounds().BoxExtent;
			const float Longest = FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 2.f;
			const float Fit = (Longest > 1.f)
				? GetWeaponRealLength(CurrentWeapon) / Longest : 1.f;
			Scale = FVector(Fit);
			// вьюмодель в CS заметно мельче мирового размера, иначе ствол
			// занимает пол-экрана
			FitFP = Fit * 0.55f;

			// доворот: ствол должен смотреть по оси Y меша (как у заглушек).
			// Крен -90 переводит +Z в +Y; при +90 ствол уезжает в -Y, то есть
			// назад, и оружие выглядит развёрнутым на 180 градусов.
			FRotator Align = FRotator::ZeroRotator;
			if (Extent.Z >= Extent.X && Extent.Z >= Extent.Y)
			{
				Align = FRotator(0.f, 0.f, -90.f); // модель вытянута вверх
			}
			else if (Extent.X > Extent.Y)
			{
				Align = FRotator(0.f, 90.f, 0.f); // вытянута вдоль X
			}
			CamRot = (FQuat(CamRot) * FQuat(Align)).Rotator();
			HandRot = (FQuat(HandRot) * FQuat(Align)).Rotator();
			TPGunAlign = FQuat(Align);

			// центр модели редко совпадает с началом координат — компенсируем,
			// иначе ствол висит в стороне от руки
			MeshPivot = Real->GetBounds().Origin * Fit;
			CamLoc -= FQuat(CamRot).RotateVector(Real->GetBounds().Origin * FitFP);

			// после центровки половина ствола уходит за камеру — выдвигаем вперёд
			// и опускаем вправо-вниз, как держат оружие в CS
			CamLoc.X += GetWeaponRealLength(CurrentWeapon) * 0.55f * 0.35f;
			CamLoc.Y += 4.f;
			CamLoc.Z -= 4.f;
		}
	}

	TPGunMesh->SetStaticMesh(WeaponMesh);
	TPGunMesh->SetRelativeLocation(HandLoc);
	TPGunMesh->SetRelativeRotation(HandRot);
	TPGunMesh->SetRelativeScale3D(Scale);
	TPGunBaseLoc = HandLoc;
	TPGunPivot = MeshPivot;

	// Руки от первого лица отключены: их посадку нельзя подобрать расчётом,
	// нужна ручная подгонка в редакторе. Оружие держим у камеры.
	const bool bUseArms = false;

	GunMesh->SetStaticMesh(WeaponMesh);
	if (bUseArms)
	{
		GunMesh->AttachToComponent(ArmsMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale, TEXT("GripPoint"));
		GunMesh->SetRelativeLocation(FVector::ZeroVector);
		GunMesh->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	}
	else
	{
		GunMesh->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		GunMesh->SetRelativeLocation(CamLoc);
		GunMesh->SetRelativeRotation(CamRot);
	}
	GunMesh->SetRelativeScale3D(FitFP > 0.f ? FVector(FitFP) : Scale);

	// база для процедурной анимации вьюмодели + анимация доставания
	GunBaseLoc = CamLoc;
	GunBaseRot = CamRot;
	if (IsLocallyControlled() && GetWorld())
	{
		// быстрая смена: анимация доставания пропускается целиком
		DrawStartTime = GetWorld()->GetTimeSeconds() - (bQuickSwitch ? 10.f : 0.f);
	}

	// Если у оружия есть скелетная модель с анимациями CS2, показываем её
	// вместо статик-меша: она умеет доставание, стрельбу и перезарядку.
	const FRSViewModel* VM = RSViewModel::Get(CurrentWeapon);
	bUsingSkeletalVM = (VM != nullptr) && !bThirdPerson;

	if (bUsingSkeletalVM)
	{
		FPGun->SetSkeletalMesh(VM->Mesh);

		// Посадка берётся из таблицы, а не считается по габаритам. Автоподбор
		// здесь не работает: у скелета AK-47 размеры в исходнике меньше
		// сантиметра, и вычисленный масштаб раздувал модель на пол-экрана.
		FRSVMPlace Place = RSViewModel::GetPlace(CurrentWeapon);

		// Нулевой масштаб = «посчитать по мешу»: приводим длинную ось меша
		// к настоящей длине ствола. Это и есть натуральный размер, с него
		// разумно начинать подбор.
		if (Place.Scale <= 0.f)
		{
			const FVector Ext = VM->Mesh->GetBounds().BoxExtent;
			const float Longest = FMath::Max3(Ext.X, Ext.Y, Ext.Z) * 2.f;
			Place.Scale = (Longest > KINDA_SMALL_NUMBER)
				? RSWeapons::RealLength(CurrentWeapon) / Longest : 1.f;
		}
		FPGun->SetRelativeScale3D(FVector(Place.Scale));

		// В режиме подгонки печатаем реальные габариты меша и масштаб, при
		// котором ствол получил бы натуральную длину. Без этих цифр подбор
		// превращается в гадание: по картинке не отличить «слишком крупно»
		// от «слишком близко».
		if (bVMTune)
		{
			const FVector Ext = VM->Mesh->GetBounds().BoxExtent;
			const float Longest = FMath::Max3(Ext.X, Ext.Y, Ext.Z) * 2.f;
			const float Natural = (Longest > KINDA_SMALL_NUMBER)
				? RSWeapons::RealLength(CurrentWeapon) / Longest : 0.f;
			UE_LOG(LogTemp, Display,
				TEXT("RS/вьюмодель %d: габариты %.3f x %.3f x %.3f, длина %.3f, "
					 "натуральный масштаб %.2f, сейчас %.2f"),
				(int32)CurrentWeapon, Ext.X * 2.f, Ext.Y * 2.f, Ext.Z * 2.f,
				Longest, Natural, Place.Scale);
			CheatLog(FString::Printf(TEXT("габарит %.2f  натуральный масштаб %.1f"),
				Longest, Natural), FLinearColor(0.4f, 0.9f, 1.f));
		}

		GunBaseRot = Place.Rot;
		GunBaseLoc = Place.Loc;
		FPGun->SetRelativeLocation(GunBaseLoc);
		FPGun->SetRelativeRotation(GunBaseRot);

		// доставание, затем простой — как в CS при смене оружия
		if (VM->Draw)
		{
			PlayVMAnim(VM->Draw, false, VM->Draw->GetPlayLength() * 0.9f);
		}
		else if (VM->Idle)
		{
			PlayVMAnim(VM->Idle, true, 0.f);
		}
	}

	ArmsMesh->SetVisibility(bUseArms && !bThirdPerson);
	GunMesh->SetVisibility(!bThirdPerson && !bUsingSkeletalVM && !RSOptions::GetHideViewmodel());
	FPGun->SetVisibility(bUsingSkeletalVM && !RSOptions::GetHideViewmodel());
	TPGunMesh->SetOwnerNoSee(!bThirdPerson);
}

void ARSCharacter::PlayVMAnim(UAnimSequence* Anim, bool bLoop, float LockSeconds, float PlayRate)
{
	if (!FPGun || !Anim)
	{
		return;
	}
	FPGun->PlayAnimation(Anim, bLoop);
	FPGun->SetPlayRate(PlayRate);
	VMAnimLockUntil = GetWorld()->GetTimeSeconds() + LockSeconds;
}

void ARSCharacter::UpdateSkeletalViewModel()
{
	if (!bUsingSkeletalVM || GetWorld()->GetTimeSeconds() < VMAnimLockUntil)
	{
		return; // доигрывает выстрел, перезарядка или доставание
	}
	// закончилось — возвращаемся в простой
	if (const FRSViewModel* VM = RSViewModel::Get(CurrentWeapon))
	{
		if (VM->Idle && FPGun->GetSingleNodeInstance()
			&& FPGun->GetSingleNodeInstance()->GetAnimationAsset() != VM->Idle)
		{
			PlayVMAnim(VM->Idle, true, 0.f);
		}
	}
}

void ARSCharacter::ToggleVMTune()
{
	bVMTune = !bVMTune;
	CheatLog(bVMTune
		? FString::Printf(TEXT("подгонка вьюмодели вкл — %s"),
			*RSViewModel::PlaceToString(CurrentWeapon))
		: FString(TEXT("подгонка вьюмодели выкл")),
		FLinearColor(0.4f, 0.9f, 1.f));
}

void ARSCharacter::CycleVMStep()
{
	if (!bVMTune)
	{
		return;
	}
	// 1 / 5 / 20 см: грубо вытащить модель из лица и потом довести
	VMStep = (VMStep >= 20.f) ? 1.f : (VMStep >= 5.f ? 20.f : 5.f);
	CheatLog(FString::Printf(TEXT("шаг %.0f см"), VMStep), FLinearColor(0.4f, 0.9f, 1.f));
}

void ARSCharacter::ResetVMPlace()
{
	if (!bVMTune)
	{
		return;
	}
	// Стираем подобранное и возвращаемся к натуральной длине, посчитанной
	// по габаритам меша — той же формуле, что верно сажает статик-меши.
	const FRSViewModel* VM = RSViewModel::Get(CurrentWeapon);
	if (!VM || !VM->Mesh)
	{
		return;
	}
	const FVector Ext = VM->Mesh->GetBounds().BoxExtent;
	const float Longest = FMath::Max3(Ext.X, Ext.Y, Ext.Z) * 2.f;

	FRSVMPlace P;
	// натуральная длина ствола, без коэффициента 0.55 от статик-мешей
	P.Scale = (Longest > KINDA_SMALL_NUMBER)
		? RSWeapons::RealLength(CurrentWeapon) / Longest : 1.f;
	P.Loc = FVector(RSWeapons::RealLength(CurrentWeapon) * 0.3f, 9.f, -9.f);
	P.Rot = FRotator(0.f, -90.f, 0.f);
	RSViewModel::SetPlace(CurrentWeapon, P);

	ApplyWeaponVisuals();
	CheatLog(FString::Printf(TEXT("сброс: %s"), *RSViewModel::PlaceToString(CurrentWeapon)),
		FLinearColor(1.f, 0.8f, 0.3f));
}

void ARSCharacter::NudgeVM(const FVector& DLoc, const FRotator& DRot, float DScale)
{
	if (!bVMTune)
	{
		return; // вне режима подгонки нампад занят обычной игрой
	}
	if (!RSViewModel::Get(CurrentWeapon))
	{
		CheatLog(TEXT("у этого ствола нет скелетной вьюмодели"), FLinearColor(1.f, 0.6f, 0.3f));
		return;
	}

	FRSVMPlace P = RSViewModel::GetPlace(CurrentWeapon);
	P.Loc += DLoc;
	P.Rot += DRot;
	// Шаг масштаба умножающий, а не прибавляющий: диапазон подбора тут
	// в сотни раз, и линейным шагом его не пройти. 15% за нажатие —
	// удвоение примерно за пять нажатий.
	P.Scale = FMath::Clamp(P.Scale * (DScale > 0.f ? 1.15f : (DScale < 0.f ? 1.f / 1.15f : 1.f)),
		0.001f, 1000.f);
	RSViewModel::SetPlace(CurrentWeapon, P);

	ApplyWeaponVisuals();
	CheatLog(RSViewModel::PlaceToString(CurrentWeapon), FLinearColor(0.4f, 0.9f, 1.f));
	UE_LOG(LogTemp, Display, TEXT("RS/вьюмодель %d: %s"),
		(int32)CurrentWeapon, *RSViewModel::PlaceToString(CurrentWeapon));
}

void ARSCharacter::SetThirdPerson(bool bOn)
{
	bThirdPerson = bOn;
	ApplyViewMode();
	ApplyWeaponVisuals();
}

void ARSCharacter::ToggleView()
{
	bThirdPerson = !bThirdPerson;
	ApplyViewMode();
	ApplyWeaponVisuals();
}

void ARSCharacter::ApplyViewMode()
{
	if (bThirdPerson)
	{
		Camera->AttachToComponent(SpringArm, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Camera->bUsePawnControlRotation = false;
		GetMesh()->SetOwnerNoSee(false);
	}
	else
	{
		Camera->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Camera->SetRelativeLocation(FVector(0.f, 0.f, 64.f));
		Camera->SetRelativeRotation(FRotator::ZeroRotator);
		Camera->bUsePawnControlRotation = true;
		GetMesh()->SetOwnerNoSee(true);
	}
}

void ARSCharacter::SyncCheats()
{
	if (!HasAuthority())
	{
		ServerSyncCheats(bGodMode, bSpeedhack, bInfiniteMoney, bAntiAim,
			AntiAimMode, AntiAimSwing, AntiAimPitch, AntiAimSpin);
	}
}

void ARSCharacter::ServerSyncCheats_Implementation(bool bInGod, bool bInSpeed,
	bool bInMoney, bool bInAntiAim, int32 InMode, float InSwing, float InPitch, float InSpin)
{
	// сетевому клиенту бессмертия не выдаём, как бы он ни просил
	bGodMode = bInGod && GetNetMode() == NM_Standalone;
	bSpeedhack = bInSpeed;
	// сетевому клиенту деньги не выдаём, как бы он ни просил
	bInfiniteMoney = bInMoney && GetNetMode() == NM_Standalone;
	// разворот тела считает сервер, поэтому настройки анти-аима нужны ему тоже
	bAntiAim = bInAntiAim;
	AntiAimMode = InMode;
	AntiAimSwing = InSwing;
	AntiAimPitch = InPitch;
	AntiAimSpin = InSpin;
}

FVector ARSCharacter::GetVisibleAimPoint() const
{
	// Целятся не в капсулу, а в то, что видно, — в грудь модели. Пока
	// анти-аим выключен, подменённый разворот нулевой и точка совпадает
	// с прежней, поэтому на обычную игру это не влияет.
	const FVector Chest = GetActorLocation() + FVector(0.f, 0.f, 30.f);
	if (!bAntiAim)
	{
		return Chest;
	}
	const FVector FakeDir = FRotator(0.f, AntiAimYaw, 0.f).Vector();
	return Chest + FakeDir * 30.f;
}

bool ARSCharacter::IsReachableTo(const AActor* Target) const
{
	// Достижима ли цель выстрелом. Отличается от видимости: пуля пробивает
	// тонкие преграды, и цель за такой стеной аимботу годится, хотя прямой
	// видимости нет. Считаем тем же проходом, что и настоящий выстрел.
	if (!Target || !Camera)
	{
		return false;
	}
	const FVector From = Camera->GetComponentLocation();
	const FVector To = Target->GetActorLocation() + FVector(0.f, 0.f, 55.f);
	const FRSBulletPath Path = TraceBullet(From, (To - From).GetSafeNormal(), CurrentWeapon);
	return Path.bHit && Path.Hit.GetActor() == Target;
}

bool ARSCharacter::IsVisibleTo(const AActor* Target) const
{
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;
	const FVector From = Camera->GetComponentLocation();
	const FVector To = Target->GetActorLocation() + FVector(0.f, 0.f, 55.f);
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);
	return !bBlocked || Hit.GetActor() == Target;
}

FVector ARSCharacter::PredictPoint(const AActor* Target, bool bTargetHidden) const
{
	// целимся в выбранный хитбокс, а не всегда в голову
	const FVector Head = Target->GetActorLocation() + FVector(0.f, 0.f, HitboxHeight(AimHitbox));

	// По умолчанию упреждение — только для тех, кого сейчас не видно: по
	// видимой цели обычная наводка точнее, а упреждение уводило бы прицел
	// вперёд и заставляло мазать.
	if (!bPredict || (bPredictOnlyHidden && !bTargetHidden))
	{
		return Head;
	}

	const FVector Velocity = Target->GetVelocity();
	if (Velocity.Size2D() < 50.f)
	{
		return Head; // стоит на месте — упреждать нечего
	}

	const float Lead = PredictTicks * PredictTickSeconds;
	const FVector Ahead = Head + Velocity * Lead;

	// Упреждённую точку обрезаем по стене: без этого прицел уезжал сквозь
	// угол в соседнюю комнату, куда цель физически не выйдет.
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.AddIgnoredActor(Target);
	Params.bTraceComplex = true;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Head, Ahead, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint - Velocity.GetSafeNormal() * 20.f;
	}
	return Ahead;
}

bool ARSCharacter::IsFrozen() const
{
	// стоим только в закупку; после раунда можно двигаться
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	return State && State->Phase == ERSPhase::Intermission;
}

void ARSCharacter::MoveForward(float Value)
{
	if (IsFrozen())
	{
		return;
	}
	if (Value != 0.f)
	{
		AddMovementInput(FRotator(0.f, GetControlRotation().Yaw, 0.f).Vector(), Value);
	}
}

void ARSCharacter::MoveRight(float Value)
{
	if (IsFrozen())
	{
		return;
	}
	if (Value != 0.f)
	{
		const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
		AddMovementInput(FRotationMatrix(YawRot).GetScaledAxis(EAxis::Y), Value);
	}
}

void ARSCharacter::Turn(float Value)
{
	const ARSPlayerController* PC = Cast<ARSPlayerController>(GetController());
	AddControllerYawInput(Value * (PC ? PC->MouseSens : 1.f));
}

void ARSCharacter::LookUp(float Value)
{
	const ARSPlayerController* PC = Cast<ARSPlayerController>(GetController());
	AddControllerPitchInput(Value * (PC ? PC->MouseSens : 1.f));
}

void ARSCharacter::StartJump()
{
	if (IsFrozen())
	{
		return;
	}

	// вторая клавиша переключения наблюдения, кроме ЛКМ
	if (!bAlive)
	{
		if (HasAuthority())
		{
			CycleSpectate();
		}
		else
		{
			ServerCycleSpectate();
		}
		return;
	}
	bJumpHeld = true;
	Jump();
}
void ARSCharacter::StopJump()  { bJumpHeld = false; StopJumping(); }

void ARSCharacter::StartCrouchInput()
{
	if (!IsFrozen())
	{
		Crouch();
	}
}
void ARSCharacter::StopCrouchInput()  { UnCrouch(); }
void ARSCharacter::StartWalk() { bWalking = true; }
void ARSCharacter::StopWalk()  { bWalking = false; }

void ARSCharacter::StartFire()
{
	// меню читов забирает мышь себе: клики обрабатывает Slate, стрелять нельзя
	if (bCheatMenuOpen)
	{
		return;
	}

	// в закупке ЛКМ покупает то, на что наведён курсор, а не стреляет
	if (bBuyMenuOpen)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			float MX = 0.f, MY = 0.f;
			if (ARSHUD* RSHud = Cast<ARSHUD>(PC->GetHUD()))
			{
				if (PC->GetMousePosition(MX, MY))
				{
					RSHud->HandleBuyClick(FVector2D(MX, MY), this);
				}
			}
		}
		return;
	}

	// погибший «стреляет» переключением наблюдения
	if (!bAlive)
	{
		if (HasAuthority())
		{
			CycleSpectate();
		}
		else
		{
			ServerCycleSpectate();
		}
		return;
	}
	bFireHeld = true;
	bShotSincePress = false;
}

void ARSCharacter::ServerCycleSpectate_Implementation()
{
	CycleSpectate();
}

void ARSCharacter::CycleSpectate()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	// собираем живых союзников
	TArray<AActor*> Allies;
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && *It != this && It->bAlive && It->Team == Team)
		{
			Allies.Add(*It);
		}
	}
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->Health > 0.f && It->Team == Team)
		{
			Allies.Add(*It);
		}
	}

	if (Allies.Num() == 0)
	{
		// союзников не осталось — камера остаётся на месте гибели
		SpectateTarget = nullptr;
		PC->SetViewTargetWithBlend(this, 0.2f);
		return;
	}

	const int32 Current = Allies.IndexOfByKey(SpectateTarget);
	SpectateTarget = Allies[(Current + 1) % Allies.Num()];
	PC->SetViewTargetWithBlend(SpectateTarget, 0.4f);
}

void ARSCharacter::StopSpectating()
{
	SpectateTarget = nullptr;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetViewTargetWithBlend(this, 0.2f);
	}
}
void ARSCharacter::StopFire()  { bFireHeld = false; }

void ARSCharacter::StartAim()
{
	bAiming = true;
	bAimingNow = true;
	const FRSWeaponDef& Def = RSWeapons::Get(CurrentWeapon);
	if (Def.bScope && !bThirdPerson)
	{
		Camera->SetFieldOfView(Def.ScopeFOV);
		if (Def.Mesh == ERSMeshKind::Sniper)
		{
			// в оптике оружие не видно, как в CS
			GunMesh->SetVisibility(false);
			FPGun->SetVisibility(false);
		}
	}
}

void ARSCharacter::StopAim()
{
	bAiming = false;
	bAimingNow = false;
	Camera->SetFieldOfView(RSOptions::GetFov());
	// показываем ровно то, что было до прицеливания: иначе рядом со скелетной
	// вьюмоделью всплывал ещё и статик-меш, и оружие двоилось
	GunMesh->SetVisibility(!bThirdPerson && !bUsingSkeletalVM && !RSOptions::GetHideViewmodel());
	FPGun->SetVisibility(bUsingSkeletalVM && !bThirdPerson && !RSOptions::GetHideViewmodel());
}

void ARSCharacter::Reload()
{
	const FRSWeaponDef& Def = RSWeapons::Get(CurrentWeapon);
	if (bReloading || Def.Mag == 0 || Ammo[(uint8)CurrentWeapon] == Def.Mag
		|| Reserve[(uint8)CurrentWeapon] <= 0)
	{
		return;
	}
	bReloading = true;
	ReloadDuration = Def.ReloadTime;
	ReloadEndTime = GetWorld()->GetTimeSeconds() + Def.ReloadTime;
	PlayArmsAnim(AnimReload, false, Def.ReloadTime);

	RSAudio::PlayAt(this, RSAudio::GetReloadSound(CurrentWeapon),
		GetActorLocation(), 0.9f, RSAudio::ERange::Step);

	// анимация перезарядки растягивается ровно на время перезарядки оружия
	if (const FRSViewModel* VM = RSViewModel::Get(CurrentWeapon))
	{
		if (VM->Reload)
		{
			const float Rate = FMath::Clamp(
				VM->Reload->GetPlayLength() / FMath::Max(0.2f, Def.ReloadTime), 0.25f, 4.f);
			PlayVMAnim(VM->Reload, false, Def.ReloadTime, Rate);
		}
	}

	GetWorldTimerManager().SetTimer(ReloadTimer, this, &ARSCharacter::FinishReload, Def.ReloadTime, false);
}

void ARSCharacter::FinishReload()
{
	const uint8 W = (uint8)CurrentWeapon;
	const int32 Need = RSWeapons::Get(CurrentWeapon).Mag - Ammo[W];
	const int32 Take = FMath::Min(Need, Reserve[W]);
	Ammo[W] += Take;
	Reserve[W] -= Take;
	bReloading = false;
}

void ARSCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// во время закупки все стоят на месте, как в CS
	const bool bFrozen = IsFrozen();
	if (bFrozen)
	{
		GetCharacterMovement()->StopMovementImmediately();
	}

	// параметры движения меняем только там, где им управляют:
	// на чужих копиях это сбивает сетевое сглаживание и даёт рывки
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (GetLocalRole() != ROLE_SimulatedProxy)
	{
		// скорость как в CS: зависит от оружия, Shift — тихая ходьба (52%),
		// попадания замедляют (tagging); спидхак перекрывает всё
		float Speed = GetWeaponMaxSpeed();
		if (bWalking)
		{
			Speed *= 0.52f;
		}
		if (GetWorld()->GetTimeSeconds() < TaggedUntil)
		{
			Speed *= 0.5f;
		}
		// раньше спидхак был настолько резким, что игрока выносило за карту
		Move->MaxWalkSpeed = bFrozen ? 0.f : (bSpeedhack ? 1100.f : Speed);
		Move->MaxWalkSpeedCrouched = Speed * 0.34f;
		Move->JumpZVelocity = bSpeedhack ? 520.f : 450.f;
		Move->AirControl = bSpeedhack ? 0.8f : 0.35f;
	}

	// Оружие в руке: нужный мировой поворот пересчитываем в систему координат
	// кости hand_r и ставим как относительный — мировой анимация затирает,
	// а оси кости у скелета Manny не совпадают с осями мешей оружия.
	if (TPGunMesh && TPGunMesh->GetStaticMesh())
	{
		const FRotator Offset = (RSWeapons::Get(CurrentWeapon).Mesh == ERSMeshKind::Knife)
			? FRotator(-50.f, 0.f, 0.f)    // клинок вытянут по оси Z
			: FRotator(-5.f, -90.f, 0.f);  // стволы вытянуты по оси Y
		// доворот модели обязателен и здесь: без него пересчёт под кость
		// сбрасывает подгонку по габаритам, и AWP встаёт стоймя
		const FQuat DesiredWorld = (GetActorRotation() + Offset).Quaternion() * TPGunAlign;
		const FQuat BoneQuat = GetMesh()->GetSocketQuaternion(TEXT("hand_r"));
		const FQuat RelQuat = BoneQuat.Inverse() * DesiredWorld;
		TPGunMesh->SetRelativeRotation(RelQuat);

		// У скачанных моделей центр далеко от пивота, поэтому к руке приводим
		// габаритный центр. Но в кисти должна быть рукоять, а не середина
		// ствола — иначе оружие висит у бедра. Сдвигаем назад по направлению
		// взгляда: направление модели у всех авторов своё, а мировая ось
		// одна и та же. Пересчёт в систему кости — потому что положение
		// компонента задаётся относительно неё.
		FVector Rel = TPGunBaseLoc - RelQuat.RotateVector(TPGunPivot);
		const float Back = RSWeapons::GripOffsetBack(CurrentWeapon);
		if (Back > 0.f)
		{
			Rel -= BoneQuat.UnrotateVector(GetActorForwardVector() * Back);
		}
		TPGunMesh->SetRelativeLocation(Rel);
	}

	// Анти-аим: сервер крутит подменённый разворот, клиенты получают его
	// репликацией. Прицел и стрельба идут по настоящему направлению взгляда —
	// расходится только то, что видно противнику.
	if (HasAuthority())
	{
		if (bAntiAim)
		{
			const float Time = GetWorld()->GetTimeSeconds();
			float Offset = 180.f; // «спиной» — база всех режимов
			switch (AntiAimMode)
			{
			case 1: // спин: тело крутится непрерывно с заданной скоростью
				Offset = FMath::Fmod(Time * AntiAimSpin, 360.f);
				break;
			case 2: // дрожь: рывками влево-вправо, без плавности
				Offset += (FMath::Fmod(Time, 0.2f) < 0.1f ? AntiAimSwing : -AntiAimSwing);
				break;
			default: // спиной с мягким качанием
				Offset += FMath::Sin(Time * 6.f) * AntiAimSwing;
				break;
			}
			AntiAimYaw = FMath::UnwindDegrees(GetActorRotation().Yaw + Offset);
		}
		else
		{
			AntiAimYaw = 0.f;
		}
		AntiAimPitchRep = bAntiAim ? AntiAimPitch : 0.f;
	}
	if (GetMesh())
	{
		// Базовый разворот меша -90 задан в конструкторе и должен применяться
		// последним, в модельных осях; подмена — в осях актёра, поверх неё.
		const float FakeYaw = bAntiAim
			? FMath::UnwindDegrees(AntiAimYaw - GetActorRotation().Yaw) : 0.f;
		const FQuat Fake = FRotator(AntiAimPitchRep, FakeYaw, 0.f).Quaternion();
		const FQuat Base = FRotator(0.f, -90.f, 0.f).Quaternion();
		GetMesh()->SetRelativeRotation(Fake * Base);
	}

	// Бесконечные деньги и бессмертие — только в одиночной игре: в сетевой
	// они портят матч остальным, поэтому там просто выключаются.
	if (GetNetMode() != NM_Standalone)
	{
		if (bInfiniteMoney)
		{
			bInfiniteMoney = false;
			CheatLog(TEXT("бесконечные деньги недоступны в сетевой игре"),
				FLinearColor(1.f, 0.5f, 0.3f), TEXT("moneynet"));
		}
		if (bGodMode)
		{
			bGodMode = false;
			CheatLog(TEXT("бессмертие недоступно в сетевой игре"),
				FLinearColor(1.f, 0.5f, 0.3f), TEXT("godnet"));
		}
	}

	// кошелёк не пустеет: покупки списывают деньги как обычно, а сервер
	// тут же возвращает счёт к максимуму
	if (HasAuthority() && bInfiniteMoney)
	{
		Money = 16000;
	}

	// упал за пределы карты — возвращаем, а не убиваем
	if (HasAuthority() && bAlive && GetActorLocation().Z < ARSArena::GetMapFloor(GetWorld()) - 1000.f)
	{
		SetActorLocation(ARSArena::FindSpawnPoint(GetWorld(), Team));
		Move->StopMovementImmediately();
	}

	UpdateFootsteps(DeltaTime);

	if (!IsLocallyControlled())
	{
		return;
	}

	// музыка закупки: только у своего игрока и только в фазу покупок
	{
		const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
		const bool bWantMusic = State && State->Phase == ERSPhase::Intermission;
		if (bWantMusic && !BuyMusic)
		{
			BuyMusic = UGameplayStatics::SpawnSound2D(this,
				RSAudio::Get(RSAudio::ESound::MusicBuy), 0.5f * RSOptions::GetMusicVolume(), 1.f, 0.f, nullptr, false, false);
		}
		else if (!bWantMusic && BuyMusic)
		{
			BuyMusic->Stop();
			BuyMusic->DestroyComponent();
			BuyMusic = nullptr;
		}
	}

	// auto-bhop: пока держим пробел, прыгаем сразу как коснулись земли
	if (bJumpHeld && bSpeedhack && !Move->IsFalling())
	{
		Jump();
	}

	UpdateArmsAnimation();
	UpdateRecoil(DeltaTime);
	UpdateViewmodel(DeltaTime);

	// разброс как в CS: первый выстрел стоя точный, движение и прыжок
	// сильно мажут, присед помогает, снайперка без прицела не попадает
	SpreadBloom = FMath::Max(0.f, SpreadBloom - 3.5f * DeltaTime);
	const FRSWeaponDef& Def = RSWeapons::Get(CurrentWeapon);
	float Base = Def.BaseSpread;
	if (Def.bScope && Def.Mesh == ERSMeshKind::Sniper && !bAiming)
	{
		Base = 6.f;
	}
	const float SpeedFrac = GetVelocity().Size2D() / GetWeaponMaxSpeed();
	float MovePenalty = (SpeedFrac > 0.34f) ? SpeedFrac * 2.5f : 0.f;
	if (Move->IsFalling())
	{
		MovePenalty += 5.f; // стрельба в прыжке — лотерея
	}
	float Spread = Base + MovePenalty + SpreadBloom;
	if (bIsCrouched)
	{
		Spread *= 0.7f;
	}
	CurrentSpreadDeg = bNoRecoilSpread ? 0.f : Spread;

	// бинд: пока клавиша зажата, действует второй порог урона
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		const FKey Key = BindKey();
		bBindHeld = Key.IsValid() && PC->IsInputKeyDown(Key);
	}

	RecordEnemyHistory();
	UpdateChams();
	UpdateAutoBuy();

	// Обзор применяем каждый кадр, пока не смотрим в прицел: раньше он
	// ставился только при выходе из прицеливания, и ползунок в меню
	// не менял ничего, пока не прицелишься и не отпустишь.
	if (IsLocallyControlled() && Camera && !bAimingNow)
	{
		const float Want = RSOptions::GetFov();
		if (!FMath::IsNearlyEqual(Camera->FieldOfView, Want, 0.1f))
		{
			Camera->SetFieldOfView(Want);
		}
	}

	// Автоперезарядка: запускаем после выстрела, а не внутри него — иначе
	// перезарядка началась бы до того, как выстрел успел отработать.
	if (bAutoReloadPending)
	{
		bAutoReloadPending = false;
		Reload();
	}

	// заряд двойного выстрела копится, пока не стреляешь
	DoubleTapCharge = bDoubleTap ? (DoubleTapCharge + DeltaTime) : 0.f;

	// Автострейф: в воздухе доворот мыши сам добавляет боковое движение —
	// то, что в CS делают руками, чтобы разгоняться прыжками.
	if (bAirStrafe && GetCharacterMovement()->IsFalling())
	{
		const float Yaw = GetControlRotation().Yaw;
		const float Delta = FMath::UnwindDegrees(Yaw - LastAirYaw);
		if (FMath::Abs(Delta) > 0.02f)
		{
			AddMovementInput(GetActorRightVector(), FMath::Sign(Delta));
		}
		LastAirYaw = Yaw;
	}
	else
	{
		LastAirYaw = GetControlRotation().Yaw;
	}

	if (bAimbot)
	{
		RunAimbot(DeltaTime);
	}
	if (bTriggerbot && !bFireHeld)
	{
		RunTriggerbot();
	}
	if (bFireHeld)
	{
		TryFire();
	}
}

bool ARSCharacter::IsEnemyActor(const AActor* Other) const
{
	if (const ARSBot* Bot = Cast<ARSBot>(Other))
	{
		return Bot->Health > 0.f && Bot->Team != Team;
	}
	if (const ARSCharacter* Player = Cast<ARSCharacter>(Other))
	{
		return Player != this && Player->bAlive && Player->Team != Team;
	}
	return false;
}

AActor* ARSCharacter::FindBestTarget(FVector& OutAimPoint) const
{
	UWorld* World = GetWorld();
	const FVector CamLoc = Camera->GetComponentLocation();
	const FVector Forward = Camera->GetForwardVector();

	AActor* Best = nullptr;
	float BestAngle = FMath::Clamp(RageFov, 0.f, 180.f); // конус поиска цели

	auto Consider = [&](AActor* Candidate)
	{
		if (!IsValid(Candidate) || !IsEnemyActor(Candidate))
		{
			return;
		}
		// Видимость считаем первой: от неё зависит, упреждать эту цель или
		// целиться в неё напрямую.
		const bool bVisible = IsVisibleTo(Candidate);
		const FVector Aim = PredictPoint(Candidate, !bVisible);
		const FVector Dir = (Aim - CamLoc).GetSafeNormal();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Forward, Dir)));
		if (Angle >= BestAngle)
		{
			return;
		}

		// Наводимся на тех, до кого долетит пуля: либо прямая видимость,
		// либо цель за простреливаемой стеной. Прицел, уезжающий в глухую
		// стену на недостижимую цель, только мешает — стрельбой по тем, кто
		// вот-вот выбежит, занимается триггербот с упреждением.
		if (bVisible || IsReachableTo(Candidate))
		{
			Best = Candidate;
			BestAngle = Angle;
			// бэктрек: если включён, наводимся туда, где цель была недавно
			OutAimPoint = BacktrackPoint(Candidate, Aim);
		}
	};

	for (TActorIterator<ARSBot> It(World); It; ++It)
	{
		Consider(*It);
	}
	for (TActorIterator<ARSCharacter> It(World); It; ++It)
	{
		Consider(*It);
	}
	return Best;
}

void ARSCharacter::RunAimbot(float DeltaTime)
{
	FVector AimPoint;
	AActor* Target = FindBestTarget(AimPoint);
	if (!Target || !GetController())
	{
		LastAimTarget = nullptr;
		TargetSeenAt = -1.f;
		return;
	}

	// Условие включения: рейдж работает всегда, легиту нужен либо огонь,
	// либо прицеливание — иначе прицел ездит сам по себе всё время.
	if (bLegitAim)
	{
		if ((AimActivation == 1 && !bFireHeld) || (AimActivation == 2 && !bAiming))
		{
			LastAimTarget = nullptr;
			TargetSeenAt = -1.f;
			return;
		}
	}

	// Время реакции считаем от появления конкретной цели: сменилась —
	// отсчёт начинается заново, как у живого игрока.
	const float Now = GetWorld()->GetTimeSeconds();
	if (LastAimTarget.Get() != Target)
	{
		LastAimTarget = Target;
		TargetSeenAt = Now;
	}
	if (bLegitAim && Now - TargetSeenAt < ReactionMs * 0.001f)
	{
		return;
	}

	const FVector CamLoc = Camera->GetComponentLocation();
	const FRotator Want = (AimPoint - CamLoc).Rotation();

	if (!bLegitAim)
	{
		GetController()->SetControlRotation(Want);
		return;
	}

	// Плавная подводка: чем выше сглаживание, тем медленнее прицел доходит
	// до цели. Скорость обратна ползунку, поэтому 100 — почти незаметное
	// подтягивание, а 0 — рывок как у рейджа.
	const float Speed = FMath::Lerp(35.f, 1.5f, FMath::Clamp(AimSmooth / 100.f, 0.f, 1.f));
	GetController()->SetControlRotation(
		FMath::RInterpTo(GetControlRotation(), Want, DeltaTime, Speed));
}

FKey ARSCharacter::BindKey() const
{
	switch (BindKeyIndex)
	{
	case 1: return EKeys::LeftAlt;
	case 2: return EKeys::LeftShift;
	case 3: return EKeys::C;
	case 4: return EKeys::X;
	case 5: return EKeys::ThumbMouseButton;
	case 6: return EKeys::ThumbMouseButton2;
	default: return EKeys::Invalid;
	}
}

FLinearColor ARSCharacter::EspPalette(int32 Index)
{
	switch (Index)
	{
	case 1:  return FLinearColor(0.1f, 1.f, 0.3f);
	case 2:  return FLinearColor(0.2f, 0.6f, 1.f);
	case 3:  return FLinearColor(1.f, 0.85f, 0.1f);
	case 4:  return FLinearColor(0.75f, 0.3f, 1.f);
	case 5:  return FLinearColor(1.f, 1.f, 1.f);
	default: return FLinearColor(1.f, 0.15f, 0.15f);
	}
}

void ARSCharacter::RecordEnemyHistory()
{
	if (!bBacktrack)
	{
		EnemyHistory.Reset();
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float Keep = BacktrackMs * 0.001f;

	auto Remember = [&](AActor* Candidate)
	{
		if (!IsValid(Candidate) || !IsEnemyActor(Candidate))
		{
			return;
		}
		TArray<FRSPastPos>& List = EnemyHistory.FindOrAdd(Candidate);
		List.Add({ Candidate->GetActorLocation(), Now });
		// выбрасываем всё, что старше окна бэктрека
		while (List.Num() > 0 && Now - List[0].Time > Keep)
		{
			List.RemoveAt(0);
		}
	};

	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		Remember(*It);
	}
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		Remember(*It);
	}

	// цели уходят из мира — чистим мёртвые ключи, иначе карта растёт вечно
	for (auto It = EnemyHistory.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

FVector ARSCharacter::BacktrackPoint(const AActor* Target, const FVector& Fallback) const
{
	if (!bBacktrack)
	{
		return Fallback;
	}
	const TArray<FRSPastPos>* List = EnemyHistory.Find(Target);
	if (!List || List->Num() == 0)
	{
		return Fallback;
	}

	// Берём самую старую запись в окне: именно она сильнее всего отличается
	// от текущей позиции, а значит бьёт «назад» по движению цели.
	const FVector Offset = Fallback - Target->GetActorLocation();
	return (*List)[0].Location + Offset;
}

float ARSCharacter::EstimateHitChance() const
{
	// Разброс — конус, поэтому шанс считаем честно: прогоняем пучок лучей
	// и смотрим, какая доля наносит нужный урон. Дешевле, чем кажется:
	// проверка идёт только когда чит уже собрался стрелять.
	const float Spread = CurrentSpreadDeg;
	if (Spread <= 0.01f)
	{
		return (DamageIfFiredNow() >= EffectiveMinDamage()) ? 100.f : 0.f;
	}

	const FVector Start = Camera->GetComponentLocation();
	const FVector Forward = Camera->GetForwardVector();
	const int32 Samples = 12;
	int32 Good = 0;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	for (int32 i = 0; i < Samples; i++)
	{
		const FVector Dir = FMath::VRandCone(Forward, FMath::DegreesToRadians(Spread));
		// тем же проходом, что и настоящая пуля: иначе оценка шанса не видит
		// пробития и занижает его до нуля за любой преградой
		const FRSBulletPath Path = TraceBullet(Start, Dir, CurrentWeapon);
		if (!Path.bHit)
		{
			continue;
		}
		const FHitResult& Hit = Path.Hit;
		bool bHead = false;
		if (DamageForHit(Hit, CurrentWeapon, Start, bHead) * Path.DamageScale
			>= EffectiveMinDamage())
		{
			Good++;
		}
	}
	return 100.f * Good / Samples;
}

namespace
{
	// Одна таблица полей на сохранение и на загрузку: если завести две,
	// они рано или поздно разойдутся, и настройка будет молча теряться.
	struct FBoolField { const TCHAR* Key; bool ARSCharacter::* Field; };
	struct FFloatField { const TCHAR* Key; float ARSCharacter::* Field; };
	struct FIntField { const TCHAR* Key; int32 ARSCharacter::* Field; };

	const FBoolField BoolFields[] = {
		{ TEXT("Aimbot"), &ARSCharacter::bAimbot },
		{ TEXT("Silent"), &ARSCharacter::bSilentAim },
		{ TEXT("Trigger"), &ARSCharacter::bTriggerbot },
		{ TEXT("NoRecoil"), &ARSCharacter::bNoRecoilSpread },
		{ TEXT("DoubleTap"), &ARSCharacter::bDoubleTap },
		{ TEXT("Predict"), &ARSCharacter::bPredict },
		{ TEXT("PredictOnlyHidden"), &ARSCharacter::bPredictOnlyHidden },
		{ TEXT("Backtrack"), &ARSCharacter::bBacktrack },
		{ TEXT("AntiAim"), &ARSCharacter::bAntiAim },
		{ TEXT("Speed"), &ARSCharacter::bSpeedhack },
		{ TEXT("God"), &ARSCharacter::bGodMode },
		{ TEXT("Money"), &ARSCharacter::bInfiniteMoney },
		{ TEXT("Esp"), &ARSCharacter::bESP },
		{ TEXT("EspBox"), &ARSCharacter::bEspBox },
		{ TEXT("EspFill"), &ARSCharacter::bEspFill },
		{ TEXT("EspSkeleton"), &ARSCharacter::bEspSkeleton },
		{ TEXT("EspHealth"), &ARSCharacter::bEspHealth },
		{ TEXT("EspDist"), &ARSCharacter::bEspDist },
		{ TEXT("EspLine"), &ARSCharacter::bEspLine },
		{ TEXT("EspMark"), &ARSCharacter::bEspMark },
		{ TEXT("CheatLogs"), &ARSCharacter::bCheatLogs },
		{ TEXT("CheatLogReasons"), &ARSCharacter::bCheatLogReasons },
		{ TEXT("QuickSwitch"), &ARSCharacter::bQuickSwitch },
		{ TEXT("HitSound"), &ARSCharacter::bHitSound },
		{ TEXT("AirStrafe"), &ARSCharacter::bAirStrafe },
		{ TEXT("LegitAim"), &ARSCharacter::bLegitAim },
		{ TEXT("Chams"), &ARSCharacter::bChams },
	};

	const FFloatField FloatFields[] = {
		{ TEXT("TriggerFov"), &ARSCharacter::TriggerFov },
		{ TEXT("RageFov"), &ARSCharacter::RageFov },
		{ TEXT("MinDamage"), &ARSCharacter::MinDamage },
		{ TEXT("MinDamageAlt"), &ARSCharacter::MinDamageAlt },
		{ TEXT("HitChance"), &ARSCharacter::HitChance },
		{ TEXT("BacktrackMs"), &ARSCharacter::BacktrackMs },
		{ TEXT("AntiAimSwing"), &ARSCharacter::AntiAimSwing },
		{ TEXT("AntiAimPitch"), &ARSCharacter::AntiAimPitch },
		{ TEXT("AntiAimSpin"), &ARSCharacter::AntiAimSpin },
		{ TEXT("AimSmooth"), &ARSCharacter::AimSmooth },
		{ TEXT("ReactionMs"), &ARSCharacter::ReactionMs },
		{ TEXT("RecoilControl"), &ARSCharacter::RecoilControl },
		{ TEXT("TriggerReactionMs"), &ARSCharacter::TriggerReactionMs },
	};

	const FIntField IntFields[] = {
		{ TEXT("AntiAimMode"), &ARSCharacter::AntiAimMode },
		{ TEXT("PredictTicks"), &ARSCharacter::PredictTicks },
		{ TEXT("AimHitbox"), &ARSCharacter::AimHitbox },
		{ TEXT("BindKey"), &ARSCharacter::BindKeyIndex },
		{ TEXT("EspColor"), &ARSCharacter::EspColor },
		{ TEXT("AimActivation"), &ARSCharacter::AimActivation },
	};

	const TCHAR* CheatSection = TEXT("Cheat");
}

FString ARSCharacter::CheatConfigPath(const FString& Name)
{
	// Абсолютный путь: относительный с шестью «../» до записи не доводил,
	// файл молча не появлялся.
	return FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("Cheats") / (Name + TEXT(".ini")));
}

void ARSCharacter::SaveCheatConfig(const FString& Name)
{
	if (Name.IsEmpty())
	{
		return;
	}
	const FString Path = CheatConfigPath(Name);

	// Папки Saved/Cheats может не быть, а запись ini её не создаёт.
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);

	// Пишем файл сами: GConfig на этом пути молча ничего не сохранял,
	// а тут видно и результат, и ошибку.
	FString Text = FString::Printf(TEXT("[%s]\n"), CheatSection);
	for (const FBoolField& F : BoolFields)
	{
		Text += FString::Printf(TEXT("%s=%s\n"), F.Key, this->*F.Field ? TEXT("True") : TEXT("False"));
	}
	for (const FFloatField& F : FloatFields)
	{
		Text += FString::Printf(TEXT("%s=%f\n"), F.Key, this->*F.Field);
	}
	for (const FIntField& F : IntFields)
	{
		Text += FString::Printf(TEXT("%s=%d\n"), F.Key, this->*F.Field);
	}
	FFileHelper::SaveStringToFile(Text, *Path);

	CurrentConfig = Name;
	const bool bWritten = FPaths::FileExists(Path);
	CheatLog(bWritten
		? FString::Printf(TEXT("конфиг сохранён: %s"), *Name)
		: FString::Printf(TEXT("не удалось записать конфиг: %s"), *Path),
		bWritten ? FLinearColor(0.4f, 0.8f, 1.f) : FLinearColor(1.f, 0.4f, 0.4f));
	UE_LOG(LogTemp, Log, TEXT("RS/чит: конфиг %s -> %s (%s)"), *Name, *Path,
		bWritten ? TEXT("записан") : TEXT("ОШИБКА"));
}

bool ARSCharacter::LoadCheatConfig(const FString& Name)
{
	const FString Path = CheatConfigPath(Name);
	if (!FPaths::FileExists(Path))
	{
		return false;
	}

	// читаем тем же способом, каким писали: файл разбирается построчно
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return false;
	}
	TMap<FString, FString> Values;
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		FString Key, Value;
		if (Line.Split(TEXT("="), &Key, &Value))
		{
			Values.Add(Key.TrimStartAndEnd(), Value.TrimStartAndEnd());
		}
	}

	for (const FBoolField& F : BoolFields)
	{
		if (const FString* V = Values.Find(F.Key)) { this->*F.Field = V->ToBool(); }
	}
	for (const FFloatField& F : FloatFields)
	{
		if (const FString* V = Values.Find(F.Key)) { this->*F.Field = FCString::Atof(**V); }
	}
	for (const FIntField& F : IntFields)
	{
		if (const FString* V = Values.Find(F.Key)) { this->*F.Field = FCString::Atoi(**V); }
	}

	CurrentConfig = Name;
	// сервер должен узнать про годмод, скорость, деньги и анти-аим
	SyncCheats();
	CheatLog(FString::Printf(TEXT("конфиг загружен: %s"), *Name), FLinearColor(0.4f, 0.8f, 1.f));
	return true;
}

TArray<FString> ARSCharacter::ListCheatConfigs()
{
	TArray<FString> Files;
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Cheats"));
	IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.ini")), true, false);
	for (FString& File : Files)
	{
		File = FPaths::GetBaseFilename(File);
	}
	return Files;
}

void ARSCharacter::UpdateAutoBuy()
{
	// Покупки разрешены только в фазу закупки, поэтому ждём её, а не
	// покупаем при возрождении: там фаза ещё прежняя, и сервер отклонял
	// каждый запрос. Раз в раунд — иначе набор докупался бы каждый кадр.
	if (!IsLocallyControlled() || !RSOptions::GetAutoBuy())
	{
		return;
	}
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State || !State->IsBuyTime() || State->RoundNumber == LastAutoBuyRound)
	{
		return;
	}
	LastAutoBuyRound = State->RoundNumber;

	// Порядок важен: сначала основное — оно дороже всего, потом пистолет,
	// потом броня. Иначе на ствол не осталось бы денег.
	const int32 Primary = RSOptions::GetLoadoutPrimary();
	if (Primary >= 0 && Money >= RSWeapons::Get((ERSWeapon)Primary).Price)
	{
		ServerBuyWeapon((ERSWeapon)Primary);
	}
	const int32 Secondary = RSOptions::GetLoadoutSecondary();
	if (Secondary >= 0 && Money >= RSWeapons::Get((ERSWeapon)Secondary).Price)
	{
		ServerBuyWeapon((ERSWeapon)Secondary);
	}
	if (RSOptions::GetLoadoutArmor() && Money >= 1000)
	{
		ServerBuyArmor(true);
	}
}

void ARSCharacter::UpdateChams()
{
	if (!IsLocallyControlled() || !Camera)
	{
		return;
	}

	// Помечаем врагов в CustomDepth: материал рисует поверх только их.
	// Своих не помечаем — сквозь стены нужны именно противники.
	auto Mark = [this](ACharacter* Who, bool bEnemy)
	{
		if (USkeletalMeshComponent* Mesh = Who ? Who->GetMesh() : nullptr)
		{
			Mesh->SetRenderCustomDepth(bChams && bEnemy);
			Mesh->SetCustomDepthStencilValue(1);
		}
	};
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		Mark(*It, IsEnemyActor(*It));
	}
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		Mark(*It, IsEnemyActor(*It));
	}

	if (!bChams)
	{
		if (ChamsMID)
		{
			Camera->PostProcessSettings.WeightedBlendables.Array.Empty();
			ChamsMID = nullptr;
		}
		return;
	}

	if (!ChamsMID)
	{
		if (UMaterialInterface* Base = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Game/UI/M_RSChams.M_RSChams")))
		{
			ChamsMID = UMaterialInstanceDynamic::Create(Base, this);
			Camera->PostProcessSettings.WeightedBlendables.Array.Empty();
			Camera->PostProcessSettings.WeightedBlendables.Array.Add(
				FWeightedBlendable(1.f, ChamsMID));
		}
	}
	if (ChamsMID)
	{
		ChamsMID->SetVectorParameterValue(TEXT("ChamsColor"), EspPalette(EspColor));
	}
}

void ARSCharacter::CheatLog(const FString& Text, const FLinearColor& Color, const FString& ThrottleKey)
{
	if (!bCheatLogs || !IsLocallyControlled())
	{
		return;
	}

	// строки с ключом — диагностика; без отдельного переключателя их не
	// показываем, иначе «predict error» идёт сплошным потоком
	if (!ThrottleKey.IsEmpty() && !bCheatLogReasons)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	// Причины отказа стрелять повторяются каждый кадр — душим их по ключу,
	// иначе лог превращается в сплошную стену одинаковых строк.
	if (!ThrottleKey.IsEmpty())
	{
		static TMap<FString, float> LastByKey;
		float& Last = LastByKey.FindOrAdd(ThrottleKey);
		if (Now - Last < 0.6f)
		{
			return;
		}
		Last = Now;
	}

	CheatLogLines.Add({ Text, Now, Color });
	while (CheatLogLines.Num() > 10)
	{
		CheatLogLines.RemoveAt(0);
	}
}

void ARSCharacter::TryFireIfWorthIt()
{
	// Порог урона: чит не жмёт курок, пока выстрел не наносит хотя бы
	// MinDamage. Это и отсекает стрельбу в стену — там урона ноль.
	const float Damage = DamageIfFiredNow();
	const float Threshold = EffectiveMinDamage();
	if (Damage < Threshold)
	{
		// Пустой прицел — не событие: под ним почти всегда стена, и писать
		// об этом каждый кадр бессмысленно. Пишем только реальную причину:
		// цель есть, но урона не хватает.
		if (Damage > 0.f)
		{
			CheatLog(FString::Printf(TEXT("нет выстрела: урон %.0f < %.0f (min damage)"),
				Damage, Threshold), FLinearColor(0.9f, 0.6f, 0.2f), TEXT("mindmg"));
		}
		return;
	}

	// Шанс попадания: с разбросом даже точная наводка не гарантирует урон,
	// поэтому ждём, пока конус не накроет цель достаточно плотно.
	if (HitChance > 0.f)
	{
		const float Chance = EstimateHitChance();
		if (Chance < HitChance)
		{
			CheatLog(FString::Printf(TEXT("нет выстрела: шанс %.0f%% < %.0f%% (hit chance error)"),
				Chance, HitChance), FLinearColor(0.9f, 0.6f, 0.2f), TEXT("hitchance"));
			return;
		}
	}

	// отдача уводит ствол от точки прицеливания — отдельная причина промаха
	if (PunchCurrent.SizeSquared() > 4.f)
	{
		CheatLog(FString::Printf(TEXT("выстрел с уводом отдачи %.1f° (aim punch)"),
			PunchCurrent.Size()), FLinearColor(0.75f, 0.75f, 0.8f), TEXT("punch"));
	}

	// триггербот жмёт «курок» сам: сбрасываем блокировку полуавтомата,
	// иначе пистолеты и AWP стреляли бы один раз за всё время
	bShotSincePress = false;
	TryFire();

}

void ARSCharacter::RunTriggerbot()
{
	UWorld* World = GetWorld();
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * 20000.f;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (IsEnemyActor(Hit.GetActor()))
		{
			// Время реакции: курок жмётся не в тот же кадр, когда цель
			// пересекла прицел. Отсчёт с момента, как она там появилась.
			const float Now = GetWorld()->GetTimeSeconds();
			if (TriggerSeenAt < 0.f)
			{
				TriggerSeenAt = Now;
			}
			if (Now - TriggerSeenAt >= TriggerReactionMs * 0.001f)
			{
				TryFireIfWorthIt();
			}
			return;
		}
	}

	// под прицелом никого — отсчёт реакции начнётся заново
	TriggerSeenAt = -1.f;

	// Дальше — стрельба по цели рядом с прицелом, а не строго под ним:
	// в пределах TriggerFov. При нуле остаётся только точный луч выше.
	if (TriggerFov <= 0.f)
	{
		return;
	}

	const FVector Forward = Camera->GetForwardVector();
	auto Consider = [&](AActor* Candidate)
	{
		if (!IsValid(Candidate) || !IsEnemyActor(Candidate))
		{
			return false;
		}

		const bool bVisible = IsVisibleTo(Candidate);
		if (bVisible)
		{
			// видимая цель: стреляем, если она попала в конус триггербота
			const FVector Head = Candidate->GetActorLocation() + FVector(0.f, 0.f, 55.f);
			const FVector Dir = (Head - Start).GetSafeNormal();
			const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Forward, Dir)));
			return Angle < TriggerFov;
		}

		// Префайр: по тому, кого не видно, стреляем только с упреждением —
		// туда, куда цель выбегает. Попадёт, когда она там окажется;
		// промахи по стене — цена приёма.
		if (!bPredict || Candidate->GetVelocity().Size2D() < 100.f)
		{
			return false;
		}
		const FVector Point = PredictPoint(Candidate, true);
		const FVector Dir = (Point - Start).GetSafeNormal();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Forward, Dir)));
		return Angle < TriggerFov;
	};

	for (TActorIterator<ARSBot> It(World); It; ++It)
	{
		if (Consider(*It)) { TryFireIfWorthIt(); return; }
	}
	for (TActorIterator<ARSCharacter> It(World); It; ++It)
	{
		if (Consider(*It)) { TryFireIfWorthIt(); return; }
	}
}

void ARSCharacter::TryFire(bool bIgnoreCadence)
{
	UWorld* World = GetWorld();
	const FRSWeaponDef& Def = RSWeapons::Get(CurrentWeapon);
	const float Now = World->GetTimeSeconds();
	if (IsFrozen())
	{
		return; // в закупку не стреляем
	}
	if (!bAlive || bReloading || (!bIgnoreCadence && Now - LastFireTime < Def.Interval))
	{
		return;
	}
	// полуавтомат: одно нажатие — один выстрел
	if (!Def.bAuto && bShotSincePress && !bIgnoreCadence)
	{
		return;
	}

	if (Def.Slot == ERSSlot::Grenade)
	{
		ThrowGrenade();
		return;
	}

	if (Def.Mag > 0)
	{
		if (Ammo[(uint8)CurrentWeapon] <= 0)
		{
			Reload();
			return;
		}
		Ammo[(uint8)CurrentWeapon]--;

		// Магазин опустел этим выстрелом — перезаряжаемся сразу, не дожидаясь
		// R и не заставляя жать в пустоту. Сам выстрел при этом происходит.
		bAutoReloadPending = (Ammo[(uint8)CurrentWeapon] == 0
			&& Reserve[(uint8)CurrentWeapon] > 0);
	}

	LastFireTime = Now;
	bShotSincePress = true;

	const FVector CamLoc = Camera->GetComponentLocation();
	FVector Dir = Camera->GetForwardVector();

	// silent aim: пуля летит в голову ближайшей видимой цели, куда бы ты ни смотрел
	if (bSilentAim && CurrentWeapon != ERSWeapon::Knife)
	{
		FVector AimPoint;
		if (FindBestTarget(AimPoint))
		{
			Dir = (AimPoint - CamLoc).GetSafeNormal();
		}
	}
	else if (CurrentSpreadDeg > 0.f && CurrentWeapon != ERSWeapon::Knife)
	{
		Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(CurrentSpreadDeg));
	}

	if (HasAuthority())
	{
		DoFireTrace(CamLoc, Dir, CurrentWeapon);
	}
	else
	{
		ServerFire(CamLoc, Dir, CurrentWeapon);
	}

	PlayArmsAnim(AnimFire, false, 0.2f);

	// у скелетной вьюмодели своя анимация выстрела, три варианта по кругу
	if (const FRSViewModel* VM = RSViewModel::Get(CurrentWeapon))
	{
		if (UAnimSequence* Shot = VM->Shoot[RecoilIndex % 3])
		{
			// длинную анимацию поджимаем под темп стрельбы
			const float Rate = FMath::Clamp(Shot->GetPlayLength() / FMath::Max(0.05f, Def.Interval),
				1.f, 8.f);
			PlayVMAnim(Shot, false, Def.Interval * 0.9f, Rate);
		}
	}

	// вьюмодель дёргается назад, осмотр прерывается
	GunKick = 1.f;
	InspectEndTime = -10.f;

	if (!bNoRecoilSpread && CurrentWeapon != ERSWeapon::Knife)
	{
		float KickPitch = 0.f, KickYaw = 0.f;
		GetRecoilKick(CurrentWeapon, RecoilIndex, KickPitch, KickYaw);

		// Контроль отдачи: гасим заданную долю подброса. В отличие от
		// «без отдачи» это не выключает её целиком — ствол всё равно ведёт,
		// просто слабее, и спрей остаётся отличимым от лазера.
		const float Keep = 1.f - FMath::Clamp(RecoilControl / 100.f, 0.f, 1.f);
		KickPitch *= Keep;
		KickYaw *= Keep;

		PunchTarget.X += KickPitch;
		PunchTarget.Y += KickYaw;
		RecoilIndex++;
		SpreadBloom = FMath::Min(SpreadBloom + 0.12f, 1.2f);
	}

	// Двойной выстрел. Заряд копится заранее; когда он полон — второй патрон
	// уходит в тот же момент, вне очереди. Не накопился — стреляем один раз
	// и обнуляем прогресс, ждать «дозарядки» посреди боя нельзя.
	if (bDoubleTap && !bIgnoreCadence && Def.Slot != ERSSlot::Grenade
		&& CurrentWeapon != ERSWeapon::Knife)
	{
		const float Need = Def.Interval * 2.f; // заряд стоит ровно два выстрела
		if (DoubleTapCharge < Need)
		{
			CheatLog(FString::Printf(TEXT("одиночный: заряд %.0f%%"),
				100.f * DoubleTapCharge / Need), FLinearColor(0.75f, 0.75f, 0.8f), TEXT("dtcharge"));
		}
		else
		{
			// Второй патрон имеет смысл только если кого-то убивает: добивает
			// того же (первый выстрел не свалил) или снимает второго врага.
			// Поэтому перед ним наводимся заново — на живую цель.
			FVector AimPoint;
			if (AActor* Next = FindBestTarget(AimPoint))
			{
				if (GetController())
				{
					GetController()->SetControlRotation(
						(AimPoint - Camera->GetComponentLocation()).Rotation());
				}
				if (WouldKillWithOneShot())
				{
					CheatLog(FString::Printf(TEXT("двойной выстрел → %s"), *RSCombatantName(Next)),
						FLinearColor(0.4f, 0.8f, 1.f), TEXT("dtfire"));
					TryFire(true);
				}
				else
				{
					CheatLog(TEXT("второй выстрел не добивает — придержал"),
						FLinearColor(0.75f, 0.75f, 0.8f), TEXT("dthold"));
				}
			}
		}
		DoubleTapCharge = 0.f;
	}
}

bool ARSCharacter::WouldKillWithOneShot() const
{
	// Смотрим, кто под прицелом и сколько у него осталось: если урон одного
	// выстрела перекрывает здоровье, второй патрон только тратит боезапас.
	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * 20000.f;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		return false;
	}

	bool bHead = false;
	const float Damage = DamageForHit(Hit, CurrentWeapon, Start, bHead);
	if (Damage <= 0.f)
	{
		return false;
	}

	float TargetHealth = 0.f;
	if (const ARSBot* Bot = Cast<ARSBot>(Hit.GetActor()))
	{
		TargetHealth = Bot->Health;
	}
	else if (const ARSCharacter* Player = Cast<ARSCharacter>(Hit.GetActor()))
	{
		TargetHealth = Player->Health;
	}
	return TargetHealth > 0.f && Damage >= TargetHealth;
}

void ARSCharacter::ThrowGrenade()
{
	const int32 GI = RSWeapons::GrenadeIndex(CurrentWeapon);
	if (GI < 0 || Grenades[GI] <= 0)
	{
		return;
	}

	LastFireTime = GetWorld()->GetTimeSeconds();
	GunKick = 1.f;

	const FVector CamLoc = Camera->GetComponentLocation();
	const FVector Dir = Camera->GetForwardVector();
	// ПКМ зажата — мягкий подброс под ноги
	ServerThrowGrenade(CamLoc, Dir, CurrentWeapon, bAiming);

	// последняя граната этого типа улетела — перехватываем нож
	if (Grenades[GI] <= 1)
	{
		RequestWeapon(ERSWeapon::Knife);
	}
}

void ARSCharacter::ServerThrowGrenade_Implementation(FVector Start, FVector_NetQuantizeNormal Dir,
	ERSWeapon Weapon, bool bLob)
{
	const int32 GI = RSWeapons::GrenadeIndex(Weapon);
	if (GI < 0 || Grenades[GI] <= 0 || !bAlive)
	{
		return;
	}
	Grenades[GI]--;

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.Instigator = this;
	const FVector SpawnLoc = Start + FVector(Dir) * 60.f;
	if (ARSGrenade* Nade = GetWorld()->SpawnActor<ARSGrenade>(
		ARSGrenade::StaticClass(), SpawnLoc, FRotator::ZeroRotator, SP))
	{
		const float Power = bLob ? 700.f : 1500.f;
		Nade->InitThrow(Weapon, this, Team, FVector(Dir) * Power + FVector(0.f, 0.f, 180.f));
	}
}

void ARSCharacter::GetRecoilKick(ERSWeapon W, int32 Index, float& OutPitch, float& OutYaw)
{
	// детерминированный паттерн, как спрей в CS: одинаковый каждый раз,
	// его можно выучить и компенсировать мышью
	const FRSWeaponDef& Def = RSWeapons::Get(W);
	if (!Def.bAuto || Def.YawAmp <= 0.f)
	{
		OutPitch = Def.KickBase + Def.KickRamp * FMath::Min(Index, 6);
		OutYaw = 0.f;
		return;
	}
	if (Index < 8)
	{
		OutPitch = Def.KickBase + Def.KickRamp * Index;
		OutYaw = 0.02f * Index;
	}
	else
	{
		OutPitch = Def.KickBase + Def.KickRamp * 7.5f;
		OutYaw = Def.YawAmp * FMath::Sin(Index * 0.7f + 1.1f);
	}
}

void ARSCharacter::UpdateRecoil(float DeltaTime)
{
	const float Now = GetWorld()->GetTimeSeconds();
	const float Interval = RSWeapons::Get(CurrentWeapon).Interval;

	// пауза в стрельбе сбрасывает индекс паттерна и возвращает камеру
	if (Now - LastFireTime > FMath::Max(0.35f, Interval * 2.f))
	{
		RecoilIndex = 0;
		PunchTarget = FMath::Vector2DInterpTo(PunchTarget, FVector2D::ZeroVector, DeltaTime, 7.f);
	}

	const FVector2D Prev = PunchCurrent;
	PunchCurrent = FMath::Vector2DInterpTo(PunchCurrent, PunchTarget, DeltaTime, 22.f);
	const FVector2D Delta = PunchCurrent - Prev;
	if (!Delta.IsNearlyZero())
	{
		AddControllerPitchInput(-Delta.X);
		AddControllerYawInput(Delta.Y);
	}
}

void ARSCharacter::UpdateViewmodel(float DeltaTime)
{
	if (!GunMesh || bThirdPerson)
	{
		return;
	}

	UpdateSkeletalViewModel();

	// покачивание и отдача применяются к тому, что сейчас на экране
	USceneComponent* Visual = bUsingSkeletalVM ? (USceneComponent*)FPGun : (USceneComponent*)GunMesh;

	const float Now = GetWorld()->GetTimeSeconds();
	UCharacterMovementComponent* Move = GetCharacterMovement();

	// приземление — короткий присед оружия
	const bool bFallingNow = Move->IsFalling();
	if (bWasFalling && !bFallingNow)
	{
		LandDipStart = Now;
	}
	bWasFalling = bFallingNow;

	// покачивание при ходьбе + дыхание на месте
	const float SpeedFrac = FMath::Clamp(GetVelocity().Size2D() / GetWeaponMaxSpeed(), 0.f, 1.2f);
	if (SpeedFrac > 0.05f && !bFallingNow)
	{
		BobTime += DeltaTime * (6.f + 8.f * SpeedFrac);
	}
	FVector Offset = FVector::ZeroVector;
	Offset.Y = FMath::Sin(BobTime) * 1.1f * SpeedFrac;
	Offset.Z = -FMath::Abs(FMath::Cos(BobTime)) * 1.0f * SpeedFrac
		+ FMath::Sin(Now * 1.6f) * 0.25f; // дыхание

	// инерция от мыши: ствол отстаёт от поворота камеры
	const FRotator ControlRot = GetControlRotation();
	const float YawDelta = FMath::FindDeltaAngleDegrees(PrevControlRot.Yaw, ControlRot.Yaw);
	const float PitchDelta = FMath::FindDeltaAngleDegrees(PrevControlRot.Pitch, ControlRot.Pitch);
	PrevControlRot = ControlRot;
	const FVector TargetSwayLoc(0.f, FMath::Clamp(-YawDelta * 0.35f, -3.f, 3.f),
		FMath::Clamp(PitchDelta * 0.25f, -2.f, 2.f));
	SwayLoc = FMath::VInterpTo(SwayLoc, TargetSwayLoc, DeltaTime, 9.f);
	const FRotator TargetSwayRot(FMath::Clamp(PitchDelta * 1.2f, -4.f, 4.f),
		FMath::Clamp(-YawDelta * 1.5f, -5.f, 5.f), FMath::Clamp(-YawDelta * 0.8f, -4.f, 4.f));
	SwayRot = FMath::RInterpTo(SwayRot, TargetSwayRot, DeltaTime, 9.f);

	// толчок при выстреле
	GunKick = FMath::FInterpTo(GunKick, 0.f, DeltaTime, 12.f);
	Offset.X -= GunKick * 4.f;
	Offset.Z += GunKick * 0.6f;
	FRotator RotOffset = SwayRot;
	RotOffset.Pitch += GunKick * 4.f;
	Offset += SwayLoc;

	// доставание оружия: поднимается снизу за четверть секунды
	const float DrawAlpha = FMath::Clamp((Now - DrawStartTime) / 0.25f, 0.f, 1.f);
	Offset.Z -= (1.f - DrawAlpha) * 25.f;
	RotOffset.Pitch -= (1.f - DrawAlpha) * 35.f;

	// у скелетной вьюмодели перезарядку и осмотр играет сама анимация,
	// процедурные наклоны только помешают
	if (bReloading && !bUsingSkeletalVM)
	{
		const float A = FMath::Clamp((ReloadEndTime - Now) / ReloadDuration, 0.f, 1.f);
		const float Dip = FMath::Sin(A * PI); // плавно вниз и обратно
		Offset.Z -= Dip * 6.f;
		RotOffset.Pitch -= Dip * 25.f;
		RotOffset.Roll += Dip * 12.f;
	}

	// осмотр по F: оружие поворачивается и наклоняется
	if (Now < InspectEndTime && !bUsingSkeletalVM)
	{
		const float A = 1.f - FMath::Clamp((InspectEndTime - Now) / 2.6f, 0.f, 1.f);
		const float Curve = FMath::Sin(A * PI);
		RotOffset.Yaw += Curve * 55.f;
		RotOffset.Roll -= Curve * 30.f;
		Offset += FVector(-2.f, 2.f, -1.5f) * Curve;
	}

	// присед после приземления
	if (Now - LandDipStart < 0.25f)
	{
		Offset.Z -= FMath::Sin((Now - LandDipStart) / 0.25f * PI) * 3.f;
	}

	// Сдвиг вьюмодели из настроек кладём здесь, а не при выдаче оружия:
	// так ползунок в меню двигает ствол сразу, а не после смены слота.
	Offset.X += RSOptions::GetVmOffsetX();
	Offset.Y += RSOptions::GetVmOffset();
	Offset.Z += RSOptions::GetVmOffsetZ();

	Visual->SetRelativeLocation(GunBaseLoc + Offset);
	Visual->SetRelativeRotation(GunBaseRot + RotOffset);
}

void ARSCharacter::ServerFire_Implementation(FVector Start, FVector_NetQuantizeNormal Dir, ERSWeapon Weapon)
{
	DoFireTrace(Start, Dir, Weapon);
}

void ARSCharacter::DoFireTrace(const FVector& Start, const FVector& Dir, ERSWeapon Weapon)
{
	const FRSWeaponDef& Def = RSWeapons::Get(Weapon);
	// дробовик выпускает пучок дробин, остальные — одну пулю
	for (int32 i = 0; i < FMath::Max(1, Def.Pellets); i++)
	{
		FVector PelletDir = Dir;
		if (Def.Pellets > 1)
		{
			PelletDir = FMath::VRandCone(Dir, FMath::DegreesToRadians(2.5f));
		}
		FireOnePellet(Start, PelletDir, Weapon);
	}
}

void ARSCharacter::FireOnePellet(const FVector& Start, const FVector& Dir, ERSWeapon Weapon)
{
	UWorld* World = GetWorld();
	const FRSWeaponDef& Def = RSWeapons::Get(Weapon);
	const float Range = (Weapon == ERSWeapon::Knife) ? 200.f
		: (Def.Pellets > 1 ? 3000.f : 20000.f);
	const FVector End = Start + Dir * Range;

	const FRSBulletPath Path = TraceBullet(Start, Dir, Weapon);
	const FHitResult& Hit = Path.Hit;
	const bool bHit = Path.bHit;

	const FVector TracerStart = Start + Dir * 60.f + FVector(0.f, 0.f, -12.f);
	MulticastTracer(TracerStart, bHit ? Hit.ImpactPoint : End, Weapon == ERSWeapon::Knife);

	if (!bHit)
	{
		return;
	}

	AActor* Target = Hit.GetActor();
	bool bHeadshot = false;
	float Damage = DamageForHit(Hit, Weapon, Start, bHeadshot) * Path.DamageScale;
	const int32 WallsPassed = Path.WallsPassed;

	if (Damage <= 0.f)
	{
		// в кого-то попали, но не во врага — для лога это промах
		CheatLog(TEXT("промах"), FLinearColor(0.7f, 0.7f, 0.75f), TEXT("miss"));
	}

	if (Damage > 0.f)
	{
		static const TCHAR* BoxNames[] = { TEXT("голова"), TEXT("грудь"), TEXT("живот"), TEXT("конечность") };
		const ERSHitbox Box = Hit.BoneName.IsNone() ? ERSHitbox::Chest : HitboxFromBone(Hit.BoneName);
		CheatLog(FString::Printf(TEXT("урон %.0f → %s (%s)"), Damage,
			*RSCombatantName(Target), BoxNames[(int32)Box]), FLinearColor(0.3f, 1.f, 0.4f));

		if (ARSCharacter* Victim = Cast<ARSCharacter>(Target))
		{
			Victim->bLastHitHeadshot = bHeadshot;
			Victim->bLastHitThroughWall = (WallsPassed > 0);
		}
		else if (ARSBot* BotVictim = Cast<ARSBot>(Target))
		{
			BotVictim->bLastHitHeadshot = bHeadshot;
			BotVictim->bLastHitThroughWall = (WallsPassed > 0);
		}

		UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, nullptr);
		ClientHitMarker();
	}
}

ARSCharacter::ERSHitbox ARSCharacter::HitboxFromBone(FName Bone)
{
	const FString Name = Bone.ToString().ToLower();
	if (Name.Contains(TEXT("head")) || Name.Contains(TEXT("neck")))
	{
		return ERSHitbox::Head;
	}
	if (Name.Contains(TEXT("pelvis")) || Name == TEXT("spine_01") || Name == TEXT("spine_02"))
	{
		return ERSHitbox::Stomach;
	}
	if (Name.Contains(TEXT("spine")) || Name.Contains(TEXT("clavicle")))
	{
		return ERSHitbox::Chest;
	}
	if (Name.Contains(TEXT("thigh")) || Name.Contains(TEXT("calf")) || Name.Contains(TEXT("foot"))
		|| Name.Contains(TEXT("arm")) || Name.Contains(TEXT("hand")))
	{
		return ERSHitbox::Limb;
	}
	return ERSHitbox::Chest;
}

float ARSCharacter::HitboxMult(ERSHitbox Box)
{
	// как в CS: живот больнее груди, конечности заметно слабее
	switch (Box)
	{
	case ERSHitbox::Stomach: return 1.25f;
	case ERSHitbox::Limb:    return 0.75f;
	default:                 return 1.f;
	}
}

float ARSCharacter::HitboxHeight(int32 Index)
{
	// высота точки прицеливания от центра капсулы
	switch (Index)
	{
	case 1:  return 25.f;  // грудь
	case 2:  return 5.f;   // живот
	default: return 55.f;  // голова
	}
}

float ARSCharacter::DamageForHit(const FHitResult& Hit, ERSWeapon Weapon,
	const FVector& Start, bool& bOutHeadshot) const
{
	const FRSWeaponDef& Def = RSWeapons::Get(Weapon);
	AActor* Target = Hit.GetActor();
	bOutHeadshot = false;

	// по своим не стреляем: ни по ботам своей команды, ни по союзным игрокам
	float TargetZ = 0.f;
	bool bValidTarget = false;
	if (const ARSBot* Bot = Cast<ARSBot>(Target))
	{
		TargetZ = Bot->GetActorLocation().Z;
		bValidTarget = (Bot->Team != Team);
	}
	else if (const ARSCharacter* Player = Cast<ARSCharacter>(Target))
	{
		TargetZ = Player->GetActorLocation().Z;
		bValidTarget = (Player != this && Player->Team != Team);
	}
	if (!bValidTarget)
	{
		return 0.f;
	}

	// Кость известна — берём хитбокс по ней. Если луч пришёл в капсулу
	// (кости нет), падаем на прежнюю прикидку по высоте.
	const ERSHitbox Box = Hit.BoneName.IsNone()
		? ((Hit.ImpactPoint.Z > TargetZ + 40.f) ? ERSHitbox::Head : ERSHitbox::Chest)
		: HitboxFromBone(Hit.BoneName);

	bOutHeadshot = (Box == ERSHitbox::Head);
	float Damage = Def.BodyDamage * (bOutHeadshot ? Def.HeadMult : HitboxMult(Box));

	// падение урона с дистанцией: в полную силу до 15 м, минимум 60%
	if (Def.Mesh != ERSMeshKind::Sniper && Weapon != ERSWeapon::Knife)
	{
		const float Dist = FVector::Dist(Start, Hit.ImpactPoint);
		Damage *= FMath::Clamp(1.f - (Dist - 1500.f) / 4500.f * 0.4f, 0.6f, 1.f);
	}

	// шлем спасает голову, пока цела броня
	if (const ARSCharacter* Victim = Cast<ARSCharacter>(Target))
	{
		if (bOutHeadshot && Victim->bHasHelmet && Victim->Armor > 0.f)
		{
			Damage *= 0.5f;
		}
	}
	return Damage;
}

FRSBulletPath ARSCharacter::TraceBullet(const FVector& Start, const FVector& Dir,
	ERSWeapon Weapon) const
{
	FRSBulletPath Out;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return Out;
	}

	const FRSWeaponDef& Def = RSWeapons::Get(Weapon);
	const float Range = (Weapon == ERSWeapon::Knife) ? 200.f
		: (Def.Pellets > 1 ? 3000.f : 20000.f);
	const FVector End = Start + Dir * Range;

	// Пуля проходит сквозь тонкую преграду, теряя часть урона. Сколько
	// преград осилит — зависит от класса оружия, как в CS.
	const int32 MaxWalls = RSPenetrationPower(Weapon);
	FVector TraceFrom = Start;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	Params.bTraceComplex = true;

	while (true)
	{
		Out.bHit = World->LineTraceSingleByChannel(Out.Hit, TraceFrom, End, ECC_Visibility, Params);
		if (!Out.bHit)
		{
			break;
		}

		// в бойца попали — пуля останавливается
		if (Cast<ARSCharacter>(Out.Hit.GetActor()) || Cast<ARSBot>(Out.Hit.GetActor()))
		{
			break;
		}

		if (Out.WallsPassed >= MaxWalls)
		{
			break; // запас пробития кончился, пуля вязнет в стене
		}

		// Тыльную грань ищем обратной трассировкой из точки за преградой.
		// Приём рабочий: замер на Mirage дал толщины 7 и 42 единицы. Если
		// тыла в пределах порога нет, стена толстая и пуля в ней остаётся.
		const float MaxThickness = 110.f;
		const FVector Probe = Out.Hit.ImpactPoint + Dir * MaxThickness;
		FHitResult Back;
		FCollisionQueryParams BackParams;
		BackParams.bTraceComplex = true;
		if (!World->LineTraceSingleByChannel(Back, Probe, Out.Hit.ImpactPoint,
			ECC_Visibility, BackParams))
		{
			break;
		}

		// Потеря урона зависит от толщины: фанерная дверь почти не мешает,
		// кирпичная стена съедает две трети. Плоский коэффициент уравнивал
		// бы их, а толщину мы всё равно измеряем.
		const float Thickness = FVector::Dist(Out.Hit.ImpactPoint, Back.ImpactPoint);
		Out.DamageScale *= FMath::Lerp(0.85f, 0.3f,
			FMath::Clamp(Thickness / MaxThickness, 0.f, 1.f));

		Out.WallsPassed++;
		// продолжаем чуть за тыльной гранью, иначе трасса упрётся в неё же
		TraceFrom = Back.ImpactPoint + Dir * 2.f;
	}

	return Out;
}

float ARSCharacter::DamageIfFiredNow() const
{
	// Проверка перед выстрелом: чит не должен жать курок в стену. Считаем
	// тем же проходом, что и настоящая пуля — иначе триггербот видит стену
	// и молчит там, где выстрел на самом деле проходит насквозь.
	const FVector Start = Camera->GetComponentLocation();
	const FRSBulletPath Path = TraceBullet(Start, Camera->GetForwardVector(), CurrentWeapon);
	if (!Path.bHit)
	{
		return 0.f;
	}

	bool bHead = false;
	return DamageForHit(Path.Hit, CurrentWeapon, Start, bHead) * Path.DamageScale;
}

void ARSCharacter::MulticastTracer_Implementation(FVector Start, FVector End, bool bMelee)
{
	const FRSWeaponDef& Def = RSWeapons::Get(CurrentWeapon);

	if (!bMelee)
	{
		// светящаяся трасса вместо отладочной линии
		const bool bBig = (Def.Mesh == ERSMeshKind::Sniper) || Def.Pellets > 1;
		ARSTracer::Spawn(GetWorld(), Start, End, bBig);
	}

	// у ножа свой замах, у стволов — звук по классу оружия
	RSAudio::PlayAt(this, RSAudio::GetFireSound(CurrentWeapon), Start,
		bMelee ? 0.7f : 1.f,
		bMelee ? RSAudio::ERange::Step : RSAudio::ERange::Gun,
		!IsLocallyControlled());
}

void ARSCharacter::ClientHitMarker_Implementation()
{
	LastHitMarkerTime = GetWorld()->GetTimeSeconds();

	// звук попадания: слышно только стрелявшему, поэтому играем локально
	if (bHitSound)
	{
		RSAudio::PlayAt(this, RSAudio::Get(RSAudio::ESound::HitMarker),
			GetActorLocation(), 0.9f, RSAudio::ERange::Step, false);
	}
}

void ARSCharacter::ClientDamaged_Implementation()
{
	LastDamagedTime = GetWorld()->GetTimeSeconds();
	// aim punch: попадание подбрасывает прицел
	PunchTarget.X += 0.9f;
	PunchTarget.Y += FMath::FRandRange(-0.4f, 0.4f);
}

float ARSCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return 0.f;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (bGodMode || !bAlive || Now < SpawnProtectionUntil || Health <= 0.f)
	{
		return 0.f;
	}

	CheatLog(FString::Printf(TEXT("получено %.0f ← %s"), DamageAmount,
		*RSCombatantName(DamageCauser)), FLinearColor(1.f, 0.4f, 0.4f));

	// гранаты и огонь бьют через своего «виновника»: дружественного урона нет,
	// но своей же гранатой подорваться можно
	AActor* KillerActor = DamageCauser;
	FString WeaponName;
	if (ARSGrenade* Nade = Cast<ARSGrenade>(DamageCauser))
	{
		if (Nade->Team == Team && Nade->Thrower.Get() != this)
		{
			return 0.f;
		}
		KillerActor = Nade->Thrower.Get();
		WeaponName = RSWeapons::Get(Nade->Type).Name;
	}
	else if (ARSFireZone* Fire = Cast<ARSFireZone>(DamageCauser))
	{
		if (Fire->Team == Team && Fire->Thrower.Get() != this)
		{
			return 0.f;
		}
		KillerActor = Fire->Thrower.Get();
		WeaponName = TEXT("MOLOTOV");
	}

	// кевлар гасит половину урона и стачивается
	float Taken = DamageAmount;
	if (Armor > 0.f)
	{
		Taken *= 0.5f;
		Armor = FMath::Max(0.f, Armor - DamageAmount * 0.25f);
	}

	// tagging: попадание замедляет, как в CS
	TaggedUntil = Now + 0.6f;

	Health -= Taken;
	ClientDamaged();

	if (Health <= 0.f)
	{
		Deaths++;
		FString KillerName = TEXT("?");
		uint8 KillerTeam = (uint8)ERSTeam::CT;
		// COUNT = неизвестно: killfeed нарисует текстом, а не иконкой
		uint8 KillerWeapon = (uint8)ERSWeapon::COUNT;
		if (ARSCharacter* Killer = Cast<ARSCharacter>(KillerActor))
		{
			if (Killer != this)
			{
				Killer->Kills++;
				Killer->AddMoney(RSKillReward(Killer->CurrentWeapon));
			}
			KillerName = RSCombatantName(Killer);
			if (WeaponName.IsEmpty())
			{
				WeaponName = Killer->GetWeaponName();
			}
			KillerTeam = (uint8)Killer->Team;
			KillerWeapon = (uint8)Killer->CurrentWeapon;
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
			KillerWeapon = (uint8)BotKiller->Weapon;
		}
		if (ARSGameState* GS = GetWorld()->GetGameState<ARSGameState>())
		{
			GS->MulticastAddKill(KillerName, RSCombatantName(this), WeaponName,
				bLastHitHeadshot, KillerTeam, (uint8)Team, KillerWeapon,
				RSComputeKillFlags(KillerActor, this, bLastHitHeadshot));
		}
		bLastHitHeadshot = false;
		bLastHitThroughWall = false;
		Die();
	}
	return Taken;
}

void ARSCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (bAlive)
	{
		SetActorLocation(ARSArena::FindSpawnPoint(GetWorld(), Team));
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}
	Super::FellOutOfWorld(DmgType);
}

void ARSCharacter::Die()
{
	bAlive = false;
	Health = 0.f;
	bFireHeld = false;

	// оружие выпадает на землю, нож остаётся при владельце
	if (bHasPrimary)
	{
		DropWeapon(PrimaryType);
	}
	if (bHasSecondary)
	{
		DropWeapon(SecondaryType);
	}
	// броня и гранаты теряются вместе с жизнью
	Armor = 0.f;
	bHasHelmet = false;
	for (int32 i = 0; i < RSWeapons::GrenadeTypes; i++)
	{
		Grenades[i] = 0;
	}

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	// сразу переключаем взгляд на живого союзника
	CycleSpectate();

	if (ARSGameMode* GM = GetWorld()->GetAuthGameMode<ARSGameMode>())
	{
		GM->OnCombatantDied();
	}
}

void ARSCharacter::FreezeUntilRound()
{
	bAlive = false;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
}

void ARSCharacter::RespawnForRound(const FVector& Location)
{
	StopSpectating();
	if (bBuyMenuOpen)
	{
		ToggleBuyMenu(); // заодно уберёт курсор и вернёт игровой ввод
	}
	BuyCategory = -1;
	bAlive = true;
	Health = 100.f;


	// пистолет по команде, если своего нет; купленное остаётся у выживших
	if (!bHasSecondary || !RSWeapons::AllowedFor(SecondaryType, Team))
	{
		GiveWeapon(Team == ERSTeam::CT ? ERSWeapon::USP : ERSWeapon::Glock);
	}
	if (bHasPrimary && !RSWeapons::AllowedFor(PrimaryType, Team))
	{
		bHasPrimary = false; // после смены сторон чужое оружие не тащим
	}

	// боезапас пополняется к каждому раунду
	if (bHasPrimary)
	{
		Ammo[(uint8)PrimaryType] = RSWeapons::Get(PrimaryType).Mag;
		Reserve[(uint8)PrimaryType] = RSWeapons::Get(PrimaryType).ReserveMax;
	}
	Ammo[(uint8)SecondaryType] = RSWeapons::Get(SecondaryType).Mag;
	Reserve[(uint8)SecondaryType] = RSWeapons::Get(SecondaryType).ReserveMax;

	CurrentWeapon = bHasPrimary ? PrimaryType : SecondaryType;
	ApplyWeaponVisuals();
	bReloading = false;
	GetWorldTimerManager().ClearTimer(ReloadTimer);
	RecoilIndex = 0;
	PunchCurrent = PunchTarget = FVector2D::ZeroVector;
	TaggedUntil = -10.f;
	InspectEndTime = -10.f;
	FlashEndTime = -10.f;
	SpawnProtectionUntil = GetWorld()->GetTimeSeconds() + 1.f;

	SetActorLocation(Location);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

void ARSCharacter::AddBot()
{
	UWorld* World = GetWorld();
	const FVector Start = Camera->GetComponentLocation();
	const FVector Forward = Camera->GetForwardVector();

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, Start + Forward * 20000.f, ECC_Visibility, Params);

	const FVector SpawnLoc = bHit ? Hit.ImpactPoint + FVector(0.f, 0.f, 100.f)
	                              : Start + Forward * 600.f;
	ServerAddBot(SpawnLoc);
}

void ARSCharacter::ServerAddBot_Implementation(FVector Location)
{
	if (ARSGameMode* GM = GetWorld()->GetAuthGameMode<ARSGameMode>())
	{
		GM->SpawnBotAt(Location, Team == ERSTeam::CT ? ERSTeam::T : ERSTeam::CT);
	}
}

void ARSCharacter::RemoveBot()
{
	UWorld* World = GetWorld();
	const FVector Start = Camera->GetComponentLocation();

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	if (World->LineTraceSingleByChannel(Hit, Start, Start + Camera->GetForwardVector() * 20000.f, ECC_Visibility, Params))
	{
		// удаление без счёта килов и без респавна
		if (ARSBot* Bot = Cast<ARSBot>(Hit.GetActor()))
		{
			ServerRemoveBot(Bot);
		}
	}
}

void ARSCharacter::ServerRemoveBot_Implementation(ARSBot* Bot)
{
	if (IsValid(Bot))
	{
		Bot->Destroy();
	}
}

void ARSCharacter::ClearBots()
{
	ServerClearBots();
}

void ARSCharacter::ServerClearBots_Implementation()
{
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		It->Destroy();
	}
}
