// Copyright (c) 2018 AccelByte, inc. All rights reserved.


#include "AsyncAction_CommonSession.h"
#include "CommonSessionSubsystem.h"
#include "Online.h"

UAsyncAction_CommonSessionEndSession* UAsyncAction_CommonSessionEndSession::TryToEndSession(UCommonSessionSubsystem* InCommonSession)
{
	UAsyncAction_CommonSessionEndSession* Action = NewObject<UAsyncAction_CommonSessionEndSession>();

	if(Action)
	{
		Action->CommonSession = InCommonSession;
	}
	else
	{
		Action->SetReadyToDestroy();
	}
	
	return Action;
}

void UAsyncAction_CommonSessionEndSession::HandleDestroySessionComplete(FName Name, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("Destroy SESSION Complete with %s"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"))
	OnComplete.Broadcast(bWasSuccessful);
	SetReadyToDestroy();
}

void UAsyncAction_CommonSessionEndSession::HandleOnEndSessionComplete(FName GameSession, bool bWasSuccessful)
{
	UE_LOG(LogTemp, Warning, TEXT("End SESSION Complete with %s"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"))
}

void UAsyncAction_CommonSessionEndSession::Activate()
{
	Super::Activate();

	IOnlineSessionPtr Session = Online::GetSessionInterface();
	if(Session)
	{
		Session->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));
		Session->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleOnEndSessionComplete));
		CommonSession->CleanUpSessions();
	}
	else
	{
		SetReadyToDestroy();
	}
}
