#include "RSCheatMenu.h"
#include "RSCharacter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Палитра снята с эталонного меню: почти чёрный фон, панели чуть светлее,
	// синий акцент только на включённом и на выбранном разделе.
	const FLinearColor CMBack   = FLinearColor(0.055f, 0.059f, 0.075f, 0.98f);
	const FLinearColor CMPanel  = FLinearColor(0.086f, 0.090f, 0.110f, 1.f);
	const FLinearColor CMRow    = FLinearColor(0.11f, 0.115f, 0.14f, 1.f);
	const FLinearColor CMAccent = FLinearColor(0.29f, 0.42f, 0.94f, 1.f);
	const FLinearColor CMOff    = FLinearColor(0.20f, 0.21f, 0.25f, 1.f);
	const FLinearColor CMText   = FLinearColor(0.84f, 0.86f, 0.90f, 1.f);
	const FLinearColor CMDim    = FLinearColor(0.45f, 0.47f, 0.53f, 1.f);

	const FSlateBrush* WhiteBrush()
	{
		return FCoreStyle::Get().GetBrush("WhiteBrush");
	}

	FSlateFontInfo Font(int32 Size, bool bBold = false)
	{
		return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	}

	TSharedRef<SWidget> Fill(const FLinearColor& Color, TSharedRef<SWidget> Content, float Pad = 0.f)
	{
		return SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(Color)
			.Padding(Pad)
			[
				Content
			];
	}
}

void SRSCheatMenu::Construct(const FArguments& InArgs)
{
	Owner = InArgs._Owner;

	ChildSlot
	[
		SNew(SOverlay)

		// затемнение под меню: игра остаётся видна, но не отвлекает
		+ SOverlay::Slot()
		[
			Fill(FLinearColor(0.f, 0.f, 0.f, 0.55f), SNullWidget::NullWidget)
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(940.f)
			.HeightOverride(620.f)
			[
				Fill(CMBack,
					SNew(SHorizontalBox)

					// слева разделы
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SBox)
						.WidthOverride(180.f)
						[
							Fill(CMPanel,
								SNew(SVerticalBox)

								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(16.f, 18.f, 16.f, 4.f)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("RAGESTRIKE")))
									.Font(Font(15, true))
									.ColorAndOpacity(CMText)
								]

								+ SVerticalBox::Slot()
								.AutoHeight()
								.Padding(16.f, 0.f, 16.f, 18.f)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("встроенные читы")))
									.Font(Font(9))
									.ColorAndOpacity(CMDim)
								]

								+ SVerticalBox::Slot().AutoHeight()[ MakeSideButton(TEXT("Rage"), 0) ]
								+ SVerticalBox::Slot().AutoHeight()[ MakeSideButton(TEXT("Legit"), 1) ]
								+ SVerticalBox::Slot().AutoHeight()[ MakeSideButton(TEXT("Visuals"), 2) ]
								+ SVerticalBox::Slot().AutoHeight()[ MakeSideButton(TEXT("Misc"), 3) ]

								+ SVerticalBox::Slot()
								.FillHeight(1.f)
								.VAlign(VAlign_Bottom)
								.Padding(16.f, 12.f)
								[
									SNew(STextBlock)
									.Text(FText::FromString(TEXT("Del / Insert — закрыть")))
									.Font(Font(9))
									.ColorAndOpacity(CMDim)
								], 0.f)
						]
					]

					// справа: панель конфигов и страницы
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.Padding(14.f, 14.f)
					[
						SNew(SVerticalBox)

						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.f, 0.f, 0.f, 12.f)
						[
							MakeConfigBar()
						]

						+ SVerticalBox::Slot()
						.FillHeight(1.f)
						[
						SAssignNew(Pages, SWidgetSwitcher)
						.WidgetIndex(this, &SRSCheatMenu::GetPageIndex)

						+ SWidgetSwitcher::Slot()[ MakeRagePage() ]
						+ SWidgetSwitcher::Slot()[ MakeLegitPage() ]
						+ SWidgetSwitcher::Slot()[ MakeVisualsPage() ]
						+ SWidgetSwitcher::Slot()[ MakeMiscPage() ]
						]
					], 0.f)
			]
		]
	];
}

FReply SRSCheatMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::Delete || Key == EKeys::Insert || Key == EKeys::Escape)
	{
		if (ARSCharacter* Char = Owner.Get())
		{
			Char->ToggleCheatMenu();
			return FReply::Handled();
		}
	}
	return SCompoundWidget::OnKeyDown(Geometry, KeyEvent);
}

