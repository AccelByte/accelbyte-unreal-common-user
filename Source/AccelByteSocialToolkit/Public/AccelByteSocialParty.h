// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "CoreMinimal.h"
#include "Party/SocialParty.h"
#include "AccelByteSocialParty.generated.h"

USTRUCT()
struct ACCELBYTESOCIALTOOLKIT_API FAccelBytePartyRepData : public FPartyRepData
{
	GENERATED_BODY()
public:
	FAccelBytePartyRepData() = default;

protected:
	virtual void CompareAgainst(const FOnlinePartyRepDataBase & OldData) const override;
};

/**
 * 
 */
UCLASS()
class ACCELBYTESOCIALTOOLKIT_API UAccelByteSocialParty : public USocialParty
{
	GENERATED_BODY()
public:
	UAccelByteSocialParty();

protected:
	virtual FPartyPrivacySettings GetDesiredPrivacySettings() const override;
	virtual TSubclassOf<UPartyMember> GetDesiredMemberClass(bool bLocalPlayer) const override;
	
private:
	FAccelBytePartyRepData RepData;
};
