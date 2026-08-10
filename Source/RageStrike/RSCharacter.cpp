#include "RSCharacter.h"
#include "RSBot.h"
#include "RSGameMode.h"
#include "RSGameState.h"
#include "RSArena.h"
#include "RSWeaponPickup.h"
#include "RSPlayerController.h"
#include "RSGrenade.h"
#include "RSFireZone.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
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
		return FString::Printf(TEXT("Бот %d"), Bot->BotNumber);
	}
	if (Cast<ARSCharacter>(Who))
	{
		return TEXT("Игрок");
	}
	return TEXT("?");
}

ARSCharacter::ARSCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	// пресет Pawn игнорирует Visibility — боты не могли попасть по игроку
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

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

void ARSCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARSCharacter, Health);
	DOREPLIFETIME(ARSCharacter, Kills);
	DOREPLIFETIME(ARSCharacter, Deaths);
	DOREPLIFETIME(ARSCharacter, CurrentWeapon);
	DOREPLIFETIME(ARSCharacter, Team);
	DOREPLIFETIME(ARSCharacter, bAlive);
	DOREPLIFETIME(ARSCharacter, Money);
	DOREPLIFETIME(ARSCharacter, Armor);
	DOREPLIFETIME(ARSCharacter, bHasHelmet);
	DOREPLIFETIME(ARSCharacter, bHasPrimary);
	DOREPLIFETIME(ARSCharacter, PrimaryType);
	DOREPLIFETIME(ARSCharacter, bHasSecondary);
	DOREPLIFETIME(ARSCharacter, SecondaryType);
	DOREPLIFETIME(ARSCharacter, Grenades);
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
	if (!State || State->Phase != ERSPhase::Intermission)
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
			return; // сначала выбросить старое (G)
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
		// новый пистолет заменяет старый
		Money -= Def.Price;
		GiveWeapon(Weapon);
		CurrentWeapon = Weapon;
		ApplyWeaponVisuals();
	}
}

void ARSCharacter::ServerBuyArmor_Implementation(bool bWithHelmet)
{
	const ARSGameState* State = GetWorld()->GetGameState<ARSGameState>();
	if (!State || State->Phase != ERSPhase::Intermission)
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
		}
	}
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

	PlayerInputComponent->BindAction("ToggleAimbot", IE_Pressed, this, &ARSCharacter::ToggleAimbot);
	PlayerInputComponent->BindAction("ToggleESP", IE_Pressed, this, &ARSCharacter::ToggleESP);
	PlayerInputComponent->BindAction("ToggleTrigger", IE_Pressed, this, &ARSCharacter::ToggleTrigger);
	PlayerInputComponent->BindAction("ToggleNoRecoil", IE_Pressed, this, &ARSCharacter::ToggleNoRecoil);
	PlayerInputComponent->BindAction("ToggleSpeed", IE_Pressed, this, &ARSCharacter::ToggleSpeed);
	PlayerInputComponent->BindAction("ToggleSilent", IE_Pressed, this, &ARSCharacter::ToggleSilent);
	PlayerInputComponent->BindAction("ToggleGod", IE_Pressed, this, &ARSCharacter::ToggleGod);

	// B освободили под закупку, управление ботами ушло на F9-F12
	PlayerInputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ARSCharacter::AddBot);
	PlayerInputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ARSCharacter::RemoveBot);
	PlayerInputComponent->BindKey(EKeys::F12, IE_Pressed, this, &ARSCharacter::ClearBots);
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &ARSCharacter::ToggleBuyMenu);
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
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &ARSCharacter::ToggleView);
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
	// открывается всегда: подсказка внутри объяснит, почему покупка недоступна
	bBuyMenuOpen = !bBuyMenuOpen;
	BuyCategory = -1;
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
			if (N >= 1 && N <= 6)
			{
				BuyCategory = N - 1; // 6-я категория — броня
			}
			return;
		}
		if (BuyCategory == 5) // броня
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
	Camera->SetFieldOfView(90.f);
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

	UStaticMesh* WeaponMesh = AKAsset;
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

	TPGunMesh->SetStaticMesh(WeaponMesh);
	TPGunMesh->SetRelativeLocation(HandLoc);
	TPGunMesh->SetRelativeRotation(HandRot);
	TPGunMesh->SetRelativeScale3D(Scale);

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
	GunMesh->SetRelativeScale3D(Scale);

	// база для процедурной анимации вьюмодели + анимация доставания
	GunBaseLoc = CamLoc;
	GunBaseRot = CamRot;
	if (IsLocallyControlled() && GetWorld())
	{
		DrawStartTime = GetWorld()->GetTimeSeconds();
	}

	ArmsMesh->SetVisibility(bUseArms && !bThirdPerson);
	GunMesh->SetVisibility(!bThirdPerson);
	TPGunMesh->SetOwnerNoSee(!bThirdPerson);
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
		ServerSyncCheats(bGodMode, bSpeedhack);
	}
}

