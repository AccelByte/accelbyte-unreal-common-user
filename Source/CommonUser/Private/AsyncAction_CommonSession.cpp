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
	UE_LOG(LogTemp, Warning, TEXT("END SESSION Complete with %s"), bWasSuccessful ? TEXT("Success") : TEXT("Failed"))
	// This cases is so weird. Somehow remove session from SessionBrowser is hang and can't return anything. Not sure why
	// But calling it again will make it work.
	if(!bWasSuccessful)
	{
		IOnlineSessionPtr SessionPtr = Online::GetSessionInterface();
		FNamedOnlineSession* Session = SessionPtr->GetNamedSession(GameSession);
		if(SessionPtr && Session)
		{
			Session->SessionState = EOnlineSessionState::InProgress;
			SessionPtr->EndSession(GameSession);
		}
	}
}

void UAsyncAction_CommonSessionEndSession::Activate()
{
	Super::Activate();
	UE_LOG(LogTemp, Warning, TEXT("UAsyncAction_CommonSessionEndSession::Activate"))

	IOnlineSessionPtr Session = Online::GetSessionInterface();
	if(Session)
	{
		EOnlineSessionState::Type SessionState = Session->GetSessionState(NAME_GameSession);
		if(SessionState == EOnlineSessionState::NoSession)
		{
			UE_LOG(LogTemp, Warning, TEXT("No Session available!"))
			OnComplete.Broadcast(false);
			SetReadyToDestroy();
		}
		else
		{
			Session->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleDestroySessionComplete));
			Session->AddOnEndSessionCompleteDelegate_Handle(FOnEndSessionCompleteDelegate::CreateUObject(this, &ThisClass::HandleOnEndSessionComplete));
			CommonSession->CleanUpSessions();
		}
	}
	else
	{
		SetReadyToDestroy();
	}
}
