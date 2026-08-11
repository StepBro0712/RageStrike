#include "RSPlayerController.h"
#include "RSMenu.h"
#include "RSCharacter.h"
#include "RSMenuCamera.h"
#include "RSGameMode.h"
#include "RSAudio.h"
#include "RSMatchSettings.h"
#include "Components/AudioComponent.h"
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

	// игра запускается в лобби: персонаж стоит, матч не идёт
	bool GLobbyMode = true;
}

bool ARSPlayerController::IsLobby()
{
	return GLobbyMode;
}

void ARSPlayerController::SetLobby(bool bValue)
{
	GLobbyMode = bValue;
}

void ARSPlayerController::EnterLobby()
{
	// возврат в лобби — это перезапуск уровня: матч должен закончиться,
	// боты исчезнуть, счёт обнулиться
	GLobbyMode = true;
	AllowStartupMenu();
	ReloadLevel();
}

void ARSPlayerController::StartMatch()
{
	GLobbyMode = false;
	ReloadLevel();
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
		RSAudio::SetMasterVolume(FMath::Clamp(Value, 0.f, 1.f));
	}

	// яркость держит движок, а не GameUserSettings — ставим её при входе
	RSOptions::ApplyGamma();

	// сторона из настроек: пешка каждый раз создаётся заново и по умолчанию CT
	if (ARSCharacter* RSPawn = Cast<ARSCharacter>(GetPawn()))
	{
		RSPawn->SetTeam(RSMatch::GetPlayerIsCT() ? ERSTeam::CT : ERSTeam::T);
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

	// В лобби смотрим на своего персонажа со стороны: он стоит на карте,
	// а меню открыто поверх. Вид от третьего лица включаем сами, потому что
	// от первого в лобби видно только руки.
	if (GLobbyMode && IsLocalController())
	{
		if (ARSCharacter* RSPawn = Cast<ARSCharacter>(GetPawn()))
		{
			RSPawn->SetThirdPerson(true);
		}
	}
}

void ARSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// Прямые привязки: не зависят от ActionMappings в конфиге.
	// bExecuteWhenPaused обязателен — иначе на стартовом меню, где стоит пауза,
	// Esc не доходит до контроллера и меню не закрывается.
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this,
		&ARSPlayerController::ToggleMenu).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::P, IE_Pressed, this,
		&ARSPlayerController::ToggleMenu).bExecuteWhenPaused = true;
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

void ARSPlayerController::ApplyMatchSettingsNow()
{
	if (ARSGameMode* GM = GetWorld()->GetAuthGameMode<ARSGameMode>())
	{
		GM->ApplyMatchSettings();
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
	// Только интерфейс: при GameAndUI мышь продолжала крутить камеру,
	// а WASD уходили в персонажа прямо во время меню.
	FInputModeUIOnly Mode;
	Mode.SetWidgetToFocus(MenuWidget);
	Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(Mode);

	// персонаж перестаёт слушать клавиши и мышь, пока меню открыто
	SetIgnoreMoveInput(true);
	SetIgnoreLookInput(true);

	// Облётная камера и пауза — только для стартового меню. По Esc в бою
	// игра продолжается, а вид остаётся от лица персонажа.
	// В лобби облётная камера не нужна: там своя, наведённая на персонажа,
	// и облёт просто перехватывал вид, из-за чего лобби было не видно.
	if (bStartup && !GLobbyMode)
	{
		ShowMenuCamera();
	}

	if (!MenuMusic)
	{
		MenuMusic = UGameplayStatics::SpawnSound2D(this,
			RSAudio::Get(RSAudio::ESound::MusicMenu), 0.45f * RSOptions::GetMusicVolume(), 1.f, 0.f, nullptr, false, false);
	}

	// Паузу ставим только на стартовом меню и только вне лобби: на паузе
	// не тикают таймеры мира, поэтому расстановка лобби никогда не
	// срабатывала — персонажа на площадку никто не ставил.
	if (bStartup && !GLobbyMode && GetNetMode() == NM_Standalone)
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

	if (MenuMusic)
	{
		MenuMusic->Stop();
		MenuMusic->DestroyComponent();
		MenuMusic = nullptr;
	}

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
	SetIgnoreMoveInput(false);
	SetIgnoreLookInput(false);
	bMenuOpen = false;
	bStartupMenu = false;
}

void ARSPlayerController::AllowStartupMenu()
{
	GStartupMenuShown = false;
}

void ARSPlayerController::SelectTeam(bool bCT)
{
	// Запоминаем выбор в настройках: старт матча перезапускает уровень,
	// и сторона, живущая только на пешке, до матча не доезжала — игрок
	// выбирал T, а появлялся за CT.
	RSMatch::SetPlayerIsCT(bCT);

	if (ARSCharacter* RSPawn = Cast<ARSCharacter>(GetPawn()))
	{
		RSPawn->SetTeam(bCT ? ERSTeam::CT : ERSTeam::T);
	}
}

bool ARSPlayerController::IsTeamCT() const
{
	return RSMatch::GetPlayerIsCT();
}

void ARSPlayerController::ReloadLevel()
{
	// В лобби уровень перезагружается ради смены карты, и меню должно
	// открыться снова: оно показывается один раз за запуск, поэтому без
	// этого выглядело так, будто выбор карты сразу запускает матч.
	if (GLobbyMode)
	{
		AllowStartupMenu();
	}

	CloseMenu();
	ConsoleCommand(GetNetMode() == NM_ListenServer
		? TEXT("open /Engine/Maps/Entry?listen")
		: TEXT("open /Engine/Maps/Entry"));
}

void ARSPlayerController::HostGame()
{
	// Сервер поднимается сразу в матч: иначе подключившийся попадал в лобби,
	// где нет ни раундов, ни ботов, и игра выглядела сломанной.
	GLobbyMode = false;
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
	GLobbyMode = false;
	CloseMenu();
	ConsoleCommand(FString::Printf(TEXT("open %s"), *Clean));
}

void ARSPlayerController::QuitGame()
{
	ConsoleCommand(TEXT("quit"));
}
