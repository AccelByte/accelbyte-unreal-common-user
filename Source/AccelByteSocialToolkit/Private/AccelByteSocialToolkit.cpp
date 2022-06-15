// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager."


#include "AccelByteSocialToolkit.h"

#include "AccelByteSocialManager.h"
#include "AccelByteSocialToolkitModule.h"
#include "OnlineSubsystemAccelByte.h"
#include "OnlineIdentityInterfaceAccelByte.h"
#include "OnlinePartyInterfaceAccelByte.h"

void UAccelByteSocialToolkit::InitializeToolkit(ULocalPlayer& InOwningLocalPlayer)
{
	Super::InitializeToolkit(InOwningLocalPlayer);
	
	bQueryFriendsOnStartup = false;
	bQueryBlockedPlayersOnStartup = false;
	bQueryRecentPlayersOnStartup = false;
	
	IOnlineSubsystem* Subsystem = GetSocialOss(ESocialSubsystem::Primary);
	check(Subsystem);
	if(Subsystem->GetSubsystemName().IsEqual(TEXT("ACCELBYTE")))
	{
		FOnlineIdentityAccelBytePtr IdentityAccelByte = StaticCastSharedPtr<FOnlineIdentityAccelByte>(Subsystem->GetIdentityInterface());
		IdentityAccelByte->AddOnConnectLobbyCompleteDelegate_Handle(InOwningLocalPlayer.GetLocalPlayerIndex(),
			FOnConnectLobbyCompleteDelegate::CreateUObject(this, &UAccelByteSocialToolkit::OnLobbyConnected));
	}
}

UAccelByteSocialToolkit::UAccelByteSocialToolkit() : Super()
{
}

void UAccelByteSocialToolkit::OnCreatePartyComplete(ECreatePartyCompletionResult CreatePartyCompletionResult)
{
	UE_LOG(LogAccelByteToolkit, Log, TEXT("Party Creation Succeed!"));
}

void UAccelByteSocialToolkit::OnLobbyConnected(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId,
	const FString& Error)
{
	if (IsOwnerLoggedIn())
	{
		QueryFriendsLists();
		QueryBlockedPlayers();
		QueryRecentPlayers();

		bool bAutoCreateParty = false;
		GConfig->GetBool(TEXT("AccelByteSocialToolkit"), TEXT("bAutoCreateParty"), bAutoCreateParty, GEngineIni);
		if(bAutoCreateParty)
		{
			FPartyConfiguration Config;
			Config.bIsAcceptingMembers = true;
			GetSocialManager().CreateParty(
				FOnlinePartySystemAccelByte::GetAccelBytePartyTypeId(),
				Config,
				USocialManager::FOnCreatePartyAttemptComplete::CreateUObject(this, &ThisClass::OnCreatePartyComplete)
			);
		}
	}
	
	OnLobbyConnectedDelegate.Broadcast();
}
