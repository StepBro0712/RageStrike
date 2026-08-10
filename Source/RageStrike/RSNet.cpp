#include "RSNet.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Common/UdpSocketBuilder.h"
#include "SocketSubsystem.h"
#include "Sockets.h"
#include "IPAddress.h"
#include "Async/Async.h"
#include "HAL/CriticalSection.h"

namespace RSNet
{
	namespace
	{
		FCriticalSection StateLock;
		FString PublicIP;
		FString PortMessage = TEXT("Проброс порта не запрашивался");
		EPortState PortState = EPortState::Idle;
		bool bIPRequested = false;
		bool bPortRequested = false;

		void SetPort(EPortState NewState, const FString& Message)
		{
			FScopeLock Lock(&StateLock);
			PortState = NewState;
			PortMessage = Message;
		}

		// --- шаг 1: найти роутер по SSDP ---------------------------------
		// Рассылаем M-SEARCH в мультикаст и ждём ответ шлюза: в нём есть
		// ссылка на описание устройства.
		FString DiscoverGatewayDescriptionURL()
		{
			ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
			if (!Sockets)
			{
				return FString();
			}

			FSocket* Socket = FUdpSocketBuilder(TEXT("RSNetSSDP"))
				.AsNonBlocking()
				.AsReusable()
				.WithBroadcast()
				.Build();
			if (!Socket)
			{
				return FString();
			}

			const FString Query =
				TEXT("M-SEARCH * HTTP/1.1\r\n")
				TEXT("HOST: 239.255.255.250:1900\r\n")
				TEXT("MAN: \"ssdp:discover\"\r\n")
				TEXT("MX: 2\r\n")
				TEXT("ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n\r\n");

			TSharedRef<FInternetAddr> Multicast = Sockets->CreateInternetAddr();
			bool bValid = false;
			Multicast->SetIp(TEXT("239.255.255.250"), bValid);
			Multicast->SetPort(1900);

			const FTCHARToUTF8 Utf8(*Query);
			int32 Sent = 0;
			Socket->SendTo((const uint8*)Utf8.Get(), Utf8.Length(), Sent, *Multicast);

			// роутер отвечает почти сразу, но даём две секунды
			FString Location;
			const double Deadline = FPlatformTime::Seconds() + 2.0;
			TArray<uint8> Buffer;
			Buffer.SetNumUninitialized(4096);

			while (FPlatformTime::Seconds() < Deadline && Location.IsEmpty())
			{
				uint32 Pending = 0;
				if (Socket->HasPendingData(Pending))
				{
					int32 Read = 0;
					TSharedRef<FInternetAddr> From = Sockets->CreateInternetAddr();
					if (Socket->RecvFrom(Buffer.GetData(), Buffer.Num() - 1, Read, *From) && Read > 0)
					{
						Buffer[Read] = 0;
						const FString Reply = FString(UTF8_TO_TCHAR((const char*)Buffer.GetData()));

						// ищем строку LOCATION: <url>
						FString Lower = Reply.ToLower();
						int32 At = Lower.Find(TEXT("location:"));
						if (At != INDEX_NONE)
						{
							FString Tail = Reply.Mid(At + 9);
							Tail.TrimStartInline();
							int32 End = INDEX_NONE;
							if (Tail.FindChar('\r', End) || Tail.FindChar('\n', End))
							{
								Tail = Tail.Left(End);
							}
							Location = Tail.TrimStartAndEnd();
						}
					}
				}
				else
				{
					FPlatformProcess::Sleep(0.05f);
				}
			}

			Socket->Close();
			Sockets->DestroySocket(Socket);
			return Location;
		}

