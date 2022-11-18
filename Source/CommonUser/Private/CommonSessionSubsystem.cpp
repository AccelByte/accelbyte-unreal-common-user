// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonSessionSubsystem.h"

#include <OnlineSessionSettingsAccelByte.h>

#include "OnlineSessionInterfaceV1AccelByte.h"
#include "OnlineSubsystemAccelByteDefines.h"
#include "GameFramework/GameModeBase.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"

#if COMMONUSER_OSSV1
#include "OnlineSessionInterfaceV2AccelByte.h"
#include "OnlineIdentityInterfaceAccelByte.h"
#include "OnlineSubsystemAccelByteSessionSettings.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "OnlineSubsystem.h"
#include "Online.h"
#include "OnlineSubsystemSessionSettings.h"
#include "OnlineSubsystemUtils.h"

FName SETTING_ONLINESUBSYSTEM_VERSION(TEXT("OSSv1"));
#else
#include "Online/OnlineSessionNames.h"
#include "Online/OnlineServicesEngineUtils.h"

FName SETTING_ONLINESUBSYSTEM_VERSION(TEXT("OSSv2"));
using namespace UE::Online;
#endif // COMMONUSER_OSSV1
#include "OnlineSubsystemAccelByte.h"

#if COMMONUSER_OSSV1
// #SESSIONv2 Define custom session settings
#define SETTING_HOSTNAME FName(TEXT("HOSTNAME"))
#define SETTING_SESSIONNAME FName(TEXT("SESSIONNAME"))
#define SETTING_LOCALADDRESS FName(TEXT("LOCALADDRESS"))
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogCommonSession, Log, All);
DEFINE_LOG_CATEGORY(LogCommonSession);

#define LOCTEXT_NAMESPACE "CommonUser"

#define EARLY_RETURN_IF_INVALID(Condition, Subject, ReturnValue)        \
	if(!ensure(Condition))                                              \
	{                                                                   \
		UE_LOG(LogCommonSession, Warning, TEXT(Subject " is invalid")); \
		return ReturnValue;                                             \
	}

#define EARLY_RETURN_IF_INVALID_WITH_DELEGATE(Condition, Subject, ...)  \
	if(!ensure(Condition))                                              \
	{                                                                   \
		UE_LOG(LogCommonSession, Warning, TEXT(Subject " is invalid")); \
		OnComplete.ExecuteIfBound(__VA_ARGS__);                         \
		return;                                                         \
	}

//////////////////////////////////////////////////////////////////////
//UCommonSession_SearchResult

void UCommonSession_SearchSessionRequest::NotifySearchFinished(bool bSucceeded, const FText& ErrorMessage)
{
	OnSearchFinished.Broadcast(bSucceeded, ErrorMessage);
	K2_OnSearchFinished.Broadcast(bSucceeded, ErrorMessage);
}


//////////////////////////////////////////////////////////////////////
//UCommonSession_SearchResult

#if COMMONUSER_OSSV1
FString UCommonSession_SearchResult::GetDescription() const
{
	return Result.GetSessionIdStr();
}

void UCommonSession_SearchResult::GetStringSetting(FName Key, FString& Value, bool& bFoundValue) const
{
	bFoundValue = Result.Session.SessionSettings.Get<FString>(Key, /*out*/ Value);
}

void UCommonSession_SearchResult::GetIntSetting(FName Key, int32& Value, bool& bFoundValue) const
{
	bFoundValue = Result.Session.SessionSettings.Get<int32>(Key, /*out*/ Value);
}

int32 UCommonSession_SearchResult::GetNumOpenPrivateConnections() const
{
	return Result.Session.NumOpenPrivateConnections;
}

int32 UCommonSession_SearchResult::GetNumOpenPublicConnections() const
{
	return Result.Session.NumOpenPublicConnections;
}

int32 UCommonSession_SearchResult::GetMaxPublicConnections() const
{
	return Result.Session.SessionSettings.NumPublicConnections;
}

int32 UCommonSession_SearchResult::GetPingInMs() const
{
	return Result.PingInMs;
}

FString UCommonSession_SearchResult::GetUsername() const
{
	return Result.Session.OwningUserName;
}

FString UCommonSession_SearchResult::GetOwningAccelByteIdString() const
{
	const TSharedRef<const FUniqueNetIdAccelByteUser> ABUser = FUniqueNetIdAccelByteUser::Cast(*Result.Session.OwningUserId);
	return ABUser->GetAccelByteId();
}

#else
FString UCommonSession_SearchResult::GetDescription() const
{
	return ToLogString(Lobby->LobbyId);
}

void UCommonSession_SearchResult::GetStringSetting(FName Key, FString& Value, bool& bFoundValue) const
{
	if (const FLobbyVariant* VariantValue = Lobby->Attributes.Find(Key))
	{
		bFoundValue = true;
		Value = VariantValue->GetString();
	}
	else
	{
		bFoundValue = false;
	}
}

void UCommonSession_SearchResult::GetIntSetting(FName Key, int32& Value, bool& bFoundValue) const
{
	if (const FLobbyVariant* VariantValue = Lobby->Attributes.Find(Key))
	{
		bFoundValue = true;
		Value = (int32)VariantValue->GetInt64();
	}
	else
	{
		bFoundValue = false;
	}
}

int32 UCommonSession_SearchResult::GetNumOpenPrivateConnections() const
{
	// TODO:  Private connections
	return 0;
}

int32 UCommonSession_SearchResult::GetNumOpenPublicConnections() const
{
	return Lobby->MaxMembers - Lobby->Members.Num();
}

int32 UCommonSession_SearchResult::GetMaxPublicConnections() const
{
	return Lobby->MaxMembers;
}

int32 UCommonSession_SearchResult::GetPingInMs() const
{
	// TODO:  Not a property of lobbies.  Need to implement with sessions.
	return 0;
}
#endif //COMMONUSER_OSSV1


class FCommonOnlineSearchSettingsBase : public FGCObject
{
public:
	FCommonOnlineSearchSettingsBase(UCommonSession_SearchSessionRequest* InSearchRequest)
	{
		SearchRequest = InSearchRequest;
	}

	virtual ~FCommonOnlineSearchSettingsBase() {}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(SearchRequest);
	}

	virtual FString GetReferencerName() const override
	{
		static const FString NameString = TEXT("FCommonOnlineSearchSettings");
		return NameString;
	}

public:
	UCommonSession_SearchSessionRequest* SearchRequest = nullptr;
};

#if COMMONUSER_OSSV1
//////////////////////////////////////////////////////////////////////
// FCommonSession_OnlineSessionSettings

class FCommonSession_OnlineSessionSettings : public FOnlineSessionSettings
{
public:

	FCommonSession_OnlineSessionSettings(bool bIsLAN = false, bool bIsPresence = false, int32 MaxNumPlayers = 4)
	{
		NumPublicConnections = MaxNumPlayers;
		if (NumPublicConnections < 0)
		{
			NumPublicConnections = 0;
		}
		NumPrivateConnections = 0;
		bIsLANMatch = bIsLAN;
		bShouldAdvertise = true;
		bAllowJoinInProgress = true;
		bAllowInvites = true;
		bUsesPresence = bIsPresence;
		bAllowJoinViaPresence = true;
		bAllowJoinViaPresenceFriendsOnly = false;
	}

	virtual ~FCommonSession_OnlineSessionSettings() {}
};

//////////////////////////////////////////////////////////////////////
// FCommonOnlineSearchSettingsOSSv1

class FCommonOnlineSearchSettingsOSSv1 : public FOnlineSessionSearch, public FCommonOnlineSearchSettingsBase
{
public:
	FCommonOnlineSearchSettingsOSSv1(UCommonSession_SearchSessionRequest* InSearchRequest)
		: FCommonOnlineSearchSettingsBase(InSearchRequest)
	{
		bIsLanQuery = (InSearchRequest->OnlineMode == ECommonSessionOnlineMode::LAN);
		MaxSearchResults = 250;
		PingBucketSize = 50;

		//#TODO #SESSIONv2 Set any additional QuerySettings here
	}

	virtual ~FCommonOnlineSearchSettingsOSSv1() {}
};
#else

class FCommonOnlineSearchSettingsOSSv2 : public FCommonOnlineSearchSettingsBase
{
public:
	FCommonOnlineSearchSettingsOSSv2(UCommonSession_SearchSessionRequest* InSearchRequest)
		: FCommonOnlineSearchSettingsBase(InSearchRequest)
	{
		FindLobbyParams.MaxResults = 10;

		FindLobbyParams.Filters.Emplace(FFindLobbySearchFilter{ SETTING_ONLINESUBSYSTEM_VERSION, ELobbyComparisonOp::Equals, true });

		if (InSearchRequest->bUseLobbies)
		{
			FindLobbyParams.Filters.Emplace(FFindLobbySearchFilter{ SEARCH_PRESENCE, ELobbyComparisonOp::Equals, true });
		}
	}
public:
	FFindLobbies::Params FindLobbyParams;
};

#endif // COMMONUSER_OSSV1

//////////////////////////////////////////////////////////////////////
// UCommonSession_HostSessionRequest

FString UCommonSession_HostSessionRequest::GetMapName() const
{
	FAssetData MapAssetData;
	if (UAssetManager::Get().GetPrimaryAssetData(MapID, /*out*/ MapAssetData))
	{
		return MapAssetData.PackageName.ToString();
	}
	else
	{
		return FString();
	}
}

FString UCommonSession_HostSessionRequest::ConstructTravelURL() const
{
	FString CombinedExtraArgs;

	if (OnlineMode == ECommonSessionOnlineMode::LAN)
	{
		CombinedExtraArgs += TEXT("?bIsLanMatch");
	}

	if (OnlineMode != ECommonSessionOnlineMode::Offline)
	{
		CombinedExtraArgs += TEXT("?listen");
	}

	for (const auto& KVP : ExtraArgs)
	{
		if (!KVP.Key.IsEmpty())
		{
			if (KVP.Value.IsEmpty())
			{
				CombinedExtraArgs += FString::Printf(TEXT("?%s"), *KVP.Key);
			}
			else
			{
				CombinedExtraArgs += FString::Printf(TEXT("?%s=%s"), *KVP.Key, *KVP.Value);
			}
		}
	}

	//bIsRecordingDemo ? TEXT("?DemoRec") : TEXT(""));

	return FString::Printf(TEXT("%s%s"),
		*GetMapName(),
		*CombinedExtraArgs);
}

bool UCommonSession_HostSessionRequest::ValidateAndLogErrors() const
{
	if (GetMapName().IsEmpty())
	{
		UE_LOG(LogCommonSession, Error, TEXT("Couldn't find asset data for MapID %s, hosting request failed"), *MapID.ToString());
		return false;
	}

	return true;
}

int32 UCommonSession_HostSessionRequest::GetMaxPlayers() const
{
	return MaxPlayerCount;
}

//////////////////////////////////////////////////////////////////////
// UCommonSessionSubsystem

#define SETTING_CUSTOMSESSION_EXPERIENCENAME FName(TEXT("EXPERIENCENAMESTRING"))
#define SETTING_ISCUSTOMSESSION FName(TEXT("ISCUSTOMSESSION"))

void UCommonSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BindOnlineDelegates();
	GEngine->OnTravelFailure().AddUObject(this, &UCommonSessionSubsystem::TravelLocalSessionFailure);

	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UCommonSessionSubsystem::HandlePostLoadMap);

	bReceivedServerReadyUpdate = false;
}

void UCommonSessionSubsystem::BindOnlineDelegates()
{
#if COMMONUSER_OSSV1
	BindOnlineDelegatesOSSv1();
#else
	BindOnlineDelegatesOSSv2();
#endif
}

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::BindOnlineDelegatesOSSv1()
{
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete));
	SessionInterface->AddOnStartSessionCompleteDelegate_Handle(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete));
	SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnUpdateSessionComplete));
	SessionInterface->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnEndSessionComplete));
	SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete));

	// #START @AccelByte Implementation
	SessionInterface->AddOnCancelMatchmakingCompleteDelegate_Handle(FOnCancelMatchmakingCompleteDelegate::CreateUObject(this, &ThisClass::OnCancelMatchmakingComplete));
	SessionInterface->AddOnSessionParticipantsChangeDelegate_Handle(FOnSessionParticipantsChangeDelegate::CreateUObject(this, &ThisClass::OnSessionParticipantsChange));
	SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnSessionJoined));
	SessionInterface->AddOnMatchmakingCanceledDelegate_Handle(FOnMatchmakingCanceledDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnMatchmakingCanceledNotification));
	SessionInterface->AddOnMatchmakingStartedDelegate_Handle(FOnMatchmakingStartedDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnMatchmakingStartedNotification));
	// #END

	SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete));
// 	SessionInterface->AddOnCancelFindSessionsCompleteDelegate_Handle(FOnCancelFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnCancelFindSessionsComplete));
// 	SessionInterface->AddOnPingSearchResultsCompleteDelegate_Handle(FOnPingSearchResultsCompleteDelegate::CreateUObject(this, &ThisClass::OnPingSearchResultsComplete));
//	SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete));
	SessionInterface->AddOnMatchmakingCompleteDelegate_Handle(FOnMatchmakingCompleteDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnMatchmakingComplete));

	if (GetGameInstance()->IsDedicatedServerInstance())
	{
		UE_LOG(LogCommonSession, Log, TEXT("Server - Binding to OnServerReceivedSession!"));

		// For server, hook into the moment that the DS gets session information, read the MAPNAME, then do a ServerTravel to that map
		const FOnServerReceivedSessionDelegate OnServerReceivedSessionDelegate = FOnServerReceivedSessionDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnServerReceivedSession);
		ServerReceivedSessionDelegateHandle = SessionInterface->AddOnServerReceivedSessionDelegate_Handle(OnServerReceivedSessionDelegate);
	}

	SessionInterface->AddOnSessionFailureDelegate_Handle(FOnSessionFailureDelegate::CreateUObject(this, &ThisClass::HandleSessionFailure));
}

