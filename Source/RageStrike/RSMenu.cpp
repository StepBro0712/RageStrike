#include "RSMenu.h"
#include "RSPlayerController.h"
#include "RSMaps.h"
#include "RSWeaponData.h"
#include "RSNet.h"
#include "RSBinds.h"
#include "RSMatchSettings.h"
#include "RSAudio.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Misc/App.h"
#include "HAL/PlatformProcess.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SEditableTextBox.h"

static const FIntPoint GResolutions[] = { {1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440} };

namespace
{
	const FLinearColor MenuBarBg(0.f, 0.f, 0.f, 0.62f);
	const FLinearColor MenuPanelBg(0.02f, 0.03f, 0.05f, 0.82f);
	const FLinearColor MenuAccent(0.98f, 0.68f, 0.15f);   // жёлтый CS2
	const FLinearColor MenuGreen(0.20f, 0.85f, 0.35f);
	const FLinearColor MenuDim(0.92f, 0.94f, 0.96f, 0.55f);
	const FLinearColor MenuCT(0.35f, 0.65f, 1.f);
	const FLinearColor MenuT(1.f, 0.6f, 0.2f);
}

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
	static const TCHAR* Names[] = { TEXT("Низкое"), TEXT("Среднее"), TEXT("Высокое"), TEXT("Эпик") };
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
		case EWindowMode::Fullscreen:         return FText::FromString(TEXT("Полный экран"));
		case EWindowMode::WindowedFullscreen: return FText::FromString(TEXT("Без рамки"));
		default:                              return FText::FromString(TEXT("В окне"));
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
	return FText::FromString(S && S->IsVSyncEnabled() ? TEXT("Вкл") : TEXT("Выкл"));
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

TSharedRef<SWidget> SRSMenu::MakeTab(const FText& Label, int32 TabIndex)
{
	return SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
		.ContentPadding(FMargin(18.f, 12.f))
		.OnClicked_Lambda([this, TabIndex]()
		{
			ActiveTab = TabIndex;
			return FReply::Handled();
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 17))
				.ColorAndOpacity_Lambda([this, TabIndex]()
				{
					return ActiveTab == TabIndex ? FSlateColor(FLinearColor::White) : FSlateColor(MenuDim);
				})
				.Text(Label)
			]
			// полоска под активной вкладкой, как в CS2
			+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SBox).HeightOverride(3.f)
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor_Lambda([this, TabIndex]()
					{
						return ActiveTab == TabIndex ? MenuAccent : FLinearColor(0.f, 0.f, 0.f, 0.f);
					})
				]
			]
		];
}

void SRSMenu::PushRules()
{
	// Без этого настройки лежали в конфиге, а матч продолжал играть по старым:
	// состав ботов и число раундов не менялись до перезапуска уровня.
	if (PC.IsValid())
	{
		PC->ApplyMatchSettingsNow();
	}
}

TSharedRef<SWidget> SRSMenu::MakeTopBar()
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(MenuBarBg)
		.Padding(FMargin(16.f, 0.f))
		[
			SNew(SHorizontalBox)

			// выход слева, как кнопка питания в CS2
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SButton)
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
				.ContentPadding(FMargin(10.f, 12.f))
				.OnClicked_Lambda([this]()
				{
					if (PC.IsValid()) { PC->QuitGame(); }
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
					.ColorAndOpacity(FLinearColor(1.f, 0.35f, 0.3f))
					.Text(FText::FromString(TEXT("ВЫХОД")))
				]
			]

			// вкладки по центру
			+ SHorizontalBox::Slot().FillWidth(1.f).HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth()[ MakeTab(FText::FromString(TEXT("ИНВЕНТАРЬ")), 4) ]
				+ SHorizontalBox::Slot().AutoWidth()[ MakeTab(FText::FromString(TEXT("СНАРЯЖЕНИЕ")), 2) ]
				+ SHorizontalBox::Slot().AutoWidth()[ MakeTab(FText::FromString(TEXT("ИГРАТЬ")), 0) ]
				+ SHorizontalBox::Slot().AutoWidth()[ MakeTab(FText::FromString(TEXT("НОВОСТИ")), 3) ]
				+ SHorizontalBox::Slot().AutoWidth()[ MakeTab(FText::FromString(TEXT("НАСТРОЙКИ")), 1) ]
			]
		];
}

