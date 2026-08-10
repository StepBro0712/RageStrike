#include "RSPlayerController.h"
#include "RSMenu.h"
#include "RSCharacter.h"
#include "RSMenuCamera.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWeakWidget.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformProcess.h"

namespace
{
	// стартовое меню показываем один раз за запуск процесса,
	// а не после каждого перехода (Host/Join делают travel на новую карту)
	bool GStartupMenuShown = false;
}

void ARSPlayerController::BeginPlay()
{
	Super::BeginPlay();

	float Value = 1.f;
	if (GConfig->GetFloat(TEXT("RageStrike"), TEXT("MouseSens"), Value, GGameUserSettingsIni))
	{
		MouseSens = FMath::Clamp(Value, 0.1f, 3.f);
	}
	if (GConfig->GetFloat(TEXT("RageStrike"), TEXT("Volume"), Value, GGameUserSettingsIni))
	{
		FApp::SetVolumeMultiplier(FMath::Clamp(Value, 0.f, 1.f));
	}

	// ник: сохранённый или имя пользователя Windows как заготовка
	FString Nick;
	if (!GConfig->GetString(TEXT("RageStrike"), TEXT("Nick"), Nick, GGameUserSettingsIni) || Nick.IsEmpty())
	{
		Nick = FPlatformProcess::UserName();
	}
	SetPlayerNick(Nick);

	// -nomenu пропускает стартовое меню: с ним игра стоит на паузе,
	// и автоматические проверки не доходят до матча
	if (FParse::Param(FCommandLine::Get(), TEXT("nomenu")))
	{
		GStartupMenuShown = true;
	}

	if (!GStartupMenuShown && IsLocalController())
	{
		GStartupMenuShown = true;
		OpenMenu(true);
	}
}

void ARSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// прямые привязки: не зависят от ActionMappings в конфиге
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ARSPlayerController::ToggleMenu);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ARSPlayerController::ToggleMenu);
}

void ARSPlayerController::SaveUserFloat(const TCHAR* Key, float Value)
{
	GConfig->SetFloat(TEXT("RageStrike"), Key, Value, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void ARSPlayerController::SetPlayerNick(const FString& NewNick)
{
	FString Clean = NewNick.TrimStartAndEnd().Left(16);
	if (Clean.IsEmpty())
	{
		Clean = TEXT("Игрок");
	}
	PlayerNick = Clean;

	GConfig->SetString(TEXT("RageStrike"), TEXT("Nick"), *PlayerNick, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);

	// пешка везёт ник на сервер: killfeed и таблица собираются там
	if (ARSCharacter* RSPawn = Cast<ARSCharacter>(GetPawn()))
	{
		RSPawn->ApplyNick(PlayerNick);
	}
}

void ARSPlayerController::ToggleMenu()
{
	if (bMenuOpen)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu(false);
	}
}

void ARSPlayerController::OpenMenu(bool bStartup)
{
	if (bMenuOpen || !GEngine || !GEngine->GameViewport)
	{
		return;
	}

	bStartupMenu = bStartup;

	// пересоздаём, чтобы кнопки соответствовали режиму (стартовое меню / пауза)
	MenuWidget.Reset();
	SAssignNew(MenuWidget, SRSMenu).OwnerPC(this);

	GEngine->GameViewport->AddViewportWidgetContent(
		SAssignNew(MenuContainer, SWeakWidget).PossiblyNullContent(MenuWidget.ToSharedRef()), 100);

	bShowMouseCursor = true;
	FInputModeGameAndUI Mode;
	Mode.SetWidgetToFocus(MenuWidget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	Mode.SetHideCursorDuringCapture(false);
	SetInputMode(Mode);

	ShowMenuCamera();

	// в одиночной игре ставим паузу, чтобы боты не расстреляли в меню
	if (GetNetMode() == NM_Standalone)
	{
		UGameplayStatics::SetGamePaused(this, true);
	}

	bMenuOpen = true;
}

void ARSPlayerController::ShowMenuCamera()
{
	if (!IsLocalController())
	{
		return;
	}

	if (!IsValid(MenuCamera))
	{
		FActorSpawnParameters SP;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SP.ObjectFlags |= RF_Transient;
		MenuCamera = GetWorld()->SpawnActor<ARSMenuCamera>(
			ARSMenuCamera::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP);
	}
	if (!IsValid(MenuCamera))
	{
		return;
	}

	// запоминаем, куда смотрели: живой игрок, наблюдаемый союзник или своё тело
	AActor* Current = GetViewTarget();
	if (Current != MenuCamera)
	{
		ViewTargetBeforeMenu = Current;
	}
	SetViewTargetWithBlend(MenuCamera, 0.5f);
}

void ARSPlayerController::RestoreGameView()
{
	if (GetViewTarget() != MenuCamera)
	{
		return;
	}
	AActor* Back = IsValid(ViewTargetBeforeMenu) ? ViewTargetBeforeMenu.Get() : Cast<AActor>(GetPawn());
	if (Back)
	{
		SetViewTargetWithBlend(Back, 0.35f);
	}
}

void ARSPlayerController::CloseMenu()
{
	if (GEngine && GEngine->GameViewport && MenuContainer.IsValid())
	{
		GEngine->GameViewport->RemoveViewportWidgetContent(MenuContainer.ToSharedRef());
	}

	if (GetNetMode() == NM_Standalone)
	{
		UGameplayStatics::SetGamePaused(this, false);
	}

	RestoreGameView();

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	bMenuOpen = false;
	bStartupMenu = false;
}

void ARSPlayerController::AllowStartupMenu()
{
	GStartupMenuShown = false;
}

void ARSPlayerController::SelectTeam(bool bCT)
{
	if (ARSCharacter* RSPawn = Cast<ARSCharacter>(GetPawn()))
	{
		RSPawn->SetTeam(bCT ? ERSTeam::CT : ERSTeam::T);
	}
}

bool ARSPlayerController::IsTeamCT() const
{
	const ARSCharacter* RSPawn = Cast<ARSCharacter>(GetPawn());
	return !RSPawn || RSPawn->Team == ERSTeam::CT;
}

void ARSPlayerController::ReloadLevel()
{
	CloseMenu();
	ConsoleCommand(GetNetMode() == NM_ListenServer
		? TEXT("open /Engine/Maps/Entry?listen")
		: TEXT("open /Engine/Maps/Entry"));
}

void ARSPlayerController::HostGame()
{
	CloseMenu();
	ConsoleCommand(TEXT("open /Engine/Maps/Entry?listen"));
}

void ARSPlayerController::JoinGame(const FString& IP)
{
	const FString Clean = IP.TrimStartAndEnd();
	if (Clean.IsEmpty())
	{
		return;
	}
	CloseMenu();
	ConsoleCommand(FString::Printf(TEXT("open %s"), *Clean));
}

void ARSPlayerController::QuitGame()
{
	ConsoleCommand(TEXT("quit"));
}
