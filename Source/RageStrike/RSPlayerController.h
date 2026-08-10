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

	bool IsStartupMenu() const { return bStartupMenu; }

	void SaveUserFloat(const TCHAR* Key, float Value);

private:
	TSharedPtr<SRSMenu> MenuWidget;
	TSharedPtr<SWidget> MenuContainer;
	bool bMenuOpen = false;
	bool bStartupMenu = false;
};
