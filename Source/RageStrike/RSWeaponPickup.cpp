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
	// сначала пробуем модель именно того оружия, что выбросили
	UStaticMesh* Exact = RSWeapons::LoadWeaponMesh(WeaponType);
	if (!Exact)
	{
		switch (RSWeapons::Get(WeaponType).Mesh)
		{
		case ERSMeshKind::Sniper: Exact = SniperAsset; break;
		case ERSMeshKind::Pistol: Exact = PistolAsset; break;
		default:                  Exact = RifleAsset;  break;
		}
	}
	Mesh->SetStaticMesh(Exact);
	if (!Exact)
	{
		return;
	}

	// приводим к реальному размеру, как в руках
	const FVector Extent = Exact->GetBounds().BoxExtent;
	const float Longest = FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 2.f;
	const float Fit = (Longest > 0.01f) ? RSWeapons::RealLength(WeaponType) / Longest : 1.f;
	Mesh->SetRelativeScale3D(FVector(Fit));

	// Кладём плашмя. Фиксированный поворот не годится: у моделей от разных
	// авторов ствол вытянут по разным осям, поэтому считаем поворот так,
	// чтобы длинная ось меша легла горизонтально, а самая тонкая смотрела вверх.
	const FVector Axes[3] = { FVector::XAxisVector, FVector::YAxisVector, FVector::ZAxisVector };
	const float Sizes[3] = { Extent.X, Extent.Y, Extent.Z };

	int32 LongIdx = 0, ThinIdx = 0;
	for (int32 i = 1; i < 3; i++)
	{
		if (Sizes[i] > Sizes[LongIdx]) { LongIdx = i; }
		if (Sizes[i] < Sizes[ThinIdx]) { ThinIdx = i; }
	}
	if (ThinIdx == LongIdx)
	{
		ThinIdx = (LongIdx + 1) % 3;
	}

	// матрица переводит X в длинную ось и Z в тонкую; нам нужен обратный поворот
	const FQuat Lay = FRotationMatrix::MakeFromXZ(Axes[LongIdx], Axes[ThinIdx])
		.ToQuat().Inverse();
	Mesh->SetRelativeRotation(Lay);

	const float HalfThickness = Sizes[ThinIdx] * Fit;
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, -GroundOffset + HalfThickness));
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
		SetActorLocation(Hit.ImpactPoint + FVector(0.f, 0.f, GroundOffset));
		// разворачиваем по случайному углу и кладём вдоль поверхности,
		// чтобы стволы не лежали все одинаково
		SetActorRotation(FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
	}
	else
	{
		SetActorLocation(From);
	}

	// меш пересчитываем после поворота: он зависит от габаритов модели
	ApplyMesh();

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
