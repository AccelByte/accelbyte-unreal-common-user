// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/AccelByteError.h"
#include "Engine/GameInstance.h"
#include "OnlineUserInterfaceAccelByte.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/StrongObjectPtr.h"

#if COMMONUSER_OSSV1
#include "OnlineSubsystemAccelByte.h"
#include "OnlineSubsystemTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Public/OnlineSessionSettings.h"
#else
#include "Online/Lobbies.h"
#endif // COMMONUSER_OSSV1

#include "CommonSessionSubsystem.generated.h"

class UWorld;
class FCommonSession_OnlineSessionSettings;

#if COMMONUSER_OSSV1
class FCommonOnlineSearchSettingsOSSv1;
using FCommonOnlineSearchSettings = FCommonOnlineSearchSettingsOSSv1;
#else
class FCommonOnlineSearchSettingsOSSv2;
using FCommonOnlineSearchSettings = FCommonOnlineSearchSettingsOSSv2;
#endif // COMMONUSER_OSSV1

#if COMMONUSER_OSSV1
// #SESSIONv2 Define custom session settings
#define SETTING_HOSTNAME FName(TEXT("HOSTNAME"))
#define SETTING_SESSIONNAME FName(TEXT("SESSIONNAME"))
#define SETTING_LOCALADDRESS FName(TEXT("LOCALADDRESS"))

#define SETTING_BOTSENABLED FName(TEXT("BOTSENABLED"))
#define SETTING_SESSION_SERVER_CONNECT_READY FName(TEXT("SERVERCONNECTREADY"))
#endif

//////////////////////////////////////////////////////////////////////
// UCommonSession_HostSessionRequest

/** Specifies the online features and connectivity that should be used for a game session */
UENUM(BlueprintType)
enum class ECommonSessionOnlineMode : uint8
{
	Offline,
	LAN,
	Online
};

UENUM(BlueprintType)
enum class ECommonSessionOnlineServerType : uint8
{
	NONE UMETA(Hidden),
	P2P UMETA(DisplayName = "P2P"),
	Dedicated UMETA(DisplayName = "DS")
};

UENUM(BlueprintType)
enum class ECommonSessionTeamChangeDirection : uint8
{
	NEXT,
	PREVIOUS
};

USTRUCT(BlueprintType)
struct FCommonSessionOnlineUser
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FUniqueNetIdRepl UserId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	FCommonSessionOnlineUser() {}

	FCommonSessionOnlineUser(const FUniqueNetIdRepl UserId, const FString& DisplayName) :
		UserId(UserId),
		DisplayName(DisplayName)
	{}
};

USTRUCT(BlueprintType)
struct FCommonSessionMember
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FCommonSessionOnlineUser UserInfo;

	UPROPERTY(BlueprintReadOnly)
	bool bIsLeader = false;

	UPROPERTY(BlueprintReadOnly)
	bool bIsLocalUser = false;

	FCommonSessionMember() {}

	FCommonSessionMember(
		const FUniqueNetIdRepl UserId,
		const FString& DisplayName,
		bool bInIsLeader,
		bool bInIsLocalUser
	) :
		UserInfo(UserId, DisplayName),
		bIsLeader(bInIsLeader),
		bIsLocalUser(bInIsLocalUser)
	{}
};

USTRUCT(BlueprintType)
struct FCommonSessionTeam
{
	GENERATED_BODY();

	UPROPERTY(BlueprintReadOnly)
	TArray<FCommonSessionMember> Members;

	FCommonSessionTeam() {} 
};

/** A request object that stores the parameters used when hosting a gameplay session */
UCLASS(BlueprintType)
class COMMONUSER_API UCommonSession_HostSessionRequest : public UObject
{
	GENERATED_BODY()

public:
	/** Indicates if the session is a full online session or a different type */
	UPROPERTY(BlueprintReadWrite, Category=Session)
	ECommonSessionOnlineMode OnlineMode;

