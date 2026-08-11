#include "RSFireZone.h"
#include "RSCharacter.h"
#include "RSBot.h"
#include "RSAudio.h"
#include "RSGrenade.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

UNiagaraSystem* ARSFireZone::PreloadFX()
{
	// Система грузится один раз и держится за корень. Раньше LoadObject стоял
	// прямо в BeginPlay зоны огня — то есть первый молотов за матч тянул с
	// диска систему, её материалы и текстуры синхронно, в игровом потоке и
	// посреди боя. Отсюда и был рывок в момент срабатывания.
	static UNiagaraSystem* Cached = nullptr;
	static bool bTried = false;
	if (!bTried)
	{
		bTried = true;
		Cached = LoadObject<UNiagaraSystem>(
			nullptr, TEXT("/Game/NiagaraExamples/FX_Misc/NS_Fire.NS_Fire"));
		if (Cached)
		{
			Cached->AddToRoot();
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("RS/огонь: NS_Fire не найден, откат на сферы"));
		}
	}
	return Cached;
}

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
	// Тени от огня не считаем: динамический источник с тенями на каждый
	// молотов — заметная часть просадки кадров, а видно её почти никак.
	Light->SetCastShadows(false);

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

	// Языки пламени. Настоящий эффект Niagara вместо оранжевых сфер; если
	// пак не подключён, откатываемся на прежние сферы — без них зона огня
	// стала бы невидимой, а урон она наносит по-прежнему.
	FRandomStream Rand(GetTypeHash(GetActorLocation()));
	UNiagaraSystem* FireFX = ARSFireZone::PreloadFX();

	// Два очага. Замер stat unit: каждая система NS_Fire стоит около 260
	// вызовов отрисовки, четыре роняли кадр с 6 до 14 мс. Один очаг был
	// дёшев (123 FPS), но заметно уже зоны урона — игрок обжигался там,
	// где огня не видно. Два закрывают площадь честнее примерно за 100 FPS.
	// Упирается именно поток отрисовки, игровая логика тут ни при чём.
	const int32 FireCount = FireFX ? 2 : 10;

	for (int32 i = 0; i < FireCount; i++)
	{
		// Раскладка кольцом, а не случайная: при четырёх очагах случайность
		// то сбивала их в кучу, то оставляла дыры в зоне.
		const float Ang = (2.f * PI * i) / FMath::Max(1, FireCount) + Rand.FRandRange(-0.3f, 0.3f);
		// При двух очагах оба сдвигаем от центра в разные стороны: если один
		// оставить по центру, второй будет выглядеть случайным довеском.
		const float R = (FireCount == 2)
			? Radius * 0.35f
			: ((i == 0) ? 0.f : Radius * 0.45f);
		const FVector Pos(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 15.f);

		if (FireFX)
		{
			UNiagaraComponent* FX = UNiagaraFunctionLibrary::SpawnSystemAttached(
				FireFX, GetRootComponent(), NAME_None, Pos, FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset, /*bAutoDestroy*/ false);
			if (FX)
			{
					// два очага, разнесённые от центра, каждый пошире — вместе
				// они накрывают зону урона, не сливаясь в одно пятно
				FX->SetRelativeScale3D(FVector(2.6f, 2.6f, 2.0f));
				FireFXs.Add(FX);
			}
			continue;
		}

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
		Flame->SetRelativeLocation(Pos);
		Flame->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.5f) * Rand.FRandRange(0.7f, 1.2f));
		Flame->RegisterComponent();
		Flames.Add(Flame);
	}

	// Потрескивание огня цепляем к самому актору: звук в точке мира живёт
	// своей жизнью и продолжал гудеть после того, как огонь погас.
	if (USoundBase* Snd = RSAudio::Get(RSAudio::ESound::Burn))
	{
		UGameplayStatics::SpawnSoundAttached(Snd, GetRootComponent(), NAME_None,
			FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
			/*bStopWhenAttachedToDestroyed*/ true, 0.8f, 1.f, 0.f,
			RSAudio::GetAttenuation(RSAudio::ERange::Ambient));
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
	// Дым тушит огонь. Проверка стоит здесь, на уже существующем таймере,
	// и закрывает оба случая разом: и дым, брошенный на горящую зону, и
	// огонь, подожжённый на краю дыма.
	if (RSSmokeCovers(GetWorld(), GetActorLocation()))
	{
		if (USoundBase* Snd = RSAudio::Get(RSAudio::ESound::Smoke))
		{
			UGameplayStatics::PlaySoundAtLocation(this, Snd, GetActorLocation(), 0.9f);
		}
		Destroy();
		return;
	}

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
