#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RSPlayerController.generated.h"

class SRSMenu;
class SWidget;

UCLASS()
class RAGESTRIKE_API ARSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	void ToggleMenu();
	void OpenMenu(bool bStartup);
	void CloseMenu();

	// после матча стартовое меню нужно показать снова
	static void AllowStartupMenu();

	void SelectTeam(bool bCT);
	bool IsTeamCT() const;

	void ReloadLevel();
	void HostGame();
	void JoinGame(const FString& IP);
	void QuitGame();

	// чувствительность мыши (читается персонажем)
	float MouseSens = 1.f;

	// ник игрока: показывается в таблице и killfeed, хранится в конфиге
	FString PlayerNick;
	void SetPlayerNick(const FString& NewNick);

	bool IsStartupMenu() const { return bStartupMenu; }
	// HUD прячется, пока открыто меню — иначе радар и патроны лезут поверх
	bool IsMenuOpen() const { return bMenuOpen; }

	void SaveUserFloat(const TCHAR* Key, float Value);

	// хост поменял правила в меню — применяем их к текущему матчу
	void ApplyMatchSettingsNow();

	// Лобби: матч не идёт, персонаж стоит перед камерой, открыто меню.
	// Флаг статический, потому что переход в лобби и обратно — перезагрузка
	// уровня, и обычное поле её бы не пережило.
	static bool IsLobby();
	static void SetLobby(bool bValue);
	void EnterLobby();   // из матча в лобби
	void StartMatch();   // из лобби в матч

private:
	// облётная камера меню и то, куда вернуть взгляд при закрытии
	void ShowMenuCamera();
	void RestoreGameView();

	UPROPERTY()
	TObjectPtr<class ARSMenuCamera> MenuCamera = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> ViewTargetBeforeMenu = nullptr;

	// музыка меню: живёт пока меню открыто
	UPROPERTY()
	TObjectPtr<class UAudioComponent> MenuMusic = nullptr;

	TSharedPtr<SRSMenu> MenuWidget;
	TSharedPtr<SWidget> MenuContainer;
	bool bMenuOpen = false;
	bool bStartupMenu = false;
};