	/** True if this request should create a player-hosted lobbies if available */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	bool bUseLobbies;

	/** String used during matchmaking to specify what type of game mode this is */
	UPROPERTY(BlueprintReadWrite, Category=Session)
	FString ModeNameForAdvertisement;

	/** The map that will be loaded at the start of gameplay, this needs to be a valid Primary Asset top-level map */
	UPROPERTY(BlueprintReadWrite, Category=Session, meta=(AllowedTypes="World"))
	FPrimaryAssetId MapID;

	/** Extra arguments passed as URL options to the game */
	UPROPERTY(BlueprintReadWrite, Category=Session)
	TMap<FString, FString> ExtraArgs;

	/** Maximum players allowed per gameplay session */
	UPROPERTY(BlueprintReadWrite, Category=Session)
	int32 MaxPlayerCount = 16;

	/** #START @AccelByte Implementation : GameMode on matchmaking service */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	FString MatchPool;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	ECommonSessionOnlineServerType ServerType{ECommonSessionOnlineServerType::NONE};
	// #END

public:
	/** Returns the maximum players that should actually be used, could be overridden in child classes */
	virtual int32 GetMaxPlayers() const;

	/** Returns the full map name that will be used during gameplay */
	virtual FString GetMapName() const;

	/** Constructs the full URL that will be passed to ServerTravel */
	virtual FString ConstructTravelURL() const;

	/** Returns true if this request is valid, returns false and logs errors if it is not */
	virtual bool ValidateAndLogErrors() const;
};


//////////////////////////////////////////////////////////////////////
// UCommonSession_SearchResult

/** A result object returned from the online system that describes a joinable game session */
UCLASS(BlueprintType)
class COMMONUSER_API UCommonSession_SearchResult : public UObject
{
	GENERATED_BODY()

public:
	/** Returns an internal description of the session, not meant to be human readable */
	UFUNCTION(BlueprintCallable, Category=Session)
	FString GetDescription() const;

	/** Gets an arbitrary string setting, bFoundValue will be false if the setting does not exist */
	UFUNCTION(BlueprintPure, Category=Sessions)
	void GetStringSetting(FName Key, FString& Value, bool& bFoundValue) const;

	/** Gets an arbitrary integer setting, bFoundValue will be false if the setting does not exist */
	UFUNCTION(BlueprintPure, Category = Sessions)
	void GetIntSetting(FName Key, int32& Value, bool& bFoundValue) const;

	/** The number of private connections that are available */
	UFUNCTION(BlueprintPure, Category=Sessions)
	int32 GetNumOpenPrivateConnections() const;

	/** The number of publicly available connections that are available */
	UFUNCTION(BlueprintPure, Category=Sessions)
	int32 GetNumOpenPublicConnections() const;

	/** The maximum number of publicly available connections that could be available, including already filled connections */
	UFUNCTION(BlueprintPure, Category = Sessions)
	int32 GetMaxPublicConnections() const;

	/** Ping to the search result, MAX_QUERY_PING is unreachable */
	UFUNCTION(BlueprintPure, Category=Sessions)
	int32 GetPingInMs() const;

	UFUNCTION(BlueprintCallable, Category=Sessions)
	FString GetUsername() const;

	UFUNCTION(BlueprintCallable, Category=Sessions)
	FString GetOwningAccelByteIdString() const;

public:
	/** Pointer to the platform-specific implementation */
#if COMMONUSER_OSSV1
	FOnlineSessionSearchResult Result;
#else
	TSharedPtr<const UE::Online::FLobby> Lobby;
#endif // COMMONUSER_OSSV1

};


//////////////////////////////////////////////////////////////////////
// UCommonSession_SearchSessionRequest

/** Delegates called when a session search completes */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCommonSession_FindSessionsFinished, bool bSucceeded, const FText& ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonSession_FindSessionsFinishedDynamic, bool, bSucceeded, FText, ErrorMessage);