TSharedRef<SWidget> SRSMenu::MakePlayPanel()
{
	const bool bStartup = PC.IsValid() && PC->IsStartupMenu();

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 14.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Карта")),
				TAttribute<FText>::CreateSP(this, &SRSMenu::GetMapText),
				[this]() { CycleMap(-1); }, [this]() { CycleMap(1); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.ColorAndOpacity(MenuDim)
			.Text(FText::FromString(TEXT("Сторона")))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 14.f)
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
							? FSlateColor(MenuCT) : FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
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
							? FSlateColor(MenuT) : FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f));
					})
					.Text(FText::FromString(TEXT("Террористы")))
				]
			]
		]

		// большая жёлтая кнопка, как в CS2. Тонировать штатный тёмный брашик
		// бесполезно — жёлтый выходит оливковым, поэтому фон рисуем бордером,
		// а сверху кладём кнопку без рамки.
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(MenuAccent)
			.Padding(0.f)
			[
				SNew(SButton)
				.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
				.HAlign(HAlign_Center)
				.ContentPadding(FMargin(0.f, 14.f))
				.OnClicked_Lambda([this]()
				{
					if (PC.IsValid()) { PC->CloseMenu(); }
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 20))
					.ColorAndOpacity(FLinearColor(0.05f, 0.05f, 0.05f))
					.Text(FText::FromString(bStartup ? TEXT("ИГРАТЬ") : TEXT("ПРОДОЛЖИТЬ")))
				]
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeButton(FText::FromString(TEXT("Новый матч")), [this]()
			{
				if (PC.IsValid()) { PC->ReloadLevel(); }
			})
		]

		// --- правила матча ---
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 14.f, 0.f, 4.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			.ColorAndOpacity(MenuAccent)
			.Text(FText::FromString(TEXT("Правила матча")))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Игроков в команде")),
				TAttribute<FText>::Create([]()
				{
					return FText::FromString(FString::Printf(TEXT("%d"), RSMatch::GetTeamSize()));
				}),
				[this]() { RSMatch::SetTeamSize(RSMatch::GetTeamSize() - 1); PushRules(); },
				[this]() { RSMatch::SetTeamSize(RSMatch::GetTeamSize() + 1); PushRules(); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Раундов до победы")),
				TAttribute<FText>::Create([]()
				{
					const int32 Win = RSMatch::GetRoundsToWin();
					return FText::FromString(FString::Printf(TEXT("%d  (всего %d)"),
						Win, RSMatch::RoundsTotalFor(Win)));
				}),
				[this]() { RSMatch::SetRoundsToWin(RSMatch::GetRoundsToWin() - 1); PushRules(); },
				[this]() { RSMatch::SetRoundsToWin(RSMatch::GetRoundsToWin() + 1); PushRules(); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Время раунда")),
				TAttribute<FText>::Create([]()
				{
					const int32 S = RSMatch::GetRoundSeconds();
					return FText::FromString(FString::Printf(TEXT("%d:%02d"), S / 60, S % 60));
				}),
				[this]() { RSMatch::SetRoundSeconds(RSMatch::GetRoundSeconds() - 15); PushRules(); },
				[this]() { RSMatch::SetRoundSeconds(RSMatch::GetRoundSeconds() + 15); PushRules(); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Время закупки")),
				TAttribute<FText>::Create([]()
				{
					return FText::FromString(FString::Printf(TEXT("%d с"), RSMatch::GetBuySeconds()));
				}),
				[this]() { RSMatch::SetBuySeconds(RSMatch::GetBuySeconds() - 5); PushRules(); },
				[this]() { RSMatch::SetBuySeconds(RSMatch::GetBuySeconds() + 5); PushRules(); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Добирать ботами")),
				TAttribute<FText>::Create([]()
				{
					return FText::FromString(RSMatch::GetUseBots() ? TEXT("Да") : TEXT("Нет"));
				}),
				[this]() { RSMatch::SetUseBots(!RSMatch::GetUseBots()); PushRules(); },
				[this]() { RSMatch::SetUseBots(!RSMatch::GetUseBots()); PushRules(); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
			.ColorAndOpacity(MenuDim)
			.Text(FText::FromString(TEXT("Правила применяются сразу, состав ботов подстраивается")))
		]

		// --- сетевая игра ---
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 16.f, 0.f, 6.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			.ColorAndOpacity(MenuAccent)
			.Text(FText::FromString(TEXT("Игра по сети")))
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			MakeButton(FText::FromString(TEXT("Создать сервер")), [this]()
			{
				// сервер поднимаем и сразу просим роутер открыть порт
				RSNet::RequestPortMapping();
				if (PC.IsValid()) { PC->HostGame(); }
			})
		]

		// адрес, который надо дать друзьям
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 12))
				.ColorAndOpacity(MenuDim)
				.Text(FText::FromString(TEXT("Твой адрес: ")))
			]
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
				.Text_Lambda([]() { return FText::FromString(RSNet::GetJoinAddress()); })
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				MakeButton(FText::FromString(TEXT("Копировать")), []()
				{
					FPlatformApplicationMisc::ClipboardCopy(*RSNet::GetJoinAddress());
				})
			]
		]

		// состояние проброса порта: без него друзья не подключатся извне
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
			.AutoWrapText(true)
			.ColorAndOpacity_Lambda([]()
			{
				switch (RSNet::GetPortState())
				{
				case RSNet::EPortState::Mapped: return FSlateColor(MenuGreen);
				case RSNet::EPortState::Failed: return FSlateColor(FLinearColor(1.f, 0.45f, 0.4f));
				default:                        return FSlateColor(MenuDim);
				}
			})
			.Text_Lambda([]() { return FText::FromString(RSNet::GetPortMessage()); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
			.ColorAndOpacity(MenuDim)
			.Text_Lambda([]()
			{
				return FText::FromString(FString::Printf(TEXT("В домашней сети: %s:%d"),
					*RSNet::GetLocalAddress(), RSNet::GamePort));
			})
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 0.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 6.f, 0.f)
			[
				SAssignNew(IPBox, SEditableTextBox)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.HintText(FText::FromString(TEXT("адрес сервера, например 1.2.3.4:7777")))
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
		];
}