#else

void UCommonSessionSubsystem::BindOnlineDelegatesOSSv2()
{
	// TODO: Bind OSSv2 delegates when they are available
	// Note that most OSSv1 delegates above are implemented as completion delegates in OSSv2 and don't need to be subscribed to
	TSharedPtr<IOnlineServices> OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
	check(Lobbies);
}
#endif

void UCommonSessionSubsystem::Deinitialize()
{
#if COMMONUSER_OSSV1
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());

	if (OnlineSub)
	{
		// During shutdown this may not be valid
		const IOnlineSessionPtr SessionInterface = OnlineSub->GetSessionInterface();
		if (SessionInterface)
		{
			SessionInterface->ClearOnSessionFailureDelegates(this);
		}
	}
#endif // COMMONUSER_OSSV1

	if (GEngine)
	{
		GEngine->OnTravelFailure().RemoveAll(this);
	}

	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Deinitialize();
}

bool UCommonSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);

	// Only create an instance if there is not a game-specific subclass
	return ChildClasses.Num() == 0;
}

UCommonSession_HostSessionRequest* UCommonSessionSubsystem::CreateOnlineHostSessionRequest()
{
	/** Game-specific subsystems can override this or you can modify after creation */

	UCommonSession_HostSessionRequest* NewRequest = NewObject<UCommonSession_HostSessionRequest>(this);
	NewRequest->OnlineMode = ECommonSessionOnlineMode::Online;
	NewRequest->bUseLobbies = true;

	return NewRequest;
}

UCommonSession_SearchSessionRequest* UCommonSessionSubsystem::CreateOnlineSearchSessionRequest()
{
	/** Game-specific subsystems can override this or you can modify after creation */

	UCommonSession_SearchSessionRequest* NewRequest = NewObject<UCommonSession_SearchSessionRequest>(this);
	NewRequest->OnlineMode = ECommonSessionOnlineMode::Online;
	NewRequest->bUseLobbies = true;
	NewRequest->ServerType = ECommonSessionOnlineServerType::P2P;

	return NewRequest;
}

void UCommonSessionSubsystem::HostSession(APlayerController* HostingPlayer, UCommonSession_HostSessionRequest* Request)
{
	if (Request == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("HostSession passed a null request"));
		OnCreateSessionComplete(NAME_None, false);
		return;
	}

	ULocalPlayer* LocalPlayer = (HostingPlayer != nullptr) ? HostingPlayer->GetLocalPlayer() : nullptr;
	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("HostingPlayer is invalid"));
		OnCreateSessionComplete(NAME_None, false);
		return;
	}

	if (!Request->ValidateAndLogErrors())
	{
		OnCreateSessionComplete(NAME_None, false);
		return;
	}

	if (Request->OnlineMode == ECommonSessionOnlineMode::Offline)
	{
		if (GetWorld()->GetNetMode() == NM_Client)
		{
			UE_LOG(LogCommonSession, Error, TEXT("Client trying to do an offline game mode, need to move to a Standalone world first"));
			OnCreateSessionComplete(NAME_None, false);
			return;
		}
		else
		{
			// Offline so travel to the specified match URL immediately
			GetWorld()->ServerTravel(Request->ConstructTravelURL());
		}
	}
	else
	{
		CreateOnlineSessionInternal(LocalPlayer, Request);
	}
}

void UCommonSessionSubsystem::CreateOnlineSessionInternal(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request)
{
	PendingTravelURL = Request->ConstructTravelURL();

#if COMMONUSER_OSSV1
	CreateOnlineSessionInternalOSSv1(LocalPlayer, Request);
#else
	CreateOnlineSessionInternalOSSv2(LocalPlayer, Request);
#endif
}

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::CreateOnlineSessionInternalOSSv1(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request)
{
	const FOnlineIdentityAccelBytePtr IdentityInterface = GetIdentityInterface();
	ensure(IdentityInterface.IsValid());

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	const FName SessionName(NAME_GameSession);
	const int32 MaxPlayers = Request->GetMaxPlayers();
	const bool bIsPresence = Request->bUseLobbies; // Using lobbies implies presence

	const FUniqueNetIdPtr LocalPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	if (!ensure(LocalPlayerId.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("LocalPlayerId is invalid"));
		OnCreateSessionComplete(SessionName, false);
		return;
	}

	HostSettings = MakeShareable(new FCommonSession_OnlineSessionSettings(Request->OnlineMode == ECommonSessionOnlineMode::LAN, bIsPresence, MaxPlayers));

	HostSettings->NumPublicConnections = MaxPlayers;
	HostSettings->NumPrivateConnections = 0;

	HostSettings->Set(SETTING_SESSION_TYPE, SETTING_SESSION_TYPE_GAME_SESSION);
	HostSettings->Set(SETTING_SESSION_JOIN_TYPE, TEXT("OPEN"));

	HostSettings->Set(SETTING_HOSTNAME, IdentityInterface->GetPlayerNickname(LocalPlayerId.ToSharedRef().Get()));
	HostSettings->Set(SETTING_SESSIONNAME, SessionName.ToString());

	HostSettings->bUseLobbiesIfAvailable = Request->bUseLobbies;
	HostSettings->Set(SETTING_GAMEMODE, Request->ModeNameForAdvertisement, EOnlineDataAdvertisementType::ViaOnlineService);
	HostSettings->Set(SETTING_MAPNAME, Request->GetMapName(), EOnlineDataAdvertisementType::ViaOnlineService);
	HostSettings->Set(SETTING_MATCHING_TIMEOUT, 120.0f, EOnlineDataAdvertisementType::ViaOnlineService);

	// Define session template depending on server type
	switch (Request->ServerType)
	{
	// Local
	case ECommonSessionOnlineServerType::NONE:
	{
		HostSettings->Set(SETTING_SESSION_TEMPLATE_NAME, TEXT("LyraGameSession"));

		const FString LocalAddress = GetLocalSessionAddress();
		HostSettings->Set(SETTING_LOCALADDRESS, LocalAddress);
		break;
	}
	// DS
	case ECommonSessionOnlineServerType::Dedicated:
	{
		HostSettings->bIsDedicated = true;
		HostSettings->Set(SETTING_SESSION_TEMPLATE_NAME, TEXT("LyraDSGameSession"));

		TArray<FString> Regions = SessionInterface->GetRegionList(LocalPlayerId.ToSharedRef().Get());
		if (ensure(Regions.IsValidIndex(0)))
		{
			HostSettings->Set(SETTING_GAMESESSION_REQUESTEDREGIONS, Regions[0]);
		}

		break;
	}
	// P2P
	case ECommonSessionOnlineServerType::P2P:
	{
		HostSettings->Set(SETTING_SESSION_TEMPLATE_NAME, TEXT("LyraP2PGameSession"));
		break;
	}
	}
	HostSettings->Set(SETTING_ONLINESUBSYSTEM_VERSION, true, EOnlineDataAdvertisementType::ViaOnlineService);
	HostSettings->bIsDedicated = Request->ServerType == ECommonSessionOnlineServerType::Dedicated;

	const FOnSessionServerUpdateDelegate OnSessionServerUpdateDelegate = FOnSessionServerUpdateDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnSessionServerUpdate);
	SessionServerUpdateDelegateHandle = SessionInterface->AddOnSessionServerUpdateDelegate_Handle(OnSessionServerUpdateDelegate);

	const FOnCreateSessionCompleteDelegate OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnCreateSessionComplete);
	CreateSessionDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);

	SessionInterface->CreateSession(LocalPlayerId.ToSharedRef().Get(), NAME_GameSession, *HostSettings);
}
#else

void UCommonSessionSubsystem::CreateOnlineSessionInternalOSSv2(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request)
{
	// Only lobbies are supported for now
	if (!ensureMsgf(Request->bUseLobbies, TEXT("Only Lobbies are supported in this release")))
	{
		Request->bUseLobbies = true;
	}

	const FName SessionName(NAME_GameSession);
	const int32 MaxPlayers = Request->GetMaxPlayers();
	const bool bIsPresence = Request->bUseLobbies; // Using lobbies implies presence

	IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
	check(Lobbies);
	FCreateLobby::Params CreateParams;
	CreateParams.LocalUserId = LocalPlayer->GetPreferredUniqueNetId().GetV2();
	CreateParams.LocalName = SessionName;
	CreateParams.SchemaName = FName(TEXT("GameLobby")); // TODO: make a parameter
	CreateParams.MaxMembers = MaxPlayers;
	CreateParams.JoinPolicy = ELobbyJoinPolicy::PublicAdvertised; // TODO: Check parameters

	CreateParams.Attributes.Emplace(SETTING_GAMEMODE, Request->ModeNameForAdvertisement);
	CreateParams.Attributes.Emplace(SETTING_MAPNAME, Request->GetMapName());
	//@TODO: CreateParams.Attributes.Emplace(SETTING_MATCHING_HOPPER, FString("TeamDeathmatch"));
	CreateParams.Attributes.Emplace(SETTING_MATCHING_TIMEOUT, 120.0f);
	CreateParams.Attributes.Emplace(SETTING_SESSION_TEMPLATE_NAME, FString(TEXT("GameSession")));
	CreateParams.Attributes.Emplace(SETTING_ONLINESUBSYSTEM_VERSION, true);
	if (bIsPresence)
	{
		// Add presence setting so it can be searched for
		CreateParams.Attributes.Emplace(SEARCH_PRESENCE, true);
	}

	FJoinLobbyLocalUserData& LocalUserData = CreateParams.LocalUsers.Emplace_GetRef();
	LocalUserData.LocalUserId = LocalPlayer->GetPreferredUniqueNetId().GetV2();
	LocalUserData.Attributes.Emplace(SETTING_GAMEMODE, FString(TEXT("GameSession")));
	// TODO: Add splitscreen players

	Lobbies->CreateLobby(MoveTemp(CreateParams)).OnComplete(this, [this, SessionName](const TOnlineResult<FCreateLobby>& CreateResult)
	{
		OnCreateSessionComplete(SessionName, CreateResult.IsOk());
	});
}

#endif

void UCommonSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnCreateSessionComplete(SessionName: %s, bWasSuccessful: %d)"), *SessionName.ToString(), bWasSuccessful);

	if (bWasSuccessful)
	{
		OnSessionCreatedDelegate.Broadcast();
	}

#if COMMONUSER_OSSV1 // OSSv2 joins splitscreen players as part of the create call

	// Ignore non-game session create results
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	if (!bWasSuccessful)
	{
		return;
	}

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	// Remove our delegate handler for create session, we will rebind if we create a new session
	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionDelegateHandle);
	CreateSessionDelegateHandle.Reset();

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
	if (!ensure(Session != nullptr))
	{
		return;
	}

	TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo);
	if (!ensure(SessionInfo.IsValid()))
	{
		return;
	}

	// Finally issue server travel to specified match URL set in UCommonSessionSubsystem::CreateOnlineSessionInternal
	FinishSessionCreation(true);
#else

	// We either failed or there is only a single local user
	FinishSessionCreation(bWasSuccessful);

#endif
}

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::OnRegisterLocalPlayerComplete_CreateSession(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result)
{
	FinishSessionCreation(Result == EOnJoinSessionCompleteResult::Success);
}

void UCommonSessionSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnStartSessionComplete(SessionName: %s, bWasSuccessful: %d)"), *SessionName.ToString(), bWasSuccessful);

	if (bWantToDestroyPendingSession)
	{
		CleanUpSessions();
	}
}
#endif // COMMONUSER_OSSV1

void UCommonSessionSubsystem::FinishSessionCreation(bool bWasSuccessful)
{
	if (bWasSuccessful && !bCreatingCustomSession && !GetGameInstance()->IsDedicatedServerInstance())
	{
		// Travel to the specified match URL
		GetWorld()->ServerTravel(PendingTravelURL);
	}

	if(bCreatingCustomSession)
	{
		bCreatingCustomSession = false;
	}
//@TODO: handle failure
// 	else
// 	{
// 		FText ReturnReason = NSLOCTEXT("NetworkErrors", "CreateSessionFailed", "Failed to create session.");
// 		FText OKButton = NSLOCTEXT("DialogButtons", "OKAY", "OK");
// 		ShowMessageThenGoMain(ReturnReason, OKButton, FText::GetEmpty());
// 	}
}

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnUpdateSessionComplete(SessionName: %s, bWasSuccessful: %s"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	UE_LOG(LogCommonSession, Log, TEXT("Session updated"));
	OnSessionChangedDelegate.Broadcast();

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	if (!ensure(SessionInterface.IsValid()))
	{
		UE_LOG(LogCommonSession, Warning, TEXT("OnUpdateSessionComplete: Session interface was invalid!"));
		return;
	}

	FOnlineSessionSettings* SessionSettings = SessionInterface->GetSessionSettings(NAME_GameSession);
	if (SessionSettings == nullptr)
	{
		UE_LOG(LogCommonSession, Warning, TEXT("OnUpdateSessionComplete: SessionSettings was nullptr!"));
		return;
	}

	bool ServerConnectSettingValue;
	if (!SessionSettings->Get(SETTING_SESSION_SERVER_CONNECT_READY, ServerConnectSettingValue))
	{
		bReceivedServerReadyUpdate = false;
		UE_LOG(LogCommonSession, Warning, TEXT("OnUpdateSessionComplete: Ignoring notif. Not reguarding connect ready flag for matchmaking."));
		return;
	}


	if (ServerConnectSettingValue && !bReceivedServerReadyUpdate)
	{
		bReceivedServerReadyUpdate = true;
		InternalTravelToSession(NAME_GameSession);
	}
	else if (!ServerConnectSettingValue)
	{
		bReceivedServerReadyUpdate = false;
	}
}