/** Request object describing a session search, this object will be updated once the search has completed */
UCLASS(BlueprintType)
class COMMONUSER_API UCommonSession_SearchSessionRequest : public UObject
{
	GENERATED_BODY()

public:
	/** Indicates if the this is looking for full online games or a different type like LAN */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	ECommonSessionOnlineMode OnlineMode;

	/** True if this request should look for player-hosted lobbies if they are available, false will only search for registered server sessions */
	UPROPERTY(BlueprintReadWrite, Category = Session)
	bool bUseLobbies;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Experience)
	ECommonSessionOnlineServerType ServerType{ECommonSessionOnlineServerType::NONE};

	/** List of all found sessions, will be valid when OnSearchFinished is called */
	UPROPERTY(BlueprintReadOnly, Category=Session)
	TArray<UCommonSession_SearchResult*> Results;

	/** Native Delegate called when a session search completes */
	FCommonSession_FindSessionsFinished OnSearchFinished;

	/** Called by subsystem to execute finished delegates */
	void NotifySearchFinished(bool bSucceeded, const FText& ErrorMessage);

private:
	/** Delegate called when a session search completes */
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (DisplayName = "On Search Finished", AllowPrivateAccess = true))
	FCommonSession_FindSessionsFinishedDynamic K2_OnSearchFinished;
};

// #START @AccelByte Implementation

/** Delegates called when a matchmaking session completes */
DECLARE_MULTICAST_DELEGATE_TwoParams(FCommonSession_MatchmakingSessionsFinished, bool bSucceeded, const FText& ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCommonSession_MatchmakingSessionsFinishedDynamic, bool, bSucceeded, FText, ErrorMessage);

DECLARE_DELEGATE_OneParam(FOnCustomSessionUpdateComplete, bool bWasSuccessful);

/** Request object describing a matcmaking session, this object will be updated once the search has completed */
UCLASS(BlueprintType)
class COMMONUSER_API UCommonSession_MatchmakingSessionRequest : public UObject
{
	GENERATED_BODY()

public:
	/** Native Delegate called when a session search completes */
	FCommonSession_MatchmakingSessionsFinished OnMatchmakingFinished;

	/** Called by subsystem to execute finished delegates */
	void NotifyMatchmakingFinished(bool bSucceeded, const FText& ErrorMessage);

private:
	/** Delegate called when a session search completes */
	UPROPERTY(BlueprintAssignable, Category = "Events", meta = (DisplayName = "On Matchmaking Finished", AllowPrivateAccess = true))
	FCommonSession_MatchmakingSessionsFinishedDynamic K2_OnMatchmakingFinished;
};
// #END


//////////////////////////////////////////////////////////////////////
// UCommonSessionSubsystem

/** 
 * Game subsystem that handles requests for hosting and joining online games.
 * One subsystem is created for each game instance and can be accessed from blueprints or C++ code.
 * If a game-specific subclass exists, this base subsystem will not be created.
 */