void ARSCharacter::ServerSyncCheats_Implementation(bool bInGod, bool bInSpeed)
{
	bGodMode = bInGod;
	bSpeedhack = bInSpeed;
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
			GunMesh->SetVisibility(false); // в оптике оружие не видно, как в CS
		}
	}
}

void ARSCharacter::StopAim()
{
	bAiming = false;
	bAimingNow = false;
	Camera->SetFieldOfView(90.f);
	GunMesh->SetVisibility(!bThirdPerson);
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
		const FQuat DesiredWorld = (GetActorRotation() + Offset).Quaternion();
		const FQuat BoneQuat = GetMesh()->GetSocketQuaternion(TEXT("hand_r"));
		TPGunMesh->SetRelativeRotation(BoneQuat.Inverse() * DesiredWorld);
	}

	// упал за пределы карты — возвращаем, а не убиваем
	if (HasAuthority() && bAlive && GetActorLocation().Z < ARSArena::GetMapFloor(GetWorld()) - 1000.f)
	{
		SetActorLocation(ARSArena::FindSpawnPoint(GetWorld(), Team));
		Move->StopMovementImmediately();
	}

	if (!IsLocallyControlled())
	{
		return;
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

	if (bAimbot)
	{
		RunAimbot();
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
	float BestAngle = 90.f; // рейдж-FOV: пол-экрана

	auto Consider = [&](AActor* Candidate)
	{
		if (!IsValid(Candidate) || !IsEnemyActor(Candidate))
		{
			return;
		}
		const FVector Head = Candidate->GetActorLocation() + FVector(0.f, 0.f, 55.f);
		const FVector Dir = (Head - CamLoc).GetSafeNormal();
		const float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Forward, Dir)));
		if (Angle >= BestAngle)
		{
			return;
		}

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.bTraceComplex = true;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, CamLoc, Head, ECC_Visibility, Params);
		if (!bBlocked || Hit.GetActor() == Candidate)
		{
			Best = Candidate;
			BestAngle = Angle;
			OutAimPoint = Head;
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

void ARSCharacter::RunAimbot()
{
	FVector AimPoint;
	if (FindBestTarget(AimPoint) && GetController())
	{
		const FVector CamLoc = Camera->GetComponentLocation();
		GetController()->SetControlRotation((AimPoint - CamLoc).Rotation());
	}
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
			// триггербот жмёт «курок» сам: сбрасываем блокировку полуавтомата,
			// иначе пистолеты и AWP стреляли бы один раз за всё время
			bShotSincePress = false;
			TryFire();
		}
	}
}

