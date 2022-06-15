// Copyright (c) 2022 AccelByte Inc. All Rights Reserved.
// This is licensed software from AccelByte Inc, for limitations
// and restrictions contact your company contract manager.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

ACCELBYTESOCIALTOOLKIT_API DECLARE_LOG_CATEGORY_EXTERN(LogAccelByteToolkit, Display, All);

class FAccelByteSocialToolkitModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