void SRSCheatMenu::Sync()
{
	if (ARSCharacter* Char = Owner.Get())
	{
		Char->SyncCheats();
	}
}

TSharedRef<SWidget> SRSCheatMenu::MakeSideButton(const FString& Label, int32 Index)
{
	return SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(0.f)
		.OnClicked_Lambda([this, Index]() { Page = Index; return FReply::Handled(); })
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor_Lambda([this, Index]()
				{ return Page == Index ? CMRow : FLinearColor(0.f, 0.f, 0.f, 0.f); })
			.Padding(FMargin(16.f, 9.f))
			[
				SNew(SHorizontalBox)

				// синяя полоска у выбранного раздела
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 10.f, 0.f)
				[
					SNew(SBox).WidthOverride(3.f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor_Lambda([this, Index]()
							{ return Page == Index ? CMAccent : FLinearColor(0.f, 0.f, 0.f, 0.f); })
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Label))
					.Font(Font(11, true))
					.ColorAndOpacity_Lambda([this, Index]() { return Page == Index ? CMText : CMDim; })
				]
			]
		];
}

void SRSCheatMenu::RefreshConfigs()
{
	ConfigNames.Reset();
	for (const FString& Name : ARSCharacter::ListCheatConfigs())
	{
		ConfigNames.Add(MakeShared<FString>(Name));
	}
	if (ConfigCombo.IsValid())
	{
		ConfigCombo->RefreshOptions();
	}
}

TSharedRef<SWidget> SRSCheatMenu::MakeConfigBar()
{
	RefreshConfigs();

	auto Button = [this](const FString& Label, TFunction<void()> OnClick)
	{
		return SNew(SButton)
			.ButtonStyle(FCoreStyle::Get(), "NoBorder")
			.ContentPadding(0.f)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })
			[
				SNew(SBorder)
				.BorderImage(WhiteBrush())
				.BorderBackgroundColor(CMRow)
				.Padding(FMargin(12.f, 6.f))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Label))
					.Font(Font(10))
					.ColorAndOpacity(CMText)
				]
			];
	};

	return Fill(CMPanel,
		SNew(SHorizontalBox)

		// выбор сохранённого
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SBox)
			.WidthOverride(170.f)
			[
				SAssignNew(ConfigCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&ConfigNames)
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> In)
					{
						return SNew(STextBlock).Text(FText::FromString(*In))
							.Font(Font(10)).ColorAndOpacity(CMText);
					})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> In, ESelectInfo::Type)
					{
						if (!In.IsValid()) { return; }
						if (ARSCharacter* C = Owner.Get())
						{
							C->LoadCheatConfig(*In);
							if (NameBox.IsValid()) { NameBox->SetText(FText::FromString(*In)); }
						}
					})
				[
					SNew(STextBlock)
					.Font(Font(10))
					.ColorAndOpacity(CMDim)
					.Text_Lambda([this]()
						{
							const ARSCharacter* C = Owner.Get();
							return FText::FromString(C ? C->CurrentConfig : TEXT("—"));
						})
				]
			]
		]

		// имя для сохранения нового
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.VAlign(VAlign_Center)
		.Padding(8.f, 0.f)
		[
			// Текст задаём один раз при открытии. Привязка через Text_Lambda
			// перетирала бы поле каждый кадр — набранное имя не доживало
			// до нажатия «Сохранить», и конфиг уходил под старым.
			SAssignNew(NameBox, SEditableTextBox)
			.Text(FText::FromString(Owner.IsValid() ? Owner->CurrentConfig : TEXT("default")))
			.Font(Font(10))
			.HintText(FText::FromString(TEXT("имя конфига")))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			Button(TEXT("Сохранить"), [this]()
				{
					if (ARSCharacter* C = Owner.Get())
					{
						const FString Name = NameBox.IsValid()
							? NameBox->GetText().ToString().TrimStartAndEnd() : C->CurrentConfig;
						C->SaveCheatConfig(Name.IsEmpty() ? C->CurrentConfig : Name);
						RefreshConfigs();
					}
				})
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			Button(TEXT("Загрузить"), [this]()
				{
					if (ARSCharacter* C = Owner.Get())
					{
						const FString Name = NameBox.IsValid()
							? NameBox->GetText().ToString().TrimStartAndEnd() : C->CurrentConfig;
						C->LoadCheatConfig(Name);
					}
				})
		], 8.f);
}