TSharedRef<SWidget> SRSMenu::MakeBindsPanel()
{
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);

	const TArray<RSBinds::FEntry>& Entries = RSBinds::All();
	for (int32 i = 0; i < Entries.Num(); i++)
	{
		const RSBinds::FEntry& Entry = Entries[i];
		List->AddSlot()
		.AutoHeight()
		.Padding(0.f, 3.f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.ColorAndOpacity(FLinearColor(0.85f, 0.87f, 0.9f))
				.Text(FText::FromString(Entry.Display))
			]

			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(190.f)
				[
					SNew(SButton)
					.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
					.ContentPadding(FMargin(10.f, 7.f))
					.OnClicked_Lambda([this, i]()
						{
							// строка переходит в режим ожидания: следующая
							// нажатая клавиша и станет привязкой
							CapturingBind = i;
							return FReply::Handled();
						})
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor_Lambda([this, i]()
							{
								return CapturingBind == i ? FLinearColor(0.9f, 0.7f, 0.15f)
														  : FLinearColor(0.16f, 0.17f, 0.2f);
							})
						.Padding(FMargin(10.f, 6.f))
						[
							SNew(STextBlock)
							.Justification(ETextJustify::Center)
							.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
							.ColorAndOpacity(FLinearColor::White)
							.Text_Lambda([this, i, &Entry]()
								{
									if (CapturingBind == i)
									{
										return FText::FromString(TEXT("нажмите клавишу…"));
									}
									return RSBinds::GetKey(Entry).GetDisplayName();
								})
						]
					]
				]
			]
		];
	}

	List->AddSlot()
	.AutoHeight()
	.Padding(0.f, 14.f, 0.f, 0.f)
	[
		MakeButton(FText::FromString(TEXT("ВЕРНУТЬ ПО УМОЛЧАНИЮ")),
			[]() { RSBinds::ResetAll(); })
	];

	List->AddSlot()
	.AutoHeight()
	.Padding(0.f, 10.f, 0.f, 0.f)
	[
		SNew(STextBlock)
		.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
		.ColorAndOpacity(FLinearColor(0.55f, 0.57f, 0.62f))
		.Text(FText::FromString(TEXT("Клавиши сохраняются сразу и переживают перезапуск.")))
	];

	return List;
}

