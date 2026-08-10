#include "RSArena.h"
#include "RSMaps.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Landscape.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "TimerManager.h"

ARSArena::ARSArena()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		CubeMesh = Cube.Object;
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Mat.Succeeded())
	{
		BaseMat = Mat.Object;
	}
}

void ARSArena::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARSArena, Seed);
	DOREPLIFETIME(ARSArena, MapIndex);
}

void ARSArena::OnRep_Build()
{
	Build();
}

FVector ARSArena::GetRandomNavPoint(UWorld* World, const FVector& Near, float MinDist)
{
	for (TActorIterator<ARSArena> It(World); It; ++It)
	{
		const TArray<FVector>& Points = It->NavPoints;
		if (Points.Num() == 0)
		{
			break;
		}
		// несколько попыток найти точку не под носом, иначе бот топчется
		for (int32 i = 0; i < 12; i++)
		{
			const FVector& P = Points[FMath::RandHelper(Points.Num())];
			if (FVector::Dist2D(P, Near) > MinDist)
			{
				return P;
			}
		}
		return Points[FMath::RandHelper(Points.Num())];
	}
	return Near;
}

float ARSArena::GetMapFloor(UWorld* World)
{
	for (TActorIterator<ARSArena> It(World); It; ++It)
	{
		return It->GroundLevel;
	}
	return 0.f;
}

FVector ARSArena::FindSpawnPoint(UWorld* World, ERSTeam Team, bool bTeamZone)
{
	float Radius = 3000.f;
	FVector Center = FVector::ZeroVector;
	float Ground = 0.f;
	FBox Bounds(ForceInit);
	for (TActorIterator<ARSArena> It(World); It; ++It)
	{
		Radius = It->PlayRadius;
		Center = It->MapCenter;
		Ground = It->GroundLevel;
		Bounds = It->MapBounds;

		// готовые зоны на краях проходимой части — основной путь
		const TArray<FVector>& Zone = (Team == ERSTeam::CT) ? It->SpawnsCT : It->SpawnsT;
		if (bTeamZone && Zone.Num() > 0)
		{
			const FVector Point = Zone[FMath::RandHelper(Zone.Num())];
			const FVector2D Jitter = FMath::RandPointInCircle(150.f);
			return Point + FVector(Jitter.X, Jitter.Y, 0.f);
		}
		break;
	}

	// Зона команды — дальняя треть карты вдоль её длинной стороны:
	// так CT и T начинают раунд в разных концах, как в CS.
	bool bAxisX = false;
	float ZoneMin = -FLT_MAX;
	float ZoneMax = FLT_MAX;
	if (bTeamZone && Bounds.IsValid)
	{
		const FVector Size = Bounds.GetSize();
		bAxisX = Size.X > Size.Y;
		const float Lo = bAxisX ? Bounds.Min.X : Bounds.Min.Y;
		const float Hi = bAxisX ? Bounds.Max.X : Bounds.Max.Y;
		const float Third = (Hi - Lo) / 3.f;
		if (Team == ERSTeam::CT)
		{
			ZoneMin = Lo;
			ZoneMax = Lo + Third;
		}
		else
		{
			ZoneMin = Hi - Third;
			ZoneMax = Hi;
		}
	}

	// Случайная точка в круге часто попадает мимо карты или на крышу,
	// поэтому кандидат проверяется по трём условиям: под ним есть пол,
	// пол горизонтальный и рядом с уровнем земли, и туда влезает капсула.
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(34.f, 88.f);
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	FVector Best = FVector::ZeroVector;
	float BestDelta = FLT_MAX;

	for (int32 Attempt = 0; Attempt < 200; Attempt++)
	{
		const FVector2D Offset = FMath::RandPointInCircle(Radius * 1.4f);
		const FVector Probe(Center.X + Offset.X, Center.Y + Offset.Y, Center.Z + 8000.f);

		const float AxisValue = bAxisX ? Probe.X : Probe.Y;
		if (AxisValue < ZoneMin || AxisValue > ZoneMax)
		{
			continue; // не наша половина карты
		}

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Probe, Probe - FVector(0.f, 0.f, 20000.f), ECC_Visibility, Params))
		{
			continue; // мимо карты
		}
		if (!Hit.GetActor() || !Hit.GetActor()->Tags.Contains(TEXT("RSMap")))
		{
			continue; // не наша карта
		}
		if (Hit.ImpactNormal.Z < 0.7f)
		{
			continue; // склон или стена
		}

		const FVector Candidate = Hit.ImpactPoint + FVector(0.f, 0.f, 95.f);
		if (World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Capsule))
		{
			continue; // внутри геометрии
		}

		FHitResult Above;
		if (World->LineTraceSingleByChannel(Above, Candidate,
			Candidate + FVector(0.f, 0.f, 700.f), ECC_Visibility, Params))
		{
			continue; // над головой карта — точка под ней
		}

		// чем ближе к основному полу, тем лучше: так не попадаем ни на крышу,
		// ни под карту
		const float Delta = FMath::Abs(Hit.ImpactPoint.Z - Ground);
		if (Delta < 400.f)
		{
			return Candidate;
		}
		if (Delta < BestDelta)
		{
			BestDelta = Delta;
			Best = Candidate;
		}
	}

	if (BestDelta < FLT_MAX)
	{
		return Best;
	}
	// в зоне команды ничего не нашлось — ищем где угодно на карте
	return bTeamZone ? FindSpawnPoint(World, Team, false) : Center + FVector(0.f, 0.f, 200.f);
}