TSharedRef<SWidget> SRSCheatMenu::MakeRagePage()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeSection(TEXT("СТРЕЛЬБА"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Триггербот"), &ARSCharacter::bTriggerbot) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Триггербот: FOV"), &ARSCharacter::TriggerFov, 0.f, 20.f, TEXT("°")) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Минимальный урон"), &ARSCharacter::MinDamage, 0.f, 100.f, TEXT("")) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Шанс попадания"), &ARSCharacter::HitChance, 0.f, 100.f, TEXT("%")) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Без отдачи и разброса"), &ARSCharacter::bNoRecoilSpread) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Двойной выстрел"), &ARSCharacter::bDoubleTap) ])
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeSection(TEXT("УПРЕЖДЕНИЕ"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Включено"), &ARSCharacter::bPredict) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Только за стеной"), &ARSCharacter::bPredictOnlyHidden) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeIntSlider(TEXT("Тиков"), &ARSCharacter::PredictTicks, 1, 32, TEXT("")) ])
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeSection(TEXT("БЭКТРЕК"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Включено"), &ARSCharacter::bBacktrack) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Глубина"), &ARSCharacter::BacktrackMs, 0.f, 400.f, TEXT(" мс")) ])
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeSection(TEXT("БИНД"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ MakeCombo(TEXT("Клавиша"), &ARSCharacter::BindKeyIndex,
							{ TEXT("нет"), TEXT("Left Alt"), TEXT("Left Shift"), TEXT("C"), TEXT("X"),
							  TEXT("Мышь 4"), TEXT("Мышь 5") }) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Урон под биндом"), &ARSCharacter::MinDamageAlt, 0.f, 100.f, TEXT("")) ])
				]
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeSection(TEXT("АНТИ-АИМ"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Включено"), &ARSCharacter::bAntiAim, true) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeCombo(TEXT("Режим"), &ARSCharacter::AntiAimMode,
							{ TEXT("Спиной"), TEXT("Спин"), TEXT("Дрожь") }, true) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Качание"), &ARSCharacter::AntiAimSwing, 0.f, 90.f, TEXT("°"), true) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Наклон головы"), &ARSCharacter::AntiAimPitch, -89.f, 89.f, TEXT("°"), true) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Кручение"), &ARSCharacter::AntiAimSpin, 30.f, 1440.f, TEXT("°/с"), true) ])
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					MakeSection(TEXT("ПРОЧЕЕ"),
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Бессмертие"), &ARSCharacter::bGodMode, true) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Скорость и распрыжка"), &ARSCharacter::bSpeedhack, true) ]
						+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Бесконечные деньги"), &ARSCharacter::bInfiniteMoney, true) ])
				]
			]
		];
}

TSharedRef<SWidget> SRSCheatMenu::MakeLegitPage()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(TEXT("НАВОДКА"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Аимбот"), &ARSCharacter::bAimbot) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Silent Aim"), &ARSCharacter::bSilentAim) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Field of View"), &ARSCharacter::RageFov, 0.f, 180.f, TEXT("°")) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeCombo(TEXT("Хитбокс"), &ARSCharacter::AimHitbox,
						{ TEXT("Голова"), TEXT("Грудь"), TEXT("Живот") }) ])
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(TEXT("ПО-ЧЕЛОВЕЧЕСКИ"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Плавная наводка"), &ARSCharacter::bLegitAim) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeCombo(TEXT("Включается"), &ARSCharacter::AimActivation,
						{ TEXT("Всегда"), TEXT("При стрельбе"), TEXT("При прицеле") }) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Сглаживание"), &ARSCharacter::AimSmooth, 0.f, 100.f, TEXT("%")) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Время реакции"), &ARSCharacter::ReactionMs, 0.f, 500.f, TEXT(" мс")) ])
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Наводится только на тех, кого видно:\nсквозь стены прицел не уводится.")))
				.Font(Font(9))
				.ColorAndOpacity(CMDim)
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(TEXT("ТРИГГЕРБОТ"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Включён"), &ARSCharacter::bTriggerbot) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Время реакции"), &ARSCharacter::TriggerReactionMs, 0.f, 500.f, TEXT(" мс")) ]
					+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("FOV"), &ARSCharacter::TriggerFov, 0.f, 20.f, TEXT("°")) ])
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				MakeSection(TEXT("ОТДАЧА"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()[ MakeSlider(TEXT("Контроль отдачи"), &ARSCharacter::RecoilControl, 0.f, 100.f, TEXT("%")) ])
			]

			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Контроль гасит долю подброса, а не всю отдачу:\nспрей остаётся живым, в отличие от «без отдачи».")))
				.Font(Font(9))
				.ColorAndOpacity(CMDim)
			]
		];
}