void UCommonSessionSubsystem::OnEndSessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnEndSessionComplete(SessionName: %s, bWasSuccessful: %s)"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	CleanUpSessions();
}

void UCommonSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnDestroySessionComplete(SessionName: %s, bWasSuccessful: %s)"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));
	bWantToDestroyPendingSession = false;
}


// #START @AccelByte Implementation Matchmaking Handler
void UCommonSessionSubsystem::OnMatchmakingStarted()
{
	if(!SearchSettings.IsValid())
	{
		UCommonSession_SearchSessionRequest* MatchRequest = CreateOnlineSearchSessionRequest();
		TWeakObjectPtr<APlayerController> JoinUser = MakeWeakObjectPtr(GetGameInstance()->GetFirstLocalPlayerController());
		UCommonSession_HostSessionRequest* HostRequest = CreateOnlineHostSessionRequest();
		TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequestPtr = TStrongObjectPtr<UCommonSession_HostSessionRequest>(HostRequest);
		MatchRequest->OnSearchFinished.AddUObject(this, &UCommonSessionSubsystem::HandleMatchmakingFinished, JoinUser, HostRequestPtr);

		SearchSettings = CreateMatchmakingSearchSettings(HostRequest, MatchRequest);
	}

	OnMatchmakingStartDelegate.Broadcast();
}

void UCommonSessionSubsystem::OnMatchmakingStartedNotification()
{	
	OnMatchmakingStarted();
}

void UCommonSessionSubsystem::OnMatchmakingCanceledNotification()
{
	OnMatchmakingCancelDelegate.Broadcast();
}

void UCommonSessionSubsystem::OnMatchmakingComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnMatchmakingComplete(SessionName: %s, bWasSuccessful: %s)"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	if(!SearchSettings.IsValid())
	{
		// If SearchSettings are invalid for some reason, as a fallback, check for matchmaking handle in session interface
		TSharedPtr<FOnlineSessionSearch> SessionSearch = SessionInterface->GetCurrentMatchmakingSearchHandle();
		if (SessionSearch.IsValid())
		{
			// If found, create a search setting template
			UCommonSession_SearchSessionRequest* MatchRequest = CreateOnlineSearchSessionRequest();
			TWeakObjectPtr<APlayerController> JoinUser = MakeWeakObjectPtr(GetGameInstance()->GetFirstLocalPlayerController());
			UCommonSession_HostSessionRequest* HostRequest = CreateOnlineHostSessionRequest();
			TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequestPtr = TStrongObjectPtr<UCommonSession_HostSessionRequest>(HostRequest);
			MatchRequest->OnSearchFinished.AddUObject(this, &UCommonSessionSubsystem::HandleMatchmakingFinished, JoinUser, HostRequestPtr);

			SearchSettings = CreateMatchmakingSearchSettings(HostRequest, MatchRequest);
		} 
		else 
		{
			// Otherwise matchmaking failed or was canceled
			return;
		}
	}

	// Get the search results from the OSS
	TSharedPtr<FOnlineSessionSearch> SessionSearch = SessionInterface->GetCurrentMatchmakingSearchHandle();
	if (SessionSearch.IsValid())
	{
		if (SessionSearch->SearchResults.Num() > 0)
		{
			SearchSettings->SearchResults = SessionSearch->SearchResults;
		}
		SearchSettings->SearchState = SessionSearch->SearchState;
	}

	FCommonOnlineSearchSettingsOSSv1& SearchSettingsV1 = *StaticCastSharedPtr<FCommonOnlineSearchSettingsOSSv1>(SearchSettings);
	if (SearchSettingsV1.SearchState == EOnlineAsyncTaskState::InProgress)
	{
		UE_LOG(LogCommonSession, Error, TEXT("OnMatchmakingComplete called when search is still in progress!"));
		return;
	}

	if (!ensure(SearchSettingsV1.SearchRequest))
	{
		UE_LOG(LogCommonSession, Error, TEXT("OnMatchmakingComplete called with invalid search request object!"));
		return;
	}

	if (bWasSuccessful)
	{
		SearchSettingsV1.SearchRequest->Results.Reset(SearchSettingsV1.SearchResults.Num());

		for (const FOnlineSessionSearchResult& Result : SearchSettingsV1.SearchResults)
		{
			UCommonSession_SearchResult* Entry = NewObject<UCommonSession_SearchResult>(SearchSettingsV1.SearchRequest);
			Entry->Result = Result;
			SearchSettingsV1.SearchRequest->Results.Add(Entry);

			// #SESSIONv2 We will want to broadcast match found here to match session v1 parity
			OnMatchFoundDelegate.Broadcast(Entry->Result.Session.GetSessionIdStr());
		}
	}
	else
	{
		SearchSettingsV1.SearchRequest->Results.Empty();
	}

	SearchSettingsV1.SearchRequest->NotifySearchFinished(bWasSuccessful, bWasSuccessful ? FText() : LOCTEXT("Error_Matchmaking Failed!", "Please look at log file"));
	SearchSettings.Reset();
}
void UCommonSessionSubsystem::OnCancelMatchmakingComplete(FName SessionName, bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnCancelMatchmakingComplete(SessionName: %s, bWasSuccessful: %s)"), *SessionName.ToString(), bWasSuccessful ? TEXT("true") : TEXT("false"));

	OnMatchmakingCancelDelegate.Broadcast();
	CleanUpSessions();
}

void UCommonSessionSubsystem::OnMatchmakingTimeout(const FErrorInfo& Error)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnMatchmakingTimeoutDelegate"));

	OnMatchmakingTimeoutDelegate.Broadcast(Error);
	CleanUpSessions();
}

void UCommonSessionSubsystem::OnMatchFound(FString MatchId)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnMatchFoundDelegate"));

	OnMatchFoundDelegate.Broadcast(MatchId);
}

// #END

#endif // COMMONUSER_OSSV1

#if AB_USE_V2_SESSIONS
void UCommonSessionSubsystem::QuerySessionMembersData(int32 LocalUserNum, const FNamedOnlineSession* Session, const FOnQueryUserInfoCompleteDelegate& OnComplete)
{
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(SessionInterface.IsValid(), "Session interface", LocalUserNum, false, {}, {});

	const IOnlineUserPtr UserInterface = GetUserInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(UserInterface.IsValid(), "User interface", LocalUserNum, false, {}, {});

	TArray<FUniqueNetIdRef> UsersToFetch;
	for(const auto& MemberId : Session->RegisteredPlayers)
	{
		if(UserInterface->GetUserInfo(LocalUserNum, MemberId.Get()) == nullptr)
		{
			UsersToFetch.Add(MemberId);
		}
	}

	if(UsersToFetch.Num() > 0)
	{
		OnQueryUserInfoCompleteHandle = UserInterface->AddOnQueryUserInfoCompleteDelegate_Handle(LocalUserNum, FOnQueryUserInfoCompleteDelegate::CreateWeakLambda(
			this, [this, OnComplete, UserInterface](int32 LocalUserNum, bool bWasSuccessful, const TArray<FUniqueNetIdRef>& UserIds, const FString& ErrorStr) {
				UserInterface->ClearOnQueryUserInfoCompleteDelegate_Handle(LocalUserNum, OnQueryUserInfoCompleteHandle);
				OnComplete.ExecuteIfBound(LocalUserNum, bWasSuccessful, UserIds, ErrorStr);
			}));
		UserInterface->QueryUserInfo(LocalUserNum, UsersToFetch);

		return;
	}

	OnComplete.ExecuteIfBound(LocalUserNum, true, {}, {});
}

void UCommonSessionSubsystem::GetSessionTeams(const APlayerController* QueryingPlayer, const FOnGetSessionTeamsCompleteDelegate& OnComplete)
{
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(SessionInterface.IsValid(), "Session interface", {});

	const IOnlineUserPtr UserInterface = GetUserInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(UserInterface.IsValid(), "User interface", {});

	const FOnlineIdentityAccelBytePtr IdentityInterface = GetIdentityInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(IdentityInterface.IsValid(), "Identity interface", {});

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(Session != nullptr, "Current session", {});

	const ULocalPlayer* LocalPlayer = QueryingPlayer->GetLocalPlayer();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(LocalPlayer != nullptr, "Local player", {});

	const int32 LocalUserNum = LocalPlayer->GetControllerId();
	const auto LocalUserId = LocalPlayer->GetPreferredUniqueNetId();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(LocalUserId.IsValid(), "Local user ID", {});

	const TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo);
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(SessionInfo.IsValid(), "Session info", {});

	const FUniqueNetIdPtr LeaderId = SessionInfo->GetLeaderId();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(LeaderId.IsValid(), "Session leader ID", {});

	QuerySessionMembersData(LocalUserNum, Session, FOnQueryUserInfoCompleteDelegate::CreateWeakLambda(this,
		[this, SessionInfo, UserInterface, LocalUserId, LeaderId, OnComplete](int32 LocalUserNum, bool, const TArray<FUniqueNetIdRef>&, const FString&) {
			TArray<FCommonSessionTeam> OutTeams;
			for(const FAccelByteModelsV2GameSessionTeam& Team : SessionInfo->GetTeamAssignments())
			{
				FCommonSessionTeam SessionTeam;

				for(const FString& UserId : Team.UserIDs)
				{
					FAccelByteUniqueIdComposite IdComponents;
					IdComponents.Id = UserId;
					TSharedPtr<const FUniqueNetIdAccelByteUser> AccelByteId = FUniqueNetIdAccelByteUser::Create(IdComponents);

					const TSharedPtr<FOnlineUser> UserInfo = UserInterface->GetUserInfo(LocalUserNum, AccelByteId.ToSharedRef().Get());
					if(!ensure(UserInfo.IsValid()))
					{
						UE_LOG(LogCommonSession, Warning, TEXT("User info for %s is invalid/nonexistent"), *UserId);
						continue;
					}

					const bool bIsLeader = LeaderId.ToSharedRef().Get() == AccelByteId.ToSharedRef().Get();
					const bool bIsLocalUser = LocalUserId == AccelByteId.ToSharedRef().Get();
					const auto Member = FCommonSessionMember(UserInfo->GetUserId(), UserInfo->GetDisplayName(), bIsLeader, bIsLocalUser);

					SessionTeam.Members.Add(Member);
				}

				OutTeams.Add(SessionTeam);
			}

			const uint8 RemainingTeams = 3 - OutTeams.Num();

			// Make sure that there is always 3 teams in the output
			for(uint8 i = 0; i < RemainingTeams; i++)
			{
				OutTeams.Add({});
			}

			OnComplete.ExecuteIfBound(OutTeams);
		}));
}

bool UCommonSessionSubsystem::IsLocalUserLeader(const APlayerController* QueryingPlayer)
{
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID(SessionInterface.IsValid(), "Session interface", false);

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	EARLY_RETURN_IF_INVALID(Session != nullptr, "Current session", false);

	const TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo);
	EARLY_RETURN_IF_INVALID(SessionInfo.IsValid(), "Session info", false);

	const FUniqueNetIdPtr LeaderId = SessionInfo->GetLeaderId();
	EARLY_RETURN_IF_INVALID(LeaderId.IsValid(), "Session leader ID", false);

	const auto LocalUserId = QueryingPlayer->GetLocalPlayer()->GetPreferredUniqueNetId();
	EARLY_RETURN_IF_INVALID(LocalUserId.IsValid(), "Local user ID", false);

	return LocalUserId == LeaderId;
}

// #START @AccelByte Implementation SessionV2
void UCommonSessionSubsystem::OnSessionParticipantsChange(FName SessionName, const FUniqueNetId&, bool bJoined)
{
	if(SessionName != NAME_GameSession)
	{
		return;
	}

	UE_LOG(LogCommonSession, Log, TEXT("Session participants changed"));

	if(bJoined)
	{
		OnSessionChangedDelegate.Broadcast();
		return;
	}

	// #NOTE: This is a quick and dirty solution to remove leaving members from the teams array until the backend does this (?)
	{
		const auto SessionInterface = GetSessionInterface();
		const auto Session = SessionInterface->GetNamedSession(SessionName);
		const auto SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo);
		const auto Teams = SessionInfo->GetTeamAssignments();

		TArray<FAccelByteModelsV2GameSessionTeam> NewTeams;
		for(const auto& Team : Teams)
		{
			TArray<FString> NewUserIds;
			for(const auto& UserIdString : Team.UserIDs)
			{
				FAccelByteUniqueIdComposite IdComponents;
				IdComponents.Id = UserIdString;
				TSharedPtr<const FUniqueNetIdAccelByteUser> AccelByteId = FUniqueNetIdAccelByteUser::Create(IdComponents);

				const FUniqueNetIdRef* FoundId = SessionInfo->GetJoinedMembers().FindByPredicate([AccelByteId](FUniqueNetIdRef Id){
					return AccelByteId.ToSharedRef().Get() == Id.Get();
				});

				if(FoundId != nullptr)
				{
					NewUserIds.Add(UserIdString);
				}
			}
			NewTeams.Add({NewUserIds});
		}

		// Not updating the backend because the it already performed the above to its copy of the teams array
		SessionInfo->SetTeamAssignments(NewTeams);
	}

	OnSessionChangedDelegate.Broadcast();
}