void ARSArena::SpawnBlock(const FVector& Location, const FVector& Scale, const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStaticMeshActor* Block = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, SP);
	if (!Block)
	{
		return;
	}
	Block->Tags.Add(TEXT("RSMap"));
	UStaticMeshComponent* Comp = Block->GetStaticMeshComponent();
	Comp->SetMobility(EComponentMobility::Movable);
	Comp->SetStaticMesh(CubeMesh);
	Block->SetActorScale3D(Scale);

	if (BaseMat)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, Block);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Comp->SetMaterial(0, MID);
	}
}

void ARSArena::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	const TArray<FRSMapDef>& Maps = RSMaps::All();
	const FRSMapDef& Map = Maps[FMath::Clamp(MapIndex, 0, Maps.Num() - 1)];
	PlayRadius = Map.PlayRadius;

	ClearTemplateGeometry();

	if (Map.ContentFolder)
	{
		BuildFromContent(Map.ContentFolder, Map.Scale, Map.GroundZ);
	}
	else
	{
		BuildProcedural();
	}

	SpawnLighting();

	// физика Chaos добавляет новые тела в сцену только со следующим шагом,
	// поэтому трассировать карту в этом же кадре бесполезно
	GetWorldTimerManager().SetTimer(MeasureTimer, this, &ARSArena::MeasureGroundLevel, 0.1f, false);
}

void ARSArena::ClearTemplateGeometry()
{
	// в пустом уровне-заготовке есть свой пол; после подъёма карты игроки
	// проваливались на него и оказывались под картой
	TArray<AActor*> ToRemove;
	for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
	{
		ToRemove.Add(*It);
	}
	// в редакторе уровень может быть с ландшафтом — он тоже мешает
	for (TActorIterator<ALandscapeProxy> It(GetWorld()); It; ++It)
	{
		ToRemove.Add(*It);
	}
	for (AActor* Actor : ToRemove)
	{
		Actor->Destroy();
	}
}

