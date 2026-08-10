#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class ARSPlayerController;
class SEditableTextBox;

// Игровое меню (Esc): настройки графики/звука/мыши, LAN хост/подключение.
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