void UCommonSessionSubsystem::OnSessionJoined(FName SessionName, EOnJoinSessionCompleteResult::Type)
{
	if(SessionName != NAME_GameSession)
	{
		return;
	}

	UE_LOG(LogCommonSession, Log, TEXT("Session joined"));
	OnSessionJoinedDelegate.Broadcast();
}

void UCommonSessionSubsystem::OnStartMatchmakingComplete(FName SessionName, const FOnlineError& ErrorDetails, const FSessionMatchmakingResults& Results)
{
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	if (!ErrorDetails.bSucceeded)
	{
		CurrentMatchmakingSearchHandle.Reset();
	}

	// Instead of calling OnMatchmakingStarted here, we will wait for the matchmaking started notification from lobby service
}

void UCommonSessionSubsystem::OnDestroySessionForJoinComplete(FName SessionName, bool bWasSuccessful, UCommonSession_SearchResult* Request, APlayerController* JoiningPlayer)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	JoinSession(JoiningPlayer, Request);
}

void UCommonSessionSubsystem::OnSessionServerUpdate(FName SessionName)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnSessionServerUpdate called"));

	// Ignore non-game session join results
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID(SessionInterface.IsValid(), "Session interface", );

	SessionInterface->GetResolvedConnectString(NAME_GameSession, PendingTravelURL);

	UE_LOG(LogCommonSession, Error, TEXT("Travel URL: '%s'"), *PendingTravelURL);

	// Remove our delegate handler for update session server, we will rebind if needed later
	SessionInterface->ClearOnSessionServerUpdateDelegate_Handle(SessionServerUpdateDelegateHandle);
	SessionServerUpdateDelegateHandle.Reset();

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
	EARLY_RETURN_IF_INVALID(Session != nullptr, "Current session", );

	APlayerController* const PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (PlayerController == nullptr)
	{
		FText ReturnReason = NSLOCTEXT("NetworkErrors", "InvalidPlayerController", "Invalid Player Controller");
		UE_LOG(LogCommonSession, Error, TEXT("InternalTravelToSession(Failed due to %s)"), *ReturnReason.ToString());
		return;
	}

	FString TravelUrl{};
	if (!SessionInterface->GetResolvedConnectString(SessionName, TravelUrl))
	{
		FText FailReason = NSLOCTEXT("NetworkErrors", "TravelSessionFailed", "OnSessionServerUpdate - Travel to Session failed. Unable to get resolved connect string.");
		UE_LOG(LogCommonSession, Error, TEXT("InternalTravelToSession(%s)"), *FailReason.ToString());
		return;
	}

	PlayerController->ClientTravel(TravelUrl, TRAVEL_Absolute);
	UE_LOG(LogCommonSession, Log, TEXT("Client traveling to: %s"), *TravelUrl);
}

void UCommonSessionSubsystem::OnServerReceivedSession(FName SessionName)
{
	UE_LOG(LogCommonSession, Log, TEXT("Server - OnServerReceivedSession called"));

	// Ignore non-game session join results
	if (SessionName != NAME_GameSession)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Server - Named session was not of type GameSession, skipping!"));
		return;
	}

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	// Remove our delegate handler we will rebind if needed later
	SessionInterface->ClearOnServerReceivedSessionDelegate_Handle(ServerReceivedSessionDelegateHandle);
	ServerReceivedSessionDelegateHandle.Reset();

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
	if (!ensure(Session != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("Server - Named session was null! Unable to travel to map"));
		return;
	}

	// Read the MAPNAME from session settings, then do a ServerTravel to that map
	FString MapName = TEXT("");
	Session->SessionSettings.Get(SETTING_MAPNAME, MapName);

	if (!MapName.IsEmpty())
	{
		UE_LOG(LogCommonSession, Log, TEXT("Server - Traveling to %s"), *MapName);

		if (GetWorld()->IsInSeamlessTravel())
		{
			UE_LOG(LogCommonSession, Log, TEXT("Server - Currently in seemless travel! Waiting until travel is done until attempting to server travel to target map."));
			// #TODO if this is the case, then bind to some travel complete delegate then call server travel?
		}

		GetWorld()->ServerTravel(MapName, true);
	}
	else
	{
		UE_LOG(LogCommonSession, Error, TEXT("Server - Map name empty! Unable to travel to map"));
	}
}

// #END
#endif // AB_USE_V2_SESSIONS

void UCommonSessionSubsystem::FindSessions(APlayerController* SearchingPlayer, UCommonSession_SearchSessionRequest* Request)
{
	if (Request == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("FindSessions passed a null request"));
		return;
	}

#if COMMONUSER_OSSV1
	FindSessionsInternal(SearchingPlayer, MakeShared<FCommonOnlineSearchSettingsOSSv1>(Request));
#else
	FindSessionsInternal(SearchingPlayer, MakeShared<FCommonOnlineSearchSettingsOSSv2>(Request));
#endif // COMMONUSER_OSSV1
}

void UCommonSessionSubsystem::FindSessionsInternal(APlayerController* SearchingPlayer, const TSharedRef<FCommonOnlineSearchSettings>& InSearchSettings)
{
	if (SearchSettings.IsValid())
	{
		//@TODO: This is a poor user experience for the API user, we should let the additional search piggyback and
		// just give it the same results as the currently pending one
		// (or enqueue the request and service it when the previous one finishes or fails)
		UE_LOG(LogCommonSession, Error, TEXT("A previous FindSessions call is still in progress, aborting"));
		SearchSettings->SearchRequest->NotifySearchFinished(false, LOCTEXT("Error_FindSessionAlreadyInProgress", "Session search already in progress"));
	}

	ULocalPlayer* LocalPlayer = (SearchingPlayer != nullptr) ? SearchingPlayer->GetLocalPlayer() : nullptr;
	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("SearchingPlayer is invalid"));
		InSearchSettings->SearchRequest->NotifySearchFinished(false, LOCTEXT("Error_FindSessionBadPlayer", "Session search was not provided a local player"));
		return;
	}

	SearchSettings = InSearchSettings;

#if COMMONUSER_OSSV1
	FindSessionsInternalOSSv1(LocalPlayer);
#else
	FindSessionsInternalOSSv2(LocalPlayer);
#endif
}

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::FindSessionsInternalOSSv1(ULocalPlayer* LocalPlayer)
{
	// #START @AccelByte Implementation
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("SearchingPlayer is invalid"));
		return;
	}

	const FUniqueNetIdPtr LocalPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	if (!ensure(LocalPlayerId.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("LocalPlayerId is invalid"));
		return;
	}

	const FOnFindSessionsCompleteDelegate OnFindSessionsCompleteDelegate = FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnFindSessionsComplete);
	QuerySessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(OnFindSessionsCompleteDelegate);

	if (!SessionInterface->FindSessions(LocalPlayerId.ToSharedRef().Get(), SearchSettings.ToSharedRef()))
	{
		// Some session search failures will call this delegate inside the function, others will not
		OnFindSessionsComplete(false);
	}
	//#END
}

#else

void UCommonSessionSubsystem::FindSessionsInternalOSSv2(ULocalPlayer* LocalPlayer)
{
	IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
	check(Lobbies);

	FFindLobbies::Params FindLobbyParams = StaticCastSharedPtr<FCommonOnlineSearchSettingsOSSv2>(SearchSettings)->FindLobbyParams;
	FindLobbyParams.LocalUserId = LocalPlayer->GetPreferredUniqueNetId().GetV2();

	Lobbies->FindLobbies(MoveTemp(FindLobbyParams)).OnComplete(this, [this, LocalSearchSettings = SearchSettings](const TOnlineResult<FFindLobbies>& FindResult)
	{
		if (LocalSearchSettings != SearchSettings)
		{
			// This was an abandoned search, ignore
			return;
		}
		const bool bWasSuccessful = FindResult.IsOk();
		UE_LOG(LogCommonSession, Log, TEXT("FindLobbies(bWasSuccessful: %s)"), *LexToString(bWasSuccessful));
		check(SearchSettings.IsValid());
		if (bWasSuccessful)
		{
			const FFindLobbies::Result& FindResults = FindResult.GetOkValue();
			SearchSettings->SearchRequest->Results.Reset(FindResults.Lobbies.Num());

			for (const TSharedRef<const FLobby>& Lobby : FindResults.Lobbies)
			{
				if (!Lobby->OwnerAccountId.IsValid())
				{
					UE_LOG(LogCommonSession, Verbose, TEXT("\tIgnoring Lobby with no owner (LobbyId: %s)"),
						*ToLogString(Lobby->LobbyId));
				}
				else if (Lobby->Members.Num() == 0)
				{
					UE_LOG(LogCommonSession, Verbose, TEXT("\tIgnoring Lobby with no members (UserId: %s)"),
						*ToLogString(Lobby->OwnerAccountId));
				}
				else
				{
					UCommonSession_SearchResult* Entry = NewObject<UCommonSession_SearchResult>(SearchSettings->SearchRequest);
					Entry->Lobby = Lobby;
					SearchSettings->SearchRequest->Results.Add(Entry);

					UE_LOG(LogCommonSession, Log, TEXT("\tFound lobby (UserId: %s, NumOpenConns: %d)"),
						*ToLogString(Lobby->OwnerAccountId), Lobby->MaxMembers - Lobby->Members.Num());
				}
			}
		}
		else
		{
			SearchSettings->SearchRequest->Results.Empty();
		}

		const FText ResultText = bWasSuccessful ? FText() : FindResult.GetErrorValue().GetText();

		SearchSettings->SearchRequest->NotifySearchFinished(bWasSuccessful, ResultText);
		SearchSettings.Reset();
	});
}
#endif // COMMONUSER_OSSV1

void UCommonSessionSubsystem::QuickPlaySession(APlayerController* JoiningOrHostingPlayer, UCommonSession_HostSessionRequest* HostRequest)
{
	UE_LOG(LogCommonSession, Log, TEXT("QuickPlay Requested"));

	if (HostRequest == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("QuickPlaySession passed a null request"));
		return;
	}

	TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequestPtr = TStrongObjectPtr<UCommonSession_HostSessionRequest>(HostRequest);
	TWeakObjectPtr<APlayerController> JoiningOrHostingPlayerPtr = TWeakObjectPtr<APlayerController>(JoiningOrHostingPlayer);

	UCommonSession_SearchSessionRequest* QuickPlayRequest = CreateOnlineSearchSessionRequest();
	QuickPlayRequest->OnSearchFinished.AddUObject(this, &UCommonSessionSubsystem::HandleQuickPlaySearchFinished, JoiningOrHostingPlayerPtr, HostRequestPtr);

	FindSessionsInternal(JoiningOrHostingPlayer, CreateQuickPlaySearchSettings(HostRequest, QuickPlayRequest));
}

void UCommonSessionSubsystem::StartSession()
{
	IOnlineSessionPtr Session = Online::GetSessionInterface();
	check(Session)
	FName GameSession = NAME_GameSession;
	EOnlineSessionState::Type SessionState = Session->GetSessionState(GameSession);
	if(SessionState == EOnlineSessionState::Pending)
	{
		UE_LOG(LogCommonSession, Log, TEXT("UCommonSessionSubsystem::StartSession: Start session %s"), *GameSession.ToString());
		Session->StartSession(NAME_GameSession);
		return;
	}
	UE_LOG(LogCommonSession, Warning, TEXT("UCommonSessionSubsystem::StartSession: Failed to start session, session state is not Pending. Current Session State: %s"), EOnlineSessionState::ToString(SessionState));
}

/** #START @AccelByte Custom session implementation */
void UCommonSessionSubsystem::HostCustomSession(APlayerController* HostingPlayer, const FOnHostCustomSessionComplete& OnHostCustomSessionComplete)
{
	const ULocalPlayer* LocalPlayer = (HostingPlayer != nullptr) ? HostingPlayer->GetLocalPlayer() : nullptr;
	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("HostingPlayer is invalid"));
		return;
	}

	const FUniqueNetIdPtr LocalPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	if (!ensure(LocalPlayerId.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("LocalPlayerId is invalid"));
		return;
	}

	RestoreAndLeaveActiveGameSessions(HostingPlayer, TDelegate<void(bool)>::CreateLambda([this, HostingPlayer, OnHostCustomSessionComplete](bool bWasSuccessful) {
		if (bWasSuccessful)
		{
			CreateCustomGameSessionInternal(HostingPlayer, OnHostCustomSessionComplete);
		}
	}));
}

void UCommonSessionSubsystem::StartCustomSession(APlayerController* RequestingPlayer, const FOnHostCustomSessionComplete& OnComplete)
{
	const FOnlineSessionV2AccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(SessionInterface.IsValid(), "Session interface", false);

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(Session != nullptr, "Current session", false);

	const EAccelByteV2SessionConfigurationServerType ServerType =
		StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo)->GetServerType();

	FDelegateHandle UpdateHandler = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateWeakLambda(this,
        [SessionInterface, &UpdateHandler, OnComplete](FName SessionName, bool bWasSuccessful) {
            if(SessionName != NAME_GameSession)
            {
            	return;
            }

            SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandler);
            OnComplete.ExecuteIfBound(bWasSuccessful);
        }));

	if(ServerType == EAccelByteV2SessionConfigurationServerType::DS)
	{
		FOnlineSessionSettings SessionSettings = Session->SessionSettings;

		// #NOTE: Triggering a DS request by setting min players to the number of members in the session
		SessionSettings.Set(SETTING_SESSION_MINIMUM_PLAYERS, Session->RegisteredPlayers.Num());

		// #NOTE: This is only here because the backend wipes out the deployment setting if one is not provided on the PATCH endpoint
		SessionSettings.Set(SETTING_GAMESESSION_DEPLOYMENT, TEXT("default"));

		// TODO: Hardcoding this here is incorrect
		FOnlineSessionSettingsAccelByte::Set(SessionSettings, SETTING_GAMESESSION_REQUESTEDREGIONS, {TEXT("us-east-1")});

		SessionInterface->UpdateSession(NAME_GameSession, SessionSettings);
	}
	else if(ServerType == EAccelByteV2SessionConfigurationServerType::P2P)
	{
		FString MapName;
		EARLY_RETURN_IF_INVALID_WITH_DELEGATE(Session->SessionSettings.Get(SETTING_MAPNAME, MapName) && !MapName.IsEmpty(), "Map name", false);
		RequestingPlayer->ClientTravel(FString::Printf(TEXT("%s?listen"), *MapName), TRAVEL_Absolute);
	}
}

