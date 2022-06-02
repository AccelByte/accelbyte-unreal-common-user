// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "CoreMinimal.h"
#include "Party/PartyMember.h"
#include "AccelBytePartyMember.generated.h"

USTRUCT()
struct FAccelBytePartyMemberRepData : public FPartyMemberRepData
{
	GENERATED_BODY()
public:
	FAccelBytePartyMemberRepData() = default;
	
};

/**
 * 
 */
UCLASS()
class ACCELBYTESOCIALTOOLKIT_API UAccelBytePartyMember : public UPartyMember
{
	GENERATED_BODY()
public:
	UAccelBytePartyMember();

private:
	FAccelBytePartyMemberRepData RepData;
};