TSharedRef<SWidget> SRSCheatMenu::MakeVisualsPage()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			MakeSection(TEXT("ПРОТИВНИКИ"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("ESP / WH"), &ARSCharacter::bESP) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Рамка"), &ARSCharacter::bEspBox) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Chams (модель сквозь стены)"), &ARSCharacter::bChams) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Заливка силуэта"), &ARSCharacter::bEspFill) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Скелет"), &ARSCharacter::bEspSkeleton) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Полоса здоровья"), &ARSCharacter::bEspHealth) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Дистанция"), &ARSCharacter::bEspDist) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Линия к цели"), &ARSCharacter::bEspLine) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Метка упреждения"), &ARSCharacter::bEspMark) ])
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.Padding(8.f, 0.f, 0.f, 0.f)
		[
			MakeSection(TEXT("ЦВЕТ"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ MakeCombo(TEXT("Цвет вх"), &ARSCharacter::EspColor,
					{ TEXT("Красный"), TEXT("Зелёный"), TEXT("Синий"), TEXT("Жёлтый"),
					  TEXT("Фиолетовый"), TEXT("Белый") }) ])
		];
}

TSharedRef<SWidget> SRSCheatMenu::MakeMiscPage()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeSection(TEXT("ДВИЖЕНИЕ"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Распрыжка и скорость"), &ARSCharacter::bSpeedhack, true) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Автострейф в воздухе"), &ARSCharacter::bAirStrafe) ])
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			MakeSection(TEXT("ПРОЧЕЕ"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Мгновенная смена оружия"), &ARSCharacter::bQuickSwitch) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Звук попадания"), &ARSCharacter::bHitSound) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Лог урона"), &ARSCharacter::bCheatLogs) ]
				+ SVerticalBox::Slot().AutoHeight()[ MakeToggle(TEXT("Лог причин промаха"), &ARSCharacter::bCheatLogReasons) ])
		];
}

TSharedRef<SWidget> SRSCheatMenu::MakeSection(const FString& Title, TSharedRef<SWidget> Rows)
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f, 0.f, 0.f, 6.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Title))
			.Font(Font(9, true))
			.ColorAndOpacity(CMDim)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.f, 0.f, 0.f, 14.f)
		[
			Fill(CMPanel, Rows, 10.f)
		];
}

TSharedRef<SWidget> SRSCheatMenu::MakeToggle(const FString& Label, bool ARSCharacter::* Field, bool bSync)
{
	auto Get = [this, Field]() { const ARSCharacter* C = Owner.Get(); return C && C->*Field; };

	return SNew(SButton)
		.ButtonStyle(FCoreStyle::Get(), "NoBorder")
		.ContentPadding(0.f)
		.OnClicked_Lambda([this, Field, bSync]()
			{
				if (ARSCharacter* C = Owner.Get())
				{
					C->*Field = !(C->*Field);
					if (bSync) { Sync(); }
				}
				return FReply::Handled();
			})
		[
			SNew(SBorder)
			.BorderImage(WhiteBrush())
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
			.Padding(FMargin(8.f, 6.f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Label))
					.Font(Font(10))
					.ColorAndOpacity(CMText)
				]

				// сам тумблер: рамка и бегунок, как в эталоне
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(34.f)
					.HeightOverride(18.f)
					[
						SNew(SBorder)
						.BorderImage(WhiteBrush())
						.BorderBackgroundColor_Lambda([Get]() { return FSlateColor(Get() ? CMAccent : CMOff); })
						.Padding(2.f)
						[
							// бегунок двигаем отступом: выравнивание слота
							// значением не привяжешь, а отступ — можно
							SNew(SBorder)
							.BorderImage(WhiteBrush())
							.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
							.Padding_Lambda([Get]()
								{ return Get() ? FMargin(16.f, 0.f, 0.f, 0.f) : FMargin(0.f, 0.f, 16.f, 0.f); })
							[
								Fill(FLinearColor(0.93f, 0.94f, 0.96f), SNullWidget::NullWidget)
							]
						]
					]
				]
			]
		];
}

