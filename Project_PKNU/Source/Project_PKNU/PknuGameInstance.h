// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "PknuGameInstance.generated.h"

class UWebSocketManager;
class AMyRemoteCharacter;

/**
 * 
 */
UCLASS()
class PROJECT_PKNU_API UPknuGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	UPknuGameInstance(); // 생성자 선언 추가
	// GameInstance 초기화 시 호출
	virtual void Init() override;
	// GameInstance 종료 시 호출
	virtual void Shutdown() override;

	// WebSocketManager 인스턴스를 반환하는 Getter
	UFUNCTION(BlueprintCallable, Category = "Network")
	UWebSocketManager* GetWebSocketManager() const;

protected:
	// WebSocketManager 인스턴스를 저장할 변수
	UPROPERTY()
	UWebSocketManager* WebSocketManager;

	// 원격 캐릭터 블루프린트 클래스 (캐릭터에서 이곳으로 이동)
	UPROPERTY(EditDefaultsOnly, Category = "WebSocket")
	TSubclassOf<AMyRemoteCharacter> OtherPlayerBlueprintClass;
};
