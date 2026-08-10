#include "RSFireZone.h"
#include "RSCharacter.h"
#include "RSBot.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

ARSFireZone::ARSFireZone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	Light->SetLightColor(FLinearColor(1.f, 0.45f, 0.1f));
	Light->SetIntensity(9000.f);
	Light->SetAttenuationRadius(700.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Ball(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Ball.Succeeded())
	{
		SphereMesh = Ball.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Mat.Succeeded())
	{
		BaseMat = Mat.Object;
	}
}

void ARSFireZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARSFireZone, Team);
}

void ARSFireZone::BeginPlay()
{
	Super::BeginPlay();

	// языки пламени: приплюснутые оранжевые сферы по площади
	FRandomStream Rand(GetTypeHash(GetActorLocation()));
	for (int32 i = 0; i < 10; i++)
	{
		UStaticMeshComponent* Flame = NewObject<UStaticMeshComponent>(this);
		Flame->SetStaticMesh(SphereMesh);
		Flame->SetupAttachment(GetRootComponent());
		if (BaseMat)
		{
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
			MID->SetVectorParameterValue(TEXT("Color"),
				FLinearColor(1.f, Rand.FRandRange(0.25f, 0.55f), 0.03f));
			Flame->SetMaterial(0, MID);
		}
		Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flame->SetCastShadow(false);
		const float Ang = Rand.FRandRange(0.f, 2.f * PI);
		const float R = Rand.FRandRange(0.f, Radius * 0.8f);
		Flame->SetRelativeLocation(FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 15.f));
		Flame->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.5f) * Rand.FRandRange(0.7f, 1.2f));
		Flame->RegisterComponent();
		Flames.Add(Flame);
	}

	SetLifeSpan(Duration);

	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(DamageTimer, this, &ARSFireZone::DamageTick, 0.25f, true);
	}
}

void ARSFireZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// пламя дышит
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 i = 0; i < Flames.Num(); i++)
	{
		if (Flames[i])
		{
			const float S = 1.f + 0.18f * FMath::Sin(Now * 11.f + i * 1.7f);
			Flames[i]->SetRelativeScale3D(FVector(1.1f * S, 1.1f * S, 0.5f * S));
		}
	}
}

void ARSFireZone::DamageTick()
{
	auto Burn = [&](AActor* Victim)
	{
		const FVector D = Victim->GetActorLocation() - GetActorLocation();
		if (D.Size2D() < Radius && FMath::Abs(D.Z) < 220.f)
		{
			UGameplayStatics::ApplyDamage(Victim, 7.f, nullptr, this, nullptr);
		}
	};
	for (TActorIterator<ARSCharacter> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->bAlive) { Burn(*It); }
	}
	for (TActorIterator<ARSBot> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->Health > 0.f) { Burn(*It); }
	}
}
