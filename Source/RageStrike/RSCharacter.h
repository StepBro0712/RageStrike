#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Engine/NetSerialization.h"
#include "RSTeam.h"
#include "RSWeaponData.h"
#include "RSCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UStaticMesh;
class UAnimSequence;
class USoundBase;
class ARSBot;

// общие помощники экономики и killfeed (используются и ботами)
int32 RSKillReward(ERSWeapon W);
FString RSCombatantName(const AActor* Who);

// Обстоятельства убийства для killfeed. Хранятся битовой маской: строка может
// нести сразу несколько значков — например хедшот вслепую сквозь дым.
namespace RSKill
{
	enum : uint8
	{
		Headshot = 1 << 0,
		Noscope  = 1 << 1,   // снайперка без прицела
		Blind    = 1 << 2,   // убийца сам был ослеплён
		Smoke    = 1 << 3,   // между убийцей и жертвой стоял дым
	};
}

// Считает маску по состоянию убийцы и жертвы в момент смерти. Работает и для
// игрока, и для бота: классы разные, а признаки одни и те же.
uint8 RSComputeKillFlags(const AActor* Killer, const AActor* Victim, bool bHeadshot);

UCLASS()
class RAGESTRIKE_API ARSCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARSCharacter();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void FellOutOfWorld(const UDamageType& DmgType) override;

	// --- Rage cheats (HUD reads these) ---
	bool bAimbot = false;       // F1
	bool bESP = false;          // F2
	bool bTriggerbot = false;   // F3
	bool bNoRecoilSpread = false; // F4
	bool bSpeedhack = false;    // F5 (+ auto-bhop)
	bool bSilentAim = false;    // F6
	bool bGodMode = false;      // F7
	bool bInfiniteMoney = false; // F8
	bool bAntiAim = false;      // тело смотрит не туда, куда целишься
	bool bPredict = false;      // упреждение: наводка туда, где цель окажется

	// Подменённый разворот тела: считает сервер, клиенты получают по репликации
	// и крутят по нему меш. Прицел от него не зависит — целимся по-настоящему.
	UPROPERTY(Replicated)
	float AntiAimYaw = 0.f;

	// наклон корпуса тоже уезжает клиентам: иначе у них тело стоит прямо
	UPROPERTY(Replicated)
	float AntiAimPitchRep = 0.f;

	// --- настройки читов (правятся в оверлее) ---
	// анти-аим: 0 — спиной, 1 — спин, 2 — дрожь
	int32 AntiAimMode = 0;
	float AntiAimSwing = 10.f;   // амплитуда качания в градусах
	float AntiAimPitch = -35.f;  // наклон корпуса: минус — головой вниз
	float AntiAimSpin = 180.f;   // скорость кручения в градусах в секунду

	// триггербот: в пределах какого угла от прицела он жмёт курок.
	// 0 — только точное попадание луча, как было раньше.
	float TriggerFov = 2.5f;

	// рейдж: конус поиска цели и порог урона, ниже которого чит не стреляет
	float RageFov = 90.f;    // 0-180°, от прицела
	float MinDamage = 15.f;  // не жать курок, если выстрел не нанесёт столько
	int32 AimHitbox = 0;     // куда целиться: 0 голова, 1 грудь, 2 живот
	float HitChance = 45.f;  // доля лучей в конусе разброса, которые должны попасть

	// --- legit: то же наведение, но по-человечески ---
	bool bLegitAim = false;        // подводить плавно, а не мгновенно
	float AimSmooth = 50.f;        // 0 — рывком, 100 — очень медленно
	float ReactionMs = 200.f;      // пауза после появления цели
	int32 AimActivation = 1;       // 0 всегда, 1 при стрельбе, 2 при прицеле
	float RecoilControl = 0.f;     // сколько процентов отдачи гасить
	float TriggerReactionMs = 200.f;

	// служебное для задержек
	TWeakObjectPtr<AActor> LastAimTarget;
	float TargetSeenAt = -1.f;
	float TriggerSeenAt = -1.f;

	// бэктрек: стрельба по тому, где цель была недавно
	bool bBacktrack = false;
	float BacktrackMs = 200.f;

	// бинд: пока клавиша зажата, действует другой порог урона
	int32 BindKeyIndex = 0;   // 0 нет, дальше — список в BindKey()
	float MinDamageAlt = 100.f;
	bool bBindHeld = false;   // считается в Tick опросом клавиши
	FKey BindKey() const;
	float EffectiveMinDamage() const { return bBindHeld ? MinDamageAlt : MinDamage; }

	// Двойной выстрел: заряд копится временем, пока не стреляешь, и стоит
	// ровно два выстрела — больше их и не бывает. Полный заряд — оба патрона
	// уходят мгновенно; неполный — один, и прогресс сбрасывается.
	bool bDoubleTap = false;
	float DoubleTapCharge = 0.f; // накоплено секунд
	// хватит ли одного выстрела, чтобы добить цель под прицелом
	bool WouldKillWithOneShot() const;

	// Конфиги читов: набор настроек под именем. Лежат в Saved/Cheats/<имя>.ini,
	// поэтому переживают перезапуск и не мешают друг другу.
	FString CurrentConfig = TEXT("default");
	void SaveCheatConfig(const FString& Name);
	bool LoadCheatConfig(const FString& Name);
	static TArray<FString> ListCheatConfigs();
	static FString CheatConfigPath(const FString& Name);

	// Chams: подсветка моделей сквозь стены пост-процессом. Меши врагов
	// помечаются в CustomDepth, материал сравнивает её со сценой и заливает
	// закрытые куски цветом. Silhouette-заливка вх этого не умеет — она
	// рисует прямоугольник, а тут светится сама модель.
	bool bChams = false;
	UPROPERTY() class UMaterialInstanceDynamic* ChamsMID = nullptr;
	void UpdateChams();

	// автозакупка набора из инвентаря, раз в раунд в фазу закупки
	void UpdateAutoBuy();
	int32 LastAutoBuyRound = -1;

	// прочее: мелочи, которые в CS делают руками
	bool bQuickSwitch = false; // достаём оружие мгновенно, без анимации
	bool bHitSound = false;    // щелчок при попадании
	bool bAirStrafe = false;   // автоматический стрейф в воздухе
	float LastAirYaw = 0.f;    // для расчёта поворота мыши в прыжке

	// Лог событий чита. По умолчанию — только нанесённый и полученный урон:
	// причины отказа стрелять пишутся каждый кадр и забивают ленту, поэтому
	// вынесены в отдельный переключатель. Правило простое: строка с ключом
	// подавления — это диагностика, без ключа — событие боя.
	bool bCheatLogs = true;
	bool bCheatLogReasons = false;
	struct FRSCheatLog { FString Text; float Time; FLinearColor Color; };
	TArray<FRSCheatLog> CheatLogLines;
	void CheatLog(const FString& Text, const FLinearColor& Color, const FString& ThrottleKey = FString());

	// вх: силуэт и скелет рисуются поверх геометрии, то есть видны сквозь стены
	bool bEspSkeleton = false;
	bool bEspFill = false;
	int32 EspColor = 0;
	static FLinearColor EspPalette(int32 Index);

	// предикт: тики по 1/64 с, как серверный такт в CS
	int32 PredictTicks = 4;
	bool bPredictOnlyHidden = true; // упреждать только тех, кого не видно

	// что рисует вх
	bool bEspBox = true;
	bool bEspHealth = true;
	bool bEspDist = true;
	bool bEspLine = true;
	bool bEspMark = true; // метка упреждения

	static constexpr float PredictTickSeconds = 1.f / 64.f;
	static constexpr int32 AntiAimModes = 3;

	// Куда целятся по видимой модели: с анти-аимом это не центр капсулы,
	// поэтому боты начинают мазать. Хитбокс — капсула, она не вращается.
	FVector GetVisibleAimPoint() const;

	// Где цель окажется через PredictTicks тиков. bTargetHidden — не видно ли
	// её сейчас: по умолчанию упреждение работает только по скрытым целям,
	// ради префайра из-за угла, а не вместо обычной наводки.
	// (Имя не bHidden — так называется поле AActor.)
	FVector PredictPoint(const AActor* Target, bool bTargetHidden = true) const;
	bool IsVisibleTo(const AActor* Target) const;

	// Хитбоксы: попадание разбирается по кости, в которую пришёл луч.
	// Раньше голова определялась по высоте точки — на присевших и на
	// наклонённых анти-аимом это врало.
	enum class ERSHitbox : uint8 { Head, Chest, Stomach, Limb };
	static ERSHitbox HitboxFromBone(FName Bone);
	static float HitboxMult(ERSHitbox Box);
	// куда бьёт этот хитбокс по высоте — для наводки аимбота
	static float HitboxHeight(int32 Index);

	// Урон, который нанесёт это попадание: общий счёт для стрельбы и для
	// проверки «а стоит ли жать курок». 0 — не по врагу.
	float DamageForHit(const struct FHitResult& Hit, ERSWeapon Weapon,
		const FVector& Start, bool& bOutHeadshot) const;
	// Что нанесёт выстрел прямо сейчас, по текущему направлению взгляда
	float DamageIfFiredNow() const;
	// выстрел от чита: только если урон не ниже порога и шанс попасть достаточный
	void TryFireIfWorthIt();
	// доля лучей в конусе разброса, попадающих во врага с нужным уроном
	float EstimateHitChance() const;

	// история позиций врагов для бэктрека: пишется каждый кадр у стрелка,
	// чтобы не трогать сами цели
	struct FRSPastPos { FVector Location; float Time; };
	TMap<TWeakObjectPtr<AActor>, TArray<FRSPastPos>> EnemyHistory;
	void RecordEnemyHistory();
	// точка из прошлого, по которой стоит стрелять; если бэктрек выключен
	// или истории нет — возвращает Fallback
	FVector BacktrackPoint(const AActor* Target, const FVector& Fallback) const;

	// --- State (HUD reads these) ---
	UPROPERTY(Replicated)
	float Health = 100.f;

	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Weapon)
	ERSWeapon CurrentWeapon = ERSWeapon::Knife;

	UPROPERTY(ReplicatedUsing = OnRep_Team)
	ERSTeam Team = ERSTeam::CT;

	UPROPERTY(Replicated)
	bool bAlive = true;

	// ник владельца: задаётся из меню, сервер раздаёт его всем
	UPROPERTY(Replicated)
	FString Nick;

	void ApplyNick(const FString& NewNick);

	UFUNCTION(Server, Reliable)
	void ServerSetNick(const FString& NewNick);

	// экономика
	UPROPERTY(Replicated)
	int32 Money = 800;

	// броня как в CS: кевлар гасит половину урона, шлем защищает голову
	UPROPERTY(Replicated)
	float Armor = 0.f;

	UPROPERTY(Replicated)
	bool bHasHelmet = false;

	// слоты: основное, пистолет, нож (всегда), гранаты
	UPROPERTY(Replicated)
	bool bHasPrimary = false;

	UPROPERTY(Replicated)
	ERSWeapon PrimaryType = ERSWeapon::AK47;

	UPROPERTY(Replicated)
	bool bHasSecondary = true;

	UPROPERTY(Replicated)
	ERSWeapon SecondaryType = ERSWeapon::Glock;

	// счётчики гранат по типам: HE, флешка, дым, молотов, зажигалка
	UPROPERTY(Replicated)
	uint8 Grenades[5] = { 0, 0, 0, 0, 0 };

	static constexpr int32 PriceKevlar = 650;
	static constexpr int32 PriceKevlarHelmet = 1000;

	// true, если слот свободен и оружие удалось взять
	bool TryPickUpWeapon(ERSWeapon Type);

	void AddMoney(int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerBuyWeapon(ERSWeapon Weapon);

	UFUNCTION(Server, Reliable)
	void ServerBuyArmor(bool bWithHelmet);

	void SetTeam(ERSTeam NewTeam);

	// раунды: погибший ждёт следующего раунда, а не воскресает сразу
	void RespawnForRound(const FVector& Location);
	// до начала первого раунда пешку держим неподвижной, иначе она падает
	void FreezeUntilRound();

	AActor* SpectateTarget = nullptr;
	bool bBuyMenuOpen = false;
	bool bCheatMenuOpen = false;   // оверлей читов, Del или Insert

	// переключение чита по номеру строки оверлея: читы включаются мышью,
	// клавиш F1-F8 больше нет
	void ToggleCheatByIndex(int32 Index);
	// правка настройки из оверлея: Delta -1 или +1, у переключателей знак
	// не важен. Номера настроек совпадают с порядком строк в оверлее.
	void ApplyCheatSetting(int32 Id, int32 Delta);
	// вид от третьего лица снаружи: в лобби на персонажа смотрят со стороны
	void SetThirdPerson(bool bOn);
	int32 BuyCategory = -1;      // -1 — выбор категории
	bool bScoreboardOpen = false;

	bool bReloading = false;
	bool bThirdPerson = false;
	float CurrentSpreadDeg = 0.f;
	float LastHitMarkerTime = -100.f;
	float LastDamagedTime = -100.f;
	bool bAimingNow = false;         // HUD: прицел снайпера

	// флешка: белая пелена на экране локального игрока
	float FlashEndTime = -10.f;
	float FlashDuration = 1.f;

	// сервер помечает последний хит хедшотом для killfeed
	bool bLastHitHeadshot = false;

	// подтверждение разметки спавна, показывает HUD
	FString SpawnMarkMessage;
	float SpawnMarkUntil = -10.f;

	int32 GetAmmo() const;
	int32 GetMaxAmmo() const;
	int32 GetReserveAmmo() const;
	FString GetWeaponName() const;
	float GetWeaponMaxSpeed() const; // скорость зависит от оружия, как в CS

	UPROPERTY(VisibleAnywhere)
	UCameraComponent* Camera;

	UPROPERTY(VisibleAnywhere)
	USpringArmComponent* SpringArm;

	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* ArmsMesh;   // анимированные руки от первого лица

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* GunMesh;      // вид от первого лица (только владелец)

	// скелетная вьюмодель с анимациями CS2 — там, где модель приехала с ригом
	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* FPGun;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* TPGunMesh;    // оружие в руке для остальных игроков

protected:
	virtual void BeginPlay() override;

private:
	bool IsFrozen() const; // закупка и пауза между раундами

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void StartJump();
	void StopJump();
	void StartCrouchInput();
	void StopCrouchInput();
	// Shift — тихая ходьба, как в CS (спринта там нет)
	void StartWalk();
	void StopWalk();
	void StartFire();
	void StopFire();
	void StartAim();
	void StopAim();
	void Reload();
	void FinishReload();
	void ToggleView();

	// цифровые клавиши: вне закупки — слоты, в закупке — навигация по меню
	void Num1() { HandleNumberKey(1); }
	void Num2() { HandleNumberKey(2); }
	void Num3() { HandleNumberKey(3); }
	void Num4() { HandleNumberKey(4); }
	void Num5() { HandleNumberKey(5); }
	void Num6() { HandleNumberKey(6); }
	void Num0() { HandleNumberKey(0); }
	void HandleNumberKey(int32 N);

	void SelectGrenade(); // 4 — листает имеющиеся гранаты
	void StartInspect();  // F — осмотр оружия

	// разметка спавнов: встать в нужное место и нажать F11 (T) или F12 (CT)
	void MarkSpawnT() { MarkSpawn(false); }
	void MarkSpawnCT() { MarkSpawn(true); }
	void MarkSpawn(bool bCT);
	// закупка открыта, пока держим клавишу
	void ToggleBuyMenu();
	void SetBuyMenuOpen(bool bOpen);
	void OpenBuyMenu()  { SetBuyMenuOpen(true); }
	void CloseBuyMenu() { SetBuyMenuOpen(false); }
	void ShowScoreboard() { bScoreboardOpen = true; }
	void HideScoreboard() { bScoreboardOpen = false; }
	void DropCurrentWeapon();
	void DropWeapon(ERSWeapon Type);

	// выдать оружие в слот с полным боезапасом (сервер)
	void GiveWeapon(ERSWeapon Type);

	UFUNCTION(Server, Reliable)
	void ServerDropWeapon();
	void RequestWeapon(ERSWeapon NewWeapon);

	UFUNCTION()
	void OnRep_Weapon();

	UFUNCTION()
	void OnRep_Team();

	UFUNCTION(Server, Reliable)
	void ServerSetTeam(ERSTeam NewTeam);

	void ApplyTeamVisuals();

	UFUNCTION(Server, Reliable)
	void ServerSetWeapon(ERSWeapon NewWeapon);

	void ApplyWeaponVisuals();
	void ApplyViewMode();

	// меню читов правит поля персонажа напрямую и само себя закрывает
	friend class SRSCheatMenu;

	void ToggleCheatMenu();

	// меню читов на Slate: живёт, пока открыто
	TSharedPtr<class SRSCheatMenu> CheatMenuWidget;
	TSharedPtr<class SWeakWidget> CheatMenuContainer;
	void ToggleAimbot()   { bAimbot = !bAimbot; }
	void ToggleESP()      { bESP = !bESP; }
	void ToggleTrigger()  { bTriggerbot = !bTriggerbot; }
	void ToggleNoRecoil() { bNoRecoilSpread = !bNoRecoilSpread; }
	void ToggleSpeed()    { bSpeedhack = !bSpeedhack; SyncCheats(); }
	void ToggleSilent()   { bSilentAim = !bSilentAim; }
	void ToggleGod()      { bGodMode = !bGodMode; SyncCheats(); }
	void ToggleMoney()    { bInfiniteMoney = !bInfiniteMoney; SyncCheats(); }
	void ToggleAntiAim()  { bAntiAim = !bAntiAim; SyncCheats(); }
	void TogglePredict()  { bPredict = !bPredict; }

	// годмод, спидхак, деньги и анти-аим должен знать сервер: урон, скорость,
	// кошелёк и подменённый разворот тела считаются на нём
	void SyncCheats();

	UFUNCTION(Server, Reliable)
	void ServerSyncCheats(bool bInGod, bool bInSpeed, bool bInMoney, bool bInAntiAim,
		int32 InMode, float InSwing, float InPitch, float InSpin);

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector Start, FVector_NetQuantizeNormal Dir, ERSWeapon Weapon);

	UFUNCTION(Server, Reliable)
	void ServerThrowGrenade(FVector Start, FVector_NetQuantizeNormal Dir, ERSWeapon Weapon, bool bLob);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastTracer(FVector Start, FVector End, bool bMelee);

	UFUNCTION(Client, Unreliable)
	void ClientHitMarker();

	UFUNCTION(Client, Unreliable)
	void ClientDamaged();

	void DoFireTrace(const FVector& Start, const FVector& Dir, ERSWeapon Weapon);
	void FireOnePellet(const FVector& Start, const FVector& Dir, ERSWeapon Weapon);

	// цель для читов — любой живой противник: бот или игрок
	AActor* FindBestTarget(FVector& OutAimPoint) const;
	bool IsEnemyActor(const AActor* Other) const;
	void RunAimbot(float DeltaTime);
	void RunTriggerbot();
	// bIgnoreCadence — выстрел вне очереди, для двойного выстрела
	void TryFire(bool bIgnoreCadence = false);
	void ThrowGrenade();
	void Die();

	// наблюдение после смерти
	void CycleSpectate();
	void StopSpectating();

	UFUNCTION(Server, Reliable)
	void ServerCycleSpectate();

	// управление ботами
	void AddBot();
	void RemoveBot();
	void ClearBots();

	UFUNCTION(Server, Reliable)
	void ServerAddBot(FVector Location);

	UFUNCTION(Server, Reliable)
	void ServerRemoveBot(ARSBot* Bot);

	UFUNCTION(Server, Reliable)
	void ServerClearBots();

	UPROPERTY()
	USoundBase* FireSound = nullptr;

	UPROPERTY()
	UStaticMesh* AKAsset = nullptr;      // винтовки Т и общий силуэт

	UPROPERTY()
	UStaticMesh* M4Asset = nullptr;      // винтовки CT, ПП и дробовики

	UPROPERTY()
	UStaticMesh* SniperAsset = nullptr;

	UPROPERTY()
	UStaticMesh* KnifeAsset = nullptr;

	UPROPERTY()
	UStaticMesh* PistolAsset = nullptr;

	UPROPERTY()
	UStaticMesh* GrenadeAsset = nullptr; // сфера-болванка

	// длина ствола в сантиметрах: по ней модель приводится к нужному размеру
	static float GetWeaponRealLength(ERSWeapon W);

	UPROPERTY()
	USkeletalMesh* CTBodyMesh = nullptr;

	UPROPERTY()
	USkeletalMesh* TBodyMesh = nullptr;

	// анимации рук проигрываются напрямую, без анимационного блюпринта
	UPROPERTY()
	UAnimSequence* AnimIdle = nullptr;

	UPROPERTY()
	UAnimSequence* AnimWalk = nullptr;

	UPROPERTY()
	UAnimSequence* AnimFire = nullptr;

	UPROPERTY()
	UAnimSequence* AnimReload = nullptr;

	void UpdateArmsAnimation();
	void PlayArmsAnim(UAnimSequence* Anim, bool bLoop, float LockSeconds);

	// скелетная вьюмодель: настоящие анимации вместо процедурных
	bool bUsingSkeletalVM = false;
	float VMAnimLockUntil = 0.f;
	void PlayVMAnim(UAnimSequence* Anim, bool bLoop, float LockSeconds, float PlayRate = 1.f);
	void UpdateSkeletalViewModel();

	UAnimSequence* CurrentArmsAnim = nullptr;
	float ArmsAnimLockUntil = 0.f;

	// патроны по каждому оружию: магазин и запас
	int32 Ammo[RSWeapons::Count] = { 0 };
	int32 Reserve[RSWeapons::Count] = { 0 };

	FVector GunBaseLoc = FVector::ZeroVector;
	FRotator GunBaseRot = FRotator::ZeroRotator;

	// оружие в руке тела: поворот пересчитывается каждый кадр под кость,
	// поэтому смещение пивота компенсируется там же, в Tick
	FVector TPGunBaseLoc = FVector::ZeroVector;
	FVector TPGunPivot = FVector::ZeroVector;
	// доворот конкретной модели под ось ствола: считается по габаритам в
	// ApplyWeaponVisuals и обязан применяться в Tick — иначе пересчёт поворота
	// под кость затирает его, и модели с длинной осью вверх (AWP) висят стоймя
	FQuat TPGunAlign = FQuat::Identity;

	// магазин кончился этим выстрелом — перезарядимся в конце кадра
	bool bAutoReloadPending = false;

	bool bFireHeld = false;
	bool bShotSincePress = false; // полуавтомат: один выстрел на нажатие
	bool bJumpHeld = false;
	bool bWalking = false;
	bool bAiming = false;
	float LastFireTime = -10.f;
	float SpreadBloom = 0.f;
	float SpawnProtectionUntil = 0.f;
	float ReloadEndTime = 0.f;
	float ReloadDuration = 2.f;
	FTimerHandle ReloadTimer;

	// --- отдача как в CS: детерминированный паттерн + возврат камеры ---
	static void GetRecoilKick(ERSWeapon W, int32 Index, float& OutPitch, float& OutYaw);
	int32 RecoilIndex = 0;
	FVector2D PunchCurrent = FVector2D::ZeroVector; // X — питч, Y — йо
	FVector2D PunchTarget = FVector2D::ZeroVector;
	void UpdateRecoil(float DeltaTime);

	// замедление от попаданий (tagging)
	float TaggedUntil = -10.f;

	// шаги слышны и от чужих пешек: движение реплицируется, поэтому каждый
	// клиент озвучивает всех сам
	void UpdateFootsteps(float DeltaTime);
	float StepDistance = 0.f;
	bool bWasFallingAudio = false;

	// музыка закупки: включается на время фазы покупок
	UPROPERTY()
	class UAudioComponent* BuyMusic = nullptr;

	// --- процедурная анимация вьюмодели ---
	void UpdateViewmodel(float DeltaTime);
	float BobTime = 0.f;
	float GunKick = 0.f;          // толчок ствола назад при выстреле
	float DrawStartTime = -10.f;  // анимация доставания
	float InspectEndTime = -10.f; // осмотр по F
	float LandDipStart = -10.f;   // присед камеры после приземления
	bool bWasFalling = false;
	FVector SwayLoc = FVector::ZeroVector;
	FRotator SwayRot = FRotator::ZeroRotator;
	FRotator PrevControlRot = FRotator::ZeroRotator;
};
