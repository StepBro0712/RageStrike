#include "RSMenu.h"
#include "RSPlayerController.h"
#include "RSMaps.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SEditableTextBox.h"

static const FIntPoint GResolutions[] = { {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440} };

static UGameUserSettings* GetUS()
{
	return GEngine ? GEngine->GetGameUserSettings() : nullptr;
}

TSharedRef<SWidget> SRSMenu::MakeButton(const FText& Label, TFunction<void()> OnClick)
{
	return SNew(SButton)
		.HAlign(HAlign_Center)
		.ContentPadding(FMargin(0.f, 8.f))
		.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			.Text(Label)
		];
}

TSharedRef<SWidget> SRSMenu::MakeCycleRow(const FText& Label, TAttribute<FText> Value,
	TFunction<void()> OnPrev, TFunction<void()> OnNext)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text(Label)
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			SNew(SButton).Text(FText::FromString(TEXT("<")))
			.OnClicked_Lambda([OnPrev]() { OnPrev(); return FReply::Handled(); })
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).MinDesiredWidth(150.f).HAlign(HAlign_Center)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13)).Text(Value)
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.f, 0.f)
		[
			SNew(SButton).Text(FText::FromString(TEXT(">")))
			.OnClicked_Lambda([OnNext]() { OnNext(); return FReply::Handled(); })
		];
}

FText SRSMenu::GetMapText() const
{
	return FText::FromString(RSMaps::Selected().DisplayName);
}

void SRSMenu::CycleMap(int32 Dir)
{
	const int32 Count = RSMaps::All().Num();
	const int32 Next = (RSMaps::GetSelectedIndex() + Dir + Count) % Count;
	RSMaps::SetSelectedIndex(Next);

	// карта строится при старте уровня — перезапускаем его
	if (PC.IsValid())
	{
		PC->ReloadLevel();
	}
}

FText SRSMenu::GetQualityText() const
{
	static const TCHAR* Names[] = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };
	int32 Q = 2;
	if (UGameUserSettings* S = GetUS())
	{
		Q = FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 3);
	}
	return FText::FromString(Names[Q]);
}

void SRSMenu::CycleQuality(int32 Dir)
{
	if (UGameUserSettings* S = GetUS())
	{
		const int32 Q = FMath::Clamp(FMath::Clamp(S->GetOverallScalabilityLevel(), 0, 3) + Dir, 0, 3);
		S->SetOverallScalabilityLevel(Q);
		S->ApplySettings(false);
		S->SaveSettings();
	}
}

FText SRSMenu::GetWindowModeText() const
{
	if (UGameUserSettings* S = GetUS())
	{
		switch (S->GetFullscreenMode())
		{
		case EWindowMode::Fullscreen:         return FText::FromString(TEXT("Fullscreen"));
		case EWindowMode::WindowedFullscreen: return FText::FromString(TEXT("Borderless"));
		default:                              return FText::FromString(TEXT("Windowed"));
		}
	}
	return FText::GetEmpty();
}

void SRSMenu::CycleWindowMode()
{
	if (UGameUserSettings* S = GetUS())
	{
		EWindowMode::Type Mode = S->GetFullscreenMode();
		Mode = (Mode == EWindowMode::Fullscreen) ? EWindowMode::WindowedFullscreen
		     : (Mode == EWindowMode::WindowedFullscreen) ? EWindowMode::Windowed
		     : EWindowMode::Fullscreen;
		S->SetFullscreenMode(Mode);
		S->ApplySettings(false);
		S->SaveSettings();
	}
}

FText SRSMenu::GetResolutionText() const
{
	if (UGameUserSettings* S = GetUS())
	{
		const FIntPoint R = S->GetScreenResolution();
		return FText::FromString(FString::Printf(TEXT("%dx%d"), R.X, R.Y));
	}
	return FText::GetEmpty();
}

void SRSMenu::CycleResolution(int32 Dir)
{
	UGameUserSettings* S = GetUS();
	if (!S)
	{
		return;
	}
	const FIntPoint Cur = S->GetScreenResolution();
	int32 Idx = 2;
	for (int32 i = 0; i < UE_ARRAY_COUNT(GResolutions); i++)
	{
		if (GResolutions[i] == Cur)
		{
			Idx = i;
			break;
		}
	}
	Idx = (Idx + Dir + UE_ARRAY_COUNT(GResolutions)) % UE_ARRAY_COUNT(GResolutions);
	S->SetScreenResolution(GResolutions[Idx]);
	S->ApplySettings(false);
	S->SaveSettings();
}

FText SRSMenu::GetVSyncText() const
{
	UGameUserSettings* S = GetUS();
	return FText::FromString(S && S->IsVSyncEnabled() ? TEXT("On") : TEXT("Off"));
}

void SRSMenu::ToggleVSync()
{
	if (UGameUserSettings* S = GetUS())
	{
		S->SetVSyncEnabled(!S->IsVSyncEnabled());
		S->ApplySettings(false);
		S->SaveSettings();
	}
}

