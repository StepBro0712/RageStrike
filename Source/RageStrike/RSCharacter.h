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

	void ToggleCheatMenu();
	void ToggleAimbot()   { bAimbot = !bAimbot; }
	void ToggleESP()      { bESP = !bESP; }
	void ToggleTrigger()  { bTriggerbot = !bTriggerbot; }
	void ToggleNoRecoil() { bNoRecoilSpread = !bNoRecoilSpread; }
	void ToggleSpeed()    { bSpeedhack = !bSpeedhack; SyncCheats(); }
	void ToggleSilent()   { bSilentAim = !bSilentAim; }
	void ToggleGod()      { bGodMode = !bGodMode; SyncCheats(); }
	void ToggleMoney()    { bInfiniteMoney = !bInfiniteMoney; SyncCheats(); }

	// годмод, спидхак и деньги должен знать сервер: урон, скорость и кошелёк
	// считаются на нём
	void SyncCheats();

	UFUNCTION(Server, Reliable)
	void ServerSyncCheats(bool bInGod, bool bInSpeed, bool bInMoney);

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
	void RunAimbot();
	void RunTriggerbot();
	void TryFire();
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
