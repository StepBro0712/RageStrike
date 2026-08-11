#include "RageStrike.h"
#include "RSLoadingScreen.h"
#include "Modules/ModuleManager.h"

// Свой модуль вместо стандартного: нужен, чтобы подписаться на загрузку
// карты и показать экран загрузки с логотипом.
class FRageStrikeModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		RSLoadingScreen::Register();
	}

	virtual void ShutdownModule() override
	{
		RSLoadingScreen::Unregister();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FRageStrikeModule, RageStrike, "RageStrike");