void ARSArena::MeasureGroundLevel()
{
	// Уровень пола = медиана высот под случайными точками карты.
	// Медиана устойчива к крышам и ямам: большая часть площади — это пол.
	UWorld* World = GetWorld();
	TArray<float> Heights;
	Heights.Reserve(200);

	// карта импортирована без простых коллизий, поэтому трассируем
	// по треугольникам меша
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	for (int32 i = 0; i < 200; i++)
	{
		const FVector2D Offset = FMath::RandPointInCircle(PlayRadius);
		const FVector Probe(MapCenter.X + Offset.X, MapCenter.Y + Offset.Y, MapCenter.Z + 8000.f);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Probe, Probe - FVector(0.f, 0.f, 20000.f), ECC_Visibility, Params)
			&& Hit.GetActor() && Hit.GetActor()->Tags.Contains(TEXT("RSMap")))
		{
			Heights.Add(Hit.ImpactPoint.Z);
		}
	}

	if (Heights.Num() > 0)
	{
		Heights.Sort();
		GroundLevel = Heights[Heights.Num() / 2];
	}
	else
	{
		GroundLevel = MapCenter.Z;
	}

	BuildSpawnZones();

	float AvgCT = 0.f, AvgT = 0.f;
	for (const FVector& P : SpawnsCT) { AvgCT += P.Z; }
	for (const FVector& P : SpawnsT) { AvgT += P.Z; }
	if (SpawnsCT.Num()) { AvgCT /= SpawnsCT.Num(); }
	if (SpawnsT.Num()) { AvgT /= SpawnsT.Num(); }

	UE_LOG(LogTemp, Log, TEXT("RageStrike: map ready, ground=%.0f, probes %d/200, spawns CT=%d (z=%.0f) T=%d (z=%.0f), separation=%.0f м"),
		GroundLevel, Heights.Num(), SpawnsCT.Num(), AvgCT, SpawnsT.Num(), AvgT,
		(SpawnsCT.Num() && SpawnsT.Num())
			? FVector::Dist(SpawnsCT[0], SpawnsT[0]) / 100.f : 0.f);
}

void ARSArena::BuildSpawnZones()
{
	// Габаритная коробка карты включает пустоту, поэтому зоны считаем
	// по реально проходимым точкам: собираем их и берём крайние группы
	// вдоль длинной стороны карты.
	UWorld* World = GetWorld();
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(34.f, 88.f);
	FCollisionQueryParams Params;
	Params.bTraceComplex = true;

	TArray<FVector> Walkable;
	Walkable.Reserve(256);

	for (int32 i = 0; i < 900; i++)
	{
		const FVector2D Offset = FMath::RandPointInCircle(PlayRadius * 1.4f);
		const FVector Probe(MapCenter.X + Offset.X, MapCenter.Y + Offset.Y, MapCenter.Z + 8000.f);

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Probe, Probe - FVector(0.f, 0.f, 20000.f), ECC_Visibility, Params))
		{
			continue;
		}
		// точка должна быть на нашей карте, а не на чужой геометрии уровня
		if (!Hit.GetActor() || !Hit.GetActor()->Tags.Contains(TEXT("RSMap")))
		{
			continue;
		}
		if (Hit.ImpactNormal.Z < 0.85f || FMath::Abs(Hit.ImpactPoint.Z - GroundLevel) > 250.f)
		{
			continue; // склон, нижняя терраса или крыша
		}

		const FVector Candidate = Hit.ImpactPoint + FVector(0.f, 0.f, 95.f);
		if (World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, ECC_Pawn, Capsule))
		{
			continue;
		}

		// над точкой должно быть открыто: если сверху нависает карта,
		// значит точка под ней, а не на игровой площадке
		FHitResult Above;
		if (World->LineTraceSingleByChannel(Above, Candidate,
			Candidate + FVector(0.f, 0.f, 700.f), ECC_Visibility, Params))
		{
			continue;
		}
		Walkable.Add(Candidate);
	}

	SpawnsCT.Reset();
	SpawnsT.Reset();
	if (Walkable.Num() < 8)
	{
		return;
	}

	// Отсекаем внешнее кольцо модели (песок за стенами): оставляем точки
	// в центральных 90% облака по обеим осям.
	{
		TArray<float> Xs, Ys;
		Xs.Reserve(Walkable.Num());
		Ys.Reserve(Walkable.Num());
		for (const FVector& P : Walkable)
		{
			Xs.Add(P.X);
			Ys.Add(P.Y);
		}
		Xs.Sort();
		Ys.Sort();
		const int32 Lo = Walkable.Num() * 5 / 100;
		const int32 Hi = Walkable.Num() - 1 - Lo;
		const float MinX = Xs[Lo], MaxX = Xs[Hi];
		const float MinY = Ys[Lo], MaxY = Ys[Hi];

		Walkable.RemoveAll([&](const FVector& P)
		{
			return P.X < MinX || P.X > MaxX || P.Y < MinY || P.Y > MaxY;
		});
	}

	if (Walkable.Num() < 8)
	{
		return;
	}

	NavPoints = Walkable;

	const FVector Size = MapBounds.GetSize();
	const bool bAxisX = Size.X > Size.Y;
	Walkable.Sort([bAxisX](const FVector& A, const FVector& B)
	{
		return (bAxisX ? A.X : A.Y) < (bAxisX ? B.X : B.Y);
	});

	// Самые крайние точки — это внешняя окантовка модели за стенами карты,
	// поэтому берём полосы внутри: 10–28% и 72–90% от края.
	const int32 Num = Walkable.Num();
	const int32 LoStart = FMath::Max(1, Num * 10 / 100);
	const int32 LoEnd = FMath::Max(LoStart + 1, Num * 28 / 100);
	const int32 HiStart = FMath::Min(Num - 2, Num * 72 / 100);
	const int32 HiEnd = FMath::Min(Num - 1, Num * 90 / 100);

	for (int32 i = LoStart; i <= LoEnd; i++)
	{
		SpawnsCT.Add(Walkable[i]);
	}
	for (int32 i = HiStart; i <= HiEnd; i++)
	{
		SpawnsT.Add(Walkable[i]);
	}
}