TSharedRef<SWidget> SRSCheatMenu::MakeSlider(const FString& Label, float ARSCharacter::* Field,
	float Min, float Max, const FString& Suffix, bool bSync)
{
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
		.Padding(FMargin(8.f, 6.f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.9f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(Font(10))
				.ColorAndOpacity(CMText)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f)
			[
				SNew(SSlider)
				.SliderBarColor(CMOff)
				.SliderHandleColor(CMAccent)
				.Value_Lambda([this, Field, Min, Max]()
					{
						const ARSCharacter* C = Owner.Get();
						return C ? (C->*Field - Min) / FMath::Max(0.001f, Max - Min) : 0.f;
					})
				.OnValueChanged_Lambda([this, Field, Min, Max, bSync](float V)
					{
						if (ARSCharacter* C = Owner.Get())
						{
							C->*Field = Min + V * (Max - Min);
							if (bSync) { Sync(); }
						}
					})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(58.f)
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Right)
					.Font(Font(10))
					.ColorAndOpacity(CMDim)
					.Text_Lambda([this, Field, Suffix]()
						{
							const ARSCharacter* C = Owner.Get();
							return FText::FromString(C
								? FString::Printf(TEXT("%.0f%s"), C->*Field, *Suffix) : TEXT("—"));
						})
				]
			]
		];
}

TSharedRef<SWidget> SRSCheatMenu::MakeIntSlider(const FString& Label, int32 ARSCharacter::* Field,
	int32 Min, int32 Max, const FString& Suffix)
{
	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
		.Padding(FMargin(8.f, 6.f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(0.9f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(Font(10))
				.ColorAndOpacity(CMText)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			.Padding(6.f, 0.f)
			[
				SNew(SSlider)
				.SliderBarColor(CMOff)
				.SliderHandleColor(CMAccent)
				.Value_Lambda([this, Field, Min, Max]()
					{
						const ARSCharacter* C = Owner.Get();
						return C ? float(C->*Field - Min) / FMath::Max(1, Max - Min) : 0.f;
					})
				.OnValueChanged_Lambda([this, Field, Min, Max](float V)
					{
						if (ARSCharacter* C = Owner.Get())
						{
							C->*Field = Min + FMath::RoundToInt(V * (Max - Min));
						}
					})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(58.f)
				[
					SNew(STextBlock)
					.Justification(ETextJustify::Right)
					.Font(Font(10))
					.ColorAndOpacity(CMDim)
					.Text_Lambda([this, Field, Suffix]()
						{
							const ARSCharacter* C = Owner.Get();
							return FText::FromString(C
								? FString::Printf(TEXT("%d%s"), C->*Field, *Suffix) : TEXT("—"));
						})
				]
			]
		];
}

TSharedRef<SWidget> SRSCheatMenu::MakeCombo(const FString& Label, int32 ARSCharacter::* Field,
	const TArray<FString>& Items, bool bSync)
{
	TSharedPtr<TArray<TSharedPtr<FString>>> Owned = MakeShared<TArray<TSharedPtr<FString>>>();
	for (const FString& Item : Items)
	{
		Owned->Add(MakeShared<FString>(Item));
	}
	ComboLists.Add(Owned);

	return SNew(SBorder)
		.BorderImage(WhiteBrush())
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
		.Padding(FMargin(8.f, 6.f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Label))
				.Font(Font(10))
				.ColorAndOpacity(CMText)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(150.f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(Owned.Get())
					.OnGenerateWidget_Lambda([](TSharedPtr<FString> In)
						{
							return SNew(STextBlock)
								.Text(FText::FromString(*In))
								.Font(Font(10))
								.ColorAndOpacity(CMText);
						})
					.OnSelectionChanged_Lambda([this, Field, Owned, bSync](TSharedPtr<FString> In, ESelectInfo::Type)
						{
							if (!In.IsValid()) { return; }
							if (ARSCharacter* C = Owner.Get())
							{
								C->*Field = Owned->IndexOfByPredicate(
									[&In](const TSharedPtr<FString>& X) { return X == In; });
								if (bSync) { Sync(); }
							}
						})
					[
						SNew(STextBlock)
						.Font(Font(10))
						.ColorAndOpacity(CMDim)
						.Text_Lambda([this, Field, Owned]()
							{
								const ARSCharacter* C = Owner.Get();
								const int32 Index = C ? C->*Field : 0;
								return FText::FromString(Owned->IsValidIndex(Index)
									? **(*Owned)[Index] : FString(TEXT("—")));
							})
					]
				]
			]
		];
}