bool UCommonSessionSubsystem::IsCustomSession(APlayerController* RequestingPlayer)
{
	const FOnlineSessionV2AccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID(SessionInterface.IsValid(), "Session interface", false);

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	EARLY_RETURN_IF_INVALID(Session != nullptr, "Current session", false);

	bool bResult = false;
	Session->SessionSettings.Get(SETTING_ISCUSTOMSESSION, bResult);

	return bResult;
}

void UCommonSessionSubsystem::CreateCustomGameSessionInternal(const APlayerController* HostingPlayer, FOnHostCustomSessionComplete OnComplete)
{
	const FOnlineSessionV2AccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(SessionInterface.IsValid(), "Session interface", false);

	const FOnlineIdentityAccelBytePtr IdentityInterface = GetIdentityInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(IdentityInterface.IsValid(), "Identity interface", false);

	const ULocalPlayer* LocalPlayer = HostingPlayer->GetLocalPlayer();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(LocalPlayer != nullptr, "Local player", false);

	const FUniqueNetIdPtr HostingPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(HostingPlayerId.IsValid(), "Hosting player ID", false);

	FOnlineSessionSettings NewSessionSettings;
	NewSessionSettings.Set(SETTING_SESSION_TYPE, SETTING_SESSION_TYPE_GAME_SESSION);
	NewSessionSettings.Set(SETTING_SESSION_JOIN_TYPE, TEXT("OPEN"));
	NewSessionSettings.Set(SETTING_SESSION_TEMPLATE_NAME, TEXT("LyraCustomGameSession"));
	NewSessionSettings.Set(SETTING_GAMESESSION_DEPLOYMENT, TEXT("default"));
	NewSessionSettings.Set(SETTING_ISCUSTOMSESSION, true);

	// #NOTE Defaulting to DS server type
	NewSessionSettings.Set(SETTING_SESSION_SERVER_TYPE, TEXT("DS"));
	// #NOTE Defaulting to bots enabled
	NewSessionSettings.Set(SETTING_BOTSENABLED, true);
	// #NOTE HOSTNAME is the username of the hosting player
	NewSessionSettings.Set(SETTING_HOSTNAME, IdentityInterface->GetPlayerNickname(HostingPlayerId.ToSharedRef().Get()));
	
	const TSharedRef<const FUniqueNetIdAccelByteUser> ABUser = FUniqueNetIdAccelByteUser::Cast(HostingPlayerId.ToSharedRef().Get());
	const FString AccelByteId = ABUser->GetAccelByteId();

	OnCreateCustomGameSessionHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateCustomSessionComplete, AccelByteId, OnComplete));

	bCreatingCustomSession = true;
	const int32 HostingPlayerNum = LocalPlayer->GetControllerId();
	SessionInterface->CreateSession(HostingPlayerNum, NAME_GameSession, NewSessionSettings);
}

void UCommonSessionSubsystem::OnCreateCustomSessionComplete(FName SessionName, bool bWasSuccessful, FString AccelByteId, FOnHostCustomSessionComplete OnComplete)
{
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	const FOnlineSessionV2AccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(SessionInterface.IsValid(), "Session interface", false);

	const auto OnTeamUpdateComplete = FOnCustomSessionUpdateComplete::CreateLambda([OnComplete](bool bWasSuccessful) {
		OnComplete.ExecuteIfBound(bWasSuccessful);
	});
	SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(OnCreateCustomGameSessionHandle);

	if(bWasSuccessful)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Custom game session created"));
		AddPlayerToCustomGameSessionTeam(AccelByteId, OnTeamUpdateComplete);

		return;
	}

	OnComplete.ExecuteIfBound(false);
}

void UCommonSessionSubsystem::AddPlayerToCustomGameSessionTeam(const APlayerController* Player, const FOnAddPlayerToTeamComplete& OnComplete)
{
	const TSharedRef<const FUniqueNetIdAccelByteUser> ABUser = FUniqueNetIdAccelByteUser::Cast(
		Player->GetLocalPlayer()->GetPreferredUniqueNetId().GetUniqueNetId().ToSharedRef().Get());
	const FString AccelByteId = ABUser->GetAccelByteId();
	AddPlayerToCustomGameSessionTeam(AccelByteId,
		FOnCustomSessionUpdateComplete::CreateWeakLambda(this, [OnComplete](bool bWasSuccessful) {
			OnComplete.ExecuteIfBound(bWasSuccessful);
		}));
}

void UCommonSessionSubsystem::AddPlayerToCustomGameSessionTeam(const FString& PlayerAccelByteId, const FOnCustomSessionUpdateComplete& OnComplete)
{
	const FOnlineSessionV2AccelBytePtr SessionInterface = GetSessionInterface();
	if (!ensure(SessionInterface.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("SessionInterface is invalid"));
		return;
	}

	FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ensure(CurrentSession != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("CurrentSession is invalid"));
		return;
	}

	const TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(CurrentSession->SessionInfo);
	if (!ensure(SessionInfo.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("SessionInfo is invalid"));
		return;
	}

	TArray<FAccelByteModelsV2GameSessionTeam> Teams = SessionInfo->GetTeamAssignments();
	if(Teams.Num() == 0)
	{
		Teams.Add({});
	}

	/*const TSharedRef<const FUniqueNetIdAccelByteUser> ABUser = FUniqueNetIdAccelByteUser::Cast(PlayerId.ToSharedRef().Get());
	const FString AccelByteId = ABUser->GetAccelByteId();*/

	if(GetCurrentTeamIndex(Teams, PlayerAccelByteId) != -1)
	{
		OnComplete.ExecuteIfBound(true);
		return;
	}

	Teams[0].UserIDs.Add(PlayerAccelByteId);

	SessionInfo->SetTeamAssignments(Teams);

	OnUpdateTeamsCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateWeakLambda(this,
		[this, SessionInterface, OnComplete](FName SessionName, bool bWasSuccessful) {
			if (SessionName != NAME_GameSession)
			{
				return;
			}

			OnComplete.ExecuteIfBound(bWasSuccessful);
			SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(OnUpdateTeamsCompleteHandle); 
		}));
	SessionInterface->UpdateSession(CurrentSession->SessionName, CurrentSession->SessionSettings);
}

void UCommonSessionSubsystem::OnUpdateCustomGameSessionComplete(FName SessionName, bool bWasSuccessful, FOnSetCustomGameSettingComplete OnSettingUpdateComplete)
{
	if (SessionName != NAME_GameSession || !bWasSuccessful)
	{
		return;
	}

	FOnlineSessionV2AccelBytePtr SessionInterface = nullptr;
	if (!FOnlineSessionV2AccelByte::GetFromWorld(GetWorld(), SessionInterface))
	{
		return;
	}

	OnSettingUpdateComplete.ExecuteIfBound(bWasSuccessful);
}

int32 UCommonSessionSubsystem::GetCurrentTeamIndex(const TArray<FAccelByteModelsV2GameSessionTeam>& Teams, const FString& PlayerAccelByteId)
{
	int32 i = 0;
	for(const auto& Team : Teams)
	{
		for(const auto& MemberId : Team.UserIDs)
		{
			if(MemberId.Equals(PlayerAccelByteId))
			{
				return i;
			}
		}
		i++;
	}

	return -1;
}

/** #END */

/** #START @AccelByte Implementation : Starts a process to matchmaking with other player. */
void UCommonSessionSubsystem::MatchmakingSession(APlayerController* JoiningOrHostingPlayer, UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest*& OutMatchmakingSessionRequest)
{
	UE_LOG(LogCommonSession, Log, TEXT("Matchmaking Requested"));
#if AB_USE_V2_SESSIONS
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	// Create a new search handle to pass to start matchmaking
	TSharedRef<FOnlineSessionSearch> NewSearchHandle = MakeShared<FOnlineSessionSearch>();

	NewSearchHandle->QuerySettings.Set(SETTING_SESSION_MATCHPOOL, HostRequest->ModeNameForAdvertisement, EOnlineComparisonOp::Equals);
	NewSearchHandle->QuerySettings.Set(SETTING_MAPNAME, HostRequest->GetMapName(), EOnlineComparisonOp::Equals);

	ULocalPlayer* LocalPlayer = (JoiningOrHostingPlayer != nullptr) ? JoiningOrHostingPlayer->GetLocalPlayer() : nullptr;
	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("JoiningOrHostingPlayer is invalid"));
		return;
	}

	const FUniqueNetIdPtr LocalPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	if (!ensure(LocalPlayerId.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("LocalPlayerId is invalid"));
		return;
	}

	const FOnStartMatchmakingComplete OnStartMatchmakingCompleteDelegate = FOnStartMatchmakingComplete::CreateUObject(this, &UCommonSessionSubsystem::OnStartMatchmakingComplete);
	if (SessionInterface->StartMatchmaking(USER_ID_TO_MATCHMAKING_USER_ARRAY(LocalPlayerId.ToSharedRef()), NAME_GameSession, FOnlineSessionSettings(), NewSearchHandle, OnStartMatchmakingCompleteDelegate))
	{
		// Set search handle to our new instance here, as the start matchmaking call will modify it
		CurrentMatchmakingSearchHandle = NewSearchHandle;
	}
#else 
	if (SearchSettings.IsValid())
	{
		UE_LOG(LogCommonSession, Log, TEXT("Matchmaking Search Session already in progress. Aborting this request!"));
		return;
	}

	if (HostRequest == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("Matchmaking passed a null request"));
		return;
	}

	TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequestPtr = TStrongObjectPtr<UCommonSession_HostSessionRequest>(HostRequest);
	TWeakObjectPtr<APlayerController> JoiningOrHostingPlayerPtr = TWeakObjectPtr<APlayerController>(JoiningOrHostingPlayer);

	OutMatchmakingSessionRequest = CreateOnlineSearchSessionRequest();
	OutMatchmakingSessionRequest->OnSearchFinished.AddUObject(this, &UCommonSessionSubsystem::HandleMatchmakingFinished, JoiningOrHostingPlayerPtr, HostRequestPtr);

	FindSessionsInternal(JoiningOrHostingPlayer, CreateMatchmakingSearchSettings(HostRequest, OutMatchmakingSessionRequest));
#endif
}

void UCommonSessionSubsystem::CancelMatchmakingSession(APlayerController* CancelPlayer)
{
#if AB_USE_V2_SESSIONS
	if (!CurrentMatchmakingSearchHandle.IsValid())
	{
		UE_LOG(LogCommonSession, Error, TEXT("CurrentMatchmakingSearchHandle is invalid"));
		return;
	}

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	ULocalPlayer* LocalPlayer = (CancelPlayer != nullptr) ? CancelPlayer->GetLocalPlayer() : nullptr;
	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("CancelPlayer is invalid"));
		return;
	}

	const FUniqueNetIdPtr LocalPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	if (!ensure(LocalPlayerId.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("LocalPlayerId is invalid"));
		return;
	}

	CurrentMatchmakingSearchHandle.Reset();
	SessionInterface->CancelMatchmaking(LocalPlayerId.ToSharedRef().Get(), NAME_GameSession);
#else 
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);
	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	check(Sessions);

	SearchSettings.Reset();

	int32 LocalPlayerIndex = CancelPlayer->GetLocalPlayer()->GetLocalPlayerIndex();

	Sessions->CancelMatchmaking(LocalPlayerIndex, NAME_GameSession);
#endif
}

// #END

void UCommonSessionSubsystem::InviteToCustomSession(const APlayerController* SendingPlayer, const FUniqueNetIdRepl ReceivingPlayerId)
{
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	ensure(Session != nullptr);

	SessionInterface->SendSessionInviteToFriend(SendingPlayer->GetLocalPlayer()->GetControllerId(),
		NAME_GameSession, ReceivingPlayerId.GetUniqueNetId().ToSharedRef().Get());
}

TSharedRef<FCommonOnlineSearchSettings> UCommonSessionSubsystem::CreateQuickPlaySearchSettings(UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest* SearchRequest)
{
#if COMMONUSER_OSSV1
	return CreateQuickPlaySearchSettingsOSSv1(HostRequest, SearchRequest);
#else
	return CreateQuickPlaySearchSettingsOSSv2(HostRequest, SearchRequest);
#endif // COMMONUSER_OSSV1
}

#if COMMONUSER_OSSV1
TSharedRef<FCommonOnlineSearchSettings> UCommonSessionSubsystem::CreateQuickPlaySearchSettingsOSSv1(UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest* SearchRequest)
{
	TSharedRef<FCommonOnlineSearchSettingsOSSv1> QuickPlaySearch = MakeShared<FCommonOnlineSearchSettingsOSSv1>(SearchRequest);

	/** By default quick play does not want to include the map or game mode, games can fill this in as desired
	if (!HostRequest->ModeNameForAdvertisement.IsEmpty())
	{
		QuickPlaySearch->QuerySettings.Set(SETTING_GAMEMODE, HostRequest->ModeNameForAdvertisement, EOnlineComparisonOp::Equals);
	}

	if (!HostRequest->GetMapName().IsEmpty())
	{
		QuickPlaySearch->QuerySettings.Set(SETTING_MAPNAME, HostRequest->GetMapName(), EOnlineComparisonOp::Equals);
	} 
	*/

	// QuickPlaySearch->QuerySettings.Set(SEARCH_DEDICATED_ONLY, true, EOnlineComparisonOp::Equals);
	return QuickPlaySearch;
}