TSharedRef<SWidget> SRSMenu::MakeCrosshairPreview()
{
	// Прицел собирается из четырёх полосок: вертикальные в столбце,
	// горизонтальные в строке, между ними зазор — то же, что рисует HUD.
	auto Bar = [this](bool bVertical)
	{
		return SNew(SBox)
			.WidthOverride_Lambda([bVertical]()
				{ return bVertical ? RSCrosshair::GetThickness() : RSCrosshair::GetLength(); })
			.HeightOverride_Lambda([bVertical]()
				{ return bVertical ? RSCrosshair::GetLength() : RSCrosshair::GetThickness(); })
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor_Lambda([]() { return FSlateColor(RSCrosshair::GetColor()); })
			];
	};
	auto Spacer = [](bool bVertical)
	{
		return SNew(SBox)
			.WidthOverride_Lambda([bVertical]() { return bVertical ? 1.f : RSCrosshair::GetGap() * 2.f; })
			.HeightOverride_Lambda([bVertical]() { return bVertical ? RSCrosshair::GetGap() * 2.f : 1.f; });
	};

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.06f, 0.07f, 0.09f, 0.9f))
		.Padding(10.f)
		[
			SNew(SBox)
			.HeightOverride(120.f)
			[
				SNew(SOverlay)

				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Bar(true) ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Spacer(true) ]
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Bar(true) ]
				]

				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Bar(false) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Spacer(false) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Bar(false) ]
				]

				// точка в центре — если включена
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
				[
					SNew(SBox)
					.Visibility_Lambda([]()
						{ return RSCrosshair::GetDot() ? EVisibility::Visible : EVisibility::Hidden; })
					.WidthOverride_Lambda([]() { return RSCrosshair::GetThickness(); })
					.HeightOverride_Lambda([]() { return RSCrosshair::GetThickness(); })
					[
						SNew(SBorder)
						.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
						.BorderBackgroundColor_Lambda([]() { return FSlateColor(RSCrosshair::GetColor()); })
					]
				]
			]
		];
}

TSharedRef<SWidget> SRSMenu::MakeChannelRow(int32 Which)
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text(FText::FromString(RSCrosshair::ChannelName(Which)))
		]

		+ SHorizontalBox::Slot().FillWidth(1.4f).VAlign(VAlign_Center).Padding(8.f, 0.f)
		[
			SNew(SSlider)
			.Value_Lambda([Which]() { return RSCrosshair::GetChannel(Which) / 255.f; })
			.OnValueChanged_Lambda([Which](float V)
				{ RSCrosshair::SetChannel(Which, FMath::RoundToInt(V * 255.f)); })
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SBox).MinDesiredWidth(46.f).HAlign(HAlign_Right)
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 13))
				.Text_Lambda([Which]()
					{ return FText::AsNumber(RSCrosshair::GetChannel(Which)); })
			]
		];
}

