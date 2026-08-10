#pragma once

#include "CoreMinimal.h"

// Сетевая игра через интернет.
//
// Игра поднимает обычный listen-сервер Unreal на UDP-порту 7777. Чтобы к нему
// можно было подключиться из другого города, нужно две вещи: знать свой
// внешний адрес и чтобы роутер пропускал этот порт внутрь. Первое узнаём у
// внешнего сервиса, второе просим у роутера по UPnP.
namespace RSNet
{
	static constexpr int32 GamePort = 7777;

	enum class EPortState : uint8
	{
		Idle,       // ещё не пробовали
		Working,    // запрос ушёл, ждём
		Mapped,     // роутер согласился пробросить порт
		Failed      // UPnP выключен или не поддерживается
	};

	// Внешний адрес: пустая строка, пока запрос не вернулся.
	const FString& GetPublicIP();
	void RequestPublicIP();

	// Проброс порта на роутере. Асинхронно, состояние читаем через GetPortState.
	EPortState GetPortState();
	const FString& GetPortMessage();
	void RequestPortMapping();

	// Адрес для друзей: "1.2.3.4:7777" или подсказка, если адрес ещё не готов.
	FString GetJoinAddress();

	// Локальный адрес в домашней сети — для игры по LAN.
	FString GetLocalAddress();
}