UCLASS()
class COMMONUSER_API UCommonSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UCommonSessionSubsystem() { }

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Creates a host session request with default options for online games, this can be modified after creation */
	UFUNCTION(BlueprintCallable, Category = Session)
	virtual UCommonSession_HostSessionRequest* CreateOnlineHostSessionRequest();

	/** Creates a session search object with default options to look for default online games, this can be modified after creation */
	UFUNCTION(BlueprintCallable, Category = Session)
	virtual UCommonSession_SearchSessionRequest* CreateOnlineSearchSessionRequest();

	/** Creates a new online game using the session request information, if successful this will start a hard map transfer */
	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void HostSession(APlayerController* HostingPlayer, UCommonSession_HostSessionRequest* Request);

	/** Starts a process to look for existing sessions or create a new one if no viable sessions are found */
	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void QuickPlaySession(APlayerController* JoiningOrHostingPlayer, UCommonSession_HostSessionRequest* Request);

	/** #START @AccelByte Implementation : Starts a process to matchmaking with other player. */
	/** @brief Start Session, must manually called after Map / Experience successfully loaded */
	virtual void StartSession();

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnHostCustomSessionComplete, bool, bWasSuccessful);

	/** Creates a new custom session */
	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void HostCustomSession(APlayerController* HostingPlayer, const FOnHostCustomSessionComplete& OnHostCustomSessionComplete);

	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void StartCustomSession(APlayerController* RequestingPlayer, const FOnHostCustomSessionComplete& OnComplete);

	UFUNCTION(BlueprintCallable, Category = Session)
	virtual bool IsCustomSession(APlayerController* RequestingPlayer);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchmakingStartDelegate);

	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void MatchmakingSession(APlayerController* JoiningOrHostingPlayer, UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest*& OutMatchmakingSessionRequest);

	UPROPERTY(BlueprintAssignable, Category=Session)
	FOnMatchmakingStartDelegate OnMatchmakingStartDelegate;
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMatchmakingCancelDelegate);

	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void CancelMatchmakingSession(APlayerController* CancelPlayer);

	UPROPERTY(BlueprintAssignable, Category=Session)
	FOnMatchmakingCancelDelegate OnMatchmakingCancelDelegate;
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchmakingTimeoutDelegate, const FErrorInfo&, Error);

	UPROPERTY(BlueprintAssignable, Category=Session)
	FOnMatchmakingTimeoutDelegate OnMatchmakingTimeoutDelegate;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMatchFoundDelegate, FString, MatchId);

	UPROPERTY(BlueprintAssignable, Category=Session)
	FOnMatchFoundDelegate OnMatchFoundDelegate;

	// Custom sessions
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSessionUpdatedDelegate);

	UPROPERTY(BlueprintAssignable, Category=Session)
	FOnSessionUpdatedDelegate OnSessionChangedDelegate;

	UPROPERTY(BlueprintAssignable, Category=Session)
	FOnSessionUpdatedDelegate OnSessionJoinedDelegate;

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnGetSessionTeamsCompleteDelegate, const TArray<FCommonSessionTeam>&, Teams);

	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void GetSessionTeams(const APlayerController* QueryingPlayer, const FOnGetSessionTeamsCompleteDelegate& OnComplete);

	UFUNCTION(BlueprintCallable, Category=Session)
	virtual bool IsLocalUserLeader(const APlayerController* QueryingPlayer);

	/*UFUNCTION(BlueprintCallable, Category=Session)
	virtual void JoinLeaderIfStarted(APlayerController* JoiningPlayer);*/

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAddPlayerToTeamComplete, bool, bWasSuccessful);
	
	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void AddPlayerToCustomGameSessionTeam(const APlayerController* Player, const FOnAddPlayerToTeamComplete& OnComplete);
	// #END

	/** Starts process to join an existing session, if successful this will connect to the specified server */
	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void JoinSession(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Request);

	/** Queries online system for the list of joinable sessions matching the search request */
	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void FindSessions(APlayerController* SearchingPlayer, UCommonSession_SearchSessionRequest* Request);

	/** Clean up any active sessions, called from cases like returning to the main menu */
	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void CleanUpSessions();

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnLeaveCurrentGameSession, bool, bWasSuccessful);

	/** Leave and destroy the current game session, used for custom games during lobby */
	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void LeaveCurrentGameSession(APlayerController* LeavingPlayer, const FOnLeaveCurrentGameSession& OnLeaveComplete);

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnSetCustomGameSettingComplete, bool, bWasSuccessful);

	/** Set the current network mode for the custom game session to be the value passed in */
	UFUNCTION(BlueprintCallable, Category = Session)
	virtual void SetCustomGameSessionNetworkMode(APlayerController* UpdatingPlayer, const ECommonSessionOnlineServerType& NetworkMode, const FOnSetCustomGameSettingComplete& OnSettingUpdateComplete);

	/** Set whether this game session has bots enabled or not */
	UFUNCTION(BlueprintCallable, Category = Session)
	virtual void SetCustomGameSessionBotsEnabled(APlayerController* UpdatingPlayer, bool bBotsEnabled, const FOnSetCustomGameSettingComplete& OnSettingUpdateComplete);

	/** Set the map selection for this custom game session */
	UFUNCTION(BlueprintCallable, Category = Session)
	virtual void SetCustomGameSessionMap(const FPrimaryAssetId& MapId, const FName& ExperienceAssetName, const FString& ExperienceName, const FOnSetCustomGameSettingComplete& OnComplete);

	UFUNCTION(BlueprintCallable, Category = Session)
	virtual FString GetCustomGameSessionNetworkModeString(APlayerController* QueryingPlayer);

	UFUNCTION(BlueprintCallable, Category = Session)
	virtual FString GetCustomGameSessionBotsEnabledString(APlayerController* QueryingPlayer);

	UFUNCTION(BlueprintCallable, Category = Session)
	virtual FString GetCustomGameSessionMapNameString(APlayerController* QueryingPlayer);

	UFUNCTION(BlueprintCallable, Category = Session)
	virtual void ChangeCustomSessionTeam(APlayerController* UpdatingPlayer, const ECommonSessionTeamChangeDirection& Direction, const FOnSetCustomGameSettingComplete& OnTeamChangeComplete);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSessionCreatedDelegate);

	UFUNCTION(BlueprintCallable, Category=Session)
	virtual void InviteToCustomSession(const APlayerController* SendingPlayer, const FUniqueNetIdRepl ReceivingPlayerId);

	UPROPERTY(BlueprintAssignable, Category=Session)
	FOnSessionCreatedDelegate OnSessionCreatedDelegate;