TSharedRef<SWidget> SRSMenu::MakeCrosshairPanel()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			MakeCrosshairPreview()
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Длина")),
				TAttribute<FText>::Create([]() {
					return FText::FromString(FString::Printf(TEXT("%.0f"), RSCrosshair::GetLength())); }),
				[]() { RSCrosshair::SetLength(RSCrosshair::GetLength() - 1.f); },
				[]() { RSCrosshair::SetLength(RSCrosshair::GetLength() + 1.f); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Толщина")),
				TAttribute<FText>::Create([]() {
					return FText::FromString(FString::Printf(TEXT("%.0f"), RSCrosshair::GetThickness())); }),
				[]() { RSCrosshair::SetThickness(RSCrosshair::GetThickness() - 1.f); },
				[]() { RSCrosshair::SetThickness(RSCrosshair::GetThickness() + 1.f); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Зазор")),
				TAttribute<FText>::Create([]() {
					return FText::FromString(FString::Printf(TEXT("%.0f"), RSCrosshair::GetGap())); }),
				[]() { RSCrosshair::SetGap(RSCrosshair::GetGap() - 1.f); },
				[]() { RSCrosshair::SetGap(RSCrosshair::GetGap() + 1.f); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[ MakeChannelRow(0) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[ MakeChannelRow(1) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[ MakeChannelRow(2) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)[ MakeChannelRow(3) ]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Обводка")),
				TAttribute<FText>::Create([]() {
					return FText::FromString(RSCrosshair::GetOutline() ? TEXT("вкл") : TEXT("выкл")); }),
				[]() { RSCrosshair::SetOutline(!RSCrosshair::GetOutline()); },
				[]() { RSCrosshair::SetOutline(!RSCrosshair::GetOutline()); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Точка в центре")),
				TAttribute<FText>::Create([]() {
					return FText::FromString(RSCrosshair::GetDot() ? TEXT("вкл") : TEXT("выкл")); }),
				[]() { RSCrosshair::SetDot(!RSCrosshair::GetDot()); },
				[]() { RSCrosshair::SetDot(!RSCrosshair::GetDot()); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Расходится при стрельбе")),
				TAttribute<FText>::Create([]() {
					return FText::FromString(RSCrosshair::GetDynamic() ? TEXT("вкл") : TEXT("выкл")); }),
				[]() { RSCrosshair::SetDynamic(!RSCrosshair::GetDynamic()); },
				[]() { RSCrosshair::SetDynamic(!RSCrosshair::GetDynamic()); })
		];
}

TSharedRef<SWidget> SRSMenu::MakeSettingsSubTab(const FText& Label, int32 Index)
{
	return SNew(SButton)
		.ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
		.ContentPadding(FMargin(14.f, 6.f))
		.OnClicked_Lambda([this, Index]() { SettingsSection = Index; return FReply::Handled(); })
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor_Lambda([this, Index]()
				{
					return SettingsSection == Index ? FLinearColor(0.25f, 0.45f, 0.75f, 0.55f)
													: FLinearColor(0.f, 0.f, 0.f, 0.f);
				})
			.Padding(FMargin(12.f, 5.f))
			[
				SNew(STextBlock)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity_Lambda([this, Index]()
					{
						return SettingsSection == Index ? FSlateColor(FLinearColor::White) : FSlateColor(MenuDim);
					})
				.Text(Label)
			]
		];
}

TSharedRef<SWidget> SRSMenu::MakeSettingsPanel()
{
	// Внутренние вкладки, как в CS2: изображение, управление, прицел —
	// отдельными разделами верхнего уровня они только загромождали меню.
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()[ MakeSettingsSubTab(FText::FromString(TEXT("ИЗОБРАЖЕНИЕ")), 0) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeSettingsSubTab(FText::FromString(TEXT("УПРАВЛЕНИЕ")), 1) ]
			+ SHorizontalBox::Slot().AutoWidth()[ MakeSettingsSubTab(FText::FromString(TEXT("ПРИЦЕЛ")), 2) ]
		]

		+ SVerticalBox::Slot().FillHeight(1.f)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this]() { return SettingsSection; })
			+ SWidgetSwitcher::Slot()[ MakeVideoPanel() ]
			+ SWidgetSwitcher::Slot()[ MakeBindsPanel() ]
			+ SWidgetSwitcher::Slot()[ MakeCrosshairPanel() ]
		];
}

