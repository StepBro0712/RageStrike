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

private:
	TWeakObjectPtr<ARSPlayerController> PC;
	TSharedPtr<SEditableTextBox> IPBox;
	TSharedPtr<SEditableTextBox> NickBox;

	// вкладки: 0 играть, 1 настройки, 2 снаряжение, 3 новости, 4 заглушка
	int32 ActiveTab = 0;

	TSharedRef<SWidget> MakeTopBar();
	TSharedRef<SWidget> MakeTab(const FText& Label, int32 TabIndex);
	TSharedRef<SWidget> MakePlayPanel();
	TSharedRef<SWidget> MakeSettingsPanel();
	TSharedRef<SWidget> MakeArsenalPanel();
	TSharedRef<SWidget> MakeNewsPanel();
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