protected:
	// Functions called during the process of creating or joining a session, these can be overidden for game-specific behavior

	/** Called to fill in a session request from quick play host settings, can be overridden for game-specific behavior */
	virtual TSharedRef<FCommonOnlineSearchSettings> CreateQuickPlaySearchSettings(UCommonSession_HostSessionRequest* Request, UCommonSession_SearchSessionRequest* QuickPlayRequest);

	// #START @AccelByte Implementation
	virtual TSharedRef<FCommonOnlineSearchSettings> CreateMatchmakingSearchSettings(UCommonSession_HostSessionRequest* Request, UCommonSession_SearchSessionRequest* SearchRequest);
	// #END

	/** Called when a quick play search finishes, can be overridden for game-specific behavior */
	virtual void HandleQuickPlaySearchFinished(bool bSucceeded, const FText& ErrorMessage, TWeakObjectPtr<APlayerController> JoiningOrHostingPlayer, TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequest);

	// #START @AccelByte Implementation HandleMatchmaking Finished
	virtual void HandleMatchmakingFinished(bool bSucceeded, const FText& ErrorMessage, TWeakObjectPtr<APlayerController> JoiningOrHostingPlayer, TStrongObjectPtr<UCommonSession_HostSessionRequest> HostRequest);
	// #END

	/** Called when traveling to a session fails */
	virtual void TravelLocalSessionFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ReasonString);

	/** Called when a new session is either created or fails to be created */
	virtual void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);

	/** Called to finalize session creation */
	virtual void FinishSessionCreation(bool bWasSuccessful);

	/** Called after traveling to the new hosted session map */
	virtual void HandlePostLoadMap(UWorld* World);

protected:
	// Internal functions for initializing and handling results from the online systems

	void BindOnlineDelegates();
	void CreateOnlineSessionInternal(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request);
	void FindSessionsInternal(APlayerController* SearchingPlayer, const TSharedRef<FCommonOnlineSearchSettings>& InSearchSettings);
	void JoinSessionInternal(APlayerController* JoiningPlayer, UCommonSession_SearchResult* Request);
	void InternalTravelToSession(const FName SessionName);

