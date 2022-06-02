// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.


#include "AccelByteSocialParty.h"

#include "AccelBytePartyMember.h"


void FAccelBytePartyRepData::CompareAgainst(const FOnlinePartyRepDataBase& OldData) const
{
	FPartyRepData::CompareAgainst(OldData);
	
	const FAccelBytePartyRepData& TypedOldData = static_cast<const FAccelBytePartyRepData&>(OldData);
}

UAccelByteSocialParty::UAccelByteSocialParty() : Super()
{
	PartyDataReplicator.EstablishRepDataInstance<FAccelBytePartyRepData>(RepData);
}


FPartyPrivacySettings UAccelByteSocialParty::GetDesiredPrivacySettings() const
{
	UAccelByteSocialParty *NonConstThis = const_cast<UAccelByteSocialParty*>(this);
	return GetPrivacySettingsForConfig(NonConstThis->GetCurrentConfiguration());
}

TSubclassOf<UPartyMember> UAccelByteSocialParty::GetDesiredMemberClass(bool bLocalPlayer) const
{
	return UAccelBytePartyMember::StaticClass();
}
