// Fill out your copyright notice in the Description page of Project Settings.


#include "PknuGameInstance.h"
#include "WebSocketManager.h"
#include "MyRemoteCharacter.h"

UPknuGameInstance::UPknuGameInstance()
{
	UE_LOG(LogTemp, Warning, TEXT("UPknuGameInstance::Constructor - Instance created."));
}

void UPknuGameInstance::Init()
{
	Super::Init();

	UE_LOG(LogTemp, Warning, TEXT("UPknuGameInstance::Init - GetWorld() is %s"), (GetWorld() ? TEXT("Valid") : TEXT("NULL")));
	UE_LOG(LogTemp, Warning, TEXT("UPknuGameInstance::Init - OtherPlayerBlueprintClass is %s"), (OtherPlayerBlueprintClass ? *OtherPlayerBlueprintClass->GetName() : TEXT("NULL")));

	// WebSocketManager 생성
	WebSocketManager = NewObject<UWebSocketManager>(this);
	// WebSocketManager 초기화
	WebSocketManager->Initialize(OtherPlayerBlueprintClass, GetWorld());
	// WebSocketManager->Connect(); // 자동 연결 제거
}

void UPknuGameInstance::Shutdown()
{
	if (WebSocketManager)
	{
		WebSocketManager->Close();
	}

	Super::Shutdown();
}

UWebSocketManager* UPknuGameInstance::GetWebSocketManager() const
{
	return WebSocketManager;
}