void SRSMenu::Construct(const FArguments& InArgs)
{
	PC = InArgs._OwnerPC;

	const FSlateFontInfo TitleFont = FCoreStyle::GetDefaultFontStyle("Bold", 30);
	const FSlateFontInfo LabelFont = FCoreStyle::GetDefaultFontStyle("Regular", 13);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.75f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(480.f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 18.f).HAlign(HAlign_Center)
				[
					SNew(STextBlock).Font(TitleFont).ColorAndOpacity(FLinearColor(1.f, 0.25f, 0.25f))
					.Text(FText::FromString(TEXT("RAGESTRIKE")))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					MakeButton(
						FText::FromString(PC.IsValid() && PC->IsStartupMenu()
							? TEXT("Одиночная игра")
							: TEXT("Продолжить  (Esc)")),
						[this]()
						{
							if (PC.IsValid()) { PC->CloseMenu(); }
						})
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					MakeButton(FText::FromString(TEXT("Создать лобби (LAN)")), [this]()
					{
						if (PC.IsValid()) { PC->HostGame(); }
					})
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f, 0.f)
					[
						SAssignNew(IPBox, SEditableTextBox)
						.Font(LabelFont)
						.Text(FText::FromString(TEXT("127.0.0.1")))
					]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						MakeButton(FText::FromString(TEXT("Подключиться")), [this]()
						{
							if (PC.IsValid() && IPBox.IsValid())
							{
								PC->JoinGame(IPBox->GetText().ToString());
							}
						})
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 4.f)
				[
					MakeCycleRow(FText::FromString(TEXT("Карта")),
						TAttribute<FText>::CreateSP(this, &SRSMenu::GetMapText),
						[this]() { CycleMap(-1); }, [this]() { CycleMap(1); })
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 12.f, 0.f, 2.f)
				[
					SNew(STextBlock).Font(LabelFont).Text(FText::FromString(TEXT("Команда")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 4.f, 0.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center).ContentPadding(FMargin(0.f, 8.f))
						.OnClicked_Lambda([this]()
						{
							if (PC.IsValid()) { PC->SelectTeam(true); }
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.ColorAndOpacity_Lambda([this]()
							{
								return (PC.IsValid() && PC->IsTeamCT())
									? FSlateColor(FLinearColor(0.35f, 0.65f, 1.f))
									: FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
							})
							.Text(FText::FromString(TEXT("Контр-террористы")))
						]
					]
					+ SHorizontalBox::Slot().FillWidth(1.f).Padding(4.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.HAlign(HAlign_Center).ContentPadding(FMargin(0.f, 8.f))
						.OnClicked_Lambda([this]()
						{
							if (PC.IsValid()) { PC->SelectTeam(false); }
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
							.ColorAndOpacity_Lambda([this]()
							{
								return (PC.IsValid() && !PC->IsTeamCT())
									? FSlateColor(FLinearColor(1.f, 0.6f, 0.2f))
									: FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
							})
							.Text(FText::FromString(TEXT("Террористы")))
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 4.f)
				[
					MakeCycleRow(FText::FromString(TEXT("Graphics quality")),
						TAttribute<FText>::CreateSP(this, &SRSMenu::GetQualityText),
						[this]() { CycleQuality(-1); }, [this]() { CycleQuality(1); })
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					MakeCycleRow(FText::FromString(TEXT("Window mode")),
						TAttribute<FText>::CreateSP(this, &SRSMenu::GetWindowModeText),
						[this]() { CycleWindowMode(); }, [this]() { CycleWindowMode(); })
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					MakeCycleRow(FText::FromString(TEXT("Resolution")),
						TAttribute<FText>::CreateSP(this, &SRSMenu::GetResolutionText),
						[this]() { CycleResolution(-1); }, [this]() { CycleResolution(1); })
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
				[
					MakeCycleRow(FText::FromString(TEXT("VSync")),
						TAttribute<FText>::CreateSP(this, &SRSMenu::GetVSyncText),
						[this]() { ToggleVSync(); }, [this]() { ToggleVSync(); })
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 2.f)
				[
					SNew(STextBlock).Font(LabelFont).Text(FText::FromString(TEXT("Sound volume")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SSlider)
					.Value_Lambda([]() { return FApp::GetVolumeMultiplier(); })
					.OnValueChanged_Lambda([this](float V)
					{
						FApp::SetVolumeMultiplier(V);
						if (PC.IsValid()) { PC->SaveUserFloat(TEXT("Volume"), V); }
					})
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
				[
					SNew(STextBlock).Font(LabelFont).Text(FText::FromString(TEXT("Mouse sensitivity")))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SSlider)
					.MinValue(0.1f).MaxValue(3.f)
					.Value_Lambda([this]() { return PC.IsValid() ? PC->MouseSens : 1.f; })
					.OnValueChanged_Lambda([this](float V)
					{
						if (PC.IsValid())
						{
							PC->MouseSens = V;
							PC->SaveUserFloat(TEXT("MouseSens"), V);
						}
					})
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 4.f)
				[
					MakeButton(FText::FromString(TEXT("Новый матч")), [this]()
					{
						if (PC.IsValid()) { PC->ReloadLevel(); }
					})
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
				[
					MakeButton(FText::FromString(TEXT("Выход из игры")), [this]()
					{
						if (PC.IsValid()) { PC->QuitGame(); }
					})
				]
			]
		]
	];
}