		// из описания устройства достаём адрес службы, которая умеет
		// пробрасывать порты
		bool ParseControlURL(const FString& Xml, FString& OutControl, FString& OutServiceType)
		{
			const TCHAR* Candidates[] = {
				TEXT("urn:schemas-upnp-org:service:WANIPConnection:1"),
				TEXT("urn:schemas-upnp-org:service:WANIPConnection:2"),
				TEXT("urn:schemas-upnp-org:service:WANPPPConnection:1")
			};

			for (const TCHAR* Service : Candidates)
			{
				const int32 At = Xml.Find(Service);
				if (At == INDEX_NONE)
				{
					continue;
				}
				const int32 Open = Xml.Find(TEXT("<controlURL>"), ESearchCase::IgnoreCase,
					ESearchDir::FromStart, At);
				const int32 Close = Xml.Find(TEXT("</controlURL>"), ESearchCase::IgnoreCase,
					ESearchDir::FromStart, At);
				if (Open != INDEX_NONE && Close != INDEX_NONE && Close > Open)
				{
					OutControl = Xml.Mid(Open + 12, Close - Open - 12).TrimStartAndEnd();
					OutServiceType = Service;
					return true;
				}
			}
			return false;
		}

		// --- шаг 3: попросить роутер открыть порт -------------------------
		void SendAddPortMapping(const FString& BaseURL, const FString& ControlPath,
			const FString& ServiceType, const FString& LocalIP)
		{
			FString ControlURL = ControlPath;
			if (!ControlURL.StartsWith(TEXT("http")))
			{
				if (!ControlURL.StartsWith(TEXT("/")))
				{
					ControlURL = TEXT("/") + ControlURL;
				}
				ControlURL = BaseURL + ControlURL;
			}

			const FString Body = FString::Printf(
				TEXT("<?xml version=\"1.0\"?>")
				TEXT("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" ")
				TEXT("s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">")
				TEXT("<s:Body><u:AddPortMapping xmlns:u=\"%s\">")
				TEXT("<NewRemoteHost></NewRemoteHost>")
				TEXT("<NewExternalPort>%d</NewExternalPort>")
				TEXT("<NewProtocol>UDP</NewProtocol>")
				TEXT("<NewInternalPort>%d</NewInternalPort>")
				TEXT("<NewInternalClient>%s</NewInternalClient>")
				TEXT("<NewEnabled>1</NewEnabled>")
				TEXT("<NewPortMappingDescription>RageStrike</NewPortMappingDescription>")
				TEXT("<NewLeaseDuration>0</NewLeaseDuration>")
				TEXT("</u:AddPortMapping></s:Body></s:Envelope>"),
				*ServiceType, GamePort, GamePort, *LocalIP);

			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
			Req->SetURL(ControlURL);
			Req->SetVerb(TEXT("POST"));
			Req->SetHeader(TEXT("Content-Type"), TEXT("text/xml; charset=\"utf-8\""));
			Req->SetHeader(TEXT("SOAPAction"),
				FString::Printf(TEXT("\"%s#AddPortMapping\""), *ServiceType));
			Req->SetContentAsString(Body);
			Req->OnProcessRequestComplete().BindLambda(
				[](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
				{
					if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200)
					{
						SetPort(EPortState::Mapped, TEXT("Роутер открыл порт 7777 — можно играть через интернет"));
					}
					else
					{
						SetPort(EPortState::Failed,
							TEXT("Роутер отказал: включи UPnP или пробрось UDP 7777 вручную"));
					}
				});
			Req->ProcessRequest();
		}
	}

	const FString& GetPublicIP()
	{
		FScopeLock Lock(&StateLock);
		return PublicIP;
	}

	void RequestPublicIP()
	{
		if (bIPRequested)
		{
			return;
		}
		bIPRequested = true;

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetURL(TEXT("https://api.ipify.org"));
		Req->SetVerb(TEXT("GET"));
		Req->OnProcessRequestComplete().BindLambda(
			[](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
			{
				if (bOk && Resp.IsValid() && Resp->GetResponseCode() == 200)
				{
					const FString Body = Resp->GetContentAsString().TrimStartAndEnd();
					FScopeLock Lock(&StateLock);
					PublicIP = Body;
				}
			});
		Req->ProcessRequest();
	}

	EPortState GetPortState()
	{
		FScopeLock Lock(&StateLock);
		return PortState;
	}

	const FString& GetPortMessage()
	{
		FScopeLock Lock(&StateLock);
		return PortMessage;
	}

	void RequestPortMapping()
	{
		if (bPortRequested)
		{
			return;
		}
		bPortRequested = true;
		SetPort(EPortState::Working, TEXT("Спрашиваю роутер про порт..."));

		const FString LocalIP = GetLocalAddress();

		// поиск роутера блокирующий, поэтому уводим его в фоновый поток,
		// а запросы к нему делаем уже на игровом: HTTP-модуль сам асинхронный
		AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [LocalIP]()
		{
			const FString Location = DiscoverGatewayDescriptionURL();

			AsyncTask(ENamedThreads::GameThread, [Location, LocalIP]()
			{
				if (Location.IsEmpty())
				{
					SetPort(EPortState::Failed,
						TEXT("Роутер не ответил на UPnP: пробрось UDP 7777 вручную"));
					return;
				}

				// базовый адрес роутера без пути
				FString Base = Location;
				const int32 SchemeEnd = Base.Find(TEXT("://"));
				const int32 PathStart = (SchemeEnd != INDEX_NONE)
					? Base.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromStart, SchemeEnd + 3)
					: INDEX_NONE;
				if (PathStart != INDEX_NONE)
				{
					Base = Base.Left(PathStart);
				}

				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
				Req->SetURL(Location);
				Req->SetVerb(TEXT("GET"));
				Req->OnProcessRequestComplete().BindLambda(
					[Base, LocalIP](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
					{
						FString Control, ServiceType;
						if (bOk && Resp.IsValid()
							&& ParseControlURL(Resp->GetContentAsString(), Control, ServiceType))
						{
							SendAddPortMapping(Base, Control, ServiceType, LocalIP);
						}
						else
						{
							SetPort(EPortState::Failed,
								TEXT("Роутер не умеет UPnP: пробрось UDP 7777 вручную"));
						}
					});
				Req->ProcessRequest();
			});
		});
	}

	FString GetLocalAddress()
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets)
		{
			return TEXT("127.0.0.1");
		}