void ARSCharacter::TryFire()
{
	UWorld* World = GetWorld();
	const FRSWeaponDef& Def = RSWeapons::Get(CurrentWeapon);
	const float Now = World->GetTimeSeconds();
	if (IsFrozen())
	{
		return; // в закупку не стреляем
	}
	if (!bAlive || bReloading || Now - LastFireTime < Def.Interval)
	{
		return;
	}
	// полуавтомат: одно нажатие — один выстрел
	if (!Def.bAuto && bShotSincePress)
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

	// вьюмодель дёргается назад, осмотр прерывается
	GunKick = 1.f;
	InspectEndTime = -10.f;

	if (!bNoRecoilSpread && CurrentWeapon != ERSWeapon::Knife)
	{
		float KickPitch = 0.f, KickYaw = 0.f;
		GetRecoilKick(CurrentWeapon, RecoilIndex, KickPitch, KickYaw);
		PunchTarget.X += KickPitch;
		PunchTarget.Y += KickYaw;
		RecoilIndex++;
		SpreadBloom = FMath::Min(SpreadBloom + 0.12f, 1.2f);
	}
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

	// перезарядка: ствол опущен и покачивается
	if (bReloading)
	{
		const float A = FMath::Clamp((ReloadEndTime - Now) / ReloadDuration, 0.f, 1.f);
		const float Dip = FMath::Sin(A * PI); // плавно вниз и обратно
		Offset.Z -= Dip * 6.f;
		RotOffset.Pitch -= Dip * 25.f;
		RotOffset.Roll += Dip * 12.f;
	}

	// осмотр по F: оружие поворачивается и наклоняется
	if (Now < InspectEndTime)
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

	GunMesh->SetRelativeLocation(GunBaseLoc + Offset);
	GunMesh->SetRelativeRotation(GunBaseRot + RotOffset);
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

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);
	const bool bHit = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params);

	const FVector TracerStart = Start + Dir * 60.f + FVector(0.f, 0.f, -12.f);
	MulticastTracer(TracerStart, bHit ? Hit.ImpactPoint : End, Weapon == ERSWeapon::Knife);

	if (!bHit)
	{
		return;
	}

	AActor* Target = Hit.GetActor();
	float TargetZ = 0.f;
	bool bValidTarget = false;

	// по своим не стреляем: ни по ботам своей команды, ни по союзным игрокам
	if (ARSBot* Bot = Cast<ARSBot>(Target))
	{
		TargetZ = Bot->GetActorLocation().Z;
		bValidTarget = (Bot->Team != Team);
	}
	else if (ARSCharacter* Player = Cast<ARSCharacter>(Target))
	{
		TargetZ = Player->GetActorLocation().Z;
		bValidTarget = (Player->Team != Team);
	}

	if (bValidTarget)
	{
		const bool bHeadshot = Hit.ImpactPoint.Z > TargetZ + 40.f;
		float Damage = Def.BodyDamage * (bHeadshot ? Def.HeadMult : 1.f);

		// падение урона с дистанцией: в полную силу до 15 м, минимум 60%
		if (Def.Mesh != ERSMeshKind::Sniper && Weapon != ERSWeapon::Knife)
		{
			const float Dist = FVector::Dist(Start, Hit.ImpactPoint);
			Damage *= FMath::Clamp(1.f - (Dist - 1500.f) / 4500.f * 0.4f, 0.6f, 1.f);
		}

		if (ARSCharacter* Victim = Cast<ARSCharacter>(Target))
		{
			// шлем спасает голову, пока цела броня
			if (bHeadshot && Victim->bHasHelmet && Victim->Armor > 0.f)
			{
				Damage *= 0.5f;
			}
			Victim->bLastHitHeadshot = bHeadshot;
		}
		else if (ARSBot* BotVictim = Cast<ARSBot>(Target))
		{
			BotVictim->bLastHitHeadshot = bHeadshot;
		}

		UGameplayStatics::ApplyDamage(Target, Damage, GetController(), this, nullptr);
		ClientHitMarker();
	}
}

void ARSCharacter::MulticastTracer_Implementation(FVector Start, FVector End, bool bMelee)
{
	if (!bMelee)
	{
		DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false, 0.06f, 0, 0.5f);
	}
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Start, bMelee ? 0.25f : 0.5f);
	}
}

void ARSCharacter::ClientHitMarker_Implementation()
{
	LastHitMarkerTime = GetWorld()->GetTimeSeconds();
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
	bBuyMenuOpen = false;
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