TSharedRef<SWidget> SRSMenu::MakeVideoPanel()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Качество графики")),
				TAttribute<FText>::CreateSP(this, &SRSMenu::GetQualityText),
				[this]() { CycleQuality(-1); }, [this]() { CycleQuality(1); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Режим окна")),
				TAttribute<FText>::CreateSP(this, &SRSMenu::GetWindowModeText),
				[this]() { CycleWindowMode(); }, [this]() { CycleWindowMode(); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Разрешение")),
				TAttribute<FText>::CreateSP(this, &SRSMenu::GetResolutionText),
				[this]() { CycleResolution(-1); }, [this]() { CycleResolution(1); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Верт. синхронизация")),
				TAttribute<FText>::CreateSP(this, &SRSMenu::GetVSyncText),
				[this]() { ToggleVSync(); }, [this]() { ToggleVSync(); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Счётчик FPS и нагрузки")),
				TAttribute<FText>::Create([]()
				{
					return FText::FromString(RSOptions::PerfModeName(RSOptions::GetPerfMode()));
				}),
				[]() { RSOptions::SetPerfMode(RSOptions::GetPerfMode() - 1); },
				[]() { RSOptions::SetPerfMode(RSOptions::GetPerfMode() + 1); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 10.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text(FText::FromString(TEXT("Громкость")))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSlider)
			.Value_Lambda([]() { return RSAudio::GetMasterVolume(); })
			.OnValueChanged_Lambda([this](float V)
			{
				RSAudio::SetMasterVolume(V);
				if (PC.IsValid()) { PC->SaveUserFloat(TEXT("Volume"), V); }
			})
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text_Lambda([]() { return FText::FromString(FString::Printf(
				TEXT("Громкость музыки — %d%%"), FMath::RoundToInt(RSOptions::GetMusicVolume() * 100.f))); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSlider)
			.Value_Lambda([]() { return RSOptions::GetMusicVolume(); })
			.OnValueChanged_Lambda([](float V) { RSOptions::SetMusicVolume(V); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text_Lambda([]() { return FText::FromString(FString::Printf(
				TEXT("Громкость эффектов — %d%%"), FMath::RoundToInt(RSOptions::GetFxVolume() * 100.f))); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSlider)
			.Value_Lambda([]() { return RSOptions::GetFxVolume(); })
			.OnValueChanged_Lambda([](float V) { RSOptions::SetFxVolume(V); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text_Lambda([]() { return FText::FromString(FString::Printf(
				TEXT("Обзор — %d°"), FMath::RoundToInt(RSOptions::GetFov()))); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSlider)
			.Value_Lambda([]() { return (RSOptions::GetFov() - 70.f) / 50.f; })
			.OnValueChanged_Lambda([](float V) { RSOptions::SetFov(70.f + V * 50.f); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text_Lambda([]() { return FText::FromString(FString::Printf(
				TEXT("Яркость — %.1f"), RSOptions::GetGamma())); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSlider)
			.Value_Lambda([]() { return (RSOptions::GetGamma() - 1.6f) / 1.6f; })
			.OnValueChanged_Lambda([](float V) { RSOptions::SetGamma(1.6f + V * 1.6f); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text_Lambda([]() { return FText::FromString(FString::Printf(
				TEXT("Оружие вправо-влево — %.0f"), RSOptions::GetVmOffset())); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSlider)
			.Value_Lambda([]() { return (RSOptions::GetVmOffset() + 8.f) / 16.f; })
			.OnValueChanged_Lambda([](float V) { RSOptions::SetVmOffset(-8.f + V * 16.f); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text_Lambda([]() { return FText::FromString(FString::Printf(
				TEXT("Оружие вверх-вниз — %.0f"), RSOptions::GetVmOffsetZ())); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSlider)
			.Value_Lambda([]() { return (RSOptions::GetVmOffsetZ() + 8.f) / 16.f; })
			.OnValueChanged_Lambda([](float V) { RSOptions::SetVmOffsetZ(-8.f + V * 16.f); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text_Lambda([]() { return FText::FromString(FString::Printf(
				TEXT("Оружие вперёд-назад — %.0f"), RSOptions::GetVmOffsetX())); })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SSlider)
			.Value_Lambda([]() { return (RSOptions::GetVmOffsetX() + 8.f) / 16.f; })
			.OnValueChanged_Lambda([](float V) { RSOptions::SetVmOffsetX(-8.f + V * 16.f); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f)
		[
			MakeCycleRow(FText::FromString(TEXT("Оружие на экране")),
				TAttribute<FText>::Create([]() {
					return FText::FromString(RSOptions::GetHideViewmodel() ? TEXT("скрыто") : TEXT("видно")); }),
				[]() { RSOptions::SetHideViewmodel(!RSOptions::GetHideViewmodel()); },
				[]() { RSOptions::SetHideViewmodel(!RSOptions::GetHideViewmodel()); })
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 8.f, 0.f, 2.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.Text_Lambda([this]() { return FText::FromString(FString::Printf(
				TEXT("Чувствительность мыши — %.2f"), PC.IsValid() ? PC->MouseSens : 1.f)); })
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
		];
}

TSharedRef<SWidget> SRSMenu::MakeArsenalPanel()
{
	TSharedRef<SScrollBox> List = SNew(SScrollBox);

	List->AddSlot().Padding(0.f, 0.f, 0.f, 8.f)
	[
		SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
		.ColorAndOpacity(MenuAccent)
		.Text(FText::FromString(TEXT("Арсенал — цены и урон")))
	];

	for (int32 i = 0; i < RSWeapons::Count; i++)
	{
		const FRSWeaponDef& Def = RSWeapons::Get((ERSWeapon)i);
		if (Def.Slot == ERSSlot::Knife)
		{
			continue;
		}
		const FString Line = FString::Printf(TEXT("%-12s  $%-5d  урон %d%s"),
			Def.Name, Def.Price, FMath::RoundToInt(Def.BodyDamage),
			Def.Pellets > 1 ? *FString::Printf(TEXT(" x%d дроби"), Def.Pellets) : TEXT(""));
		List->AddSlot().Padding(0.f, 2.f)
		[
			SNew(STextBlock)
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 12))
			.ColorAndOpacity(FLinearColor::White)
			.Text(FText::FromString(Line))
		];
	}

	return SNew(SBox).MaxDesiredHeight(460.f)[ List ];
}

TSharedRef<SWidget> SRSMenu::MakeNewsPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
			.ColorAndOpacity(MenuAccent)
			.Text(FText::FromString(TEXT("Обновление v0.1")))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
			.AutoWrapText(true)
			.Text(FText::FromString(TEXT(
				"— Карта Mirage\n"
				"— Весь арсенал CS: 20 стволов с ценами и паттернами отдачи\n"
				"— Гранаты: HE, флешка, дым, молотов\n"
				"— Броня и шлем, экономика с лосс-бонусами\n"
				"— Killfeed, радар, HUD в стиле CS2\n"
				"— Rage-читы F1–F7 как всегда бесплатно :)")))
		];
}

TSharedRef<SWidget> SRSMenu::MakePlaceholderPanel()
{
	return SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Center).MinDesiredHeight(200.f)
	[
		SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18))
		.ColorAndOpacity(MenuDim)
		.Text(FText::FromString(TEXT("Скоро™")))
	];
}

TSharedRef<SWidget> SRSMenu::MakePlayerCard()
{
	// ник игрока: сохраняется в конфиг и уходит на сервер для таблицы и killfeed
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(MenuPanelBg)
		.Padding(FMargin(18.f, 10.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
			[
				SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13))
				.ColorAndOpacity(MenuDim)
				.Text(FText::FromString(TEXT("Ник")))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SBox).WidthOverride(220.f)
				[
					SAssignNew(NickBox, SEditableTextBox)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
					.Text(FText::FromString(PC.IsValid() ? PC->PlayerNick : FString()))
					.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type)
					{
						if (PC.IsValid()) { PC->SetPlayerNick(NewText.ToString()); }
					})
				]
			]
		];
}

FReply SRSMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	// Ждём клавишу для перебинда: перехватываем любую, кроме Esc — ею
	// отменяем, иначе из режима ожидания было бы не выйти.
	if (CapturingBind != INDEX_NONE)
	{
		const FKey Key = KeyEvent.GetKey();
		if (Key != EKeys::Escape && RSBinds::All().IsValidIndex(CapturingBind))
		{
			RSBinds::SetKey(RSBinds::All()[CapturingBind], Key);
		}
		CapturingBind = INDEX_NONE;
		return FReply::Handled();
	}

	// обе клавиши открытия меню закрывают его тоже, как было до UIOnly
	if (KeyEvent.GetKey() == EKeys::Escape || KeyEvent.GetKey() == EKeys::P)
	{
		if (PC.IsValid())
		{
			PC->CloseMenu();
			return FReply::Handled();
		}
	}
	return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
}

void SRSMenu::Construct(const FArguments& InArgs)
{
	PC = InArgs._OwnerPC;

	// внешний адрес узнаём заранее, чтобы к моменту хоста он уже был на экране
	RSNet::RequestPublicIP();

	ChildSlot
	[
		SNew(SOverlay)

		// лёгкое затемнение — сцена с картой остаётся видна, как в CS2
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.35f))
		]

		// верхняя панель вкладок
		+ SOverlay::Slot().VAlign(VAlign_Top)
		[
			MakeTopBar()
		]

		// контент активной вкладки
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
		[
			SNew(SBox).WidthOverride(540.f)
			[
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
				.BorderBackgroundColor(MenuPanelBg)
				.Padding(FMargin(28.f, 24.f))
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return ActiveTab; })
					+ SWidgetSwitcher::Slot()[ MakePlayPanel() ]
					+ SWidgetSwitcher::Slot()[ MakeSettingsPanel() ]
					+ SWidgetSwitcher::Slot()[ MakeArsenalPanel() ]
					+ SWidgetSwitcher::Slot()[ MakeNewsPanel() ]
					+ SWidgetSwitcher::Slot()[ MakePlaceholderPanel() ]
				]
			]
		]

		// карточка игрока внизу по центру
		+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0.f, 0.f, 0.f, 34.f)
		[
			MakePlayerCard()
		]
	];
}