#if COMMONUSER_OSSV1
	// #START #SESSIONv2 @AccelByte SessionV2
	void OnStartMatchmakingComplete(FName SessionName, const FOnlineError& ErrorDetails, const FSessionMatchmakingResults& Results);
	void OnDestroySessionForJoinComplete(FName SessionName, bool bWasSuccessful, UCommonSession_SearchResult* Request, APlayerController* JoiningPlayer);
	void OnSessionServerUpdate(FName SessionName);
	void OnServerReceivedSession(FName SessionName);
	FDelegateHandle QuerySessionsCompleteDelegateHandle{};
	FDelegateHandle SessionServerUpdateDelegateHandle{};
	FDelegateHandle ServerReceivedSessionDelegateHandle{};
	FDelegateHandle CreateSessionDelegateHandle{};
	FDelegateHandle JoinSessionDelegateHandle{};
	TSharedPtr<FOnlineSessionSearch> CurrentMatchmakingSearchHandle{ nullptr };
	TSharedPtr<FOnlineSessionSearch> CurrentQuerySessionsHandle{ nullptr };
	// #End

	void BindOnlineDelegatesOSSv1();
	void CreateOnlineSessionInternalOSSv1(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request);
	void FindSessionsInternalOSSv1(ULocalPlayer* LocalPlayer);
	void JoinSessionInternalOSSv1(APlayerController* JoiningPlayer, ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request);
	TSharedRef<FCommonOnlineSearchSettings> CreateQuickPlaySearchSettingsOSSv1(UCommonSession_HostSessionRequest* Request, UCommonSession_SearchSessionRequest* QuickPlayRequest);
	void CleanUpSessionsOSSv1();

	void HandleSessionFailure(const FUniqueNetId& NetId, ESessionFailure::Type FailureType);
	void OnStartSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnRegisterLocalPlayerComplete_CreateSession(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result);
	void OnUpdateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnEndSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);

	// #START @AccelByte Implementation Matchmaking Handler
	void OnMatchmakingStarted();
	void OnMatchmakingStartedNotification();
	void OnMatchmakingCanceledNotification();
	void OnMatchmakingComplete(FName SessionName, bool bWasSuccessful);
	void OnCancelMatchmakingComplete(FName SessionName, bool bWasSuccessful);
	void OnMatchmakingTimeout(const FErrorInfo& Error);
	void OnMatchFound(FString MatchId);
	// #End

	void OnSessionParticipantsChange(FName SessionName, const FUniqueNetId& UniqueId, bool bJoined);
	void OnSessionJoined(FName, EOnJoinSessionCompleteResult::Type);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnRegisterJoiningLocalPlayerComplete(const FUniqueNetId& PlayerId, EOnJoinSessionCompleteResult::Type Result);
	void FinishJoinSession(EOnJoinSessionCompleteResult::Type Result);

#else
	void BindOnlineDelegatesOSSv2();
	void CreateOnlineSessionInternalOSSv2(ULocalPlayer* LocalPlayer, UCommonSession_HostSessionRequest* Request);
	void FindSessionsInternalOSSv2(ULocalPlayer* LocalPlayer);
	void JoinSessionInternalOSSv2(ULocalPlayer* LocalPlayer, UCommonSession_SearchResult* Request);
	TSharedRef<FCommonOnlineSearchSettings> CreateQuickPlaySearchSettingsOSSv2(UCommonSession_HostSessionRequest* HostRequest, UCommonSession_SearchSessionRequest* SearchRequest);
	void CleanUpSessionsOSSv2();

	/** Get the local user id for a given controller */
	UE::Online::FOnlineAccountIdHandle GetAccountId(APlayerController* PlayerController) const;
	/** Get the lobby id for a given session name */
	UE::Online::FOnlineLobbyIdHandle GetLobbyId(const FName SessionName) const;
#endif // COMMONUSER_OSSV1