TSharedRef<FCommonOnlineSearchSettings> UCommonSessionSubsystem::CreateMatchmakingSearchSettings(
	UCommonSession_HostSessionRequest* Request, UCommonSession_SearchSessionRequest* SearchRequest)
{
	TSharedRef<FCommonOnlineSearchSettingsOSSv1> MatchmakingSearch = MakeShared<FCommonOnlineSearchSettingsOSSv1>(SearchRequest);

	MatchmakingSearch->QuerySettings.Set(SETTING_GAMEMODE, Request->MatchPool, EOnlineComparisonOp::Equals);
	MatchmakingSearch->QuerySettings.Set(SEARCH_MATCHMAKING_QUEUE, Request->MatchPool, EOnlineComparisonOp::Equals);
	MatchmakingSearch->QuerySettings.Set(SEARCH_DEDICATED_ONLY, true, EOnlineComparisonOp::Equals);
	MatchmakingSearch->QuerySettings.Set(SETTING_MAPNAME, Request->GetMapName(), EOnlineComparisonOp::Equals);
	FString* NumBots = Request->ExtraArgs.Find(TEXT("NumBots"));
	if(NumBots != nullptr)
	{
		MatchmakingSearch->QuerySettings.Set(SETTING_NUMBOTS, FCString::Atoi(**NumBots), EOnlineComparisonOp::Equals);
	}
	return MatchmakingSearch;
}

#else

TSharedRef<FCommonOnlineSearchSettings> UCommonSessionSubsystem::CreateQuickPlaySearchSettingsOSSv2(UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest* SearchRequest)
{
	TSharedRef<FCommonOnlineSearchSettingsOSSv2> QuickPlaySearch = MakeShared<FCommonOnlineSearchSettingsOSSv2>(SearchRequest);

	/** By default quick play does not want to include the map or game mode, games can fill this in as desired
	if (!HostRequest->ModeNameForAdvertisement.IsEmpty())
	{
		QuickPlaySearch->FindLobbyParams.Filters.Emplace(FFindLobbySearchFilter{SETTING_GAMEMODE, ELobbyComparisonOp::Equals, HostRequest->ModeNameForAdvertisement});
	}
	if (!HostRequest->GetMapName().IsEmpty())
	{
		QuickPlaySearch->FindLobbyParams.Filters.Emplace(FFindLobbySearchFilter{SETTING_MAPNAME, ELobbyComparisonOp::Equals, HostRequest->GetMapName()});
	}
	*/

	return QuickPlaySearch;
}

#endif // COMMONUSER_OSSV1

void UCommonSessionSubsystem::HandleQuickPlaySearchFinished(bool bSucceeded, const FText& ErrorMessage, TWeakObjectPtr<APlayerController> JoiningOrHostingPlayer, TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequest)
{
	const int32 ResultCount = SearchSettings->SearchRequest->Results.Num();
	UE_LOG(LogCommonSession, Log, TEXT("QuickPlay Search Finished %s (Results %d) (Error: %s)"), bSucceeded ? TEXT("Success") : TEXT("Failed"), ResultCount, *ErrorMessage.ToString());

	//@TODO: We have to check if the error message is empty because some OSS layers report a failure just because there are no sessions.  Please fix with OSS 2.0.
	if (bSucceeded || ErrorMessage.IsEmpty())
	{
		// Join the best search result.
		if (ResultCount > 0)
		{
			//@TODO: We should probably look at ping?  maybe some other factors to find the best.  Idk if they come pre-sorted or not.
			for (UCommonSession_SearchResult* Result : SearchSettings->SearchRequest->Results)
			{
				JoinSession(JoiningOrHostingPlayer.Get(), Result);
				return;
			}
		}
		else
		{
			HostSession(JoiningOrHostingPlayer.Get(), HostRequest.Get());
		}
	}
	else
	{
		//@TODO: This sucks, need to tell someone.
	}
}

void UCommonSessionSubsystem::HandleMatchmakingFinished(bool bSucceeded, const FText& ErrorMessage,
	TWeakObjectPtr<APlayerController> JoiningOrHostingPlayer,
	TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequest)
{
	const int32 ResultCount = SearchSettings->SearchRequest->Results.Num();
	UE_LOG(LogCommonSession, Log, TEXT("Matchmaking Search Finished %s (Results %d) (Error: %s)"), bSucceeded ? TEXT("Success") : TEXT("Failed"), ResultCount, *ErrorMessage.ToString());

	if (bSucceeded || ErrorMessage.IsEmpty())
	{
		// Matchmaking found suitable DS.
		if (ResultCount > 0)
		{
			for (UCommonSession_SearchResult* Result : SearchSettings->SearchRequest->Results)
			{
				// #NOTE Here we will need to wait until the server has finished obtaining game session details before attempting to join the session?
				JoinSession(JoiningOrHostingPlayer.Get(), Result);
				return;
			}
		}
	}

	// Fail, cleanup session
	CleanUpSessions();
}

FOnlineSessionAccelBytePtr UCommonSessionSubsystem::GetSessionInterface() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (!ensure(Subsystem != nullptr))
	{
		return nullptr;
	}

	return StaticCastSharedPtr<FOnlineSessionV2AccelByte>(Subsystem->GetSessionInterface());
}

IOnlineUserPtr UCommonSessionSubsystem::GetUserInterface() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (!ensure(Subsystem != nullptr))
	{
		return nullptr;
	}

	return Subsystem->GetUserInterface();
}

FOnlineIdentityAccelBytePtr UCommonSessionSubsystem::GetIdentityInterface() const
{
	const IOnlineSubsystem* Subsystem = Online::GetSubsystem(GetWorld());
	if (!ensure(Subsystem != nullptr))
	{
		return nullptr;
	}

	return StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());;
}

FString UCommonSessionSubsystem::GetLocalSessionAddress() const
{
	// Grab local IP and port to associate with this session. Start with local IP from socket subsystem.
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (SocketSubsystem == nullptr)
	{
		return TEXT("");
	}

	bool bCanBindAll;
	TSharedPtr<FInternetAddr> LocalIP = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);
	if (!LocalIP.IsValid())
	{
		return TEXT("");
	}

	const FString IpString = LocalIP->ToString(false);

	// Then grab the currently bound port from the world
	UWorld* World = GetWorld();
	if (!ensure(World != nullptr))
	{
		return TEXT("");
	}

	const int32 Port = World->URL.Port;
	return FString::Printf(TEXT("%s:%d"), *IpString, Port);
}

void UCommonSessionSubsystem::RestoreAndLeaveActiveGameSessions(APlayerController* Player, const TDelegate<void(bool)>& OnRestoreAndLeaveAllComplete)
{
	FOnlineSessionV2AccelBytePtr SessionInterface;
	if (!FOnlineSessionV2AccelByte::GetFromWorld(GetWorld(), SessionInterface))
	{
		UE_LOG(LogCommonSession, Error, TEXT("Failed get session interface from world"));
		OnRestoreAndLeaveAllComplete.ExecuteIfBound(false);
		return;
	}

	const ULocalPlayer* LocalPlayer = (Player != nullptr) ? Player->GetLocalPlayer() : nullptr;
	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("HostingPlayer is invalid"));
		return;
	}

	const FUniqueNetIdPtr LocalPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	if (!ensure(LocalPlayerId.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("LocalPlayerId is invalid"));
		return;
	}

	const FOnRestoreActiveSessionsComplete OnRestoreActiveSessionsCompleteDelegate = FOnRestoreActiveSessionsComplete::CreateUObject(this, &UCommonSessionSubsystem::OnRestoreAllSessionsComplete, OnRestoreAndLeaveAllComplete);
	SessionInterface->RestoreActiveSessions(LocalPlayerId.ToSharedRef().Get(), OnRestoreActiveSessionsCompleteDelegate);
}

void UCommonSessionSubsystem::OnRestoreAllSessionsComplete(const FUniqueNetId& LocalUserId, const FOnlineError& Result, TDelegate<void(bool)> OnRestoreAndLeaveAllComplete)
{
	if (!Result.bSucceeded)
	{
		UE_LOG(LogCommonSession, Error, TEXT("Failed restore sessions for player %s"), *LocalUserId.ToDebugString());
		OnRestoreAndLeaveAllComplete.ExecuteIfBound(false);
		return;
	}

	FOnlineSessionV2AccelBytePtr SessionInterface;
	if (!FOnlineSessionV2AccelByte::GetFromWorld(GetWorld(), SessionInterface))
	{
		UE_LOG(LogCommonSession, Error, TEXT("Failed get session interface from world"));
		OnRestoreAndLeaveAllComplete.ExecuteIfBound(false);
		return;
	}

	TArray<FOnlineRestoredSessionAccelByte> RestoredGameSessions = SessionInterface->GetAllRestoredGameSessions();

	NumberOfRestoredSessionsToLeave = RestoredGameSessions.Num();
	if (NumberOfRestoredSessionsToLeave <= 0)
	{
		OnRestoreAndLeaveAllComplete.ExecuteIfBound(true);
		return;
	}

	for (const FOnlineRestoredSessionAccelByte& RestoredSession : RestoredGameSessions)
	{
		const FOnLeaveSessionComplete OnLeaveRestoredSessionCompleteDelegate = FOnLeaveSessionComplete::CreateUObject(this, &UCommonSessionSubsystem::OnLeaveRestoredGameSessionComplete, OnRestoreAndLeaveAllComplete);
		SessionInterface->LeaveRestoredSession(LocalUserId, RestoredSession, OnLeaveRestoredSessionCompleteDelegate);
	}
}

void UCommonSessionSubsystem::OnLeaveRestoredGameSessionComplete(bool bWasSuccessful, FString SessionId, TDelegate<void(bool)> OnRestoreAndLeaveAllComplete)
{
	if (!bWasSuccessful)
	{
		UE_LOG(LogCommonSession, Error, TEXT("Failed to leave restored session with ID '%s'"), *SessionId);
		OnRestoreAndLeaveAllComplete.ExecuteIfBound(false);
		return;
	}

	if (--NumberOfRestoredSessionsToLeave <= 0)
	{
		OnRestoreAndLeaveAllComplete.ExecuteIfBound(true);
	}
}

void UCommonSessionSubsystem::CleanUpSessions()
{
	bWantToDestroyPendingSession = true;
	HostSettings.Reset();
#if COMMONUSER_OSSV1
	CleanUpSessionsOSSv1();
#else
	CleanUpSessionsOSSv2();
#endif // COMMONUSER_OSSV1
}

void UCommonSessionSubsystem::LeaveCurrentGameSession(APlayerController* LeavingPlayer, const FOnLeaveCurrentGameSession& OnComplete)
{
	const FOnlineSessionV2AccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(SessionInterface.IsValid(), "Session interface", false);

	const FOnDestroySessionCompleteDelegate OnDestroySessionCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateWeakLambda(this, [OnComplete](FName, bool bWasSuccessful) {
		OnComplete.ExecuteIfBound(bWasSuccessful);
	});
	SessionInterface->DestroySession(NAME_GameSession, OnDestroySessionCompleteDelegate);
}

void UCommonSessionSubsystem::SetCustomGameSessionNetworkMode(APlayerController* UpdatingPlayer, const ECommonSessionOnlineServerType& NetworkMode, const FOnSetCustomGameSettingComplete& OnSettingUpdateComplete)
{
	FOnlineSessionV2AccelBytePtr SessionInterface = nullptr;
	if (!FOnlineSessionV2AccelByte::GetFromWorld(GetWorld(), SessionInterface))
	{
		return;
	}

	FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ensure(CurrentSession != nullptr))
	{
		return;
	}

	switch (NetworkMode)
	{
	case ECommonSessionOnlineServerType::Dedicated:
		CurrentSession->SessionSettings.Set(SETTING_SESSION_SERVER_TYPE, TEXT("DS"));
		break;
	case ECommonSessionOnlineServerType::P2P:
		CurrentSession->SessionSettings.Set(SETTING_SESSION_SERVER_TYPE, TEXT("P2P"));
		break;
	case ECommonSessionOnlineServerType::NONE:
		CurrentSession->SessionSettings.Set(SETTING_SESSION_SERVER_TYPE, TEXT("NONE"));
		break;
	}

	OnUpdateNetworkModeCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateWeakLambda(this, [this, SessionInterface, OnSettingUpdateComplete](FName SessionName, bool bWasSuccessful) {
		if (SessionName != NAME_GameSession || !bWasSuccessful)
		{
			return;
		}

		OnSettingUpdateComplete.ExecuteIfBound(bWasSuccessful);
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(OnUpdateNetworkModeCompleteHandle);
	}));
	SessionInterface->UpdateSession(CurrentSession->SessionName, CurrentSession->SessionSettings);
}

void UCommonSessionSubsystem::SetCustomGameSessionBotsEnabled(APlayerController* UpdatingPlayer, bool bBotsEnabled, const FOnSetCustomGameSettingComplete& OnSettingUpdateComplete)
{
	FOnlineSessionV2AccelBytePtr SessionInterface = nullptr;
	if (!FOnlineSessionV2AccelByte::GetFromWorld(GetWorld(), SessionInterface))
	{
		return;
	}

	FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ensure(CurrentSession != nullptr))
	{
		return;
	}

	CurrentSession->SessionSettings.Set(SETTING_BOTSENABLED, bBotsEnabled);

	OnUpdateBotsEnabledCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateWeakLambda(this, [this, SessionInterface, OnSettingUpdateComplete](FName SessionName, bool bWasSuccessful) {
		if (SessionName != NAME_GameSession || !bWasSuccessful)
		{
			return;
		}

		OnSettingUpdateComplete.ExecuteIfBound(bWasSuccessful);
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(OnUpdateBotsEnabledCompleteHandle);
	}));
	SessionInterface->UpdateSession(CurrentSession->SessionName, CurrentSession->SessionSettings);
}

