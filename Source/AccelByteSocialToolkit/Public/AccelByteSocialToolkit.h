// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager."

#pragma once

#include "CoreMinimal.h"
#include "SocialToolkit.h"
#include "AccelByteSocialToolkit.generated.h"

DECLARE_LOG_CATEGORY_CLASS(LogAccelByteToolkit, Log, All);

/**
 * 
 */
UCLASS()
class ACCELBYTESOCIALTOOLKIT_API UAccelByteSocialToolkit : public USocialToolkit
{
	GENERATED_BODY()
public:
	virtual void InitializeToolkit(ULocalPlayer& InOwningLocalPlayer) override;
	UAccelByteSocialToolkit();

	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyConnectedDelegate);

	FOnLobbyConnectedDelegate OnLobbyConnectedDelegate;

protected:
	void OnCreatePartyComplete(ECreatePartyCompletionResult CreatePartyCompletionResult);
	virtual void OnLobbyConnected(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
};
