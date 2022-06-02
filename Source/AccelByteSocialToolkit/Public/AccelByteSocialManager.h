// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager."

#pragma once

#include "CoreMinimal.h"
#include "SocialManager.h"
#include "AccelByteSocialManager.generated.h"

/**
 * 
 */
UCLASS()
class ACCELBYTESOCIALTOOLKIT_API UAccelByteSocialManager : public USocialManager
{
	GENERATED_BODY()
public:
	UAccelByteSocialManager();

	virtual TSubclassOf<USocialParty> GetPartyClassForType(const FOnlinePartyTypeId& PartyTypeId) const override;
};