protected:
	/** The travel URL that will be used after session operations are complete */
	FString PendingTravelURL;

	/** True if we want to cancel the session after it is created */
	bool bWantToDestroyPendingSession = false;

	/** Settings for the current search */
	TSharedPtr<FCommonOnlineSearchSettings> SearchSettings;

	/** Settings for the current host request */
	TSharedPtr<FCommonSession_OnlineSessionSettings> HostSettings;

	// #START #SESSIONv2 @AccelByte Utility methods
	bool bCreatingCustomSession;
	bool bReceivedServerReadyUpdate;

private:
	/**
	 * Number of restored game sessions that we have left to leave
	 */
	int32 NumberOfRestoredSessionsToLeave = 0;

	/**
	 * Handle to the delegate binding to listen to create session complete for custom games
	 */
	FDelegateHandle OnCreateCustomGameSessionHandle{};

	/**
	 * Handle to the delegate binding to listen to session update complete for updating network mode
	 */
	FDelegateHandle OnUpdateNetworkModeCompleteHandle{};

	/**
	 * Handle to the delegate binding to listen to session update complete for updating bots enabled
	 */
	FDelegateHandle OnUpdateBotsEnabledCompleteHandle{};

	/**
	 * Handle to the delegate binding to listen to session update complete for updating map name
	 */
	FDelegateHandle OnUpdateMapCompleteHandle{};

	/**
	 * Handle to the delegate binding to listen to session update complete for updating teams
	 */
	FDelegateHandle OnUpdateTeamsCompleteHandle{};

	/**
	 * Handle to the delegate binding to listen to session update complete for updating teams
	 */
	FDelegateHandle OnQueryUserInfoCompleteHandle{};

	/** Get our session interface for interaction with game sessions */
	FOnlineSessionAccelBytePtr GetSessionInterface() const;

	/** Get our identity interface for checking lobby connection */
	FOnlineIdentityAccelBytePtr GetIdentityInterface() const;

	/** Get the user interface for interaction with users */
	IOnlineUserPtr GetUserInterface() const;

	/** Get local session IP for creation of a local session */
	FString GetLocalSessionAddress() const;

	/**
	 * Method to restore all game sessions that the player is in, and leave them if they are in any.
	 */
	void RestoreAndLeaveActiveGameSessions(APlayerController* Player, const TDelegate<void(bool)>& OnRestoreAndLeaveAllComplete);

	/**
	 * Handler for when the call to restore all sessions completes, allowing us to leave all game sessions
	 */
	void OnRestoreAllSessionsComplete(const FUniqueNetId& LocalUserId, const FOnlineError& Result, TDelegate<void(bool)> OnRestoreAndLeaveAllComplete);

	/**
	 * Handler for when we leave a single restored game session, will call the delegate passed in once the number of sessions to leave is zero
	 */
	void OnLeaveRestoredGameSessionComplete(bool bWasSuccessful, FString SessionId, TDelegate<void(bool)> OnRestoreAndLeaveAllComplete);

	void OnCreateCustomSessionComplete(FName SessionName, bool bWasSuccessful, FString AccelByteId, FOnHostCustomSessionComplete OnComplete);
	
	void CreateCustomGameSessionInternal(const APlayerController* LocalHostingPlayer, FOnHostCustomSessionComplete OnComplete);

	void AddPlayerToCustomGameSessionTeam(const FString& PlayerAccelByteId, const FOnCustomSessionUpdateComplete& OnComplete);

	/**
	 * Handler for when an update to the custom game session completes
	 */
	void OnUpdateCustomGameSessionComplete(FName SessionName, bool bWasSuccessful, FOnSetCustomGameSettingComplete OnSettingUpdateComplete);

	/**
	 * 
	 */
	static int32 GetCurrentTeamIndex(const TArray<FAccelByteModelsV2GameSessionTeam>& Teams, const FString& PlayerAccelByteId);

	void QuerySessionMembersData(int32 LocalUserNum, const FNamedOnlineSession* Session, const FOnQueryUserInfoCompleteDelegate& OnComplete);

	// #END
};