void ARSArena::BuildFromContent(const TCHAR* Folder, float Scale, float GroundZ)
{
	UWorld* World = GetWorld();

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	IAssetRegistry& Registry = ARM.Get();

	// В редакторе реестр на старте ещё пуст и его нужно досканировать.
	// В собранной игре данные уже пришли из кука, а пересканирование папки
	// (файлов на диске нет, всё в pak) наоборот стирает записи — и карта
	// «не находится», из-за чего строилась запасная арена.
#if WITH_EDITOR
	TArray<FString> ScanPaths;
	ScanPaths.Add(Folder);
	Registry.ScanPathsSynchronous(ScanPaths, /*bForceRescan*/ true);
#endif

	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(FName(Folder), Assets, /*bRecursive*/ true);

	if (Assets.Num() == 0)
	{
		Registry.SearchAllAssets(/*bSynchronous*/ true);
		Registry.GetAssetsByPath(FName(Folder), Assets, /*bRecursive*/ true);
	}

	UE_LOG(LogTemp, Log, TEXT("RageStrike: map '%s' — found %d assets"), Folder, Assets.Num());

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	FBox Bounds(ForceInit);
	TArray<AActor*> Pieces;

	for (const FAssetData& Data : Assets)
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(Data.GetAsset());
		if (!Mesh)
		{
			continue;
		}

		AStaticMeshActor* Piece = World->SpawnActor<AStaticMeshActor>(FVector::ZeroVector, FRotator::ZeroRotator, SP);
		if (!Piece)
		{
			continue;
		}
		Piece->Tags.Add(TEXT("RSMap"));
		UStaticMeshComponent* Comp = Piece->GetStaticMeshComponent();
		Comp->SetMobility(EComponentMobility::Movable);
		Comp->SetStaticMesh(Mesh);
		Piece->SetActorScale3D(FVector(Scale));
		Comp->SetCollisionProfileName(TEXT("BlockAll"));
		// без этого физическое тело появится только в следующем кадре,
		// и трассировки по карте в этом же кадре ничего не находят
		Comp->RecreatePhysicsState();

		Bounds += Piece->GetComponentsBoundingBox(true);
		Pieces.Add(Piece);
	}

	if (Pieces.Num() == 0 || !Bounds.IsValid)
	{
		// карта не загрузилась — не оставляем игрока в пустоте
		UE_LOG(LogTemp, Error, TEXT("RSDIAG map '%s' not found (%d assets), fallback to arena"),
			Folder, Assets.Num());
		BuildProcedural();
		return;
	}

	// сажаем карту низом на заданную высоту: у скачанных моделей начало
	// координат где угодно, и половина геометрии оказывается под уровнем пола
	const float Lift = GroundZ - Bounds.Min.Z;
	if (!FMath::IsNearlyZero(Lift))
	{
		for (AActor* Piece : Pieces)
		{
			Piece->AddActorWorldOffset(FVector(0.f, 0.f, Lift));
		}
		Bounds = Bounds.ShiftBy(FVector(0.f, 0.f, Lift));
	}

	MapCenter = FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Min.Z);
	MapBounds = Bounds;
}

