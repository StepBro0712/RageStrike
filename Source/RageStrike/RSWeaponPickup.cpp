#include "RSWeaponPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ARSWeaponPickup::ARSWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->InitSphereRadius(90.f);
	PickupSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SetRootComponent(PickupSphere);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(PickupSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RifleMesh(
		TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle"));
	if (RifleMesh.Succeeded()) { RifleAsset = RifleMesh.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SniperMesh(
		TEXT("/Game/Weapons/Sniper/Meshes/SKM_SniperR700.SKM_SniperR700"));
	if (SniperMesh.Succeeded()) { SniperAsset = SniperMesh.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PistolMesh(
		TEXT("/Game/Weapons/Pistol/Meshes/SM_Pistol.SM_Pistol"));
	if (PistolMesh.Succeeded()) { PistolAsset = PistolMesh.Object; }
}

void ARSWeaponPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARSWeaponPickup, WeaponType);
}

void ARSWeaponPickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyMesh();

	if (HasAuthority())
	{
		PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ARSWeaponPickup::OnOverlap);
	}
}

void ARSWeaponPickup::OnRep_Weapon()
{
	ApplyMesh();
}

void ARSWeaponPickup::ApplyMesh()
{
	switch (RSWeapons::Get(WeaponType).Mesh)
	{
	case ERSMeshKind::Sniper: Mesh->SetStaticMesh(SniperAsset); break;
	case ERSMeshKind::Pistol: Mesh->SetStaticMesh(PistolAsset); break;
	default:                  Mesh->SetStaticMesh(RifleAsset);  break;
	}
	// кладём плашмя, стволом вбок
	Mesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
}

void ARSWeaponPickup::DropAt(ERSWeapon Type, const FVector& From)
{
	WeaponType = Type;
	ApplyMesh();
	ArmedAt = GetWorld()->GetTimeSeconds();

	// опускаем на пол, чтобы оружие не висело в воздухе
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;
	const FVector Start = From + FVector(0.f, 0.f, 40.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, 1000.f),
		ECC_Visibility, Params))
	{
		SetActorLocation(Hit.ImpactPoint + FVector(0.f, 0.f, 45.f));
	}
	else
	{
		SetActorLocation(From);
	}

	SetLifeSpan(60.f); // мусор не копится до конца матча
}

void ARSWeaponPickup::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other,
	UPrimitiveComponent* OtherComp, int32 BodyIndex, bool bFromSweep, const FHitResult& Sweep)
{
	if (!HasAuthority() || GetWorld()->GetTimeSeconds() - ArmedAt < 1.f)
	{
		return; // короткая задержка, иначе подбираешь собственный сброс
	}

	ARSCharacter* Player = Cast<ARSCharacter>(Other);
	if (Player && Player->bAlive && Player->TryPickUpWeapon(WeaponType))
	{
		Destroy();
	}
}