void UCommonSessionSubsystem::SetCustomGameSessionMap(const FPrimaryAssetId& MapId, const FName& ExperienceAssetName, const FString& ExperienceName, const FOnSetCustomGameSettingComplete& OnComplete)
{
	const FOnlineSessionV2AccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(SessionInterface.IsValid(), "Session interface", false);

	FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession);
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(CurrentSession != nullptr, "Current session", false);

	FAssetData MapAssetData;
	EARLY_RETURN_IF_INVALID_WITH_DELEGATE(UAssetManager::Get().GetPrimaryAssetData(MapId, MapAssetData), "Map asset data", false);

	CurrentSession->SessionSettings.Set(SETTING_MAPNAME, MapAssetData.PackageName.ToString());
	CurrentSession->SessionSettings.Set(SETTING_SESSION_MATCHPOOL, ExperienceAssetName.ToString());

	// TODO: This should maybe be the name of the map rather than the name of the experience?
	CurrentSession->SessionSettings.Set(SETTING_CUSTOMSESSION_EXPERIENCENAME, ExperienceName);
	
	OnUpdateMapCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateWeakLambda(this, [this, SessionInterface, OnComplete](FName SessionName, bool bWasSuccessful) {
		if (SessionName != NAME_GameSession)
		{
			return;
		}

		OnComplete.ExecuteIfBound(bWasSuccessful);
		SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(OnUpdateMapCompleteHandle);
	}));
	SessionInterface->UpdateSession(CurrentSession->SessionName, CurrentSession->SessionSettings);
}

FString UCommonSessionSubsystem::GetCustomGameSessionNetworkModeString(APlayerController* QueryingPlayer)
{
	const FOnlineSessionV2AccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID(SessionInterface.IsValid(), "Session interface", {});

	const FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession);
	EARLY_RETURN_IF_INVALID(CurrentSession != nullptr, "Current session", {});

	const TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo
		= StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(CurrentSession->SessionInfo);
	EARLY_RETURN_IF_INVALID(SessionInfo.IsValid(), "Session info", {});

	return SessionInterface->GetServerTypeAsString(SessionInfo->GetServerType());
}

FString UCommonSessionSubsystem::GetCustomGameSessionBotsEnabledString(APlayerController* QueryingPlayer)
{
	FOnlineSessionV2AccelBytePtr SessionInterface = nullptr;
	if (!FOnlineSessionV2AccelByte::GetFromWorld(GetWorld(), SessionInterface))
	{
		UE_LOG(LogCommonSession, Error, TEXT("SessionInterface is invalid"));
		return {};
	}

	const FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ensure(CurrentSession != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("CurrentSession is invalid"));
		return {};
	}

	bool Result;
	if(CurrentSession->SessionSettings.Get(SETTING_BOTSENABLED, Result))
	{
		return Result ? TEXT("Enabled") : TEXT("Disabled");
	}

	return {};
}

FString UCommonSessionSubsystem::GetCustomGameSessionMapNameString(APlayerController* QueryingPlayer)
{
	FOnlineSessionV2AccelBytePtr SessionInterface = nullptr;
	if (!FOnlineSessionV2AccelByte::GetFromWorld(GetWorld(), SessionInterface))
	{
		UE_LOG(LogCommonSession, Error, TEXT("SessionInterface is invalid"));
		return {};
	}

	const FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ensure(CurrentSession != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("CurrentSession is invalid"));
		return {};
	}

	FString Result;
	if(CurrentSession->SessionSettings.Get(SETTING_CUSTOMSESSION_EXPERIENCENAME, Result))
	{
		return Result;
	}

	return {};
}

void UCommonSessionSubsystem::ChangeCustomSessionTeam(APlayerController* UpdatingPlayer, const ECommonSessionTeamChangeDirection& Direction, const FOnSetCustomGameSettingComplete& OnTeamChangeComplete)
{
	FOnlineSessionV2AccelBytePtr SessionInterface = nullptr;
	if (!FOnlineSessionV2AccelByte::GetFromWorld(GetWorld(), SessionInterface))
	{
		UE_LOG(LogCommonSession, Error, TEXT("SessionInterface is invalid"));
		return;
	}

	FNamedOnlineSession* CurrentSession = SessionInterface->GetNamedSession(NAME_GameSession);
	if (!ensure(CurrentSession != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("CurrentSession is invalid"));
		return;
	}

	const TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(CurrentSession->SessionInfo);
	if (!ensure(SessionInfo.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("SessionInfo is invalid"));
		return;
	}

	const ULocalPlayer* LocalPlayer = (UpdatingPlayer != nullptr) ? UpdatingPlayer->GetLocalPlayer() : nullptr;
	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("UpdatingPlayer is invalid"));
		return;
	}

	const FUniqueNetIdPtr LocalPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	if (!ensure(LocalPlayerId.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("LocalPlayerId is invalid"));
		return;
	}

	const TSharedRef<const FUniqueNetIdAccelByteUser> ABUser = FUniqueNetIdAccelByteUser::Cast(*LocalPlayerId);
	const FString AccelByteId = ABUser->GetAccelByteId();

	TArray<FAccelByteModelsV2GameSessionTeam> Teams = SessionInfo->GetTeamAssignments();
	const int32 OldTeamIndex = GetCurrentTeamIndex(Teams, AccelByteId);
	int32 NewTeamIndex = OldTeamIndex;

	if(!ensure(OldTeamIndex != -1))
	{
		UE_LOG(LogCommonSession, Error, TEXT("TeamIndex is invalid"));
		return;
	}

	if(Direction == ECommonSessionTeamChangeDirection::NEXT)
	{
		NewTeamIndex++;
	}
	else
	{
		NewTeamIndex--;
	}

	NewTeamIndex = FMath::Clamp(NewTeamIndex, 0, 2);

	for(int32 i = Teams.Num(); i <= NewTeamIndex; i++)
	{
		Teams.Add({});
	}

	if(NewTeamIndex == OldTeamIndex)
	{
		OnTeamChangeComplete.ExecuteIfBound(true);
		return;
	}

	Teams[OldTeamIndex].UserIDs.Remove(AccelByteId);
	Teams[NewTeamIndex].UserIDs.Add(AccelByteId);

	SessionInfo->SetTeamAssignments(Teams);

	OnUpdateTeamsCompleteHandle = SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateWeakLambda(this,
		[this, SessionInterface, OnTeamChangeComplete](FName SessionName, bool bWasSuccessful) {
			if (SessionName != NAME_GameSession)
			{
				return;
			}

			OnTeamChangeComplete.ExecuteIfBound(bWasSuccessful);
			SessionInterface->ClearOnUpdateSessionCompleteDelegate_Handle(OnUpdateTeamsCompleteHandle);
		}));
	SessionInterface->UpdateSession(CurrentSession->SessionName, CurrentSession->SessionSettings);
}

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::CleanUpSessionsOSSv1()
{
	IOnlineSubsystem* OnlineSub = Online::GetSubsystem(GetWorld());
	check(OnlineSub);
	IOnlineSessionPtr Sessions = OnlineSub->GetSessionInterface();
	check(Sessions);

	EOnlineSessionState::Type SessionState = Sessions->GetSessionState(NAME_GameSession);
	UE_LOG(LogCommonSession, Log, TEXT("Session state is %s"), EOnlineSessionState::ToString(SessionState));

	if (EOnlineSessionState::InProgress == SessionState)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Ending session because of return to front end"));
		Sessions->EndSession(NAME_GameSession);
	}
	else if (EOnlineSessionState::Ending == SessionState)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Waiting for session to end on return to main menu"));
	}
	else if (EOnlineSessionState::Ended == SessionState || EOnlineSessionState::Pending == SessionState)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Destroying session on return to main menu"));
		Sessions->DestroySession(NAME_GameSession);
	}
	else if (EOnlineSessionState::Starting == SessionState || EOnlineSessionState::Creating == SessionState)
	{
		UE_LOG(LogCommonSession, Log, TEXT("Waiting for session to start, and then we will end it to return to main menu"));
	}
	else
	{
		// reset if fail to cleanup session
		bWantToDestroyPendingSession = false;
	}
}

#else
void UCommonSessionSubsystem::CleanUpSessionsOSSv2()
{
	IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
	check(Lobbies);

	FOnlineAccountIdHandle LocalPlayerId = GetAccountId(GetGameInstance()->GetFirstLocalPlayerController());
	FOnlineLobbyIdHandle LobbyId = GetLobbyId(NAME_GameSession);

	if (!LocalPlayerId.IsValid() || !LobbyId.IsValid())
	{
		return;
	}
	// TODO:  Include all local players leave the lobby
	Lobbies->LeaveLobby({LocalPlayerId, LobbyId});
}

#endif // COMMONUSER_OSSV1

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	UE_LOG(LogCommonSession, Log, TEXT("OnFindSessionsComplete(bWasSuccessful: %s)"), bWasSuccessful ? TEXT("true") : TEXT("false"));

	if (!SearchSettings.IsValid())
	{
		// This could get called twice for failed session searches, or for a search requested by a different system
		return;
	}

	FCommonOnlineSearchSettingsOSSv1& SearchSettingsV1 = *StaticCastSharedPtr<FCommonOnlineSearchSettingsOSSv1>(SearchSettings);
	if (SearchSettingsV1.SearchState == EOnlineAsyncTaskState::InProgress)
	{
		UE_LOG(LogCommonSession, Error, TEXT("OnFindSessionsComplete called when search is still in progress!"));
		return;
	}

	if (!ensure(SearchSettingsV1.SearchRequest))
	{
		UE_LOG(LogCommonSession, Error, TEXT("OnFindSessionsComplete called with invalid search request object!"));
		return;
	}

	if (bWasSuccessful)
	{
		SearchSettingsV1.SearchRequest->Results.Reset(SearchSettingsV1.SearchResults.Num());

		for (FOnlineSessionSearchResult& Result : SearchSettingsV1.SearchResults)
		{
			UCommonSession_SearchResult* Entry = NewObject<UCommonSession_SearchResult>(SearchSettingsV1.SearchRequest);

			// Set owning username as the host name found in the session settings
			Result.Session.SessionSettings.Get(SETTING_HOSTNAME, Result.Session.OwningUserName);

			Entry->Result = Result;
			SearchSettingsV1.SearchRequest->Results.Add(Entry);
			FString OwningUserId = TEXT("Unknown");
			if (Result.Session.OwningUserId.IsValid())
			{
				OwningUserId = Result.Session.OwningUserId->ToString();
			}

			UE_LOG(LogCommonSession, Log, TEXT("\tFound session (UserId: %s, UserName: %s, NumOpenPrivConns: %d, NumOpenPubConns: %d, Ping: %d ms"),
				*OwningUserId,
				*Result.Session.OwningUserName,
				Result.Session.NumOpenPrivateConnections,
				Result.Session.NumOpenPublicConnections,
				Result.PingInMs
				);
		}
	}
	else
	{
		SearchSettingsV1.SearchRequest->Results.Empty();
	}

	if (0)
	{
		// Fake Sessions OSSV1
		for (int i = 0; i < 10; i++)
		{
			UCommonSession_SearchResult* Entry = NewObject<UCommonSession_SearchResult>(SearchSettings->SearchRequest);
			FOnlineSessionSearchResult FakeResult;
			FakeResult.Session.OwningUserName = TEXT("Fake User");
			FakeResult.Session.SessionSettings.NumPublicConnections = 10;
			FakeResult.Session.SessionSettings.bShouldAdvertise = true;
			FakeResult.Session.SessionSettings.bAllowJoinInProgress = true;
			FakeResult.PingInMs=99;
			Entry->Result = FakeResult;
			SearchSettingsV1.SearchRequest->Results.Add(Entry);
		}
	}

	SearchSettingsV1.SearchRequest->NotifySearchFinished(bWasSuccessful, bWasSuccessful ? FText() : LOCTEXT("Error_FindSessionV1Failed", "Find session failed"));
	SearchSettings.Reset();

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(QuerySessionsCompleteDelegateHandle);
}
#endif // COMMONUSER_OSSV1

void UCommonSessionSubsystem::JoinSession(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Request)
{
	if (Request == nullptr)
	{
		UE_LOG(LogCommonSession, Error, TEXT("JoinSession passed a null request"));
		return;
	}

	JoinSessionInternal(JoiningPlayer, Request);
}

