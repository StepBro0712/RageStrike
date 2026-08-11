#include "RSLoadingScreen.h"
#include "MoviePlayer.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"

namespace
{
	// Надпись проявляется за секунду: сразу после чёрного экрана резкое
	// появление текста выглядит как рывок.
	class SRSLoadingWidget : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRSLoadingWidget) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			StartTime = FPlatformTime::Seconds();

			ChildSlot
			[
				SNew(SOverlay)

				// чёрный фон на весь экран
				+ SOverlay::Slot()
				[
					SNew(SBorder)
					.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.BorderBackgroundColor(FLinearColor::Black)
				]

				// надпись по центру, во всю ширину экрана
				+ SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 110))
					.Text(FText::FromString(TEXT("RAGESTRIKE")))
					.ColorAndOpacity(this, &SRSLoadingWidget::GetTitleColor)
				]

				// индикатор в правом нижнем углу, как в больших играх
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Bottom)
				.Padding(0.f, 0.f, 52.f, 44.f)
				[
					SNew(SCircularThrobber)
					.Radius(22.f)
					.NumPieces(10)
					.Period(0.75f)
				]
			];
		}

	private:
		FSlateColor GetTitleColor() const
		{
			const float Age = (float)(FPlatformTime::Seconds() - StartTime);
			return FSlateColor(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Age, 0.f, 1.f)));
		}

		double StartTime = 0.0;
	};

	void OnPreLoadMap(const FString& /*MapName*/)
	{
		if (!IsMoviePlayerEnabled())
		{
			return;
		}

		FLoadingScreenAttributes Attributes;
		Attributes.bAutoCompleteWhenLoadingCompletes = true;
		// секунда минимума: иначе на быстрой загрузке экран моргает
		Attributes.MinimumLoadingScreenDisplayTime = 1.f;
		Attributes.WidgetLoadingScreen = SNew(SRSLoadingWidget);
		GetMoviePlayer()->SetupLoadingScreen(Attributes);
	}
}

namespace RSLoadingScreen
{
	void Register()
	{
		FCoreUObjectDelegates::PreLoadMap.AddStatic(&OnPreLoadMap);
		// первый уровень грузится до первой смены карты, поэтому экран
		// ставим сразу — иначе при запуске окно просто чёрное
		OnPreLoadMap(FString());
	}

	void Unregister()
	{
		FCoreUObjectDelegates::PreLoadMap.RemoveAll(nullptr);
	}
}