		// GetLocalHostAddr отдаёт первый адаптер, а им часто оказывается
		// виртуальный от Docker или WSL (172.x). Перебираем все и выбираем
		// настоящую домашнюю сеть: 192.168 лучше 10.x, а 172.x берём последним.
		TArray<TSharedPtr<FInternetAddr>> Adapters;
		if (Sockets->GetLocalAdapterAddresses(Adapters))
		{
			FString Best;
			int32 BestScore = -1;

			for (const TSharedPtr<FInternetAddr>& Addr : Adapters)
			{
				if (!Addr.IsValid())
				{
					continue;
				}
				const FString IP = Addr->ToString(false);

				// петля и link-local не годятся никому
				if (IP.StartsWith(TEXT("127.")) || IP.StartsWith(TEXT("169.254.")))
				{
					continue;
				}
				if (!IP.Contains(TEXT(".")))
				{
					continue; // IPv6 в игре по адресу не используем
				}

				int32 Score = 1;
				if (IP.StartsWith(TEXT("192.168.")))
				{
					Score = 4;
				}
				else if (IP.StartsWith(TEXT("10.")))
				{
					Score = 3;
				}
				else if (IP.StartsWith(TEXT("172.")))
				{
					Score = 2; // сюда попадают Docker и WSL
				}

				if (Score > BestScore)
				{
					BestScore = Score;
					Best = IP;
				}
			}

			if (!Best.IsEmpty())
			{
				return Best;
			}
		}

		bool bCanBind = false;
		TSharedPtr<FInternetAddr> Addr = Sockets->GetLocalHostAddr(*GLog, bCanBind);
		return Addr.IsValid() ? Addr->ToString(false) : TEXT("127.0.0.1");
	}

	FString GetJoinAddress()
	{
		const FString IP = GetPublicIP();
		if (IP.IsEmpty())
		{
			return TEXT("узнаю адрес...");
		}
		return FString::Printf(TEXT("%s:%d"), *IP, GamePort);
	}
}
