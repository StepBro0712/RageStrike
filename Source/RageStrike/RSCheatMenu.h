#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ARSCharacter;
class SWidgetSwitcher;

// Меню читов в стиле Neverlose: слева разделы, справа две колонки панелей.
// На канвасе такое не собрать — там нет ни слайдеров, ни выпадающих списков,
// поэтому оверлей переехал на Slate целиком.
class SRSCheatMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRSCheatMenu) {}
		SLATE_ARGUMENT(TWeakObjectPtr<ARSCharacter>, Owner)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// меню держит фокус, иначе клавиши закрытия до него не доходят
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

private:
	TWeakObjectPtr<ARSCharacter> Owner;
	int32 Page = 0;

	// Строки настроек привязываются к полям персонажа указателем на член:
	// так каждая строка — одна строчка кода, без десятков лямбд-двойников.
	TSharedRef<SWidget> MakeToggle(const FString& Label, bool ARSCharacter::* Field, bool bSync = false);
	TSharedRef<SWidget> MakeSlider(const FString& Label, float ARSCharacter::* Field,
		float Min, float Max, const FString& Suffix, bool bSync = false);
	TSharedRef<SWidget> MakeIntSlider(const FString& Label, int32 ARSCharacter::* Field,
		int32 Min, int32 Max, const FString& Suffix);
	TSharedRef<SWidget> MakeCombo(const FString& Label, int32 ARSCharacter::* Field,
		const TArray<FString>& Items, bool bSync = false);

	// верхняя панель: выбор конфига, поле имени и кнопки
	TSharedRef<SWidget> MakeConfigBar();
	void RefreshConfigs();
	TArray<TSharedPtr<FString>> ConfigNames;
	TSharedPtr<class SEditableTextBox> NameBox;
	TSharedPtr<class SComboBox<TSharedPtr<FString>>> ConfigCombo;

	TSharedRef<SWidget> MakeSection(const FString& Title, TSharedRef<SWidget> Rows);
	TSharedRef<SWidget> MakeSideButton(const FString& Label, int32 Index);
	TSharedRef<SWidget> MakeRagePage();
	TSharedRef<SWidget> MakeLegitPage();
	TSharedRef<SWidget> MakeVisualsPage();
	TSharedRef<SWidget> MakeMiscPage();

	void Sync();
	int32 GetPageIndex() const { return Page; }

	TSharedPtr<SWidgetSwitcher> Pages;
	// списки выпадающих меню живут столько же, сколько меню: SComboBox
	// держит на источник слабую ссылку и упадёт, если тот умрёт раньше
	TArray<TSharedPtr<TArray<TSharedPtr<FString>>>> ComboLists;
};
