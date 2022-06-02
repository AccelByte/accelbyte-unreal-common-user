// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager."

#pragma once

#include "CoreMinimal.h"
#include "SocialToolkit.h"
#include "AccelByteSocialToolkit.generated.h"

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
};