void ARSArena::BuildProcedural()
{
	FRandomStream RS(Seed);

	// пол 80x80 м
	SpawnBlock(FVector(0.f, 0.f, -48.f), FVector(80.f, 80.f, 1.f), FLinearColor(0.16f, 0.16f, 0.18f));

	// стены по периметру
	SpawnBlock(FVector(0.f, 4000.f, 250.f), FVector(80.f, 1.f, 6.f), FLinearColor(0.35f, 0.33f, 0.28f));
	SpawnBlock(FVector(0.f, -4000.f, 250.f), FVector(80.f, 1.f, 6.f), FLinearColor(0.35f, 0.33f, 0.28f));
	SpawnBlock(FVector(4000.f, 0.f, 250.f), FVector(1.f, 80.f, 6.f), FLinearColor(0.35f, 0.33f, 0.28f));
	SpawnBlock(FVector(-4000.f, 0.f, 250.f), FVector(1.f, 80.f, 6.f), FLinearColor(0.35f, 0.33f, 0.28f));

	// ящики-укрытия (детерминированно от Seed — одинаковы у хоста и клиентов)
	for (int32 i = 0; i < 18; i++)
	{
		float X, Y;
		do
		{
			X = RS.FRandRange(-3300.f, 3300.f);
			Y = RS.FRandRange(-3300.f, 3300.f);
		}
		while (FVector2D(X, Y).Size() < 800.f);

		const float S = RS.FRandRange(1.5f, 3.5f);
		const FLinearColor CrateColor(
			RS.FRandRange(0.4f, 0.6f),
			RS.FRandRange(0.3f, 0.45f),
			RS.FRandRange(0.15f, 0.25f));
		SpawnBlock(FVector(X, Y, S * 50.f), FVector(S, S, S), CrateColor);
	}

	MapCenter = FVector::ZeroVector;
}

void ARSArena::SpawnLighting()
{
	UWorld* World = GetWorld();

	// свет и небо — только если в уровне их ещё нет
	if (TActorIterator<ADirectionalLight>(World))
	{
		return;
	}

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(
		FVector(0.f, 0.f, 1000.f), FRotator(-55.f, 40.f, 0.f), SP);
	if (Sun)
	{
		Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
	}

	if (AActor* SkyActor = World->SpawnActor<AActor>(AActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP))
	{
		USkyAtmosphereComponent* Atmo = NewObject<USkyAtmosphereComponent>(SkyActor);
		SkyActor->SetRootComponent(Atmo);
		Atmo->RegisterComponent();
	}

	ASkyLight* Sky = World->SpawnActor<ASkyLight>(FVector(0.f, 0.f, 500.f), FRotator::ZeroRotator, SP);
	if (Sky)
	{
		USkyLightComponent* SkyComp = Cast<USkyLightComponent>(Sky->GetLightComponent());
		SkyComp->SetMobility(EComponentMobility::Movable);
		SkyComp->bRealTimeCapture = true;
		SkyComp->MarkRenderStateDirty();
	}
}
