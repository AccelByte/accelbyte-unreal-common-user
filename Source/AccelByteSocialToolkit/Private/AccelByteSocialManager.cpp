// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager."


#include "AccelByteSocialManager.h"

#include "AccelByteSocialParty.h"
#include "AccelByteSocialToolkit.h"

UAccelByteSocialManager::UAccelByteSocialManager() : Super()
{
	ToolkitClass = UAccelByteSocialToolkit::StaticClass();
}

TSubclassOf<USocialParty> UAccelByteSocialManager::GetPartyClassForType(const FOnlinePartyTypeId& PartyTypeId) const
{
	return UAccelByteSocialParty::StaticClass();
}
