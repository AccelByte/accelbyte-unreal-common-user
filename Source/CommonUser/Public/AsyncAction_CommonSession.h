// Copyright (c) 2018 AccelByte, inc. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "CommonSessionSubsystem.h"
#include "AsyncAction_CommonSession.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCompleteDestroySession, bool, bWasSuccessful);
/**
 * 
 */
UCLASS()
class COMMONUSER_API UAsyncAction_CommonSessionEndSession : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Retrieved cached friends, incoming, and outgoing friends request list. If there's no cached data, retrieve from API
	 *
	 * @param LocalPlayer Local player object
	 */
	UFUNCTION(BlueprintCallable, Category = "AccelByte | Common | Friends", meta = (BlueprintInternalUseOnly = "true"))
	static UAsyncAction_CommonSessionEndSession* TryToEndSession(UCommonSessionSubsystem* InCommonSession);

	UPROPERTY(BlueprintAssignable)
	FOnCompleteDestroySession OnComplete;

protected:
	void HandleDestroySessionComplete(FName Name, bool bWasSuccessful);
	void HandleOnEndSessionComplete(FName GameSession, bool bWasSuccessful);
	
	virtual void Activate() override;
	
	TWeakObjectPtr<UCommonSessionSubsystem> CommonSession;
};

