#include "RSFont.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/Engine.h"
#include "Fonts/CompositeFont.h"
#include "UObject/Package.h"

namespace RSFont
{
	namespace
	{
		// Кегль подобран так, чтобы вёрстка HUD осталась прежней: движковый
		// GetSmallFont даёт примерно такую же высоту строки, и все множители
		// масштаба в RSHUD.cpp (1.0-1.4) продолжают работать. Захочется
		// крупнее — менять здесь, в одном месте.
		// Кегли намеренно крупные: канвас растеризует шрифт в кеш этого
		// размера, а масштаб применяет к готовой картинке. При кегле 14
		// крупные надписи вроде счётчика HP растягивались вверх и мылились
		// ровно как прежний растровый шрифт. Теперь любой масштаб в HUD
		// (1.0-1.4 после компенсации в RenderScale) идёт вниз.
		constexpr int32 FontSize = 32;
		constexpr int32 FontSizeBig = 44;

		UFont* Cached = nullptr;
		UFont* CachedBig = nullptr;
		bool bTried = false;
		bool bVector = false;

		UFont* EngineFallback()
		{
			return GEngine ? GEngine->GetSmallFont() : nullptr;
		}
		UFont* EngineFallbackBig()
		{
			return GEngine ? GEngine->GetMediumFont() : nullptr;
		}

		// Собирает UFont с рантайм-кешем поверх готового FontFace.
		UFont* Build(UFontFace* Face, const TCHAR* Name, int32 Size)
		{
			UFont* F = NewObject<UFont>(GetTransientPackage(), Name);
			if (!F)
			{
				return nullptr;
			}

			F->FontCacheType = EFontCacheType::Runtime;
			F->LegacyFontSize = Size;

			FTypefaceEntry Entry;
			Entry.Name = TEXT("Default");
			Entry.Font = FFontData(Face);
			F->CompositeFont.DefaultTypeface.Fonts.Add(Entry);

			// Тот же приём, что у RSAudio/RSIcons: объект создан в рантайме,
			// без корня его снесёт первая же сборка мусора.
			F->AddToRoot();
			return F;
		}

		void EnsureLoaded()
		{
			if (bTried)
			{
				return;
			}
			bTried = true;

			UFontFace* Face = LoadObject<UFontFace>(nullptr, TEXT("/Game/UI/Fonts/FF_RSHud.FF_RSHud"));
			if (!Face)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("RS/шрифт: FF_RSHud не найден, HUD рисуется движковым растровым шрифтом"));
				return;
			}

			Cached = Build(Face, TEXT("RSHudFont"), FontSize);
			CachedBig = Build(Face, TEXT("RSHudFontBig"), FontSizeBig);
			bVector = (Cached != nullptr);

			UE_LOG(LogTemp, Display, TEXT("RS/шрифт: FF_RSHud подключён, кегли %d и %d"),
				FontSize, FontSizeBig);
		}
	}

	UFont* Get()
	{
		EnsureLoaded();
		return Cached ? Cached : EngineFallback();
	}

	UFont* GetBig()
	{
		EnsureLoaded();
		return CachedBig ? CachedBig : EngineFallbackBig();
	}

	bool IsVector()
	{
		Get();
		return bVector;
	}

	int32 BaseSize()
	{
		return FontSize;
	}

	float RenderScale(const UFont* F)
	{
		EnsureLoaded();
		if (!F || !GEngine)
		{
			return 1.f;
		}

		// Эталон меряем у движковых шрифтов прямо в рантайме: именно под их
		// высоту подбирались все размеры панелей в RSHUD.cpp, и угадывать
		// её константой было бы лишним источником расхождения.
		if (F == Cached)
		{
			static const float Ref = GEngine->GetSmallFont()
				? GEngine->GetSmallFont()->GetMaxCharHeight() : 12.f;
			return Ref / (float)FontSize;
		}
		if (F == CachedBig)
		{
			static const float Ref = GEngine->GetMediumFont()
				? GEngine->GetMediumFont()->GetMaxCharHeight() : 16.f;
			return Ref / (float)FontSizeBig;
		}
		return 1.f;
	}
}
