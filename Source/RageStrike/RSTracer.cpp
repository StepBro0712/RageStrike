#include "RSTracer.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

ARSTracer::ARSTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;   // спавнится локально на каждой машине
	SetActorEnableCollision(false);

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cyl(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cyl.Succeeded()) { CylinderMesh = Cyl.Object; }

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sph(
		TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sph.Succeeded()) { SphereMesh = Sph.Object; }

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(
		TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (Mat.Succeeded()) { BaseMat = Mat.Object; }

	auto MakePart = [&](const TCHAR* Name, UStaticMesh* Mesh) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Comp = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Comp->SetupAttachment(Root);
		Comp->SetStaticMesh(Mesh);
		Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Comp->SetCastShadow(false);
		// трассер не должен ловить свет: он сам источник яркости
		Comp->bReceivesDecals = false;
		return Comp;
	};

	Beam = MakePart(TEXT("Beam"), CylinderMesh);
	Muzzle = MakePart(TEXT("Muzzle"), SphereMesh);
	Impact = MakePart(TEXT("Impact"), SphereMesh);
}

void ARSTracer::Show(const FVector& Start, const FVector& End, bool bBigCaliber)
{
	const FVector Delta = End - Start;
	const float Length = Delta.Size();
	if (Length < 1.f)
	{
		Destroy();
		return;
	}

	SetActorLocation(Start);
	Duration = bBigCaliber ? 0.12f : 0.08f;

	// цилиндр движка высотой 100 и радиусом 50: тянем по Z и ужимаем в нитку
	const float Thickness = bBigCaliber ? 0.032f : 0.02f;
	Beam->SetWorldLocation(Start + Delta * 0.5f);
	Beam->SetWorldRotation(Delta.Rotation() + FRotator(90.f, 0.f, 0.f));
	Beam->SetWorldScale3D(FVector(Thickness, Thickness, Length / 100.f));

	Muzzle->SetWorldLocation(Start);
	Muzzle->SetWorldScale3D(FVector(bBigCaliber ? 0.22f : 0.14f));

	Impact->SetWorldLocation(End);
	Impact->SetWorldScale3D(FVector(bBigCaliber ? 0.14f : 0.09f));

	if (BaseMat)
	{
		// жёлто-белая трасса и оранжевая вспышка
		UMaterialInstanceDynamic* BeamMat = UMaterialInstanceDynamic::Create(BaseMat, this);
		BeamMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(6.f, 5.2f, 2.2f));
		Beam->SetMaterial(0, BeamMat);

		UMaterialInstanceDynamic* FlashMat = UMaterialInstanceDynamic::Create(BaseMat, this);
		FlashMat->SetVectorParameterValue(TEXT("Color"), FLinearColor(9.f, 6.f, 1.5f));
		Muzzle->SetMaterial(0, FlashMat);
		Impact->SetMaterial(0, FlashMat);
	}

	SetLifeSpan(Duration + 0.02f);
}

void ARSTracer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Life += DeltaTime;
	const float Alpha = FMath::Clamp(1.f - Life / Duration, 0.f, 1.f);

	// вспышка у дула гаснет быстрее самой трассы
	const float MuzzleAlpha = FMath::Clamp(1.f - Life / (Duration * 0.35f), 0.f, 1.f);
	Muzzle->SetWorldScale3D(FVector(0.22f * MuzzleAlpha));

	// трасса истончается, будто улетает
	FVector BeamScale = Beam->GetComponentScale();
	BeamScale.X = BeamScale.Y = FMath::Max(0.002f, 0.02f * Alpha);
	Beam->SetWorldScale3D(BeamScale);

	Impact->SetWorldScale3D(FVector(FMath::Max(0.001f, 0.12f * Alpha)));
}

void ARSTracer::Spawn(UWorld* World, const FVector& Start, const FVector& End, bool bBigCaliber)
{
	if (!World)
	{
		return;
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SP.ObjectFlags |= RF_Transient;
	if (ARSTracer* Tracer = World->SpawnActor<ARSTracer>(ARSTracer::StaticClass(),
		Start, FRotator::ZeroRotator, SP))
	{
		Tracer->Show(Start, End, bBigCaliber);
	}
}
