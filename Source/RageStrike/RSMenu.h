#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ARSPlayerController;
class SEditableTextBox;

// Меню в стиле CS2: верхняя панель вкладок, контент по вкладкам,
// карточка игрока снизу. Работает и как стартовое, и как пауза (Esc).
class SRSMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRSMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ARSPlayerController>, OwnerPC)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Esc ловит само меню, а не InputComponent контроллера: под меню стоит
	// режим UIOnly, и клавиши до привязок контроллера не доходят вовсе.
	// Фокус приходит из OpenMenu через SetWidgetToFocus, но принять его
	// SCompoundWidget может, только если разрешён фокус с клавиатуры.
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	TWeakObjectPtr<ARSPlayerController> PC;
	TSharedPtr<SEditableTextBox> IPBox;
	TSharedPtr<SEditableTextBox> NickBox;

	// вкладки: 0 играть, 1 настройки, 2 снаряжение, 3 новости, 4 заглушка
	int32 ActiveTab = 0;

	// правила поменяли в меню — сразу применяем их к текущему матчу
	void PushRules();

	TSharedRef<SWidget> MakeTopBar();
	TSharedRef<SWidget> MakeTab(const FText& Label, int32 TabIndex);
	TSharedRef<SWidget> MakePlayPanel();
	// внутри вкладки «Играть»: выбор режима, тренировка, сеть
	TSharedRef<SWidget> MakeTrainingPanel();
	TSharedRef<SWidget> MakeNetworkPanel();
	int32 PlaySection = 0;
	TSharedRef<SWidget> MakeSettingsPanel();
	// подразделы настроек: изображение / управление / прицел
	TSharedRef<SWidget> MakeSettingsSubTab(const FText& Label, int32 Index);
	TSharedRef<SWidget> MakeVideoPanel();
	TSharedRef<SWidget> MakeBindsPanel();
	TSharedRef<SWidget> MakeCrosshairPanel();
	TSharedRef<SWidget> MakeCrosshairPreview();
	TSharedRef<SWidget> MakeChannelRow(int32 Which);
	int32 SettingsSection = 0;
	int32 CapturingBind = INDEX_NONE; // какая строка сейчас ловит клавишу
	TSharedRef<SWidget> MakeArsenalPanel();
	TSharedRef<SWidget> MakeNewsPanel();
	// инвентарь: набор на раунд и автозакупка
	TSharedRef<SWidget> MakeInventoryPanel();
	TSharedRef<SWidget> MakeLoadoutRow(const FString& Label, bool bPrimary);
	TSharedRef<SWidget> MakePlaceholderPanel();
	TSharedRef<SWidget> MakePlayerCard();

	// helpers
	FText GetMapText() const;
	void CycleMap(int32 Dir);
	FText GetQualityText() const;
	void CycleQuality(int32 Dir);
	FText GetWindowModeText() const;
	void CycleWindowMode();
	FText GetResolutionText() const;
	void CycleResolution(int32 Dir);
	FText GetVSyncText() const;
	void ToggleVSync();

	TSharedRef<SWidget> MakeButton(const FText& Label, TFunction<void()> OnClick);
	TSharedRef<SWidget> MakeCycleRow(const FText& Label, TAttribute<FText> Value,
		TFunction<void()> OnPrev, TFunction<void()> OnNext);
};