void UCommonSessionSubsystem::JoinSessionInternal(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Request)
{
	ULocalPlayer* LocalPlayer = (JoiningPlayer != nullptr) ? JoiningPlayer->GetLocalPlayer() : nullptr;
	if (!ensure(LocalPlayer != nullptr))
	{
		UE_LOG(LogCommonSession, Error, TEXT("JoiningPlayer is invalid"));
		return;
	}

#if COMMONUSER_OSSV1
	JoinSessionInternalOSSv1(JoiningPlayer, LocalPlayer, Request);
#else
	JoinSessionInternalOSSv2(LocalPlayer, Request);
#endif // COMMONUSER_OSSV1
}

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::JoinSessionInternalOSSv1(APlayerController* JoiningPlayer, ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request)
{
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	EAccelByteV2SessionType SessionType = SessionInterface->GetSessionTypeFromSettings(Request->Result.Session.SessionSettings);
	if (SessionType != EAccelByteV2SessionType::GameSession)
	{
		return;
	}

	// Check if we already have a game session that we are in, if so, destroy it to join this one
	if (SessionInterface->GetNamedSession(NAME_GameSession) != nullptr)
	{
		const FOnDestroySessionCompleteDelegate OnDestroySessionForJoinCompleteDelegate = FOnDestroySessionCompleteDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnDestroySessionForJoinComplete, Request, JoiningPlayer);
		SessionInterface->DestroySession(NAME_GameSession, OnDestroySessionForJoinCompleteDelegate);
	}

	// Register a delegate for joining the specified session
	const FOnJoinSessionCompleteDelegate OnJoinSessionCompleteDelegate = FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnJoinSessionComplete);
	JoinSessionDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(OnJoinSessionCompleteDelegate);

	const FUniqueNetIdPtr LocalPlayerId = LocalPlayer->GetPreferredUniqueNetId().GetUniqueNetId();
	if (!ensure(LocalPlayerId.IsValid()))
	{
		UE_LOG(LogCommonSession, Error, TEXT("LocalPlayerId is invalid"));
		return;
	}

	SessionInterface->JoinSession(LocalPlayerId.ToSharedRef().Get(), NAME_GameSession, Request->Result);
}

void UCommonSessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	// Ignore non-game session join results
	if (SessionName != NAME_GameSession)
	{
		return;
	}

	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		return;
	}

	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID(SessionInterface.IsValid(), "Session interface", );

	// Remove our delegate handler for join session, we will rebind if we join another session
	SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionDelegateHandle);
	JoinSessionDelegateHandle.Reset();

	FinishJoinSession(Result);
}

void UCommonSessionSubsystem::OnRegisterJoiningLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result)
{
	FinishJoinSession(Result);
}

void UCommonSessionSubsystem::FinishJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	if (Result == EOnJoinSessionCompleteResult::Success)
	{
		// Before traveling to the session, we will want to check if the server has flagged this session as joinable
		const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
		if (!ensure(SessionInterface.IsValid()))
		{
			UE_LOG(LogCommonSession, Warning, TEXT("FinishJoinSession: Unable to join game session as SessionInterface was invalid! "));
			return;
		}

		FOnlineSessionSettings* SessionSettings = SessionInterface->GetSessionSettings(NAME_GameSession);
		if (SessionSettings == nullptr)
		{
			UE_LOG(LogCommonSession, Warning, TEXT("FinishJoinSession: Unable to join game session as SessionSettings was nullptr!"));
			return;
		}

		bool ServerConnectSettingValue;
		if (!SessionSettings->Get(SETTING_SESSION_SERVER_CONNECT_READY, ServerConnectSettingValue))
		{
			// If the connect ready flag was not found, then listen to Session Update Notification to wait until server is ready
			UE_LOG(LogCommonSession, Warning, TEXT("FinishJoinSession: connect ready flag was not found in session settings. Listening to Session Update Notification!"));
			SessionInterface->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UCommonSessionSubsystem::OnUpdateSessionComplete));
			return;
		}

		if (ServerConnectSettingValue == true)
		{
			InternalTravelToSession(NAME_GameSession);
		}		
	}
	else
	{
		FText ReturnReason;
		switch (Result)
		{
		case EOnJoinSessionCompleteResult::SessionIsFull:
			ReturnReason = NSLOCTEXT("NetworkErrors", "SessionIsFull", "Game is full.");
			break;
		case EOnJoinSessionCompleteResult::SessionDoesNotExist:
			ReturnReason = NSLOCTEXT("NetworkErrors", "SessionDoesNotExist", "Game no longer exists.");
			break;
		default:
			ReturnReason = NSLOCTEXT("NetworkErrors", "JoinSessionFailed", "Join failed.");
			break;
		}

		//@TODO: Error handling
		UE_LOG(LogCommonSession, Error, TEXT("FinishJoinSession(Failed with Result: %s)"), *ReturnReason.ToString());
	}
}

#else

void UCommonSessionSubsystem::JoinSessionInternalOSSv2(ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request)
{
	const FName SessionName(NAME_GameSession);
	IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
	check(OnlineServices);
	ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
	check(Lobbies);

	FJoinLobby::Params JoinParams;
	JoinParams.LocalUserId = LocalPlayer->GetPreferredUniqueNetId().GetV2();
	JoinParams.LocalName = SessionName;
	JoinParams.LobbyId = Request->Lobby->LobbyId;
	FJoinLobbyLocalUserData& LocalUserData = JoinParams.LocalUsers.Emplace_GetRef();
	LocalUserData.LocalUserId = LocalPlayer->GetPreferredUniqueNetId().GetV2();

	// Add any splitscreen players if they exist //@TODO: See UCommonSessionSubsystem::OnJoinSessionComplete

	Lobbies->JoinLobby(MoveTemp(JoinParams)).OnComplete(this, [this, SessionName](const TOnlineResult<FJoinLobby>& JoinResult)
	{
		if (JoinResult.IsOk())
		{
			InternalTravelToSession(SessionName);
		}
		else
		{
			//@TODO: Error handling
			UE_LOG(LogCommonSession, Error, TEXT("JoinLobby Failed with Result: %s"), *ToLogString(JoinResult.GetErrorValue()));
		}
	});
}

UE::Online::FOnlineAccountIdHandle UCommonSessionSubsystem::GetAccountId(APlayerController* PlayerController) const
{
	if (const ULocalPlayer* const LocalPlayer = PlayerController->GetLocalPlayer())
	{
		FUniqueNetIdRepl LocalPlayerIdRepl = LocalPlayer->GetPreferredUniqueNetId();
		if (LocalPlayerIdRepl.IsValid())
		{
			return LocalPlayerIdRepl.GetV2();
		}
	}
	return FOnlineAccountIdHandle();
}

UE::Online::FOnlineLobbyIdHandle UCommonSessionSubsystem::GetLobbyId(const FName SessionName) const
{
	FOnlineAccountIdHandle LocalUserId = GetAccountId(GetGameInstance()->GetFirstLocalPlayerController());
	if (LocalUserId.IsValid())
	{
		IOnlineServicesPtr OnlineServices = GetServices(GetWorld());
		check(OnlineServices);
		ILobbiesPtr Lobbies = OnlineServices->GetLobbiesInterface();
		check(Lobbies);
		TOnlineResult<FGetJoinedLobbies> JoinedLobbies = Lobbies->GetJoinedLobbies({ LocalUserId });
		if (JoinedLobbies.IsOk())
		{
			for (const TSharedRef<const FLobby>& Lobby : JoinedLobbies.GetOkValue().Lobbies)
			{
				if (Lobby->LocalName == SessionName)
				{
					return Lobby->LobbyId;
				}
			}
		}
	}
	return FOnlineLobbyIdHandle();
}

#endif // COMMONUSER_OSSV1

void UCommonSessionSubsystem::InternalTravelToSession(const FName SessionName)
{
	//@TODO: Ideally we'd use triggering player instead of first (they're all gonna go at once so it probably doesn't matter)
	APlayerController* const PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (PlayerController == nullptr)
	{
		FText ReturnReason = NSLOCTEXT("NetworkErrors", "InvalidPlayerController", "Invalid Player Controller");
		UE_LOG(LogCommonSession, Error, TEXT("InternalTravelToSession(Failed due to %s)"), *ReturnReason.ToString());
		return;
	}

	FString URL;
#if COMMONUSER_OSSV1
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	ensure(SessionInterface.IsValid());

	FNamedOnlineSession* Session = SessionInterface->GetNamedSession(SessionName);
	if (!ensure(Session != nullptr))
	{
		return;
	}

	TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo);
	if (!ensure(SessionInfo.IsValid()))
	{
		return;
	}

	// If the server type for the created session is either NONE (local), then the connection info is in the session attributes
	const EAccelByteV2SessionConfigurationServerType ServerType = SessionInfo->GetServerType();
	if (ServerType == EAccelByteV2SessionConfigurationServerType::NONE)
	{
		if (!Session->SessionSettings.Get(SETTING_LOCALADDRESS, URL) || URL.IsEmpty())
		{
			UE_LOG(LogCommonSession, Error, TEXT("InternalTravelToSession failed as server type was of type NONE (local) and the local address in session settings was empty!"));
			return;
		}
	}
	else if(ServerType == EAccelByteV2SessionConfigurationServerType::P2P)
	{
		// Don't try to travel if the user is the P2P session leader
		if(PlayerController->GetLocalPlayer()->GetPreferredUniqueNetId().GetUniqueNetId().ToSharedRef().Get() == SessionInfo->GetLeaderId().ToSharedRef().Get())
		{
			return;
		}
	}

	// Otherwise, get the connection info from the SessionData received from the backend
	if (!SessionInterface->GetResolvedConnectString(SessionName, URL))
	{
		FText FailReason = NSLOCTEXT("NetworkErrors", "TravelSessionFailed", "Travel to Session failed. Waiting for server update notification instead.");
		UE_LOG(LogCommonSession, Error, TEXT("InternalTravelToSession(%s)"), *FailReason.ToString());
		return;
	}

	// Otherwise, wait for server update notification
#else
	TSharedPtr<IOnlineServices> OnlineServices = GetServices(GetWorld(), EOnlineServices::Default);
	check(OnlineServices);

	FOnlineAccountIdHandle LocalUserId = GetAccountId(PlayerController);
	if (LocalUserId.IsValid())
	{
		TOnlineResult<FGetResolvedConnectString> Result = OnlineServices->GetResolvedConnectString({LocalUserId, GetLobbyId(SessionName)});
		if (ensure(Result.IsOk()))
		{
			URL = Result.GetOkValue().ResolvedConnectString;
		}
	}
#endif // COMMONUSER_OSSV1

	// #START @AccelByte Implementation : Add options for the prefered map, it will load the map on the server after first player join.
	//URL.Append(TEXT("?preferedMap=%s"), *SearchSettings->);
	// #END
	UE_LOG(LogCommonSession, Error, TEXT("Traveling to: %s"), *URL);
	PlayerController->ClientTravel(URL, TRAVEL_Absolute);
}

#if COMMONUSER_OSSV1
void UCommonSessionSubsystem::HandleSessionFailure(const FUniqueNetId& NetId, ESessionFailure::Type FailureType)
{
	UE_LOG(LogCommonSession, Warning, TEXT("UCommonSessionSubsystem::HandleSessionFailure(NetId: %s, FailureType: %s)"), *NetId.ToDebugString(), LexToString(FailureType));

	//@TODO: Probably need to do a bit more...
}
#endif // COMMONUSER_OSSV1

void UCommonSessionSubsystem::TravelLocalSessionFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ReasonString)
{
	// The delegate for this is global, but PIE can have more than one game instance, so make
	// sure it's being raised for the same world this game instance subsystem is associated with
	if (World != GetWorld())
	{
		return;
	}

	UE_LOG(LogCommonSession, Warning, TEXT("TravelLocalSessionFailure(World: %s, FailureType: %s, ReasonString: %s)"),
		*GetPathNameSafe(World),
		ETravelFailure::ToString(FailureType),
		*ReasonString);
}

void UCommonSessionSubsystem::HandlePostLoadMap(UWorld* World)
{
	// Ignore null worlds.
	if (!World)
	{
		return;
	}

	// Ignore any world that isn't part of this game instance, which can be the case in the editor.
	if (World->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// We don't care about updating the session unless the world type is game/pie.
	if (!(World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE))
	{
		return;
	}

#if COMMONUSER_OSSV1
	const FOnlineSessionAccelBytePtr SessionInterface = GetSessionInterface();
	EARLY_RETURN_IF_INVALID(SessionInterface.IsValid(), "Session interface", );

	// If we're hosting a session, update the advertised map name.
	if (HostSettings.IsValid())
	{
		// This needs to be the full package path to match the host GetMapName function, World->GetMapName is currently the short name
		HostSettings->Set(SETTING_MAPNAME, UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()), EOnlineDataAdvertisementType::ViaOnlineService);

		const FName SessionName(NAME_GameSession);
		SessionInterface->UpdateSession(SessionName, *HostSettings, true);
	}

	const FNamedOnlineSession* Session = SessionInterface->GetNamedSession(NAME_GameSession);
	if (Session == nullptr)
	{
		UE_LOG(LogCommonSession, Warning, TEXT("HandlePostLoadMap: There is no current session!"));
		return;
	}

	const TSharedPtr<FOnlineSessionInfoAccelByteV2> SessionInfo = StaticCastSharedPtr<FOnlineSessionInfoAccelByteV2>(Session->SessionInfo);
	EARLY_RETURN_IF_INVALID(SessionInfo.IsValid(), "Session info", );

	if (SessionInfo->GetServerType() == EAccelByteV2SessionConfigurationServerType::P2P)
	{
		if (Session->LocalOwnerId.ToSharedRef().Get() != SessionInfo->GetLeaderId().ToSharedRef().Get())
		{
			return;
		}

		FOnlineSessionSettings SessionSettings = Session->SessionSettings;
		bool bServerConnectReadyValue = false;
		if (!SessionSettings.Get(SETTING_SESSION_SERVER_CONNECT_READY, bServerConnectReadyValue) || !bServerConnectReadyValue)
		{
			SessionSettings.Set(SETTING_SESSION_SERVER_CONNECT_READY, true);
			SessionInterface->UpdateSession(Session->SessionName, SessionSettings);
		}
	}
#endif // COMMONUSER_OSSV1
}

#undef LOCTEXT_NAMESPACE
#undef EARLY_RETURN_IF_INVALID
#undef EARLY_RETURN_IF_INVALID_WITH_DELEGATE
